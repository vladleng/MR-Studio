#include <mrs/midi_playback.hpp>
#include <algorithm>
#include <stdexcept>
namespace mrs::audio {
using processing::MidiKind;
std::vector<std::vector<PlaybackMidiNote>> compile_midi_clips(const Project& p,std::span<const Id> tracks){
    const Timeline time(p.time,p.sample_rate);std::vector<std::vector<PlaybackMidiNote>> result(tracks.size());
    for(const auto& clip:p.clips)if(clip.midi){const auto& m=*clip.midi;(void)time.to_samples(m.start);(void)time.to_samples(m.start+m.length);const auto at=std::find(tracks.begin(),tracks.end(),clip.track);if(at==tracks.end())throw std::invalid_argument("missing MIDI mixer track");auto& notes=result[static_cast<std::size_t>(at-tracks.begin())];
        for(const auto& n:m.notes){const auto begin=std::max(n.start,m.source_offset),end=std::min(n.start+n.length,m.source_offset+m.length);if(begin>=end)continue;const auto a=time.to_samples(m.start+begin-m.source_offset),b=time.to_samples(m.start+end-m.source_offset);if(b<=a)continue;notes.push_back({a,b,static_cast<std::uint8_t>(n.pitch),static_cast<std::uint8_t>(n.velocity),static_cast<std::uint8_t>(n.channel)});}
    }
    for(const auto& notes:result){MidiPlayback prepared;prepared.prepare(notes);} // reject budgets before model commit
    return result;
}
void MidiPlayback::prepare(std::vector<PlaybackMidiNote> notes){
    if(notes.size()>4096)throw std::invalid_argument("MIDI playback supports 4096 notes per track");
    for(const auto& n:notes)if(n.start<0||n.end<=n.start||n.end>max_sample||n.channel>15||n.pitch>127||n.velocity<1||n.velocity>127)throw std::invalid_argument("invalid playback MIDI note");
    std::sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){if(a.channel!=b.channel)return a.channel<b.channel;if(a.pitch!=b.pitch)return a.pitch<b.pitch;return a.start<b.start;});
    notes_.clear();events_.clear();
    for(const auto& n:notes){if(!notes_.empty()){auto& last=notes_.back();if(last.channel==n.channel&&last.pitch==n.pitch&&n.start<last.end){last.end=std::max(last.end,n.end);continue;}}notes_.push_back(n);}
    std::stable_sort(notes_.begin(),notes_.end(),[](const auto& a,const auto& b){return a.start<b.start;});
    chase_leaves_=1;while(chase_leaves_<notes_.size())chase_leaves_*=2;chase_ends_.assign(2*chase_leaves_,0);
    for(std::size_t i=0;i<notes_.size();++i)chase_ends_[chase_leaves_+i]=notes_[i].end;
    for(std::size_t i=chase_leaves_-1;i>0;--i)chase_ends_[i]=std::max(chase_ends_[2*i],chase_ends_[2*i+1]);
    for(const auto& n:notes_){events_.push_back({n.start,{0,MidiKind::note_on,n.channel,n.pitch,n.velocity}});events_.push_back({n.end,{0,MidiKind::note_off,n.channel,n.pitch,0}});}
    std::stable_sort(events_.begin(),events_.end(),[](const auto& a,const auto& b){if(a.sample!=b.sample)return a.sample<b.sample;return a.midi.kind<b.midi.kind;});
    int active=0;for(const auto& e:events_){active+=e.midi.kind==MidiKind::note_on?1:-1;if(active>128)throw std::invalid_argument("MIDI playback supports 128 simultaneous notes per track");}
    forget();cursor_=0;
}
bool MidiPlayback::emit(processing::MidiEvent e,processing::MidiBuffer& buffer) noexcept{
    const auto key=static_cast<std::uint16_t>(e.channel*128+e.data1);
    const auto active_end=active_.begin()+static_cast<std::ptrdiff_t>(active_count_);
    // Discontinuity already released these keys. Do not duplicate scheduled
    // offs at the new head (or consume capacity needed for 128 chased ons).
    if(e.kind==MidiKind::note_off&&std::find(active_.begin(),active_end,key)==active_end)return true;
    if(!buffer.push(e))return false;
    if(e.kind==MidiKind::note_on){if(active_count_==active_.size())return false;active_[active_count_++]=key;}
    else if(e.kind==MidiKind::note_off){auto it=std::find(active_.begin(),active_.begin()+static_cast<std::ptrdiff_t>(active_count_),key);if(it!=active_.begin()+static_cast<std::ptrdiff_t>(active_count_)){*it=active_[--active_count_];}}
    return true;
}
bool MidiPlayback::render(Sample position,std::uint32_t frames,bool playing,processing::MidiBuffer& buffer) noexcept{
    const bool discontinuity=position!=expected_||playing!=was_playing_;
    bool ok=true;
    if(discontinuity){
        while(active_count_&&ok){const auto key=active_[active_count_-1];ok=emit({0,MidiKind::note_off,static_cast<std::uint8_t>(key/128),static_cast<std::uint8_t>(key%128),0},buffer);}
        cursor_=static_cast<std::size_t>(std::lower_bound(events_.begin(),events_.end(),position,[](const Event& e,Sample s){return e.sample<s;})-events_.begin());
        if(playing&&ok&&!notes_.empty()){
            struct Range{std::size_t node{},begin{},end{};};std::array<Range,32> stack{};std::size_t size=1;stack[0]={1,0,chase_leaves_};
            while(size&&ok){const auto r=stack[--size];if(r.begin>=notes_.size()||notes_[r.begin].start>=position||chase_ends_[r.node]<=position)continue;
                if(r.end-r.begin==1){const auto& n=notes_[r.begin];ok=emit({0,MidiKind::note_on,n.channel,n.pitch,n.velocity},buffer);}
                else {const auto mid=(r.begin+r.end)/2;stack[size++]={r.node*2+1,mid,r.end};stack[size++]={r.node*2,r.begin,mid};}
            }
        }
    }
    if(playing&&ok){
        std::size_t budget=processing::event_capacity;
        while(cursor_<events_.size()&&events_[cursor_].sample<position+frames){
            if(!budget--){ok=false;break;}
            auto e=events_[cursor_++];e.midi.offset=static_cast<std::uint32_t>(e.sample-position);
            if(!emit(e.midi,buffer)){ok=false;break;}
        }
    }
    expected_=playing?position+frames:position;was_playing_=playing;
    if(!ok){active_count_=0;cursor_=static_cast<std::size_t>(std::lower_bound(events_.begin(),events_.end(),expected_,[](const Event& e,Sample s){return e.sample<s;})-events_.begin());}
    return ok;
}
}
