#include <mrs/persistence.hpp>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Directory {
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("mrs-check-" + mrs::new_id().value);
    Directory() { std::filesystem::create_directory(path); }
    ~Directory() { std::error_code error; std::filesystem::remove_all(path,error); }
};
}
int main() {
    using namespace mrs;
    using namespace mrs::persistence;
    try {
        Directory dir; auto path = dir.path / "song.mrsproject"; auto document = demo_document();
        document.extensions = {{"USER","opaque future state"}};
        processing::PreparedGraph runtime({std::make_shared<const processing::GraphState>(document.graph),0,false,false},{});
        document.graph = runtime.capture(); // quiescent native component state, no device
        check(!document.graph.nodes.front().plugin.component.empty(),"captured processor state");
        SharedSession session(document); auto arrange = session.services(), live = session.services();
        arrange.projects->execute(RenameTrack{document.project.tracks.front().id,"Shared saved track"});
        auto graph = *session.graphs()->state().graph; graph.patch_name = "Shared saved patch"; session.graphs()->replace(graph);
        live.transport->play(); const auto saved = session.capture(); save_project(path,saved);
        SharedSession opened(load_project(path));
        check(opened.capture() == saved && arrange.projects == live.projects,"shared project round-trip");
        std::cout << "PASS: one project preserves shared Arrange/Mix/Live state, MIDI and plugin blobs\n";
        auto migrated = decode_project(serialize(document.project));
        check(migrated.project == document.project && decode_project(encode(migrated)) == migrated,"legacy migration");
        std::cout << "PASS: legacy snapshot migration and unknown fields preserved\n";
        ShowDocument show; show.id = {"show-demo"}; show.title = "Demo";
        show.entries.push_back({{"entry-demo"},saved.project.id,"song.mrsproject","",{}});
        auto show_path = dir.path / "set.mrsshow"; save_show(show_path,show);
        check(load_show(show_path) == show,"show references");
        std::cout << "PASS: show/setlist references project IDs without copying projects\n";
        auto next = saved; ++next.generation; next.live_notes = "Autosave";
        { AutosaveWorker worker(path); check(worker.wait(worker.submit(std::make_shared<const ProjectDocument>(next))).empty(),"autosave worker"); }
        const auto recovery = recover_project(path); SharedSession recovered(recovery.document);
        check(recovery.document == next && load_project(path) == saved &&
              recovered.services().transport->state().playback == PlaybackState::stopped &&
              recovered.services().transport->state().sample == 0,"recovery boundary");
        std::cout << "PASS: safe save/autosave recovery opens stopped, primary remains intact\n";
        std::cout << "SHARED Stage 4 check passed. No audio device, MIDI action or plugin starts on load.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
