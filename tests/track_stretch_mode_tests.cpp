#include <mrs/arrangement.hpp>
#include <mrs/stretch_cache.hpp>
#include <iostream>
#include <stdexcept>
#include <algorithm>
using namespace mrs;
void check(bool b){if(!b)throw std::runtime_error("channel stretch check failed");}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected);}
int main(){try{
    auto p=demo_project();p.clips.resize(1);p.clips[0].length=48000;
    const auto id=p.tracks[0].id;stretch::Parameters params;params.duration_ratio=.75;
    SetAudioStretch{p.clips[0].id,params}.apply(p);p.validate();ProjectStore store(p);
    store.execute(SetTrackStretchBackend{id,stretch::Backend::drums});
    check(store.state().project->tracks[0].stretch_backend==stretch::Backend::drums);
    check(store.state().project->tracks[1].stretch_backend==stretch::Backend::signalsmith);
    check(deserialize(serialize(*store.state().project))==*store.state().project);
    check(store.undo());check(store.state().project->tracks[0].stretch_backend==stretch::Backend::signalsmith);check(store.redo());
    auto legacy=serialize(p);legacy.replace(legacy.find("MRS_CORE_SNAPSHOT 32"),20,"MRS_CORE_SNAPSHOT 31");
    for(std::size_t at;(at=legacy.find("STRETCH_MODE "))!=std::string::npos;)legacy.erase(at,legacy.find('\n',at)-at+1);
    check(deserialize(legacy)==p);
    auto bad=params;bad.semitones=1;rejects([&]{store.execute(SetAudioStretch{p.clips[0].id,bad});});
    rejects([&]{store.execute(SetTrackStretchBackend{p.tracks[2].id,stretch::Backend::drums});});
    auto source=std::make_shared<audio::AudioData>();source->channels=2;source->samples.resize(96000);source->samples[9600]=1;source->samples[9601]=-1;
    const auto& clip=p.clips[0];
    check(audio::stretch_cache_key(*source,clip)!=audio::stretch_cache_key(*source,clip,stretch::Backend::drums));
    const auto drumsKey=audio::stretch_cache_key(*source,clip,stretch::Backend::drums);
    check(drumsKey.find("0.1.1-experimental;grain24;search12;attack5")!=std::string::npos);
    auto cache=audio::prepare_stretch_cache(source,clip,{}, {},stretch::Backend::drums);
    check(cache->data->frames()==36000);auto wav=audio::load_wav(cache->data->file->path);
    check(wav.samples[7200]==1&&wav.samples[7201]==-1);
    std::stop_source stop;stop.request_stop();rejects([&]{audio::prepare_stretch_cache(source,clip,stop.get_token(),{},stretch::Backend::drums);});
    std::cout<<"PASS channel independence, default, save/load, legacy, undo/redo, unsupported parameters, cache identity, Drums worker PCM and cancellation\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
