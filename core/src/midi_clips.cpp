#include <mrs/midi_clips.hpp>
#include <algorithm>
#include <stdexcept>
#include <cmath>
namespace mrs {
namespace {
auto find_clip(Project& p,const Id& id){auto i=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});if(i==p.clips.end())throw std::invalid_argument("unknown clip");return i;}
auto midi_clip(Project& p,const Id& id){auto i=find_clip(p,id);if(!i->midi)throw std::invalid_argument("select a MIDI clip");return i;}
void instrument(const Project& p,const Id& id){auto i=std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t){return t.id==id;});if(i==p.tracks.end()||i->kind!=TrackKind::instrument)throw std::invalid_argument("MIDI clip needs an instrument track");}
void fresh_notes(Clip& c){if(c.midi){for(auto& n:c.midi->notes)n.id=new_id();for(auto& e:c.midi->events)e.id=new_id();}}
}
void AddMidiClip::apply(Project& p) const{instrument(p,clip_.track);if(!clip_.midi)throw std::invalid_argument("missing MIDI clip data");p.clips.push_back(clip_);}
void SetMidiNotes::apply(Project& p) const{midi_clip(p,id_)->midi->notes=notes_;}
int midi_event_value(const MidiChannelEvent& e){if(e.kind<2||e.kind>6)throw std::invalid_argument("unsupported MIDI event kind");return e.kind==5?(e.data2*128+e.data1)-8192:e.kind==3||e.kind==4?e.data1:e.data2;}
void set_midi_event_value(MidiChannelEvent& e,int value){if(e.kind<2||e.kind>6||value<(e.kind==5?-8192:0)||value>(e.kind==5?8191:127))throw std::invalid_argument("MIDI event value outside range");if(e.kind==5){const int bend=value+8192;e.data1=bend%128;e.data2=bend/128;}else if(e.kind==3||e.kind==4){e.data1=value;e.data2=0;}else e.data2=value;}
void SetMidiEvents::apply(Project& p) const{midi_clip(p,clip_)->midi->events=events_;}
std::vector<MidiNote> transform_midi_notes(const MidiClip& clip,std::span<const Id> ids,const NoteEdit& edit){
    if(edit.kind<NoteEditKind::quantize||edit.kind>NoteEditKind::length||edit.mode<NoteValueMode::set||edit.mode>NoteValueMode::scale)throw std::invalid_argument("unknown musical edit operation");
    if(!std::isfinite(edit.value)||!std::isfinite(edit.strength)||edit.strength<0||edit.strength>100||edit.grid<1||edit.grid>max_tick)throw std::invalid_argument("invalid musical edit settings");
    if(edit.kind==NoteEditKind::transpose&&(edit.value<-127||edit.value>127||std::floor(edit.value)!=edit.value))throw std::invalid_argument("transpose must be -127..127 semitones");
    if(edit.kind==NoteEditKind::velocity&&(edit.value<-127||edit.value>1000||(edit.mode==NoteValueMode::scale&&edit.value<0)))throw std::invalid_argument("invalid velocity value");
    if(edit.kind==NoteEditKind::length&&(edit.value<-max_tick||edit.value>max_tick||(edit.mode==NoteValueMode::set&&edit.value<1)||(edit.mode==NoteValueMode::scale&&(edit.value<0||edit.value>1000))))throw std::invalid_argument("invalid note length");
    auto notes=clip.notes;auto chosen=[&](const MidiNote& n){return std::find(ids.begin(),ids.end(),n.id)!=ids.end();};
    for(const auto& id:ids)if(std::none_of(notes.begin(),notes.end(),[&](const auto& n){return n.id==id;}))throw std::invalid_argument("selected note no longer exists");
    int low=-127,high=127;for(const auto& n:notes)if(chosen(n)){low=std::max(low,-n.pitch);high=std::min(high,127-n.pitch);}const int transpose=edit.kind==NoteEditKind::transpose?std::clamp(static_cast<int>(edit.value),low,high):0;
    const auto snapped=[&](Tick tick){return static_cast<Tick>(std::llround(static_cast<double>(tick)/edit.grid))*edit.grid;};
    for(auto& n:notes)if(chosen(n)){
        const Tick originalEnd=n.start+n.length,limit=std::max(clip.source_offset+clip.length,originalEnd);
        if(edit.kind==NoteEditKind::transpose){n.pitch+=transpose;continue;}
        if(edit.kind==NoteEditKind::velocity){const auto value=edit.mode==NoteValueMode::set?edit.value:edit.mode==NoteValueMode::add?n.velocity+edit.value:n.velocity*edit.value/100.;n.velocity=static_cast<int>(std::clamp(std::round(value),1.,127.));continue;}
        if(edit.kind==NoteEditKind::quantize){
            if(edit.starts){const auto target=clip.source_offset+snapped(n.start-clip.source_offset);const auto next=static_cast<Tick>(std::llround(n.start+(target-n.start)*edit.strength/100.));n.start=std::clamp(next,std::min(clip.source_offset,n.start),std::max(std::min(clip.source_offset,n.start),limit-n.length));}
            if(edit.lengths){const auto target=std::max(Tick{1},snapped(n.length));n.length=static_cast<Tick>(std::llround(n.length+(target-n.length)*edit.strength/100.));}
        }else if(edit.kind==NoteEditKind::length){const double value=edit.mode==NoteValueMode::set?edit.value:edit.mode==NoteValueMode::add?n.length+edit.value:n.length*edit.value/100.;n.length=static_cast<Tick>(std::clamp(std::round(value),1.,static_cast<double>(max_tick-n.start)));}
        n.length=std::clamp(n.length,Tick{1},std::max(Tick{1},std::min(max_tick-n.start,limit-n.start)));
    }
    return notes;
}
void EditMidiNotes::apply(Project& p) const{auto& clip=*midi_clip(p,clip_)->midi;clip.notes=transform_midi_notes(clip,selected_,edit_);}
void MoveMidiClip::apply(Project& p) const{instrument(p,track_);auto i=midi_clip(p,id_);i->track=track_;i->midi->start=start_;}
void trim_midi_source(MidiClip& m,Tick start,Tick end){
    if(start<0||end<=start||end>max_tick)throw std::invalid_argument("invalid MIDI trim bounds");
    const auto offset=m.source_offset+start-m.start;
    if(std::max(Tick{0},offset)>max_tick-(end-start))throw std::invalid_argument("MIDI trim exceeds source range");
    if(offset<0){const auto prefix=-offset;
        for(const auto& n:m.notes)if(n.start>max_tick-n.length-prefix)throw std::invalid_argument("MIDI prefix exceeds note range");
        for(const auto& e:m.events)if(e.start>max_tick-prefix)throw std::invalid_argument("MIDI prefix exceeds event range");
        for(auto& n:m.notes)n.start+=prefix;for(auto& e:m.events)e.start+=prefix;
    }
    m.source_offset=std::max(Tick{0},offset);m.start=start;m.length=end-start;
}
void TrimMidiClip::apply(Project& p) const{trim_midi_source(*midi_clip(p,id_)->midi,start_,end_);}
void SplitMidiClip::apply(Project& p) const{auto i=midi_clip(p,id_);auto& m=*i->midi;if(at_<=m.start||at_>=m.start+m.length)throw std::invalid_argument("split cursor must be inside MIDI clip");auto right=*i;right.id=right_;fresh_notes(right);const auto left=at_-m.start;right.midi->start=at_;right.midi->length-=left;right.midi->source_offset+=left;m.length=left;p.clips.insert(i+1,std::move(right));}
void DuplicateClip::apply(Project& p) const{auto c=*find_clip(p,id_);c.id=duplicate_;fresh_notes(c);if(c.midi)c.midi->start+=c.midi->length;else c.start+=c.length;p.clips.push_back(std::move(c));}
void RemoveClip::apply(Project& p) const{p.clips.erase(find_clip(p,id_));}
}
