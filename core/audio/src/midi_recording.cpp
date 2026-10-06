#include <mrs/midi_recording.hpp>
#include <algorithm>
#include <chrono>
namespace mrs::audio {
std::uint64_t midi_clock_ns() noexcept {return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
Sample midi_record_sample(Sample head,Sample start,std::uint64_t now,std::uint64_t stamp,std::uint32_t rate) noexcept {
    if(!stamp||stamp>=now)return head;
    const auto elapsed=static_cast<long double>(now-stamp)*rate/1000000000.L;
    return head-std::min(head-start,static_cast<Sample>(std::min(elapsed,static_cast<long double>(max_sample))));
}
MidiRecorder::MidiRecorder(Sample start,std::size_t note_limit,std::size_t event_limit,std::vector<std::uint16_t> existing_keys):start_(start),end_(start),entries_(capacity),note_limit_(note_limit),event_limit_(event_limit){
    std::sort(existing_keys.begin(),existing_keys.end());existing_keys.erase(std::unique(existing_keys.begin(),existing_keys.end()),existing_keys.end());if(existing_keys.size()>keys_.size()||!note_limit||note_limit>4096||event_limit>8176)throw std::invalid_argument("no MIDI recording capacity");key_count_=existing_keys.size();std::copy(existing_keys.begin(),existing_keys.end(),keys_.begin());
}
void MidiRecorder::capture(Sample sample,processing::MidiEvent event) noexcept {
    if(fault_)return;
    if(count_==entries_.size()){fail();return;}
    if(event.kind==processing::MidiKind::note_on){if(note_count_==note_limit_){fail();return;}const auto key=static_cast<std::size_t>(event.channel)*128+event.data1;if(!held_[key]){if(held_count_==128){fail();return;}held_[key]=true;++held_count_;}++note_count_;}
    else if(event.kind!=processing::MidiKind::note_off){if(event_count_>=event_limit_){fail();return;}const auto key=static_cast<std::uint16_t>((static_cast<int>(event.kind)*16+event.channel)*128+((event.kind==processing::MidiKind::cc||event.kind==processing::MidiKind::poly_pressure)?event.data1:0));const auto end=keys_.begin()+static_cast<std::ptrdiff_t>(key_count_);if(std::find(keys_.begin(),end,key)==end){if(key_count_==keys_.size()){fail();return;}keys_[key_count_++]=key;}++event_count_;}
    if(event.kind==processing::MidiKind::note_off){auto& held=held_[static_cast<std::size_t>(event.channel)*128+event.data1];if(held){held=false;--held_count_;}}
    if(event.kind==processing::MidiKind::cc&&(event.data1==120||event.data1==123))for(std::size_t n=0;n<128;++n){auto& held=held_[event.channel*128+n];if(held){held=false;--held_count_;}}
    entries_[count_++]={std::max(start_,sample),event,false};
}
void MidiRecorder::cut(Sample sample) noexcept {if(count_==entries_.size()){fail();return;}if(event_count_+16>event_limit_){fail();return;}event_count_+=16;held_.fill(false);held_count_=0;entries_[count_++]={std::max(start_,sample),{},true};}
MidiClip MidiRecorder::finish(const Timeline& time) const {
    using processing::MidiKind;
    MidiClip clip;clip.start=time.to_ticks(start_);clip.length=std::max<Tick>(1,time.to_ticks(end())-clip.start);
    auto entries=std::vector<Entry>(entries_.begin(),entries_.begin()+static_cast<std::ptrdiff_t>(count_));
    std::stable_sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.sample<b.sample;});
    std::array<std::optional<std::size_t>,2048> open{};
    const auto close=[&](std::size_t key,Tick tick){if(open[key]){auto& note=clip.notes[*open[key]];note.length=std::max<Tick>(0,tick-note.start);open[key].reset();}};
    for(const auto& entry:entries){const auto& e=entry.event;const auto tick=std::clamp(time.to_ticks(entry.sample)-clip.start,Tick{0},clip.length);const auto key=static_cast<std::size_t>(e.channel)*128+e.data1;
        if(entry.cut){for(std::size_t k=0;k<open.size();++k)close(k,tick);for(int ch=0;ch<16;++ch){bool held=false;for(const auto& ce:clip.events)if(ce.kind==2&&ce.channel==ch&&ce.data1==64)held=ce.data2>=64;if(held)clip.events.push_back({new_id(),tick,2,ch,64,0});}continue;}
        if(e.kind==MidiKind::note_on){close(key,tick);open[key]=clip.notes.size();clip.notes.push_back({new_id(),tick,1,e.data1,e.data2,e.channel});}
        else if(e.kind==MidiKind::note_off)close(key,tick);
        else {clip.events.push_back({new_id(),tick,static_cast<int>(e.kind),e.channel,e.data1,e.data2});if(e.kind==MidiKind::cc&&(e.data1==120||e.data1==123))for(std::size_t pitch=0;pitch<128;++pitch)close(e.channel*128+pitch,tick);}
    }
    for(std::size_t key=0;key<open.size();++key)close(key,clip.length);
    std::erase_if(clip.notes,[](const auto& note){return note.length==0;});
    // Release retained sustain at take end, including unplug/Stop with held pedal.
    for(int channel=0;channel<16;++channel){bool sustain=false;for(const auto& e:clip.events)if(e.channel==channel&&e.kind==2&&e.data1==64)sustain=e.data2>=64;if(sustain)clip.events.push_back({new_id(),clip.length,2,channel,64,0});}
    return clip;
}
}
