#include <mrs/core.hpp>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace mrs {
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
}
Id new_id() {
    static std::atomic<std::uint64_t> counter{};
    std::random_device random;
    std::ostringstream out;
    out << "mrs-" << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i) out << std::setw(8) << random();
    out << '-' << ++counter;
    return Id{out.str()};
}
void TimeMap::validate() const {
    require(!tempos.empty() && tempos.front().tick == 0, "tempo map must start at tick 0");
    Tick last = -1;
    for (const auto& p : tempos) {
        require(p.tick > last && p.tick <= max_tick, "tempo positions must be strictly increasing");
        require(std::isfinite(p.bpm) && p.bpm >= 1 && p.bpm <= 1000, "invalid tempo");
        last = p.tick;
    }
    require(!meters.empty() && meters.front().bar == 1, "meter map must start at bar 1");
    std::int64_t last_bar = 0;
    Tick start_tick = 0;
    MeterPoint previous;
    for (const auto& p : meters) {
        require(p.bar > last_bar && p.bar <= max_tick + 1, "invalid meter bar order");
        require(p.numerator >= 1 && p.numerator <= 64, "invalid meter numerator");
        require(p.denominator >= 1 && p.denominator <= 64 &&
                (p.denominator & (p.denominator - 1)) == 0, "invalid meter denominator");
        if (last_bar != 0) {
            start_tick += (p.bar - last_bar) * previous.numerator * (4 * ppq / previous.denominator);
            require(start_tick <= max_tick, "meter position out of range");
        }
        previous = p;
        last_bar = p.bar;
    }
}
void Track::Mix::validate() const {
    require(std::isfinite(gain) && gain >= 0 && gain <= 16 &&
        std::isfinite(pan) && pan >= -1 && pan <= 1, "invalid track mix");
}
void CabIr::validate() const {
    require(name.size()<=4096 && sample_rate>=8000 && sample_rate<=192000 && (channels==1 || channels==2), "Cab IR requires mono/stereo at 8–192 kHz");
    require(!samples.empty() && samples.size()%channels==0 && samples.size()/channels<=sample_rate, "Cab IR length must be greater than zero and at most one second");
    bool nonzero=false; for(float v:samples){require(std::isfinite(v) && std::abs(v)<=16,"invalid Cab IR sample");nonzero|=v!=0;}require(nonzero,"Cab IR is silent");
    require(std::isfinite(mix)&&mix>=0&&mix<=1&&std::isfinite(low_cut)&&low_cut>=20&&low_cut<=20000&&std::isfinite(high_cut)&&high_cut>=20&&high_cut<=20000&&low_cut<high_cut,"invalid Cab IR controls");
}
void NativeInsert::validate() const {
    if(kind==InsertKind::cab_ir)ir.validate();
    for (const auto& b : bands) require(std::isfinite(b.frequency) && b.frequency>=20 && b.frequency<=20000 && std::isfinite(b.gain) && b.gain>=-24 && b.gain<=24 && std::isfinite(b.q) && b.q>=0.1f && b.q<=10,"invalid EQ band");
    require(component_state.size()<=1024*1024 && controller_state.size()<=1024*1024 && parameters.size()<=4096,"plugin state budget exceeded");
    if (kind==InsertKind::vst3) {
        require(!plugin_path.empty() && plugin_path.size()<=32768 && plugin_name.size()<=4096 && class_id.size()==32 && class_id.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"invalid VST3 identity");
        for (std::size_t i=0;i<parameters.size();++i) { const auto& p=parameters[i]; require(std::isfinite(p.value) && p.value>=0 && p.value<=1,"invalid VST3 parameter"); for(std::size_t j=0;j<i;++j) require(p.id!=parameters[j].id,"duplicate VST3 parameter"); }
    }
    require(kind >= InsertKind::gain && kind <= InsertKind::cab_ir,"invalid insert kind");
    require(std::isfinite(gain) && (kind == InsertKind::eq ? gain >= -24 && gain <= 24 : gain >= 0 && gain <= 4),"invalid insert gain");
    require(std::isfinite(frequency) && frequency >= 20 && frequency <= 20000 && std::isfinite(q) && q >= 0.1f && q <= 10,"invalid filter frequency/Q");
}
void Project::validate() const {
    require(version == schema_version, "unsupported project schema");
    require(sample_rate >= 8000 && sample_rate <= 768000, "invalid sample rate");
    time.validate();
    require(std::isfinite(master_gain) && master_gain >= 0 && master_gain <= 16, "invalid master gain");
    const auto physical = [](const std::vector<int>& outputs) {
        require(outputs.size() <= 2,"output route must be mono/stereo");
        for (const auto index : outputs) require(index >= 0 && index < 64,"invalid physical output index");
        require(outputs.size() != 2 || outputs[0] != outputs[1],"duplicate physical output");
    };
    physical(master_outputs);
    std::unordered_set<std::string> ids;
    const auto add_id = [&ids](const Id& entity) {
        require(!entity.value.empty() && entity.value.size() <= 128, "invalid entity ID");
        require(ids.insert(entity.value).second, "duplicate entity ID");
    };
    std::size_t insert_count{};
    const auto inserts = [&](const std::vector<NativeInsert>& chain) {
        require(chain.size() <= 8,"insert chain supports up to eight effects"); insert_count += chain.size();
        require(insert_count <= 32,"project supports up to 32 native inserts");
        for (const auto& effect : chain) { effect.validate(); add_id(effect.id); }
    };
    inserts(master_inserts);
    add_id(id);
    std::unordered_map<std::string, const Folder*> folder_by_id;
    for (const auto& folder : folders) {
        add_id(folder.id);
        folder_by_id.emplace(folder.id.value, &folder);
    }
    for (const auto& folder : folders) {
        std::unordered_set<std::string> visited{folder.id.value};
        auto parent = folder.parent;
        while (parent) {
            require(folder_by_id.contains(parent->value), "missing parent folder");
            require(visited.insert(parent->value).second, "folder hierarchy cycle");
            parent = folder_by_id.at(parent->value)->parent;
        }
    }
    std::unordered_set<std::string> track_ids;
    for (const auto& track : tracks) {
        track.mix.validate(); physical(track.hardware_outputs); inserts(track.inserts);
        require(track.kind != TrackKind::midi || track.inserts.empty(),"audio inserts require an audio track or bus");
        require(track.hardware_outputs.empty() || (!track.output && track.kind != TrackKind::midi),"hardware output conflicts with bus/MIDI routing");
        add_id(track.id);
        require(track.kind == TrackKind::audio || track.kind == TrackKind::midi || track.kind == TrackKind::bus || track.kind == TrackKind::instrument, "invalid track kind");
        require(track.midi_input.size()<=256 && track.midi_channel>=-1 && track.midi_channel<=15,"invalid MIDI input");
        require(track.kind==TrackKind::instrument || (track.midi_input.empty()&&track.midi_channel==-1&&track.midi_monitor),"MIDI input requires an instrument track");
        require(track.kind!=TrackKind::instrument || track.inserts.empty() || track.inserts.front().kind==InsertKind::vst3,"instrument must be the first VST3 insert");
        require(!track.folder || folder_by_id.contains(track.folder->value), "missing track folder");
        track_ids.insert(track.id.value);
    }
    std::unordered_map<std::string, const Track*> channels;
    for (const auto& track : tracks) channels.emplace(track.id.value,&track);
    std::unordered_map<std::string,int> color;
    for (const auto& track : tracks) {
        require(track.kind != TrackKind::midi || (!track.output && track.sends.empty()), "MIDI audio routing is not supported");
        require(!track.input_stereo || (track.kind == TrackKind::audio && track.input >= 0 && track.input < 63),"stereo input requires adjacent physical channels");
        require(!track.input_monitor || track.kind == TrackKind::audio,"only audio tracks can monitor input");
        require(track.input >= -2 && track.input < 64 && (track.kind == TrackKind::audio || track.input == -2), "invalid track input");
        require(track.sends.size() <= 8, "at most eight sends per channel");
        std::unordered_set<std::string> destinations;
        for (const auto& send : track.sends) {
            require(std::isfinite(send.gain) && send.gain >= 0 && send.gain <= 16, "invalid send gain");
            require(destinations.insert(send.bus.value).second, "duplicate send destination");
        }
    }
    // Iterative DFS avoids recursion limits for imported projects. Main output and
    // sends participate in the same DAG, including disabled/zero-level sends.
    for (const auto& root : tracks) {
        std::vector<std::pair<const Track*,std::size_t>> stack;
        if (color[root.id.value] == 2) continue;
        color[root.id.value] = 1; stack.emplace_back(&root,0);
        while (!stack.empty()) {
            auto& [track,index] = stack.back();
            if (index == track->sends.size()+1) { color[track->id.value] = 2; stack.pop_back(); continue; }
            const auto edge_index = index++;
            const auto destination = edge_index == 0 ? track->output : std::optional<Id>{track->sends[edge_index-1].bus};
            if (!destination) continue;
            require(channels.contains(destination->value), "missing output/send bus");
            const auto* bus = channels.at(destination->value);
            require(bus->kind == TrackKind::bus, "output/send destination must be a bus");
            require(color[bus->id.value] != 1, "audio routing cycle");
            if (color[bus->id.value] == 0) { color[bus->id.value] = 1; stack.emplace_back(bus,0); }
        }
    }
    for (const auto& clip : clips) {
        add_id(clip.id);
        require(track_ids.contains(clip.track.value), "missing clip track");
        require(channels.at(clip.track.value)->kind != TrackKind::bus, "bus cannot contain clips");
        require(clip.start >= 0 && clip.start <= max_sample, "invalid clip start");
        require(clip.length > 0 && clip.length <= max_sample - clip.start, "invalid clip length");
        require(clip.source_offset >= 0 && clip.source_offset <= max_sample - clip.length,
                "invalid source offset");
    }
    const auto lane = [&add_id](const auto& events) {
        Tick last_end = 0;
        for (const auto& event : events) {
            add_id(event.id);
            require(event.start >= last_end && event.start >= 0, "unordered or overlapping musical lane");
            require(event.end > event.start && event.end <= max_tick, "invalid musical range");
            last_end = event.end;
        }
    };
    lane(chords);
    lane(sections);
    for (const auto& chord : chords)
        require(!chord.symbol.empty() && chord.symbol.size() <= 256, "invalid chord symbol");
    for (const auto& section : sections) {
        require(!section.name.empty(), "empty section name");
        require(section.color <= 0xFFFFFF, "invalid section color");
    }
    for (const auto& marker : markers) {
        add_id(marker.id);
        require(marker.tick >= 0 && marker.tick <= max_tick, "invalid marker position");
        const auto kind = static_cast<int>(marker.kind);
        require(kind >= 0 && kind <= static_cast<int>(MarkerKind::navigation), "invalid marker kind");
    }
}
Connection::Connection(std::function<void()> disconnect) : disconnect_(std::move(disconnect)) {}
Connection::Connection(Connection&& other) noexcept
    : disconnect_(std::exchange(other.disconnect_, {})) {}
Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        disconnect();
        disconnect_ = std::exchange(other.disconnect_, {});
    }
    return *this;
}
Connection::~Connection() { disconnect(); }
void Connection::disconnect() noexcept {
    if (disconnect_) {
        auto action = std::exchange(disconnect_, {});
        try { action(); } catch (...) {}
    }
}
} // namespace mrs
