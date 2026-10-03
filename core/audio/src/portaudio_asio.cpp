#include <mrs/device.hpp>
#include <portaudio.h>
#include <pa_asio.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace mrs::audio {
namespace {
class AsioDevice final : public IAudioDevice {
    PaStream* stream_{};
    std::shared_ptr<AudioEngine> engine_;
    DeviceConfig config_;
    DeviceStatus status_;
    static int callback(const void* input, void* output, unsigned long frames,
                        const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags flags, void* user) noexcept {
        auto& self = *static_cast<AsioDevice*>(user);
        const auto begin = std::chrono::steady_clock::now();
        self.engine_->process(static_cast<const float*>(input), static_cast<float*>(output), static_cast<std::uint32_t>(frames));
        const auto elapsed = std::chrono::steady_clock::now() - begin;
        const auto ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
        std::uint32_t diagnostics = 0;
        if (flags & paInputUnderflow) diagnostics |= 1;
        if (flags & paInputOverflow) diagnostics |= 2;
        if (flags & paOutputUnderflow) diagnostics |= 4;
        if (flags & paOutputOverflow) diagnostics |= 8;
        self.engine_->observe(ns, static_cast<std::uint32_t>(frames), diagnostics);
        return paContinue;
    }
    void check(PaError error, const char* operation) {
        if (error == paNoError) return;
        status_.phase = DevicePhase::error;
        status_.error = std::string(operation) + ": " + Pa_GetErrorText(error);
        if (error == paUnanticipatedHostError) {
            if (const auto* host = Pa_GetLastHostErrorInfo())
                if (host->errorText) status_.error += std::string(" / ") + host->errorText;
        }
        throw std::runtime_error(status_.error);
    }
    DeviceInfo info(int index) {
        const auto* device = Pa_GetDeviceInfo(index);
        if (!device || Pa_GetHostApiInfo(device->hostApi)->type != paASIO)
            throw std::invalid_argument("select a vendor ASIO device from --list");
        DeviceInfo result;
        result.index = index; result.name = device->name;
        check(PaAsio_GetAvailableBufferSizes(index, &result.min_buffer, &result.max_buffer,
                                            &result.preferred_buffer, &result.granularity), "ASIO buffer query");
        for (int channel = 0; channel < device->maxInputChannels; ++channel) {
            const char* name = nullptr;
            check(PaAsio_GetInputChannelName(index, channel, &name), "ASIO input name");
            result.inputs.emplace_back(name ? name : "Unnamed input");
        }
        for (int channel = 0; channel < device->maxOutputChannels; ++channel) {
            const char* name = nullptr;
            check(PaAsio_GetOutputChannelName(index, channel, &name), "ASIO output name");
            result.outputs.emplace_back(name ? name : "Unnamed output");
        }
        return result;
    }
public:
    AsioDevice() { check(Pa_Initialize(), "PortAudio initialization"); }
    ~AsioDevice() override { close(); (void)Pa_Terminate(); }
    std::vector<DeviceInfo> enumerate() override {
        if (stream_) throw std::logic_error("stop/close the stream before ASIO enumeration");
        const auto count = Pa_GetDeviceCount();
        if (count < 0) check(count, "ASIO device enumeration");
        std::vector<DeviceInfo> result;
        for (int index = 0; index < count; ++index) {
            const auto* d = Pa_GetDeviceInfo(index);
            const auto* host = d ? Pa_GetHostApiInfo(d->hostApi) : nullptr;
            if (host && host->type == paASIO) result.push_back(info(index));
        }
        return result;
    }
    void control_panel(int device) override {
        if (stream_) throw std::logic_error("close stream before vendor control panel");
        (void)info(device);
        check(PaAsio_ShowControlPanel(device, nullptr), "ASIO control panel");
    }
    void open(const DeviceConfig& config, std::shared_ptr<AudioEngine> engine) override {
        if (stream_) throw std::logic_error("one ASIO stream per shared device; close first");
        if (!engine) throw std::invalid_argument("missing shared audio engine");
        const auto device = info(config.device);
        validate_device_config(device, config);
        const auto render = engine->config();
        if (render.sample_rate != config.sample_rate || render.input_channels != config.inputs.size() ||
            render.output_channels != config.outputs.size() || render.max_block < config.buffer_frames)
            throw std::invalid_argument("engine/device configuration mismatch");
        config_ = config; engine_ = std::move(engine);
        PaAsioStreamInfo input_info{sizeof(PaAsioStreamInfo), paASIO, 1, paAsioUseChannelSelectors, config_.inputs.data()};
        PaAsioStreamInfo output_info{sizeof(PaAsioStreamInfo), paASIO, 1, paAsioUseChannelSelectors, config_.outputs.data()};
        const auto* d = Pa_GetDeviceInfo(config_.device);
        PaStreamParameters input{config_.device, static_cast<int>(config_.inputs.size()), paFloat32,
                                 d->defaultLowInputLatency, &input_info};
        PaStreamParameters output{config_.device, static_cast<int>(config_.outputs.size()), paFloat32,
                                  d->defaultLowOutputLatency, &output_info};
        // Request the native period via suggestedLatency; unspecified user frames
        // avoids an extra user/host adaptation buffer and exposes host frames.
        // Any driver fallback is visible in observed callback frames.
        input.suggestedLatency = static_cast<double>(config_.buffer_frames) / config_.sample_rate;
        output.suggestedLatency = input.suggestedLatency;
        check(Pa_IsFormatSupported(config_.inputs.empty() ? nullptr : &input, &output, config_.sample_rate),
              "ASIO format/sample-rate support");
        PaStream* opened = nullptr;
        const auto error = Pa_OpenStream(&opened, config_.inputs.empty() ? nullptr : &input, &output,
                                        config_.sample_rate, paFramesPerBufferUnspecified, paClipOff | paDitherOff,
                                        &callback, this);
        if (error != paNoError) { engine_.reset(); check(error, "ASIO open (close other DAWs if device is busy)"); }
        stream_ = opened;
        const auto* stream_info = Pa_GetStreamInfo(stream_);
        if (!stream_info) { close(); throw std::runtime_error("ASIO stream info unavailable"); }
        status_ = {DevicePhase::open, stream_info->sampleRate, stream_info->inputLatency * 1000,
                   stream_info->outputLatency * 1000, 0, {}};
        if (std::abs(status_.sample_rate - config_.sample_rate) > 0.5) {
            close();
            throw std::runtime_error("ASIO actual/project sample-rate mismatch");
        }
    }
    void start() override {
        if (!stream_) throw std::logic_error("ASIO device not open");
        check(Pa_StartStream(stream_), "ASIO start"); status_.phase = DevicePhase::running;
    }
    void stop() override {
        if (!stream_) return;
        const auto active = Pa_IsStreamActive(stream_);
        if (active < 0) check(active, "ASIO active state");
        if (active == 1) check(Pa_StopStream(stream_), "ASIO stop");
        status_.phase = DevicePhase::stopped;
    }
    void close() noexcept override {
        if (stream_) { (void)Pa_AbortStream(stream_); (void)Pa_CloseStream(stream_); stream_ = nullptr; }
        engine_.reset(); status_.phase = DevicePhase::closed;
    }
    DeviceStatus status() override {
        if (stream_) {
            status_.cpu_load = Pa_GetStreamCpuLoad(stream_);
            const auto active = Pa_IsStreamActive(stream_);
            if (status_.phase == DevicePhase::running && active != 1) {
                status_.phase = DevicePhase::error;
                status_.error = active < 0 ? Pa_GetErrorText(active) : "ASIO stream stopped unexpectedly; close and re-enumerate";
            }
        }
        return status_;
    }
};
}
std::unique_ptr<IAudioDevice> make_asio_device() { return std::make_unique<AsioDevice>(); }
} // namespace mrs::audio
