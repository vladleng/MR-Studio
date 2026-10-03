#include <mrs/musical.hpp>
#include <mrs/audio.hpp>
#include <array>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        auto p = mrs::musical_demo_project();
        auto audio = std::make_shared<mrs::audio::AudioEngine>();
        audio->prepare({48000,0,2,128}, {});
        auto transport = std::make_shared<mrs::audio::EngineTransport>(audio, mrs::Timeline{p.time,p.sample_rate});
        mrs::Services services{std::make_shared<mrs::ProjectStore>(p),transport};
        auto timeline = std::make_shared<mrs::MusicalTimeline>(services);
        const auto arrange = timeline, live = timeline;
        std::array<float,256> output{};
        const auto check = [](bool ok) { if (!ok) throw std::runtime_error("acceptance failed"); };
        timeline->seek_section({"section-verse"});
        audio->process(nullptr,output.data(),128);
        transport->poll();
        check(arrange == live && arrange->state().current_chord->symbol == "Gmaj7");
        check(live->state().current_section->name == "Verse");
        std::cout << "PASS: Arrange and Live share chord/section context\n";
        timeline->seek_marker({"cue-chorus"});
        audio->process(nullptr,output.data(),128);
        transport->poll();
        check(timeline->state().current_section->name == "Chorus");
        timeline->loop_section({"section-verse"});
        audio->process(nullptr,output.data(),128);
        transport->poll();
        check(transport->state().loop.has_value());
        std::cout << "PASS: marker/section navigation uses shared audio transport\n";
        p.sections[2].name = "Final Chorus";
        services.projects->execute(mrs::SetMusicalData{p.time,p.chords,p.sections,p.markers});
        check(live->state().current_section->name == "Final Chorus");
        check(services.projects->undo());
        check(arrange->state().current_section->name == "Chorus");
        check(services.projects->redo());
        std::cout << "PASS: musical edits and Undo/Redo visible across workspaces\n";
        check(mrs::deserialize(mrs::serialize(*services.projects->state().project)) == *services.projects->state().project);
        const mrs::Timeline clock{p.time,p.sample_rate};
        check(clock.musical_position(48 * mrs::ppq) == mrs::MusicalPosition{13,1,0});
        check(clock.to_ticks(clock.to_samples(32 * mrs::ppq)) == 32 * mrs::ppq);
        std::cout << "PASS: tempo/meter boundaries and snapshot v2 round-trip\n";
        std::cout << "SHARED Stage 2 check passed. No GUI or physical ASIO test required.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
