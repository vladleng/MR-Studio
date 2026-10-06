#include "Desktop.h"
namespace ui {
namespace {
struct RecordDevice final:mrs::audio::IAudioDevice {
    mrs::audio::DevicePhase phase{mrs::audio::DevicePhase::closed};
    std::vector<mrs::audio::DeviceInfo> enumerate() override{return {{0,"MIDI record fixture",{}, {"L","R"},16,8192,128,1}};}
    void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{phase=mrs::audio::DevicePhase::open;}
    void start()override{phase=mrs::audio::DevicePhase::running;}void stop()override{phase=mrs::audio::DevicePhase::stopped;}void close()noexcept override{phase=mrs::audio::DevicePhase::closed;}mrs::audio::DeviceStatus status()override{return {phase,48000};}
};
juce::Button* armControl(juce::Component& component){if(auto* button=dynamic_cast<juce::Button*>(&component);button&&button->getTitle()=="Arm recording"&&button->isVisible())return button;for(auto* child:component.getChildren())if(auto* result=armControl(*child))return result;return nullptr;}
}
void midi4cSmoke(Desktop& d,const juce::File& fixture){
    const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    const auto log=juce::File::getCurrentWorkingDirectory().getChildFile("midi-record-smoke-stage.txt");log.replaceWithText("start\n");
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);const auto folder=temp.getNonexistentChildFile("mrs-midi-record-ui","",false);check(folder.createDirectory(),"record UI temp folder");const auto module=folder.getChildFile("instrument.vst3");check(fixture.copyFileTo(module),"record UI fixture copy");
    d.closeEditors();d.app.disconnect();d.app.new_project();const auto track=d.app.add_instrument_track("Recorded keys");d.selectedTrack=track;
    const auto info=mrs::processing::probe_vst3(module.getFullPathName().toStdString()).front();mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::vst3;fx.plugin_path=info.path;fx.class_id=info.class_id;fx.plugin_name=info.name;d.app.set_inserts(track,{fx});d.app.set_midi_input(track,"fixture:record",-1,true);d.app.connect(std::make_unique<RecordDevice>(),{0,48000,128,{}, {0,1},2});d.setWorkspace(mrs::desktop::Workspace::arrange);d.refresh(true);
    auto* arm=armControl(d);check(arm!=nullptr,"instrument arm visible");arm->onClick();check(d.app.track_armed(track),"instrument arm click");
    d.action(33);check(d.app.recording(),"Record menu starts MIDI without file dialog");std::array<float,256> out{};d.app.engine()->process(nullptr,out.data(),128);check(d.app.engine()->enqueue_live_midi({d.app.engine()->midi_generation(),0,{0,mrs::processing::MidiKind::note_on,0,60,110}}),"UI note ingress");d.app.engine()->process(nullptr,out.data(),128);d.refresh();
    const auto save=[&](const char* name,double scale){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,static_cast<float>(scale),juce::SoftwareImageType{});juce::FileOutputStream stream(juce::File::getCurrentWorkingDirectory().getChildFile(name));juce::PNGImageFormat format;check(format.writeImageToStream(image,stream),"record UI preview");};
    save("juce-midi-record-100-preview.png",1.);save("juce-midi-record-150-preview.png",1.5);
    d.action(32);check(!d.app.recording()&&d.project()->clips.size()==1&&d.project()->clips.front().midi->notes.size()==1,"Stop commits recorded MIDI");d.app.engine()->process(nullptr,out.data(),128);d.app.poll();d.action(10);check(d.project()->clips.empty(),"record single Undo");d.action(11);check(d.project()->clips.size()==1,"record Redo");d.app.disconnect();d.app.new_project();d.refresh(true);
    check(folder.getParentDirectory()==temp&&folder.getFileName().startsWith("mrs-midi-record-ui"),"record cleanup scope");folder.deleteRecursively();log.appendText("passed\n");
}
}
