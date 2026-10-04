#include <mrs/processing.hpp>
#include <mrs/audio.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> probing{};
std::atomic<std::size_t> allocations{};
}
void* operator new(std::size_t n) {
    if (probing) ++allocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
namespace {
using namespace mrs::processing;
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
template<class F> void rejects(F f) {
    bool thrown = false; try { f(); } catch (const std::exception&) { thrown = true; } CHECK(thrown);
}
GraphSnapshot snapshot(GraphState state = demo_graph()) {
    state.validate();
    return {std::make_shared<const GraphState>(std::move(state)),0,false,false};
}
void midi() {
    MockMidiDevice device;
    CHECK(device.ports()[0].input && device.ports()[0].output);
    rejects([&] { device.open({"absent"}); });
    device.open({"mock-midi"});
    device.inject({3,MidiKind::note_on,2,60,0});
    MidiEvent event; CHECK(device.receive(event)); CHECK(!device.receive(event));
    auto g = demo_graph(); g.midi_routes[0].input_channel = 2; g.midi_routes[0].output_channel = 7; g.midi_routes[0].transpose = 12;
    PreparedGraph runtime{snapshot(g),{48000,2,128}};
    CHECK(runtime.enqueue_midi({"mock-midi"},event));
    std::array<float,256> audio{}; runtime.process(audio.data(),128);
    MidiOutput out; CHECK(runtime.pop_midi_output(out));
    CHECK(out.event.kind == MidiKind::note_off && out.event.channel == 7 && out.event.data1 == 72 && out.event.offset == 3);
    device.send(out.event); CHECK(device.sent().size() == 1);
    event.channel = 3; CHECK(runtime.enqueue_midi({"mock-midi"},event));
    runtime.process(audio.data(),128); CHECK(!runtime.pop_midi_output(out));
    event.channel = 2; event.data1 = 127;
    runtime.enqueue_midi({"mock-midi"},event);
    runtime.process(audio.data(),128); CHECK(!runtime.pop_midi_output(out));
    rejects([] { MidiEvent{0,MidiKind::cc,16,0,0}.validate(); });
    device.close(); CHECK(!device.connected());
    rejects([&] { device.send(event); });
    MidiBuffer full;
    for (std::size_t i = 0; i < event_capacity; ++i) CHECK(full.push({}));
    CHECK(!full.push({}));
}
void model() {
    const auto invalid = [](auto edit) { auto g = demo_graph(); edit(g); rejects([&] { g.validate(); }); };
    invalid([](auto& g) { g.nodes[0].id = g.id; });
    invalid([](auto& g) { g.nodes.push_back(g.nodes[0]); });
    invalid([](auto& g) { g.edges[0].to = {"absent"}; });
    invalid([](auto& g) { g.edges[0].from = g.nodes[0].id; });
    invalid([](auto& g) { g.edges.push_back(g.edges[0]); });
    invalid([](auto& g) { g.outputs.push_back(g.outputs[0]); });
    invalid([](auto& g) { g.midi_routes[0].input_channel = 16; });
    invalid([](auto& g) { g.nodes[0].parameters.push_back({0,1}); });
    invalid([](auto& g) { g.nodes[0].parameters[0].value = std::numeric_limits<float>::quiet_NaN(); });
    invalid([](auto& g) { g.nodes[0].format = ProcessorFormat::vst3; g.nodes[0].plugin.class_id = "invalid"; });
    auto unsupported = demo_graph(); unsupported.nodes[0].processor_id = "missing";
    rejects([&] { PreparedGraph graph{snapshot(unsupported),{}}; });
    rejects([] { PreparedGraph graph{snapshot(),{0,2,128}}; });
    rejects([] { PreparedGraph graph{snapshot(),{48000,0,128}}; });
    auto large = demo_graph();
    for (int i = 1; i < 32; ++i) { auto n = large.nodes[0]; n.id = {"node"+std::to_string(i)}; large.nodes.push_back(n); }
    rejects([&] { PreparedGraph graph{snapshot(large),{48000,64,65536}}; });
}
GraphState two_nodes() {
    auto g = demo_graph(); auto n = g.nodes[0]; n.id = {"second"}; n.parameters = {{0,2}};
    g.nodes.push_back(n); g.edges.push_back({g.nodes[0].id,n.id,1}); g.outputs = {n.id}; return g;
}
void graph() {
    auto g = two_nodes(); PreparedGraph runtime{snapshot(g),{48000,2,8}};
    std::array<float,16> audio; audio.fill(0.4F); runtime.process(audio.data(),8);
    for (auto value : audio) CHECK(value == 0.4F); // serial 0.5 * 2
    g.edges.push_back({std::nullopt,g.nodes[1].id,0.5F});
    PreparedGraph mixed{snapshot(g),{48000,2,8}};
    audio.fill(0.4F); mixed.process(audio.data(),8);
    for (auto value : audio) CHECK(value == 0.8F); // sum then second gain
    g.nodes[1].bypass = true;
    PreparedGraph bypass{snapshot(g),{48000,2,8}};
    audio.fill(0.4F); bypass.process(audio.data(),8);
    for (auto value : audio) CHECK(value == 0.4F);
    GraphState empty; empty.id = {"empty"};
    PreparedGraph transparent{snapshot(empty),{48000,2,8}};
    audio.fill(0.3F); transparent.process(audio.data(),8); CHECK(audio[0] == 0.3F);
    auto cycle = two_nodes(); cycle.edges.push_back({cycle.nodes[1].id,cycle.nodes[0].id,1});
    rejects([&] { cycle.validate(); });
}
void parameters() {
    PreparedGraph runtime{snapshot(),{48000,2,8}};
    CHECK(runtime.enqueue_parameter({"gain"},{4,0,2}));
    CHECK(runtime.enqueue_parameter({"gain"},{2,0,1}));
    CHECK(runtime.enqueue_parameter({"gain"},{4,0,3})); // same offset: final enqueue wins
    std::array<float,16> audio; audio.fill(0.2F); runtime.process(audio.data(),8);
    CHECK(audio[0] == 0.1F && audio[4] == 0.2F);
    CHECK(std::abs(audio[8]-0.6F) < 0.00001F);
    rejects([&] { runtime.enqueue_parameter({"gain"},{0,99,1}); });
    rejects([&] { runtime.enqueue_parameter({"gain"},{0,0,5}); });
    rejects([&] { runtime.enqueue_parameter({"absent"},{0,0,1}); });
    CHECK(runtime.capture().nodes[0].parameters[0].value == 3);
}
void state() {
    auto shared = std::make_shared<GraphStore>(demo_graph());
    const auto arrange = shared, live = shared;
    int a = 0, l = 0;
    auto ac = arrange->subscribe([&](const auto&) { ++a; });
    auto lc = live->subscribe([&](const auto&) { ++l; });
    const auto original = shared->state().graph;
    auto patch = *original; patch.patch_name = "Lead"; patch.nodes[0].parameters[0].value = 2;
    shared->replace(patch); CHECK(a == 1 && l == 1 && live->state().graph->patch_name == "Lead");
    CHECK(original->patch_name == "Clean"); CHECK(shared->undo() && live->state().graph == original);
    CHECK(shared->redo() && arrange->state().graph->patch_name == "Lead");
    PreparedGraph runtime{shared->state(),{48000,2,8}};
    CHECK(runtime.enqueue_parameter({"gain"},{0,0,3}));
    std::array<float,16> audio; audio.fill(0.1F); runtime.process(audio.data(),8);
    auto captured = runtime.capture();
    PreparedGraph restored{snapshot(captured),{48000,2,8}};
    audio.fill(0.1F); restored.process(audio.data(),8);
    CHECK(std::abs(audio[0]-0.3F) < 0.00001F);
    const auto revision = shared->state().revision;
    patch.nodes[0].id = patch.id;
    rejects([&] { shared->replace(patch); }); CHECK(shared->state().revision == revision);
    shared->replace(*shared->state().graph); CHECK(shared->state().revision == revision);
    lc.disconnect();
    shared->undo(); CHECK(l == 3);
}
class MockProcessor final : public IProcessor {
    std::uint32_t latency_;
    PluginState saved_;
public:
    bool prepared{}, warmed{};
    explicit MockProcessor(std::uint32_t latency) : latency_(latency) {}
    std::vector<ParameterInfo> parameters() const override { return {}; }
    void prepare(ProcessConfig) override { prepared = true; }
    void restore(const PluginState& saved) override { CHECK(prepared); saved_ = saved; }
    PluginState capture() const override { return saved_; }
    bool set_parameter(std::uint32_t,float) noexcept override { return false; }
    std::optional<float> parameter_value(std::uint32_t) const noexcept override { return {}; }
    std::uint32_t latency() const noexcept override { return latency_; }
    bool live_safe() const noexcept override { return true; }
    void warm() override { CHECK(prepared); warmed = true; }
    void reset() noexcept override {}
    void process(ProcessBlock block) noexcept override {
        for (auto e : block.midi) (void)block.midi_output.push(e);
    }
};
void latency() {
    auto g = two_nodes();
    for (auto& n : g.nodes) { n.parameters.clear(); n.processor_id = "mock"; }
    PreparedGraph serial{snapshot(g),{48000,2,8,128},[](const auto&) { return std::make_unique<MockProcessor>(80); }};
    CHECK(serial.latency().output == 160 && !serial.latency().live_safe);
    g.edges[1].from.reset(); g.outputs = {g.nodes[0].id,g.nodes[1].id};
    PreparedGraph parallel{snapshot(g),{48000,2,8,128},[](const auto& n) { return std::make_unique<MockProcessor>(n.id == mrs::Id{"gain"} ? 20U : 80U); }};
    CHECK(parallel.latency().output == 80 && parallel.latency().parallel_paths_need_compensation && !parallel.latency().live_safe);
    auto vst = demo_graph(); vst.nodes[0].format = ProcessorFormat::vst3;
    vst.nodes[0].parameters.clear(); vst.nodes[0].plugin.class_id = "0123456789ABCDEF0123456789ABCDEF";
    vst.nodes[0].plugin.component = {std::byte{1},std::byte{2}};
    vst.nodes[0].plugin.controller = {std::byte{3}};
    PreparedGraph mock_host{snapshot(vst),{48000,2,8},[](const auto&) { return std::make_unique<MockProcessor>(0); }};
    CHECK(mock_host.capture() == vst);
    rejects([&] { PreparedGraph real_host{snapshot(vst),{48000,2,8}}; }); // no SDK host yet
}
void engine() {
    auto processor = std::make_shared<PreparedGraph>(snapshot(),ProcessConfig{48000,2,8});
    auto asset = std::make_shared<mrs::audio::AudioData>(); asset->sample_rate = 48000; asset->channels = 1; asset->samples.assign(8,0.4F);
    mrs::audio::RenderGraph graph;
    graph.voices.push_back({asset,0,0,8,{{0,0,1},{0,1,1}}});
    graph.monitor = {{0,0,1},{0,1,1}}; graph.processors = processor;
    mrs::audio::AudioEngine audio; audio.prepare({48000,1,2,8},graph);
    std::array<float,8> input; input.fill(0.2F); std::array<float,16> output{};
    audio.process(input.data(),output.data(),8); CHECK(output[0] == 0.1F); // stopped monitoring still processed
    CHECK(audio.enqueue({mrs::audio::ControlKind::play}));
    audio.process(input.data(),output.data(),8); CHECK(std::abs(output[0]-0.3F)<0.00001F);
    processor->enqueue_parameter({"gain"},{0,0,4});
    audio.process(input.data(),output.data(),8); CHECK(output[0] == 0.8F); // playback source exhausted
    auto mismatch = graph;
    mismatch.processors = std::make_shared<PreparedGraph>(snapshot(),ProcessConfig{44100,2,8});
    rejects([&] { audio.prepare({48000,1,2,8},mismatch); });
    CHECK(audio.enqueue({mrs::audio::ControlKind::seek,0}));
    audio.process(input.data(),output.data(),8); CHECK(output[0] == 1 && audio.metrics().clipped_samples > 0);
}
void realtime() {
    PreparedGraph graph{snapshot(),{48000,2,128}};
    for (int i = 0; i < 1023; ++i) CHECK(graph.enqueue_midi({"mock-midi"},{0,MidiKind::note_on,0,60,100}));
    CHECK(!graph.enqueue_midi({"mock-midi"},{0,MidiKind::note_off,0,60,0}));
    std::array<float,256> audio{}; allocations = 0; probing = true;
    graph.process(audio.data(),128);
    probing = false; CHECK(allocations == 0);
    CHECK(graph.metrics().dropped_midi > 0 && graph.metrics().panics == 1);
    MidiOutput event; int panic_events = 0;
    while (graph.pop_midi_output(event)) { CHECK(event.event.kind == MidiKind::cc); ++panic_events; }
    CHECK(panic_events == 32);
    graph.enqueue_midi({"mock-midi"},{100,MidiKind::note_on,0,60,100});
    graph.process(audio.data(),64); CHECK(graph.metrics().invalid_events == 1 && graph.metrics().panics == 2);
    for (int i = 0; i < 1023; ++i) CHECK(graph.enqueue_parameter({"gain"},{0,0,1}));
    CHECK(!graph.enqueue_parameter({"gain"},{0,0,1}));
    graph.process(audio.data(),128); CHECK(graph.metrics().dropped_parameters > 0);
    // Output queue overflow remains explicit if the control pump stalls.
    for (int i = 0; i < 140; ++i) { graph.panic(); graph.process(audio.data(),128); }
    CHECK(graph.metrics().output_overflows > 0);
}

void filters() {
    using namespace mrs;
    const auto response=[](InsertKind kind,float db,double hz) {
        NativeInsert fx{new_id(),kind,db,1000,0.70710678f};
        PreparedGraph graph{snapshot(insert_graph(std::array{fx})),{48000,2,128}};
        std::array<float,256> audio{}; double sum{},reference{};
        for (int block=0;block<96;++block) {
            for (int f=0;f<128;++f) { const auto value=static_cast<float>(0.1*std::sin(2*3.141592653589793*hz*(block*128+f)/48000)); audio[static_cast<std::size_t>(f)*2]=value; audio[static_cast<std::size_t>(f)*2+1]=0; if (block>=32) reference+=value*value; }
            const auto before=allocations.load(); probing=true; graph.process(audio.data(),128); probing=false; CHECK(allocations.load()==before);
            for (int f=0;f<128;++f) { CHECK(std::isfinite(audio[static_cast<std::size_t>(f)*2]) && audio[static_cast<std::size_t>(f)*2+1]==0); if (block>=32) sum+=audio[static_cast<std::size_t>(f)*2]*audio[static_cast<std::size_t>(f)*2]; }
        }
        PreparedGraph restored{snapshot(graph.capture()),{48000,2,128}}; // opaque native state restore
        return std::sqrt(sum/reference);
    };
    CHECK(std::abs(response(InsertKind::lowpass,1,100)-1)<0.03); CHECK(response(InsertKind::lowpass,1,8000)<0.03);
    CHECK(response(InsertKind::highpass,1,100)<0.03); CHECK(response(InsertKind::highpass,1,8000)>0.95);
    CHECK(std::abs(response(InsertKind::eq,6,1000)-std::pow(10.0,6.0/20))<0.03);
    CHECK(std::abs(response(InsertKind::eq,0,1000)-1)<0.001);
    NativeInsert bypass{new_id(),InsertKind::lowpass,1,20,10,true}; PreparedGraph wire{snapshot(insert_graph(std::array{bypass})),{8000,2,128}};
    std::array<float,256> audio{}; audio[0]=.25f; audio[1]=-.75f; const auto exact=audio; wire.process(audio.data(),128); CHECK(audio==exact);
    bypass.bypass=false; bypass.frequency=20000; PreparedGraph low_rate{snapshot(insert_graph(std::array{bypass})),{8000,2,128}};
    for (int n=0;n<8;++n) { low_rate.process(audio.data(),128); for (auto sample:audio) CHECK(std::isfinite(sample)); }
}

void channel_eq(){
    mrs::NativeInsert fx;fx.id={"channel-eq"};fx.kind=mrs::InsertKind::channel_eq;
    auto g=insert_graph(std::array{fx});PreparedGraph runtime{snapshot(g),{48000,2,128}};
    std::array<float,256> buffer{};buffer[0]=1;runtime.process(buffer.data(),128);CHECK(buffer[0]==1&&buffer[1]==0);CHECK(std::abs(eq_response_db(fx,1000,48000))<1e-8);
    fx.bands[2].gain=12;fx.bands[2].q=1;CHECK(std::abs(eq_response_db(fx,1000,48000)-12)<0.00001);
    fx.bands[0].enabled=true;fx.bands[0].frequency=200;fx.bands[4].enabled=true;fx.bands[4].frequency=5000;
    CHECK(eq_response_db(fx,20,48000)<-35&&eq_response_db(fx,18000,48000)<-25);
    auto changed=insert_graph(std::array{fx});CHECK(runtime.enqueue_parameters(changed));allocations=0;probing=true;
    for(int n=0;n<64;++n){for(std::size_t i=0;i<buffer.size();i+=2){buffer[i]=static_cast<float>(std::sin((n*128+i/2)*0.1));buffer[i+1]=0;}runtime.process(buffer.data(),128);for(std::size_t i=0;i<buffer.size();i+=2){if(!std::isfinite(buffer[i])||buffer[i+1]!=0)std::abort();}}
    probing=false;CHECK(allocations==0);
    fx.bypass=true;CHECK(runtime.enqueue_parameters(insert_graph(std::array{fx})));buffer.fill(0.25f);runtime.process(buffer.data(),128);for(auto v:buffer)CHECK(v==0.25f);
    fx.bypass=false;fx.bands[2].frequency=20000;fx.bands[2].q=10;PreparedGraph low{snapshot(insert_graph(std::array{fx})),{8000,2,128}};for(int n=0;n<64;++n){buffer.fill(0.25f);low.process(buffer.data(),128);for(auto v:buffer)CHECK(std::isfinite(v));}
    auto p=mrs::demo_project();p.tracks.front().inserts={fx};CHECK(mrs::deserialize(mrs::serialize(p))==p);
    CHECK(runtime.enqueue_parameters(changed));for(int n=0;n<62;++n)CHECK(runtime.enqueue_parameters(changed));CHECK(!runtime.enqueue_parameters(changed));runtime.process(buffer.data(),128);CHECK(runtime.enqueue_parameters(changed));
}

}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected suite");
        const std::string suite = argv[1];
        if (suite == "channel_eq") channel_eq(); else if (suite == "filters") filters(); else if (suite == "midi") midi(); else if (suite == "model") model();
        else if (suite == "graph") graph(); else if (suite == "parameters") parameters();
        else if (suite == "state") state(); else if (suite == "latency") latency();
        else if (suite == "engine") engine(); else if (suite == "realtime") realtime();
        else throw std::runtime_error("unknown suite");
        std::cout << "PASS processing " << suite << '\n'; return 0;
    } catch (const std::exception& e) { probing = false; std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
