#include <mrs/desktop.hpp>
#include <algorithm>
#include <stdexcept>

namespace mrs::desktop {
std::shared_ptr<const audio::AudioData> Application::source_lifetime(const std::string& source) const {
    const auto at=assets_.find(source);return at==assets_.end()?nullptr:at->second.data;
}
std::shared_ptr<void> Application::separation_media_guard() const {
    return std::make_shared<std::vector<std::shared_ptr<void>>>(separation_media_);
}
std::filesystem::path Application::source_file(const std::string& source) const {
    if(source.empty()||source=="mrs:demo-tone")throw std::invalid_argument("Choose a file-backed audio clip");
    if(const auto found=owned_media_.find(source);found!=owned_media_.end())return found->second;
    auto file=std::filesystem::path(std::u8string(source.begin(),source.end()));
    return file.is_absolute()?file:asset_root_/file;
}
void Application::retain_separation_files(const SeparationResult& result){
    separation_files_.insert_or_assign(result.manifest.source,RetainedSeparation{result.id,result.manifest});
    for(const auto& file:result.stems)separation_files_.insert_or_assign(file.source,RetainedSeparation{result.id,file});
}
void Application::keep_separation(const Clip& expected,SeparationResult result,std::shared_ptr<void> media){
    require_not_recording();
    if(separation_media_.size()>=128)throw std::invalid_argument("Retained separation limit reached (128); reopen the saved project to release Undo media");
    const RegisterSeparation command{expected,result};
    auto candidate=*services_.projects->state().project;command.apply(candidate);candidate.validate();
    // Allocate ownership before publication. Undo/Redo keep the same file keys.
    auto retained=separation_files_;bool holding{};
    const auto revision=services_.projects->state().revision;
    try{separation_media_.push_back(std::move(media));holding=true;retain_separation_files(result);services_.projects->execute(command);}
    catch(...){if(services_.projects->state().revision==revision){if(holding)separation_media_.pop_back();separation_files_=std::move(retained);}throw;}
}
void Application::remove_separation(const Id& id){
    require_not_recording();services_.projects->execute(RemoveSeparation{id});
    // Do not unlink files: audible clips, saved projects and history own references.
}
void Application::place_separation(const SeparationResult& expected,
    std::vector<std::shared_ptr<const audio::AudioData>> prepared){
    require_not_playing();
    const auto project=services_.projects->state().project;
    if(std::find(project->separation_results.begin(),project->separation_results.end(),expected)==project->separation_results.end()||prepared.size()!=expected.stems.size())
        throw std::invalid_argument("Separation result changed; reopen its results");
    std::vector<Id> tracks,clips;std::vector<std::string> cached;
    const auto revision=services_.projects->state().revision;
    try{
        for(std::size_t i=0;i<prepared.size();++i){
            if(!prepared[i]||prepared[i]->sample_rate!=project->sample_rate||prepared[i]->channels!=2||prepared[i]->frames()!=expected.frames)
                throw std::invalid_argument("Prepared separation media has invalid geometry");
            const auto& key=expected.stems[i].source;
            if(!assets_.contains(key)){
                auto data=prepared[i];
                if(data->file){auto copy=*data;auto file=std::make_shared<audio::WavFile>(*data->file);file->path=source_file(key);copy.file=std::move(file);data=std::make_shared<const audio::AudioData>(std::move(copy));}
                cache_asset(key,std::move(data));cached.push_back(key);
            }
            tracks.push_back(new_id());clips.push_back(new_id());
        }
        edit(PlaceSeparation{expected.id,std::move(tracks),std::move(clips)},{},true);
    }catch(...){if(services_.projects->state().revision==revision)for(const auto& key:cached){*assets_.at(key).cancel=true;assets_.erase(key);}throw;}
}
} // namespace mrs::desktop
