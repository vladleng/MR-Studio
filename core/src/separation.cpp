#include <mrs/core.hpp>
#include <algorithm>
#include <stdexcept>

namespace mrs {
void RegisterSeparation::apply(Project& p) const {
    const auto clip=std::find(p.clips.begin(),p.clips.end(),expected_);
    if(clip==p.clips.end()||clip->midi||clip->stretch||p.sample_rate!=result_.sample_rate||
       result_.source_clip!=clip->id||result_.source!=clip->source||
       result_.start!=clip->start||result_.frames!=clip->length||result_.source_offset!=clip->source_offset||result_.source_frames!=clip->length||result_.source_rate!=p.sample_rate||result_.fade_in!=clip->fade_in||result_.fade_out!=clip->fade_out||
       std::any_of(p.ara_bindings.begin(),p.ara_bindings.end(),[&](const auto& b){return b.source==clip->source;}))
        throw std::invalid_argument("Source clip changed; separate it again");
    p.separation_results.push_back(result_);
}
void RemoveSeparation::apply(Project& p) const {
    const auto at=std::find_if(p.separation_results.begin(),p.separation_results.end(),[&](const auto& r){return r.id==id_;});
    if(at==p.separation_results.end())throw std::invalid_argument("Unknown separation result");
    // Audible clips and Undo may still reference these immutable files.
    p.separation_results.erase(at);
}
void PlaceSeparation::apply(Project& p) const {
    const auto at=std::find_if(p.separation_results.begin(),p.separation_results.end(),[&](const auto& r){return r.id==result_;});
    if(at==p.separation_results.end()||at->sample_rate!=p.sample_rate||
       tracks_.size()!=at->stems.size()||clips_.size()!=at->stems.size())
        throw std::invalid_argument("Separation result is unavailable or its rate changed");
    for(std::size_t i=0;i<at->stems.size();++i){
        const auto& stem=at->stems[i];
        Track track;track.id=tracks_[i];track.name=stem.role;track.kind=TrackKind::audio;track.input=-1;
        Clip clip;clip.id=clips_[i];clip.track=track.id;clip.name=stem.role;
        clip.start=at->start;clip.length=at->frames;clip.source=stem.source;
        clip.fade_in=at->fade_in;clip.fade_out=at->fade_out;
        p.tracks.push_back(std::move(track));p.clips.push_back(std::move(clip));
    }
}
} // namespace mrs
