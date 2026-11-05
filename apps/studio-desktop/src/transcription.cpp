#include <mrs/desktop.hpp>
#include <algorithm>
#include <stdexcept>

namespace mrs::desktop {
void Application::queue_transcription(Id clip,transcription::ModelStore models,transcription::Request settings){
    require_not_recording();
    const auto p=services_.projects->state().project;
    const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==clip;});
    if(at==p->clips.end()||at->midi)throw std::invalid_argument("Choose a file-backed audio clip");
    struct Media {std::shared_ptr<const audio::AudioData> source;std::shared_ptr<void> files;};
    auto media=std::make_shared<Media>(Media{source_lifetime(at->source),separation_media_guard()});
    if(!transcription_)transcription_=std::make_unique<transcription::Controller>();
    transcription_->poll(services_.projects,path_);
    transcription_->start(services_.projects,path_,std::move(clip),source_file(at->source),std::move(models),std::move(settings),std::move(media));
}
void Application::cancel_transcription(){if(transcription_)transcription_->cancel();}
void Application::poll_transcription(){if(transcription_)transcription_->poll(services_.projects,path_);}
transcription::JobStatus Application::transcription_status()const{return transcription_?transcription_->status():transcription::JobStatus{};}
const transcription::Result* Application::transcription_preview()const{return transcription_?transcription_->preview():nullptr;}
std::vector<Id> Application::place_transcription(const std::optional<std::vector<std::string>>& roles){
    require_not_playing();require_not_recording();
    if(!transcription_)throw std::invalid_argument("No transcription result");
    auto command=transcription_->prepare(services_.projects,path_,roles);
    const auto project=services_.projects->state().project;auto candidate=*project;command.apply(candidate);
    std::vector<Id> placed;for(std::size_t i=project->clips.size();i<candidate.clips.size();++i)placed.push_back(candidate.clips[i].id);
    commit_midi_project(std::move(candidate),std::string(command.name()));transcription_->placed();return placed;
}
}
