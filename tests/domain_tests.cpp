#include <mrs/audio.hpp>
#include <mrs/processing.hpp>
#include <mrs/recording.hpp>
#include "latency_fixture.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mrs;
using namespace mrs::audio;
using namespace mrs::processing;
using D = ProcessingDomains;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class UnsafeDelay final : public IProcessor {
    LatencyFixture dsp_;
public:
    explicit UnsafeDelay(std::uint32_t latency) : dsp_(latency) {}
    std::vector<ParameterInfo> parameters() const override { return {}; }
    void prepare(ProcessConfig c) override { dsp_.prepare(c); }
    void restore(const PluginState&) override {}
    PluginState capture() const override { return {}; }
    bool set_parameter(std::uint32_t,float) noexcept override { return false; }
    std::optional<float> parameter_value(std::uint32_t) const noexcept override { return {}; }
    std::uint32_t latency() const noexcept override { return dsp_.latency(); }
    bool live_safe() const noexcept override { return true; }
    bool anticipation_safe() const noexcept override { return false; }
    void warm() override {}
    void reset() noexcept override { dsp_.reset(); }
    void process(ProcessBlock b) noexcept override { dsp_.process(b); }
};
std::shared_ptr<PreparedGraph> chain(std::uint32_t latency, bool safe = true) {
    auto state = demo_graph(); state.nodes.front().parameters.clear();
    return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(state),0,false,false},
        ProcessConfig{48000,2,128}, [=](const auto&) -> std::unique_ptr<IProcessor> {
            if (safe) return std::make_unique<LatencyFixture>(latency);
            return std::make_unique<UnsafeDelay>(latency);
        });
}
RenderGraph mixed() {
    RenderGraph graph;
    graph.mixer.resize(6); graph.buses = {false,false,true,true,false,false};
    graph.outputs = {2,2,3,no_mixer_track,no_mixer_track,no_mixer_track};
    graph.hardware_outputs = {{},{},{},{},{1},{0,1}};
    graph.sends = {{{3,.2f,true}},{{3,.3f,false}},{},{},{},{}};
    graph.inserts = {chain(3),chain(11),chain(7),chain(5),chain(1),chain(2)};
    graph.master_inserts = chain(128);
    graph.monitor = {{0,0,1,1},{1,1,1,4}};
    graph.input_monitoring = {false,false,false,false,false,false};
    graph.monitoring = false; // Potential input still reserves its full closure.
    return graph;
}
void closure() {
    AudioEngine engine; engine.prepare({48000,2,2,128}, mixed());
    const auto& p = engine.processing_domains();
    check(p.channels[0].owner == D::Owner::ahead && p.channels[5].owner == D::Owner::ahead, "independent playback excluded");
    for (auto t : {1,2,3,4}) check(p.channels[t].owner == D::Owner::device && p.channels[t].reasons == D::live_input, "live closure incomplete");
    check(p.master.owner == D::Owner::device && p.master.reasons == D::live_input, "live master not reserved");
    MixerUpdate update; update.count = 6; update.input_monitoring[1] = true;
    check(engine.enqueue_mix(update) && engine.enqueue({ControlKind::monitor,1}), "toggle queues");
    std::array<float,256> in{}, out{}; engine.process(in.data(),out.data(),128);
    check(p.channels[0].owner == D::Owner::ahead && p.channels[1].owner == D::Owner::device, "toggle migrated prepared ownership");
    auto isolated = mixed(); isolated.monitor = {{0,0,1,4}};
    engine.prepare({48000,2,2,128}, std::move(isolated));
    check(engine.processing_domains().master.owner == D::Owner::ahead, "direct hardware monitoring contaminated Master");
    auto legacy = mixed(); legacy.monitor = {{0,0,1,no_mixer_track}}; legacy.monitor_track = 0;
    engine.prepare({48000,2,2,128}, legacy);
    check(engine.processing_domains().channels[0].reasons == D::live_input, "legacy input track missed");
    legacy.monitor_track = no_mixer_track; engine.prepare({48000,2,2,128}, legacy);
    check(engine.processing_domains().channels[0].owner == D::Owner::ahead && engine.processing_domains().master.owner == D::Owner::device, "legacy Master input closure");
}
void boundaries() {
    AudioEngine engine; auto graph = mixed(); graph.monitor.clear(); graph.inserts[2] = chain(7,false);
    engine.prepare({48000,0,2,128}, graph);
    const auto& p = engine.processing_domains();
    check(p.channels[0].owner == D::Owner::ahead && p.channels[1].owner == D::Owner::ahead, "unsupported processor pulled upstream DSP");
    check(p.channels[2].reasons == D::unsupported_processor && p.channels[3].reasons == D::unsupported_processor, "unsupported closure incomplete");
    check(p.master.owner == D::Owner::device, "unsupported output reached anticipative Master");
    check(p.merges.size() == 6, "main/send/hardware merge count");
    for (const auto& merge : p.merges) {
        check(p.channels[merge.source].owner == D::Owner::ahead, "merge source has device owner");
        if (merge.destination != no_mixer_track) check(p.channels[merge.destination].owner == D::Owner::device, "merge destination has producer owner");
    }
    auto live_and_unsafe = mixed(); live_and_unsafe.inserts[2] = chain(7,false);
    engine.prepare({48000,2,2,128}, live_and_unsafe);
    check(engine.processing_domains().channels[2].reasons == (D::live_input | D::unsupported_processor), "closure lost eligibility reasons");
    graph.inserts[2] = chain(7); graph.master_inserts = chain(128,false);
    engine.prepare({48000,0,2,128}, graph);
    check(engine.processing_domains().channels[3].owner == D::Owner::ahead && engine.processing_domains().master.owner == D::Owner::device, "unsafe Master must preserve upstream ownership");
    auto asset = std::make_shared<AudioData>(sine_fixture(48000,2,128,300));
    graph.voices = {{asset,0,0,128,{{0,0,1}},{},no_mixer_track}};
    engine.prepare({48000,0,2,128}, graph);
    check(engine.processing_domains().merges.back().kind == D::MergeKind::legacy, "legacy playback merge missing");
    graph.master_inserts = chain(128); engine.prepare({48000,0,2,128}, graph);
    check(engine.processing_domains().merges.back().kind == D::MergeKind::master_output, "producer Master terminal missing");
    auto alias = graph; alias.inserts[1] = alias.inserts[0]; bool rejected = false;
    try { engine.prepare({48000,0,2,128}, alias); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "two domains permitted aliased stateful instance");
    auto cycle = graph; cycle.outputs[3] = 2; rejected = false;
    try { engine.prepare({48000,0,2,128}, cycle); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "cyclic closure accepted");
}
void latency() {
    AudioEngine engine; engine.prepare({48000,2,2,128}, mixed());
    const auto& p = engine.processing_domains();
    check(p.monitoring_compensation_frames == 151, "mixed monitoring PDC report");
    check(p.merges.size() == 3, "mixed boundary count");
    for (const auto& m : p.merges) {
        if (m.kind == D::MergeKind::main) check(m.compensation_frames == 8 && m.destination == 2, "bus merge PDC");
        else if (m.kind == D::MergeKind::send) check(m.compensation_frames == 15 && m.destination == 3 && m.send_index == 0, "pre-fader send PDC");
        else check(m.kind == D::MergeKind::hardware && m.compensation_frames == 149, "direct hardware PDC includes Master alignment");
    }
    auto empty = RenderGraph{}; engine.prepare({48000,0,2,128}, empty);
    check(engine.processing_domains().channels.empty() && engine.processing_domains().monitoring_compensation_frames == 0, "stale domains on reprepare");
    auto graph = mixed(); graph.monitor.clear(); engine.prepare({48000,0,2,128}, graph);
    check(engine.processing_domains().monitoring_compensation_frames == 0, "unmonitored graph reports monitoring latency");
    const auto path = std::filesystem::temp_directory_path() / ("mrs-domain-raw-" + new_id().value + ".wav");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove(path,ignored); }
    } cleanup{path};
    auto recorder = std::make_shared<Recorder>(path,48000,0,std::vector<std::uint32_t>{0,1});
    graph.recording = recorder;
    engine.prepare({48000,2,2,128}, graph);
    check(engine.processing_domains().raw_capture && engine.processing_domains().channels[0].owner == D::Owner::ahead,
        "raw capture unnecessarily reserves unrelated playback DSP");
    check(!engine.anticipation_safe(), "candidate plan enabled mixed rendering prematurely");
    std::array<float,256> input{}, output{}; input[0] = .125f; input[1] = .25f;
    check(engine.enqueue({ControlKind::play}), "capture play"); engine.process(input.data(),output.data(),128);
    engine.prepare({48000,0,2,128}, {});
    check(!engine.processing_domains().raw_capture && recorder->finish().frames == 128, "capture plan lifetime");
    const auto raw = load_wav(path);
    check(raw.samples[0] == input[0] && raw.samples[1] == input[1], "raw capture gained PDC or effects");
}
}
int main(int argc, char** argv) {
    try {
        const std::string suite = argc > 1 ? argv[1] : "";
        if (suite == "closure") closure();
        else if (suite == "boundaries") boundaries();
        else if (suite == "latency") latency();
        else throw std::runtime_error("unknown suite");
        std::cout << suite << " PASS\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
