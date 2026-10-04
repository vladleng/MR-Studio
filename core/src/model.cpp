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
void Project::validate() const {
    require(version == schema_version, "unsupported project schema");
    require(sample_rate >= 8000 && sample_rate <= 768000, "invalid sample rate");
    time.validate();
    require(std::isfinite(master_gain) && master_gain >= 0 && master_gain <= 16, "invalid master gain");
    std::unordered_set<std::string> ids;
    const auto add_id = [&ids](const Id& entity) {
        require(!entity.value.empty() && entity.value.size() <= 128, "invalid entity ID");
        require(ids.insert(entity.value).second, "duplicate entity ID");
    };
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
        track.mix.validate();
        add_id(track.id);
        require(track.kind == TrackKind::audio || track.kind == TrackKind::midi, "invalid track kind");
        require(!track.folder || folder_by_id.contains(track.folder->value), "missing track folder");
        track_ids.insert(track.id.value);
    }
    for (const auto& clip : clips) {
        add_id(clip.id);
        require(track_ids.contains(clip.track.value), "missing clip track");
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
