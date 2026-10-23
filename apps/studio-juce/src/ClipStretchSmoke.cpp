#include "Desktop.h"
#include "ClipTimingMenu.h"
#include <windows.h>
#include <chrono>
#include <thread>

namespace ui {
void clipStretchSmoke(){
    const auto check=[](bool value,const char* text){if(!value)throw std::runtime_error(text);};
    Desktop owner(true);owner.stopTimer();owner.setSize(1200,800);owner.setVisible(true);owner.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    const auto id=owner.project()->clips.front().id;owner.app.trim_clip(id,0,48000);const auto original=*owner.project();
    const auto pump=[](){for(int pass=0;pass<20;++pass){MSG message{};int count{};while(count++<200&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
    const auto dialog=[&](){owner.editClipStretch(id);auto* alert=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());check(alert!=nullptr,"Time / Pitch dialog");return alert;};
    auto* alert=dialog();check(alert->getComboBoxComponent("enabled")->getSelectedId()==1,"Processing defaults enabled");
    for(const auto* field:{"ratio","pitch","cents","shift","base"})check(alert->getTextEditor(field)!=nullptr,"Numeric stretch control");
    for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"stretch-dialog-100.png":"stretch-dialog-150.png").createOutputStream();check(stream&&juce::PNGImageFormat().writeImageToStream(alert->createComponentSnapshot(alert->getLocalBounds(),true,scale),*stream),"Stretch dialog preview");}
    alert->exitModalState(0);pump();check(*owner.project()==original,"Cancel preserves project");
    alert=dialog();alert->getTextEditor("ratio")->setText("1.25");alert->getTextEditor("pitch")->setText("+7");alert->exitModalState(1);pump();
    check(owner.project()->clips.front().stretch&&owner.project()->clips.front().length==60000,"Apply binds time / pitch command");
    check(owner.project()->clips.front().stretch->parameters.semitones==7,"Signed pitch input");
    check(owner.app.undo()&&*owner.project()==original,"Dialog edit undo");check(owner.app.redo(),"Dialog edit redo");
    alert=dialog();alert->getComboBoxComponent("enabled")->setSelectedId(2);alert->exitModalState(1);pump();check(owner.project()->clips.front().length==48000,"Bypass restores duration");
    alert=dialog();alert->exitModalState(2);pump();check(!owner.project()->clips.front().stretch,"Reset removes state");
    const auto before=*owner.project();alert=dialog();alert->getTextEditor("ratio")->setText("nan");alert->exitModalState(1);pump();check(*owner.project()==before,"Invalid input preserves project");
    // Menu fields preview locally and commit one command per wheel burst.
    auto stored=[&](){const auto p=owner.project();return *std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});};
    int submissions{};
    auto submit=[&](const mrs::Clip& expected,mrs::stretch::Parameters params,std::optional<double> tempo,bool fit)->std::optional<mrs::Clip>{
        if(fit)params.duration_ratio=*tempo/mrs::project_tempo_at(*owner.project(),expected.start);
        owner.app.set_clip_timing(expected,params,tempo,expected.start);++submissions;return stored();
    };
    const auto menuBaseline=*owner.project();
    auto panel=std::make_unique<ClipTimingMenu>(owner.theme,stored(),submit);panel->setSize(370,142);
    const auto now=juce::Time::getCurrentTime();
    auto wheelEvent=[&](juce::Component& c,int modifiers=0){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{10,10},juce::ModifierKeys(modifiers),1,0,0,0,0,&c,&c,now,{10,10},now,0,false);};
    juce::MouseWheelDetails wheel;wheel.deltaY=.1f;
    for(int i=0;i<5;++i)panel->speed.mouseWheelMove(wheelEvent(panel->speed.editor),wheel);
    check(*owner.project()==menuBaseline&&submissions==0,"Wheel burst is buffered");
    check(std::abs(panel->speed.editor.value()-1.05)<1e-9,"Hover wheel changes Speedup");
    panel->transpose.mouseWheelMove(wheelEvent(panel->transpose.editor),wheel);
    panel->tune.mouseWheelMove(wheelEvent(panel->tune.editor,juce::ModifierKeys::ctrlModifier),wheel);
    for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"stretch-dialog-menu-100.png":"stretch-dialog-menu-150.png").createOutputStream();check(stream&&juce::PNGImageFormat().writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds(),true,scale),*stream),"Menu preview");}
    panel->flush();check(submissions==1&&stored().stretch->parameters.semitones==1&&std::abs(stored().stretch->parameters.cents-.1)<1e-9,"One menu command binds pitch and fine tune");
    panel.reset();check(owner.app.undo()&&*owner.project()==menuBaseline,"Wheel burst is one Undo");
    // Pending edits survive popup component destruction before its close callback.
    panel=std::make_unique<ClipTimingMenu>(owner.theme,stored(),submit);
    panel->tempo.editor.setValue(105);panel->tempo.editor.onCommit(105);
    auto finish=panel->finish();panel.reset();finish();check(stored().file_tempo==105&&!stored().stretch,"Menu-close flush preserves BPM without creating processing state");
    check(owner.app.undo()&&*owner.project()==menuBaseline,"BPM metadata Undo");
    panel=std::make_unique<ClipTimingMenu>(owner.theme,stored(),submit);
    panel->tempo.editor.setValue(105);panel->tempo.editor.onCommit(105);panel->flush(true);
    check(stored().file_tempo==105&&stored().length==42000,"Fit 105 BPM source to 120 BPM project");
    panel.reset();check(owner.app.undo()&&*owner.project()==menuBaseline,"Tempo fit is one Undo");
    // Metadata must retain an exact ratio and bypass, including reciprocal rounding.
    auto bypassed=mrs::stretch::Parameters{};bypassed.enabled=false;bypassed.duration_ratio=1.23456789012345;bypassed.semitones=7;
    owner.app.set_clip_stretch(id,bypassed);const auto bypassBaseline=*owner.project();
    panel=std::make_unique<ClipTimingMenu>(owner.theme,stored(),submit);panel->tempo.editor.setValue(99);panel->tempo.editor.onCommit(99);panel->flush();
    check(stored().stretch->parameters==bypassed&&stored().file_tempo==99,"BPM edit retains exact ratio and bypass");panel.reset();
    check(owner.app.undo()&&*owner.project()==bypassBaseline,"BPM on bypass Undo");check(owner.app.undo()&&*owner.project()==menuBaseline,"Restore menu baseline");
    // Actual Arrangement gestures: Alt edges stretch, plain edges trim, body Alt copies.
    owner.app.move_clip(id,stored().track,48000);owner.refresh(true);owner.snap=false;
    auto& arrangement=*owner.arrangement;arrangement.pixelsPerSecond=120.;
    auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&arrangement,&arrangement,now,point,now,1,true);};
    const int altLeft=juce::ModifierKeys::altModifier|juce::ModifierKeys::leftButtonModifier;
    auto gesture=[&](bool left,float distance,bool cancel=false,bool alt=true){
        const auto rect=arrangement.clipRect(stored());const juce::Point<float> point{left?rect.getX()+2:rect.getRight()-2,rect.getCentreY()};
        const auto revision=owner.app.services().projects->state().revision;
        arrangement.mouseDown(mouse(point,alt?altLeft:juce::ModifierKeys::leftButtonModifier));arrangement.mouseDrag(mouse(point.translated(distance,0),alt?altLeft:juce::ModifierKeys::leftButtonModifier));
        check(owner.app.services().projects->state().revision==revision,"Edge preview does not edit project");
        if(cancel)arrangement.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        arrangement.mouseUp(mouse(point.translated(distance,0),0));
    };
    const auto dragBaseline=*owner.project();
    owner.snap=true;const auto snapRevision=owner.app.services().projects->state().revision;gesture(false,2);check(owner.app.services().projects->state().revision==snapRevision,"Snap to original edge creates no Undo");owner.snap=false;
    gesture(false,60);
    check(stored().start==48000&&stored().length==72000&&stored().stretch,"Alt right edge stretches without moving start");
    check(owner.app.undo()&&*owner.project()==dragBaseline,"Alt right edge one Undo");check(owner.app.redo(),"Alt edge Redo");
    const auto stretchedClip=stored();gesture(true,30);
    check(stored().start+stored().length==stretchedClip.start+stretchedClip.length&&stored().length==60000,"Alt left edge keeps right edge");
    check(owner.app.undo()&&stored()==stretchedClip,"Alt left edge one Undo");
    const auto cancelBaseline=*owner.project();gesture(true,-30,true);check(*owner.project()==cancelBaseline,"Escape cancels Alt stretch");
    check(owner.app.undo()&&*owner.project()==dragBaseline,"Restore plain trim baseline");
    gesture(false,-30,false,false);check(!stored().stretch&&stored().length==36000,"Plain edge still trims");check(owner.app.undo(),"Trim Undo");
    const auto copyBaseline=*owner.project();const auto rect=arrangement.clipRect(stored());const auto centre=rect.getCentre();
    arrangement.mouseDown(mouse(centre,altLeft));arrangement.mouseDrag(mouse(centre.translated(30,0),altLeft));arrangement.mouseUp(mouse(centre.translated(30,0),0));
    check(owner.project()->clips.size()==copyBaseline.clips.size()+1,"Alt body still copies");check(owner.app.undo()&&*owner.project()==copyBaseline,"Copy Undo");
    owner.removeFromDesktop();
}
} // namespace ui
