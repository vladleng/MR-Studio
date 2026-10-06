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
std::vector<std::vector<PlaybackMidiEvent>> compile_midi_events(const Project& p,std::span<const Id> tracks){
    const Timeline time(p.time,p.sample_rate);std::vector<std::vector<PlaybackMidiEvent>> result(tracks.size());
    for(const auto& c:p.clips)if(c.midi){const auto& m=*c.midi;const auto at=std::find(tracks.begin(),tracks.end(),c.track);if(at==tracks.end())throw std::invalid_argument("missing MIDI event track");auto& lane=result[static_cast<std::size_t>(at-tracks.begin())];const auto first=lane.size();
        for(const auto& e:m.events)if(e.start>=m.source_offset&&e.start<=m.source_offset+m.length)lane.push_back({time.to_samples(m.start+e.start-m.source_offset),{0,static_cast<MidiKind>(e.kind),static_cast<std::uint8_t>(e.channel),static_cast<std::uint8_t>(e.data1),static_cast<std::uint8_t>(e.data2)}});
        if(m.source_offset){std::vector<MidiChannelEvent> latest;for(const auto& event:m.events)if(event.start<m.source_offset){const auto value_at=std::find_if(latest.begin(),latest.end(),[&](const auto& value){return value.kind==event.kind&&value.channel==event.channel&&((event.kind!=2&&event.kind!=6)||value.data1==event.data1);});if(value_at==latest.end())latest.push_back(event);else if(event.start>=value_at->start)*value_at=event;}for(const auto& e:latest)lane.insert(lane.begin()+static_cast<std::ptrdiff_t>(first),{time.to_samples(m.start),{0,static_cast<MidiKind>(e.kind),static_cast<std::uint8_t>(e.channel),static_cast<std::uint8_t>(e.data1),static_cast<std::uint8_t>(e.data2)}});}
        // Release sustain at each clip boundary; a split therefore cannot hang a voice.
        std::array<bool,16> channels{};for(const auto& e:m.events)if(e.kind==2&&e.data1==64)channels[static_cast<std::size_t>(e.channel)]=true;
        for(std::size_t ch=0;ch<channels.size();++ch)if(channels[ch])lane.push_back({time.to_samples(m.start+m.length),{0,MidiKind::cc,static_cast<std::uint8_t>(ch),64,0}});
    }
    for(const auto& lane:result){MidiPlayback check;check.prepare({},lane);}return result;
}
void MidiPlayback::prepare(std::vector<PlaybackMidiNote> notes,std::vector<PlaybackMidiEvent> controls){
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
    if(controls.size()>8192)throw std::invalid_argument("MIDI playback supports 8192 channel events per track");
    controls_=std::move(controls);std::stable_sort(controls_.begin(),controls_.end(),[](const auto& a,const auto& b){return a.sample<b.sample;});control_keys_.clear();
    for(const auto& e:controls_){if(e.sample<0||e.sample>max_sample||e.event.kind<MidiKind::cc||e.event.kind>MidiKind::poly_pressure||e.event.channel>15||e.event.data1>127||e.event.data2>127)throw std::invalid_argument("invalid MIDI playback channel event");const auto key=static_cast<std::uint16_t>((static_cast<int>(e.event.kind)*16+e.event.channel)*128+((e.event.kind==MidiKind::cc||e.event.kind==MidiKind::poly_pressure)?e.event.data1:0));if(std::find(control_keys_.begin(),control_keys_.end(),key)==control_keys_.end())control_keys_.push_back(key);}
    if(control_keys_.size()>64)throw std::invalid_argument("MIDI playback supports 64 distinct channel controls per track");
    control_lanes_.assign(control_keys_.size(),{});for(const auto& e:controls_){const auto key=static_cast<std::uint16_t>((static_cast<int>(e.event.kind)*16+e.event.channel)*128+((e.event.kind==MidiKind::cc||e.event.kind==MidiKind::poly_pressure)?e.event.data1:0));const auto at=std::find(control_keys_.begin(),control_keys_.end(),key)-control_keys_.begin();control_lanes_[static_cast<std::size_t>(at)].push_back(e);}
    forget();cursor_=0;control_cursor_=0;
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
    const auto ingress=buffer.size;
    const bool discontinuity=position!=expected_||playing!=was_playing_;
    bool ok=true;
    if(discontinuity){
        while(active_count_&&ok){const auto key=active_[active_count_-1];ok=emit({0,MidiKind::note_off,static_cast<std::uint8_t>(key/128),static_cast<std::uint8_t>(key%128),0},buffer);}
        cursor_=static_cast<std::size_t>(std::lower_bound(events_.begin(),events_.end(),position,[](const Event& e,Sample s){return e.sample<s;})-events_.begin());
        // Reset the channel controls used by this track on every discontinuity.
        for(std::size_t lane=0;lane<control_keys_.size()&&ok;++lane){const auto key=control_keys_[lane];const auto kind=static_cast<MidiKind>(key/128/16);const auto channel=static_cast<std::uint8_t>(key/128%16),number=static_cast<std::uint8_t>(key%128);processing::MidiEvent state{0,kind,channel,number,0};if(kind==MidiKind::pitch_bend){state.data1=0;state.data2=64;}else if(kind==MidiKind::cc){if(number==7)state.data2=100;else if(number==11)state.data2=127;else if(number==10)state.data2=64;}
            if(playing){const auto& values=control_lanes_[lane];auto at=std::lower_bound(values.begin(),values.end(),position,[](const auto& e,Sample sample){return e.sample<sample;});if(at!=values.begin()){state=(at-1)->event;state.offset=0;}}
            ok=buffer.push(state);
        }
        control_cursor_=static_cast<std::size_t>(std::lower_bound(controls_.begin(),controls_.end(),position,[](const auto& e,Sample s){return e.sample<s;})-controls_.begin());
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
    if(playing&&ok){std::size_t budget=processing::event_capacity;while(control_cursor_<controls_.size()&&controls_[control_cursor_].sample<position+frames){if(!budget--){ok=false;break;}auto e=controls_[control_cursor_++];e.event.offset=static_cast<std::uint32_t>(e.sample-position);if(!buffer.push(e.event)){ok=false;break;}}}
    // Stable offset order: chasing controllers were appended before chased notes.
    const auto priority=[](MidiKind kind){return kind==MidiKind::note_off?0:kind==MidiKind::note_on?2:1;};
    for(std::size_t i=ingress+1;i<buffer.size;++i){const auto item=buffer.events[i];auto j=i;while(j>ingress&&(buffer.events[j-1].offset>item.offset||(buffer.events[j-1].offset==item.offset&&priority(buffer.events[j-1].kind)>priority(item.kind)))){buffer.events[j]=buffer.events[j-1];--j;}buffer.events[j]=item;}
    // Merge timeline events with ingress by offset, preserving the driver order.
    for(std::size_t i=1;i<buffer.size;++i){const auto item=buffer.events[i];auto j=i;while(j&&buffer.events[j-1].offset>item.offset){buffer.events[j]=buffer.events[j-1];--j;}buffer.events[j]=item;}
    expected_=playing?position+frames:position;was_playing_=playing;
    if(!ok){control_cursor_=static_cast<std::size_t>(std::lower_bound(controls_.begin(),controls_.end(),expected_,[](const auto& e,Sample s){return e.sample<s;})-controls_.begin());active_count_=0;cursor_=static_cast<std::size_t>(std::lower_bound(events_.begin(),events_.end(),expected_,[](const Event& e,Sample s){return e.sample<s;})-events_.begin());}
    return ok;
}
}
