#include <mrs/processing.hpp>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace mrs::processing {
namespace {
class Gain final : public IProcessor {
    float gain_{1};
public:
    std::vector<ParameterInfo> parameters() const override { return {{0,0,4,1,true}}; }
    void prepare(ProcessConfig) override {}
    void restore(const PluginState& state) override {
        if ((!state.class_id.empty() && state.class_id != "mrs.gain") || !state.controller.empty() ||
            (!state.component.empty() && state.component.size() != sizeof(float)))
            throw std::invalid_argument("invalid native gain state");
        if (!state.component.empty()) {
            float value{};
            std::memcpy(&value,state.component.data(),sizeof(value));
            if (!set_parameter(0,value)) throw std::invalid_argument("invalid saved gain");
        }
    }
    PluginState capture() const override {
        PluginState state; state.class_id = "mrs.gain"; state.component.resize(sizeof(gain_));
        std::memcpy(state.component.data(),&gain_,sizeof(gain_)); return state;
    }
    bool set_parameter(std::uint32_t id, float value) noexcept override {
        if (id != 0 || !std::isfinite(value) || value < 0 || value > 4) return false;
        gain_ = value; return true;
    }
    std::optional<float> parameter_value(std::uint32_t id) const noexcept override {
        return id == 0 ? std::optional<float>{gain_} : std::nullopt;
    }
    std::uint32_t latency() const noexcept override { return 0; }
    bool live_safe() const noexcept override { return true; }
    void warm() override {}
    void reset() noexcept override {}
    void process(ProcessBlock block) noexcept override {
        std::size_t next = 0;
        for (std::uint32_t frame = 0; frame < block.frames; ++frame) {
            while (next < block.parameters.size() && block.parameters[next].offset == frame) {
                const auto& change = block.parameters[next++];
                (void)set_parameter(change.id,change.value);
            }
            const auto offset = static_cast<std::size_t>(frame) * block.channels;
            for (std::uint32_t ch = 0; ch < block.channels; ++ch) block.audio[offset+ch] *= gain_;
        }
        for (auto event : block.midi) (void)block.midi_output.push(event);
    }
};
}
std::unique_ptr<IProcessor> native_factory(const NodeState& state) {
    if (state.format != ProcessorFormat::native || state.processor_id != "mrs.gain")
        throw std::invalid_argument("processor unavailable; VST3 hosting belongs to MRS Stage 3");
    return std::make_unique<Gain>();
}
} // namespace mrs::processing
