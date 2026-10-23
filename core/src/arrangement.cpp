#include <mrs/arrangement.hpp>
#include <algorithm>
#include <stdexcept>
#include <cmath>
namespace mrs {
namespace {
Sample mapped_frame(double source,double ratio) {
    const auto frame=std::floor(static_cast<long double>(source)*ratio+0.5L);
    if(!std::isfinite(source)||!std::isfinite(ratio)||ratio<=0||source<0||!std::isfinite(frame)||frame>max_sample)throw std::invalid_argument("Invalid stretch frame mapping");
    return static_cast<Sample>(frame);
}

}
double audio_time_ratio(const Clip& c) noexcept {return c.stretch&&c.stretch->parameters.enabled?c.stretch->parameters.duration_ratio:1;}
Sample audio_source_span(const Clip& c) noexcept {return c.stretch?c.stretch->source_frames:c.length;}
Sample audio_output_offset(const Clip& c) {return c.stretch?mapped_frame(c.stretch->window_begin,audio_time_ratio(c)):c.source_offset;}
Sample audio_output_extent(const Clip& c) {return c.stretch?mapped_frame(static_cast<double>(c.stretch->source_frames),audio_time_ratio(c)):c.length;}
void validate_audio_stretch(const Clip& c) {
    if(!c.stretch)return;
    const auto& s=*c.stretch;
    if(c.midi||stretch::validate(s.parameters)!=stretch::Status::ok||s.source_frames<=0||s.source_frames>max_sample||
       !std::isfinite(s.window_begin)||!std::isfinite(s.window_end)||s.window_begin<0||s.window_end<=s.window_begin||s.window_end>static_cast<double>(s.source_frames))
        throw std::invalid_argument("Invalid audio stretch state");
    (void)audio_output_extent(c);
    if(mapped_frame(s.window_end,audio_time_ratio(c))-audio_output_offset(c)!=c.length)
        throw std::invalid_argument("Stretch window and clip duration differ");
}
void trim_audio_source(Clip& c,Sample start,Sample end,Sample raw_frames) {
    if(start<0||end<=start||end>max_sample||raw_frames<=0||raw_frames>max_sample)
        throw std::invalid_argument("Invalid trim bounds");
    if(c.stretch){
        validate_audio_stretch(c);
        auto& s=*c.stretch;
        const auto first=audio_output_offset(c)+start-c.start,last=first+end-start,extent=audio_output_extent(c);
        if(first<0||last>extent||c.source_offset>raw_frames||s.source_frames>raw_frames-c.source_offset)
            throw std::invalid_argument("Trim exceeds stretch source span");
        const auto ratio=audio_time_ratio(c);
        if(start!=c.start)s.window_begin=first==0?0:static_cast<double>(first)/ratio;
        if(end!=c.start+c.length)s.window_end=last==extent?static_cast<double>(s.source_frames):static_cast<double>(last)/ratio;
        c.start=start;c.length=end-start;validate_audio_stretch(c);
    }else{
        const auto offset=c.source_offset+start-c.start;
        if(offset<0||offset>raw_frames||end-start>raw_frames-offset)throw std::invalid_argument("Trim exceeds original audio source");
        c.start=start;c.length=end-start;c.source_offset=offset;
    }
    c.fade_in=std::min(c.fade_in,c.length);c.fade_out=std::min(c.fade_out,c.length);
}
Clip stretch_audio_edge(const Clip& original,Sample edge,bool left) {
    if(original.midi)throw std::invalid_argument("Stretch requires an audio clip");
    validate_audio_stretch(original);
    auto c=original;
    if(!c.stretch)c.stretch=ClipStretch{{},c.length,0,static_cast<double>(c.length)};
    auto& state=*c.stretch;auto& params=state.parameters;params.enabled=true;
    const auto anchor=left?original.start+original.length:original.start;
    const auto available=left?anchor:max_sample-anchor;
    const auto lengthAt=[&](double ratio){return mapped_frame(state.window_end,ratio)-mapped_frame(state.window_begin,ratio);};
    const auto minimum=std::max<Sample>(1,lengthAt(.125)),maximum=std::min(available,lengthAt(8));
    if(maximum<minimum)throw std::invalid_argument("No room to stretch clip");
    const auto length=std::clamp(left?anchor-std::clamp(edge,Sample{0},max_sample):std::clamp(edge,Sample{0},max_sample)-anchor,minimum,maximum);
    params.duration_ratio=std::clamp(static_cast<double>(length)/(state.window_end-state.window_begin),.125,8.);
    // Endpoint rounding can differ by a sample on a trimmed window. Keep the
    // fixed edge exact and accept the nearest representable output duration.
    c.length=lengthAt(params.duration_ratio);
    if(c.length<minimum||c.length>maximum)throw std::invalid_argument("Stretch exceeds timeline bounds");
    c.start=left?anchor-c.length:anchor;
    c.fade_in=std::min(c.fade_in,c.length);c.fade_out=std::min(c.fade_out,c.length);
    validate_audio_stretch(c);return c;
}
double project_tempo_at(const Project& p,Sample position){
    const Timeline timeline{p.time,p.sample_rate};double bpm=p.time.tempos.front().bpm;
    for(const auto& point:p.time.tempos){if(timeline.to_samples(point.tick)>position)break;bpm=point.bpm;}return bpm;
}
void RemoveTrack::apply(Project& p) const {
    const auto it = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t) { return t.id == id_; });
    if (it == p.tracks.end()) throw std::invalid_argument("unknown track");
    const auto destination = it->output; const auto hardware = it->hardware_outputs;
    for (auto& track : p.tracks) {
        if (track.output == id_) { track.output = destination; track.hardware_outputs = hardware; }
        std::erase_if(track.sends,[&](const auto& send) { return send.bus == id_; });
    }
    for(auto& folder:p.folders)if(folder.bus==id_)folder.bus.reset();
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
    if (clips_.empty() || (!tracks_.empty() && tracks_.size() != clips_.size())) throw std::invalid_argument("empty/invalid audio batch");
    for (std::size_t i = 0; i < clips_.size(); ++i) {
        if(!tracks_.empty()) {
            if(tracks_[i].kind!=TrackKind::audio||clips_[i].track!=tracks_[i].id)throw std::invalid_argument("invalid audio import");
        } else {
            const auto track=std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t){return t.id==clips_[i].track;});
            if(track==p.tracks.end()||track->kind!=TrackKind::audio)throw std::invalid_argument("audio import requires an audio track");
        }
    }
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
void SetAudioStretch::apply(Project& p) const {
    auto it=audio_clip(p,id_);auto& c=*it;
    if(stretch::validate(parameters_)!=stretch::Status::ok)throw std::invalid_argument("Invalid stretch parameters");
    if(std::any_of(p.ara_bindings.begin(),p.ara_bindings.end(),[&](const auto& b){return b.source==c.source;}))
        throw std::invalid_argument("Detach ARA processing before applying native time / pitch");
    if(reset_){
        if(!c.stretch)return;
        const auto begin=mapped_frame(c.stretch->window_begin,1),end=mapped_frame(c.stretch->window_end,1);
        if(end<=begin)throw std::invalid_argument("Source selection is shorter than one sample");
        c.source_offset+=begin;c.length=end-begin;c.stretch.reset();
    }else{
        if(!c.stretch)c.stretch=ClipStretch{parameters_,c.length,0,static_cast<double>(c.length)};
        c.stretch->parameters=parameters_;
        c.length=mapped_frame(c.stretch->window_end,audio_time_ratio(c))-audio_output_offset(c);
        if(c.length<=0||c.length>max_sample-c.start)throw std::invalid_argument("Stretch produces invalid clip duration");
        validate_audio_stretch(c);
    }
    c.fade_in=std::min(c.fade_in,c.length);c.fade_out=std::min(c.fade_out,c.length);
}
void SetAudioClipTiming::apply(Project& p) const {
    auto it=audio_clip(p,expected_.id);
    if(*it!=expected_)throw std::runtime_error("Clip changed; retry time / pitch edit");
    if(start_<0||start_>max_sample|| (tempo_&&(!std::isfinite(*tempo_)||*tempo_<1||*tempo_>999)))throw std::invalid_argument("Invalid clip timing");
    // Work on a candidate, so validation failures never partially mutate p.
    auto candidate=p;auto& c=*audio_clip(candidate,expected_.id);c.start=start_;
    if(c.stretch||parameters_!=stretch::Parameters{})SetAudioStretch{c.id,parameters_}.apply(candidate);
    c.file_tempo=tempo_;candidate.validate();p=std::move(candidate);
}
void TrimAudioClip::apply(Project& p) const {
    auto it = audio_clip(p,id_);
    trim_audio_source(*it,start_,end_,frames_);
}
void SplitAudioClip::apply(Project& p) const {
    auto it = audio_clip(p,id_);
    if (position_ <= it->start || position_ >= it->start+it->length || right_.value.empty())
        throw std::invalid_argument("split cursor must be inside the selected clip");
    auto right = *it;
    const auto left_length = position_-it->start;
    right.id = right_; right.start = position_; right.length -= left_length;
    if(it->stretch){
        const auto boundary=static_cast<double>(audio_output_offset(*it)+left_length)/audio_time_ratio(*it);
        it->stretch->window_end=boundary;right.stretch->window_begin=boundary;
    }else right.source_offset += left_length;
    it->length = left_length;
    it->fade_out=0;it->fade_in=std::min(it->fade_in,it->length);right.fade_in=0;right.fade_out=std::min(right.fade_out,right.length);
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
