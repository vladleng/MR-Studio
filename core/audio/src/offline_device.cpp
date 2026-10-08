#include <mrs/offline_device.hpp>
#include <mrs/ahead_renderer.hpp>
#include <chrono>
#include <thread>
#include <stdexcept>
namespace mrs::audio {
namespace {
class OfflineDevice final : public IAudioDevice {
public:
    ~OfflineDevice() override { close(); }
    std::vector<DeviceInfo> enumerate() override {
        return {{0,"Offline clock (no sound)",{"Offline input"},{"Offline L","Offline R"},32,2048,128,-1}};
    }
    void control_panel(int) override { throw std::runtime_error("Offline clock has no hardware control panel"); }
    void open(const DeviceConfig& config, std::shared_ptr<AudioEngine> engine) override {
        if (thread_.joinable() || !engine) throw std::invalid_argument("invalid offline open");
        validate_device_config(enumerate().front(),config);
        auto render = engine->config();
        if (render.sample_rate != config.sample_rate || render.output_channels != config.outputs.size() ||
            render.input_channels != config.inputs.size() || render.max_block < config.buffer_frames)
            throw std::invalid_argument("offline render/device mismatch");
        close(); config_ = config; engine_ = std::move(engine);
        ahead_=std::make_unique<AheadRenderer>(engine_,config.buffer_frames,config.process_buffer_frames);
        output_.assign(static_cast<std::size_t>(config.buffer_frames)*config.outputs.size(),0);
        input_.assign(static_cast<std::size_t>(config.buffer_frames)*config.inputs.size(),0);
        phase_ = DevicePhase::open;
    }
    void start() override {
        if (!engine_ || thread_.joinable()) throw std::runtime_error("offline clock not ready");
        ahead_->start();
        running_.store(true); try { thread_ = std::thread([this] {
            using clock = std::chrono::steady_clock;
            const auto interval = std::chrono::duration_cast<clock::duration>(
                std::chrono::duration<double>(static_cast<double>(config_.buffer_frames)/config_.sample_rate));
            auto next = clock::now();
            while (running_.load(std::memory_order_acquire)) {
                ahead_->process(input_.empty() ? nullptr : input_.data(),output_.data(),config_.buffer_frames);
                next += interval;
                // This is a development clock, not a hardware timing/performance benchmark.
                std::this_thread::sleep_until(next);
                if (clock::now() > next + interval*4) next = clock::now();
            }
        }); } catch(...) {running_=false;ahead_->stop();throw;}
        phase_ = DevicePhase::running;
    }
    void stop() override {
        running_.store(false,std::memory_order_release);
        if (thread_.joinable()) thread_.join();
        if(ahead_)ahead_->stop();
        if(engine_)engine_->quiesce();
        if (engine_) phase_ = DevicePhase::stopped;
    }
    void close() noexcept override {
        stop(); ahead_.reset();engine_.reset(); input_.clear(); output_.clear(); phase_ = DevicePhase::closed;
    }
    DeviceStatus status() override { return {phase_,static_cast<double>(config_.sample_rate),0,0,0,{}}; }
private:
    DeviceConfig config_;
    DevicePhase phase_{DevicePhase::closed};
    std::atomic<bool> running_{};
    std::thread thread_;
    std::shared_ptr<AudioEngine> engine_;
    std::unique_ptr<AheadRenderer> ahead_;
    std::vector<float> input_,output_;
};
}
std::unique_ptr<IAudioDevice> make_offline_device() { return std::make_unique<OfflineDevice>(); }
}
