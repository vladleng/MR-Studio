#include <mrs/audio_to_midi.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mrs {
namespace {
// Protocol vocabulary/GM representatives verified against pinned ace-midi.cpp
// instrument_for_program; these are 34 melodic categories plus drums.
constexpr TranscriptionInstrument instruments[]{
    {"acoustic_piano",0},{"electric_piano",2},{"chromatic_percussion",8},
    {"organ",16},{"acoustic_guitar",24},{"clean_electric_guitar",26},
    {"distorted_electric_guitar",29},{"acoustic_bass",32},{"electric_bass",33},
    {"violin",40},{"viola",41},{"cello",42},{"contrabass",43},
    {"orchestral_harp",46},{"timpani",47},{"string_ensemble",48},
    {"synth_strings",50},{"voice",52},{"orchestra_hit",55},{"trumpet",56},
    {"trombone",57},{"tuba",58},{"french_horn",60},{"brass_section",61},
    {"soprano_and_alto_sax",64},{"tenor_sax",66},{"baritone_sax",67},
    {"oboe",68},{"english_horn",69},{"bassoon",70},{"clarinet",71},
    {"flutes",72},{"synth_lead",80},{"synth_pad",88},{"drums",128}
};
constexpr std::size_t maximum_notes=100000;
}
std::span<const TranscriptionInstrument> ace_midi_instruments(){return instruments;}
std::vector<std::string> ace_midi_arguments(std::string model,std::string wav,std::string output){
    for(const auto* value:{&model,&wav,&output})
        if(value->empty()||value->find('\0')!=std::string::npos)
            throw std::invalid_argument("invalid ace-midi path");
    return {"--model",std::move(model),"--transcribe",std::move(wav),
            "--out",std::move(output),"--jsonl","--device","cpu"};
}
PlaceTranscription::PlaceTranscription(std::shared_ptr<const Project> source,Id audio_clip,
                                     std::span<const TranscribedNote> notes,std::optional<TimeMap> time,
                                     std::span<const TranscribedNote> original,std::string provenance,TranscriptionPlacement placement):source_(std::move(source)),time_(std::move(time)){
    if(!source_)throw std::invalid_argument("missing transcription source snapshot");
    source_->validate();
    const auto at=std::find_if(source_->clips.begin(),source_->clips.end(),[&](const auto& c){return c.id==audio_clip;});
    if(at==source_->clips.end()||at->midi)throw std::invalid_argument("transcription needs an audio clip");
    if(at->stretch||std::any_of(source_->ara_bindings.begin(),source_->ara_bindings.end(),
                               [&](const auto& binding){return binding.source==at->source;}))
        throw std::invalid_argument("render stretched/ARA audio before transcription");
    if(notes.size()>maximum_notes)throw std::invalid_argument("too many transcribed notes");
    auto audio=*at;
    if(placement.range_offset<0||placement.range_offset>=audio.length||
       (placement.range_length&&(*placement.range_length<=0||*placement.range_length>audio.length-placement.range_offset))||
       placement.velocity<1||placement.velocity>127||placement.quantize_grid<0||placement.quantize_grid>16*ppq||
       !std::isfinite(placement.quantize_strength)||placement.quantize_strength<0||placement.quantize_strength>1)
        throw std::invalid_argument("invalid transcription placement");
    const auto duration=placement.range_length.value_or(audio.length-placement.range_offset);
    audio.source_offset+=placement.range_offset;
    audio.start=placement.position.value_or(audio.start+placement.range_offset);audio.length=duration;
    if(audio.start<0||audio.start>max_sample-duration)throw std::invalid_argument("invalid transcription drop position");
    if(placement.target_track){
        const auto target=std::find_if(source_->tracks.begin(),source_->tracks.end(),[&](const auto& t){return t.id==*placement.target_track;});
        if(target==source_->tracks.end()||target->kind!=TrackKind::instrument)
            throw std::invalid_argument("transcription target must be an existing instrument track");
        if(!original.empty()||time_)throw std::invalid_argument("stem placement preserves project tempo and creates one clip");
    }
    if(time_)time_->validate();
    const Timeline timeline(time_?*time_:source_->time,source_->sample_rate);
    const auto first=timeline.to_ticks(audio.start),last=timeline.to_ticks(audio.start+audio.length);
    if(last<=first)throw std::invalid_argument("audio range shorter than one MIDI tick");
    const double visible_start=static_cast<double>(audio.source_offset)/source_->sample_rate;
    const double visible_end=static_cast<double>(audio.source_offset+audio.length)/source_->sample_rate;
    std::array<std::vector<MidiNote>,std::size(instruments)> groups;
    for(const auto& note:notes){
        if(note.pitch<0||note.pitch>127||!std::isfinite(note.start)||!std::isfinite(note.end)||
           note.start<0||note.end<=note.start||note.end>static_cast<double>(max_sample)/source_->sample_rate)
            throw std::invalid_argument("invalid transcribed note");
        const auto role=std::find_if(std::begin(instruments),std::end(instruments),[&](const auto& r){return r.role==note.instrument;});
        if(role==std::end(instruments))throw std::invalid_argument("unknown ace-midi instrument");
        const auto onset=std::max(visible_start,note.start),offset=std::min(visible_end,note.end);
        if(offset<=onset)continue;
        const auto tick=[&](double seconds){
            const double delta=(seconds-visible_start)*source_->sample_rate;
            const auto sample=audio.start+std::clamp(static_cast<Sample>(std::llround(delta)),Sample{0},audio.length);
            return std::clamp(timeline.to_ticks(sample),first,last);
        };
        auto begin=tick(onset),end=tick(offset);
        if(placement.quantize_grid){
            const auto nearest=static_cast<Tick>(std::llround(static_cast<double>(begin)/placement.quantize_grid))*placement.quantize_grid;
            const auto shifted=static_cast<Tick>(std::llround((nearest-begin)*placement.quantize_strength));
            const auto length=end-begin;begin=std::clamp(begin+shifted,first,last);end=std::min(last,begin+length);
        }
        // Never invent a minimum-length note; grid changes are explicit opt-in.
        if(end<=begin)continue;
        groups[static_cast<std::size_t>(role-std::begin(instruments))].push_back(
            {new_id(),begin-first,end-begin,note.pitch,placement.velocity,role->program==128?9:0});
    }
    if(placement.target_track&&std::count_if(groups.begin(),groups.end(),[](const auto& g){return !g.empty();})!=1)
        throw std::invalid_argument("stem conversion must return exactly one instrument group");
    for(std::size_t i=0;i<groups.size();++i){
        if(groups[i].empty())continue;
        Track track;track.id=new_id();track.kind=TrackKind::instrument;track.name=std::string(instruments[i].role);
        Clip clip;clip.id=new_id();clip.track=placement.target_track.value_or(track.id);clip.name=audio.name+" - "+track.name;
        clip.midi=MidiClip{first,last-first,0,std::move(groups[i])};
        if(instruments[i].program!=128)
            clip.midi->events.push_back({new_id(),0,3,0,instruments[i].program,0});
        if(!placement.target_track)tracks_.push_back(std::move(track));
        clips_.push_back(std::move(clip));
    }
    if(clips_.empty())throw std::invalid_argument("no transcribed notes inside audio clip");
    if(!original.empty()){
        auto raw_placement=placement;raw_placement.quantize_grid=0;
        PlaceTranscription reference(source_,audio_clip,original,time_,{}, {},raw_placement);
        for(auto& track:reference.tracks_){track.name+=" (original timing)";track.mix.mute=true;tracks_.push_back(std::move(track));}
        for(auto& clip:reference.clips_){clip.name+=" (original timing)";clips_.push_back(std::move(clip));}
    }
    if(!provenance.empty()){
        provenance_=TranscriptionRecord{new_id(),audio_clip,{},std::move(provenance)};
        for(const auto& clip:clips_)provenance_->midi_clips.push_back(clip.id);
    }
}
void PlaceTranscription::apply(Project& project) const{
    if(project!=*source_)throw std::invalid_argument("project changed since transcription started; run again");
    if(time_)project.time=*time_;
    if(provenance_)project.transcription_results.push_back(*provenance_);
    project.tracks.insert(project.tracks.end(),tracks_.begin(),tracks_.end());
    project.clips.insert(project.clips.end(),clips_.begin(),clips_.end());
}
}
