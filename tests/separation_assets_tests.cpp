#include <mrs/desktop.hpp>
#include <mrs/sha256.hpp>
#include <bit>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

namespace {
using namespace mrs;namespace fs=std::filesystem;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>void rejects(F f){bool rejected{};try{f();}catch(const std::exception&){rejected=true;}check(rejected,"Expected rejection");}
std::string text(const fs::path& p){const auto s=p.generic_u8string();return {s.begin(),s.end()};}
void write(const fs::path& p,std::string_view s){std::ofstream f(p,std::ios::binary);f.write(s.data(),static_cast<std::streamsize>(s.size()));check(static_cast<bool>(f),"Write fixture");}
void wav(const fs::path& path,std::uint32_t frames,std::uint32_t rate=48000){
    std::ofstream o(path,std::ios::binary);const auto put=[&](std::uint32_t v,int n){for(int i=0;i<n;++i)o.put(static_cast<char>((v>>(8*i))&255));};
    o.write("RIFF",4);put(36+frames*8,4);o.write("WAVEfmt ",8);put(16,4);put(3,2);put(2,2);put(rate,4);put(rate*8,4);put(8,2);put(32,2);o.write("data",4);put(frames*8,4);
    for(std::uint32_t i=0;i<frames*2;++i)put(std::bit_cast<std::uint32_t>(i%2?.25f:-.25f),4);
    check(static_cast<bool>(o),"Write FLOAT WAV");
}
struct Temp{fs::path root=fs::temp_directory_path()/("mrs-separation-assets-"+new_id().value);Temp(){fs::create_directory(root);}~Temp(){std::error_code ec;fs::remove_all(root,ec);}};
struct EditClip final:ICommand{Clip clip;explicit EditClip(Clip c):clip(std::move(c)){}std::string_view name()const override{return "Fixture clip range";}void apply(Project& p)const override{p.clips.front()=clip;}};
SeparationResult result(const fs::path& root,const Clip& clip,int stems=2){
    fs::create_directories(root);wav(root/"vocals.wav",4800);wav(root/"instrumental.wav",4800);write(root/"manifest.json","{\"fixture\":true}");
    SeparationResult r;r.id=new_id();r.source_clip=clip.id;r.source=clip.source;r.source_sha256=sha256_file(fs::path(std::u8string(clip.source.begin(),clip.source.end())));r.model="official-htdemucs-derived-2-v1";r.model_version="955717e8";r.provenance="{\"fixture\":true}";
    r.source_offset=clip.source_offset;r.source_frames=clip.length;r.start=clip.start;r.frames=clip.length;r.fade_in=clip.fade_in;r.fade_out=clip.fade_out;r.source_rate=r.sample_rate=48000;
    const auto f=[&](const char* role,const char* name){const auto path=root/name;return SeparationFile{role,text(path),sha256_file(path),fs::file_size(path)};};
    r.manifest=f("manifest","manifest.json");r.stems={f("Vocals","vocals.wav"),f("Instrumental","instrumental.wav")};
    if(stems>2){r.stems.clear();for(const auto* label:{"Vocals","Drums","Bass","Other","Guitar","Piano"}){if(stems==4&&(std::string(label)=="Guitar"||std::string(label)=="Piano"))continue;auto name=std::string(label);std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});name+=".wav";wav(root/name,4800);r.stems.push_back(f(label,name.c_str()));}}
    return r;
}
std::vector<std::shared_ptr<const audio::AudioData>> prepare(desktop::Application& app,const SeparationResult& r){std::vector<std::shared_ptr<const audio::AudioData>> out;for(const auto& f:r.stems)out.push_back(std::make_shared<const audio::AudioData>(audio::open_wav(app.source_file(f.source),0)));return out;}
void hashes(){
    check(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","Empty SHA256");
    check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","ABC SHA256");
    check(sha256(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","Multiblock SHA256");
}
void lifecycle(int stems){
    Temp temp;const auto original=temp.root/"original.wav";wav(original,9600);const auto originalHash=sha256_file(original);
    desktop::Application app;app.import_wav(original);auto clip=app.services().projects->state().project->clips.front();clip.start=12345;clip.source_offset=1200;clip.length=4800;clip.fade_in=48;clip.fade_out=96;app.services().projects->execute(EditClip{clip});
    const auto r=result(temp.root/"job",clip,stems);app.rename_track(clip.track,"Unrelated track edit while processing");const auto initial=*app.services().projects->state().project;
    auto changed=clip;changed.start++;rejects([&]{RegisterSeparation{changed,r}.apply(*std::make_shared<Project>(initial));});
    auto rate=initial;rate.sample_rate=44100;rejects([&]{RegisterSeparation{clip,r}.apply(rate);});
    app.keep_separation(clip,r,{});check(app.services().projects->state().project->clips==initial.clips&&app.services().projects->state().project->tracks==initial.tracks,"Hidden result created audible tracks");
    check(app.services().projects->state().project->separation_results.size()==1,"Keep result");
    check(app.undo()&&app.services().projects->state().project->separation_results.empty(),"Undo hidden registration");
    check(app.redo()&&app.services().projects->state().project->separation_results.front()==r,"Redo preserves manifest and IDs");
    check(serialize(initial).starts_with("MRS_CORE_SNAPSHOT 32\n"),"Plain projects retain previous wire version");
    check(serialize(*app.services().projects->state().project).starts_with("MRS_CORE_SNAPSHOT 33\n"),"Separated projects use schema 33");
    auto round=deserialize(serialize(*app.services().projects->state().project));check(round==*app.services().projects->state().project,"Core separation roundtrip");
    auto legacy=serialize(initial);const auto header=legacy.find('\n');legacy.replace(0,header,"MRS_CORE_SNAPSHOT 32");check(deserialize(legacy).separation_results.empty(),"Old schema migration");
    app.place_separation(r,prepare(app,r));const auto placed=*app.services().projects->state().project;
    check(placed.tracks.size()==initial.tracks.size()+static_cast<std::size_t>(stems)&&placed.clips.size()==initial.clips.size()+static_cast<std::size_t>(stems),"Place requested stems");
    for(std::size_t i=1;i<placed.clips.size();++i)check(placed.clips[i].start==clip.start&&placed.clips[i].length==clip.length&&placed.clips[i].source_offset==0&&placed.clips[i].fade_in==48&&placed.clips[i].fade_out==96,"Stem alignment/fades");
    check(placed.clips.front()==clip,"Source clip modified");check(app.undo()&&app.services().projects->state().project->clips==initial.clips,"Placement is one Undo");
    check(app.redo()&&app.services().projects->state().project->clips==placed.clips,"Placement Redo stable IDs");
    const auto project=temp.root/"saved"/"song.mrsproject";app.save_project(project);
    const auto saved=persistence::load_project(project);check(saved.project.separation_results.size()==1,"Archive roundtrip");
    for(const auto& f:saved.project.separation_results.front().stems){check(fs::path(f.source).is_relative()&&f.source.starts_with("song.assets/separation/"),"Durable relative separation reference");check(sha256_file(project.parent_path()/fs::path(f.source))==f.sha256,"Durable asset checksum");}
    const auto moved=temp.root/"relocated"/"other.mrsproject";app.save_project(moved);
    check(sha256_file(original)==originalHash,"Original WAV changed");
    fs::remove_all(project.parent_path());fs::remove_all(temp.root/"job");fs::remove(original);
    desktop::Application reopened;reopened.open_project(moved);auto loaded=reopened.services().projects->state().project->separation_results.front();
    reopened.place_separation(loaded,prepare(reopened,loaded));check(reopened.services().projects->state().project->clips.back().start==12345,"Save As/moved source alignment");
    reopened.remove_separation(loaded.id);check(reopened.services().projects->state().project->separation_results.empty(),"Remove hidden record");check(reopened.undo()&&reopened.services().projects->state().project->separation_results.size()==1,"Undo cleanup");
    for(int i=0;i<1000&&!reopened.waveform(loaded.stems.front().source);++i){reopened.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    reopened.disconnect();
    const auto missing=reopened.source_file(loaded.stems.front().source);
    std::ifstream before(missing,std::ios::binary);std::string pristine{std::istreambuf_iterator<char>(before),{}};before.close();auto damaged=pristine;damaged.back()^=1;write(missing,damaged);rejects([&]{reopened.save_project(moved);});write(missing,pristine);fs::remove(missing);
    rejects([&]{(void)prepare(reopened,loaded);});rejects([&]{reopened.save_project(moved);});
    check(persistence::load_project(moved).project.separation_results.size()==1,"Failed save corrupted archive");
}
void source_guard(){
    Temp t;const auto input=t.root/"external-44100.wav";wav(input,4410,44100);
    desktop::Application app;app.new_project(48000);app.import_wavs({input});
    const auto source=app.source_file(app.services().projects->state().project->clips.front().source);
    check(source!=input&&fs::exists(source),"Converted import fixture");
    auto guard=app.separation_media_guard();app.new_project(48000);
    check(fs::exists(source),"Project switch retired a source still owned by the separation worker");
    guard.reset();check(!fs::exists(source),"Converted source lease was not released");
}
void hidden_history_saveas(){
    Temp t;wav(t.root/"original.wav",9600);desktop::Application app;app.import_wav(t.root/"original.wav");auto c=app.services().projects->state().project->clips.front();c.length=4800;app.services().projects->execute(EditClip{c});auto r=result(t.root/"job",c);app.keep_separation(c,r,{});
    app.remove_separation(r.id);app.save_project(t.root/"one"/"one.mrsproject");app.save_project(t.root/"two"/"two.mrsproject");fs::remove_all(t.root/"one");fs::remove_all(t.root/"job");
    check(app.undo(),"Undo removal after Save As");app.place_separation(r,prepare(app,r));check(app.services().projects->state().project->clips.size()==3,"Undo-only media survived relocation");
    // Foreign destination content must not be overwritten, even for a valid record.
    const auto destination=t.root/"collision";fs::create_directories(destination/"collision.assets"/"separation"/r.id.value);write(destination/"collision.assets"/"separation"/r.id.value/"vocals.wav","foreign");
    rejects([&]{app.save_project(destination/"collision.mrsproject");});std::ifstream input(destination/"collision.assets"/"separation"/r.id.value/"vocals.wav");std::string s;input>>s;check(s=="foreign","Foreign file overwritten");
}
}
int main(){try{hashes();lifecycle(2);lifecycle(4);lifecycle(6);source_guard();hidden_history_saveas();std::cout<<"PASS separation assets: SHA256 vectors, stale guard, hidden/placement Undo/Redo, schema migration, Save/Open/Save As/moved project, missing media, foreign-file preservation\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
