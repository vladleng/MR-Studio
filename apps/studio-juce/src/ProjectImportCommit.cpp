#include "ProjectImportCommit.h"
#include <mrs/audio.hpp>
#include <mrs/wav_resample.hpp>
#include <mrs/persistence.hpp>
#include <mrs/project_folders.hpp>
#include <juce_core/juce_core.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <array>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace mrs::project_import {
namespace {
namespace fs=std::filesystem;
using namespace std::string_view_literals;
constexpr std::uint64_t maxAssetBytes=2ULL*1024*1024*1024;
struct Cancelled {};
void checkStop(std::stop_token stop){if(stop.stop_requested())throw Cancelled{};}
std::string_view get(const Object& o,std::string_view key){
    for(const auto& [k,v]:o.properties)if(k==key)return v;
    return {};
}
std::optional<double> numeric(const Object& o,std::string_view key){
    auto s=get(o,key);if(s.empty())return {};
    if(s.starts_with('+'))s.remove_prefix(1);
    double value{};const auto end=s.data()+s.size();const auto r=std::from_chars(s.data(),end,value);
    if(r.ec!=std::errc{}||r.ptr!=end||!std::isfinite(value))return {};
    return value;
}
std::optional<int> integer(const Object& o,std::string_view key,int low,int high){
    const auto n=numeric(o,key);if(!n||*n<low||*n>high||std::trunc(*n)!=*n)return {};return static_cast<int>(*n);
}
std::optional<std::uint32_t> color(const Object& o){
    auto s=get(o,"color");if(s.size()!=7||s.front()!='#')return {};s.remove_prefix(1);
    std::uint32_t rgb{};const auto end=s.data()+s.size();const auto r=std::from_chars(s.data(),end,rgb,16);
    return r.ec==std::errc{}&&r.ptr==end?std::optional{rgb}:std::nullopt;
}
std::string utf8(const fs::path& p){const auto s=p.generic_u8string();return {s.begin(),s.end()};}
fs::path fromUtf8(std::string_view s){return fs::path(std::u8string(s.begin(),s.end()));}
std::string name(const Object& o,const char* fallback){return o.name.empty()?fallback:o.name;}
void warn(std::vector<std::string>& out,std::string message){
    if(out.size()<200)out.push_back(std::move(message));
    else if(out.size()==200)out.emplace_back("Additional import warnings omitted.");
}
Id entity(std::size_t i,std::string_view kind){return {"daw-"+std::string(kind)+"-"+std::to_string(i+1)};}
Tick ticks(double beats){
    if(!std::isfinite(beats)||std::abs(beats)>static_cast<double>(max_tick)/ppq)throw std::runtime_error("DAWproject beat position exceeds supported range");
    return std::llround(beats*ppq);
}
Sample samples(double seconds,std::uint32_t rate){
    if(!std::isfinite(seconds)||std::abs(seconds)>static_cast<double>(max_sample)/rate)throw std::runtime_error("DAWproject media time exceeds supported range");
    return std::llround(seconds*rate);
}
bool seconds(TimeUnit u){return u==TimeUnit::seconds;}
class Clock {
    std::uint32_t rate;
    Timeline timeline;
public:
    explicit Clock(const Project& p):rate(p.sample_rate),timeline(p.time,rate){}
    double secAt(Tick tick) const {
        if(tick<0)return -static_cast<double>(timeline.to_samples(0)-timeline.to_samples(1))*tick/rate;
        return static_cast<double>(timeline.to_samples(tick))/rate;
    }
    Tick tickAt(double sec) const {
        if(sec<0)return -timeline.to_ticks(samples(-sec,rate));
        return timeline.to_ticks(samples(sec,rate));
    }
    double toSec(double x,TimeUnit u,double origin=0) const {
        if(seconds(u))return origin+x;
        const auto t=tickAt(origin),delta=ticks(x);
        if(delta>max_tick-t)throw std::runtime_error("DAWproject nested beat position exceeds supported range");
        return origin+secAt(t+delta)-secAt(t);
    }
    double duration(double x,TimeUnit u,double origin=0) const {return toSec(x,u,origin)-origin;}
    double origin(double at,double offset,TimeUnit u) const {
        return seconds(u)?at-offset:at+secAt(tickAt(at)-ticks(offset))-secAt(tickAt(at));
    }
};

struct Projection {
    const ImportSessionDTO& session;
    CreateResult& result;
    std::stop_token stop;
    persistence::ProjectDocument doc;
    const std::vector<Object>& objects=session.objects;
    std::vector<std::vector<std::size_t>> children;
    std::vector<std::optional<std::size_t>> audioTracks, midiTracks, folders, assets;
    std::map<std::string_view,std::size_t> sourceIds;
    std::ostringstream provenance;
    struct Audio {std::size_t asset{}, index{};double start{}, length{}, offset{}, fadeIn{}, fadeOut{}; Clip clip;};
    std::vector<Audio> audio;
    explicit Projection(const ImportSessionDTO& s,CreateResult& r,std::stop_token token):session(s),result(r),stop(token),children(s.objects.size()),audioTracks(s.objects.size()),midiTracks(s.objects.size()),folders(s.objects.size()),assets(s.objects.size()){
        doc.project.id=new_id();doc.project.title=s.title.empty()?"Imported DAWproject":s.title;
        doc.graph=processing::insert_graph({}); // Valid empty native graph; no imported processors.
        provenance<<(s.sourceFormat=="StudioOneSong"?"STUDIOONESONG 1\n":"DAWPROJECT 1\n")<<std::quoted(s.sourceVersion)<<' '<<std::quoted(s.application)<<' '<<std::quoted(s.applicationVersion)<<'\n';
        for(std::size_t i=0;i<objects.size();++i){const auto& o=objects[i];if(o.parent)children[*o.parent].push_back(i);if(!o.id.empty())sourceIds.emplace(o.id,i);}
        for(std::size_t i=0;i<s.assets.size();++i)if(!s.assets[i].pluginState)assets[s.assets[i].owner]=i;
    }
    void warning(std::string text){warn(result.warnings,std::move(text));}
    void map(std::size_t i,const Id& id){provenance<<std::quoted(id.value)<<' '<<i+1<<' '<<std::quoted(objects[i].id)<<'\n';}
    bool ancestor(std::size_t i,Kind kind) const { for(auto p=objects[i].parent;p;p=objects[*p].parent)if(objects[*p].kind==kind)return true;return false; }
    std::optional<std::size_t> sourceTrack(std::size_t i) const {
        const auto at=objects[i].trackScope;if(!at)return {};
        const auto f=sourceIds.find(objects[*at].trackId); if(f==sourceIds.end()||objects[f->second].kind!=Kind::track)return {};return f->second;
    }
    bool isSong() const {return session.sourceFormat=="StudioOneSong";}
    std::optional<std::uint32_t> songColor(const Object& o) const {
        auto s=get(o,"color");
        if(s.size()!=8)return {};
        std::uint32_t argb{};
        const auto end=s.data()+s.size();
        const auto v=std::from_chars(s.data(),end,argb,16);
        if(v.ec!=std::errc{}||v.ptr!=end)return {};
        return argb&0xffffffu;
    }
    void songTimeMap(){
        auto& project=doc.project;
        bool tempo=false,meter=false;
        for(const auto& o:objects){
            if(o.type=="Attributes"&&o.kind==Kind::transport){
                if(auto rate=integer(o,"sampleRate",8000,768000))project.sample_rate=static_cast<std::uint32_t>(*rate);
                else warning("Song sample rate missing or invalid; using native default.");
            }
            if(o.type=="TempoMapSegment"||o.type=="TimeSignatureMapSegment"){
                if(!o.time||*o.time!=0||o.timeUnit!=TimeUnit::beats)
                    throw std::runtime_error("Song map has a nonzero or unverified segment; import stopped to avoid incorrect musical timing");
                if(o.type=="TempoMapSegment"){
                    if(tempo)throw std::runtime_error("Duplicate Song initial tempo");
                    const auto period=numeric(o,"tempo");
                    if(!period||*period<=0||60.0 / *period<1||60.0 / *period>1000||get(o,"curveType")!="0")
                        throw std::runtime_error("Unsupported Song tempo or tempo curve");
                    project.time.tempos[0].bpm=60.0 / *period;tempo=true;
                }else{
                    if(meter)throw std::runtime_error("Duplicate Song initial meter");
                    auto n=integer(o,"numerator",1,64),d=integer(o,"denominator",1,64);
                    if(!n||!d||(*d&(*d-1)))throw std::runtime_error("Invalid Song initial meter");
                    project.time.meters[0]={1,*n,*d};meter=true;
                }
            }
        }
        if(!tempo||!meter)throw std::runtime_error("Song initial tempo or meter missing");
    }
    void songArrangement(){
        auto& project=doc.project;
        for(std::size_t i=0;i<objects.size();++i){
            checkStop(stop);
            const auto& o=objects[i];
            // Preserve raw values and offsets in a dedicated provenance extension,
            // including unsupported chords and source-only transport metadata.
            provenance<<"RAW "<<i+1<<' '<<std::quoted(o.type)<<' '<<o.properties.size();
            for(const auto& [key,value]:o.properties)provenance<<' '<<std::quoted(key)<<' '<<std::quoted(value);
            provenance<<'\n';
            if(o.kind!=Kind::section&&o.kind!=Kind::chord&&o.kind!=Kind::marker)continue;
            if(!o.time||o.timeUnit!=TimeUnit::beats){
                warning("Song event omitted: unverified time unit ("+o.type+").");continue;
            }
            const auto start=ticks(*o.time);
            if(o.kind==Kind::marker){
                auto id=entity(i,"song-marker");
                project.markers.push_back({id,o.name,start,MarkerKind::generic,songColor(o)});map(i,id);
                continue;
            }
            if(!o.duration||o.durationUnit!=TimeUnit::beats||*o.duration<=0){
                warning("Song event omitted: invalid duration ("+o.type+").");continue;
            }
            const auto end=ticks(*o.time+*o.duration);
            if(end<=start){warning("Song event omitted: span below tick resolution.");continue;}
            if(o.kind==Kind::section){
                if(o.name.empty()){warning("Unnamed Song section omitted.");continue;}
                auto id=entity(i,"song-section");
                project.sections.push_back({id,o.name,start,end,songColor(o).value_or(0x4056D6u)});map(i,id);
            }else{
                if(o.name.empty()||o.name.size()>256){warning("Undecoded Song chord omitted; raw encoding retained.");continue;}
                auto id=entity(i,"song-chord");
                project.chords.push_back({id,o.name,start,end,songColor(o)});map(i,id);
            }
        }
        auto sortLane=[](auto& events){
            std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.start<b.start;});
            Tick last=0;
            for(const auto& e:events){
                if(e.start<last)throw std::runtime_error("Overlapping Song musical events cannot be represented without data loss");
                last=e.end;
            }
        };
        sortLane(project.sections);sortLane(project.chords);
        std::stable_sort(project.markers.begin(),project.markers.end(),[](const auto& a,const auto& b){return a.tick<b.tick;});
    }
    void songAudio(){
        const auto bpm=doc.project.time.tempos.front().bpm;
        const double secondsPerBeat=60.0/bpm;
        // The reader currently accepts only the verified constant-tempo schema.
        for(std::size_t i=0;i<objects.size();++i){
            checkStop(stop);
            const auto& o=objects[i];
            if(o.kind!=Kind::track||o.type!="MediaTrack")continue;
            const bool music=get(o,"mediaType")=="Music";
            Track t;t.id=entity(i,music?"song-midi-track":"song-audio-track");
            t.name=name(o,music?"Imported instrument":"Imported audio");
            t.kind=music?TrackKind::instrument:TrackKind::audio;
            t.color=songColor(o);if(!music)t.input=-1;
            else{t.midi_drum_mode=get(o,"drumMode")=="1";t.midi_monitor=false;}
            if(auto v=numeric(o,"mix.gain");v&&*v>=0&&*v<=Track::Mix::maximum_gain)t.mix.gain=static_cast<float>(*v);
            else if(!get(o,"mix.gain").empty())warning("Unverified Song channel gain: "+t.name);
            if(auto v=numeric(o,"mix.pan");v&&*v>=0&&*v<=1)t.mix.pan=static_cast<float>(*v*2-1);
            else if(!get(o,"mix.pan").empty())warning("Unverified Song channel pan: "+t.name);
            t.mix.mute=get(o,"mix.mute")=="1"||get(o,"mute")=="1";
            t.mix.solo=get(o,"mix.solo")=="1";
            (music?midiTracks:audioTracks)[i]=doc.project.tracks.size();map(i,t.id);
            doc.project.tracks.push_back(std::move(t));
        }
        for(std::size_t i=0;i<objects.size();++i){
            checkStop(stop);
            const auto& o=objects[i];
            if(o.kind!=Kind::clip||o.type!="AudioEvent")continue;
            if(!o.parent||!audioTracks[*o.parent]||!assets[i]||!o.time||!o.duration)
                throw std::runtime_error("Incomplete Song audio placement");
            const auto track=*audioTracks[*o.parent];
            const auto unit=o.durationUnit==TimeUnit::beats?secondsPerBeat:1.0;
            const auto offset=numeric(o,"offset").value_or(0.)*unit;
            const auto length=*o.duration*unit;
            if(offset<0||length<=0)throw std::runtime_error("Invalid Song audio source window");
            const auto speed=numeric(o,"speed").value_or(1.);
            stretch::Parameters parameters;
            if(speed<=0)throw std::runtime_error("Invalid Song clip speed");
            parameters.duration_ratio=1./speed;
            parameters.semitones=numeric(o,"transpose").value_or(0.);
            parameters.cents=numeric(o,"tune").value_or(0.);
            if(stretch::validate(parameters)!=stretch::Status::ok)
                throw std::runtime_error("Song clip stretch parameters outside native range");
            Clip clip;clip.id=entity(i,"song-audio");clip.track=doc.project.tracks[track].id;clip.name=name(o,"Imported audio");
            clip.color=songColor(o);
            if(!clip.color)clip.color=doc.project.tracks[track].color;
            if(get(o,"normalize")=="1"||(!get(o,"modification").empty()&&get(o,"modification")!="0"))
                warning("Song normalization or clip modification not applied: "+clip.name);
            // Song offset is in the stretched/output domain; the native Clip
            // source_offset is in original WAV frames. Scale both offset and
            // visible source span by speed, never the timeline start/duration.
            audio.push_back({*assets[i],i,*o.time*secondsPerBeat,length,offset*speed,0.,0.,std::move(clip)});
        }
    }
    void songMidi(){
        for(std::size_t i=0;i<objects.size();++i){
            checkStop(stop);
            const auto& o=objects[i];
            if(o.kind!=Kind::clip||o.type!="MusicPart")continue;
            if(!o.parent||!midiTracks[*o.parent]||!o.time||!o.duration)
                throw std::runtime_error("Incomplete Song MIDI placement");
            const auto track=*midiTracks[*o.parent];
            const auto offset=numeric(o,"offset").value_or(0.);
            if(offset<0||*o.time<0||*o.duration<=0)throw std::runtime_error("Invalid Song MIDI window");
            MidiClip midi;midi.start=ticks(*o.time);midi.length=ticks(*o.time+*o.duration)-midi.start;
            midi.source_offset=ticks(offset);
            if(midi.length<=0)throw std::runtime_error("Song MIDI part below tick resolution");
            const auto transpose=numeric(o,"transpose").value_or(0.);
            const auto velocity=numeric(o,"velocity").value_or(0.);
            if(transpose!=0||velocity!=0)warning("Song part transpose/velocity modification not applied: "+name(o,"MIDI"));
            for(const auto j:children[i]){
                const auto& note=objects[j];
                if(note.kind!=Kind::note||!note.time||!note.duration)continue;
                const auto pitch=integer(note,"pitch",0,127);
                const auto vel=numeric(note,"velocity");
                const auto channel=integer(note,"channel",0,15);
                if(!pitch||!vel||*vel<=0||*vel>1||*note.time<0||*note.duration<=0||
                   (!get(note,"channel").empty()&&!channel))
                    throw std::runtime_error("Invalid Song MIDI note fields");
                const auto start=ticks(*note.time);
                const auto end=ticks(*note.time+*note.duration);
                // Preserve source-relative positions (including hidden notes)
                // so trimming back to the original region remains possible.
                const auto id=entity(j,"song-note");
                midi.notes.push_back({id,start,std::max<Tick>(1,end-start),*pitch,
                    std::clamp(static_cast<int>(std::lround(*vel*127.)),1,127),channel.value_or(0)});
                map(j,id);
            }
            std::stable_sort(midi.notes.begin(),midi.notes.end(),[](const auto& lhs,const auto& rhs){return lhs.start<rhs.start;});
            Clip clip;clip.id=entity(i,"song-midi");clip.track=doc.project.tracks[track].id;
            clip.name=name(o,"Imported MIDI");clip.color=songColor(o);
            if(!clip.color)clip.color=doc.project.tracks[track].color;
            clip.midi=std::move(midi);map(i,clip.id);doc.project.clips.push_back(std::move(clip));
        }
    }
    void timeMap(){
        auto& map=doc.project.time;
        auto meter=[&](const Object& o){return std::pair{integer(o,"numerator",1,64),integer(o,"denominator",1,64)};};
        std::string_view tempoId,meterId;
        for(const auto& o:objects){
            if(o.type=="Tempo"){tempoId=o.id;if(auto v=numeric(o,"value"); v&&*v>=1&&*v<=1000)map.tempos[0].bpm=*v;else warning("Invalid initial tempo; using 120 BPM.");}
            if(o.type=="TimeSignature"){meterId=o.id;auto [n,d]=meter(o);if(n&&d&&((*d&(*d-1))==0))map.meters[0]={1,*n,*d};else warning("Invalid initial meter; using 4/4.");}
        }
        struct Tempo {double position{},bpm{};TimeUnit unit{};};
        struct Meter {double position{};int n{},d{};TimeUnit unit{};};
        std::vector<Tempo> tempos;std::vector<Meter> meters;
        for(const auto& o:objects){
            if(o.kind!=Kind::point||!o.parent||!o.time||*o.time<0||ancestor(*o.parent,Kind::clip)||o.support==Support::unsupported)continue;
            const auto& lane=objects[*o.parent];const auto target=get(lane,"target.parameter");
            if(lane.type=="TempoAutomation"||(lane.type=="Points"&&!tempoId.empty()&&target==tempoId)){
                auto bpm=numeric(o,"value");if(!bpm||*bpm<1||*bpm>1000){warning("Invalid tempo point omitted.");continue;}
                if(get(o,"interpolation")=="linear")warning("Tempo ramps imported as discrete tempo points.");
                tempos.push_back({*o.time,*bpm,o.timeUnit});
            }else if(lane.type=="TimeSignatureAutomation"||(lane.type=="Points"&&!meterId.empty()&&target==meterId)){
                auto [n,d]=meter(o);if(!n||!d||(*d&(*d-1))){warning("Invalid meter point omitted.");continue;}
                meters.push_back({*o.time,*n,*d,o.timeUnit});
            }
        }
        const bool secTempo=!tempos.empty()&&seconds(tempos.front().unit);
        const bool mixed=std::any_of(tempos.begin(),tempos.end(),[&](const Tempo& t){return seconds(t.unit)!=secTempo;});
        if(mixed)warning("Mixed-unit tempo lanes: points in seconds omitted to preserve beat positions.");
        std::stable_sort(tempos.begin(),tempos.end(),[](const Tempo& a,const Tempo& b){return a.position<b.position;});
        double lastSec=0,beat=0;
        for(const auto& t:tempos){
            if(seconds(t.unit)&&mixed)continue;
            double position=t.position;
            if(seconds(t.unit)){beat+=(t.position-lastSec)*map.tempos.back().bpm/60.;lastSec=t.position;position=beat;}
            const auto tick=ticks(position);
            if(tick==map.tempos.back().tick)map.tempos.back().bpm=t.bpm;
            else map.tempos.push_back({tick,t.bpm});
        }
        const Clock clock(doc.project);
        std::vector<std::pair<Tick,Meter>> ordered;
        for(const auto& p:meters)ordered.emplace_back(seconds(p.unit)?clock.tickAt(p.position):ticks(p.position),p);
        std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.first<b.first;});
        Tick last=0;
        for(const auto& [t,m]:ordered){
            const auto old=map.meters.back();const auto barSize=old.numerator*(4*ppq/old.denominator);
            if((t-last)%barSize!=0){warning("Meter change outside a bar boundary omitted.");continue;}
            const auto bar=old.bar+(t-last)/barSize;
            if(t==last)map.meters.back()={bar,m.n,m.d};else map.meters.push_back({bar,m.n,m.d});
            last=t;
        }
    }
    void tracks(){
        std::vector<bool> hasTrackChild(objects.size());
        for(const auto& o:objects)if(o.kind==Kind::track&&o.parent)hasTrackChild[*o.parent]=true;
        std::vector<bool> inferredAudio(objects.size()), inferredMidi(objects.size());
        for(std::size_t i=0;i<objects.size();++i){
            const auto& o=objects[i];if(o.kind!=Kind::media&&o.type!="Notes")continue;
            if(const auto t=sourceTrack(i);t&&o.support!=Support::unsupported)(o.kind==Kind::media?inferredAudio:inferredMidi)[*t]=true;
        }
        auto& p=doc.project;
        for(std::size_t i=0;i<objects.size();++i){
            const auto& o=objects[i];if(o.kind!=Kind::track||o.support==Support::unsupported)continue;
            auto parent=o.parent;while(parent&&!folders[*parent])parent=objects[*parent].parent;
            std::optional<Id> folder=parent?std::optional{p.folders[*folders[*parent]].id}:std::nullopt;
            auto type=get(o,"contentType");
            if(hasTrackChild[i]||type.find("tracks")!=type.npos){
                const auto id=entity(i,"folder");folders[i]=p.folders.size();p.folders.push_back({id,name(o,"Imported folder"),folder});map(i,id);folder=id;
            }
            bool isMaster=false;
            for(const auto child:children[i])if(objects[child].kind==Kind::channel){const auto role=get(objects[child],"role");isMaster=role=="master"||role=="effect"||role=="submix"||role=="vca";}
            if(isMaster&&!inferredAudio[i]&&!inferredMidi[i])continue;
            const bool hasAudio=inferredAudio[i]||type.find("audio")!=type.npos;
            const bool hasMidi=inferredMidi[i]||type.find("notes")!=type.npos;
            if(hasAudio&&hasMidi)warning("Mixed audio/MIDI track split: "+name(o,"Imported track"));
            auto add=[&](bool midi){
                Track t; t.id=entity(i,midi?"midi-track":"audio-track");t.name=name(o,midi?"Imported instrument":"Imported audio");
                if(hasAudio&&hasMidi)t.name+=midi?" (MIDI)":" (Audio)";
                t.kind=midi?TrackKind::instrument:TrackKind::audio;t.color=color(o);t.folder=folder;
                if(!midi)t.input=-1;
                (midi?midiTracks:audioTracks)[i]=p.tracks.size();map(i,t.id);p.tracks.push_back(std::move(t));
            };
            if(hasAudio)add(false);if(hasMidi)add(true);
            if(!hasAudio&&!hasMidi&&!folders[i])warning("Unsupported track omitted: "+name(o,"unnamed"));
        }
    }
    struct Window {double at{}, begin{}, end{}, origin{};};
    std::optional<Window> window(std::size_t i,const Clock& clock) {
        const auto& o=objects[i];
        if(o.support==Support::unsupported||get(o,"enable")=="false"||get(o,"enable")=="0")return {};
        constexpr double epsilon=1e-4;
        const auto loopA=numeric(o,"loopStart"),loopB=numeric(o,"loopEnd");
        auto len=o.duration;
        const auto play=numeric(o,"playStart").value_or(0.);
        const auto stopAt=numeric(o,"playStop");
        if(!len&&stopAt&&*stopAt>play)len=*stopAt-play;
        if(!o.time||!len||*len<=0)return {};
        if(!get(o,"reference").empty()){warning("Linked clip omitted: "+name(o,"unnamed clip"));return {};}
        std::optional<Window> enclosing;
        for(auto p=o.parent;p;p=objects[*p].parent)if(objects[*p].kind==Kind::clip){enclosing=window(*p,clock);if(!enclosing)return {};break;}
        const auto base=enclosing?enclosing->origin:0.;
        const auto at=clock.toSec(*o.time,o.timeUnit,base);
        const auto end=o.duration?clock.toSec(*o.time+*len,o.timeUnit,base):clock.toSec(*len,o.contentTimeUnit,at);
        const auto origin=clock.origin(at,play,o.contentTimeUnit);
        if(loopA&&loopB&&*loopB>*loopA&&(play+epsilon<*loopA||end>clock.toSec(*loopB,o.contentTimeUnit,origin)+epsilon)){
            warning("Repeating clip omitted: "+name(o,"unnamed clip"));return {};
        }
        const auto a=std::max(at,enclosing?enclosing->begin:0.);
        const auto b=std::min(end,enclosing?enclosing->end:end);
        if(b<=a)return {};
        return Window{at,a,b,origin};
    }
    std::optional<std::size_t> clipParent(std::size_t i) const {
        for(auto p=objects[i].parent;p;p=objects[*p].parent)if(objects[*p].kind==Kind::clip)return p;
        return {};
    }
    void midiClip(std::size_t i,std::size_t track,const Window& w,const Clock& clock){
        const auto& o=objects[i];
        const auto start=clock.tickAt(w.begin),end=clock.tickAt(w.end),origin=clock.tickAt(w.origin);
        if(start<0||end<=start)return;
        MidiClip midi; midi.start=start;midi.length=end-start;midi.source_offset=std::max<Tick>(0,start-origin);
        bool found=false;
        std::vector<std::size_t> stack(children[i].rbegin(),children[i].rend());
        while(!stack.empty()){
            const auto index=stack.back();stack.pop_back();const auto& node=objects[index];
            if(node.kind==Kind::clip||node.kind==Kind::media||node.support==Support::unsupported)continue;
            if(node.type=="Notes")found=true;
            if(node.kind==Kind::note&&node.time&&node.duration){
                const auto begin=clock.tickAt(clock.toSec(*node.time,node.timeUnit,w.origin))-origin;
                const auto stopTick=clock.tickAt(clock.toSec(*node.time+*node.duration,node.timeUnit,w.origin))-origin;
                const auto pitch=integer(node,"key",0,127);const auto velocity=numeric(node,"vel").value_or(1.);
                if(pitch&&velocity>0&&stopTick>begin&&begin>=0&&stopTick<=max_tick){
                    const auto id=entity(index,"note");
                    const auto ch=integer(node,"channel",0,15).value_or(0);
                    midi.notes.push_back({id,begin,stopTick-begin,*pitch,std::clamp(static_cast<int>(std::lround(velocity*127.)),1,127),ch});map(index,id);
                    if(get(node,"channel").empty()&&!defaultChannelWarned){warning("Studio Pro notes without a channel use MIDI channel 1 in the new project.");defaultChannelWarned=true;}
                }
                continue;
            }
            if(node.kind==Kind::automation){
                const auto expr=get(node,"target.expression");const auto cc=integer(node,"target.controller",0,127);
                if(expr=="channelController"&&cc){
                    const auto channel=integer(node,"target.channel",0,15).value_or(0);
                    for(auto child:children[index]){
                        const auto& point=objects[child];const auto value=numeric(point,"value");if(!point.time||!value)continue;
                        const auto offset=clock.tickAt(clock.toSec(*point.time,point.timeUnit,w.origin))-origin;
                        if(offset<0||offset>max_tick)continue;
                        const auto scaled=get(node,"unit")=="normalized"?*value*127.:*value;
                        if(scaled<0||scaled>127)continue;
                        const auto id=entity(child,"cc");midi.events.push_back({id,offset,2,channel,*cc,static_cast<int>(std::lround(scaled))});map(child,id);
                    }
                }else if(!children[index].empty())warning("Unsupported MIDI expression/automation omitted: "+name(o,"unnamed clip"));
                continue;
            }
            for(auto it=children[index].rbegin();it!=children[index].rend();++it)stack.push_back(*it);
        }
        if(!found&&midi.events.empty())return;
        std::stable_sort(midi.notes.begin(),midi.notes.end(),[](const auto& a,const auto& b){return a.start<b.start;});
        std::stable_sort(midi.events.begin(),midi.events.end(),[](const auto& a,const auto& b){return a.start<b.start;});
        Clip clip;clip.id=entity(i,"midi");clip.track=doc.project.tracks[track].id;clip.name=name(o,"Imported MIDI");clip.color=color(o);clip.midi=std::move(midi);
        map(i,clip.id);doc.project.clips.push_back(std::move(clip));
    }
    bool defaultChannelWarned{};
    void audioClip(std::size_t i,std::size_t at,std::size_t track,const Window& w,const Clock& clock){
        const auto& media=objects[i];const auto& clipObject=objects[at];
        if(!assets[i])return;
        double offset=w.begin-w.origin;
        if(media.parent&&objects[*media.parent].type=="Warps"){
            const auto warp=*media.parent;std::optional<double> displacement;std::size_t points=0;bool valid=true;
            for(auto child:children[warp]){
                const auto& point=objects[child];if(point.kind!=Kind::warp)continue;
                const auto content=numeric(point,"contentTime");if(!point.time||!content||!seconds(point.contentTimeUnit)){valid=false;break;}
                const auto diff=*content-clock.duration(*point.time,point.timeUnit,w.origin);
                if(!displacement)displacement=diff;else if(std::abs(diff-*displacement)>0.002){valid=false;break;}
                ++points;
            }
            if(!valid||!displacement||points<2){warning("Nonlinear or stretched audio omitted: "+name(clipObject,"unnamed clip"));return;}
            offset+=*displacement;
        }
        auto begin=w.begin;
        if(offset<0){begin-=offset;offset=0;}
        if(w.end<=begin)return;
        Clip clip;clip.id=entity(i,"audio");clip.track=doc.project.tracks[track].id;clip.name=name(clipObject,"Imported audio");clip.color=color(clipObject);
        for(auto parent=clipObject.parent;parent&&!clip.color;parent=objects[*parent].parent)if(objects[*parent].kind==Kind::clip)clip.color=color(objects[*parent]);
        const auto fadeUnit=get(clipObject,"fadeTimeUnit")=="seconds"?TimeUnit::seconds:get(clipObject,"fadeTimeUnit")=="beats"?TimeUnit::beats:clipObject.timeUnit;
        const auto a=numeric(clipObject,"fadeInTime").value_or(0.),b=numeric(clipObject,"fadeOutTime").value_or(0.);
        if(a<0||b<0)warning("Crossfade not restored: "+clip.name);
        Audio entry{*assets[i],i,begin,w.end-begin,offset,clock.duration(std::max(0.,a),fadeUnit,begin),-clock.duration(-std::max(0.,b),fadeUnit,w.end),std::move(clip)};
        audio.push_back(std::move(entry));
    }
    void arrangement(){
        const Clock clock(doc.project);
        for(std::size_t i=0;i<objects.size();++i){
            checkStop(stop);const auto& o=objects[i];if(o.support==Support::unsupported)continue;
            if(o.kind==Kind::marker&&o.time&&!ancestor(i,Kind::clip)){
                const auto tick=clock.tickAt(clock.toSec(*o.time,o.timeUnit));if(tick<0)continue;
                const auto id=entity(i,"marker");doc.project.markers.push_back({id,name(o,"Marker"),tick,MarkerKind::generic,color(o)});map(i,id);
            }
            if(o.kind==Kind::clip){const auto track=sourceTrack(i);if(!track||!midiTracks[*track])continue;
                if(const auto w=window(i,clock))midiClip(i,*midiTracks[*track],*w,clock);
            }
            if(o.kind==Kind::media&&o.type=="Audio"){
                const auto track=sourceTrack(i), parent=clipParent(i);if(!track||!audioTracks[*track]||!parent){warning("Audio outside a supported arrangement clip omitted.");continue;}
                if(const auto w=window(*parent,clock))audioClip(i,*parent,*audioTracks[*track],*w,clock);
            }
        }
        const auto byTime=[](const Marker& a,const Marker& b){return a.tick<b.tick;};
        std::stable_sort(doc.project.markers.begin(),doc.project.markers.end(),byTime);
    }
};

