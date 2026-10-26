#include "LiveWindowLayout.h"
#include "LiveWorkspace.h"
#include "ProjectHome.h"
#include <fstream>
#include <windows.h>

namespace ui {
namespace {
// A manually driven test device: no second engine and no concurrent audio clock.
class LivePlaybackTestDevice final : public mrs::audio::IAudioDevice {
  public:
    std::vector<mrs::audio::DeviceInfo> enumerate() override {
        return {{0, "Live PCM test", {}, {"L", "R"}, 32, 2048, 128, -1}};
    }
    void control_panel(int) override {}
    void open(const mrs::audio::DeviceConfig &, std::shared_ptr<mrs::audio::AudioEngine>) override {
        phase = mrs::audio::DevicePhase::open;
    }
    void start() override { phase = mrs::audio::DevicePhase::running; }
    void stop() override { phase = mrs::audio::DevicePhase::stopped; }
    void close() noexcept override { phase = mrs::audio::DevicePhase::closed; }
    mrs::audio::DeviceStatus status() override { return {phase, 48000, 0, 0, 0, {}}; }

  private:
    mrs::audio::DevicePhase phase{mrs::audio::DevicePhase::closed};
};
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
    for (const float dpi : {1.f, 1.25f, 1.5f, 2.f}) {
        const juce::Rectangle<int> work(0, 0, juce::roundToInt(1920.f / dpi), juce::roundToInt(1040.f / dpi));
        const juce::BorderSize<int> frame(32, 8, 8, 8);
        const auto fitted = live::fitWindow({-200, -100, 1920, 1080}, work, frame, true);
        liveCheck(work.contains(frame.addedTo(fitted)), "Full Live window including title bar fits work area at DPI");
        const auto restored = live::fitWindow({4000, -800, 1920, 1080}, work, frame, false);
        liveCheck(work.contains(frame.addedTo(restored)), "Off-screen saved window is recovered after display changes");
    }
    const juce::Rectangle<int> secondMonitor(-1280, 0, 1280, 984);
    liveCheck(secondMonitor.contains(
                  juce::BorderSize<int>(32, 8, 8, 8)
                      .addedTo(live::fitWindow({100, -100, 1920, 1080}, secondMonitor, {32, 8, 8, 8}, false))),
              "Negative-coordinate secondary monitor is supported");
    const juce::Rectangle<int> contextArea(0, 0, 1200, 700);
    liveCheck(liveContextBounds({100, 120}, {600, 520}, contextArea).getPosition() == juce::Point<int>(102, 122),
              "Context opens to bottom right of pointer");
    for (const auto pointer : {juce::Point<int>{0, 0}, {1199, 699}, {1199, 0}, {0, 699}})
        liveCheck(contextArea.contains(liveContextBounds(pointer, {600, 520}, contextArea)),
                  "Context remains inside workspace at every corner");
    ViewSettings appearance;
    liveCheck(appearance.liveMenuOpacity == .9, "Live context defaults to ten percent transparency");
    auto oldAppearance = juce::JSON::parse(appearance.encode());
    oldAppearance.getDynamicObject()->removeProperty("liveContextStyleVersion");
    oldAppearance.getDynamicObject()->setProperty("liveMenuOpacity", .85);
    liveCheck(ViewSettings::decode(juce::JSON::toString(oldAppearance)).liveMenuOpacity == .9,
              "Previous Live defaults migrate to accepted context opacity");
    appearance.liveUiZoom = 1.25;
    appearance.liveMenuOpacity = .7;
    appearance.liveWindowOpacity = .8;
    const auto savedAppearance = ViewSettings::decode(appearance.encode());
    liveCheck(savedAppearance.liveUiZoom == 1.25 && savedAppearance.liveMenuOpacity == .7 &&
                  savedAppearance.liveWindowOpacity == .8,
              "Appearance preferences round-trip outside show metadata");
    Theme isolatedTheme;
    isolatedTheme.setPopupOpacity(.7f);
    Theme studioTheme;
    liveCheck(isolatedTheme.popupOpacity() == .7f && studioTheme.popupOpacity() == popupFillAlpha,
              "Live popup opacity does not change Studio defaults");
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
    auto *saveWorkspace = dynamic_cast<LiveWorkspace *>(liveComponent(desktop, "mr-live-workspace"));
    juce::TextButton *filesControl = nullptr;
    if (saveWorkspace)
        for (auto *child : saveWorkspace->getChildren())
            if (auto *button = dynamic_cast<juce::TextButton *>(child); button && button->getButtonText() == "Files")
                filesControl = button;
    liveCheck(saveWorkspace && filesControl, "Native Files toolbar exists");
    const auto clickTime = juce::Time::getCurrentTime();
    const juce::Point<float> clickPosition{5, 5};
    saveWorkspace->mouseDown(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), clickPosition,
                                              juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
                                              filesControl, filesControl, clickTime, clickPosition, clickTime, 1,
                                              false));
    desktop.saveFile(saved);
    liveCheck(desktop.liveShowEditing, "Files/Save preserves the show Undo context");
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
    auto *timeline = dynamic_cast<LiveTimeline *>(liveComponent(desktop, "live-show-timeline"));
    liveCheck(timeline != nullptr, "Native show timeline exists");
    timeline->refresh();
    const auto beforeTimeline = live::canonical(desktop.liveDocument->interfaceState);
    liveCheck(timeline->itemCount() == 3 && timeline->channelCount() == 4,
              "Timeline uses active setlist and shared show channel definitions");
    const auto originalWidth = timeline->itemBounds(0).getWidth();
    liveCheck(timeline->itemBounds(1).getX() == timeline->itemBounds(0).getRight(),
              "Items form one horizontal sequence with aligned lanes");
    timeline->refresh();
    liveCheck(live::canonical(desktop.liveDocument->interfaceState) == beforeTimeline,
              "Native timeline projection does not alter imported show data");
    desktop.editLiveInterface("Timeline scale", [&](auto &state) {
        auto view = live::object();
        live::put(view, "scale", 8);
        live::put(view, "lead", 2);
        live::put(live::map(live::performance(state), "arrangement"), id, view);
    });
    timeline->refresh();
    liveCheck(timeline->itemBounds(0).getWidth() == 706, "Native timeline uses saved bar scale and lead-in");
    desktop.action(10);
    timeline->refresh();
    liveCheck(timeline->itemBounds(0).getWidth() == originalWidth, "Timeline follows metadata Undo");
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
    timeline->refresh();
    liveCheck(timeline->itemCount() == 4, "Repeated setlist reference appears as a separate timeline item");
    desktop.action(10);
    timeline->refresh();
    liveCheck(timeline->itemCount() == 3 &&
                  live::activeSetlist(desktop.liveDocument->interfaceState)["entries"].size() == count,
              "Drop Undo restores both setlist and timeline");
    desktop.editLiveInterface("Empty setlist",
                              [](auto &state) { live::activeEntries(state) = juce::Array<juce::var>(); });
    timeline->refresh();
    liveCheck(timeline->itemCount() == 1 && timeline->channelCount() == 4 &&
                  live::activeEntries(desktop.liveDocument->interfaceState).size() == 0,
              "Empty setlist previews selected item without inserting it and retains shared channels");
    desktop.action(10);
    timeline->refresh();
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
    native->showBrowser(false);
    livePump();
    livePreview(desktop, "timeline-1920");
    LiveItemEditor card(desktop, id);
    card.setVisible(true);
    livePump();
    int taskFields = 0;
    std::function<void(juce::Component &)> countTasks = [&](juce::Component &rootComponent) {
        if (auto *field = dynamic_cast<juce::TextEditor *>(&rootComponent); field && field->getTitle() == "Task title")
            ++taskFields;
        for (auto *child : rootComponent.getChildren())
            countTasks(*child);
    };
    countTasks(card);
    liveCheck(taskFields == 7, "Native organizer creates all visible task editors");
    livePreview(card, "organizer");
    const auto beforeContext = juce::JSON::toString(desktop.liveDocument->interfaceState);
    auto contextContent = std::make_unique<LiveItemEditor>(desktop, id);
    auto &contextBox = showLiveContext(std::move(contextContent), desktop, desktop.getScreenBounds().withSize(1, 1));
    livePump();
    liveCheck(contextBox.getParentComponent() == &desktop &&
                  dynamic_cast<Theme *>(&contextBox.getLookAndFeel())->liveContextStyle(),
              "Item editor uses an owned contextual surface");
    liveCheck(desktop.getLocalBounds().contains(contextBox.getBounds()), "Item contextual panel fits the workspace");
    livePreview(desktop, "item-context");
    contextBox.dismiss();
    livePump();
    liveCheck(juce::JSON::toString(desktop.liveDocument->interfaceState) == beforeContext,
              "Opening and dismissing item context preserves show data");
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
    {
        // Synthetic PCM only: exercise shared rendering without ASIO or personal media.
        const auto waveFile = folder.getChildFile("live-tone.wav");
        {
            std::ofstream out(std::filesystem::path(waveFile.getFullPathName().toWideCharPointer()), std::ios::binary);
            auto u16 = [&](unsigned value) {
                out.put(static_cast<char>(value & 255));
                out.put(static_cast<char>((value >> 8) & 255));
            };
            auto u32 = [&](unsigned value) {
                u16(value & 65535);
                u16(value >> 16);
            };
            out.write("RIFF", 4);
            u32(36 + 192000);
            out.write("WAVEfmt ", 8);
            u32(16);
            u16(1);
            u16(2);
            u32(48000);
            u32(192000);
            u16(4);
            u16(16);
            out.write("data", 4);
            u32(192000);
            for (int n = 0; n < 96000; ++n)
                u16(8192);
        }
        Desktop playbackDesktop(true, false, true);
        playbackDesktop.importLiveShow(fixture);
        playbackDesktop.app.disconnect();
        playbackDesktop.selectLiveItem(id);
        playbackDesktop.attachLivePlayback(id, waveFile);
        liveCheck(live::itemDirty(playbackDesktop.liveDocument->interfaceState, id),
                  "Attaching audio marks item content unsaved against Browser");
        playbackDesktop.editLiveInterface("Save for All", [&](auto &state) { live::saveItem(state, id); });
        liveCheck(!live::itemDirty(playbackDesktop.liveDocument->interfaceState, id),
                  "Save for All captures audio reference and clears saved dot");
        playbackDesktop.undoLiveInterface();
        liveCheck(live::itemDirty(playbackDesktop.liveDocument->interfaceState, id),
                  "Undo Save for All restores unsaved audio dot");
        playbackDesktop.undoLiveInterface(true);
        liveCheck(!live::itemDirty(playbackDesktop.liveDocument->interfaceState, id),
                  "Redo Save for All restores saved audio version");

        const auto waitPlayback = [&] {
            const auto deadline = juce::Time::getMillisecondCounter() + 5000;
            while (playbackDesktop.livePlaybackLoading && juce::Time::getMillisecondCounter() < deadline) {
                playbackDesktop.app.poll();
                playbackDesktop.syncLivePlayback();
                juce::Thread::sleep(1);
            }
        };
        waitPlayback();
        liveCheck(playbackDesktop.livePlaybackReady() && playbackDesktop.project()->clips.size() == 1 &&
                      playbackDesktop.liveDocument->projects.empty(),
                  "WAV materializes into shared Core without Studio project references");

        liveCheck(playbackDesktop.project()->time.tempos.front().bpm == 92 &&
                      playbackDesktop.project()->sections.size() == 4,
                  "Native tempo and sections materialize in shared Project");
        playbackDesktop.app.connect(std::make_unique<LivePlaybackTestDevice>(), {0, 48000, 128, {}, {0, 1}, 1});
        auto *playbackTimeline = dynamic_cast<LiveTimeline *>(liveComponent(playbackDesktop, "live-show-timeline"));
        liveCheck(playbackTimeline != nullptr, "Playback has native timeline");
        playbackTimeline->refresh();
        auto *transportLine = dynamic_cast<PlayheadLine *>(liveComponent(*playbackTimeline, "live-transport-playhead"));
        liveCheck(transportLine != nullptr, "Timeline owns a real transport playhead");
        transportLine->update();
        const int startX = transportLine->getX();
        const float startPosition = playbackTimeline->playheadBounds().getX();
        std::array<float, 256> audio{};
        const auto render = [&] {
            for (int n = 0; n < 4; ++n)
                playbackDesktop.app.engine()->process(nullptr, audio.data(), 128);
        };
        for (auto *child : playbackTimeline->getChildren())
            if (auto *scroll = dynamic_cast<juce::Viewport *>(child))
                scroll->setViewPosition(1000, 0);
        playbackDesktop.action(30);
        render();
        const float fullLevel = std::abs(audio[200]);
        playbackTimeline->refresh();
        for (auto *child : playbackTimeline->getChildren())
            if (auto *scroll = dynamic_cast<juce::Viewport *>(child))
                liveCheck(scroll->getViewPositionX() <= playbackTimeline->playheadBounds().getX(),
                          "Starting Playback reveals an off-screen playhead");
        transportLine->update();
        liveCheck(transportLine->getHeight() > 0 && playbackTimeline->playheadBounds().getX() > startPosition,
                  "Timeline playhead advances with rendered Core audio");
        liveCheck(fullLevel > .01f && playbackDesktop.app.engine()->state().sample > 0,
                  "Live transport produces actual PCM through shared AudioEngine");
        playbackDesktop.setLivePlaybackMix(-6., false, true);
        render();
        liveCheck(std::abs(audio[200]) < fullLevel * .75f && playbackDesktop.project()->tracks.front().mix.solo,
                  "Item gain and Solo reach the Core mixer");
        playbackDesktop.setLivePlaybackMix(-6., true, true);
        render();
        liveCheck(std::abs(audio[200]) < .0001f, "Mute silences rendered audio");
        playbackDesktop.undoLiveInterface();
        playbackDesktop.syncLivePlayback();
        render();
        liveCheck(std::abs(audio[200]) > .01f, "Show Undo restores audible mixer state");
        playbackDesktop.action(31);
        render();
        liveCheck(playbackDesktop.app.engine()->state().playback == mrs::PlaybackState::paused,
                  "Live Pause reaches shared Transport");
        transportLine->update();
        const int pausedX = transportLine->getX();
        render();
        transportLine->update();
        liveCheck(transportLine->getX() == pausedX, "Paused playhead stays at actual transport position");
        playbackDesktop.action(32);
        render();
        liveCheck(playbackDesktop.app.engine()->state().sample == 0, "Live Stop resets actual transport position");
        transportLine->update();
        liveCheck(transportLine->getX() == startX &&
                      std::abs(playbackTimeline->playheadBounds().getX() - startPosition) < .001f,
                  "Stop returns timeline playhead to item start");

        playbackDesktop.editLiveInterface("Zoom for clip editing", [&](auto &state) {
            auto &viewState = live::map(live::map(live::performance(state), "arrangement"), id);
            live::put(viewState, "scale", 128.);
        });
        playbackTimeline->refresh();
        auto *trimTimelineBody = liveComponent(*playbackTimeline, "live-timeline-items");
        const auto dragTime = juce::Time::getCurrentTime();
        const auto dragEvent = [&](float x) {
            const juce::Point<float> point{x, 426.f};
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                                    juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
                                    trimTimelineBody, trimTimelineBody, dragTime, point, dragTime, 1, false);
        };
        const auto untrimmed = playbackDesktop.project();
        trimTimelineBody->mouseDown(dragEvent(1.f));
        trimTimelineBody->mouseDrag(dragEvent(4.f));
        liveCheck(playbackDesktop.project() == untrimmed, "Trim drag previews without publishing per-frame Core edits");
        trimTimelineBody->mouseUp(dragEvent(4.f));
        liveCheck(playbackDesktop.project()->clips.front().start > 0 &&
                      playbackDesktop.project()->clips.front().start ==
                          playbackDesktop.project()->clips.front().source_offset,
                  "Releasing the left clip edge commits nondestructive trim");
        playbackDesktop.undoLiveInterface();
        playbackDesktop.syncLivePlayback();
        liveCheck(playbackDesktop.project()->clips.front().length == 48000,
                  "One show Undo reverses the whole edge drag");

        // Trim uses the existing Core source window; no destructive WAV rewrite or second audio path.
        auto *trimIn = dynamic_cast<juce::TextEditor *>(liveComponent(playbackDesktop, "live-trim-start"));
        auto *trimOut = dynamic_cast<juce::TextEditor *>(liveComponent(playbackDesktop, "live-trim-end"));
        auto *trimButton = dynamic_cast<juce::TextButton *>(liveComponent(playbackDesktop, "live-trim-apply"));
        liveCheck(trimIn && trimOut && trimButton, "Playback exposes precise source trim controls");
        trimIn->setText("0.02", false);
        trimOut->setText("0.08", false);
        trimButton->onClick();
        liveCheck(playbackDesktop.project()->clips.front().start == 960 &&
                      playbackDesktop.project()->clips.front().source_offset == 960 &&
                      playbackDesktop.project()->clips.front().length == 2880,
                  "Trim maps seconds into the real Core source window");
        const auto trimmedProject = playbackDesktop.project();
        bool trimRejected = false;
        try {
            playbackDesktop.setLivePlaybackTrim(.02, 2.);
        } catch (const std::exception &) {
            trimRejected = true;
        }
        liveCheck(trimRejected && playbackDesktop.project() == trimmedProject,
                  "Out-of-source trim is rejected without committing show or Core changes");
        playbackDesktop.undoLiveInterface();
        playbackDesktop.syncLivePlayback();
        liveCheck(playbackDesktop.project()->clips.front().start == 0 &&
                      playbackDesktop.project()->clips.front().source_offset == 0 &&
                      playbackDesktop.project()->clips.front().length == 48000,
                  "Show Undo restores the full original source extent");
        playbackDesktop.undoLiveInterface(true);
        playbackDesktop.syncLivePlayback();
        liveCheck(playbackDesktop.project()->clips.front().length == 2880, "Show Redo restores nondestructive trim");
        playbackDesktop.app.connect(std::make_unique<LivePlaybackTestDevice>(), {0, 48000, 128, {}, {0, 1}, 1});
        playbackDesktop.app.seek(0);
        playbackDesktop.app.play();
        render();
        liveCheck(std::abs(audio[200]) < .0001f, "Trimmed source prefix is silent on the actual timeline");
        const auto beforePlayingTrim = playbackDesktop.liveDocument->signature();
        trimRejected = false;
        try {
            playbackDesktop.setLivePlaybackTrim(0., 1.);
        } catch (const std::exception &) {
            trimRejected = true;
        }
        liveCheck(trimRejected && playbackDesktop.liveDocument->signature() == beforePlayingTrim,
                  "Playing trim guard preserves show history atomically");
        playbackDesktop.app.seek(960);
        render();
        liveCheck(std::abs(audio[200]) > .01f, "Trim retains audible original PCM inside the source window");
        playbackDesktop.app.stop();
        render();
        playbackTimeline->refresh();
        auto *channelMute = dynamic_cast<juce::TextButton *>(liveComponent(*playbackTimeline, "live-channel-mute"));
        auto *channelSolo = dynamic_cast<juce::TextButton *>(liveComponent(*playbackTimeline, "live-channel-solo"));
        auto *channelGain = dynamic_cast<juce::Slider *>(liveComponent(*playbackTimeline, "live-channel-gain"));
        liveCheck(channelMute && channelSolo && channelGain && channelMute->isEnabled(),
                  "Playback channel has bound mix controls");
        channelMute->setToggleState(true, juce::dontSendNotification);
        channelMute->onClick();
        liveCheck(playbackDesktop.project()->tracks.front().mix.mute,
                  "Channel Mute edits the same shared mixer as the Playback panel");
        playbackDesktop.undoLiveInterface();
        playbackDesktop.syncLivePlayback();
        playbackTimeline->refresh();
        liveCheck(!channelMute->getToggleState(), "Channel controls reflect show Undo");

        liveCheck(live::itemDirty(playbackDesktop.liveDocument->interfaceState, id),
                  "Changing saved item mix makes content unsaved");
        playbackDesktop.editLiveInterface("Save for All", [&](auto &state) { live::saveItem(state, id); });
        const auto playbackShow = folder.getChildFile("playback.mrlive");
        playbackDesktop.saveFile(playbackShow);
        const auto playbackSaved = LiveDocument::read(playbackShow);
        liveCheck(playbackSaved.projects.empty() &&
                      live::playback(playbackSaved.interfaceState, id)["file"].toString() ==
                          waveFile.getFullPathName() &&
                      static_cast<double>(live::playback(playbackSaved.interfaceState, id)["gain"]) == -6. &&
                      static_cast<double>(live::playback(playbackSaved.interfaceState, id)["trimStart"]) == .02 &&
                      static_cast<double>(live::playback(playbackSaved.interfaceState, id)["trimEnd"]) == .08 &&
                      !playbackDesktop.documentDirty(),
                  "Playback media and mix round-trip solely through mrlive");
        const auto savedJson = juce::JSON::parse(playbackShow.loadFileAsString());
        liveCheck(live::playback(savedJson["interface"], id)["file"].toString() == "live-tone.wav",
                  "Playback reference is relative to the show folder");

        liveCheck(!live::itemDirty(playbackSaved.interfaceState, id),
                  "Saved audio item remains green after media path round-trip");
        const auto movedShow = folder.getChildFile("relocated").getChildFile("copy.mrlive");
        playbackSaved.write(movedShow);
        const auto moved = LiveDocument::read(movedShow);
        liveCheck(!live::itemDirty(moved.interfaceState, id) &&
                      live::playback(moved.interfaceState, id)["file"].toString() == waveFile.getFullPathName(),
                  "Save as rebases both working audio and saved Browser version");
        playbackDesktop.openFile(playbackShow);
        playbackDesktop.syncLivePlayback();
        waitPlayback();
        liveCheck(playbackDesktop.livePlaybackReady() &&
                      playbackDesktop.project()->clips.front().source_offset == 960 &&
                      playbackDesktop.project()->clips.front().length == 2880,
                  "Reopened show materializes saved trim in shared Core");
        liveCheck(playbackDesktop.livePlaybackReady(), "Reopened show prepares its audio again");

        const auto projected = playbackDesktop.project();
        playbackDesktop.action(10);
        liveCheck(playbackDesktop.project() == projected && playbackDesktop.livePlaybackReady(),
                  "Show Undo never undoes generated Core projection after reopen");
        playbackDesktop.setLivePlaybackTrim(0., 1.);
        playbackDesktop.app.connect(std::make_unique<LivePlaybackTestDevice>(), {0, 48000, 128, {}, {0, 1}, 1});
        playbackDesktop.editLiveInterface("Musical context", [&](auto &state) {
            auto &item = live::require(state, "items", id);
            live::put(item, "tempo", 90.);
            live::put(item, "meter", "3/4");
            live::put(*live::property(item, "sections").getArray()->getRawDataPointer(), "length", 22.);
            auto chord = live::object();
            live::put(chord, "id", "native-chord-test");
            live::put(chord, "symbol", "C");
            live::put(chord, "start", 0.);
            live::put(chord, "length", 2.);
            juce::Array<juce::var> chords;
            chords.add(chord);
            live::put(live::map(live::performance(state), "chords"), id, chords);
        });
        playbackDesktop.syncLivePlayback();
        liveCheck(playbackDesktop.livePlaybackReady() && playbackDesktop.project()->time.tempos.front().bpm == 90 &&
                      playbackDesktop.project()->time.meters.front().numerator == 3 &&
                      playbackDesktop.project()->chords.size() == 1,
                  "Tempo meter and Chord Track use shared Project without reimporting audio");
        liveCheck(playbackDesktop.project()->sections[0].end == playbackDesktop.project()->sections[1].start &&
                      static_cast<double>(live::find(playbackDesktop.liveDocument->interfaceState, "items",
                                                     id)["sections"][0]["length"]) == 22.,
                  "Core resolves section overlap at next start without rewriting authored show spans");
        playbackDesktop.app.poll();
        liveCheck(playbackDesktop.app.musical().state().current_chord.has_value(),
                  "Shared musical context exposes authored chord");
        playbackDesktop.action(35);
        render();
        playbackDesktop.app.poll();
        const auto verse = playbackDesktop.project()->sections[1];
        const mrs::Timeline musicClock(playbackDesktop.project()->time, 48000);
        liveCheck(playbackDesktop.app.engine()->state().sample == musicClock.to_samples(verse.start) &&
                      playbackDesktop.app.musical().state().current_section->id == verse.id,
                  "Next Section seeks shared transport under item meter and tempo");
        playbackDesktop.action(36);
        render();
        liveCheck(
            playbackDesktop.app.engine()->state().loop ==
                std::optional<mrs::LoopRange>{{musicClock.to_samples(verse.start), musicClock.to_samples(verse.end)}},
            "Section Loop reaches real engine sample range");
        playbackDesktop.action(36);
        render();
        playbackDesktop.app.poll();
        playbackDesktop.action(34);
        render();
        playbackDesktop.app.poll();
        liveCheck(playbackDesktop.app.engine()->state().sample == 0 && !playbackDesktop.app.engine()->state().loop,
                  "Previous Section and Loop Off affect real transport");
        playbackTimeline->refresh();
        auto *timelineBody = liveComponent(*playbackTimeline, "live-timeline-items");
        liveCheck(timelineBody != nullptr, "Native timeline body exists");
        const auto view =
            playbackDesktop.liveDocument->interfaceState["performance"]["arrangement"][juce::Identifier(id)];
        const double viewScale = view.hasProperty("scale") ? static_cast<double>(view["scale"]) : 16.;
        const double viewLead = view.hasProperty("lead") ? static_cast<double>(view["lead"]) : 0.;
        const auto seekTime = juce::Time::getCurrentTime();
        const auto seekClick = [&](juce::Point<float> point) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                                    juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
                                    timelineBody, timelineBody, seekTime, point, seekTime, 1, false);
        };
        const float rulerX = static_cast<float>(1. + (viewLead + 1.) * viewScale);
        timelineBody->mouseDown(seekClick({rulerX, 72.f}));
        render();
        playbackDesktop.app.poll();
        liveCheck(playbackDesktop.app.engine()->state().sample == musicClock.to_samples(3 * mrs::ppq),
                  "Timeline ruler seeks real audio under 3/4 meter");
        const float audioX = static_cast<float>(1. + (viewLead + .5 / 2.) * viewScale);
        int playbackLane = 0;
        for (const auto &channel : *playbackDesktop.liveDocument->interfaceState["channels"].getArray()) {
            if (channel["type"].toString() == "Playback")
                break;
            ++playbackLane;
        }
        timelineBody->mouseDown(seekClick({audioX, static_cast<float>(158 + playbackLane * 124 + 32)}));
        render();
        liveCheck(std::abs(playbackDesktop.app.engine()->state().sample - 24000) < 50,
                  "Clicking the real Playback lane seeks shared audio to that time");
        const float verseX = static_cast<float>(1. + (viewLead + 21.75) * viewScale);
        timelineBody->mouseDown(seekClick({verseX, 100.f}));
        render();
        playbackDesktop.app.poll();
        liveCheck(playbackDesktop.app.engine()->state().sample == musicClock.to_samples(verse.start),
                  "Section click within authored overlap selects the same section as Core");
        playbackDesktop.action(32);
        render();
        playbackDesktop.app.poll();
        playbackDesktop.action(30);
        render();
        bool musicBlocked = false;
        try {
            playbackDesktop.editLiveInterface(
                "Unsafe tempo", [&](auto &state) { live::put(live::require(state, "items", id), "tempo", 100.); });
        } catch (const std::exception &) {
            musicBlocked = true;
        }
        liveCheck(musicBlocked && playbackDesktop.project()->time.tempos.front().bpm == 90 &&
                      static_cast<double>(
                          live::find(playbackDesktop.liveDocument->interfaceState, "items", id)["tempo"]) == 90,
                  "Playing tempo changes are rejected without mutating show or Core");
        playbackDesktop.action(32);
        render();
        playbackDesktop.app.poll();
        playbackDesktop.selectLiveItem(second);
        liveCheck(!playbackDesktop.livePlaybackReady() && playbackDesktop.project()->clips.empty(),
                  "Changing to an empty item clears previous audio");
        playbackTimeline->refresh();
        liveCheck(playbackTimeline->playheadBounds().isEmpty(), "Unbound item does not show a fake transport playhead");
        for (auto *child : playbackTimeline->getChildren())
            if (auto *scroll = dynamic_cast<juce::Viewport *>(child))
                liveCheck(scroll->getViewPositionX() > 0,
                          "Selecting next item reveals its start instead of retaining previous scroll");

        auto *browserWorkspace = dynamic_cast<LiveWorkspace *>(liveComponent(playbackDesktop, "mr-live-workspace"));
        liveCheck(browserWorkspace != nullptr, "Browser workspace exists");
        browserWorkspace->showBrowser(true);
        livePump();
        auto *browserCard = liveComponent(playbackDesktop, "live-browser-" + id);
        liveCheck(browserCard != nullptr, "Browser item exists");
        const auto t = juce::Time::getCurrentTime();
        const juce::Point<float> p{8, 8};
        const auto click = [&](int mods) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(mods), 1,
                                    0, 0, 0, 0, browserCard, browserCard, t, p, t, 1, false);
        };
        browserCard->mouseDown(click(juce::ModifierKeys::leftButtonModifier));
        browserCard->mouseUp(click(0));
        liveCheck(playbackDesktop.liveDocument->interfaceState["currentItem"].toString() == id,
                  "Released-button Browser click activates item");
        waitPlayback();
        playbackTimeline->refresh();
        livePump();
        browserCard = liveComponent(playbackDesktop, "live-browser-" + id);
        liveCheck(browserCard != nullptr, "Active Browser row survives refresh");
        const auto focusProject = playbackDesktop.project();
        for (auto *child : playbackTimeline->getChildren())
            if (auto *scroll = dynamic_cast<juce::Viewport *>(child))
                scroll->setViewPosition(1000, 0);
        browserCard->mouseDown(click(juce::ModifierKeys::leftButtonModifier));
        browserCard->mouseUp(click(0));
        playbackTimeline->refresh();
        liveCheck(playbackDesktop.project() == focusProject,
                  "Reclicking active Browser item does not restart Core audio");
        for (auto *child : playbackTimeline->getChildren())
            if (auto *scroll = dynamic_cast<juce::Viewport *>(child))
                liveCheck(scroll->getViewPositionX() == 0,
                          "Reclicking active Browser item focuses timeline even without selection change");
        playbackDesktop.editLiveInterface("Library-only view",
                                          [](auto &state) { live::activeEntries(state) = juce::Array<juce::var>(); });
        playbackTimeline->refresh();
        liveCheck(playbackTimeline->itemCount() == 1 &&
                      live::activeEntries(playbackDesktop.liveDocument->interfaceState).size() == 0,
                  "Browser item outside active setlist appears as preview without modifying setlist");
        playbackDesktop.selectLiveItem(second);
        const auto badWave = folder.getChildFile("invalid.wav");
        badWave.replaceWithText("not a WAV");
        playbackDesktop.attachLivePlayback(second, badWave);
        waitPlayback();
        liveCheck(!playbackDesktop.livePlaybackReady() && playbackDesktop.livePlaybackError.isNotEmpty() &&
                      playbackDesktop.project()->clips.empty(),
                  "Decode failure disables Playback and cannot retain old song audio");
        playbackDesktop.attachLivePlayback(second, waveFile);
        playbackDesktop.selectLiveItem(id);
        waitPlayback();
        liveCheck(playbackDesktop.livePlaybackReady() && playbackDesktop.livePlaybackItem == id,
                  "Rapid item switch cannot publish a stale import");
    }
    desktop.home.reset();
    liveCheck(folder.deleteRecursively(), "Native interface fixture cleanup");
}
} // namespace ui
