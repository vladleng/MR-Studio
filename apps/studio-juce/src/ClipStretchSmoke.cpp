#include "Desktop.h"
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
    const auto track=owner.project()->clips.front().track;
    owner.selectedTrack=track;if(!owner.inspectorPanelVisible())owner.toggleInspector();owner.refresh();
    auto* inspector=owner.findChildWithID("channel-inspector");
    auto* mode=inspector?dynamic_cast<juce::ComboBox*>(inspector->findChildWithID("inspector-stretch-mode")):nullptr;
    check(mode&&mode->isShowing()&&mode->getWidth()>0&&mode->getSelectedId()==1,"Inspector shows default Signalsmith model");
    {
        const auto menu=owner.channelMenu(track);juce::PopupMenu::MenuItemIterator it(menu,true);
        while(it.next())check(it.getItem().itemID!=72&&it.getItem().itemID!=73,"Track menu no longer contains model selection");
    }
    mode->setSelectedId(2,juce::sendNotificationSync);
    check(owner.app.clip_stretch_backend(owner.project()->clips.front())==mrs::stretch::Backend::drums,"Inspector selects Drums model");
    check(owner.app.undo(),"Channel mode undo");owner.refresh();check(mode->getSelectedId()==1,"Undo updates inspector");
    check(owner.app.redo(),"Channel mode redo");owner.refresh();check(mode->getSelectedId()==2,"Redo updates inspector");

    auto& arrange=*owner.arrangement;owner.snap=false;arrange.pixelsPerSecond=120;arrange.horizontal=0;arrange.vertical=0;arrange.resized();
    const auto clip=[&]{return owner.project()->clips.front();};
    const auto edge=[&](bool right){const auto r=arrange.clipRect(clip());return juce::Point<float>{right?r.getRight()-2:r.getX()+2,r.getCentreY()};};
    const auto mouse=[&](juce::Point<float> point,int mods){const auto now=juce::Time::getCurrentTime();
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&arrange,&arrange,now,point,now,1,true);};
    const int altDrag=juce::ModifierKeys::altModifier|juce::ModifierKeys::leftButtonModifier;
    const auto initial=*owner.project();const auto right=edge(true);const auto revision=owner.app.services().projects->state().revision;
    const auto alt=juce::ModifierKeys::altModifier;
    for(bool rightEdge:{false,true}){
        const auto point=edge(rightEdge);
        arrange.mouseMove(mouse(point,0));const auto bracket=arrange.getMouseCursor();
        arrange.mouseMove(mouse(point,alt));
        check(!arrange.stretchDragging()&&arrange.getMouseCursor()!=bracket,"Alt at the same edge position shows the clock before mouse down");
        arrange.mouseMove(mouse(point,0));
        check(arrange.getMouseCursor()==bracket,"Releasing Alt at the same edge position restores the trim cursor");
    }
    arrange.mouseMove(mouse(arrange.clipRect(clip()).getCentre(),alt));
    check(arrange.getMouseCursor()==juce::MouseCursor(juce::MouseCursor::NormalCursor),"Alt hover over the clip body does not show the clock");
    arrange.mouseMove(mouse(right,alt));const auto clock=arrange.getMouseCursor();
    arrange.mouseDown(mouse(right,altDrag));
    check(arrange.stretchDragging()&&arrange.getMouseCursor()==clock,"Mouse down starts stretching with the same clock cursor");
    arrange.mouseDrag(mouse(right.translated(60,0),juce::ModifierKeys::leftButtonModifier));
    check(arrange.getMouseCursor()==clock,"Releasing Alt after mouse down preserves the active stretch cursor");
    check(*owner.project()==initial&&owner.app.services().projects->state().revision==revision,"Stretch drag has no project edit before release");
    {
        auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("stretch-arrange-preview.png").createOutputStream();
        if(stream)stream->truncate();
        check(stream&&juce::PNGImageFormat().writeImageToStream(owner.createComponentSnapshot(owner.getLocalBounds()),*stream),"Arrange stretch and inspector preview");
    }
    arrange.mouseUp(mouse(right.translated(60,0),0));
    check(clip().start==0&&clip().length==72000&&clip().stretch->parameters.duration_ratio==1.5,"Right edge stretches with fixed left edge even if Alt was released");
    const auto stretched=*owner.project();const auto left=edge(false);
    arrange.mouseDown(mouse(left,altDrag));arrange.mouseDrag(mouse(left.translated(60,0),altDrag));arrange.mouseUp(mouse(left.translated(60,0),0));
    check(clip().start==24000&&clip().length==48000&&clip().stretch->parameters.duration_ratio==1,"Left edge shortens with the right edge fixed");
    check(owner.app.undo()&&*owner.project()==stretched,"One Undo restores left-edge stretch");
    check(owner.app.undo()&&*owner.project()==initial,"One Undo restores right-edge stretch");owner.refresh();
    arrange.mouseDown(mouse(right,altDrag));arrange.mouseDrag(mouse(right.translated(60,0),altDrag));
    check(arrange.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Escape handles in-progress stretch");
    arrange.mouseUp(mouse(right.translated(60,0),0));check(*owner.project()==initial,"Escape leaves the original clip untouched");
    arrange.mouseDown(mouse(right,altDrag));arrange.mouseUp(mouse(right,0));check(*owner.project()==initial,"An Alt-click without movement does not edit the clip");
    arrange.mouseDown(mouse(right,altDrag));arrange.mouseDrag(mouse(right.translated(480,0),altDrag));arrange.mouseUp(mouse(right.translated(480,0),0));
    check(clip().length==192000&&clip().stretch->parameters.duration_ratio==4,"Drums limits the gesture to its supported ratio");
    check(owner.app.undo()&&*owner.project()==initial,"Undo restores the clip after a clamped gesture");owner.refresh();
    const auto centre=arrange.clipRect(clip()).getCentre();
    arrange.mouseDown(mouse(centre,altDrag));arrange.mouseDrag(mouse(centre.translated(60,0),altDrag));arrange.mouseUp(mouse(centre.translated(60,0),0));
    check(owner.project()->clips.size()==initial.clips.size()+1&&!owner.project()->clips.back().stretch,"Alt-drag in clip body still copies");
    owner.removeFromDesktop();
}
} // namespace ui
