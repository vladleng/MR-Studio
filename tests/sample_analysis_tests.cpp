#include <mrs/sample_library.hpp>
#include <mrs/sample_preview.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <thread>

namespace {
namespace fs=std::filesystem;
using namespace mrs::library;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Pred> auto wait(SampleLibrary& lib,Pred predicate){
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(std::chrono::steady_clock::now()<until){auto snap=lib.snapshot();if(snap->phase==Phase::error)throw std::runtime_error(snap->message);if(predicate(*snap))return snap;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    const auto snap=lib.snapshot();
    throw std::runtime_error("Smart analysis worker timeout: assets="+std::to_string(snap->visible_assets)+
        ", analysis="+std::to_string(snap->analysis.completed)+"/"+std::to_string(snap->analysis.total)+
        ", running="+std::to_string(snap->analysis.running)+", paused="+std::to_string(snap->analysis.paused)+
        ", message="+snap->message);
}
void signal(const fs::path& path,int kind,double bpm=120.,int root=0,bool minor=false,double seconds=0.){
    fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);
    constexpr int rate=8000;const int frames=static_cast<int>(std::lround((seconds>0?seconds:kind==0||kind==3?480./bpm:4.)*rate));
    const auto put=[&](std::uint32_t x,int width){for(int i=0;i<width;++i)out.put(static_cast<char>((x>>(8*i))&255));};
    out.write("RIFF",4);put(static_cast<std::uint32_t>(36+frames*2),4);out.write("WAVEfmt ",8);put(16,4);put(1,2);put(1,2);put(rate,4);put(rate*2,4);put(2,2);put(16,2);out.write("data",4);put(static_cast<std::uint32_t>(frames*2),4);
    std::uint32_t noise=18341;
    for(int i=0;i<frames;++i){const double t=static_cast<double>(i)/rate;double value{};
        if(kind==0){const auto phase=std::fmod(t,60./bpm);value=.7*std::exp(-phase*28.)*std::sin(2.*std::numbers::pi*111.*t);}
        else if(kind==1)value=.7*std::exp(-t*5.)*std::sin(2.*std::numbers::pi*173.*t);
        else if(kind==2){noise=noise*1664525u+1013904223u;value=(static_cast<double>(noise)/4294967295.-.5)*.6;}
        else if(kind==5){for(double at:{0.,.38,1.24,1.61,2.77,3.47})if(t>=at)value+=.55*std::exp(-(t-at)*30.)*std::sin(2.*std::numbers::pi*137.*t);}
        else if(kind==6)value=.4*std::sin(2.*std::numbers::pi*261.63*std::pow(2.,root/12.)*t);
        else if(kind==3){
            const auto beat=static_cast<int>(t*bpm/60.);const int chord=beat<3?0:beat<4?5:beat<5?7:0;
            for(int interval:{0,minor?3:4,7}){const auto note=60+root+chord+interval;value+=.19*std::sin(2.*std::numbers::pi*440.*std::pow(2.,(note-69)/12.)*t);}
            value*=.4+.6*std::exp(-std::fmod(t,60./bpm)*12.);
        }
        const auto pcm=static_cast<std::int16_t>(std::clamp(value,-1.,1.)*32767.);put(static_cast<std::uint16_t>(pcm),2);
    }
}
}
void audio_analysis256_tests(){
    using namespace mrs::library;
    const auto root=fs::temp_directory_path()/("mrs256-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{fs::path root;~Cleanup(){std::error_code ec;fs::remove_all(root,ec);}}cleanup{root};
    std::size_t exact{},octave{},unknown{},wrong{},typeCorrect{};double absoluteError{};
    std::ofstream report("library256-benchmark.csv");report<<"kind,truth,bpm,key,state,confidence,type,reason\n";
    for(double bpm:{60.,75.,90.,100.,120.,128.,140.,160.,180.}){
        const auto path=root/("T"+std::to_string(static_cast<int>(bpm))+".wav");signal(path,0,bpm);const auto f=analyse_wav_features(path);
        double error=1.e9;for(const auto candidate:f.tempo_candidates)error=std::min(error,std::abs(candidate-bpm));
        if(f.bpm==0)++unknown;else if(error>bpm*.025)++wrong;else {++octave;if(std::abs(f.bpm-bpm)<bpm*.025)++exact;absoluteError+=error;}
        typeCorrect+=f.type.label=="Loop"?1u:0u;
        report<<"pulse,"<<bpm<<','<<f.bpm<<','<<f.key<<','<<f.state<<','<<f.bpm_confidence<<','<<f.type.label<<','<<f.reason<<'\n';
    }
    check(octave>=8&&wrong==0,"Tempo audit must retain the labelled tempo in half/double candidates without confident wrong estimates");
    std::size_t keys{},keyWrong{},keyUnknown{};
    constexpr std::array<const char*,12> names={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    for(int tonic:{0,2,5,7,9})for(bool minor:{false,true}){
        const auto path=root/("K"+std::to_string(tonic)+(minor?"m":"M")+".wav");signal(path,3,120.,tonic,minor);const auto f=analyse_wav_features(path);
        const auto truth=std::string(names[static_cast<std::size_t>(tonic)])+(minor?"m":" major");
        if(f.key.empty())++keyUnknown;else if(f.key==truth)++keys;else ++keyWrong;
        report<<"tonal,"<<truth<<','<<f.bpm<<','<<f.key<<','<<f.state<<','<<f.key_confidence<<','<<f.type.label<<','<<f.reason<<'\n';
    }
    for(int kind:{1,2,4,5,6}){const auto path=root/("N"+std::to_string(kind)+".wav");signal(path,kind);const auto f=analyse_wav_features(path);check(f.bpm==0&&f.key.empty(),"Isolated/irregular hits, single pitch, noise and silence must not acquire invented BPM/key");report<<"negative,"<<kind<<','<<f.bpm<<','<<f.key<<','<<f.state<<",0,"<<f.type.label<<','<<f.reason<<'\n';}
    std::cout<<"#256 synthetic audit: BPM exact="<<exact<<"/9, octave-aware="<<octave<<"/9, unknown="<<unknown<<", wrong="<<wrong<<", mean error="<<(octave?absoluteError/static_cast<double>(octave):0.)<<"; Type="<<typeCorrect<<"/9; key exact="<<keys<<"/10, unknown="<<keyUnknown<<", wrong="<<keyWrong<<"\n";
    // Results are reported, including unknown/incorrect keys; a small constructed
    // set cannot establish accuracy on commercial or user libraries.
    check(keys>=3&&keyWrong==0,"Key audit rejects confident errors while retaining useful tonal-loop detections");
    signal(root/"S1.wav",6,120.,0,false,.05);signal(root/"S2.wav",6,120.,36,false,.05);
    const auto dark=analyse_wav_features(root/"S1.wav"),bright=analyse_wav_features(root/"S2.wav");
    check(bright.centroid>dark.centroid+1000.&&dark.type.label=="One-shot"&&dark.key.empty(),"Short One-shots receive useful spectral features without acquiring a key");
    {
    SampleLibrary lib(root/"catalog.sqlite");lib.add_source(root);
    auto snap=wait(lib,[](const auto& v){return v.phase==Phase::idle&&!v.analysis.running&&v.visible_assets==26;});
    const auto reference=std::find_if(snap->assets.begin(),snap->assets.end(),[](const auto& a){return a.relative_path=="T120.wav";});check(reference!=snap->assets.end(),"Find signal analysis reference");
    std::cout<<"#256 catalogue reference: audio BPM="<<reference->analysis.bpm<<", detected BPM="<<reference->detected_bpm<<", source="<<reference->bpm_source<<"\n";
    const auto refId=reference->id;LibraryQuery query;query.similar_asset_id=refId;query.limit=3;lib.browse(query);
    snap=wait(lib,[&](const auto& v){return v.query==query;});check(snap->visible_assets>3&&snap->assets.size()==3&&snap->assets.front().id!=refId,"Feature similarity ranks a bounded page and excludes its reference");
    query.similar_asset_id=0;query.bpm_min=119;query.bpm_max=121;lib.browse(query);snap=wait(lib,[&](const auto& v){return v.query==query;});check(snap->visible_assets>=1,"BPM filters include signal estimates");
    lib.save_search("Rhythmic",query);snap=wait(lib,[](const auto& v){return v.saved_searches.size()==1;});check(snap->saved_searches.front().query.bpm_min==119,"Saved search retains musical filters");
    const auto searchId=snap->saved_searches.front().id;lib.delete_search(searchId);wait(lib,[](const auto& v){return v.saved_searches.empty();});
    lib.set_bpm({refId},137);lib.set_key({refId},"Gm");LibraryQuery manual;manual.search="T120";lib.browse(manual);
    snap=wait(lib,[&](const auto& v){return v.query==manual&&!v.assets.empty()&&v.assets.front().manual_bpm==137&&v.assets.front().manual_key=="Gm";});
    lib.analyse_audio(true);snap=wait(lib,[](const auto& v){return v.analysis.running;});
    const auto beforeBrowse=std::chrono::steady_clock::now();LibraryQuery active;active.search="T128";lib.browse(active);wait(lib,[&](const auto& v){return v.query==active;});
    check(std::chrono::steady_clock::now()-beforeBrowse<std::chrono::milliseconds(500),"Active audio analysis must not hold the browsing worker");
    lib.pause_analysis(true);snap=wait(lib,[](const auto& v){return v.analysis.paused;});
    const auto completed=snap->analysis.completed;LibraryQuery browse; browse.search="T120";lib.browse(browse);snap=wait(lib,[&](const auto& v){return v.query==browse;});check(snap->visible_assets==1,"Browsing remains available while analysis is paused");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));check(lib.snapshot()->analysis.completed<=completed+1,"Pause bounds in-flight completion and stops further work");
    lib.pause_analysis(false);snap=wait(lib,[](const auto& v){return !v.analysis.running;});
    check(snap->assets.front().manual_bpm==137&&snap->assets.front().manual_key=="Gm"&&snap->assets.front().bpm_source=="manual","Reanalysis preserves manual musical metadata and its provenance");
    browse.tags={{"Type","Loop"}};browse.excluded_tags={{"Genre","Jazz"}};lib.save_search("Retained",browse);wait(lib,[](const auto& v){return v.saved_searches.size()==1;});
    lib.analyse_audio(true);wait(lib,[](const auto& v){return v.analysis.running;});lib.cancel_analysis();wait(lib,[](const auto& v){return !v.analysis.running;});
    for(int i=0;i<70;++i)signal(root/"batch"/("Q"+std::to_string(i)+".wav"),4);
    lib.rescan();snap=wait(lib,[](const auto& v){return !v.sources.empty()&&v.sources.front().available==96&&!v.analysis.running&&v.analysis.total==70;});
    check(snap->analysis.completed==70,"Bounded analysis queue refills beyond its 64-job capacity");
    lib.analyse_audio(true);wait(lib,[](const auto& v){return v.analysis.running;});lib.pause_analysis(true);wait(lib,[](const auto& v){return v.analysis.paused;});
    signal(root/"batch"/"Q69.wav",0);LibraryQuery changed;changed.search="Q69";lib.browse(changed);wait(lib,[&](const auto& v){return v.query==changed;});
    lib.pause_analysis(false);snap=wait(lib,[](const auto& v){return !v.analysis.running;});
    check(snap->assets.size()==1&&snap->assets.front().analysis.state=="error"&&snap->assets.front().analysis.reason.find("changed")!=std::string::npos,"Changed-in-flight files produce an explicit error instead of stale signal metadata");
    lib.rescan();snap=wait(lib,[](const auto& v){return !v.analysis.running&&!v.assets.empty()&&v.assets.front().analysis.state=="complete";});
    check(snap->assets.front().detected_bpm>0,"Rescan recovers a changed-file analysis error");
    }
    {SampleLibrary lib(root/"catalog.sqlite");const auto snap=wait(lib,[](const auto& v){return v.phase==Phase::idle&&!v.analysis.running;});
        check(snap->saved_searches.size()==1&&snap->saved_searches.front().query.search=="T120"&&snap->saved_searches.front().query.tags.size()==1&&snap->saved_searches.front().query.excluded_tags.size()==1,"Combined saved searches survive restart");}
}
