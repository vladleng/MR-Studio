#include "Desktop.h"
namespace ui {
namespace {
juce::String csvName(juce::String name){return "\""+name.replace("\"","\"\"")+"\"";}
juce::String number(std::uint64_t n){return juce::String(static_cast<juce::int64>(n));}
juce::String profilingReport(Desktop& owner,bool csv){
    const auto e=owner.app.engine();const auto p=e->profile();const auto m=e->metrics();
    juce::String text=csv?"scope,name,calls,frames,total_ns,max_ns\n":"Profiling "+juce::String(p.enabled?"ON":"OFF")+" | cumulative while enabled; counters belong to each prepared object\n";
    auto row=[&](juce::String scope,juce::String name,mrs::TimingSample t){
        if(csv)text+=scope+","+csvName(name)+","+number(t.calls)+","+number(t.frames)+","+number(t.total_ns)+","+number(t.max_ns)+"\n";
        else if(t.calls)text+=scope+" / "+name+": mean "+juce::String(static_cast<double>(t.total_ns)/static_cast<double>(t.calls)/1000,2)+" us; max "+juce::String(t.max_ns/1000.,2)+" us; "+number(t.calls)+" calls\n";
    };
    auto value=[&](juce::String name,std::uint64_t n){if(csv)text+="metric,"+csvName(name)+","+number(n)+",0,0,0\n";else text+=name+" "+number(n)+"  ";};
    if(csv)text+="metadata,"+csvName("Device: "+juce::String::fromUTF8(owner.app.audio_name().c_str()))+",0,0,0,0\n";
    value("sample_rate",e->config().sample_rate);value("B_min",m.min_frames);value("B_max",m.max_frames);value("Workers",m.processing_workers);
    value("Late",m.deadline_misses);value("XR",m.input_underflows+m.input_overflows+m.output_underflows+m.output_overflows);value("Disk_misses",m.disk_underruns);
    value("Process",m.process_buffer_frames);value("Ahead_buffered",m.ahead_buffered_frames);value("Ahead_underruns",m.ahead_underruns);value("Ahead_invalidations",m.ahead_invalidations);value("Ahead_max_ns",m.ahead_max_process_ns);value("Worker_timeouts",m.worker_timeouts);
    if(!csv)text+="\nCallback wall time: p50/p95/p99 load "+juce::String(m.p50_load_percent,1)+" / "+juce::String(m.p95_load_percent,1)+" / "+juce::String(m.p99_load_percent,1)+"%; max "+juce::String(m.max_callback_ns/1000.,2)+" us\nJob wall times overlap across workers; their sum is not process CPU utilization.\nDependency estimate excludes source, reduction, scheduling and queue waits.\n\n";
    if(csv){text+="metric,Callback_p50_load_percent,"+juce::String(m.p50_load_percent,2)+",0,0,0\nmetric,Callback_p95_load_percent,"+juce::String(m.p95_load_percent,2)+",0,0,0\nmetric,Callback_p99_load_percent,"+juce::String(m.p99_load_percent,2)+",0,0,0\n";}
    value("Callback_max_ns",m.max_callback_ns);value("PDC_samples",e->compensation().output);value("Profile_enabled",p.enabled?1:0);
    if(!csv)text+="\n";
    row("device","Dependency cost estimate",p.device_path);row("ahead","Dependency cost estimate",p.ahead_path);
    row("device","Master/shared inserts",p.device_master);row("ahead","Master/shared inserts",p.ahead_master);
    const auto project=owner.project();std::vector<const mrs::Track*> tracks;for(const auto& track:project->tracks)if(track.kind!=mrs::TrackKind::midi)tracks.push_back(&track);
    for(std::size_t i=0;i<p.channels;++i){const auto name=i<tracks.size()?juce::String::fromUTF8(tracks[i]->name.c_str()):"Channel "+juce::String(static_cast<int>(i+1));row("device",name,p.device[i]);row("ahead",name,p.ahead[i]);}
    for(std::size_t i=0;i<8;++i){row("device-worker",number(i),p.device_workers[i]);row("ahead-worker",number(i),p.ahead_workers[i]);}
    const auto graphs=e->profile_graphs();
    for(std::size_t i=0;i<graphs.size();++i)if(graphs[i]){const auto timings=graphs[i]->profile();const auto& nodes=graphs[i]->snapshot().graph->nodes;
        for(std::size_t n=0;n<nodes.size();++n){juce::String name=juce::String::fromUTF8(nodes[n].processor_id.c_str());
            const auto& effects=i<tracks.size()?tracks[i]->inserts:project->master_inserts;
            for(const auto& effect:effects)if(effect.id==nodes[n].id&&!effect.plugin_name.empty())name=juce::String::fromUTF8(effect.plugin_name.c_str());
            row("insert-"+number(i),name,timings[n]);}}
    return text;
}
class ProfilingPanel final:public juce::Component,private juce::Timer {
public:
    explicit ProfilingPanel(Desktop& d):owner(d){setSize(940,600);setTitle("Engine profiling");
        addAndMakeVisible(enabled);addAndMakeVisible(exportButton);addAndMakeVisible(report);
        enabled.setTitle("Enable timing diagnostics");enabled.setToggleState(owner.app.engine()->profile().enabled,juce::dontSendNotification);
        enabled.onClick=[this]{owner.app.engine()->set_profiling(enabled.getToggleState());update();};
        report.setMultiLine(true);report.setReadOnly(true);report.setScrollbarsShown(true);report.setTitle("Engine profiling snapshot");
        exportButton.onClick=[this]{const auto snapshot=profilingReport(owner,true);juce::Component::SafePointer<Desktop> safe(&owner);
            owner.choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,snapshot](const juce::File& file){if(safe&&!file.withFileExtension("csv").replaceWithText(snapshot))throw std::runtime_error("Cannot write profiling CSV");},"*.csv");};
        update();startTimerHz(2);
    }
    ~ProfilingPanel() override{stopTimer();}
    void resized() override{enabled.setBounds(12,10,350,30);exportButton.setBounds(getWidth()-152,10,140,30);report.setBounds(12,50,getWidth()-24,getHeight()-62);}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));}
    void stopRefresh(){stopTimer();}
    void update(){report.setText(profilingReport(owner,false),false);}
    Desktop& owner;juce::ToggleButton enabled{"Enable timing diagnostics (adds overhead)"};juce::TextButton exportButton{"Export CSV..."};juce::TextEditor report;
