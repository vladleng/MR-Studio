#include "LiveWorkspace.h"
#include "ProjectHome.h"
#include <windows.h>

namespace ui {
namespace {
void liveCheck(bool condition, const char *text) {
    if (!condition)
        throw std::runtime_error(text);
}
void livePump() {
    const auto deadline = juce::Time::getMillisecondCounter() + 120;
    while (juce::Time::getMillisecondCounter() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        juce::Thread::sleep(1);
    }
}
juce::Component *liveComponent(juce::Component &root, const juce::String &id) {
    if (root.getComponentID() == id)
        return &root;
    for (auto *child : root.getChildren())
        if (auto *found = liveComponent(*child, id))
            return found;
    return nullptr;
}
void livePreview(juce::Component &component, const juce::String &name) {
    for (const float scale : {1.f, 1.25f, 1.5f, 2.f}) {
        auto stream = juce::File::getCurrentWorkingDirectory()
                          .getChildFile("juce-live-interface-" + name + "-" +
                                        juce::String(static_cast<int>(scale * 100)) + ".png")
                          .createOutputStream();
        liveCheck(stream != nullptr, "Live preview stream");
        stream->setPosition(0);
        stream->truncate();
        liveCheck(
            juce::PNGImageFormat().writeImageToStream(
                component.createComponentSnapshot(component.getLocalBounds(), true, scale, juce::SoftwareImageType{}),
                *stream),
            "Live preview export");
    }
}
} // namespace
void liveInterfaceSmoke(const juce::File &fixture) {
    liveCheck(fixture.existsAsFile(), "Pass --show-fixture with the editor show fixture");
    const auto original = fixture.loadFileAsString();
    const auto imported = live::importDemo(original);
    const auto id = imported["items"][0]["id"].toString(), second = imported["items"][1]["id"].toString();
    liveCheck(imported["items"].size() == 3 && imported["folders"].size() == 2 && imported["channels"].size() == 4,
              "Import preserves authoring collections");
    liveCheck(static_cast<int>(imported["performance"]["itemCardLayout"]["height"]) == 53 &&
                  !static_cast<bool>(imported["performance"]["showOrganizer"]),
              "User card settings override bundled defaults");
    liveCheck(live::progress(imported, id) == 50, "Recursive organizer progress");
    liveCheck(!live::itemDirty(imported, id), "Imported saved Browser version stays saved");
    LiveLayout layout;
    liveCheck(layout.bounds("show.setlist", 1920, 1080) == juce::Rectangle<int>(0, 80, 380, 624),
              "Native geometry uses accepted v5");
    liveCheck(layout.bounds("control.transport", 1920, 1080).isEmpty() &&
                  layout.bounds("input.patches", 1920, 1080).isEmpty(),
              "Legacy layout blocks stay hidden");
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getNonexistentChildFile("mr-live-interface", "", false);
    liveCheck(folder.createDirectory().wasOk(), "Native show temporary folder");
    Desktop desktop(true, true);
    desktop.importLiveShow(fixture);
    liveCheck(desktop.liveDocument && desktop.project()->tracks.empty() && !desktop.liveDocument->currentLinked(),
              "Editor channels do not create a second or simulated audio backend");
    const auto engine = desktop.app.engine();
    const auto store = desktop.app.services().projects;
    desktop.action(30);
    liveCheck(desktop.displayState().playback != mrs::PlaybackState::playing && !desktop.message.isEmpty(),
              "Unlinked editor items cannot play simulated audio");
    desktop.message.clear();
    desktop.editLiveInterface("Workflow", [&](auto &state) { live::setWorkflow(state, id, "Wait", "P2", 0); });
    liveCheck(live::workflow(desktop.liveDocument->interfaceState, id)["status"].toString() == "Wait" &&
                  !live::itemDirty(desktop.liveDocument->interfaceState, id),
              "Status/priority are shared and do not dirty Browser content");
    desktop.action(10);
    liveCheck(live::workflow(desktop.liveDocument->interfaceState, id)["status"].toString() == "Todo",
              "Show metadata Undo");
    desktop.action(11);
    liveCheck(live::workflow(desktop.liveDocument->interfaceState, id)["status"].toString() == "Wait",
              "Show metadata Redo");
    const auto saved = folder.getChildFile("interface.mrlive");
    desktop.saveFile(saved);
    liveCheck(!desktop.documentDirty() && desktop.liveDocument->projects.empty(),
              "Metadata save creates no phantom song");
    const auto savedName = live::libraryItem(desktop.liveDocument->interfaceState, id)["name"].toString();
    desktop.editLiveInterface(
        "Name", [&](auto &state) { live::put(live::require(state, "items", id), "name", "Edited Alpha"); });
    liveCheck(live::itemDirty(desktop.liveDocument->interfaceState, id) &&
                  live::libraryItem(desktop.liveDocument->interfaceState, id)["name"].toString() == savedName,
              "Browser keeps its saved content separate from working item");
    desktop.action(10);
    liveCheck(!desktop.documentDirty(), "Undo to saved metadata clears dirty state");
    desktop.action(11);
    desktop.editLiveInterface("Save for All", [&](auto &state) { live::saveItem(state, id); });
    liveCheck(!live::itemDirty(desktop.liveDocument->interfaceState, id), "Save for All updates Browser version");
    desktop.saveFile(saved);
    desktop.action(10);
    liveCheck(desktop.documentDirty() && live::itemDirty(desktop.liveDocument->interfaceState, id),
              "Saving preserves metadata Undo and saved-point dirty tracking");
    desktop.action(11);
    liveCheck(!desktop.documentDirty(), "Redo after Save returns to saved metadata");
    desktop.editLiveInterface("Move folder",
                              [&](auto &state) { live::put(live::require(state, "items", id), "folder", {}); });
    liveCheck(!live::itemDirty(desktop.liveDocument->interfaceState, id),
              "Folder bookkeeping does not dirty item content");
    const auto before = live::canonical(desktop.liveDocument->interfaceState);
    bool rejected = false;
    try {
        desktop.editLiveInterface("Cycle", [](auto &state) {
            live::put(live::require(state, "folders", "fixture-folder-a"), "parent", "fixture-folder-b");
        });
    } catch (const std::exception &) {
        rejected = true;
    }
    liveCheck(rejected && live::canonical(desktop.liveDocument->interfaceState) == before,
              "Folder cycles fail atomically");
    desktop.editLiveInterface("Task status", [&](auto &state) {
        auto &task = live::require(live::map(live::performance(state), "itemTasks"), id, "task-b");
        live::put(task, "status", "Done");
    });
    liveCheck(live::progress(desktop.liveDocument->interfaceState, id) == 67 &&
                  !live::itemDirty(desktop.liveDocument->interfaceState, id),
              "Organizer branches recompute without Save for All");
    desktop.action(10);
    liveCheck(live::progress(desktop.liveDocument->interfaceState, id) == 50, "Organizer Undo");
    desktop.action(11);
    desktop.editLiveInterface("Remove task", [&](auto &state) { live::removeTask(state, id, "task-root-a"); });
    liveCheck(live::tasks(desktop.liveDocument->interfaceState, id).size() == 4, "Task removal includes descendants");
    desktop.action(10);
    liveCheck(live::tasks(desktop.liveDocument->interfaceState, id).size() == 7, "Task subtree Undo");
    liveCheck(desktop.app.engine() == engine && desktop.app.services().projects == store,
              "Show authoring retains shared engine and Project Model");
    desktop.editLiveInterface("Card layout", [](auto &state) {
        live::put(live::property(live::performance(state), "itemCardLayout"), "height", 64);
    });
    desktop.action(10);
    liveCheck(static_cast<int>(desktop.liveDocument->interfaceState["performance"]["itemCardLayout"]["height"]) == 53,
              "Card constructor one-step Undo");
    auto *native = dynamic_cast<LiveWorkspace *>(liveComponent(desktop, "mr-live-workspace"));
    liveCheck(native != nullptr, "Native show workspace exists");
    livePump();
    native->showBrowser(true);
    livePump();
    auto *list = dynamic_cast<LiveSetlist *>(liveComponent(desktop, "live-setlist"));
    liveCheck(list != nullptr, "Native Setlist exists");
    const int count = live::activeSetlist(desktop.liveDocument->interfaceState)["entries"].size();
    auto payload = live::object();
    live::put(payload, "mrLiveRow", second);
    live::put(payload, "kind", "item");
    live::put(payload, "itemId", second);
    juce::DragAndDropTarget::SourceDetails drag(payload, list, {12, 42});
    liveCheck(list->isInterestedInDragSource(drag), "Native setlist accepts Browser items");
    list->itemDropped(drag);
    liveCheck(live::activeSetlist(desktop.liveDocument->interfaceState)["entries"].size() == count + 1 &&
                  desktop.liveDocument->interfaceState["items"].size() == 3,
              "Browser drop inserts a reference, not a copied item");
    desktop.action(10);
    liveCheck(live::activeSetlist(desktop.liveDocument->interfaceState)["entries"].size() == count,
              "Drop is one Undo transaction");
    desktop.app.new_project();
    desktop.app.add_audio_track("Real shared channel");
    const auto song = folder.getChildFile("song.mrsproject");
    desktop.app.save_project(std::filesystem::path(song.getFullPathName().toWideCharPointer()));
    desktop.bindLiveItem(id, song);
    liveCheck(desktop.liveDocument->currentLinked() && desktop.project()->tracks.size() == 1,
              "Explicit item binding loads real Studio project");
    desktop.saveFile(saved);
    const auto reopened = LiveDocument::read(saved);
    liveCheck(reopened.projectFor(id) == 0 && reopened.interfaceState["items"].size() == 3 &&
                  live::tasks(reopened.interfaceState, id).size() == 7,
              "Native v2 round-trips references and organizer");
    desktop.selectLiveItem(second);
    liveCheck(desktop.liveDocument->active == -1 && desktop.project()->tracks.empty(),
              "Unlinked selection clears previous project audio state");
    desktop.saveFile(saved);
    desktop.openFile(saved);
    liveCheck(!desktop.liveDocument->currentLinked() && desktop.project()->tracks.empty(),
              "Show reopens an unlinked current item without playing another song");
    desktop.selectLiveItem(id);
    livePump();
    native->showBrowser(true);
    livePump();
    for (const auto size : {juce::Point<int>{1200, 700}, juce::Point<int>{1920, 1080}}) {
        desktop.setSize(size.x, size.y);
        livePump();
        livePreview(desktop, "workspace-" + juce::String(size.x));
    }
    LiveItemEditor card(desktop, id);
    livePreview(card, "organizer");
    LiveCardEditor cardLayout(desktop, layout.defaults["itemCardLayout"]);
    livePreview(cardLayout, "card-layout");
    const auto wrongName = folder.getChildFile("chosen-name.json");
    desktop.saveFile(wrongName);
    liveCheck(wrongName.withFileExtension("mrlive").existsAsFile() && !wrongName.existsAsFile(),
              "Native show save always uses .mrlive");
    LiveDocument legacy;
    legacy.projects = {song};
    legacy.active = 0;
    const auto old = folder.getChildFile("legacy.mrlive");
    legacy.write(old);
    liveCheck(LiveDocument::read(old).interfaceState["items"].size() == 1,
              "Native v1 files migrate to catalog metadata");
    liveCheck(fixture.loadFileAsString() == original, "Editor source export remains unchanged");
    desktop.home.reset();
    liveCheck(folder.deleteRecursively(), "Native interface fixture cleanup");
}
} // namespace ui
