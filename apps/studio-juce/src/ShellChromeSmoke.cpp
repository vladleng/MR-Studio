#include "Desktop.h"
#include "ShellChrome.h"
#include "MusicalGrid.h"
#include <windows.h>
#include <chrono>
#include <thread>
#include <mrs/vst3.hpp>

namespace ui {
void channel213Smoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    const auto pump=[](){for(int i=0;i<20;++i){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}juce::Thread::sleep(1);}};
    Desktop d(true);d.app.new_project(48000);const auto audio=d.app.add_audio_track("Audio / Гитара"),keys=d.app.add_instrument_track("Keys"),bus=d.app.add_bus("Bus");
    d.app.set_track_output(audio,bus);d.app.set_track_sends(audio,{{bus,.5f,false}});mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::gain;d.app.set_inserts(audio,{fx});
    d.refresh(true);d.setAssetDesign(true);d.setWorkspace(mrs::desktop::Workspace::mix);d.setSize(1200,760);d.addToDesktop(0);d.setVisible(true);d.toFront(true);d.grabKeyboardFocus();
    const auto strip=[&](const mrs::Id& id,bool mini)->Strip*{auto& list=mini?d.arrangement->rows:d.mixer;for(auto& s:list)if(s->target==id)return s.get();return nullptr;};
    const auto button=[&](juce::Component& c,const char* id){auto* b=dynamic_cast<juce::Button*>(c.findChildWithID(id));check(b!=nullptr,"Channel control exists");return b;};
    auto* mixer=strip(audio,false);auto* row=strip(audio,true);const auto generation=d.app.insert_generation();
    const auto clips=d.project()->clips;d.toFront(true);d.grabKeyboardFocus();pump();button(*mixer,"channel-color")->onClick();
    std::function<juce::Component*(juce::Component*)> findPalette=[&](juce::Component* c)->juce::Component*{if(dynamic_cast<MenuColourPalette*>(c))return c;for(auto* child:c->getChildren())if(auto* found=findPalette(child))return found;return nullptr;};juce::Component* actualPalette{};
    for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)if(auto* found=findPalette(juce::Desktop::getInstance().getComponent(i)))actualPalette=found;
    if(!actualPalette||actualPalette->getNumChildComponents()!=32){juce::String tree;std::function<void(juce::Component*,int)> dump=[&](juce::Component* c,int depth){tree+=juce::String::repeatedString(" ",depth)+c->getName()+" / "+c->getComponentID()+" / "+juce::String(c->getNumChildComponents())+" / visible="+juce::String(c->isVisible()?1:0)+"\n";if(depth<5)for(auto* child:c->getChildren())dump(child,depth+1);};for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)dump(juce::Desktop::getInstance().getComponent(i),0);juce::File::getCurrentWorkingDirectory().getChildFile("channel213-popup-tree.txt").replaceWithText(tree);}
    check(actualPalette&&actualPalette->getNumChildComponents()==32,"Actual LMB channel popup has 32 choices");auto* swatch=dynamic_cast<juce::TextButton*>(actualPalette->getChildComponent(31));check(swatch!=nullptr,"Last swatch button");swatch->onClick();pump();d.refresh();check(d.project()->tracks.front().color==ObjectPalette::clips.back().rgb,"Popup selection commits RGB");
    check(d.project()->clips==clips&&d.app.insert_generation()==generation,"Color leaves clips/runtime untouched");
    const auto colorRevision=d.app.services().projects->state().revision;button(*mixer,"channel-color")->onClick();juce::PopupMenu::dismissAllActiveMenus();pump();check(d.app.services().projects->state().revision==colorRevision,"Cancel color popup has no history");
    check(button(*mixer,"channel-color")->isVisible()&&button(*row,"channel-color")->isVisible(),"Both color fields");d.openInsert(audio,fx.id);check(!d.windows.empty(),"Native editor opens");auto* window=d.windows.back().get();
    d.dropTrack("mrs-track:"+juce::String::fromUTF8(audio.value.c_str()),3);pump();d.refresh();check(d.project()->tracks.back().id==audio&&strip(audio,false)==mixer&&strip(audio,true)==row,"Shared order retains both strips");
    check(d.windows.back().get()==window&&window->isVisible()&&d.app.insert_generation()==generation,"Reorder retains editor and DSP generation");
    d.action(10);check(d.project()->tracks.front().id==audio&&d.windows.back().get()==window&&window->isVisible(),"Reorder Undo retains editor");d.action(11);check(d.project()->tracks.back().id==audio&&window->isVisible(),"Reorder Redo retains editor");
    const auto revision=d.app.services().projects->state().revision;d.dropTrack("mrs-track:"+juce::String::fromUTF8(audio.value.c_str()),3);pump();check(d.app.services().projects->state().revision==revision,"No-op drag has no history");
    auto* empty=strip(keys,false);check(button(*empty,"channel-instrument")->isVisible()&&button(*empty,"midi-input-status")->isVisible()&&button(*empty,"channel-piano")->isVisible(),"Empty instrument controls separated and visible");
    const auto sourceModule=juce::File::getCurrentWorkingDirectory().getChildFile("../../Release/mrs_vst3_fixture.vst3");check(sourceModule.existsAsFile(),"Synthetic plugin fixture exists");const auto module=juce::File::getCurrentWorkingDirectory().getChildFile("instrument.vst3");check(sourceModule.copyFileTo(module),"Copy isolated instrument fixture");const auto descriptor=mrs::processing::probe_vst3(module.getFullPathName().toStdString()).front();
    mrs::NativeInsert synth;synth.id=mrs::new_id();synth.kind=mrs::InsertKind::vst3;synth.plugin_path=descriptor.path;synth.class_id=descriptor.class_id;synth.plugin_name=descriptor.name;auto effect=fx;effect.id=mrs::new_id();d.app.set_inserts(keys,{synth,effect});d.app.set_midi_input(keys,"missing:test-input",3,true);d.refresh();
    check(empty->pluginInsertionAt(empty->pluginDropArea().getTopLeft())==1,"FX drop translates past hidden instrument slot");
    d.closeEditors();button(*empty,"channel-piano")->onClick();check(!d.windows.empty()&&d.windows.back()->isVisible(),"Piano opens synthetic instrument");auto* synthWindow=d.windows.back().get();const auto count=d.windows.size();button(*empty,"channel-instrument")->onClick();check(d.windows.size()==count&&d.windows.back().get()==synthWindow,"Instrument button raises same editor");
    const auto synthGeneration=d.app.insert_generation();d.app.reorder_track(keys,0);d.refresh();check(d.app.insert_generation()==synthGeneration&&synthWindow->isVisible()&&d.windows.back().get()==synthWindow,"Instrument reorder retains DSP/editor");check(d.app.undo()&&d.app.insert_generation()==synthGeneration&&synthWindow->isVisible(),"Instrument order Undo retains DSP/editor");d.refresh();
    auto result=std::make_shared<std::uint32_t>(0);MenuColourPalette palette(d.theme,0,result,true,true);int pw{},ph{};palette.getIdealSize(pw,ph);palette.setSize(pw,ph);check(palette.getNumChildComponents()==32,"Shared palette has 32 choices");for(auto* c:palette.getChildren())check(palette.getLocalBounds().contains(c->getBounds()),"All 32 choices fit popup");
    const auto save=[&](juce::Component& c,juce::String name){for(float scale:{1.f,1.25f,1.5f,2.f}){auto out=juce::File::getCurrentWorkingDirectory().getChildFile(name+"-"+juce::String(juce::roundToInt(scale*100))+".png").createOutputStream();check(out!=nullptr,"Preview output");out->setPosition(0);out->truncate();check(juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds(),true,scale),*out),"Preview PNG");}};
    save(palette,"channel213-palette");save(d,"channel213-mix");d.setWorkspace(mrs::desktop::Workspace::edit);for(int pitch:{40,84,128}){d.arrangement->trackHeight=pitch;d.arrangement->resized();check(d.arrangement->rows.front()->getHeight()==pitch-2,"Actual track pitch in preview");save(d,"channel213-tracks-"+juce::String(pitch));}d.setSize(1200,700);d.setWorkspace(mrs::desktop::Workspace::mix);save(d,"channel213-minimum");
    const auto path=juce::File::getCurrentWorkingDirectory().getChildFile("channel213-fixture-"+juce::String::fromUTF8(d.project()->id.value.c_str())+".mrsproject");d.app.save_project(std::filesystem::path(path.getFullPathName().toWideCharPointer()));const auto saved=mrs::persistence::load_project(std::filesystem::path(path.getFullPathName().toWideCharPointer())).project;d.closeEditors();d.app.open_project(std::filesystem::path(path.getFullPathName().toWideCharPointer()));if(*d.project()!=saved){juce::File::getCurrentWorkingDirectory().getChildFile("channel213-saved.txt").replaceWithText(juce::String::fromUTF8(mrs::serialize(saved).c_str()));juce::File::getCurrentWorkingDirectory().getChildFile("channel213-loaded.txt").replaceWithText(juce::String::fromUTF8(mrs::serialize(*d.project()).c_str()));}check(*d.project()==saved,"Channel order/color/routing SaveOpen");
    auto missing=d.app.snapshot();for(auto& t:missing.project.tracks)if(t.id==keys)t.inserts.front().plugin_path+=".missing";const auto missingFile=juce::File::getCurrentWorkingDirectory().getChildFile("channel213-missing-"+juce::String::fromUTF8(d.project()->id.value.c_str())+".mrsproject");mrs::persistence::save_project(std::filesystem::path(missingFile.getFullPathName().toWideCharPointer()),missing);d.app.open_project(std::filesystem::path(missingFile.getFullPathName().toWideCharPointer()));d.resetDevice();d.refresh(true);check(button(*strip(keys,false),"midi-input-status")->getButtonText().contains("unavailable"),"Missing instrument is explicit in actual channel status");save(d,"channel213-missing");d.removeFromDesktop();
    juce::File::getCurrentWorkingDirectory().getChildFile("channel213-results.txt").replaceWithText("PASS channel color32/order/strip identity/native editor lifetime/generation/Undo/Redo/no-op/SaveOpen; layout software100/125/150/200. Physical ASIO/listening/DPI NOT TESTED.\n");
}
void compactFader215Smoke(){
    const auto require=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    const auto save=[&](juce::Component& c,juce::String name){for(const auto scale:{1.f,1.25f,1.5f,2.f}){auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name+"-"+juce::String(juce::roundToInt(scale*100))+".png").createOutputStream();require(file!=nullptr,"Fader preview file");file->setPosition(0);file->truncate();require(juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*file),"Fader preview PNG");}};
    Desktop d(true);d.app.demo();d.app.add_instrument_track("MIDI gain");d.app.add_audio_track("Audio gain");d.refresh(true);d.setAssetDesign(true);d.setVisible(true);d.setWorkspace(mrs::desktop::Workspace::arrange);d.setSize(1200,700);
    auto& a=*d.arrangement;auto& gain=a.rows.front()->gain;const auto id=*a.rows.front()->target;
    const auto mouse=[](Handle& control,juce::Point<float> point,int extra=0){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|extra),1,0,0,0,0,&control,&control,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};
    const auto state=[&]{return *d.project();};
    for(const int pitch:{40,56,84,128}){a.trackHeight=pitch;a.resized();for(const auto& row:a.rows){require(row->gain.isVisible()&&row->getLocalBounds().contains(row->gain.getBounds()),"All track gains visible within header");require(row->gain.compact==(pitch<86),"Compact presentation threshold");require(row->gain.knob().getHeight()>0&&row->gain.getLocalBounds().toFloat().contains(row->gain.knob()),"Fader hit target valid");}save(d,"compact215-track-"+juce::String(pitch));}
    a.trackHeight=40;a.resized();const auto original=state();const auto revision=d.app.services().projects->state().revision;const auto point=gain.knob().getCentre();
    gain.mouseDown(mouse(gain,point));gain.mouseDrag(mouse(gain,point.translated(10,0)));require(state()==original&&gain.dragging(),"Gain preview does not mutate Project");gain.mouseUp(mouse(gain,point));require(d.project()->tracks.front().mix.gain!=original.tracks.front().mix.gain,"Compact drag changes actual gain");require(d.app.undo()&&state()==original,"Compact drag one Undo");require(d.app.redo(),"Compact drag Redo");require(d.app.undo(),"Restore gain baseline");d.refresh();
    const auto cancelRevision=d.app.services().projects->state().revision;const auto cancelPoint=gain.knob().getCentre();gain.mouseDown(mouse(gain,cancelPoint));gain.mouseDrag(mouse(gain,cancelPoint.translated(-10,0)));gain.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));gain.mouseUp(mouse(gain,cancelPoint));require(state()==original&&d.app.services().projects->state().revision==cancelRevision,"Compact Escape cancels preview");
    const auto startValue=gain.value;gain.mouseDown(mouse(gain,gain.knob().getCentre()));gain.mouseDrag(mouse(gain,gain.knob().getCentre().translated(10,0),juce::ModifierKeys::ctrlModifier));require(std::abs((gain.value-startValue)-.005)<1.e-7,"Compact Ctrl fine relative drag");gain.focusLost(juce::Component::focusChangedDirectly);require(state()==original,"Focus loss cancels compact drag");
    gain.keyPressed(juce::KeyPress(juce::KeyPress::leftKey));require(state()!=original,"Compact keyboard changes gain");gain.mouseDoubleClick(mouse(gain,gain.knob().getCentre()));require(std::abs(d.project()->tracks.front().mix.gain-1.f)<1.e-6f,"Compact double-click unity reset");require(d.app.undo(),"Reset Undo");require(d.app.undo()&&state()==original,"Keyboard Undo");d.refresh();
    const auto noOpRevision=d.app.services().projects->state().revision;gain.mouseDown(mouse(gain,gain.knob().getCentre()));gain.mouseUp(mouse(gain,gain.knob().getCentre()));require(d.app.services().projects->state().revision==noOpRevision&&revision<=noOpRevision,"Compact no-op preserves history");
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("compact215-fixture.mrsproject");d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));const auto saved=d.app.snapshot().project;d.app.open_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));require(*d.project()==saved,"Compact gain Save/Open");
    struct Gallery final:juce::Component{
        std::vector<std::unique_ptr<Handle>> controls;
        Gallery(Theme* theme){setSize(380,270);const std::array<double,7> values{0,.55,60./72.,1,.6,.7,.4};for(std::size_t i=0;i<values.size();++i){auto c=std::make_unique<Handle>(false);c->compact=true;c->value=values[i];c->setLookAndFeel(theme);c->setBounds(110,42+static_cast<int>(i)*30,250,18);addAndMakeVisible(*c);controls.push_back(std::move(c));}controls[4]->setEnabled(false);}
        void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(surface));g.setColour(juce::Colour(primaryText));g.setFont(Theme::detailFont());g.drawText("MR Slide fill / real shared gain control",12,8,360,24,juce::Justification::left);const std::array<const char*,7> names{"Minimum","SVG 55%","0 dB unity","Maximum","Disabled","Focus","Drag"};for(std::size_t i=0;i<names.size();++i)g.drawText(names[i],12,40+static_cast<int>(i)*30,94,22,juce::Justification::left);}
    } gallery(d.assetLookAndFeel());gallery.addToDesktop(juce::ComponentPeer::windowIsTemporary);gallery.setVisible(true);gallery.toFront(true);gallery.controls[5]->grabKeyboardFocus();gallery.controls[5]->focusGained(juce::Component::focusChangedByTabKey);save(gallery,"compact215-gallery-focus");auto& active=*gallery.controls[6];const auto p=active.knob().getCentre();active.mouseDown(mouse(active,p));active.mouseDrag(mouse(active,p.translated(12,0)));save(gallery,"compact215-gallery-drag");active.mouseUp(mouse(active,p));gallery.removeFromDesktop();
    juce::File::getCurrentWorkingDirectory().getChildFile("compact215-results.txt").replaceWithText("PASS compact gain: min40/56/84/full128; real gain/preview/Undo/Redo/fine/Escape/focus/reset/no-op/SaveOpen; Gallery100/125/150/200. Physical DPI/ASIO NOT TESTED. No MIDI/RT source changes.\n");
}
void interface212Smoke(){
    const auto pump=[](int milliseconds){for(int n=0;n<milliseconds;++n){MSG message{};int limit=0;while(limit++<200&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}juce::Thread::sleep(1);}};
    const auto require=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.demo();for(int i=0;i<12;++i)d.app.add_audio_track("Compact "+std::to_string(i+1));d.refresh(true);d.setAssetDesign(true);d.setVisible(true);d.addToDesktop(0);d.setWorkspace(mrs::desktop::Workspace::mix);d.setSize(1200,700);
    const auto original=*d.project();auto& a=*d.arrangement;const auto selected=d.project()->clips.front().id;d.activateClip(selected);
    const auto snapshot=[&](juce::String name){for(const auto scale:{1.f,1.25f,1.5f,2.f}){auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name+"-"+juce::String(juce::roundToInt(scale*100))+".png").createOutputStream();require(file!=nullptr,"Preview file");file->setPosition(0);file->truncate();require(juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*file),"Preview PNG");}};
    require(d.clickButton.getY()>d.arrangeArea.getBottom()&&d.countButton.getY()==d.clickButton.getY(),"Click/count live on transport");
    for(auto* button:{&d.pointerTool,&d.knifeTool,&d.undo,&d.redo,&d.duplicate,&d.zoomIn,&d.zoomOut,&d.fit,&d.snapButton})require(button->getProperties()["mrHostIcon"].toString().isNotEmpty()&&button->getTooltip().isNotEmpty(),"Toolbar icon has semantics");
    const auto click=d.app.click_settings();d.clickButton.onClick();require(d.app.click_settings().playback!=click.playback,"Click callback retained");d.app.set_click_settings(click);d.countButton.onClick();require(d.app.click_settings().count_bars!=click.count_bars,"Count callback retained");d.app.set_click_settings(click);
    auto* fx=d.browser->findChildWithID("browser-vst-tab");auto* files=d.browser->findChildWithID("browser-files-tab");auto* ai=d.browser->findChildWithID("browser-ai-tab");require(fx&&files&&ai&&fx->getX()==8&&files->getX()-fx->getRight()==2&&ai->getRight()==108,"Browser tabs left compact group");snapshot("interface212-min");
    a.pixelsPerSecond=30;a.horizontal=200;const auto anchor=(a.getWidth()-Arrangement::left)*.5;const auto before=(a.horizontal+anchor)/a.pixelsPerSecond;a.zoom(2);require(std::abs((a.horizontal+anchor)/a.pixelsPerSecond-before)<1.e-8,"Horizontal centre anchor");
    a.wheel(-100,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),400);require(a.trackHeight==40&&a.rows.front()->getHeight()==38,"Vertical extended minimum and gap");
    for(const auto& row:a.rows)for(auto* child:row->getChildren())if(child->isVisible())require(row->getLocalBounds().contains(child->getBounds()),"Compact visible controls within row");
    a.zoom(.000001);require(a.pixelsPerSecond==.1&&d.selectedClip==selected&&*d.project()==original,"Extended horizontal minimum retains model/selection");
    ViewSettings settings;settings.trackHeight=40;settings.pixelsPerSecond=.1;const auto decoded=ViewSettings::decode(settings.encode());require(decoded.trackHeight==40&&decoded.pixelsPerSecond==.1,"Zoom preference roundtrip");
    a.pixelsPerSecond=30;a.horizontal=0;d.setWorkspace(mrs::desktop::Workspace::arrange);a.resized();snapshot("interface212-compact");
    std::size_t oldLines{},newLines{};const auto spacing=a.displaySpacing();visitDisplayGrid(d.project()->time,0,32*mrs::ppq,spacing,[&](auto,auto){++oldLines;});visitDisplayGrid(d.project()->time,0,32*mrs::ppq,static_cast<mrs::Tick>(std::ceil(spacing*2.4)),[&](auto,auto){++newLines;});require(newLines<oldLines&&a.displaySpacing()==spacing,"Earlier visual grid LOD leaves snap ladder intact");
    d.setSize(1800,1000);snapshot("interface212-nominal");
    for(const auto corner:{juce::Point<int>{320,150},juce::Point<int>{d.getWidth()-8,d.getHeight()-8}}){d.toFront(true);d.grabKeyboardFocus();pump(5);juce::PopupMenu menu;menu.setLookAndFeel(&d.theme);menu.addItem(1,"Rename selected clips");menu.addItem(2,"Disabled action",false);juce::PopupMenu submenu;submenu.addItem(3,"Nested action");menu.addSubMenu("Submenu",submenu);menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({d.getScreenX()+corner.x,d.getScreenY()+corner.y,1,1}),[](int){});pump(20);bool found=false;auto& desktop=juce::Desktop::getInstance();for(int i=0;i<desktop.getNumComponents();++i){auto* popup=desktop.getComponent(i);if(popup!=&d&&&popup->getLookAndFeel()==&d.theme&&popup->isVisible()){found=true;auto file=juce::File::getCurrentWorkingDirectory().getChildFile(corner.x==320?"interface212-menu.png":"interface212-menu-edge.png").createOutputStream();require(file&&juce::PNGImageFormat().writeImageToStream(popup->createComponentSnapshot(popup->getLocalBounds()),*file),"Menu snapshot");}}if(!found){juce::String debug;for(int i=0;i<desktop.getNumComponents();++i){auto* c=desktop.getComponent(i);debug+=c->getName()+" visible="+juce::String(c->isVisible()?1:0)+" bounds="+c->getBounds().toString()+" sameTheme="+juce::String(&c->getLookAndFeel()==&d.theme?1:0)+"\\n";}debug+="modal="+juce::String(juce::Component::getCurrentlyModalComponent()?1:0);juce::File::getCurrentWorkingDirectory().getChildFile("interface212-popup-debug.txt").replaceWithText(debug);}require(found,"Real MR popup shown");if(corner.x==320){juce::Component* rootMenu=nullptr;for(int i=0;i<desktop.getNumComponents();++i){auto* candidate=desktop.getComponent(i);if(candidate!=&d&&&candidate->getLookAndFeel()==&d.theme&&candidate->isVisible())rootMenu=candidate;}require(rootMenu!=nullptr,"Menu keyboard root");rootMenu->keyPressed(juce::KeyPress(juce::KeyPress::downKey));rootMenu->keyPressed(juce::KeyPress(juce::KeyPress::downKey));rootMenu->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));pump(30);bool nested=false;for(int i=0;i<desktop.getNumComponents();++i){auto* child=desktop.getComponent(i);if(child!=&d&&child!=rootMenu&&&child->getLookAndFeel()==&d.theme&&child->isVisible()){nested=true;auto output=juce::File::getCurrentWorkingDirectory().getChildFile("interface212-submenu.png").createOutputStream();require(output&&juce::PNGImageFormat().writeImageToStream(child->createComponentSnapshot(child->getLocalBounds()),*output),"Nested menu preview");}}require(nested,"Keyboard opens submenu");}juce::PopupMenu::dismissAllActiveMenus();pump(10);}
    struct Contrast final:juce::Component{void paint(juce::Graphics& g)override{g.fillAll(juce::Colours::white);g.setColour(juce::Colours::black);for(int y=0;y<getHeight();y+=8)for(int x=0;x<getWidth();x+=8)if((x/8+y/8)%2)g.fillRect(x,y,8,8);}} contrast;
    contrast.getProperties().set("mrPopupRoot",true);contrast.setBounds(280,180,400,260);contrast.addToDesktop(0);contrast.setVisible(true);contrast.toFront(true);contrast.grabKeyboardFocus();pump(5);juce::PopupMenu contrastMenu;contrastMenu.setLookAndFeel(&d.theme);contrastMenu.addItem(1,"High contrast background");contrastMenu.addItem(2,"Keyboard navigation");contrastMenu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({300,200,1,1}),[](int){});pump(20);
    auto& desktop=juce::Desktop::getInstance();for(int i=0;i<desktop.getNumComponents();++i){auto* popup=desktop.getComponent(i);if(popup!=&d&&popup!=&contrast&&&popup->getLookAndFeel()==&d.theme&&popup->isVisible()){auto file=juce::File::getCurrentWorkingDirectory().getChildFile("interface212-menu-contrast.png").createOutputStream();require(file&&juce::PNGImageFormat().writeImageToStream(popup->createComponentSnapshot(popup->getLocalBounds()),*file),"Contrast menu preview");require(popup->keyPressed(juce::KeyPress(juce::KeyPress::downKey)),"Menu keyboard navigation");popup->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));break;}}juce::PopupMenu::dismissAllActiveMenus();contrast.removeFromDesktop();
    require(*d.project()==original,"UI geometry and menus leave model intact");d.removeFromDesktop();
}
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void save(const juce::Image& image,const juce::String& name) {
    auto output=juce::File::getCurrentWorkingDirectory().getChildFile(name+".png").createOutputStream();
    check(output!=nullptr,"Shell image stream"); output->setPosition(0); output->truncate();
    check(juce::PNGImageFormat().writeImageToStream(image,*output),"Shell image write");
}
class Clock final : public mrs::audio::IAudioDevice {
public:
    int opens{};
    std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Shell fixture (no hardware)",{"Input"},{"L","R"},16,2048,128,-1}};}
    void control_panel(int)override{}
    void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{++opens;}
    void start()override{} void stop()override{} void close()noexcept override{}
    mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}
};
}

