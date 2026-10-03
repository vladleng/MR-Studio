#include <mrs/core.hpp>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main() {
    try {
        const auto services = mrs::demo_services();
        // Arrange and Live receive the same services, not separate mocks.
        const auto arrange = services;
        const auto live = services;
        require(arrange.projects == live.projects && arrange.transport == live.transport, "services diverged");
        unsigned arrange_events = 0;
        unsigned live_events = 0;
        auto a = arrange.transport->subscribe([&](const auto&) { ++arrange_events; });
        auto l = live.transport->subscribe([&](const auto&) { ++live_events; });
        arrange.transport->play();
        live.transport->seek(48000);
        require(arrange.transport->state().sample == 48000, "seek not shared");
        live.transport->pause();
        require(arrange_events == 3 && live_events == 3, "state subscriptions diverged");
        arrange.projects->execute(mrs::RenameTrack{{"track-guitar"}, "Guitar - lead"});
        require(live.projects->state().project->tracks.back().name == "Guitar - lead", "project edit not shared");
        require(live.projects->undo(), "undo unavailable");
        require(arrange.projects->state().project->tracks.back().name == "Guitar", "undo not shared");
        require(arrange.projects->redo(), "redo unavailable");
        const auto before = *services.projects->state().project;
        require(mrs::deserialize(mrs::serialize(before)) == before, "snapshot round-trip failed");
        live.transport->stop();
        std::cout << "PASS: Arrange and Live share Project/Transport services\n"
                  << "PASS: subscriptions, play/seek/pause/stop\n"
                  << "PASS: commands and Undo/Redo visible across workspaces\n"
                  << "PASS: snapshot v2 round-trip preserves stable IDs\n"
                  << "SHARED Stage 0 check passed. No audio engine or GUI in this build.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
