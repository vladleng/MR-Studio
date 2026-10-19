#include "Desktop.h"
#include "ReverbKit.h"
#include "ReverbBrowser.h"
#include "PluginPreset.h"
#include <mrs/recording.hpp>
#include <chrono>
#include <thread>
#include <windows.h>

namespace ui {
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::filesystem::path path(const juce::File& f) { return std::filesystem::path(f.getFullPathName().toWideCharPointer()); }
void pump(Desktop& desktop) {
    MSG msg{};
    for (int i = 0; i < 200 && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE); ++i) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    desktop.app.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
template<class Predicate> void wait(Desktop& desktop, Predicate predicate, const char* message) {
    for (int i = 0; i < 3000; ++i) { pump(desktop); if (predicate()) return; }
    throw std::runtime_error(message);
}
juce::String id(const mrs::Id& value) { return juce::String::fromUTF8(value.value.c_str()); }
void image(juce::Component& component, const juce::String& name, float scale) {
    const auto snapshot = component.createComponentSnapshot(component.getLocalBounds(), true, scale, juce::SoftwareImageType{});
    auto output = juce::File::getCurrentWorkingDirectory().getChildFile(name + ".png").createOutputStream();
    check(output != nullptr, "Pilot snapshot stream");
    output->setPosition(0); output->truncate();
    check(juce::PNGImageFormat().writeImageToStream(snapshot, *output), "Pilot snapshot write");
}
struct Target { mrs::Id track, insert; juce::File folder; };
Target fixture(Desktop& desktop, const juce::File& parent) {
    desktop.closeEditors();
    desktop.app.new_project(48000, "MR Reverb Kit / private fixture");
    desktop.resetDevice();
    const auto track = desktop.app.add_audio_track("Reverb pilot");
    mrs::NativeInsert effect; effect.id = mrs::new_id(); effect.kind = mrs::InsertKind::reverb;
    desktop.applyChain(track, {effect});
    desktop.selectedTrack = track;
    desktop.refresh(true);
    const auto folder = parent.getChildFile("mrs-ui195-" + id(mrs::new_id()));
    check(!folder.exists() && folder.createDirectory().wasOk(), "Fresh private pilot directory");
    const auto hall = folder.getChildFile("Halls").getChildFile(juce::String::fromUTF8("Зал.wav"));
    check(hall.getParentDirectory().createDirectory().wasOk(), "Pilot fixture subdirectory");
    std::array<float, 256> impulse{}; impulse[0] = .5f; impulse[1] = .25f;
    { mrs::audio::Recorder writer(path(hall), 48000, 0, {0, 1}); writer.capture(impulse.data(), 2, 128, 0); writer.finish(); }
    desktop.openInsert(track, effect.id);
    auto* browser = dynamic_cast<ReverbBrowser*>(desktop.windows.back()->getContentComponent()->findChildWithID("reverb-browser"));
    check(browser != nullptr, "Native browser in pilot");
    browser->chooseRoot(folder);
    wait(desktop, [&] { return browser->visibleRows() == 1; }, "Pilot synthetic IR scan");
    browser->activate(0);
    wait(desktop, [&] { return !desktop.chain(track).front().reverb.first.digest.empty(); }, "Pilot IR reaches model");
    wait(desktop, [&] { return desktop.app.reverb_loader(track, effect.id)->snapshot()->phase == mrs::audio::ReverbLoadPhase::ready; }, "Pilot IR Ready");
    return {track, effect.id, folder};
}
kit::MRKnob& knob(ReverbKitControls& controls, int parameter) {
    auto* result = dynamic_cast<kit::MRKnob*>(controls.findChildWithID("kit-reverb-" + juce::String(parameter)));
    check(result != nullptr, "Pilot uses actual shared MRKnob");
    return *result;
}
}

void setupReverbKitPreview(Desktop& desktop) {
    desktop.reverbKitPilot = true;
    fixture(desktop, juce::File::getSpecialLocation(juce::File::tempDirectory));
    desktop.windows.back()->toFront(true);
}

