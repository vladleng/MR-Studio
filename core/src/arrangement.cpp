#include <mrs/arrangement.hpp>
#include <algorithm>
#include <stdexcept>
#include <cmath>
namespace mrs {
void RemoveTrack::apply(Project& p) const {
    const auto it = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t) { return t.id == id_; });
    if (it == p.tracks.end()) throw std::invalid_argument("unknown track");
    const auto destination = it->output;
    for (auto& track : p.tracks) if (track.output == id_) track.output = destination;
    p.tracks.erase(it);
    std::erase_if(p.clips,[&](const auto& c) { return c.track == id_; });
}
void ReorderTrack::apply(Project& p) const {
    const auto it = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t) { return t.id == id_; });
    if (it == p.tracks.end() || index_ >= p.tracks.size()) throw std::invalid_argument("invalid track position");
    auto track = *it; p.tracks.erase(it);
    p.tracks.insert(p.tracks.begin()+static_cast<std::ptrdiff_t>(index_),std::move(track));
}
void ImportAudio::apply(Project& p) const {
    if (tracks_.empty() || tracks_.size() != clips_.size()) throw std::invalid_argument("empty/invalid audio batch");
    for (std::size_t i = 0; i < tracks_.size(); ++i)
        if (tracks_[i].kind != TrackKind::audio || clips_[i].track != tracks_[i].id) throw std::invalid_argument("invalid audio import");
    p.tracks.insert(p.tracks.end(),tracks_.begin(),tracks_.end());
    p.clips.insert(p.clips.end(),clips_.begin(),clips_.end());
}
void AddRecordedClip::apply(Project& p) const {
    const auto track = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t) { return t.id == clip_.track; });
    if (track == p.tracks.end() || track->kind != TrackKind::audio || clip_.source.empty())
        throw std::invalid_argument("recording needs an existing audio track and source");
    p.clips.push_back(clip_);
}
namespace {
auto audio_clip(Project& p, const Id& id) {
    auto it = std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& clip) { return clip.id == id; });
    if (it == p.clips.end()) throw std::invalid_argument("unknown clip");
    auto track = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t) { return t.id == it->track; });
    if (track == p.tracks.end() || track->kind != TrackKind::audio) throw std::invalid_argument("clip must be on an audio track");
    return it;
}
}
void MoveAudioClip::apply(Project& p) const {
    auto it = audio_clip(p,id_);
    auto target = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t) { return t.id == track_; });
    if (target == p.tracks.end() || target->kind != TrackKind::audio || start_ < 0 || start_ > max_sample-it->length)
        throw std::invalid_argument("invalid audio clip destination");
    it->track = track_; it->start = start_;
}
void TrimAudioClip::apply(Project& p) const {
    auto it = audio_clip(p,id_);
    if (start_ < 0 || end_ <= start_ || end_ > max_sample || frames_ <= 0 || frames_ > max_sample)
        throw std::invalid_argument("invalid trim bounds");
    const auto offset = it->source_offset + start_ - it->start;
    if (offset < 0 || offset > frames_ || end_-start_ > frames_-offset)
        throw std::invalid_argument("trim exceeds original audio source");
    it->start = start_; it->length = end_-start_; it->source_offset = offset;
}
void SplitAudioClip::apply(Project& p) const {
    auto it = audio_clip(p,id_);
    if (position_ <= it->start || position_ >= it->start+it->length || right_.value.empty())
        throw std::invalid_argument("split cursor must be inside the selected clip");
    auto right = *it;
    const auto left_length = position_-it->start;
    right.id = right_; right.start = position_; right.length -= left_length; right.source_offset += left_length;
    it->length = left_length;
    p.clips.insert(it+1,std::move(right));
}
void RemoveAudioClip::apply(Project& p) const { p.clips.erase(audio_clip(p,id_)); }
Sample snap_to_grid(const Project& p, Sample position, Tick grid) {
    if (position < 0 || position > max_sample || grid <= 0 || grid > max_tick) throw std::invalid_argument("invalid snap position/grid");
    Timeline time(p.time,p.sample_rate); const auto tick = time.to_ticks(position);
    const auto snapped = static_cast<Tick>(std::llround(static_cast<double>(tick)/static_cast<double>(grid)))*grid;
    if (snapped < 0 || snapped > max_tick) throw std::invalid_argument("snap exceeds timeline");
    return time.to_samples(snapped);
}
} // namespace mrs
