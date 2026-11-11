#include <mrs/desktop.hpp>
#include <mrs/sha256.hpp>
#include <fstream>
#include <iostream>
#include <source_location>
#include <thread>

namespace {
using namespace mrs;
using namespace mrs::transcription;
namespace fs=std::filesystem;
void check(bool b,const std::source_location where=std::source_location::current()){if(!b)throw std::runtime_error("Transcription controller assertion at line "+std::to_string(where.line()));}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught);}
void write(const fs::path& file,std::string_view s){std::ofstream out(file,std::ios::binary);out.write(s.data(),static_cast<std::streamsize>(s.size()));check(out.good());}
void wav(const fs::path& file,int mode=0){
    std::string b="RIFF";const auto le=[&](unsigned n,int count){for(int i=0;i<count;++i)b+=static_cast<char>((n>>(8*i))&255);};
    le(32036,4);b+="WAVEfmt ";le(16,4);le(1,2);le(1,2);le(16000,4);le(32000,4);le(2,2);le(16,2);b+="data";le(32000,4);b.resize(32044,0);b[44]=static_cast<char>(mode);write(file,b);
}
Project fixture(const fs::path& source){
    Project p;p.id=new_id();p.sample_rate=16000;p.time.tempos={{0,120},{3*ppq,60}};
    Track t;t.id=new_id();t.name="Source";p.tracks.push_back(t);
    Clip c;c.id=new_id();c.track=t.id;c.name="Fixture";const auto u=source.generic_u8string();c.source={u.begin(),u.end()};c.start=16000;c.length=16000;p.clips.push_back(c);p.validate();return p;
}
bool busy(JobState s){return s==JobState::running||s==JobState::cancelling;}
template<class Poll,class Status>void wait(Poll poll,Status status){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(6);
    while(busy(status().state)){check(std::chrono::steady_clock::now()<deadline);poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
}
struct Manual final:audio::IAudioDevice {
    audio::DevicePhase phase{audio::DevicePhase::closed};bool fail{};
    std::vector<audio::DeviceInfo> enumerate()override{return {{0,"Transcription fixture",{}, {"L","R"},16,8192,128,1}};}
    void control_panel(int)override{}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>)override{phase=audio::DevicePhase::open;}
    void start()override{if(fail){fail=false;throw std::runtime_error("Fixture start failure");}phase=audio::DevicePhase::running;}
    void stop()override{phase=audio::DevicePhase::stopped;}
    void close()noexcept override{phase=audio::DevicePhase::closed;}
    audio::DeviceStatus status()override{return {phase,16000,0,0,0,{}};}
};
}
int main(int argc,char** argv){try{
    check(argc==2);const auto root=fs::temp_directory_path()/("mrs-transcription-controller "+new_id().value);fs::create_directory(root);
    struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{root};
    const auto source_model=root/"authorized fixture";fs::create_directory(source_model);
    write(source_model/"config.json","{}");write(source_model/"model.safetensors","fixture");write(source_model/"README.md","Self-generated test terms");
    ModelSpec spec{"fixture-model",std::string(40,'a'),"Self-generated fixture terms",{}};
    for(const auto* name:{"config.json","model.safetensors","README.md"})spec.files.push_back({name,sha256_file(source_model/name),fs::file_size(source_model/name)});
    ModelStore models(root/"models",spec);rejects([&]{models.acquire();});rejects([&]{models.install(source_model,false);});check(!fs::exists(root/"models"));
    std::stop_source stopped;stopped.request_stop();rejects([&]{models.install(source_model,true,stopped.get_token());});check(!fs::exists(models.directory()));
    models.install(source_model,true);models.verify();rejects([&]{models.install(source_model,true);});
    {const auto lease=models.acquire();Request req;lease.configure(req);check(req.model_id==spec.id&&req.weights_sha256==spec.files[1].sha256);auto copy=models;rejects([&]{copy.remove();});rejects([&]{copy.install(source_model,true);});}
    write(models.directory()/"model.safetensors","changed");rejects([&]{models.verify();});models.remove();check(!fs::exists(models.directory())&&sha256_file(source_model/"model.safetensors")==spec.files[1].sha256);
    models.install(source_model,true);write(models.directory()/"unrelated.txt","keep");rejects([&]{models.remove();});check(fs::exists(models.directory()/"unrelated.txt"));fs::remove(models.directory()/"unrelated.txt");
    auto bad=spec;bad.id="../unsafe";rejects([&]{ModelStore invalid(root/"bad",bad);});
    bad=spec;bad.files[1].sha256=std::string(64,'0');ModelStore wrong(root/"wrong",bad);rejects([&]{wrong.install(source_model,true);});check(!fs::exists(wrong.directory()));
    check(model_download_target("https://huggingface.co:443/model").authorize);
    check(!model_download_target("https://cas-bridge.xethub.hf.co/file?signature=fixture").authorize);
    check(model_download_redirect("https://huggingface.co/path","/api/config")=="https://huggingface.co/api/config");
    for(const auto* url:{"http://huggingface.co/file","https://huggingface.co.evil/file","https://user@huggingface.co/file","https://huggingface.co:444/file","https://evil.invalid/file","https://huggingface.co/file#fragment"})rejects([&]{model_download_target(url);});
    rejects([]{model_download_redirect("https://huggingface.co/file","//evil.invalid/file");});
    rejects([]{authenticated_model_fetch("hf_token\r\nInjected: header");});
    ModelStore downloaded(root/"downloaded",spec);const auto downloads=root/"downloads";bool fetched=false;std::uint64_t downloaded_bytes{};
    const ModelFetch fetch=[&](const std::string& url,const fs::path& target,std::uint64_t bytes,std::stop_token stop,const ModelDownloadProgress& progress){check(!stop.stop_requested()&&model_download_target(url).authorize&&url.find(spec.revision)!=std::string::npos);fetched=true;fs::copy_file(source_model/target.filename(),target);if(progress)progress(bytes,bytes);};
    rejects([&]{downloaded.download(downloads,false,fetch);});check(!fetched);
    downloaded.download(downloads,true,fetch,{},[&](auto now,auto total){check(now<=total&&now>=downloaded_bytes);downloaded_bytes=now;});downloaded.verify();check(fetched&&fs::is_empty(downloads));downloaded.remove();
    rejects([&]{downloaded.download(downloads,true,[&](const auto&,const fs::path& target,auto,std::stop_token,const ModelDownloadProgress&){write(target,"corrupt");});});check(!fs::exists(downloaded.directory())&&fs::is_empty(downloads));
    const auto source=root/"source.wav";wav(source);const auto jobs=root/"jobs";fs::create_directory(jobs);
    Request settings;settings.executable=fs::absolute(argv[1]);settings.executable_sha256=sha256_file(settings.executable);settings.temporary_root=jobs;settings.timeout=std::chrono::seconds(3);
    auto project=fixture(source);std::shared_ptr<IProjectStore> store=std::make_shared<ProjectStore>(project);Controller controller;
    const auto launch=[&]{controller.start(store,{},project.clips[0].id,source,models,settings);};
    const auto poll=[&]{controller.poll(store,{});};const auto status=[&]{return controller.status();};
    launch();rejects(launch);wait(poll,status);check(status().state==JobState::ready&&status().completed==1&&status().total==1);
    check(controller.preview()->notes.size()==1&&fs::is_empty(jobs));
    rejects(launch); // a preview must be explicitly placed/discarded
    rejects([&]{controller.prepare(store,{},std::vector<std::string>{});});rejects([&]{controller.prepare(store,{},std::vector<std::string>{"voice"});});
    auto command=controller.prepare(store,{},std::vector<std::string>{"acoustic_piano"});store->execute(command);controller.placed();
    const auto converted=*store->state().project;check(converted.clips.size()==2&&converted.clips[0]==project.clips[0]);
    check(converted.clips[1].midi->notes[0].length==3*ppq/4&&converted.clips[1].midi->notes[0].start==ppq/2);
    check(store->undo()&&*store->state().project==project);check(store->redo()&&*store->state().project==converted);check(store->undo());
    // Preview and role selection concern the chosen clip range, not other file regions.
    auto trimmed=project;trimmed.clips[0].source_offset=12000;trimmed.clips[0].length=4000;trimmed.validate();
    std::shared_ptr<IProjectStore> trim_store=std::make_shared<ProjectStore>(trimmed);Controller crop;
    crop.start(trim_store,{},trimmed.clips[0].id,source,models,settings);
    wait([&]{crop.poll(trim_store,{});},[&]{return crop.status();});check(crop.status().state==JobState::empty&&crop.preview()->notes.empty());crop.cancel();
    trimmed.clips[0].source_offset=8000;trimmed.clips[0].length=8000;trimmed.validate();trim_store=std::make_shared<ProjectStore>(trimmed);
    crop.start(trim_store,{},trimmed.clips[0].id,source,models,settings);wait([&]{crop.poll(trim_store,{});},[&]{return crop.status();});
    check(crop.status().state==JobState::ready&&crop.preview()->notes.size()==1&&crop.preview()->notes[0].start==.75&&crop.preview()->analysis_seconds==.5&&crop.preview()->source_offset_seconds==.5);
    auto cropped=crop.prepare(trim_store,{});trim_store->execute(cropped);crop.placed();check(trim_store->state().project->clips[1].midi->notes[0].start==ppq/2);
    // ABA: equal Project content after Undo must still reject the captured revision.
    launch();store->execute(RenameTrack{project.tracks[0].id,"Edited"});check(store->undo());wait(poll,status);check(status().state==JobState::stale&&*store->state().project==project);
    rejects([&]{controller.prepare(store,{});});
    launch();auto replacement=std::make_shared<ProjectStore>(project);wait([&]{controller.poll(replacement,{});},status);check(status().state==JobState::stale);
    launch();wait([&]{controller.poll(store,root/"SaveAs.mrsproject");},status);check(status().state==JobState::stale);
    launch();wait(poll,status);wav(source,9);poll();check(status().state==JobState::stale);wav(source);
    // Cancellation is nonblocking; no new job until the old process has reaped.
    wav(source,1);launch();const auto began=std::chrono::steady_clock::now();controller.cancel();check(std::chrono::steady_clock::now()-began<std::chrono::milliseconds(100));rejects(launch);wait(poll,status);check(status().state==JobState::cancelled&&fs::is_empty(jobs));
    wav(source,5);launch();wait(poll,status);check(status().state==JobState::empty);rejects([&]{controller.prepare(store,{});});controller.cancel();
    wav(source,1);launch();controller.shutdown();check(fs::is_empty(jobs));wav(source);
    // Source media and model lease outlive cancel until the process has reaped.
    wav(source,1);auto media=std::make_shared<int>(1);std::weak_ptr<int> weak_media=media;
    controller.start(store,{},project.clips[0].id,source,models,settings,media);media.reset();check(!weak_media.expired());
    const auto lease_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(fs::is_empty(jobs)){check(std::chrono::steady_clock::now()<lease_deadline);poll();check(busy(status().state));std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    rejects([&]{models.remove();});controller.cancel();wait(poll,status);check(weak_media.expired()&&fs::is_empty(jobs));wav(source);

    // Application publication uses the accepted MIDI graph/device path.
    desktop::Application app;app.new_project(16000);app.import_wavs({source},{},16000);
    auto device=std::make_unique<Manual>();auto* device_ptr=device.get();app.connect(std::move(device),{0,16000,128,{}, {0,1}});
    const auto clip=app.services().projects->state().project->clips[0].id;
    const auto before=*app.services().projects->state().project;const auto revision=app.services().projects->state().revision;
    settings.musical.import_tempo=true;settings.musical.tempo_map=true;settings.musical.quantize=true;settings.musical.bpm=90;
    app.queue_transcription(clip,models,settings);wait([&]{app.poll();},[&]{return app.transcription_status();});check(app.transcription_status().state==JobState::ready);
    check(!app.transcription_preview()->musical.quantize&&!app.transcription_preview()->musical.import_tempo&&!app.transcription_preview()->musical.tempo_map&&app.transcription_preview()->musical.bpm==0);
    device_ptr->fail=true;rejects([&]{app.place_transcription();});check(app.services().projects->state().revision==revision&&*app.services().projects->state().project==before&&app.audio_running());
    const auto placed=app.place_transcription();check(placed.size()==1&&app.transcription_status().state==JobState::placed);
    const auto after=*app.services().projects->state().project;check(after.clips[0]==before.clips[0]&&after.clips.size()==before.clips.size()+1&&after.time==before.time);
    check(app.undo()&&*app.services().projects->state().project==before);check(app.redo()&&*app.services().projects->state().project==after);
    // Archives canonicalize audio aliases to Media/...; compare the full persisted snapshot.
    rejects([&]{app.place_transcription();});const auto file=root/"saved.mrsproject";app.save_project(file);const auto saved=app.snapshot().project;app.open_project(file);check(*app.services().projects->state().project==saved);
    const auto second=root/"saved-as.mrsproject";app.save_project(second);const auto saved_as=app.snapshot().project;app.open_project(second);check(*app.services().projects->state().project==saved_as);
    app.queue_transcription(app.services().projects->state().project->clips[0].id,models,settings);app.new_project(16000);wait([&]{app.poll();},[&]{return app.transcription_status();});check(app.transcription_preview()==nullptr&&app.services().projects->state().project->clips.empty());
    app.disconnect();models.remove();check(!fs::exists(models.directory())&&fs::is_empty(jobs));
    std::cout<<"PASS transcription model store/controller/Application integration\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
