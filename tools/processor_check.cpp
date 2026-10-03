#include <mrs/processing.hpp>
#include <mrs/audio.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        using namespace mrs::processing;
        const auto check = [](bool ok) { if (!ok) throw std::runtime_error("processor acceptance failed"); };
        auto shared = std::make_shared<GraphStore>(demo_graph());
        const auto arrange = shared, live = shared;
        auto patch = *arrange->state().graph;
        patch.patch_name = "Lead"; patch.nodes[0].parameters[0].value = 2;
        arrange->replace(patch);
        check(live->state().graph->patch_name == "Lead");
        check(live->undo() && arrange->state().graph->patch_name == "Clean");
        check(arrange->redo());
        std::cout << "PASS: Arrange and Live share graph/patch state and Undo/Redo\n";
        auto processor = std::make_shared<PreparedGraph>(shared->state(),ProcessConfig{48000,2,128});
        MockMidiDevice midi; midi.open({"mock-midi"});
        midi.inject({0,MidiKind::note_on,0,60,100});
        MidiEvent event; check(midi.receive(event));
        check(processor->enqueue_midi({"mock-midi"},event));
        check(processor->enqueue_parameter({"gain"},{64,0,0.5F}));
        mrs::audio::RenderGraph graph; graph.monitor = {{0,0,1},{0,1,1}}; graph.processors = processor;
        mrs::audio::AudioEngine audio; audio.prepare({48000,1,2,128},graph);
        std::array<float,128> input; input.fill(0.2F); std::array<float,256> output{};
        audio.process(input.data(),output.data(),128);
        check(std::abs(output[0]-0.4F)<0.00001F && output[128] == 0.1F);
        std::cout << "PASS: native processing and sample-offset automation in shared AudioEngine\n";
        MidiOutput routed; check(processor->pop_midi_output(routed));
        check(routed.event.data1 == 60 && routed.event.kind == MidiKind::note_on);
        midi.send(routed.event);
        processor->panic(); audio.process(input.data(),output.data(),128);
        check(processor->metrics().panics == 1);
        std::cout << "PASS: MIDI routing, device abstraction and panic\n";
        const auto saved = processor->capture();
        GraphStore restored_store{saved};
        PreparedGraph restored{restored_store.state(),{48000,2,128}};
        output.fill(0.2F); restored.process(output.data(),128);
        check(output[0] == 0.1F);
        check(restored.latency().output == 0 && restored.latency().live_safe);
        std::cout << "PASS: patch capture/restore and latency metadata\n";
        std::cout << "SHARED Stage 3 check passed. VST3 host, hardware MIDI and GUI are future stages.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
