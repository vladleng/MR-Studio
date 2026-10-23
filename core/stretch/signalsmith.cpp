#include <mrs/stretch.hpp>
#include <mrs/no_denormals.hpp>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace mrs::stretch {
namespace {
using DSP = signalsmith::stretch::SignalsmithStretch<float>;
bool overlap(const float* a, std::uint32_t an, const float* b, std::uint32_t bn) noexcept {
    if (!an || !bn) return false;
    const auto aa = reinterpret_cast<std::uintptr_t>(a), bb = reinterpret_cast<std::uintptr_t>(b);
    return aa <= bb ? bb - aa < static_cast<std::uintptr_t>(an)*sizeof(float)
                    : aa - bb < static_cast<std::uintptr_t>(bn)*sizeof(float);
}
class Engine final : public ITimeStretchEngine {
public:
    Capabilities capabilities() const noexcept override { return {2,true,true,true,true}; }
    Status prepare(const Config& c, const Parameters& p) override {
        const auto status = validate(p);
        if (status != Status::ok) return status;
        if ((c.sample_rate != 44100 && c.sample_rate != 48000 && c.sample_rate != 96000) ||
            !c.channels || c.channels > 2) return Status::unsupported;
        if (!c.max_input_frames || c.max_input_frames > 32768 ||
            !c.max_output_frames || c.max_output_frames > 4096) return Status::invalid_parameter;
        auto next = std::make_unique<DSP>(0); // reproducible initial random phase
        if (p.quality == Quality::balanced)
            next->presetDefault(static_cast<int>(c.channels), static_cast<float>(c.sample_rate), c.split_computation);
        else next->presetCheaper(static_cast<int>(c.channels), static_cast<float>(c.sample_rate), c.split_computation);
        config_ = c;
        parameters_ = p;
        dsp_ = std::move(next);
        demand_.reset();
        drained_ = false;
        failed_ = false;
        ++generation_;
        apply();
        return Status::ok;
    }
    Status reset(std::uint64_t generation) override {
        if (!dsp_) return Status::not_prepared;
        // Reconstruct on control/worker: upstream reset does not reseed its RNG.
        const auto status = prepare(config_, parameters_);
        generation_ = generation;
        return status;
    }
    Status seek(std::span<const float* const> context, std::uint32_t frames,
                std::uint64_t generation) override {
        if (!dsp_) return Status::not_prepared;
        if (frames > latency().seek_context_frames || context.size() != config_.channels)
            return Status::invalid_buffer;
        for (const auto* channel : context) {
            if (frames && !channel) return Status::invalid_buffer;
            for (std::uint32_t i=0;i<frames;++i) if (!std::isfinite(channel[i])) return Status::invalid_buffer;
        }
        const auto status = reset(generation);
        if (status != Status::ok) return status;
        dsp_->seek(context, static_cast<int>(frames), 1/ratio());
        return Status::ok;
    }
    Status set_parameters(const Parameters& p) noexcept override {
        if (!dsp_) return Status::not_prepared;
        const auto status = validate(p);
        if (status != Status::ok) return status;
        if (p.quality != parameters_.quality || p.enabled != parameters_.enabled)
            return Status::unsupported; // latency/fast-path transitions require reprepare
        parameters_ = p;
        apply();
        return Status::ok;
    }
    Latency latency() const noexcept override {
        if (!dsp_) return {};
        return {static_cast<std::uint32_t>(dsp_->inputLatency()),
                static_cast<std::uint32_t>(dsp_->outputLatency()),
                static_cast<std::uint32_t>(dsp_->seekLength())};
    }
    std::uint64_t generation() const noexcept override { return generation_; }
    std::uint32_t input_required(std::uint32_t frames) const noexcept override {
        if (!dsp_ || frames > config_.max_output_frames) return UINT32_MAX;
        return demand_.peek(frames,ratio());
    }
    Result process(std::span<const float* const> input, std::uint32_t available,
                   std::span<float* const> output, std::uint32_t frames) noexcept override {
        if (!dsp_) return {Status::not_prepared};
        if (failed_) return {Status::backend_error};
        if (drained_) return {Status::drained};
        if (frames > config_.max_output_frames) return {Status::output_full};
        const auto required = input_required(frames);
        if (required > config_.max_input_frames) return {Status::invalid_parameter};
        if (available < required) return {Status::need_input};
        if (!buffers(input,required,output,frames)) return {Status::invalid_buffer};
        if (!frames) return {};
        for (const auto* channel : input)
            for (std::uint32_t i=0;i<required;++i) if (!std::isfinite(channel[i])) return {Status::invalid_buffer};
        const mrs::ScopedNoDenormals no_denormals;
        dsp_->process(input,static_cast<int>(required),output,static_cast<int>(frames));
        demand_.advance(frames,ratio());
        for (const auto* channel : output) for (std::uint32_t i=0;i<frames;++i)
            if (!std::isfinite(channel[i])) {
                for (auto* out : output) std::fill_n(out,frames,0.f);
                failed_ = true;
                return {Status::backend_error,required,frames};
            }
        return {Status::ok,required,frames};
    }
    Result flush(std::span<float* const> output, std::uint32_t capacity) override {
        if (!dsp_) return {Status::not_prepared};
        if (failed_) return {Status::backend_error};
        if (drained_) return {Status::drained};
        const auto frames = latency().output_frames;
        if (capacity < frames) return {Status::output_full};
        const std::array<const float*,2> empty{};
        if (!buffers(std::span{empty}.first(config_.channels),0,output,frames)) return {Status::invalid_buffer};
        const mrs::ScopedNoDenormals no_denormals;
        dsp_->flush(output,static_cast<int>(frames),static_cast<float>(1/ratio()));
        for (const auto* channel : output) for (std::uint32_t i=0;i<frames;++i)
            if (!std::isfinite(channel[i])) {
                for (auto* out : output) std::fill_n(out,frames,0.f);
                failed_ = true;
                return {Status::backend_error,0,frames};
            }
        drained_ = true;
        return {Status::drained,0,frames};
    }
private:
    double ratio() const noexcept { return parameters_.enabled ? parameters_.duration_ratio : 1; }
    void apply() noexcept {
        dsp_->setTransposeSemitones(parameters_.enabled ? static_cast<float>(parameters_.semitones + parameters_.cents/100) : 0.f);
        const bool modify = parameters_.enabled && parameters_.formants != Formants::follow_pitch;
        dsp_->setFormantSemitones(modify && parameters_.formants == Formants::shift ? static_cast<float>(parameters_.formant_semitones) : 0.f, modify);
        // Source uses freqToBand(), whose input is cycles/sample (not /Nyquist).
        const auto base = parameters_.formant_base_hz == 0 ? 200 : parameters_.formant_base_hz;
        dsp_->setFormantBase(static_cast<float>(base/config_.sample_rate));
    }
    bool buffers(std::span<const float* const> in, std::uint32_t ni,
                 std::span<float* const> out, std::uint32_t no) const noexcept {
        if (in.size() != config_.channels || out.size() != config_.channels) return false;
        for (std::size_t c=0;c<out.size();++c) {
            if ((ni && !in[c]) || (no && !out[c])) return false;
            for (std::size_t d=0;d<out.size();++d) {
                if (overlap(in[c],ni,out[d],no)) return false;
                if (c!=d && overlap(out[c],no,out[d],no)) return false;
            }
        }
        return true;
    }
    std::unique_ptr<DSP> dsp_;
    Config config_;
    Parameters parameters_;
    FrameDemand demand_;
    std::uint64_t generation_{};
    bool drained_{};
    bool failed_{};
};
} // namespace
std::unique_ptr<ITimeStretchEngine> make_signalsmith_engine() { return std::make_unique<Engine>(); }
std::string_view backend_revision() noexcept { return "signalsmith-stretch/a670068d9aeb64913331d5cc29337b19a457a7df;linear/de55e6a50ffcf6f8f43f649692d94691c7025151;builtin-fft;adapter1"; }
} // namespace mrs::stretch