void reverbKitSmoke() {
    Desktop desktop(true);
    desktop.setSize(1400, 850);
    desktop.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktop.setVisible(true);
    const auto acceptedTheme = desktop.theme.findColour(juce::TextButton::buttonColourId);
    // Same-session before/after compares the accepted DAW with opt-in on, no editor.
    desktop.app.stop();
    desktop.app.disconnect();
    const auto emptyDirectory=juce::File::getCurrentWorkingDirectory().getChildFile("matrix-empty");
    check(emptyDirectory.createDirectory().wasOk(),"Matrix directory");desktop.browser->navigate(emptyDirectory);
    for(int scene=0;scene<3;++scene){
        desktop.setWorkspace(scene==0?mrs::desktop::Workspace::mix:mrs::desktop::Workspace::arrange);
        desktop.browser->showFiles(scene==2);
        for (const auto size : {juce::Point<int>{1200,700},{1400,850},{1800,1000}}){
            desktop.setSize(size.x,size.y);
            for (const auto enabled : {false, true}) {
                desktop.reverbKitPilot = enabled;
                for (const auto scale : {1.f, 1.25f, 1.5f, 2.f})
                    image(desktop, "daw-"+juce::String(scene)+"-"+juce::String(size.x)+"x"+juce::String(size.y)+"-" + juce::String(enabled ? "pilot" : "accepted") + "-" + juce::String(static_cast<int>(scale * 100)), scale);
            }
        }
    }
    desktop.setSize(1400,850);desktop.browser->showFiles(false);
    const auto target = fixture(desktop, juce::File::getCurrentWorkingDirectory());
    auto* window = desktop.windows.back().get();
    auto* content = window->getContentComponent();
    auto* controls = dynamic_cast<ReverbKitControls*>(content->findChildWithID("reverb-kit"));
    check(controls && controls->profile() == DesignTokens::target(), "Pilot approved defaults and real controls");
    check(desktop.theme.findColour(juce::TextButton::buttonColourId) == acceptedTheme, "Pilot leaves Desktop theme intact");
    const auto hwnd = static_cast<HWND>(window->getPeer()->getNativeHandle());
    check(!(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST), "Pilot has native owner, not global topmost");
    check(GetWindow(hwnd, GW_OWNER) == GetAncestor(static_cast<HWND>(desktop.getPeer()->getNativeHandle()), GA_ROOT), "Pilot owned by Studio");
    desktop.openInsert(target.track, target.insert);
    check(desktop.windows.back().get() == window, "Reopen raises same live pilot");
    auto revision = desktop.app.services().projects->state().revision;
    auto& mix = knob(*controls, 2);
    check(std::abs(mix.slider.proportionOfLengthToValue(.5)-25)<1e-6&&std::abs(mix.slider.proportionOfLengthToValue(.1)-1)<1e-6,"Accepted Reverb Mix travel preserved");
    desktop.app.play();
    wait(desktop, [&] { return desktop.app.engine()->state().playback == mrs::PlaybackState::playing; }, "Pilot live playback");
    const auto generation = desktop.app.insert_generation();
    mix.beginGesture(); mix.change(35); mix.change(60);
    check(desktop.app.services().projects->state().revision == revision, "Pilot preview no history");
    mix.endGesture();
    check(desktop.app.services().projects->state().revision == revision + 1 && desktop.chain(target.track).front().reverb.mix == .6f, "Pilot one commit, physical percent");
    check(desktop.app.undo(), "Pilot live Undo"); controls->sync(false, false);
    check(mix.value() == 25, "Pilot Undo visible");
    check(desktop.app.redo() && desktop.app.insert_generation() == generation, "Pilot live Redo keeps graph");
    controls->sync(false, false);
    mix.slider.keyPressed(juce::KeyPress(juce::KeyPress::leftKey));
    const auto afterKey = desktop.app.services().projects->state().revision;
    check(afterKey == revision + 4, "Pilot keyboard one history command");
    juce::MouseWheelDetails wheel; wheel.deltaY = .25f;
    const auto now = juce::Time::getCurrentTime();
    juce::MouseEvent mouse{juce::Desktop::getInstance().getMainMouseSource(), {10, 10}, {}, 1, 0, 0, 0, 0, &mix.slider, &mix.slider, now, {10, 10}, now, 1, false};
    mix.slider.mouseWheelMove(mouse, wheel);
    check(desktop.app.services().projects->state().revision == afterKey + 1, "Pilot wheel one history command");
    revision = desktop.app.services().projects->state().revision;
    mix.beginGesture(); mix.change(10); mix.cancelGesture();
    check(desktop.app.services().projects->state().revision == revision, "Pilot Escape/cancel no commit");
    auto* options = dynamic_cast<juce::TextButton*>(content->findChildWithID("reverb-options"));
    auto* gate = dynamic_cast<juce::ToggleButton*>(content->findChildWithID("reverb-gate"));
    check(options && gate && !knob(*controls, 4).isVisible(), "Options stays optional");
    options->onClick();
    check(knob(*controls, 4).isVisible() && !knob(*controls, 4).isEnabled(), "Disabled gate threshold");
    gate->setToggleState(true, juce::dontSendNotification); gate->onClick();
    check(knob(*controls, 4).isEnabled(), "Existing gate command enables threshold");
    knob(*controls, 4).field.setText("-75.3 dB", false); check(knob(*controls, 4).field.commitText(), "Pilot threshold typed units");
    check(std::abs(desktop.chain(target.track).front().reverb.gate_threshold + 75.3f) < .0001f, "Pilot threshold physical value");
    const auto beforeStyle = desktop.chain(target.track).front();
    revision = desktop.app.services().projects->state().revision;
    GalleryDraft draft; draft.tokens.colours[static_cast<std::size_t>(ColourRole::focus)] = 0xffb7cce8; draft.tokens.metrics[6] = 8;
    controls->applyProfile(draft.encode());
    check(controls->profile() == draft.tokens && desktop.theme.findColour(juce::TextButton::buttonColourId) == acceptedTheme && desktop.chain(target.track).front() == beforeStyle && desktop.app.services().projects->state().revision == revision, "Gallery import scoped, no model/Undo change");
    bool invalid = false; try { controls->applyProfile("{\"galleryVersion\":999}"); } catch (const std::exception&) { invalid = true; }
    check(invalid && controls->profile() == draft.tokens, "Invalid Gallery import atomic");
    auto* reset = dynamic_cast<kit::MRButton*>(controls->findChildWithID("kit-reset-profile")); check(reset != nullptr, "Shared MRButton"); reset->onClick();
    check(controls->profile() == DesignTokens::target(), "Reset approved Kit variant");
    desktop.app.stop(); wait(desktop, [&] { return desktop.app.engine()->state().playback != mrs::PlaybackState::playing; }, "Pilot stopped");
    const auto geometry = window->getBounds();
    auto preset = desktop.chain(target.track).front();
    const auto presetFile = target.folder.getChildFile("Reverb.mrspreset");
    for (const auto value : {.3f, .8f, .3f}) {
        preset.reverb.mix = value; preset.bypass = value == .8f;
        check(presetFile.replaceWithText(encodePreset(preset)), "Pilot preset fixture");
        desktop.loadPresetFile(target.track, target.insert, presetFile);
        controls->sync(true, true);
        check(window->getContentComponent() == content && window->getBounds() == geometry && std::abs(mix.value() - value * 100.) < .00001 && desktop.chain(target.track).front().bypass == preset.bypass, "Preset A/B/A in place, geometry/values/bypass");
    }
    const auto projectFile = target.folder.getChildFile("pilot.mrsproject");
    desktop.app.save_project(path(projectFile));
    double maxSyncUs{}, maxPaintUs{};
    for (int i = 0; i < 100; ++i) { const auto begin = std::chrono::steady_clock::now(); controls->sync(true, true); maxSyncUs = (std::max)(maxSyncUs, std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - begin).count()); }
    for (const auto size : {juce::Point<int>{900, 650}, {1040, 720}, {1500, 1000}}) {
        content->setSize(size.x, size.y);
        check(content->getLocalBounds().contains(controls->getBounds()), "Pilot controls inside editor");
        for (int parameter : {0, 1, 2, 4}) check(controls->getLocalBounds().contains(knob(*controls, parameter).getBounds()), "Pilot knob not clipped");
        for (const auto scale : {1.f, 1.25f, 1.5f, 2.f}) {
            const auto begin = std::chrono::steady_clock::now();
            image(*content, "pilot-" + juce::String(size.x) + "x" + juce::String(size.y) + "-" + juce::String(static_cast<int>(scale * 100)), scale);
            maxPaintUs = (std::max)(maxPaintUs, std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - begin).count());
        }
    }
    // Accepted native browser search still guards the native Space bridge.
    auto* browser = content->findChildWithID("reverb-browser");
    auto* search = dynamic_cast<juce::TextEditor*>(browser->findChildWithID("reverb-search")); check(search != nullptr, "Pilot search");
    SetFocus(hwnd); search->grabKeyboardFocus();
    PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 1); PostMessageW(hwnd, WM_CHAR, L' ', 1); PostMessageW(hwnd, WM_KEYUP, VK_SPACE, LPARAM{1} << 31);
    for (int i = 0; i < 10; ++i) pump(desktop);
    check(desktop.app.engine()->state().playback != mrs::PlaybackState::playing, "Pilot text Space guard");
    for (int i = 0; i < 8; ++i) {
        knob(*controls, 2).beginGesture(); knob(*controls, 2).change(55);
        const auto beforeClose = desktop.app.services().projects->state().revision;
        window->closeButtonPressed();
        check(desktop.app.services().projects->state().revision == beforeClose, "Pilot close cancels without commit");
        desktop.openInsert(target.track, target.insert); window = desktop.windows.back().get(); content = window->getContentComponent();
        controls = dynamic_cast<ReverbKitControls*>(content->findChildWithID("reverb-kit")); check(controls != nullptr, "Pilot repeated teardown/reopen");
        // Resolve controls anew after retirement; never retain an old editor pointer.
        if (i != 7) { knob(*controls, 2).beginGesture(); knob(*controls, 2).change(45); knob(*controls, 2).cancelGesture(); }
    }
    desktop.closeEditors();
    desktop.app.open_project(path(projectFile));
    desktop.refresh(true); desktop.openInsert(target.track, target.insert);
    controls = dynamic_cast<ReverbKitControls*>(desktop.windows.back()->getContentComponent()->findChildWithID("reverb-kit"));
    check(controls && std::abs(knob(*controls, 2).value() - 30.) < .00001, "Pilot project round-trip parameters");
    desktop.closeEditors(); desktop.reverbKitPilot = false; desktop.openInsert(target.track, target.insert);
    check(!desktop.windows.back()->getContentComponent()->findChildWithID("reverb-kit") && desktop.windows.back()->getContentComponent()->findChildWithID("reverb-parameter-2"), "Opt-out restores accepted editor");
    check(desktop.message.isEmpty(), "Pilot shell no errors");
    desktop.closeEditors(); desktop.removeFromDesktop();
    juce::File::getCurrentWorkingDirectory().getChildFile("pilot195-results.txt").replaceWithText("Reverb Kit native shell/IR/presets/bypass/units/one Undo/live history/Gallery import/lifetime/Space/project PASS\nmax UI sync us=" + juce::String(maxSyncUs, 3) + "; max snapshot+PNG us=" + juce::String(maxPaintUs, 3) + "\nNo physical ASIO/xrun/DPI/screen-reader claim.\n");
}
}
