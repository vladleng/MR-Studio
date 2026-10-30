#include "LiveWindowLayout.h"
#include "LiveWorkspace.h"
#include "../../../tests/live_playback_view_checks.hpp"
#include "../../../tests/live_timeline_edit_checks.hpp"
#include "../../../tests/live_clip_edit_checks.hpp"
#include "../../../tests/live_performance_checks.hpp"
#include "../../../tests/live_show_value_checks.hpp"
#include "ProjectHome.h"
#include <fstream>
#include <windows.h>

namespace ui {
namespace {
// A manually driven test device: no second engine and no concurrent audio clock.
class LivePlaybackTestDevice final : public mrs::audio::IAudioDevice {
  public:
    explicit LivePlaybackTestDevice(int count = 2) : outputs(count) {}
    std::vector<mrs::audio::DeviceInfo> enumerate() override {
        std::vector<std::string> names;
        for (int i = 0; i < outputs; ++i)
            names.push_back("Output " + std::to_string(i + 1));
        return {{0, "Live PCM test", {"Input 1","Input 2"}, names, 32, 2048, 128, -1}};
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
    int outputs{2};
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
// Regression for build 390's AudioEngine::state() exception escaping UI refresh.
// No vendor plugins/hardware: a single synthetic callback writer competes with
// the actual Playback, Timeline and Mixer display readers while stopped.
void liveDisplayPollingSmoke(const juce::File& fixture,const juce::File& wave){
    Desktop polling(true,false,true);polling.importLiveShow(fixture);polling.setSize(1200,800);
    polling.app.connect(std::make_unique<LivePlaybackTestDevice>(),{0,48000,128,{}, {0,1},1});
    const auto id=polling.liveDocument->interfaceState["currentItem"].toString();
    polling.attachLivePlayback(id,wave);
    const auto deadline=juce::Time::getMillisecondCounter()+5000;
    while(polling.livePlaybackLoading&&juce::Time::getMillisecondCounter()<deadline){
        polling.app.poll();polling.syncLivePlayback();juce::Thread::sleep(1);
    }
    liveCheck(polling.livePlaybackReady(),"Display polling fixture has a real prepared Playback row");
    auto* playbackPanelView=dynamic_cast<LivePlaybackPanel*>(liveComponent(polling,"live-playback-panel"));
    auto* timeline=dynamic_cast<LiveTimeline*>(liveComponent(polling,"live-show-timeline"));
    auto* mixer=dynamic_cast<LiveMixer*>(liveComponent(polling,"live-mixer"));
    liveCheck(playbackPanelView&&timeline&&mixer,"Concurrent display polling uses real Live components");
    playbackPanelView->sync();timeline->refresh();mixer->sync();
    const auto engine=polling.app.engine();
    std::atomic<bool> begin{};
    std::jthread callback([&]{
        std::array<float,256> output{};
        while(!begin.load())std::this_thread::yield();
        for(int block=0;block<50000;++block)engine->process(nullptr,output.data(),128);
    });
    begin=true;
    for(int refresh=0;refresh<300;++refresh){playbackPanelView->sync();timeline->refresh();mixer->sync();}
    callback.join();
    mrs::audio::RealtimeState head;
    liveCheck(engine->try_state(head)&&head.playback==mrs::PlaybackState::stopped,
              "Concurrent display refresh leaves stopped playback intact without an escaping busy-state exception");
}
void liveSelectionRefreshSmoke(const juce::File& fixture,const juce::File& wave){
    Desktop navigation(true,false,true);navigation.importLiveShow(fixture);navigation.setSize(1200,800);
    navigation.app.connect(std::make_unique<LivePlaybackTestDevice>(),{0,48000,128,{}, {0,1},1});
    const auto values=navigation.liveDocument->interfaceState["items"];
    const auto first=values[0]["id"].toString(),second=values[1]["id"].toString(),third=values[2]["id"].toString();
    navigation.selectLiveItem(third);navigation.attachLivePlayback(first,wave);navigation.attachLivePlayback(second,wave);
    // Valid show limits: a one-second WAV spans seven 1/4 bars at 400 BPM.
    navigation.editLiveInterface("Cold waveform geometry fixture",[&](auto& state){
        for(const auto& id:{first,second,third}){auto item=live::require(state,"items",id);live::put(item,"bars",1.);live::put(item,"tempo",400.);live::put(item,"meter","1/4");live::setItemView(state,id,"scale",128.);}
    });navigation.syncLivePlayback();
    {
        LiveTimeline cold(navigation);cold.setSize(640,700);cold.refresh();
        auto* viewport=dynamic_cast<juce::Viewport*>(cold.getChildComponent(0));
        if(!viewport)for(auto* child:cold.getChildren())if(auto* candidate=dynamic_cast<juce::Viewport*>(child))viewport=candidate;
        liveCheck(viewport!=nullptr,"Cold timeline has a viewport");
        const auto deadline=juce::Time::getMillisecondCounter()+5000;
        while(cold.itemBounds(third).getX()<1000&&juce::Time::getMillisecondCounter()<deadline){cold.refresh();juce::Thread::sleep(1);}
        liveCheck(cold.itemBounds(third).getX()>1000,"Delayed WAV metadata expands preceding items");
        liveCheck(viewport->getViewArea().intersects(cold.itemBounds(third)),
                  "Waveform reflow keeps the selected entry visible instead of revealing a middle item");
        auto* body=viewport->getViewedComponent();const auto target=cold.itemBounds(second);
        viewport->setViewPosition(juce::jmax(0,target.getX()-12),0);
        const auto time=juce::Time::getCurrentTime();const juce::Point<float> point{static_cast<float>(target.getX()+60),16.f};
        body->mouseDown(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,body,body,time,point,time,1,false));
        const auto clickDeadline=juce::Time::getMillisecondCounter()+5000;
        while(navigation.liveDocument->interfaceState["currentItem"].toString()!=second&&juce::Time::getMillisecondCounter()<clickDeadline)livePump();
        cold.refresh();
        liveCheck(navigation.liveDocument->interfaceState["currentItem"].toString()==second&&viewport->getViewArea().intersects(cold.itemBounds(second)),
                  "Timeline header click activates and reveals the clicked entry after asynchronous reflow");
    }
    auto* list=dynamic_cast<LiveSetlist*>(liveComponent(navigation,"live-setlist"));
    juce::String entry;for(const auto& row:*live::activeSetlist(navigation.liveDocument->interfaceState)["entries"].getArray())if(row["itemId"].toString()==first){entry=row["id"].toString();break;}
    liveCheck(list&&entry.isNotEmpty(),"Selection fixture has the first setlist entry");
    auto* card=liveComponent(*list,"live-entry-"+entry);liveCheck(card!=nullptr,"Setlist click target exists");
    const auto now=juce::Time::getCurrentTime();const juce::Point<float> pointer{8,8};
    const auto event=[&](int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),pointer,juce::ModifierKeys(mods),1,0,0,0,0,card,card,now,pointer,now,1,false);};
    liveCheck(!list->activationBlocked||!list->activationBlocked(),"Setlist click is not drag-blocked before any drag");
    liveCheck(!navigation.songDirty(),"Selection fixture does not require a Studio song-save dialog");
    card->mouseDown(event(juce::ModifierKeys::leftButtonModifier));card->mouseUp(event(0));
    liveCheck(list->hasPendingActivation(),"Setlist mouse release schedules its entry activation");
    const auto selectionRevision=navigation.liveSelectionRevision;
    ++navigation.liveUiRevision;list->refresh();
    liveCheck(list->hasPendingActivation(),"Row rebuild retains the scheduled activation"); // Destroy/recreate cards before delayed single-click fires.
    const auto deadline=juce::Time::getMillisecondCounter()+5000;
    while((navigation.liveDocument->interfaceState["currentItem"].toString()!=first||navigation.livePlaybackLoading)&&juce::Time::getMillisecondCounter()<deadline)livePump();
    if(navigation.liveDocument->interfaceState["currentItem"].toString()!=first||navigation.liveCurrentEntry!=entry)
        throw std::runtime_error(("Pending setlist click failed: current="+navigation.liveDocument->interfaceState["currentItem"].toString()+
            "; expected="+first+"; entry="+navigation.liveCurrentEntry+"; pending="+navigation.livePendingItem+
            "; scheduled="+juce::String(list->hasPendingActivation()?1:0)+
            "; selection revision="+juce::String(static_cast<juce::int64>(navigation.liveSelectionRevision))+"/"+juce::String(static_cast<juce::int64>(selectionRevision))+"; message="+navigation.message+"; preload="+navigation.livePreloadError).toStdString());
    card=liveComponent(*list,"live-entry-"+entry);liveCheck(card!=nullptr,"Activated setlist entry remains addressable");
    card->mouseDown(event(juce::ModifierKeys::leftButtonModifier));card->mouseUp(event(0));
    navigation.selectLiveItem(third);
    for(int i=0;i<5;++i)livePump();
    liveCheck(navigation.liveDocument->interfaceState["currentItem"].toString()==third&&!list->hasPendingActivation(),
              "New explicit selection supersedes the delayed setlist click");
    juce::String preview;
    navigation.editLiveInterface("Short Browser preview fixture",[&](auto& state){
        preview=live::addItem(state,"Short Browser preview");
        live::setItemView(state,preview,"scale",16.);
    });
    LiveTimeline focus(navigation);focus.setSize(1200,700);focus.refresh();
    auto* focusViewport=dynamic_cast<juce::Viewport*>(focus.getChildComponent(0));
    liveCheck(focusViewport!=nullptr,"Focus timeline has a viewport");
    const auto selectAndCheck=[&](const juce::String& item,const juce::String& entryId){
        navigation.selectLiveItem(item,entryId);
        const auto until=juce::Time::getMillisecondCounter()+5000;
        while((navigation.liveDocument->interfaceState["currentItem"].toString()!=item||navigation.livePlaybackLoading)&&juce::Time::getMillisecondCounter()<until)livePump();
        focus.refresh();
        const int headerX=focus.itemBounds(item).getX()-focusViewport->getViewPositionX();
        liveCheck(navigation.liveDocument->interfaceState["currentItem"].toString()==item&&headerX>=0&&headerX<=12,
                  "Explicit selection reveals the requested item header at the left, including a short final setlist item");
        for(int i=0;i<3;++i){livePump();focus.refresh();}
        const int settledX=focus.itemBounds(item).getX()-focusViewport->getViewPositionX();
        liveCheck(settledX>=0&&settledX<=12,"Selected header stays focused after asynchronous refresh");
    };
    liveCheck(live::entryIndex(navigation.liveDocument->interfaceState,preview)<0,"Preview item is outside the active setlist");
    auto* browser=dynamic_cast<LiveSetlist*>(liveComponent(navigation,"live-items-browser"));
    liveCheck(browser!=nullptr,"Items browser exists");
    const auto beforeBrowser=navigation.liveDocument->interfaceState["currentItem"].toString();
    const auto beforeFocus=navigation.liveTimelineFocusRevision;
    const auto beforeSample=navigation.app.engine()->state().sample;
    const auto beforeEntry=navigation.liveCurrentEntry;
    const int beforeCount=focus.itemCount();
    browser->activate({preview,preview,"Short Browser preview",0,false,false});
    for(int i=0;i<3;++i)livePump();
    focus.refresh();
    liveCheck(navigation.liveBrowserItem==preview &&
              navigation.liveDocument->interfaceState["currentItem"].toString()==beforeBrowser&&
              navigation.liveTimelineFocusRevision==beforeFocus &&
              navigation.app.engine()->state().sample==beforeSample &&
              navigation.liveCurrentEntry==beforeEntry &&
              focus.itemCount()==beforeCount&&focus.itemBounds(preview).isEmpty()&&
              live::entryIndex(navigation.liveDocument->interfaceState,preview)<0,
              "Browser-only item selects for inspection without altering setlist, Core or timeline");
    // Even a library item already present in the setlist must not activate the show.
    browser->activate({first,first,"Fixture Alpha",0,false,false});
    for(int i=0;i<3;++i)livePump();
    focus.refresh();
    liveCheck(navigation.liveBrowserItem==first &&
              navigation.liveDocument->interfaceState["currentItem"].toString()==beforeBrowser &&
              navigation.liveCurrentEntry==beforeEntry &&
              navigation.liveTimelineFocusRevision==beforeFocus &&
              navigation.app.engine()->state().sample==beforeSample &&
              focus.itemCount()==beforeCount,
              "Browser inspection of existing setlist item never selects its timeline occurrence");
    auto* browserPlaybackPanel=dynamic_cast<LivePlaybackPanel*>(liveComponent(navigation,"live-playback-panel"));
    liveCheck(browserPlaybackPanel!=nullptr,"Browser inspection has Playback/Stems panel");
    browserPlaybackPanel->sync();
    auto* audition=dynamic_cast<juce::TextButton*>(liveComponent(*browserPlaybackPanel,"live-browser-audition"));
    liveCheck(browserPlaybackPanel->stemCount()>0 && audition && audition->isEnabled(),
              "Browser inspection displays WAV/stems and offers independent audition");
    selectAndCheck(first,entry);
    liveCheck(navigation.liveBrowserItem.isEmpty(),
              "Explicit Setlist selection restores show Playback/Stems inspection");
    navigation.editLiveInterface("Add short preview to end of setlist",[&](auto& state){live::insertEntry(state,preview);});
    const auto previewIndex=live::entryIndex(navigation.liveDocument->interfaceState,preview);
    const auto previewEntry=live::activeSetlist(navigation.liveDocument->interfaceState)["entries"][previewIndex]["id"].toString();
    selectAndCheck(preview,previewEntry);
    liveCheck(focus.itemBounds(preview).getWidth()==16*16+2,"New empty timeline item occupies sixteen bars at its current zoom");
    selectAndCheck(second,{});
    navigation.editLiveInterface("Repeat an item at the end",[&](auto& state){live::insertEntry(state,second);});
    const auto duplicateEntries=live::activeSetlist(navigation.liveDocument->interfaceState)["entries"];
    const auto duplicateEntry=duplicateEntries[duplicateEntries.size()-1]["id"].toString();
    navigation.selectLiveItem(second,duplicateEntry);
    const auto duplicateDeadline=juce::Time::getMillisecondCounter()+5000;
    while(navigation.livePlaybackLoading&&juce::Time::getMillisecondCounter()<duplicateDeadline)livePump();
    focus.refresh();
    const int duplicateX=focus.itemBounds(focus.itemCount()-1).getX()-focusViewport->getViewPositionX();
    liveCheck(navigation.liveCurrentEntry==duplicateEntry&&duplicateX>=0&&duplicateX<=12,
              "Selecting another occurrence of the same item focuses its exact entry after activation");
}
void liveTimelineAuthoringSmoke(const juce::File &fixture) {
    Desktop editing(true,false,true);editing.importLiveShow(fixture);editing.setSize(1200,800);
    const auto id=editing.liveDocument->interfaceState["items"][0]["id"].toString();
    editing.selectLiveItem(id);
    editing.editLiveInterface("Timeline test setup",[&](auto &state){
        live::put(live::require(state,"items",id),"sections",juce::Array<juce::var>());
        live::put(live::map(live::performance(state),"chords"),id,juce::Array<juce::var>());
        live::setItemView(state,id,"scale",32.);live::setItemView(state,id,"lead",2.);live::setItemView(state,id,"length",24.);
    });editing.refresh();
    auto *timeline=dynamic_cast<LiveTimeline *>(liveComponent(editing,"live-show-timeline"));
    liveCheck(timeline!=nullptr,"Authoring timeline exists");timeline->refresh();
    auto *body=liveComponent(editing,"live-timeline-items");liveCheck(body!=nullptr,"Authoring timeline body exists");
    auto bounds=timeline->itemBounds(id);
    const float origin=static_cast<float>(bounds.getX()+1+64);
    const auto event=[&](float x,float y,int clicks=1,juce::ModifierKeys mods=juce::ModifierKeys::leftButtonModifier){
        const auto now=juce::Time::getCurrentTime(); const juce::Point<float> point{x,y};
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,mods,1,0,0,0,0,body,body,now,point,now,clicks,false);
    };
    body->mouseDoubleClick(event(origin+64.f,96.f,2));timeline->refresh();
    auto rows=live::musicalRows(editing.liveDocument->interfaceState,id,live::MusicalLane::sections);
    liveCheck(rows.size()==1 && rows[0]["name"].toString()=="Verse" && static_cast<double>(rows[0]["start"])==2.,
              "Double-click empty Sections creates native Verse at common origin");
    const auto section=rows[0]["id"].toString();
    const auto before=editing.liveDocument->signature();
    body->mouseDown(event(origin+80.f,96.f));body->mouseDrag(event(origin+144.f,96.f));
    liveCheck(editing.liveDocument->signature()==before,"Musical drag is preview only until release");
    body->mouseUp(event(origin+144.f,96.f));timeline->refresh();
    rows=live::musicalRows(editing.liveDocument->interfaceState,id,live::MusicalLane::sections);
    liveCheck(static_cast<double>(live::spanById(rows,section)["start"])==4.,"Section gesture commits snapped range");
    editing.undoLiveInterface();timeline->refresh();
    liveCheck(editing.liveDocument->signature()==before,"One Undo restores complete section gesture");
    body->mouseDown(event(origin+80.f,96.f));body->mouseDrag(event(origin+112.f,96.f));
    body->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));body->mouseUp(event(origin+112.f,96.f));
    liveCheck(editing.liveDocument->signature()==before,"Escape cancels gesture without history or model writes");
    body->mouseMove(event(origin+64.f,96.f));
    liveCheck(body->getMouseCursor()==noteTrimCursor(false),"Free section edge uses bracket cursor");
    editing.editLiveInterface("Shared seam fixture",[&](auto& state){
        live::setMusicalRows(state,id,live::MusicalLane::sections,live::newMusicalSpan(state,id,live::MusicalLane::sections,10.,24.,"Chorus"));
    });timeline->refresh();
    const float seam=origin+320.f;
    body->mouseMove(event(seam-1.f,96.f));
    liveCheck(body->getMouseCursor()==juce::MouseCursor(juce::MouseCursor::LeftRightResizeCursor),"Touching section edges use shared resize arrow");
    body->mouseDown(event(seam,96.f));body->mouseDrag(event(seam+32.f,96.f));body->mouseUp(event(seam+32.f,96.f));timeline->refresh();
    rows=live::musicalRows(editing.liveDocument->interfaceState,id,live::MusicalLane::sections);
    liveCheck(static_cast<double>(rows[0]["length"])==9. && static_cast<double>(rows[1]["start"])==11. && static_cast<double>(rows[1]["length"])==7.,
              "Native seam drag commits both neighbouring boundaries together");
    editing.undoLiveInterface();editing.undoLiveInterface();timeline->refresh();
    liveCheck(editing.liveDocument->signature()==before,"Seam Undo and fixture Undo preserve original section data");
    body->mouseDoubleClick(event(origin+32.f,136.f,2));timeline->refresh();
    rows=live::musicalRows(editing.liveDocument->interfaceState,id,live::MusicalLane::chords);
    liveCheck(rows.size()==1 && static_cast<double>(rows[0]["start"])==1.,"Chord creation shares the Sections origin");
    body->mouseDown(event(origin+48.f,136.f));
    liveCheck(body->keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)),"Delete handles selected chord");timeline->refresh();
    liveCheck(live::musicalRows(editing.liveDocument->interfaceState,id,live::MusicalLane::chords).size()==0,"Delete removes actual chord metadata");
    editing.undoLiveInterface();timeline->refresh();
    const auto musical=live::canonical(live::musicalVersion(editing.liveDocument->interfaceState,id));
    bounds=timeline->itemBounds(id);
    body->mouseDown(event(static_cast<float>(bounds.getRight()-2),180.f));
    body->mouseDrag(event(static_cast<float>(bounds.getRight()-130),180.f));
    body->mouseUp(event(static_cast<float>(bounds.getRight()-130),180.f));timeline->refresh();
    const auto view=editing.liveDocument->interfaceState["performance"]["arrangement"][juce::Identifier(id)];
    liveCheck(static_cast<double>(view["length"])==20. && static_cast<double>(view["scale"])==32.,
              "Right grip changes explicit item end without changing scale");
    liveCheck(live::canonical(live::musicalVersion(editing.liveDocument->interfaceState,id))==musical,
              "Shrinking item preserves hidden musical events");
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("live-timeline-edit",".mrlive",false);
    editing.saveFile(file);const auto reopened=LiveDocument::read(file);file.deleteFile();
    liveCheck(live::canonical(reopened.interfaceState)==live::canonical(editing.liveDocument->interfaceState),
              "Native musical edits and explicit item end survive Save/Open");
    const auto beforeModes=editing.liveDocument->signature();const auto modeEngine=editing.app.engine();
    editing.editLiveInterface("Section mode fixture",[&](auto& state){live::setPerformanceMode(state,id,section,mrs::PerformanceMode::loop);});
    editing.editLiveInterface("Item mode fixture",[&](auto& state){live::setPerformanceMode(state,id,id,mrs::PerformanceMode::stop);});
    liveCheck(live::itemPerformanceMode(editing.liveDocument->interfaceState,section)==mrs::PerformanceMode::loop &&
              live::itemPerformanceMode(editing.liveDocument->interfaceState,id)==mrs::PerformanceMode::stop && editing.app.engine()==modeEngine,
              "Item/section modes are authored on the same show and Engine");
    editing.saveFile(file);const auto modeShow=LiveDocument::read(file);file.deleteFile();
    liveCheck(live::itemPerformanceMode(modeShow.interfaceState,section)==mrs::PerformanceMode::loop &&
              live::itemPerformanceMode(modeShow.interfaceState,id)==mrs::PerformanceMode::stop,"Performance modes survive native mrlive Save/Open");
    editing.undoLiveInterface();editing.undoLiveInterface();
    liveCheck(editing.liveDocument->signature()==beforeModes,"Show Undo restores both mode edits exactly");
    editing.saveFile(file);file.deleteFile();
    liveCheck(editing.liveSavedSnapshot.has_value() &&
              live::sameShowValue(editing.liveDocument->interfaceState,
                                  editing.liveSavedSnapshot->interfaceState) &&
              !editing.liveDirty,
              "Saved show and its JSON-normalized baseline compare equal before Notes Undo");
    editing.liveUndo.clearUndoHistory();
    const auto oldNotes=live::find(editing.liveDocument->interfaceState,"items",id)["notes"].toString();
    const auto text=juce::String::repeatedString("notes ",150);
    editing.setLiveItemNotes(id,text);editing.setLiveItemNotes(id,text+"\nNext line");
    liveCheck(editing.liveUndo.getNumberOfUnitsTakenUpByStoredCommands() <=
                  static_cast<int>(oldNotes.getNumBytesAsUTF8()+text.getNumBytesAsUTF8()+100),
              "Notes Undo stores text rather than repeated full show snapshots");
    editing.undoLiveInterface();
    if(editing.liveDirty) {
        const auto first=editing.liveSavedSnapshot ?
            live::firstShowDifference(editing.liveDocument->interfaceState,
                                      editing.liveSavedSnapshot->interfaceState)
            : juce::String("snapshot not present");
        throw std::runtime_error("Notes Undo stayed dirty: mismatch="+first.toStdString());
    }
    liveCheck(live::find(editing.liveDocument->interfaceState,"items",id)["notes"].toString()==oldNotes && !editing.liveDirty,
              "One Notes Undo restores saved text and clears the show dirty flag");
}
} // namespace
void liveInterfaceSmoke(const juce::File &fixture) {
    live::test::playbackViews();
    live::test::timelineEditing();
    live::test::clipEditing();
    live::test::performancePolicies();
    live::test::showValueEquality();
    liveTimelineAuthoringSmoke(fixture);
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
    for (const auto pointer : {juce::Point<int>{348, 117}, juce::Point<int>{-800, 260}}) {
        const auto target = liveContextTarget(pointer);
        liveCheck(target.getPosition() == pointer && target.getWidth() == 1 && target.getHeight() == 1,
                  "Context target preserves screen coordinates and uses a one-pixel size");
    }
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
    {
        // Freeze the pre-0.8.6 document shape without using the current writer/validator.
        auto legacy = live::object();
        live::put(legacy, "schema", "mr-live-show"); live::put(legacy, "version", 2);
        live::put(legacy, "title", "Legacy percent pan"); live::put(legacy, "active", -1);
        live::put(legacy, "projects", juce::Array<juce::var>()); live::put(legacy, "itemProjects", live::object());
        auto state = imported.clone();
        const double pans[]{-100., 1., 75., -1.};
        for (int i = 0; i < state["channels"].size(); ++i) {
            auto channel = state["channels"][i]; live::put(channel, "pan", pans[i]);
        }
        auto row = live::object(); live::put(row, "file", "legacy.wav"); live::put(row, "gain", 0.);
        live::put(row, "mute", false); live::put(row, "solo", false);
        live::put(live::map(live::performance(state), "nativePlayback"), id, row);
        live::saveItem(state, id); // Browser snapshot also contains the historical percent values.
        live::put(legacy, "interface", state);
        const auto file = folder.getChildFile("legacy-percent-pan.mrlive");
        const auto bytes = juce::JSON::toString(legacy, false, 17);
        liveCheck(file.replaceWithText(bytes), "Prepare legacy percent-pan save");
        const auto loaded = LiveDocument::read(file);
        liveCheck(file.loadFileAsString() == bytes, "Opening a legacy save does not rewrite the original");
        for (int i = 0; i < state["channels"].size(); ++i)
            liveCheck(static_cast<double>(loaded.interfaceState["channels"][i]["pan"]) == pans[i] &&
                          std::abs(live::channelPan(loaded.interfaceState["channels"][i]) - pans[i] / 100.) < .000001,
                      "Legacy percent pan retains both extremes and near-centre values");
        liveCheck(std::abs(live::playbackMix(loaded.interfaceState, id, live::playback(loaded.interfaceState, id)).pan - .75f) < .000001f,
                  "Legacy Playback pan is normalized only at the Core boundary");
        const auto roundtrip = folder.getChildFile("legacy-percent-pan-roundtrip.mrlive");
        loaded.write(roundtrip);
        const auto reopened = LiveDocument::read(roundtrip);
        liveCheck(live::canonical(reopened.interfaceState) == live::canonical(loaded.interfaceState),
                  "Old save and Browser versions round-trip without pan conversion in the document");
        auto invalid = state.clone(); auto channel = invalid["channels"][0]; live::put(channel, "pan", 101.);
        bool rejected = false;
        try { live::validate(invalid); } catch (const std::exception &) { rejected = true; }
        liveCheck(rejected, "Out-of-range show pan still fails validation");
    }
    // #298: saved physical input, routed mix and plugin state are SHOW metadata,
    // not a hidden .mrsproject. Reading must never arm a monitor automatically.
    {
        mrs::Track input;
        input.id=mrs::new_id();
        input.name="Guitar";
        input.kind=mrs::TrackKind::audio;
        input.input=0;
        input.input_monitor=true; // must not be serialized as active monitoring
        input.hardware_outputs={0,1};
        input.mix.gain=.75f;
        input.mix.pan=-.25f;
        const auto encoded=live::liveInputProfile(input,"Physical Input 1","Test ASIO device",{});
        const auto decoded=live::decodeLiveInputProfile(encoded);
        liveCheck(decoded.input==0 && decoded.name=="Physical Input 1" &&
                  decoded.device=="Test ASIO device" &&
                  decoded.outputs==input.hardware_outputs && decoded.mix==input.mix &&
                  decoded.inserts.empty() && encoded["monitor"].isVoid(),
                  "Live Input profile encodes physical binding/mix but never armed monitoring");
        const std::vector<std::byte> sampleState{std::byte{0},std::byte{0x7f},std::byte{0xff}};
        liveCheck(live::liveInputBytes(live::liveInputB64(sampleState))==sampleState &&
                  live::liveInputBytes(juce::String{}).empty(),
                  "VST3 component/controller bytes round-trip without loss");
        auto source=imported.clone();
        const auto channelId=source["channels"][0]["id"].toString();
        live::put(live::require(source,"channels",channelId),"liveInput",encoded);
        LiveDocument doc;doc.title="Profile saved";doc.interfaceState=source;
        const auto file=folder.getChildFile("live-input-profile.mrlive");
        doc.write(file);
        const auto reopened=LiveDocument::read(file);
        const auto saved=reopened.interfaceState["channels"][0]["liveInput"];
        liveCheck(reopened.projects.empty() && reopened.active==-1 &&
                  live::canonical(saved)==live::canonical(encoded) &&
                  live::decodeLiveInputProfile(saved).input==0,
                  "Standalone .mrlive preserves Live Input profile without Studio song/project refs");
        // Reproduction of user save failure: sample/VSTi plugin state can
        // exceed 256 KiB. 7 MiB raw expands to >8 MiB Base64 JSON, so this
        // exercises both the Core state cap and the .mrlive file-size cap.
        mrs::NativeInsert largeVst3;
        largeVst3.id=mrs::new_id();largeVst3.kind=mrs::InsertKind::vst3;
        largeVst3.plugin_path="test-large-state.vst3";
        largeVst3.class_id="0123456789abcdef0123456789abcdef";
        largeVst3.plugin_name="Large VST3 State";
        largeVst3.component_state.assign(7u*1024u*1024u,std::byte{0x3f});
        largeVst3.validate();
        auto largeShow=doc.clone();
        const auto cleanBaseline=largeShow.savedComparisonSnapshot();
        liveCheck(live::sameShowValue(largeShow.interfaceState,cleanBaseline.interfaceState),
                  "Saved comparison snapshot matches a cloned small show before large VST3 assignment");
        live::put(live::require(largeShow.interfaceState,"channels",channelId),"liveInput",
                  live::liveInputProfile(input,"Physical Input 1","Test ASIO device",{largeVst3}));
        const auto largeFile=folder.getChildFile("large-vst3-state.mrlive");
        largeShow.write(largeFile);
        liveCheck(largeFile.getSize()>8ll*1024ll*1024ll,
                  "Large VST3 test must exceed previous 8 MiB show file limit");
        const auto largeLoaded=LiveDocument::read(largeFile);
        const auto roundtripState=live::decodeLiveInputProfile(
            live::find(largeLoaded.interfaceState,"channels",channelId)["liveInput"]);
        liveCheck(roundtripState.inserts.size()==1 &&
                  roundtripState.inserts[0].component_state==largeVst3.component_state &&
                  roundtripState.inserts[0].class_id==largeVst3.class_id,
                  "VST3 state greater than 256 KiB survives whole-show save/reopen");
        // Regression for UI starvation with large VSTi state: Playback's
        // 30 Hz mix key depends on scalar mixer values, not binary VST3 blobs.
        const auto selectedItem=largeLoaded.interfaceState["currentItem"].toString();
        const auto baselineMixKey=live::playbackMixKey(largeLoaded.interfaceState,selectedItem);
        liveCheck(baselineMixKey.getNumBytesAsUTF8()<16*1024,
                  "Playback polling key must remain small with multi-megabyte VST3 preset data");
        auto modifiedShow=largeLoaded.interfaceState.clone();
        liveCheck(live::sameShowValue(modifiedShow,largeLoaded.interfaceState),
                  "Deep-cloned show is unchanged without JSON serialization");
        {
            auto channel=live::require(modifiedShow,"channels",channelId);
            auto profile=channel["liveInput"];
            auto insert=profile["inserts"][0];
            live::put(insert,"component",juce::String("AA=="));
        }
        liveCheck(!live::sameShowValue(modifiedShow,largeLoaded.interfaceState),
                  "In-memory show comparison still detects modified VST3 component state");
        liveCheck(live::playbackMixKey(modifiedShow,selectedItem)==baselineMixKey,
                  "Large VST3 binary state is excluded from the Playback polling mix key");
        live::put(live::require(modifiedShow,"channels",channelId),"gain",-5.);
        liveCheck(live::playbackMixKey(modifiedShow,selectedItem)!=baselineMixKey,
                  "Playback mix key still tracks channel scalar gain");
        largeFile.deleteFile();
        bool oversizedStateRejected=false;
        try{(void)live::liveInputB64(std::vector<std::byte>(live::maximumLiveVst3StateBytes+1u));}
        catch(const std::exception&){oversizedStateRejected=true;}
        liveCheck(oversizedStateRejected,"Oversized VST3 binary state remains bounded");
        auto invalid=source.clone();
        auto invalidProfile=invalid["channels"][0]["liveInput"];
        live::put(invalidProfile,"input",-5);
        bool rejected=false;
        try {live::validateLiveInputProfiles(invalid);}catch(const std::exception&){rejected=true;}
        liveCheck(rejected, "Malformed saved physical input is rejected on validation");
        auto synth=input;synth.kind=mrs::TrackKind::instrument;synth.input=-1;synth.input_stereo=false;
        synth.midi_input="saved-midi-port";synth.midi_channel=15;synth.midi_monitor=true;
        const auto midiProfile=live::liveInputProfile(synth,"MIDI keyboard","Test ASIO device",{});
        const auto midiLoaded=live::decodeLiveInputProfile(midiProfile);
        liveCheck(midiLoaded.instrument && midiLoaded.midiInput=="saved-midi-port" && midiLoaded.midiChannel==15 &&
                  midiProfile["midiMonitor"].isVoid() && midiProfile["monitor"].isVoid(),
                  "MIDI descriptor preserves port/channel without authorizing automatic monitoring or plugin load");
        // Saved .mrlive MIDI descriptors use -1, whereas the shared Core
        // Project::validate requires -2 for every non-audio track. Verify the
        // projection in both directions without changing the show schema.
        auto coreSynth=synth;
        coreSynth.input=live::liveCoreTrackInput(midiLoaded);
        const auto coreCaptured=live::liveInputProfile(coreSynth,"MIDI keyboard","Test ASIO device",{});
        liveCheck(midiLoaded.input==-1 && coreSynth.input==-2 &&
                  static_cast<int>(coreCaptured["input"])==-1 &&
                  live::decodeLiveInputProfile(coreCaptured).input==-1,
                  "MIDI channel maps saved input -1 to Core input -2 and back on capture");
        // Timeline Instrument owns MIDI input/channel/VSTi. Mixer owns FX, not
        // a second VSTi selector, and must not overwrite the .mrlive MIDI role.
        {
            Desktop uiPilot(true,false,true);uiPilot.importLiveShow(fixture);
            juce::String midiChannelId;
            uiPilot.editLiveInterface("MIDI channel control UI",[&](auto& state) {
                midiChannelId=live::addChannel(state,"Instrument","Test MIDI");
                live::put(live::require(state,"channels",midiChannelId),"liveInput",midiProfile.clone());
            });
            LiveMixer realMixer(uiPilot);
            realMixer.setSize(500,340);realMixer.sync();
            auto* instrumentPick=dynamic_cast<juce::TextButton*>(
                liveComponent(realMixer,"live-mixer-instrument-"+midiChannelId));
            liveCheck(instrumentPick && instrumentPick->isVisible() &&
                      instrumentPick->getButtonText()=="Choose VSTi",
                      "Instrument VSTi selector is present in Mixer under pan/level");
            LiveTimeline realTimeline(uiPilot);
            realTimeline.setSize(600,540);realTimeline.refresh();
            auto* editorIcon=dynamic_cast<juce::Button*>(
                liveComponent(realTimeline,"live-timeline-editor-"+midiChannelId));
            liveCheck(editorIcon!=nullptr && !editorIcon->isEnabled(),
                      "Instrument timeline exposes an editor icon, disabled until VSTi is assigned");
            LiveChannelControls timeline(uiPilot,midiChannelId,LiveChannelControls::Surface::timeline);
            LiveChannelControls mixer(uiPilot,midiChannelId,LiveChannelControls::Surface::mixer);
            timeline.setBounds(0,0,220,44);mixer.setBounds(0,0,220,44);
            timeline.sync();mixer.sync();
            const auto getButton=[](LiveChannelControls& c,const juce::String& name) {
                return dynamic_cast<juce::TextButton*>(c.findChildWithID(name));
            };
            auto* midiInput=getButton(timeline,"live-channel-input-"+midiChannelId);
            auto* midiPort=getButton(timeline,"live-channel-output-"+midiChannelId);
            auto* instrumentButton=getButton(timeline,"live-channel-effects-"+midiChannelId);
            auto* mixerInput=getButton(mixer,"live-channel-input-"+midiChannelId);
            auto* mixerOutput=getButton(mixer,"live-channel-output-"+midiChannelId);
            auto* mixerFx=getButton(mixer,"live-channel-effects-"+midiChannelId);
            liveCheck(midiInput && midiInput->getButtonText()=="Missing MIDI" &&
                      midiPort && midiPort->getButtonText()=="MIDI Ch 16" &&
                      instrumentButton && instrumentButton->getButtonText()=="Choose Instrument",
                      "Timeline Instrument labels show MIDI port/channel and VSTi, not Master or audio FX");
            // synth inherits hardware_outputs={0,1} from the audio fixture.
            // A non-empty physical route must appear as "Out 1", not "Master";
            // MIDI Channel replaces Master ONLY on the Timeline surface.
            liveCheck(midiLoaded.outputs==std::vector<int>{0,1} &&
                      mixerInput && !mixerInput->isVisible() &&
                      mixerOutput && mixerOutput->isVisible() && mixerOutput->getButtonText()=="Out 1" &&
                      mixerFx && mixerFx->isVisible() && mixerFx->getButtonText()=="Audio FX (0)",
                      "Instrument Mixer retains assigned physical output and audio FX, without duplicated MIDI/VSTi");
            // The same Mixer selector must display Master for an intentionally
            // empty physical output route (and never overwrite MIDI channel).
            // Regression for Windows #383: the original fixture has Out 1,
            // so asserting Master before clearing outputs is incorrect.
            uiPilot.editLiveInterface("Default Instrument output",[&](auto& state) {
                auto target=live::require(state,"channels",midiChannelId);
                auto savedProfile=target["liveInput"];
                live::put(savedProfile,"outputs",juce::Array<juce::var>{});
            });
            mixer.sync();timeline.sync();
            liveCheck(mixerOutput->getButtonText()=="Master" &&
                      midiPort->getButtonText()=="MIDI Ch 16",
                      "Default physical output is Master in Mixer while Timeline keeps MIDI Channel");
        }
        auto invalidMidi=midiProfile.clone();live::put(invalidMidi,"midiChannel",16);rejected=false;
        try{(void)live::decodeLiveInputProfile(invalidMidi);}catch(const std::exception&){rejected=true;}
        liveCheck(rejected,"Out-of-range saved MIDI channel rejects before activation");
        mrs::NativeInsert old;old.id=mrs::new_id();old.kind=mrs::InsertKind::vst3;
        old.plugin_path="fixture.vst3";old.class_id="fixture-class";
        auto current=old;current.component_state=sampleState;current.controller_state={std::byte{3}};
        current.parameters={{7,.75f}};
        auto bypassed=old;bypassed.bypass=true;
        std::vector<mrs::NativeInsert> edited{bypassed};
        live::retainCapturedPluginState(edited,{old},{current});
        liveCheck(edited[0].bypass && edited[0].component_state==current.component_state &&
                  edited[0].controller_state==current.controller_state && edited[0].parameters==current.parameters,
                  "Rack bypass retains fresh editor binary/controller/parameter state");
        auto replacement=old;replacement.id=mrs::new_id();edited={replacement};
        live::retainCapturedPluginState(edited,{old},{current});
        liveCheck(edited[0]==replacement,"Replacing a plugin never inherits a different instance's state");
        auto restored=old;restored.component_state={std::byte{9}};restored.parameters={{7,.25f}};edited={restored};
        live::retainCapturedPluginState(edited,{old},{current});
        liveCheck(edited[0].component_state==restored.component_state && edited[0].parameters==restored.parameters,
                  "Explicit restored state takes precedence over capture");
    }
    // #297/#300: an active monitored Core input must never be retired by
    // an item switch while playback is running. Stopped manual handoff now
    // belongs to #300 and is covered by the shared Live+WAV fixture below.
    {
        Desktop inputPilot(true,false,true);
        inputPilot.importLiveShow(fixture);
        // Regression for 0.8.9 crash log: Core rejects input=-1 on Instrument
        // even though it is the intentionally persisted MIDI descriptor value.
        auto midiCandidate=*inputPilot.project();
        mrs::Track candidateInstrument{mrs::new_id(),"MIDI core input regression",mrs::TrackKind::instrument,{}};
        candidateInstrument.input=live::liveCoreTrackInput(live::LiveInputLoaded{true});
        midiCandidate.tracks.push_back(candidateInstrument);
        midiCandidate.validate();
        midiCandidate.tracks.back().input=-1;
        bool invalidMidiCore=false;
        try{midiCandidate.validate();}catch(const std::exception&){invalidMidiCore=true;}
        liveCheck(invalidMidiCore,"Core refuses the saved MIDI -1 sentinel; Live projection must use -2");
        const auto engine=inputPilot.app.engine();
        const auto showChannel=inputPilot.liveDocument->interfaceState["channels"][1]["id"].toString();
        liveCheck(live::find(inputPilot.liveDocument->interfaceState,"channels",showChannel)["type"].toString()=="Player",
                  "Physical Live Input acceptance uses a Player rather than an Instrument");
        bool rejectedOffline=false;
        try { inputPilot.startLiveAudioInput(0,showChannel); }
        catch (const std::exception&) { rejectedOffline=true; }
        liveCheck(rejectedOffline && !inputPilot.liveInputTrack(),
                  "Live Input pilot rejects the offline clock without changing Core");
        inputPilot.app.connect(std::make_unique<LivePlaybackTestDevice>(),
                               {0,48000,128,{0},{0,1}});
        inputPilot.startLiveAudioInput(0,showChannel);
        liveCheck(inputPilot.liveInputTrack().has_value() &&
                  inputPilot.project()->tracks.size()==1 &&
                  inputPilot.project()->tracks.front().kind==mrs::TrackKind::audio &&
                  inputPilot.project()->tracks.front().input==0 &&
                  inputPilot.project()->tracks.front().input_monitor &&
                  inputPilot.app.engine()==engine,
                  "Physical Live Input monitor uses the existing shared Core engine");
        // Consume queued transport commands on the fake device clock before
        // asserting the active-Play guard (the prior stopped-only assertion
        // conflicted with the deliberately enabled #300 manual handoff).
        std::array<float,128> pilotSilentInput{};
        std::array<float,256> pilotAudioOutput{};
        inputPilot.app.play();
        inputPilot.app.engine()->process(pilotSilentInput.data(),pilotAudioOutput.data(),128);
        liveCheck(inputPilot.app.engine()->state().playback==mrs::PlaybackState::playing,
                  "Live Input guard test must reach an active Playback state");
        bool switched=false;
        try { inputPilot.selectLiveItem("item-1"); switched=true; }
        catch (const std::exception&) {}
        liveCheck(!switched && inputPilot.liveDocument->interfaceState["currentItem"].toString()=="item-0" &&
                  inputPilot.liveInputTrack().has_value(),
                  "Active setlist switch cannot retire a monitored Live Input");
        inputPilot.app.stop();
        inputPilot.app.engine()->process(pilotSilentInput.data(),pilotAudioOutput.data(),128);
        LiveChannelControls inputPanel(inputPilot,showChannel);
        inputPanel.setSize(150,44);inputPanel.sync();
        liveCheck(liveComponent(inputPanel,"live-channel-input-"+showChannel)!=nullptr &&
                  liveComponent(inputPanel,"live-channel-output-"+showChannel)!=nullptr &&
                  liveComponent(inputPanel,"live-channel-effects-"+showChannel)!=nullptr,
                  "#302 channel embeds input, output and VST3 chain selectors");
        LiveMixer channelMixer(inputPilot);channelMixer.setSize(700,300);channelMixer.sync();
        liveCheck(liveComponent(channelMixer,"live-channel-input-"+showChannel)!=nullptr &&
                  liveComponent(channelMixer,"live-channel-monitor-"+showChannel)!=nullptr,
                  "#302 Mixer embeds the same channel selectors and monitoring");
        liveCheck(live::find(inputPilot.liveDocument->interfaceState,"channels",showChannel)["liveInput"].isObject(),
                  "Channel input changes immediately belong to the show");
        const auto pilotSaved=folder.getChildFile("pilot-shared-core-save.mrlive");
        inputPilot.saveFile(pilotSaved);
        const auto pilotLoaded=LiveDocument::read(pilotSaved);
        liveCheck(pilotLoaded.projects.empty() && pilotLoaded.itemProjects.empty() &&
                  live::find(pilotLoaded.interfaceState,"channels",showChannel)["liveInput"].isObject(),
                  "Saving .mrlive with active monitor does not create a linked Studio project");
        inputPilot.releaseLiveInput();
        liveCheck(!inputPilot.liveInputTrack() &&
                  inputPilot.project()->tracks.empty() &&
                  inputPilot.app.engine()==engine,
                  "Disabling Live Input safely removes temporary Core monitoring without a second engine");
        inputPilot.activateLiveChannel(showChannel);
        liveCheck(inputPilot.liveInputTrack() &&
                  inputPilot.project()->tracks.size()==1 &&
                  !inputPilot.project()->tracks.front().input_monitor &&
                  inputPilot.app.engine()==engine,
                  "Loading saved input profile restores Core track with monitor OFF");
        inputPilot.setLiveInputMonitor(showChannel,true);
        liveCheck(inputPilot.project()->tracks.front().input_monitor,
                  "Enabling a restored Live Input requires a separate explicit action");
        inputPilot.releaseLiveInput();
        // #302 A1: use the existing Core stereo input and output routing commands
        // from the native channel inspector, without inventing a new audio host.
        inputPilot.startLiveAudioInput(0,showChannel,true);
        liveCheck(inputPilot.project()->tracks.front().input_stereo &&
                  inputPilot.project()->tracks.front().input==0,
                  "#302 stereo input selection reaches a real shared Core audio track");
        inputPilot.app.set_hardware_output(*inputPilot.liveInputTrack(),{0,1});
        inputPilot.saveFile(pilotSaved);
        const auto stereoProfile=live::decodeLiveInputProfile(
            live::find(inputPilot.liveDocument->interfaceState,"channels",showChannel)["liveInput"]);
        liveCheck(stereoProfile.stereo && stereoProfile.outputs==std::vector<int>{0,1} &&
                  stereoProfile.input==0,
                  "#302 channel stereo source and output routing belong to the saved show");
        juce::String secondChannel;
        inputPilot.editLiveInterface("Add second audio channel",[&](auto& state){secondChannel=live::addChannel(state,"Audio","Second input");});
        inputPilot.startLiveAudioInput(1,secondChannel,false);
        const auto firstCore=*inputPilot.liveInputTrack(showChannel),secondCore=*inputPilot.liveInputTrack(secondChannel);
        liveCheck(firstCore!=secondCore && inputPilot.project()->tracks.size()==2 && inputPilot.app.engine()==engine,
                  "#302 A2 maps two show channels to independent stable tracks on the original Core");
        inputPilot.setLiveChannelMix(secondChannel,-6.,.25,false,false);
        inputPilot.undoLiveInterface();
        auto secondMix=[&]{for(const auto& track:inputPilot.project()->tracks)if(track.id==secondCore)return track.mix;return mrs::Track::Mix{};};
        liveCheck(std::abs(secondMix().gain-1.f)<.0001f,"Channel mixer Undo reaches the real Core mix");
        inputPilot.undoLiveInterface(true);
        liveCheck(std::abs(secondMix().gain-static_cast<float>(std::pow(10.,-.3)))<.0001f,"Channel mixer Redo reaches the real Core mix");
        const auto multiSaved=folder.getChildFile("multi-input-show.mrlive");inputPilot.saveFile(multiSaved);
        const auto multiLoaded=LiveDocument::read(multiSaved);
        liveCheck(live::find(multiLoaded.interfaceState,"channels",showChannel)["liveInput"].isObject() &&
                  live::find(multiLoaded.interfaceState,"channels",secondChannel)["liveInput"].isObject() && multiLoaded.projects.empty(),
                  "Ordinary Save persists every channel without separate channel Save actions");
        inputPilot.releaseLiveInput(secondChannel);inputPilot.releaseLiveInput(showChannel);
        inputPilot.openFile(multiSaved);
        liveCheck(inputPilot.liveChannelTracks.empty() && inputPilot.project()->tracks.empty(),
                  "Open restores show settings with no automatic plugin activation or monitoring");
        // #302/#281: preserve legacy Instrument audio profiles and use explicit Undoable conversion.
        juce::String legacyChannel;
        inputPilot.editLiveInterface("Create legacy Instrument fixture",[&](auto& state){
            legacyChannel=live::addChannel(state,"Instrument","Legacy Guitar");
            live::put(live::require(state,"channels",legacyChannel),"liveInput",
                      live::find(state,"channels",showChannel)["liveInput"].clone());
        });
        const auto legacyBefore=live::canonical(live::find(inputPilot.liveDocument->interfaceState,"channels",legacyChannel)["liveInput"]);
        const auto itemsBeforeConversion=live::canonical(inputPilot.liveDocument->interfaceState["items"]);
        bool refused=false;
        try{inputPilot.configureLiveInput(legacyChannel,0,false,{});}catch(const std::exception&){refused=true;}
        liveCheck(refused,"Instrument rejects new physical audio input and keeps the legacy descriptor");
        refused=false;
        try{inputPilot.configureLiveMidi(legacyChannel,{},-1,{});}catch(const std::exception&){refused=true;}
        liveCheck(refused,"MIDI selection cannot silently replace a legacy Instrument audio chain");
        refused=false;
        try{inputPilot.activateLiveChannel(legacyChannel);}catch(const std::exception&){refused=true;}
        liveCheck(refused && !inputPilot.liveInputTrack(legacyChannel),
                  "Legacy Instrument audio remains inactive until explicit conversion");
        inputPilot.convertLegacyLiveInstrument(legacyChannel);
        liveCheck(live::find(inputPilot.liveDocument->interfaceState,"channels",legacyChannel)["type"].toString()=="Player" &&
                  live::canonical(live::find(inputPilot.liveDocument->interfaceState,"channels",legacyChannel)["liveInput"])==legacyBefore &&
                  live::canonical(inputPilot.liveDocument->interfaceState["items"])==itemsBeforeConversion,
                  "Undoable role conversion preserves channel ID, audio profile and item/patch relationships");
        inputPilot.undoLiveInterface();
        liveCheck(live::find(inputPilot.liveDocument->interfaceState,"channels",legacyChannel)["type"].toString()=="Instrument" &&
                  live::canonical(live::find(inputPilot.liveDocument->interfaceState,"channels",legacyChannel)["liveInput"])==legacyBefore,
                  "Undo restores the legacy Instrument role without deleting its profile");
        inputPilot.undoLiveInterface(true);
        liveCheck(live::find(inputPilot.liveDocument->interfaceState,"channels",legacyChannel)["type"].toString()=="Player",
                  "Redo re-applies the explicit Player conversion");
        // Metadata is keyed by plugin class ID, not display name or the VST3 module extension.
        mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::vst3;
        fx.plugin_path="synthetic-shared.vst3";fx.class_id="0123456789abcdef0123456789abcdef";
        fx.plugin_name="Same Name";
        auto vsti=fx;vsti.id=mrs::new_id();vsti.class_id="fedcba9876543210fedcba9876543210";
        // Use legitimate 32-hex VST3 class IDs: invalid IDs fail NativeInsert::validate
        // before the metadata-based FX/VSTi assignment guards are exercised.
        fx.validate();
        vsti.validate();
        inputPilot.catalog.push_back({fx.plugin_path,fx.class_id,fx.plugin_name,"Synthetic Vendor","1",false});
        inputPilot.catalog.push_back({vsti.plugin_path,vsti.class_id,vsti.plugin_name,"Synthetic Vendor","1",true});
        std::vector<mrs::processing::VstPlugin> mixedCatalog{
            {"z.vst3","0000000000000000000000000000000a","ZZZ","Zed","1",false},
            {"a.vst3","00000000000000000000000000000002","Equal","Acme","1",false},
            {"b.vst3","00000000000000000000000000000001","Equal","acme","1",false},
            {"i.vst3","00000000000000000000000000000003","Equal","Acme","1",true},
            {"unknown.vst3","00000000000000000000000000000004","Unknown","Acme","1",std::nullopt}};
        const auto sortedFx=live::sortedPluginClasses(mixedCatalog,false);
        const auto sortedVsti=live::sortedPluginClasses(mixedCatalog,true);
        liveCheck(sortedFx==std::vector<std::size_t>{2,1,0} &&
                  sortedVsti==std::vector<std::size_t>{3} &&
                  live::findPluginClass(mixedCatalog,"b.vst3","00000000000000000000000000000001")==&mixedCatalog[2],
                  "Vendor/name case-insensitive sorting preserves exact class ID across mixed VSTi/FX names");
        liveCheck(live::findPluginClass(mixedCatalog,"unknown.vst3","00000000000000000000000000000004") &&
                  !live::findPluginClass(mixedCatalog,"unknown.vst3","00000000000000000000000000000004")->instrument.has_value(),
                  "Unknown plugin class stays distinct from a confirmed audio effect");
        inputPilot.setLiveInputEffects(showChannel,{fx});
        const auto fxBeforeRejected=live::canonical(live::find(inputPilot.liveDocument->interfaceState,"channels",showChannel)["liveInput"]);
        refused=false;
        try{inputPilot.setLiveInputEffects(showChannel,{vsti});}catch(const std::exception&){refused=true;}
        liveCheck(refused && live::canonical(live::find(inputPilot.liveDocument->interfaceState,"channels",showChannel)["liveInput"])==fxBeforeRejected,
                  "Player rejects VSTi with the same display name and does not change show/Undo");
        auto unknown=fx;unknown.id=mrs::new_id();unknown.class_id="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        unknown.validate(); // syntactically valid class, but absent from scanner catalog
        refused=false;
        try{inputPilot.setLiveInputEffects(showChannel,{unknown});}catch(const std::exception&){refused=true;}
        liveCheck(refused && live::canonical(live::find(inputPilot.liveDocument->interfaceState,"channels",showChannel)["liveInput"])==fxBeforeRejected,
                  "Unknown class ID cannot be treated as audio FX without scanner metadata");
        // The FX editor may be requested while the show holds an inert plugin
        // descriptor. A stale slot or missing vendor module must not activate
        // monitoring, mutate the show, or leave a partial Core track.
        refused=false;
        try{inputPilot.openLiveInsertEditor(showChannel,unknown.id);}catch(const std::exception&){refused=true;}
        liveCheck(refused && !inputPilot.liveInputTrack(showChannel),
                  "Stale Live plugin menu action cannot activate a channel");
        refused=false;
        try{inputPilot.openLiveInsertEditor(showChannel,fx.id);}catch(const std::exception&){refused=true;}
        liveCheck(refused && !inputPilot.liveInputTrack(showChannel) &&
                  live::canonical(live::find(inputPilot.liveDocument->interfaceState,"channels",showChannel)["liveInput"])==fxBeforeRejected,
                  "Unavailable VST3 editor request preserves inactive channel and saved plugin descriptor");
        inputPilot.releaseLiveInput();
    }
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
    const auto savedBytes=saved.loadFileAsString();
    desktop.setPreview({},mrs::Track::Mix{},desktop.project()->master_gain,[]{return true;});
    desktop.action(3);
    liveCheck(desktop.message.contains("active edit")&&saved.loadFileAsString()==savedBytes,
              "Save during an active gesture reports why it is blocked instead of silently dropping the command");
    desktop.cancelPreview();desktop.action(3);
    liveCheck(desktop.message.isEmpty()&&!desktop.documentDirty(),"Save action works once the gesture finishes");
    desktop.run([&]{desktop.saveFile(saved.getChildFile("invalid.mrlive"));});
    liveCheck(desktop.message.isNotEmpty()&&desktop.documentFile()==saved&&saved.loadFileAsString()==savedBytes,
              "Failed save reports an error and preserves the original destination and saved file");
    desktop.message.clear();
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
    auto *playbackSurface = liveComponent(desktop, "live-playback-panel");
    liveCheck(playbackSurface != nullptr, "Browser dismissal test has Playback/Stems panel");
    const auto browserClick = [&](juce::Component *origin) {
        const auto when = juce::Time::getCurrentTime();
        const juce::Point<float> where{4.f, 4.f};
        const juce::MouseEvent event(
            juce::Desktop::getInstance().getMainMouseSource(), where,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
            origin, origin, when, where, when, 1, false);
        native->mouseDown(event);
    };
    browserClick(playbackSurface);
    liveCheck(native->itemsBrowserVisible(),
              "Playback/Stems click does not dismiss Browser inspection");
    browserClick(timeline);
    liveCheck(!native->itemsBrowserVisible(),
              "Timeline click explicitly dismisses Browser inspection");
    native->showBrowser(true);
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
    liveCheck(timeline->itemCount() == 0 && timeline->channelCount() == 4 &&
                  live::activeEntries(desktop.liveDocument->interfaceState).size() == 0,
              "Empty setlist has no timeline items and retains shared channels");
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
    auto *notesField = dynamic_cast<juce::TextEditor *>(liveComponent(card, "live-item-notes"));
    auto *nameField = dynamic_cast<juce::TextEditor *>(liveComponent(card, "live-item-name"));
    auto *tempoField = dynamic_cast<juce::TextEditor *>(liveComponent(card, "live-item-tempo"));
    liveCheck(notesField && notesField->isVisible() && nameField && tempoField, "Item fields are visible without Properties or Apply");
    liveCheck(notesField->getY() < tempoField->getY(), "Notes are directly below tasks, above musical properties");
    int rootActions = 0;
    bool hasOldItemAction = false;
    juce::StringArray availableActions;
    int statusWidth = -1;
    for (auto *child : card.getChildren()) {
        if (auto *button = dynamic_cast<juce::TextButton *>(child)) {
            ++rootActions;
            availableActions.add(button->getButtonText());
        }
        if (auto *statusBox = dynamic_cast<LiveStatusBox *>(child))
            statusWidth = statusBox->getWidth();
    }
    hasOldItemAction = availableActions.contains("Priority / Progress...") ||
                       availableActions.contains("Add to Setlist") ||
                       availableActions.contains("Undo show") || availableActions.contains("Redo show");
    liveCheck(rootActions == 4 && !hasOldItemAction && statusWidth == 94,
              "Item context keeps four compact actions and a narrower Status control");
    const auto oldNotes = live::find(desktop.liveDocument->interfaceState, "items", id)["notes"].toString();
    card.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    card.toFront(true); livePump();
    notesField->grabKeyboardFocus(); livePump();
    liveCheck(notesField->hasKeyboardFocus(true), "Notes input has native keyboard focus");
    notesField->setText("Immediate note", true);
    livePump();
    notesField->moveCaretToEnd();
    liveCheck(notesField->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)), "Notes handles Enter as a new line");
    livePump();
    notesField->insertTextAtCaret("Second line"); livePump();
    const auto actualNotes=live::find(desktop.liveDocument->interfaceState,"items",id)["notes"].toString();
    if(actualNotes != "Immediate note\nSecond line" || !desktop.liveDirty)
        throw std::runtime_error(("Notes input/model mismatch: field="+juce::JSON::toString(juce::var(notesField->getText()))+
            "; model="+juce::JSON::toString(juce::var(actualNotes))+"; dirty="+juce::String(desktop.liveDirty?1:0)).toStdString());
    // Once focus is outside the editor, a pending text notification still must
    // win over the panel's revision-driven refresh.
    const auto notesBeforePending=actualNotes;
    desktop.liveUndo.beginNewTransaction("Notes pending input");
    tempoField->grabKeyboardFocus();
    notesField->insertTextAtCaret(" pending"); ++desktop.liveUiRevision; livePump();
    liveCheck(live::find(desktop.liveDocument->interfaceState,"items",id)["notes"].toString()=="Immediate note\nSecond line pending",
              "Notes survive pending async notification after blur");
    const auto beforeNotesFocus=live::canonical(desktop.liveDocument->interfaceState);
    notesField->grabKeyboardFocus(); livePump();
    liveCheck(live::canonical(desktop.liveDocument->interfaceState)==beforeNotesFocus,
              "Leaving unchanged Tempo preserves stored JSON and does not create a spurious edit");
    desktop.undoLiveInterface();
    // A late focus notification must not write the old view over the model
    // before the revision timer can display the undone value.
    notesField->onFocusLost();
    livePump();
    for(int retry=0; retry<8 && notesField->getText()!=notesBeforePending; ++retry) livePump();
    const auto undoneNotes=live::find(desktop.liveDocument->interfaceState,"items",id)["notes"].toString();
    if(undoneNotes!=notesBeforePending || notesField->getText()!=notesBeforePending)
        throw std::runtime_error(("Notes focused Undo mismatch: expected="+juce::JSON::toString(juce::var(notesBeforePending))+
            "; model="+juce::JSON::toString(juce::var(undoneNotes))+"; field="+
            juce::JSON::toString(juce::var(notesField->getText()))).toStdString());
    desktop.setLiveItemNotes(id,oldNotes); livePump();
    tempoField->grabKeyboardFocus(); livePump();
    const auto oldName = live::find(desktop.liveDocument->interfaceState, "items", id)["name"].toString();
    nameField->setText("Immediate title", true); livePump();
    liveCheck(live::find(desktop.liveDocument->interfaceState, "items", id)["name"].toString() == "Immediate title", "Item title commits on text change");
    desktop.undoLiveInterface(); livePump();
    liveCheck(live::find(desktop.liveDocument->interfaceState, "items", id)["name"].toString() == oldName, "Title Undo restores prior value");
    card.removeFromDesktop();
    const int shortNotesHeight = notesField->getHeight();
    const int shortPanelHeight = card.getHeight();
    juce::String longNotes;
    for (int line = 0; line < 18; ++line)
        longNotes += "A longer note paragraph with wrapping and detail, line " +
                     juce::String(line) + "\n";
    notesField->setText(longNotes, true);
    livePump();
    if (notesField->getHeight() <= shortNotesHeight || card.getHeight() <= shortPanelHeight)
        throw std::runtime_error(("Notes auto-grow: previous field/panel " + juce::String(shortNotesHeight) +
            "/" + juce::String(shortPanelHeight) + ", current " + juce::String(notesField->getHeight()) +
            "/" + juce::String(card.getHeight()) + ", laid-out text " +
            juce::String(notesField->getTextHeight())).toStdString());
    notesField->setText(oldNotes, true);
    livePump();
    if (notesField->getHeight() != shortNotesHeight || card.getHeight() != shortPanelHeight)
        throw std::runtime_error(("Notes auto-shrink: previous field/panel " + juce::String(shortNotesHeight) +
            "/" + juce::String(shortPanelHeight) + ", current " + juce::String(notesField->getHeight()) +
            "/" + juce::String(card.getHeight()) + ", laid-out text " +
            juce::String(notesField->getTextHeight())).toStdString());
    livePreview(card, "organizer");
    auto *mixerToggle = dynamic_cast<juce::TextButton *>(liveComponent(desktop, "live-mixer-toggle"));
    liveCheck(mixerToggle, "Mixer has a toolbar toggle");
    mixerToggle->onClick(); livePump();
    auto *mixer = liveComponent(desktop, "live-mixer");
    liveCheck(mixer && mixer->isVisible(), "Mixer drawer opens independently of item Browser");
    const auto grid = layout.bounds("song.timeline", desktop.getWidth(), desktop.getHeight());
    liveCheck(mixer->getX() == grid.getX() && mixer->getWidth() == grid.getWidth(),
              "Mixer dock starts at the timeline grid and spans its width");
    livePreview(desktop, "mixer");
    mixerToggle->onClick(); livePump();
    liveCheck(!mixer->isVisible(), "Repeated Mixer toggle closes drawer");
    auto *setlistMenu = dynamic_cast<juce::TextButton *>(liveComponent(desktop, "live-setlist-menu"));
    liveCheck(setlistMenu != nullptr, "Setlist provides an explicit menu anchor");
    const auto beforeMenus = desktop.liveDocument->signature();
    const auto checkMenu = [&](juce::Point<int> screen) {
        livePump();
        auto *menu = liveComponent(desktop, "live-setlist-context");
        liveCheck(menu != nullptr, "Setlist uses the owned context surface");
        auto *box = menu->findParentComponentOfClass<juce::CallOutBox>();
        const auto point = desktop.getLocalPoint(nullptr, screen);
        if (!box || box->getX() != point.x + 2 || box->getY() != point.y + 2) {
            livePreview(desktop, "setlist-anchor-failure");
            throw std::runtime_error(("Setlist menu anchor: expected " + point.translated(2, 2).toString() +
                "; actual " + (box ? box->getBounds().toString() : juce::String("missing box")) +
                "; horizontal scale " + juce::String(desktop.getTransform().mat00)).toStdString());
        }
        box->dismiss(); livePump();
    };
    setlistMenu->onClick(); checkMenu(setlistMenu->getScreenBounds().getBottomLeft());
    auto *setlistSurface = liveComponent(desktop, "live-setlist");
    const auto menuTime = juce::Time::getCurrentTime();
    for (const float scale : {1.f, .75f}) {
        desktop.setTransform(juce::AffineTransform::scale(scale));
        const juce::Point<float> point{32.f, 150.f};
        const juce::MouseEvent contextClick(juce::Desktop::getInstance().getMainMouseSource(), point,
            juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier), 1, 0, 0, 0, 0,
            setlistSurface, setlistSurface, menuTime, point, menuTime, 1, false);
        setlistSurface->mouseDown(contextClick); checkMenu(contextClick.getScreenPosition());
    }
    desktop.setTransform(juce::AffineTransform());
    liveCheck(desktop.liveDocument->signature() == beforeMenus, "Opening Setlist menus does not edit the show");

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
        liveDisplayPollingSmoke(fixture,waveFile);
        liveSelectionRefreshSmoke(fixture,waveFile);
        // #300: one Core graph contains a physical Live Input + WAV simultaneously.
        // Manual switches are stopped/quiescent; #301 owns real-time seamless handoff.
        {
            Desktop combined(true,false,true);
            combined.importLiveShow(fixture);
            combined.app.connect(std::make_unique<LivePlaybackTestDevice>(),
                                 {0,48000,128,{0},{0,1},1});
            const auto player=combined.liveDocument->interfaceState["channels"][1]["id"].toString();
            liveCheck(live::find(combined.liveDocument->interfaceState,"channels",player)["type"].toString()=="Player",
                      "Shared Live + WAV uses the physical Player channel");
            combined.startLiveAudioInput(0,player);
            const auto combinedAnchorId=*combined.liveInputTrack(player);
            mrs::NativeInsert combinedNativeGain;combinedNativeGain.id=mrs::new_id();
            combinedNativeGain.kind=mrs::InsertKind::gain;combinedNativeGain.gain=.5f;
            combined.app.set_inserts(combinedAnchorId,{combinedNativeGain});
            const auto combinedInsertGeneration=combined.app.insert_generation();
            const auto combinedEngine=combined.app.engine();
            const auto combinedFirstItem=combined.liveDocument->interfaceState["currentItem"].toString();
            const auto combinedSecondItem=combined.liveDocument->interfaceState["items"][1]["id"].toString();
            combined.attachLivePlayback(combinedSecondItem,waveFile);
            combined.selectLiveItem(combinedSecondItem);
            const auto combinedPreparationDeadline=juce::Time::getMillisecondCounter()+5000;
            while(combined.livePlaybackLoading && juce::Time::getMillisecondCounter()<combinedPreparationDeadline) {
                combined.app.poll();combined.syncLivePlayback();juce::Thread::sleep(1);
            }
            liveCheck(combined.livePlaybackReady() && combined.project()->tracks.size()==2 &&
                      combined.project()->tracks.back().id==combinedAnchorId &&
                      combined.project()->tracks.back().input_monitor &&
                      combined.project()->tracks.back().inserts==std::vector<mrs::NativeInsert>{combinedNativeGain} &&
                      combined.app.insert_generation()==combinedInsertGeneration && combined.app.engine()==combinedEngine &&
                      combined.app.playback_sources().size()==1,
                      "Manual WAV handoff keeps the same Core monitor, insert runtime and source mapping");
            combined.app.validate_playback_regions(
                live::playbackStemLayouts(combined.liveDocument->interfaceState,combinedSecondItem,48000));
            liveCheck(combined.transientLiveRuntime(),
                      "Combined Live Input and Playback remain transient .mrlive runtime, not a Studio song");
            const auto combinedSavedShow=folder.getChildFile("combined-input-wav.mrlive");
            combined.saveFile(combinedSavedShow);
            liveCheck(LiveDocument::read(combinedSavedShow).projects.empty(),
                      "Saving combined Live + WAV does not create Studio project references");
            combined.app.play();
            std::array<float,128> combinedGuitarSignal{},combinedSilentInput{};
            std::array<float,256> combinedPlaybackOutput{},combinedMonitoredOutput{};
            for(int combinedBlock=0;combinedBlock<4;++combinedBlock)
                combined.app.engine()->process(combinedSilentInput.data(),combinedPlaybackOutput.data(),128);
            combinedGuitarSignal.fill(.5f);
            for(int combinedBlock=0;combinedBlock<4;++combinedBlock)
                combined.app.engine()->process(combinedGuitarSignal.data(),combinedMonitoredOutput.data(),128);
            liveCheck(combinedMonitoredOutput[24]>combinedPlaybackOutput[24]+.02f,
                      "One active Core callback mixes WAV and monitored guitar through retained insert");
            bool combinedSwitchBlocked=false;
            try{combined.selectLiveItem(combinedFirstItem);}catch(const std::exception&){combinedSwitchBlocked=true;}
            liveCheck(combinedSwitchBlocked && combined.project()->tracks.back().id==combinedAnchorId,
                      "Active Playback cannot rehome a live insert graph while audio runs");
            combined.app.stop();
            combined.app.engine()->process(combinedSilentInput.data(),combinedMonitoredOutput.data(),128);
            combined.selectLiveItem(combinedFirstItem);
            liveCheck(combined.project()->tracks.size()==1 &&
                      combined.project()->tracks.front().id==combinedAnchorId &&
                      combined.project()->tracks.front().input_monitor &&
                      combined.app.insert_generation()==combinedInsertGeneration &&
                      combined.app.engine()==combinedEngine,
                      "Returning to empty item preserves live DSP and releases WAV-only sources");
            combined.releaseLiveInput();
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
        liveCheck(playbackDesktop.livePerformanceKey==live::performanceKey(playbackDesktop.liveDocument->interfaceState,id),
                  "Activation finishes performance plan synchronization before Play");
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
        playbackDesktop.syncLivePlayback();
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
        playbackDesktop.editLiveInterface("Stop returns to start fixture",[&](auto& state){live::put(live::performance(state),"backToStart",true);});
        playbackDesktop.syncLivePlayback();
        playbackDesktop.action(32);
        render();
        liveCheck(playbackDesktop.app.engine()->state().sample == 0, "Live Stop resets actual transport position");
        transportLine->update();
        liveCheck(transportLine->getX() == startX &&
                      std::abs(playbackTimeline->playheadBounds().getX() - startPosition) < .001f,
                  "Stop returns timeline playhead to item start");

        playbackDesktop.editLiveInterface("Zoom for clip editing", [&](auto &state) {
            live::put(live::performance(state), "stemsExpanded", true);
            auto &viewState = live::map(live::map(live::performance(state), "arrangement"), id);
            live::put(viewState, "scale", 128.);
        });
        playbackTimeline->refresh();
        auto *trimTimelineBody = liveComponent(*playbackTimeline, "live-timeline-items");
        const auto dragTime = juce::Time::getCurrentTime();
        const auto dragEvent = [&](float x) {
            const auto channel = live::playbackContainer(playbackDesktop.liveDocument->interfaceState);
            const juce::Point<float> point{x, static_cast<float>(playbackTimeline->playbackStemLaneBounds(channel).getY() + 20)};
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                                    juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
                                    trimTimelineBody, trimTimelineBody, dragTime, point, dragTime, 1, false);
        };
        const auto untrimmed = playbackDesktop.project();
        trimTimelineBody->mouseDown(dragEvent(1.f));
        trimTimelineBody->mouseDrag(dragEvent(33.f));
        liveCheck(playbackDesktop.project() == untrimmed, "Trim drag previews without publishing per-frame Core edits");
        trimTimelineBody->mouseUp(dragEvent(33.f));
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
            // One-second PCM exercises trim and natural audio extent above. Musical
            // navigation below needs an explicit show length covering Verse (bar 21.5).
            live::setItemView(state, id, "length", 86.);
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
        const auto beforeInspectionItem = playbackDesktop.liveDocument->interfaceState["currentItem"].toString();
        const auto beforeInspectionEntry = playbackDesktop.liveCurrentEntry;
        const auto beforeInspectionFocus = playbackDesktop.liveTimelineFocusRevision;
        const auto beforeInspectionProject = playbackDesktop.project();
        const auto beforeInspectionSample = playbackDesktop.app.engine()->state().sample;
        const auto beforeInspectionShow = playbackDesktop.liveDocument->signature();
        browserCard->mouseDown(click(juce::ModifierKeys::leftButtonModifier));
        browserCard->mouseUp(click(0));
        liveCheck(playbackDesktop.liveBrowserItem == id &&
                      playbackDesktop.liveDocument->interfaceState["currentItem"].toString() == beforeInspectionItem &&
                      playbackDesktop.liveCurrentEntry == beforeInspectionEntry &&
                      playbackDesktop.liveTimelineFocusRevision == beforeInspectionFocus &&
                      playbackDesktop.project() == beforeInspectionProject &&
                      playbackDesktop.app.engine()->state().sample == beforeInspectionSample &&
                      playbackDesktop.liveDocument->signature() == beforeInspectionShow,
                  "Released-button Browser click inspects item without activating show or moving transport");
        auto *inspectedPlayback = dynamic_cast<LivePlaybackPanel *>(
            liveComponent(playbackDesktop, "live-playback-panel"));
        liveCheck(inspectedPlayback != nullptr, "Browser inspection displays Playback/Stems");
        inspectedPlayback->sync();
        auto *listenButton = dynamic_cast<juce::TextButton *>(
            liveComponent(*inspectedPlayback, "live-browser-audition"));
        liveCheck(inspectedPlayback->stemCount() > 0 && listenButton && listenButton->isEnabled(),
                  "Browser inspection exposes WAV and independent Listen control");
        playbackTimeline->refresh();
        livePump();
        browserCard = liveComponent(playbackDesktop, "live-browser-" + id);
        liveCheck(browserCard != nullptr, "Inspected Browser row survives refresh");
        for (auto *child : playbackTimeline->getChildren())
            if (auto *scroll = dynamic_cast<juce::Viewport *>(child))
                scroll->setViewPosition(1000, 0);
        browserCard->mouseDown(click(juce::ModifierKeys::leftButtonModifier));
        browserCard->mouseUp(click(0));
        playbackTimeline->refresh();
        liveCheck(playbackDesktop.liveBrowserItem == id &&
                      playbackDesktop.liveDocument->interfaceState["currentItem"].toString() == beforeInspectionItem &&
                      playbackDesktop.liveCurrentEntry == beforeInspectionEntry &&
                      playbackDesktop.liveTimelineFocusRevision == beforeInspectionFocus &&
                      playbackDesktop.project() == beforeInspectionProject &&
                      playbackDesktop.app.engine()->state().sample == beforeInspectionSample &&
                      playbackDesktop.liveDocument->signature() == beforeInspectionShow,
                  "Reclicking Browser item leaves show selection, Core graph and timeline focus unchanged");
        playbackDesktop.editLiveInterface("Library-only view",
                                          [](auto &state) { live::activeEntries(state) = juce::Array<juce::var>(); });
        playbackTimeline->refresh();
        liveCheck(playbackTimeline->itemCount() == 0 &&
                      live::activeEntries(playbackDesktop.liveDocument->interfaceState).size() == 0,
                  "Browser item outside active setlist is absent from timeline");
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
    {
        // Opposite-phase PCM proves shared-sample alignment and independent physical routing.
        const auto positive = folder.getChildFile("live-multi-tone.wav"),
                   negative = folder.getChildFile("live-inverted.wav");
        for (const auto &target : {positive, negative}) {
            liveCheck(folder.getChildFile("live-tone.wav").copyFileTo(target), "Prepare synthetic stem header");
            std::fstream wave(std::filesystem::path(target.getFullPathName().toWideCharPointer()),
                              std::ios::in | std::ios::out | std::ios::binary);
            wave.seekp(44);
            for (int frame = 0; frame < 48000; ++frame) {
                const int value = (4096 + (frame * 137) % 4096) * (target == positive ? 1 : -1);
                const auto bits = static_cast<unsigned short>(value);
                for (int channel = 0; channel < 2; ++channel) {
                    wave.put(static_cast<char>(bits & 255));
                    wave.put(static_cast<char>((bits >> 8) & 255));
                }
            }
        }
        Desktop multi(true, false, true);
        multi.importLiveShow(fixture);
        multi.selectLiveItem(id);
        multi.addLivePlaybackStems(id, {positive, negative});
        const auto waitMulti = [&] {
            const auto until = juce::Time::getMillisecondCounter() + 5000;
            while (multi.livePlaybackLoading && juce::Time::getMillisecondCounter() < until) {
                multi.app.poll();
                multi.syncLivePlayback();
                juce::Thread::sleep(1);
            }
        };
        waitMulti();
        liveCheck(multi.livePlaybackReady() && multi.project()->tracks.size() == 2 &&
                      multi.project()->clips.size() == 2 && multi.project()->clips[0].start == 0 &&
                      multi.project()->clips[1].start == 0,
                  "Batch stem import publishes parallel Core clips at the same sample");
        const auto rows = live::playbackRows(multi.liveDocument->interfaceState, id);
        const auto stemKey = rows[1]["id"].toString();
        liveCheck(live::find(multi.liveDocument->interfaceState, "channels", stemKey)["type"].toString() == "Playback",
                  "Additional stem uses the shared show channel layout");
        LiveMixer aggregateMixer(multi); aggregateMixer.setSize(800,300); aggregateMixer.sync();
        liveCheck(liveComponent(aggregateMixer,"live-mixer-gain-"+live::playbackContainer(multi.liveDocument->interfaceState)) &&
                      !liveComponent(aggregateMixer,"live-mixer-gain-"+stemKey),
                  "Mixer exposes aggregate Playback without separate stem strips");
        multi.app.connect(std::make_unique<LivePlaybackTestDevice>(4), {0, 48000, 128, {0, 1}, {0, 1, 2, 3}, 1});
        std::array<float, 512> audio{};
        const auto renderMulti = [&] {
            for (int i = 0; i < 4; ++i)
                multi.app.engine()->process(nullptr, audio.data(), 128);
        };
        multi.app.play();
        renderMulti();
        liveCheck(std::all_of(audio.begin(), audio.end(), [](float value) { return std::abs(value) < .0001f; }),
                  "Opposite-phase stems cancel sample-locked through shared Core mixer");
        const auto aggregateChannel=live::playbackContainer(multi.liveDocument->interfaceState);
        multi.setLiveChannelMix(aggregateChannel,-6.,0.,false,false);renderMulti();
        liveCheck(std::abs(multi.project()->tracks[0].mix.gain-multi.project()->tracks[1].mix.gain)<.00001f &&
                      std::abs(audio[400])<.0001f,"Playback channel fader changes all musical stems while preserving their balance");
        multi.undoLiveInterface();multi.syncLivePlayback();renderMulti();
        multi.livePlaybackStem = stemKey;
        multi.setLivePlaybackMix(0., true, false);
        renderMulti();
        liveCheck(audio[400] > .03f && audio[401] > .03f, "Muting one stem leaves the other audible");
        multi.undoLiveInterface();
        multi.syncLivePlayback();
        renderMulti();
        liveCheck(std::abs(audio[400]) < .0001f, "Show Undo restores both synchronized stems");
        multi.app.seek(1234);
        renderMulti();
        liveCheck(std::all_of(audio.begin(), audio.end(), [](float value) { return std::abs(value) < .0001f; }) &&
                      multi.app.engine()->state().sample == 1746,
                  "Shared seek moves every stem to the same sample");
        multi.setLivePlaybackMix(0., false, true);
        renderMulti();
        liveCheck(audio[400] < -.03f, "Stem Solo excludes the other shared Core channel");
        multi.app.stop();
        renderMulti();
        multi.setLivePlaybackMix(0., false, false);
        multi.livePlaybackStem.clear();
        multi.setLivePlaybackOutputs({0, 1});
        multi.livePlaybackStem = stemKey;
        multi.setLivePlaybackOutputs({2, 3});
        multi.app.play();
        renderMulti();
        liveCheck(audio[400] > .03f && audio[401] > .03f && audio[402] < -.03f && audio[403] < -.03f,
                  "Physical output pairs carry separate stems without leaking to Master");
        const auto beforeRoute = multi.liveDocument->signature();
        bool routeBlocked = false;
        try {
            multi.setLivePlaybackOutputs({0});
        } catch (const std::exception &) {
            routeBlocked = true;
        }
        liveCheck(routeBlocked && multi.liveDocument->signature() == beforeRoute,
                  "Playing routing changes preserve show atomically");
        multi.app.stop();
        renderMulti();
        routeBlocked = false;
        try {
            multi.setLivePlaybackOutputs({9});
        } catch (const std::exception &) {
            routeBlocked = true;
        }
        liveCheck(routeBlocked && multi.liveDocument->signature() == beforeRoute,
                  "Inactive output is rejected before state publication");
        multi.setLiveCue(true);
        auto liveClickSettings = live::liveClick(multi.liveDocument->interfaceState);
        multi.setLiveAudio(-6.020599913, liveClickSettings);
        multi.app.play();
        renderMulti();
        liveCheck(std::abs(audio[400] + audio[402] * .5f) < .0001f && audio[400] > .01f,
                  "Global Playback gain affects direct music outputs but leaves Cue at its own level");
        multi.undoLiveInterface();
        multi.syncLivePlayback();
        renderMulti();
        liveCheck(std::abs(audio[400] + audio[402]) < .0001f,
                  "Show Undo restores global Playback gain without changing Cue");
        const auto clickStopPosition=multi.app.engine()->state().sample;
        multi.app.stop();
        renderMulti();
        liveCheck(multi.app.engine()->state().sample==clickStopPosition,
                  "Stop holds the current position when Back to start is disabled");
        liveClickSettings.playback = true;
        liveClickSettings.output_left = 2;
        liveClickSettings.output_right = 3;
        multi.setLiveAudio(-6.020599913, liveClickSettings);
        multi.livePlaybackStem.clear();
        multi.setLivePlaybackMix(0., true, false);
        multi.livePlaybackStem = stemKey;
        multi.setLivePlaybackMix(0., true, false);
        // Route testing needs a known beat, independently of the Stop preference.
        multi.app.seek(0);
        multi.app.play();
        renderMulti();
        float clickPeak = 0.f;
        for (std::size_t frame = 0; frame < 128; ++frame) {
            liveCheck(std::abs(audio[frame * 4]) < .0001f && std::abs(audio[frame * 4 + 1]) < .0001f &&
                          audio[frame * 4 + 2] == audio[frame * 4 + 3],
                      "Routed Click stays out of Master and follows the same stereo pair");
            clickPeak = (std::max)(clickPeak, std::abs(audio[frame * 4 + 2]));
        }
        liveCheck(clickPeak > .001f, "Shared Core metronome reaches the dedicated Live Click outputs");
        auto reroutedClick = liveClickSettings;
        reroutedClick.output_left = 0;
        reroutedClick.output_right = 1;
        const auto beforeClickRoute = multi.liveDocument->signature();
        bool clickRouteBlocked = false;
        try {
            multi.setLiveAudio(0., reroutedClick);
        } catch (const std::exception &) {
            clickRouteBlocked = true;
        }
        liveCheck(clickRouteBlocked && multi.liveDocument->signature() == beforeClickRoute,
                  "Click route changes during Playback fail before show mutation");
        multi.app.stop();
        renderMulti();
        reroutedClick.output_left = 9;
        clickRouteBlocked = false;
        try {
            multi.setLiveAudio(0., reroutedClick);
        } catch (const std::exception &) {
            clickRouteBlocked = true;
        }
        liveCheck(clickRouteBlocked && multi.liveDocument->signature() == beforeClickRoute,
                  "Unavailable Click route is rejected before show publication");
        liveClickSettings.playback = false;
        // Keep Click on Master for the existing offline two-output reload fixture below.
        liveClickSettings.output_left = liveClickSettings.output_right = -1;
        multi.setLiveAudio(-6.020599913, liveClickSettings);
        multi.livePlaybackStem.clear();
        multi.setLivePlaybackMix(0., false, false);
        multi.livePlaybackStem = stemKey;
        multi.setLivePlaybackMix(0., false, false);
        multi.setLivePlaybackTrim(.02, .08);
        liveCheck(multi.project()->clips[0].length == 48000 && multi.project()->clips[1].source_offset == 960 &&
                      multi.project()->clips[1].length == 2880,
                  "Stem trim edits only its own source window");
        // Item group, stem and persistent channel levels remain independent.
        const auto musicChannel = live::playbackChannelId(multi.liveDocument->interfaceState, rows[0]);
        multi.editLiveInterface("Baseline item", [&](auto &state) { live::saveItem(state, id); });
        multi.setLiveChannelMix(musicChannel, -3., .25, false, false);
        liveCheck(!live::itemDirty(multi.liveDocument->interfaceState, id), "Channel mix does not dirty saved item content");
        multi.setLiveItemPlaybackGain(-6.);
        const auto expectedGain = std::pow(10., (-6.020599913 - 6. - 3.) / 20.);
        liveCheck(std::abs(multi.project()->tracks[0].mix.gain - expectedGain) < .00001 &&
                      std::abs(multi.project()->tracks[0].mix.pan - .25f) < .00001f,
                  "Core receives stem + item group + persistent channel gain and pan");
        liveCheck(live::itemDirty(multi.liveDocument->interfaceState, id), "Item Playback gain dirties item content");
        liveCheck(std::abs(multi.project()->tracks[1].mix.gain - 1.f) < .00001f,
                  "Cue remains independent of item Playback group gain");
        multi.editLiveInterface("Another item Playback level", [&](auto &state) {
            live::put(live::require(state, "items", second), "playbackGroupGain", -12.);
        });
        liveCheck(live::itemPlaybackGain(multi.liveDocument->interfaceState, second) == -12. &&
                      live::itemPlaybackGain(multi.liveDocument->interfaceState, id) == -6., "Each item stores its own aggregate Playback level");
        multi.editLiveInterface("Fold stems", [](auto &state) { live::put(live::performance(state), "stemsExpanded", false); });
        multi.syncLivePlayback();
        liveCheck(multi.project()->clips.size() == 2 && std::abs(multi.project()->tracks[0].mix.gain - expectedGain) < .00001,
                  "Folding stems preserves Core clips and mix");
        multi.editLiveInterface("Expand stems", [](auto &state) { live::put(live::performance(state), "stemsExpanded", true); });
        // Preview uses existing Core publication without modifying show/history.
        const auto beforePreview = multi.liveDocument->signature();
        multi.previewLiveChannelMix(musicChannel, -9., -.5, false, false);
        liveCheck(multi.liveDocument->signature() == beforePreview, "Mixer preview leaves persistent values unchanged");
        multi.app.cancel_mix_preview();
        multi.editLiveInterface("Save stem version", [&](auto &state) { live::saveItem(state, id); });
        const auto show = folder.getChildFile("stems.mrlive");
        multi.saveFile(show);
        const auto stemSaved = LiveDocument::read(show);
        const auto savedRows = live::playbackRows(stemSaved.interfaceState, id);
        liveCheck(savedRows.size() == 2 && !live::itemDirty(stemSaved.interfaceState, id) &&
                      live::itemPlaybackGain(stemSaved.interfaceState, id) == -6. &&
                      live::itemPlaybackGain(stemSaved.interfaceState, second) == -12. &&
                      static_cast<double>(live::find(stemSaved.interfaceState, "channels", musicChannel)["gain"]) == -3. &&
                      savedRows[1]["file"].toString() == negative.getFullPathName() &&
                      static_cast<bool>(savedRows[1]["cue"]) &&
                      std::abs(live::playbackGain(stemSaved.interfaceState) + 6.020599913) < .000001 &&
                      live::playbackOutputs(savedRows[1]) == std::vector<int>({2, 3}),
                  "All stem paths, trims and routes survive mrlive round-trip");
        const auto relocated = folder.getChildFile("stems-save-as").getChildFile("show.mrlive");
        stemSaved.write(relocated);
        const auto moved = LiveDocument::read(relocated);
        liveCheck(live::playbackRows(moved.interfaceState, id)[1]["file"].toString() == negative.getFullPathName() &&
                      !live::itemDirty(moved.interfaceState, id),
                  "Save as rebases additional stem references");
        multi.removeLivePlaybackStem();
        waitMulti();
        liveCheck(multi.livePlaybackReady() && multi.project()->clips.size() == 1,
                  "Removing a stem reloads the remaining batch safely");
        multi.undoLiveInterface();
        multi.syncLivePlayback();
        waitMulti();
        liveCheck(multi.livePlaybackReady() && multi.app.active_outputs() == std::vector<int>({0, 1, 2, 3}),
                  "Stem reload and Undo preserve the same active physical output selection");
        // Explicit device change removes outputs 3/4; saved routing must still fail closed.
        multi.app.disconnect();
        multi.app.new_project();
        multi.app.connect(std::make_unique<LivePlaybackTestDevice>(), {0, 48000, 128, {}, {0, 1}, 1});
        multi.syncLivePlayback();
        waitMulti();
        liveCheck(!multi.livePlaybackReady() && multi.livePlaybackError.isNotEmpty(),
                  "Unavailable saved physical route blocks Playback rather than leaking to Master");
        multi.livePlaybackStem = stemKey;
        multi.setLivePlaybackOutputs({});
        liveCheck(multi.livePlaybackReady(), "Unavailable route can be repaired to Master without discarding media");
        multi.refresh();
        livePump(); // The sidebar synchronizes on its UI timer.
        auto *playbackSidebar = dynamic_cast<LivePlaybackPanel *>(liveComponent(multi, "live-playback-panel"));
        liveCheck(playbackSidebar && playbackSidebar->stemCount() == 2, "Playback sidebar shows all real stems simultaneously");
        auto *firstLevel = dynamic_cast<juce::Slider *>(liveComponent(*playbackSidebar, "live-stem-level-" + id + ":primary"));
        liveCheck(firstLevel != nullptr, "Each stem has its own compact horizontal fader");
        const auto selectedStem = multi.livePlaybackStem;
        firstLevel->setValue(-4., juce::sendNotificationSync);
        liveCheck(static_cast<double>(live::playback(multi.liveDocument->interfaceState, id)["gain"]) == -4. &&
                      multi.livePlaybackStem == selectedStem, "Sidebar fader edits its own stem without changing selection");
        multi.undoLiveInterface(); multi.syncLivePlayback();
        const auto sharedEngine = multi.app.engine();
        multi.attachLivePlayback(second, positive);
        const auto activeBeforePreload = multi.app.services().projects;
        const auto savedBeforePreload = multi.liveDocument->signature();
        multi.app.play();
        renderMulti();
        const auto preloadDeadline = juce::Time::getMillisecondCounter() + 5000;
        while (!multi.liveNextStatus.contains("audio ready") && juce::Time::getMillisecondCounter() < preloadDeadline) {
            multi.app.poll();
            multi.syncLivePlayback();
            renderMulti();
            juce::Thread::sleep(1);
        }
        liveCheck(multi.liveNextItem == second && multi.liveNextStatus.contains("audio ready") &&
                      multi.app.services().projects == activeBeforePreload &&
                      multi.liveDocument->signature() == savedBeforePreload &&
                      multi.app.engine()->state().playback == mrs::PlaybackState::playing,
                  "Next-item preload follows setlist order without publishing a project or editing the show");
        multi.stepLiveItem(1);
        liveCheck(multi.liveDocument->interfaceState["currentItem"].toString() == second &&
                      !multi.livePlaybackLoading && multi.livePlaybackReady() && multi.project()->clips.size() == 1 &&
                      multi.app.engine() == sharedEngine && multi.app.engine()->state().sample == 0,
                  "A ready next item switches synchronously through the same Core without reimport");
        liveCheck(std::abs(multi.project()->tracks[0].mix.gain - std::pow(10., (-6.020599913 - 12. - 3.) / 20.)) < .00001,
                  "Switching item restores its group gain while keeping the channel fader");
        multi.stepLiveItem(-1);
        liveCheck(std::abs(multi.project()->tracks[0].mix.gain - expectedGain) < .00001,
                  "Returning to previous item restores its independent Playback group");
        liveCheck(multi.livePlaybackReady() && multi.project()->clips.size() == 2 &&
                      multi.liveDocument->interfaceState["currentItem"].toString() == id,
                  "Warm previous-item selection restores its stems/trim/routes");
        const auto beforeAutomatic=multi.liveDocument->interfaceState.clone();
        multi.editLiveInterface("Automatic boundary fixture",[&](auto& state){
            live::put(live::map(live::map(live::performance(state),"arrangement"),id),"length",1.);
            live::setPerformanceMode(state,id,id,mrs::PerformanceMode::normal);
            live::setPerformanceMode(state,second,second,mrs::PerformanceMode::stop);
        });multi.syncLivePlayback();multi.app.play();renderMulti();multi.app.poll();multi.syncLivePlayback();
        const auto automaticToken=multi.liveAutoToken;const auto automaticEntry=multi.liveAutoEntry;
        liveCheck(automaticToken!=0,"Next native graph is staged before the current item end");
        for(int block=0;block<2000&&multi.app.engine()->performance_status().token!=automaticToken;++block)
            multi.app.engine()->process(nullptr,audio.data(),128);
        multi.app.poll();multi.syncLivePlayback();
        liveCheck(multi.liveDocument->interfaceState["currentItem"].toString()==second&&multi.liveCurrentEntry==automaticEntry&&
                  multi.livePlaybackItem==second&&multi.livePlaybackReady()&&multi.app.engine()==sharedEngine&&
                  multi.app.engine()->state().playback==mrs::PlaybackState::playing,
                  "Native UI follows the Core boundary and entry identity without manual activation");
        liveCheck(!multi.songDirty(),"Automatic runtime projection does not trigger a Studio song-save guard");
        multi.app.stop();renderMulti();multi.stepLiveItem(-1);
        multi.editLiveInterface("Advance and wait fixture",[&](auto& state){live::setPerformanceMode(state,id,id,mrs::PerformanceMode::stop);});
        multi.syncLivePlayback();const auto waitingToken=multi.liveAutoToken;
        multi.app.play();
        for(int block=0;block<2000&&multi.app.engine()->performance_status().token!=waitingToken;++block)
            multi.app.engine()->process(nullptr,audio.data(),128);
        multi.app.poll();multi.syncLivePlayback();
        liveCheck(waitingToken&&multi.liveDocument->interfaceState["currentItem"].toString()==second&&
                  multi.app.engine()->state().sample==0&&multi.app.engine()->state().playback==mrs::PlaybackState::stopped,
                  "Item play-to-end advances the native playhead to its successor and waits for Play");
        multi.stepLiveItem(-1);multi.applyLiveInterface(beforeAutomatic);multi.syncLivePlayback();
        const auto third = multi.liveDocument->interfaceState["items"][2]["id"].toString();
        const auto broken = folder.getChildFile("preload-invalid.wav");
        broken.replaceWithText("invalid WAV");
        multi.attachLivePlayback(third, broken);
        const auto beforeFailedSwitch = multi.app.services().projects;
        multi.app.play();
        renderMulti();
        multi.selectLiveItem(third);
        waitMulti();
        liveCheck(multi.liveDocument->interfaceState["currentItem"].toString() == id &&
                      multi.app.services().projects == beforeFailedSwitch &&
                      multi.app.engine()->state().playback == mrs::PlaybackState::playing &&
                      multi.livePlaybackReady() && multi.livePreloadError.isNotEmpty(),
                  "Failed cold selection keeps the current item/audio and reports its preparation error");
        multi.editLiveInterface("Inactive trim before relink",[&](auto& state){auto row=live::playback(state,third);live::put(row,"trimStart",.01);live::put(row,"trimEnd",.08);});
        const auto beforeRepair=multi.liveDocument->interfaceState.clone();
        const auto repairProject=multi.app.services().projects;const auto repairToken=multi.app.playback_token();
        {
            LivePreflightPanel inspection(multi);
            const auto deadline=juce::Time::getMillisecondCounter()+5000;
            while(inspection.busy()&&juce::Time::getMillisecondCounter()<deadline){inspection.pollInspection();juce::Thread::sleep(1);}
            liveCheck(!inspection.busy()&&inspection.sourceCount()>=2&&multi.app.services().projects==repairProject,
                      "Show source inspection checks inactive media without replacing the active Core project");
        }
        const auto pollRepairs=[&]{const auto deadline=juce::Time::getMillisecondCounter()+5000;
            while(std::any_of(multi.livePreflightJobs.begin(),multi.livePreflightJobs.end(),[](const auto& job){return static_cast<bool>(job->completion);})&&juce::Time::getMillisecondCounter()<deadline){
                multi.pollLivePreflightJobs();renderMulti();juce::Thread::sleep(1);}
        };
        queueLiveRelink(multi,third,{},broken);pollRepairs();
        liveCheck(live::canonical(multi.liveDocument->interfaceState)==live::canonical(beforeRepair),
                  "Corrupt replacement is rejected before show bindings/history are changed");
        queueLiveRelink(multi,third,{},positive);pollRepairs();
        liveCheck(live::playback(multi.liveDocument->interfaceState,third)["file"].toString()==positive.getFullPathName()&&
                  multi.app.services().projects==repairProject&&multi.app.playback_token()==repairToken&&
                  multi.app.engine()->state().playback==mrs::PlaybackState::playing&&
                  static_cast<double>(live::playback(multi.liveDocument->interfaceState,third)["trimStart"])==.01&&
                  static_cast<double>(live::playback(multi.liveDocument->interfaceState,third)["trimEnd"])==.08,
                  "Inactive source relink preserves the active item, shared engine and running playback");
        multi.editLiveInterface("Unavailable inactive output fixture",[&](auto& state){auto row=live::playback(state,third);live::put(row,"outputs",juce::Array<juce::var>{99});});
        multi.repairLivePlaybackOutputs(third,{});
        liveCheck(live::playbackOutputs(live::playback(multi.liveDocument->interfaceState,third)).empty()&&multi.app.services().projects==repairProject,
                  "Inactive output can be explicitly repaired to Master without switching items");
        multi.undoLiveInterface();multi.syncLivePlayback();
        liveCheck(live::playbackOutputs(live::playback(multi.liveDocument->interfaceState,third))==std::vector<int>{99},
                  "Output repair remains a show Undo action");
        multi.applyLiveInterface(beforeRepair);multi.syncLivePlayback();
        multi.editLiveInterface("Repeat item entry", [&](auto &state) {
            auto repeated = live::object();
            live::put(repeated, "id", "repeat-alpha");
            live::put(repeated, "itemId", id);
            live::activeEntries(state).getArray()->insert(4, repeated);
        });
        multi.selectLiveItem(id);
        multi.stepLiveItem(1);
        multi.stepLiveItem(1);
        liveCheck(multi.liveCurrentEntry == "repeat-alpha" &&
                      multi.liveDocument->interfaceState["currentItem"].toString() == id &&
                      live::neighborItem(multi.liveDocument->interfaceState, 1, multi.liveCurrentEntry) == third,
                  "Repeated item references advance by entry identity rather than returning to the first occurrence");
        multi.stepLiveItem(-1);
        liveCheck(multi.liveDocument->interfaceState["currentItem"].toString() == second,
                  "Previous item across a repeated reference follows the actual setlist order");
    }
    {
        Desktop clickOnly(true, false, true);
        clickOnly.importLiveShow(fixture);
        clickOnly.selectLiveItem(id);
        clickOnly.app.connect(std::make_unique<LivePlaybackTestDevice>(4), {0, 48000, 128, {}, {1, 3}, 1});
        mrs::audio::ClickSettings clickOnlySettings;
        clickOnlySettings.playback = true;
        clickOnlySettings.output_left = 3;
        clickOnly.setLiveAudio(0., clickOnlySettings);
        liveCheck(clickOnly.liveTransportReady() && !clickOnly.livePlaybackReady(),
                  "Click-only item can run the shared Transport without a WAV");
        clickOnly.action(30);
        std::array<float, 256> clickOnlyAudio{};
        clickOnly.app.engine()->process(nullptr, clickOnlyAudio.data(), 128);
        float mappedPeak = 0.f;
        for (std::size_t frame = 0; frame < 128; ++frame) {
            liveCheck(clickOnlyAudio[frame * 2] == 0.f,
                      "Physical Click output does not leak into another stream channel");
            mappedPeak = (std::max)(mappedPeak, std::abs(clickOnlyAudio[frame * 2 + 1]));
        }
        liveCheck(mappedPeak > .001f, "Physical output 4 maps to stream channel 2 with nonconsecutive active outputs");
        clickOnly.app.stop();
        clickOnly.app.engine()->process(nullptr, clickOnlyAudio.data(), 128);
        const auto clickShow = folder.getChildFile("click.mrlive");
        clickOnly.saveFile(clickShow);
        const auto reopenedClick = LiveDocument::read(clickShow);
        liveCheck(live::liveClick(reopenedClick.interfaceState) == clickOnlySettings,
                  "Click enabled/accent/level/physical route survive mrlive round-trip");
        // A show requesting a missing Click output must not silently fall back to Master.
        clickOnly.app.disconnect();
        clickOnly.app.set_click_settings({});
        clickOnly.app.connect(std::make_unique<LivePlaybackTestDevice>(), {0, 48000, 128, {}, {0, 1}, 1});
        clickOnly.syncLivePlayback();
        liveCheck(!clickOnly.liveTransportReady() && clickOnly.livePlaybackError.isNotEmpty() &&
                      !clickOnly.app.click_settings().playback,
                  "Unavailable saved Click route stops Playback and disables the click fail-closed");
        clickOnlySettings.output_left = -1;
        clickOnly.setLiveAudio(0., clickOnlySettings);
        liveCheck(clickOnly.liveTransportReady(), "Missing Click output can be explicitly repaired to Master");
        LiveAudioPanel audioSettings(clickOnly);
        liveCheck(liveComponent(audioSettings, "live-audio-output") != nullptr,
                  "Live context exposes Click route settings through the native controls");
        {
            Desktop clipHost(true,false,true);clipHost.importLiveShow(fixture);clipHost.app.disconnect();clipHost.selectLiveItem(id);
            clipHost.editLiveInterface("Clip edit fixture",[&](auto& state){
                live::put(live::require(state,"items",id),"tempo",240.);
                live::setItemView(state,id,"scale",128.);live::setItemView(state,id,"lead",2.);live::setItemView(state,id,"length",8.);
            });
            clipHost.attachLivePlayback(id,folder.getChildFile("live-tone.wav"));
            const auto readyUntil=juce::Time::getMillisecondCounter()+5000;
            while(juce::Time::getMillisecondCounter()<readyUntil){
                clipHost.app.poll();clipHost.syncLivePlayback();clipHost.refresh();
                if(clipHost.livePlaybackReady() && clipHost.app.clip_waveform(clipHost.app.playback_sources()[0]))break;
                juce::Thread::sleep(1);
            }
            liveCheck(clipHost.livePlaybackReady(),"Clip authoring has the prepared shared source");
            auto* clipTimeline=dynamic_cast<LiveTimeline*>(liveComponent(clipHost,"live-show-timeline"));
            liveCheck(clipTimeline!=nullptr,"Clip authoring timeline exists");clipTimeline->refresh();
            auto* clipBody=liveComponent(*clipTimeline,"live-timeline-items");
            const auto clipBounds=clipTimeline->itemBounds(id);
            const auto channel=live::playbackContainer(clipHost.liveDocument->interfaceState);
            const float clipOrigin=static_cast<float>(clipBounds.getX()+1+256);
            const float clipY=static_cast<float>(clipTimeline->playbackStemLaneBounds(channel).getY()+20);
            const auto mouse=[&](float x){const auto now=juce::Time::getCurrentTime();const juce::Point<float> point{x,clipY};return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,clipBody,clipBody,now,point,now,1,false);};
            clipTimeline->setClipScissors(true);clipBody->mouseDown(mouse(clipOrigin+64.f));clipTimeline->refresh();
            liveCheck(clipHost.project()->clips.size()==2 && clipHost.project()->clips[1].source_offset==24000 && clipTimeline->clipScissors(),
                      "Native Scissors splits actual Core regions and stays selected");
            clipTimeline->setClipScissors(false);
            clipBody->mouseDown(mouse(clipOrigin+96.f));clipBody->mouseDrag(mouse(clipOrigin+160.f));
            liveCheck(clipHost.project()->clips[1].start==24000,"Clip drag remains UI preview until release");
            clipBody->mouseUp(mouse(clipOrigin+160.f));clipTimeline->refresh();
            liveCheck(clipHost.project()->clips[1].start==48000 && clipHost.project()->clips[1].source_offset==24000,
                      "Native Pointer moves cut region without source slip");
            clipHost.undoLiveInterface();clipHost.syncLivePlayback();clipTimeline->refresh();
            liveCheck(clipHost.project()->clips[1].start==24000,"One show Undo reverses the whole region drag");
            const auto clipShow=folder.getChildFile("clip-edits.mrlive");clipHost.saveFile(clipShow);
            const auto savedClips=LiveDocument::read(clipShow);
            liveCheck(live::playbackRegions(savedClips.interfaceState,id,live::playback(savedClips.interfaceState,id)).size()==2,
                      "Native split arrangement survives mrlive Save/Open");
        }
        LiveWaveformCache displayCache;
        const auto source = folder.getChildFile("live-tone.wav").getFullPathName();
        const auto waitWave = [&] {
            const auto until = juce::Time::getMillisecondCounter() + 5000;
            while (juce::Time::getMillisecondCounter() < until) {
                displayCache.sync({source});
                const auto *entry = displayCache.find(source);
                if (entry && entry->wave)
                    return true;
                juce::Thread::sleep(5);
            }
            return false;
        };
        liveCheck(waitWave(), "Display-only cache prepares actual WAV peaks without activating an item");
        const auto *cached = displayCache.find(source);
        liveCheck(cached && cached->rate == 48000 && cached->wave->frames() == 48000,
                  "Inactive waveform retains original sample rate and duration");
        displayCache.sync({});
        liveCheck(displayCache.find(source) && displayCache.find(source)->wave,
                  "Waveform survives item deactivation / leaving the visible timeline");
    }
    desktop.home.reset();
    liveCheck(folder.deleteRecursively(), "Native interface fixture cleanup");
}
} // namespace ui