private:
    void timerCallback() override{update();}
};
}
void Desktop::showProfiling(){for(auto& w:windows)if(w->isVisible()&&w->getName()=="Engine profiling"){w->toFront(true);return;}
    auto* profilePanel=new ProfilingPanel(*this);auto w=std::make_unique<EditorWindow>("Engine profiling",profilePanel);w->setLookAndFeel(&theme);w->onClose=[this,profilePanel]{profilePanel->stopRefresh();app.engine()->set_profiling(false);};windows.push_back(std::move(w));}
void profilingSmoke(Desktop& d){const auto revision=d.app.services().projects->state().revision;
    d.action(41);auto* profilePanel=dynamic_cast<ProfilingPanel*>(d.windows.back()->getContentComponent());if(!profilePanel)throw std::runtime_error("profiling menu opens profilePanel");
    profilePanel->enabled.setToggleState(true,juce::dontSendNotification);profilePanel->enabled.onClick();std::array<float,256> output{};for(int n=0;n<4;++n)d.app.engine()->process(nullptr,output.data(),128);profilePanel->update();
    if(!d.app.engine()->profile().enabled||!profilePanel->report.getText().contains("Profiling ON")||!profilingReport(d,true).contains("scope,name,calls"))throw std::runtime_error("profiling control/report binding");
    if(d.app.services().projects->state().revision!=revision)throw std::runtime_error("profiling changes project history");
    auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-profiling-preview.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(profilePanel->createComponentSnapshot(profilePanel->getLocalBounds(),true,1.f,juce::SoftwareImageType{}),*stream);}
    auto scaled=juce::File::getCurrentWorkingDirectory().getChildFile("juce-profiling-150-preview.png").createOutputStream();if(scaled){scaled->setPosition(0);scaled->truncate();juce::PNGImageFormat().writeImageToStream(profilePanel->createComponentSnapshot(profilePanel->getLocalBounds(),true,1.5f,juce::SoftwareImageType{}),*scaled);}
    const auto csv=juce::File::getCurrentWorkingDirectory().getChildFile("juce-profiling-smoke.csv");if(!csv.replaceWithText(profilingReport(d,true))||!csv.loadFileAsString().contains("Device:"))throw std::runtime_error("profiling CSV persistence");
    d.windows.back()->closeButtonPressed();if(d.app.engine()->profile().enabled)throw std::runtime_error("closing diagnostics disables profiling");
}
}
