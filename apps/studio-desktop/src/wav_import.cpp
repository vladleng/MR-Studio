#include <mrs/desktop.hpp>
#include <mrs/wav_resample.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <stop_token>
#include <stdexcept>

namespace mrs::desktop {
namespace fs=std::filesystem;
namespace {
void require_import(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
std::string import_text(const fs::path& value){const auto bytes=value.generic_u8string();return {bytes.begin(),bytes.end()};}
bool in_media(const fs::path& file,const fs::path& root){
    if(root.empty())return false;
    const auto relative=file.lexically_relative(fs::weakly_canonical(root/"Media"));
    return !relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..";
}
void copy_import(const fs::path& source,const fs::path& destination,std::stop_token stop,
    const std::function<void(float)>& progress){
    std::ifstream input(source,std::ios::binary);std::ofstream output(destination,std::ios::binary);
    if(!input||!output)throw std::runtime_error("Cannot stage WAV import");
    std::array<char,65536> block{};
    const auto total=fs::file_size(source);std::uint64_t copied{};
    while(input){
        if(stop.stop_requested())throw std::runtime_error("WAV import cancelled");
        input.read(block.data(),static_cast<std::streamsize>(block.size()));
        output.write(block.data(),input.gcount());
        if(!output)throw std::runtime_error("Cannot stage WAV import (disk full?)");
        copied+=static_cast<std::uint64_t>(input.gcount());
        if(progress&&total)progress(static_cast<float>(static_cast<double>(copied)/static_cast<double>(total)));
    }
    if(!input.eof())throw std::runtime_error("Cannot read WAV import source");
    output.close();if(!output)throw std::runtime_error("Cannot finish staged WAV");
}
}
struct Application::PreparedWavImport {
    struct Entry {fs::path original,file;std::shared_ptr<const audio::AudioData> data;bool staged{};};
    std::vector<Entry> entries;
    fs::path staging;
    ~PreparedWavImport(){if(!staging.empty()){std::error_code ignored;fs::remove_all(staging,ignored);}}
    static std::shared_ptr<PreparedWavImport> prepare(const std::vector<fs::path>& paths,
        std::uint32_t rate,const fs::path& root,std::size_t budget,std::stop_token stop,
        std::atomic<float>* progress){
        auto result=std::make_shared<PreparedWavImport>();
        std::vector<audio::WavFile> inspected;
        for(const auto& path:paths){
            if(stop.stop_requested())throw std::runtime_error("WAV import cancelled");
            require_import(fs::is_regular_file(path),"WAV source is missing or not a regular file");
            inspected.push_back(audio::inspect_wav(path));
        }
        std::map<fs::path,Entry> prepared;
        for(std::size_t index=0;index<paths.size();++index){
            if(stop.stop_requested())throw std::runtime_error("WAV import cancelled");
            const auto original=fs::canonical(paths[index]);
            if(const auto at=prepared.find(original);at!=prepared.end()){
                auto entry=at->second;entry.original=paths[index];result->entries.push_back(std::move(entry));continue;
            }
            const auto size=fs::file_size(original);const auto modified=fs::last_write_time(original);
            const bool convert=inspected[index].sample_rate!=rate;
            const bool staged=convert||(!root.empty()&&!in_media(original,root));
            const auto report=[progress,index,total=paths.size()](float fraction){
                if(progress)progress->store((static_cast<float>(index)+fraction)/static_cast<float>(total),std::memory_order_relaxed);
            };
            auto file=original;
            if(staged){
                if(result->staging.empty()){
                    const auto folder=(root.empty()?fs::temp_directory_path():root)/(".mrs-import-"+new_id().value);
                    if(!fs::create_directory(folder))throw std::runtime_error("Cannot create exclusive WAV staging directory");
                    result->staging=folder;
                }
                file=result->staging/("item-"+std::to_string(index)+".wav");
                if(convert)audio::resample_wav_file(original,file,rate,stop,report);
                else copy_import(original,file,stop,report);
            }
            if(stop.stop_requested())throw std::runtime_error("WAV import cancelled");
            require_import(fs::file_size(original)==size&&fs::last_write_time(original)==modified,
                "Source WAV changed during import; drop it again");
            auto data=std::make_shared<const audio::AudioData>(audio::open_wav(file,std::min<std::size_t>(8*1024*1024,budget)));
            require_import(data->sample_rate==rate,"Converted WAV rate mismatch");
            budget-=data->samples.size()*sizeof(float);
            Entry entry{paths[index],file,std::move(data),staged};
            prepared.emplace(original,entry);result->entries.push_back(std::move(entry));
            if(progress)progress->store(static_cast<float>(index+1)/static_cast<float>(paths.size()),std::memory_order_relaxed);
        }
        return result;
    }
};
struct Application::WavImportJob {
    std::stop_source stop;
    std::atomic<float> progress{0};
    std::future<std::shared_ptr<PreparedWavImport>> result;
    std::shared_ptr<IProjectStore> projects;
    std::uint64_t revision{};
    fs::path project_path;
    std::optional<Id> target;
    Sample start{};
};
struct Application::PlaybackPreparation {
    std::stop_source stop;
    std::atomic<float> progress{};
    std::future<std::shared_ptr<PreparedWavImport>> pending;
    std::shared_ptr<PreparedWavImport> media;
    std::uint32_t rate{};
    std::vector<fs::path> paths;
    std::vector<std::pair<std::uintmax_t,fs::file_time_type>> stamps;
    std::string error;
};
std::shared_ptr<Application::PlaybackPreparation> Application::prepare_playback(const std::vector<fs::path>& paths){
    require_import(workspace_==Workspace::live&&path_.empty(),"Performance preparation requires a Live runtime");
    require_import(paths.size()<=32,"Live Playback supports up to 32 WAV stems");
    // Retain cancelled jobs until their worker has finished; never join them in selection handlers.
    std::erase_if(playback_preparations_,[&](const auto& job){
        return job.use_count()==1&&!playback_preparation(job).busy;
    });
    require_import(playback_preparations_.size()<6,"Previous performance preparation is still cancelling");
    auto job=std::make_shared<PlaybackPreparation>();job->rate=services_.projects->state().project->sample_rate;job->paths=paths;
    for(const auto& path:paths){require_import(fs::is_regular_file(path),"Playback WAV is missing");job->stamps.emplace_back(fs::file_size(path),fs::last_write_time(path));}
    if(paths.empty()){job->media=std::make_shared<PreparedWavImport>();job->progress=1;}
    else job->pending=std::async(std::launch::async,[paths,rate=job->rate,token=job->stop.get_token(),progress=&job->progress]{
        return PreparedWavImport::prepare(paths,rate,{},128*1024*1024,token,progress);
    });
    playback_preparations_.push_back(job);return job;
}
Application::WavImportStatus Application::playback_preparation(const std::shared_ptr<PlaybackPreparation>& job){
    if(!job)return {false,0,0,"No preparation"};
    if(job->pending.valid()&&job->pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{job->media=job->pending.get();}catch(const std::exception& error){job->error=error.what();}
    }
    return {job->pending.valid(),job->progress.load(std::memory_order_relaxed),0,job->error};
}
void Application::cancel_playback(const std::shared_ptr<PlaybackPreparation>& job){if(job)job->stop.request_stop();}
void Application::activate_playback(const std::shared_ptr<PlaybackPreparation>& job,const PlaybackLayout& layout){
    require_import(job&&!playback_preparation(job).busy&&job->media&&job->error.empty()&&!job->stop.stop_requested(),"Playback preparation is not ready");
    require_import(job->rate==services_.projects->state().project->sample_rate,"Playback preparation rate changed");
    require_import(layout.stems.size()==job->media->entries.size(),"Prepared stem mapping mismatch");
    try {
        for(std::size_t i=0;i<job->paths.size();++i)
            require_import(fs::file_size(job->paths[i])==job->stamps[i].first&&fs::last_write_time(job->paths[i])==job->stamps[i].second,"Prepared WAV changed; select the item again to reload it");
    }catch(const std::exception& error){job->error=error.what();throw;}
    Project candidate;candidate.id=new_id();candidate.title="MR Live Playback";candidate.sample_rate=job->rate;
    candidate.time=layout.time;candidate.chords=layout.chords;candidate.sections=layout.sections;
    std::map<std::string,std::shared_ptr<const audio::AudioData>> decoded;
    for(std::size_t i=0;i<layout.stems.size();++i){
        const auto& entry=job->media->entries[i];const auto& settings=layout.stems[i];
        require_import(std::isfinite(settings.begin)&&std::isfinite(settings.end)&&settings.begin>=0&&settings.begin<=static_cast<double>(max_sample)/job->rate&&settings.end>=-1&&settings.end<=static_cast<double>(max_sample)/job->rate,"Invalid Playback trim");
        const auto begin=static_cast<Sample>(std::llround(settings.begin*job->rate));
        const auto end=settings.end<0?entry.data->frames():static_cast<Sample>(std::llround(settings.end*job->rate));
        require_import(begin>=0&&end>begin&&end<=entry.data->frames(),"Playback trim is outside its WAV");
        auto data=entry.data;
        if(data->file){auto replacement=*data;auto metadata=std::make_shared<audio::WavFile>(*data->file);metadata->lifetime=job->media;replacement.file=std::move(metadata);data=std::make_shared<const audio::AudioData>(std::move(replacement));}
        const auto source=import_text(entry.file);decoded[source]=data;
        Track track{new_id(),import_text(entry.original.stem()),TrackKind::audio,{}};track.mix=settings.mix;track.hardware_outputs=settings.outputs;
        candidate.clips.push_back({new_id(),track.id,import_text(entry.original.filename()),begin,end-begin,begin,source});
        candidate.tracks.push_back(std::move(track));
    }
    candidate.validate();install_playback(std::move(candidate),std::move(decoded),job->media,layout.click);
}
void Application::validate_wav_import(std::size_t count,std::optional<Id> target,Sample start) const {
    require_not_playing();require_not_recording();
    require_import(count>0&&count<=audio::max_voices,"Select 1..128 WAV files");
    const auto current=services_.projects->state().project;
    require_import(current->clips.size()+count<=audio::max_voices,"Too many playback clips");
    require_import(start>=0,"Invalid WAV drop position");
    if(target){const auto at=std::find_if(current->tracks.begin(),current->tracks.end(),[&](const auto& track){return track.id==*target;});
        require_import(at!=current->tracks.end()&&at->kind==TrackKind::audio,"Drop samples on an audio track or empty arrangement");}
}
std::shared_ptr<Application::PreparedWavImport> Application::prepare_wav_import_sync(const std::vector<fs::path>& paths){
    std::size_t used{};for(const auto& [key,cached]:assets_){(void)key;used+=cached.data->samples.size()*sizeof(float);}
    require_import(used<=512*1024*1024,"Project audio cache exceeds 512 MiB");
    return PreparedWavImport::prepare(paths,services_.projects->state().project->sample_rate,
        path_.empty()?fs::path{}:asset_root_,512*1024*1024-used,{},nullptr);
}
void Application::import_wavs(const std::vector<fs::path>& paths,std::optional<Id> target,Sample start){
    require_import(!wav_import_job_,"Finish or cancel the current WAV import first");
    validate_wav_import(paths.size(),target,start);
    auto prepared=prepare_wav_import_sync(paths);commit_wav_import(prepared,target,start);
}
void Application::commit_wav_import(const std::shared_ptr<PreparedWavImport>& prepared,std::optional<Id> target,Sample start){
    validate_wav_import(prepared->entries.size(),target,start);
    std::unique_ptr<MediaCopy> copies;if(!path_.empty())copies=std::make_unique<MediaCopy>(asset_root_);
    std::map<fs::path,fs::path> adopted;
    std::vector<Track> tracks;std::vector<Clip> clips;
    std::map<std::string,std::shared_ptr<const audio::AudioData>> decoded;
    for(const auto& entry:prepared->entries){
        auto file=entry.file;
        if(copies){
            if(const auto at=adopted.find(file);at!=adopted.end())file=at->second;
            else{
                const auto destination=entry.staged?copies->adopt_import(file,entry.original.filename()):copies->media(file);
                adopted.emplace(file,destination);file=destination;
            }
        }
        const auto source=import_text(copies?file.lexically_relative(asset_root_):file);
        std::shared_ptr<const audio::AudioData> data;
        if(const auto cached_asset=assets_.find(source);cached_asset!=assets_.end())data=cached_asset->second.data;
        else if(const auto batch_asset=decoded.find(source);batch_asset!=decoded.end())data=batch_asset->second;
        else{
            require_import(assets_.size()+decoded.size()<128,"Retained source limit reached (128)");
            data=entry.data;
            if(data->file&&(data->file->path!=file||entry.staged)){
                auto replacement=*data;auto metadata=std::make_shared<audio::WavFile>(*data->file);
                metadata->path=file;if(entry.staged)metadata->lifetime=prepared;
                replacement.file=std::move(metadata);data=std::make_shared<const audio::AudioData>(std::move(replacement));
            }
            decoded.emplace(source,data);
        }
        require_import(data->sample_rate==services_.projects->state().project->sample_rate,"Project rate changed during WAV import");
        require_import(data->frames()<=std::numeric_limits<Sample>::max()-start,"WAV drop exceeds timeline range");
        const auto track=target?*target:new_id();
        if(!target)tracks.push_back({track,import_text(entry.original.stem()),TrackKind::audio,{}});
        clips.push_back({new_id(),track,import_text(entry.original.filename()),start,data->frames(),0,source});
        if(target)start+=data->frames();
    }
    // Allocate retention before publishing the command: Undo may retain these paths.
    import_staging_.push_back(prepared);
    const auto revision=services_.projects->state().revision;std::vector<std::string> cached;
    try{
        for(auto& [source,data]:decoded){cache_asset(source,std::move(data));cached.push_back(source);}
        edit(ImportAudio{std::move(tracks),std::move(clips)});
        if(copies)copies->commit();
        prepared->entries.clear();
        if(copies&&!prepared->staging.empty()){
            std::error_code ignored;
            if(fs::remove(prepared->staging,ignored))prepared->staging.clear();
        }
    }catch(...){
        if(services_.projects->state().revision!=revision){if(copies)copies->commit();}
        else{
            for(const auto& source:cached){*assets_.at(source).cancel=true;assets_.erase(source);}
            import_staging_.pop_back();
        }
        throw;
    }
}
void Application::queue_wav_import(const std::vector<fs::path>& paths,std::optional<Id> target,Sample start){
    require_import(!wav_import_job_,"Finish or cancel the current WAV import first");
    require_import(retired_imports_.size()<2,"Previous WAV import is still cancelling");
    validate_wav_import(paths.size(),target,start);
    std::size_t used{};for(const auto& [key,cached]:assets_){(void)key;used+=cached.data->samples.size()*sizeof(float);}
    require_import(used<=512*1024*1024,"Project audio cache exceeds 512 MiB");
    auto job=std::make_shared<WavImportJob>();job->projects=services_.projects;job->revision=services_.projects->state().revision;
    job->project_path=path_;job->target=target;job->start=start;
    const auto root=path_.empty()?fs::path{}:asset_root_;const auto rate=services_.projects->state().project->sample_rate;
    // Worker captures no Application, GUI, device or mutable command/model state.
    job->result=std::async(std::launch::async,[paths,rate,root,budget=512*1024*1024-used,
        token=job->stop.get_token(),progress=&job->progress]{return PreparedWavImport::prepare(paths,rate,root,budget,token,progress);});
    wav_import_job_=std::move(job);wav_import_status_.busy=true;wav_import_status_.progress=0;
    wav_import_status_.message="Preparing WAV import (Esc to cancel)";++wav_import_status_.revision;
}
Application::WavImportStatus Application::wav_import_status() const {
    auto result=wav_import_status_;if(wav_import_job_)result.progress=wav_import_job_->progress.load(std::memory_order_relaxed);return result;
}
void Application::cancel_wav_import(){
    if(!wav_import_job_)return;
    wav_import_job_->stop.request_stop();retired_imports_.push_back(std::move(wav_import_job_));
    wav_import_status_.busy=false;wav_import_status_.message="WAV import cancelled";++wav_import_status_.revision;
}
void Application::poll_wav_import(){
    std::erase_if(retired_imports_,[](auto& job){
        if(job->result.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
        try{(void)job->result.get();}catch(const std::exception&){}return true;
    });
    if(!wav_import_job_||wav_import_job_->result.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    auto job=std::move(wav_import_job_);wav_import_status_.busy=false;++wav_import_status_.revision;
    try{
        auto prepared=job->result.get();
        require_import(job->projects==services_.projects&&job->revision==services_.projects->state().revision&&job->project_path==path_,
            "WAV import cancelled: project changed; drop the files again");
        const auto count=prepared->entries.size();commit_wav_import(prepared,job->target,job->start);
        wav_import_status_.progress=1;wav_import_status_.message="Imported "+std::to_string(count)+" WAV file(s)";
    }catch(const std::exception& error){wav_import_status_.message=std::string("WAV import failed: ")+error.what();}
}
void Application::stop_wav_imports(){
    for(auto& job:playback_preparations_)job->stop.request_stop();
    for(auto& job:playback_preparations_)if(job->pending.valid())job->pending.wait();
    playback_preparations_.clear();
    if(wav_import_job_)wav_import_job_->stop.request_stop();
    for(auto& job:retired_imports_)job->stop.request_stop();
    if(wav_import_job_&&wav_import_job_->result.valid())wav_import_job_->result.wait();
    for(auto& job:retired_imports_)if(job->result.valid())job->result.wait();
    wav_import_job_.reset();retired_imports_.clear();
}
}
