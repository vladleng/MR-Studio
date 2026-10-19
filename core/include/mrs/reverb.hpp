#pragma once
#include <mrs/processing.hpp>
#include <array>
#include <stop_token>

namespace mrs::processing {
enum class ReverbRouting { mono, stereo, true_stereo };
enum class ReverbQuadMap { unspecified, ll_lr_rl_rr };
// Original, unnormalised samples. In True Stereo: input first, output second:
// LL, LR, RL, RR. A quad file needs an explicitly confirmed map.
struct ReverbImpulse {
    std::uint32_t sample_rate{48000};
    ReverbRouting routing{ReverbRouting::mono};
    std::array<std::vector<float>,4> kernels;
    void validate() const;
};
ReverbImpulse reverb_impulse(std::uint32_t rate, std::uint32_t channels,
    std::span<const float> interleaved, ReverbQuadMap = ReverbQuadMap::unspecified);
ReverbImpulse reverb_pair(const ReverbImpulse& left, const ReverbImpulse& right);

// NON-RT movable prepared kernel. Preparation does not publish to an audio owner;
// cancelled/superseded requests can be discarded without ever becoming audible.
class ReverbKernel {
public:
    ~ReverbKernel();
    ReverbKernel(ReverbKernel&&) noexcept;
    ReverbKernel& operator=(ReverbKernel&&) noexcept;
private:
    friend class ReverbProcessor;
    struct Impl;
    explicit ReverbKernel(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};

// One NON-RT producer; one DSP consumer. The graph/device must be quiescent for
// prepare/destruction/capture. stage_impulse prepares off-thread and publishes a
// pointer; process crossfades; collect_retired frees only acknowledged old owners.
// This DSP slice does not persist external-file selection (library/state #130/#131).
class ReverbProcessor final : public IProcessor {
public:
    explicit ReverbProcessor(bool cab=false,bool rig=false);
    ~ReverbProcessor() override;
    std::vector<ParameterInfo> parameters() const override;
    void prepare(ProcessConfig) override;
    void restore(const PluginState&) override;
    PluginState capture() const override;
    bool set_parameter(std::uint32_t,float) noexcept override;
    std::optional<float> parameter_value(std::uint32_t) const noexcept override;
    std::uint32_t latency() const noexcept override { return 0; }
    bool live_safe() const noexcept override { return false; } // no measured Live gate yet
    bool anticipation_safe() const noexcept override { return false; } // kernel handoff is external
    void warm() override;
    void reset() noexcept override;
    void advance_bypass(std::uint32_t) noexcept override;
    void process(ProcessBlock) noexcept override;
    bool stage_impulse(const ReverbImpulse&); // false = previous handoff still in flight
    static ReverbKernel prepare_kernel(const ReverbImpulse&,ProcessConfig,std::stop_token = {});
    bool stage_kernel(ReverbKernel&); // NON-RT; consumes only on success; one producer
    void collect_retired(); // NON-RT; also called before staging
    std::array<float,2> levels() const noexcept;
    bool switching() const noexcept;
    std::size_t prepared_bytes() const noexcept; // NON-RT diagnostics
    std::uint64_t tail_frames() const noexcept; // quiescent; IR + wet delay + Tone settling bound
private:
    bool cab_{},rig_{};
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::unique_ptr<IProcessor> reverb_factory();
} // namespace mrs::processing