bool localRegular(const fs::path& path){
    if(!path.is_absolute()||(path.has_root_name()&&path.root_name().native().starts_with(L"\\\\")))return false;
    std::error_code ec;return fs::is_regular_file(path,ec)&&!ec;
}
bool inside(const fs::path& base,const fs::path& file){
    const auto relative=file.lexically_relative(base);return !relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..";
}
std::optional<fs::path> externalAudio(const fs::path& base,std::string_view path){
    const auto ref=fromUtf8(path);
    if(ref.has_root_path()||!base.is_absolute()||base.root_name().native().starts_with(L"\\\\"))return {}; // Explicit selection for non-local references.
    auto current=base;
    for(const auto& part:ref){
        current/=part;
        const auto attributes=GetFileAttributesW(current.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0)return {};
    }
    std::error_code error;
    const auto canonical=fs::canonical(current,error);
    if(error||!inside(base,canonical)||!localRegular(canonical))return {};
    return canonical;
}
void copyFile(const fs::path& from,const fs::path& to,std::stop_token stop){
    const auto size=fs::file_size(from);
    if(size>maxAssetBytes)throw std::runtime_error("Audio file exceeds the 2 GiB import limit");
    std::ifstream in(from,std::ios::binary);std::ofstream out(to,std::ios::binary|std::ios::out);
    if(!in||!out)throw std::runtime_error("Cannot open audio for copying");
    std::array<char,65536> data{};std::uint64_t copied=0;
    while(copied<size){
        checkStop(stop);const auto n=static_cast<std::streamsize>(std::min<std::uint64_t>(data.size(),size-copied));
        if(!in.read(data.data(),n)||!out.write(data.data(),n))throw std::runtime_error("Audio source changed or could not be copied");
        copied+=static_cast<std::uint64_t>(n);
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Audio source grew while being copied");
    out.close();if(!out)throw std::runtime_error("Could not finish copying audio");
}
struct Staging {
    fs::path dir;
    explicit Staging(const fs::path& root){
        dir=root/(".mrs-dawproject-"+new_id().value);
        if(!fs::create_directory(dir))throw std::runtime_error("Could not reserve an import staging directory");
    }
    ~Staging(){if(!dir.empty()){std::error_code ec;fs::remove_all(dir,ec);}}
};
struct Media {
    fs::path source, relative;
    std::string failure;
    bool embedded{};
    std::optional<audio::WavFile> info;
    Sample playbackFrames{};
};
fs::path safeFolderName(std::string_view title){
    auto text=juce::String::fromUTF8(title.data(),static_cast<int>(title.size())).replaceCharacters("\\/:*?\"<>|\r\n\t","_________   ").trim().substring(0,90);
    while(text.endsWithChar('.')||text.endsWithChar(' '))text=text.dropLastCharacters(1);
    if(text.isEmpty())text="Imported DAWproject";
    // Prefix to avoid reserved device names and any ambiguity with native projects.
    return fromUtf8(("Import - "+text).toStdString());
}

CreateResult perform(const CreateRequest& req,std::stop_token stop){
    CreateResult result;
    checkStop(stop);
    const auto source=fs::absolute(req.source).lexically_normal();
    const auto sourceSize=fs::file_size(source);
    const auto sourceModified=fs::last_write_time(source);
    const auto session=MRSProjectImport::read(source);
    Projection p(session,result,stop);
    if(p.isSong()){p.songTimeMap();p.songArrangement();p.songAudio();p.songMidi();}
    else{p.timeMap();p.tracks();p.arrangement();}
    if(!p.isSong()&&p.doc.project.tracks.empty()&&p.doc.project.folders.empty()&&p.doc.project.markers.empty())throw std::runtime_error("No supported tracks or markers found in the export");
    if(std::any_of(session.objects.begin(),session.objects.end(),[](const Object& o){return o.kind==Kind::device;}))p.warning("Plug-ins and mixer settings were not imported. Assign an instrument to hear imported MIDI.");
    for(const auto& warning:session.warnings)if(!warning.starts_with("Studio Pro DAWproject 0.1:"))p.warning(warning);
    const auto base=fs::canonical(source.parent_path());
    std::map<std::string,Media> media;
    for(const auto& item:p.audio){
        const auto& ref=session.assets[item.asset];
        auto [it, inserted]=media.try_emplace(ref.path);if(!inserted)continue;
        auto& m=it->second;
        if(const auto relink=req.relinks.find(ref.path);relink!=req.relinks.end()){
            std::error_code error;
            const auto canonical=fs::canonical(relink->second,error);
            if(!error&&localRegular(canonical))m.source=canonical;
            else m.failure="Selected replacement is missing or not a local file";
        }else if(ref.location==AssetReference::Location::embedded){m.embedded=true;}
        else if(ref.location==AssetReference::Location::externalUnchecked){
            if(auto external=externalAudio(base,ref.path))m.source=*external;
            else m.failure="External media not found at its exact relative path; locate it explicitly";
        }else m.failure=p.isSong()?"Source WAV needs explicit relink":"Embedded media is missing";
        const auto file=m.embedded?fromUtf8(ref.path):m.source;
        if(m.failure.empty()&&!juce::String(file.extension().wstring().c_str()).equalsIgnoreCase(".wav"))m.failure="Only WAV media is supported";
        if(!m.failure.empty())result.missing.push_back({ref.path,m.failure});
        m.relative=fs::path("Media")/("audio-"+std::to_string(media.size())+".wav");
    }
    if(!result.missing.empty()&&!req.skipMissing)return result;
    const auto root=fs::absolute(req.projectsDirectory).lexically_normal();
    fs::create_directories(root);
    if((GetFileAttributesW(root.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)!=0)throw std::runtime_error("Import destination must be a local project directory, not a reparse point");
    Staging stage(root);
    desktop::ensure_project_folders(stage.dir/"project.mrsproject");
    for(auto& [key,m]:media){
        checkStop(stop);
        if(!m.failure.empty())continue;
        const auto target=stage.dir/m.relative;
        try{
            if(m.embedded)DAWprojectReader::copyEmbeddedAudio(source,{{key,target}},stop);
            else copyFile(m.source,target,stop);
            m.info=audio::inspect_wav(target);
            if(m.info->channels>2)throw std::runtime_error("Only mono and stereo WAV media are supported");
            if(m.info->sample_rate!=p.doc.project.sample_rate){
                // The native playback graph requires project-rate WAV. Retain the bit-for-bit
                // source and make a separate playback copy using Studio's existing offline SRC.
                const auto playback=m.relative.parent_path()/(m.relative.stem().wstring()+L"-playback.wav");
                audio::resample_wav_file(target,stage.dir/playback,p.doc.project.sample_rate,stop);
                p.warning("Original sample rate preserved in "+utf8(m.relative)+"; playback uses a project-rate copy.");
                m.relative=playback;
            }
            m.playbackFrames=audio::inspect_wav(stage.dir/m.relative).frame_count;
        }catch(const Cancelled&){throw;}catch(const std::exception& error){
            checkStop(stop);m.info.reset();m.failure=error.what();result.missing.push_back({key,m.failure});
            std::error_code removeError;fs::remove(target,removeError);
            if(removeError)throw std::runtime_error("Could not clean invalid staged audio");
        }
    }
    if(!result.missing.empty()&&!req.skipMissing)return result;
    for(const auto& [ref,reason]:result.missing)p.warning("Skipped audio: "+ref+" — "+reason);
    const auto rate=p.doc.project.sample_rate;
    for(auto& entry:p.audio){
        checkStop(stop);
        auto& m=media.at(session.assets[entry.asset].path);if(!m.info)continue;
        const auto& wav=*m.info;const auto& sourceObject=session.objects[entry.index];
        if(const auto expected=integer(sourceObject,"sampleRate",1,768000);expected&&static_cast<std::uint32_t>(*expected)!=wav.sample_rate)
            p.warning("Audio sample rate differs from source metadata: "+entry.clip.name);
        if(const auto expected=integer(sourceObject,"channels",1,64);expected&&static_cast<std::uint32_t>(*expected)!=wav.channels)
            p.warning("Audio channel count differs from source metadata: "+entry.clip.name);
        // Song speed is source/output; native duration_ratio is output/source.
        // songAudio already converted the output-domain offset to source time.
        stretch::Parameters stretchParams;
        if(p.isSong()){
            stretchParams.duration_ratio=1./numeric(sourceObject,"speed").value_or(1.);
            stretchParams.semitones=numeric(sourceObject,"transpose").value_or(0.);
            stretchParams.cents=numeric(sourceObject,"tune").value_or(0.);
        }
        const auto ratio=stretchParams.duration_ratio;
        const auto available=(static_cast<double>(wav.frame_count)/wav.sample_rate-entry.offset)*ratio;
        if(available<=0){p.warning("Audio trim starts beyond available media: "+entry.clip.name);continue;}
        if(entry.length>available+0.002)p.warning("Audio clip shortened to available media: "+entry.clip.name);
        auto& c=entry.clip;c.start=samples(entry.start,rate);c.length=std::min(samples(entry.start+std::min(available,entry.length),rate)-c.start,samples(available,rate));
        if(c.length<=0)continue;
        c.source_offset=samples(entry.offset,rate);c.source=utf8(m.relative);
        if(p.isSong()&&!stretch::neutral(stretchParams)){
            // Use the native source-window contract. Compute from integer output
            // length to preserve exact timeline endpoints across Save/Open.
            const auto remaining=m.playbackFrames-c.source_offset;
            if(remaining<=0){p.warning("Song stretch source window empty: "+c.name);continue;}
            Sample extent{};
            if(stretch::output_frames(remaining,ratio,extent)!=stretch::Status::ok)
                throw std::runtime_error("Song stretched source length invalid");
            c.length=std::min(c.length,extent);
            if(c.length<=0)continue;
            const auto end=std::min(static_cast<double>(remaining),static_cast<double>(c.length)/ratio);
            // Bound the processed source to the visible span, not the entire
            // remaining tail of a long shared stem. Keep the original WAV intact.
            const auto span=std::min(remaining,static_cast<Sample>(std::ceil(end)));
            c.stretch=ClipStretch{stretchParams,span,0.,end};
        }
        c.fade_in=std::clamp(samples(entry.fadeIn,rate),Sample{},c.length);
        c.fade_out=std::clamp(samples(entry.fadeOut,rate),Sample{},c.length);
        p.map(entry.index,c.id);p.doc.project.clips.push_back(std::move(c));
    }
    p.doc.extensions.push_back({p.isSong()?"SONG":"DAWP",p.provenance.str()});
    p.doc.validate();
    if(p.doc.project.tracks.size()>audio::max_mixer_tracks||p.doc.project.clips.size()>audio::max_voices)
        throw std::runtime_error("Imported arrangement exceeds Studio playback track or clip limits");
    std::vector<Id> playbackTracks;
    for(const auto& track:p.doc.project.tracks)playbackTracks.push_back(track.id);
    (void)audio::compile_midi_clips(p.doc.project,playbackTracks);
    (void)audio::compile_midi_events(p.doc.project,playbackTracks);
    const auto title=safeFolderName(p.doc.project.title);
    // Each retry is a fresh name. Nothing overwrites any existing project.
    for(int suffix=1;suffix<=1000;++suffix){
        checkStop(stop);
        auto stem=title;if(suffix>1)stem+=L" ("+std::to_wstring(suffix)+L")";
        const auto folder=root/stem;
        if(fs::exists(folder))continue;
        auto filename=stem;filename+=L".mrsproject";
        const auto stagedFile=stage.dir/filename;
        persistence::save_project(stagedFile,p.doc);
        const auto finalFile=folder/filename;
        if(fs::file_size(source)!=sourceSize||fs::last_write_time(source)!=sourceModified)
            throw std::runtime_error("Source project changed during import; select it again");
        checkStop(stop);
        if(MoveFileExW(stage.dir.c_str(),folder.c_str(),MOVEFILE_WRITE_THROUGH)){
            stage.dir.clear();result.projectFile=finalFile;result.missing.clear();return result;
        }
        if(!fs::exists(folder))throw std::runtime_error("Could not publish imported project directory");
        if(!fs::remove(stagedFile))throw std::runtime_error("Could not discard a staged project name collision");
    }
    throw std::runtime_error("No free project name found");
}
} // namespace

CreateResult createProject(const CreateRequest& request,std::stop_token stop){
    try{return perform(request,stop);}
    catch(const Cancelled&){CreateResult result;result.cancelled=true;return result;}
    catch(const std::exception& error){CreateResult result;result.cancelled=stop.stop_requested();if(!result.cancelled)result.error=error.what();return result;}
}
} // namespace mrs::project_import