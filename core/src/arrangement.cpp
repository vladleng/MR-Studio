#include <mrs/arrangement.hpp>
#include <algorithm>
#include <stdexcept>
namespace mrs {
void RemoveTrack::apply(Project& p) const {
    const auto it = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t) { return t.id == id_; });
    if (it == p.tracks.end()) throw std::invalid_argument("unknown track");
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
} // namespace mrs