void shellDesignSmoke() {
    Desktop d(true);
    d.addToDesktop(juce::ComponentPeer::windowIsTemporary);d.setVisible(true);
    d.app.stop();d.app.disconnect();
    const auto empty=juce::File::getCurrentWorkingDirectory().getChildFile("matrix-empty");
    check(empty.createDirectory().wasOk(),"Shell empty browser folder");d.browser->navigate(empty);
    const auto originalTheme=d.theme.findColour(juce::TextButton::buttonColourId);
    const auto originalRevision=d.app.services().projects->state().revision;
    int pairs{};
    for(int scene=0;scene<3;++scene){
        d.setWorkspace(scene==0?mrs::desktop::Workspace::mix:mrs::desktop::Workspace::arrange);d.browser->showFiles(scene==2);
        for(const auto size:{juce::Point<int>{1200,700},{1400,850},{1800,1000}}){
            d.setSize(size.x,size.y);d.setShellDesign(false);
            const auto area=d.arrangeArea,mix=d.mixArea,browser=d.browser->getBounds();
            std::array<juce::Image,4> before;
            const std::array<float,4> scales{1,1.25f,1.5f,2};
            for(std::size_t i=0;i<scales.size();++i){before[i]=d.createComponentSnapshot(d.getLocalBounds(),true,scales[i],juce::SoftwareImageType{});save(before[i],"accepted-"+juce::String(scene)+"-"+juce::String(size.x)+"x"+juce::String(size.y)+"-"+juce::String(static_cast<int>(scales[i]*100)));}
            d.setShellDesign(true);
            check(d.arrangeArea==area&&d.mixArea==mix&&d.browser->getBounds()==browser,"Shell leaves content geometry intact");
            check(!d.theme.isPreview()&&d.theme.findColour(juce::TextButton::buttonColourId)==originalTheme,"Parent Theme isolated");
            juce::Component* buttons[]{&d.pointerTool,&d.knifeTool,&d.undo,&d.redo,&d.duplicate,&d.zoomIn,&d.zoomOut,&d.fit,&d.snapButton,&d.clickButton,&d.countButton,&d.play,&d.pause,&d.stop,&d.record,&d.previous,&d.next,&d.loop,&d.editButton,&d.mixButton,&d.brows,&d.navigationMode};
            for(std::size_t i=0;i<std::size(buttons);++i){
                check(d.getLocalBounds().contains(buttons[i]->getBounds())&&buttons[i]->getHeight()>=32&&buttons[i]->getWidth()>=32,"Shell effective target size/bounds");
                for(std::size_t j=i+1;j<std::size(buttons);++j)check(!buttons[i]->getBounds().intersects(buttons[j]->getBounds()),"Shell targets cannot overlap");
            }
            check(!d.split.isVisible(),"No duplicate Split toolbar button");
            check(dynamic_cast<kit::MRButton*>(&d.play)!=nullptr,"Real shared MRButton");
            check(d.play.getParentComponent()==&d&&d.play.getExplicitFocusOrder()>0,"Root keyboard parent/focus order retained");
            for(std::size_t i=0;i<scales.size();++i){
                const auto after=d.createComponentSnapshot(d.getLocalBounds(),true,scales[i],juce::SoftwareImageType{});
                save(after,"shell-"+juce::String(scene)+"-"+juce::String(size.x)+"x"+juce::String(size.y)+"-"+juce::String(static_cast<int>(scales[i]*100)));
                const int top=juce::roundToInt(94*scales[i]),bottom=juce::roundToInt((size.y-(size.x<1500?58:42))*scales[i]);
                for(int y=top;y<bottom;++y)for(int x=0;x<after.getWidth();++x)check(before[i].getPixelAt(x,y)==after.getPixelAt(x,y),"Arrange/Mixer/Browser pixels changed by shell");
                ++pairs;
            }
        }
    }
    check(d.app.services().projects->state().revision==originalRevision,"Appearance/workspace changes do not create history");
    d.setSize(1400,850);d.browser->showFiles(false);d.setWorkspace(mrs::desktop::Workspace::mix);
    auto clock=std::make_unique<Clock>();auto* device=clock.get();d.app.connect(std::move(clock),{0,48000,128,{0},{0,1}});
    std::array<float,128> input{};std::array<float,256> output{};
    const auto flush=[&]{d.app.engine()->process(input.data(),output.data(),128);d.app.poll();d.refresh();};
    d.app.stop();flush();check(d.shellStatus.getText().startsWith("STOPPED")&&d.stop.getToggleState(),"Authoritative stopped state");
    save(d.createComponentSnapshot(d.getLocalBounds()),"state-stopped");
    d.play.onClick();flush();check(d.shellStatus.getText().startsWith("PLAYING")&&d.play.getToggleState(),"Existing Play command/state");
    d.play.grabKeyboardFocus();save(d.createComponentSnapshot(d.getLocalBounds()),"state-playing-focus");
    d.pause.onClick();flush();check(d.shellStatus.getText().startsWith("PAUSED")&&d.pause.getToggleState(),"Existing Pause command/state");
    save(d.createComponentSnapshot(d.getLocalBounds()),"state-paused");
    d.stop.onClick();flush();d.grabKeyboardFocus();
    d.spaceKey(false);d.spaceKey(true);flush();d.spaceKey(true);flush();check(d.displayState().playback==mrs::PlaybackState::playing,"Space held does not toggle twice");
    d.spaceKey(false);d.spaceKey(true);flush();d.spaceKey(false);check(d.displayState().playback==mrs::PlaybackState::stopped,"Existing Space stop semantics");
    d.browser->showFiles(true);juce::TextEditor* text=nullptr;
    for(auto* child:d.browser->getChildren())if(auto* candidate=dynamic_cast<juce::TextEditor*>(child);candidate&&candidate->getTitle()=="Computer folder path")text=candidate;
    check(text!=nullptr,"Text focus fixture");text->grabKeyboardFocus();check(!d.shortcutAllowed()&&!d.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey))&&d.displayState().playback==mrs::PlaybackState::stopped,"Text editing guards Space");
    d.grabKeyboardFocus();d.browser->showFiles(false);
    const auto priorSidebar=d.sidebar;check(d.workspaceShortcut(juce::KeyPress(juce::KeyPress::F5Key))&&d.sidebar!=priorSidebar,"F5 browser shortcut");d.workspaceShortcut(juce::KeyPress(juce::KeyPress::F5Key));
    d.workspaceShortcut(juce::KeyPress(juce::KeyPress::F3Key));check(d.app.workspace()==mrs::desktop::Workspace::arrange,"F3 Arrange");d.mixButton.onClick();check(d.app.workspace()==mrs::desktop::Workspace::mix,"Mix button original callback");
    d.knifeTool.onClick();check(d.knifeMode,"Knife command");d.pointerTool.onClick();check(!d.knifeMode,"Pointer command");
    const auto trackCount=d.project()->tracks.size();d.app.add_audio_track("History fixture");d.refresh();d.undo.onClick();check(d.project()->tracks.size()==trackCount,"Toolbar Undo");d.redo.onClick();check(d.project()->tracks.size()==trackCount+1,"Toolbar Redo");
    const auto beforeClick=d.app.click_settings();d.clickButton.onClick();check(d.app.click_settings().playback!=beforeClick.playback,"Metronome original callback");d.app.set_click_settings(beforeClick);
    auto settings=beforeClick;settings.count_bars=1;d.app.set_click_settings(settings);
    const auto token=juce::String::fromUTF8(mrs::new_id().value.c_str());
    const auto wav=juce::File::getCurrentWorkingDirectory().getChildFile("shell-record-"+token+".wav");
    const auto armed=d.project()->tracks.front().id;d.app.set_track_input(armed,0);d.app.set_track_armed(armed,true);
    d.app.start_recording(std::filesystem::path(wav.getFullPathName().toWideCharPointer()));flush();
    check(d.app.recording()&&d.displayState().count_remaining&&!d.navigationMode.isEnabled()&&d.record.getToggleState()&&d.shellStatus.getText().startsWith("COUNT-IN"),"Actual count-in/recording restrictions");
    save(d.createComponentSnapshot(d.getLocalBounds()),"state-count-in");
    for(int i=0;i<1600&&d.displayState().count_remaining;++i)flush();
    check(d.shellStatus.getText().startsWith("RECORDING"),"Actual recording status");save(d.createComponentSnapshot(d.getLocalBounds()),"state-recording");
    d.record.onClick();flush();check(!d.app.recording()&&d.navigationMode.isEnabled(),"Existing Record stop control");
    d.message="Fixture notice / check routing";d.refreshShell();check(d.shellStatus.getText().startsWith("NOTICE")&&d.shellStatus.getTooltip().contains(d.message),"Notice visible without fabricated severity");save(d.createComponentSnapshot(d.getLocalBounds()),"state-notice");d.message.clear();
    check(device->opens==1,"Shell commands did not reopen the device");d.app.disconnect();d.refresh();check(d.shellStatus.getText().contains("Disconnected"),"Disconnected is not Ready");save(d.createComponentSnapshot(d.getLocalBounds()),"state-disconnected");
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("shell-project-"+token+".mrsproject");
    d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));const auto saved=d.project();
    d.app.open_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.refresh(true);check(*d.project()==*saved,"Project round-trip unchanged");
    const auto revision=d.app.services().projects->state().revision;double maximum{};
    for(int i=0;i<100;++i){const auto start=std::chrono::steady_clock::now();d.refreshShell();maximum=juce::jmax(maximum,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());}
    for(int i=0;i<12;++i){d.setShellDesign(false);check(&d.play.getLookAndFeel()==&d.theme,"Theme borrower detached");d.setShellDesign(true);}
    check(d.app.services().projects->state().revision==revision,"Repeated shell changes do not mutate model");
    d.showProjectHome(true);check(!d.shellStatus.isVisible()&&!d.play.isVisible(),"Accepted Home visibility intact");d.showProjectHome(false);
    d.setShellDesign(false);d.removeFromDesktop();
    juce::File::getCurrentWorkingDirectory().getChildFile("shell205-results.txt").replaceWithText("Shell state/commands/focus/Space/recording/history/save-open/isolation/teardown PASS\nUnchanged content pixel pairs="+juce::String(pairs)+"\nmax UI status refresh us="+juce::String(maximum,3)+"\nPhysical ASIO/xrun/mixed-DPI/screen reader NOT TESTED. No audio callback code changed.\n");
}
}
