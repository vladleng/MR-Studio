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
    owner.removeFromDesktop();
}
} // namespace ui
