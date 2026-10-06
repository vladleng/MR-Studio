#include <mrs/midi_clips.hpp>
#include <algorithm>
#include <stdexcept>
namespace mrs {
namespace {
auto find_clip(Project& p,const Id& id){auto i=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});if(i==p.clips.end())throw std::invalid_argument("unknown clip");return i;}
auto midi_clip(Project& p,const Id& id){auto i=find_clip(p,id);if(!i->midi)throw std::invalid_argument("select a MIDI clip");return i;}
void instrument(const Project& p,const Id& id){auto i=std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t){return t.id==id;});if(i==p.tracks.end()||i->kind!=TrackKind::instrument)throw std::invalid_argument("MIDI clip needs an instrument track");}
void fresh_notes(Clip& c){if(c.midi){for(auto& n:c.midi->notes)n.id=new_id();for(auto& e:c.midi->events)e.id=new_id();}}
}
void AddMidiClip::apply(Project& p) const{instrument(p,clip_.track);if(!clip_.midi)throw std::invalid_argument("missing MIDI clip data");p.clips.push_back(clip_);}
void SetMidiNotes::apply(Project& p) const{midi_clip(p,id_)->midi->notes=notes_;}
void MoveMidiClip::apply(Project& p) const{instrument(p,track_);auto i=midi_clip(p,id_);i->track=track_;i->midi->start=start_;}
void TrimMidiClip::apply(Project& p) const{auto& m=*midi_clip(p,id_)->midi;if(start_<0||end_<=start_||end_>max_tick)throw std::invalid_argument("invalid MIDI trim bounds");const auto offset=m.source_offset+start_-m.start;if(offset<0)throw std::invalid_argument("trim precedes MIDI source");m.source_offset=offset;m.start=start_;m.length=end_-start_;}
void SplitMidiClip::apply(Project& p) const{auto i=midi_clip(p,id_);auto& m=*i->midi;if(at_<=m.start||at_>=m.start+m.length)throw std::invalid_argument("split cursor must be inside MIDI clip");auto right=*i;right.id=right_;fresh_notes(right);const auto left=at_-m.start;right.midi->start=at_;right.midi->length-=left;right.midi->source_offset+=left;m.length=left;p.clips.insert(i+1,std::move(right));}
void DuplicateClip::apply(Project& p) const{auto c=*find_clip(p,id_);c.id=duplicate_;fresh_notes(c);if(c.midi)c.midi->start+=c.midi->length;else c.start+=c.length;p.clips.push_back(std::move(c));}
void RemoveClip::apply(Project& p) const{p.clips.erase(find_clip(p,id_));}
}
