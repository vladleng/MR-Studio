#include <mrs/drum_maps.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace mrs {
namespace {
constexpr std::size_t max_articulations = 2048, max_aliases = 32;
void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
bool utf8(std::string_view value) {
    for (std::size_t i = 0; i < value.size();) {
        const auto lead = static_cast<unsigned char>(value[i++]);
        if (lead < 0x80) continue;
        const int length = lead >= 0xc2 && lead <= 0xdf ? 1 : lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xf0 && lead <= 0xf4 ? 3 : 0;
        if (!length || value.size() - i < static_cast<std::size_t>(length)) return false;
        const auto first = static_cast<unsigned char>(value[i]);
        if ((lead == 0xe0 && first < 0xa0) || (lead == 0xed && first >= 0xa0) ||
            (lead == 0xf0 && first < 0x90) || (lead == 0xf4 && first >= 0x90)) return false;
        for (int j = 0; j < length; ++j) if ((static_cast<unsigned char>(value[i++]) & 0xc0) != 0x80) return false;
    }
    return true;
}
void text(std::string_view value, std::size_t max, bool optional = false) {
    require((optional || !value.empty()) && value.size() <= max && value.find('\0') == value.npos && utf8(value),
            "invalid drum map text (expected UTF-8)");
}
struct Reader {
    std::istringstream in;
    explicit Reader(std::string_view bytes) : in(std::string(bytes)) { in.imbue(std::locale::classic()); }
    template<class T> T number() {
        T value{}; require(static_cast<bool>(in >> value), "invalid drum map number"); return value;
    }
    std::size_t count(std::size_t limit) {
        const auto n = number<std::size_t>(); require(n <= limit, "invalid drum map count"); return n;
    }
    std::string quoted() {
        std::string result;
        in >> std::ws;
        require(in.peek() == '"' && static_cast<bool>(in >> std::quoted(result)), "invalid drum map string");
        return result;
    }
};
}
void DrumMapProfile::validate() const {
    require(version == drum_map_schema_version, "unsupported drum map schema");
    // Account for the wire representation without constructing it on every project edit.
    std::size_t bytes = std::string_view("MRS_DRUM_MAP \n").size();
    const auto limit = [&] { require(bytes <= max_drum_map_bytes, "drum map byte limit exceeded"); };
    const auto wire_number = [&](int n) {
        if (n < 0) { ++bytes; n = -n; } // callers pass bounded values or -1
        do { ++bytes; n /= 10; } while (n != 0);
    };
    const auto wire_field = [&](std::string_view value, std::size_t max, bool optional = false) {
        text(value, max, optional);
        bytes += value.size() + 3; // quotes and a separator
        bytes += static_cast<std::size_t>(std::count_if(value.begin(), value.end(), [](char ch) { return ch == '"' || ch == '\\'; }));
        limit();
    };
    wire_number(static_cast<int>(version));
    wire_field(id.value, 64); wire_field(name, 256); wire_field(description, 4096, true);
    for (const auto* field : {&vendor, &plugin, &library, &kit, &technique}) wire_field(*field, 256, true);
    std::unordered_set<std::string_view> ids;
    const auto row = [&](const Id& row_id, std::string_view label) {
        wire_field(row_id.value, 64); wire_field(label, 256);
        require(ids.insert(row_id.value).second, "duplicate drum map row ID");
    };
    std::size_t groups = categories.size(), articulations = 0;
    require(groups <= max_drum_map_groups, "too many drum map groups");
    wire_number(static_cast<int>(groups)); ++bytes; // category count and newline
    for (const auto& category : categories) {
        row(category.id, category.name);
        require(category.instruments.size() <= max_drum_map_groups - groups, "too many drum map groups");
        groups += category.instruments.size();
        wire_number(static_cast<int>(category.instruments.size())); ++bytes;
        require(!category.color || *category.color <= 0xffffffu, "invalid drum group color");
        if (category.color) { wire_number(static_cast<int>(*category.color)); ++bytes; }
        for (const auto& instrument : category.instruments) {
            row(instrument.id, instrument.name);
            require(!instrument.color || *instrument.color <= 0xffffffu, "invalid drum instrument color");
            if (instrument.color) { wire_number(static_cast<int>(*instrument.color)); ++bytes; }
            require(instrument.articulations.size() <= max_articulations - articulations, "too many drum map articulations");
            articulations += instrument.articulations.size();
            wire_number(static_cast<int>(instrument.articulations.size())); ++bytes;
            for (const auto& a : instrument.articulations) {
                row(a.id, a.name);
                require(!a.note || (*a.note >= 0 && *a.note < 128), "invalid drum map note");
                require(!a.channel || (*a.channel >= 0 && *a.channel < 16), "invalid drum map channel");
                require(a.aliases.size() <= max_aliases, "too many drum map aliases");
                wire_number(a.note.value_or(-1)); ++bytes;
                wire_number(a.channel.value_or(-1)); ++bytes;
                wire_number(static_cast<int>(a.aliases.size())); ++bytes;
                for (const auto& alias : a.aliases) wire_field(alias, 256);
                require(a.alternate_notes.size() <= 127 && (a.note || a.alternate_notes.empty()), "invalid alternate drum notes");
                std::array<bool, 128> seen{};
                if (a.note) seen[*a.note] = true;
                for (int alt : a.alternate_notes) {
                    require(alt >= 0 && alt < 128 && !seen[alt], "invalid or duplicate alternate drum note");
                    seen[alt] = true;
                    wire_number(alt); ++bytes;
                }
            }
        }
    }
    limit();
}
void DrumMapBinding::validate() const {
    snapshot.validate();
    require(collapsed.size() <= max_drum_map_groups, "too many collapsed drum map groups");
    if (collapsed.empty()) return;
    std::unordered_set<std::string_view> groups;
    for (const auto& category : snapshot.categories) {
        groups.insert(category.id.value);
        for (const auto& instrument : category.instruments) groups.insert(instrument.id.value);
    }
    for (const auto& id : collapsed)
        require(groups.erase(id.value) == 1, "invalid collapsed drum map group");
}
std::vector<Id> drum_map_conflicts(const DrumMapProfile& profile) {
    profile.validate();
    struct Notes { std::size_t total{}, any{}; std::array<std::size_t, 16> channels{}; };
    std::array<Notes, 128> notes{};
    std::vector<const DrumMapArticulation*> assigned;
    for (const auto& c : profile.categories) for (const auto& i : c.instruments) for (const auto& a : i.articulations) {
        if (!a.note) continue;
        assigned.push_back(&a);
        const auto add = [&](int pitch) {
            auto& note = notes[static_cast<std::size_t>(pitch)];
            ++note.total;
            if (a.channel) ++note.channels[static_cast<std::size_t>(*a.channel)];
            else ++note.any;
        };
        add(*a.note);
        for (int alt : a.alternate_notes) add(alt);
    }
    std::vector<Id> conflicts;
    for (const auto* a : assigned) {
        const auto overlaps = [&](int pitch) {
            const auto& note = notes[static_cast<std::size_t>(pitch)];
            return (a->channel ? note.any + note.channels[static_cast<std::size_t>(*a->channel)] : note.total) > 1;
        };
        if (overlaps(*a->note) || std::any_of(a->alternate_notes.begin(), a->alternate_notes.end(), overlaps))
            conflicts.push_back(a->id);
    }
    return conflicts;
}
DrumMapProfile duplicate_drum_map(const DrumMapProfile& source, std::string name) {
    source.validate();
    auto result = source;
    result.id = new_id();
    result.name = std::move(name);
    // Row identity is local to a profile. Keep it across copies, useful for matching related maps.
    result.validate();
    return result;
}
std::string serialize_drum_map(const DrumMapProfile& profile) {
    profile.validate();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    const bool instrumentColors=std::any_of(profile.categories.begin(),profile.categories.end(),[](const auto& c){
        return std::any_of(c.instruments.begin(),c.instruments.end(),[](const auto& i){return i.color.has_value();});
    });
    const bool colored=instrumentColors || std::any_of(profile.categories.begin(),profile.categories.end(),[](const auto& c){return c.color.has_value();});
    bool extended = colored;
    for (const auto& c : profile.categories) for (const auto& i : c.instruments)
        for (const auto& a : i.articulations) extended |= !a.alternate_notes.empty();
    out << "MRS_DRUM_MAP " << (instrumentColors ? 4 : colored ? 3 : extended ? 2 : profile.version) << '\n';
    for (const auto* field : {&profile.id.value, &profile.name, &profile.description, &profile.vendor,
                              &profile.plugin, &profile.library, &profile.kit, &profile.technique})
        out << std::quoted(*field) << ' ';
    out << profile.categories.size() << '\n';
    for (const auto& c : profile.categories) {
        out << std::quoted(c.id.value) << ' ' << std::quoted(c.name) << ' ' << c.instruments.size();
        if(colored) out << ' ' << (c.color ? static_cast<int>(*c.color) : -1);
        out << '\n';
        for (const auto& i : c.instruments) {
            out << std::quoted(i.id.value) << ' ' << std::quoted(i.name) << ' ' << i.articulations.size();
            if(instrumentColors) out << ' ' << (i.color ? static_cast<int>(*i.color) : -1);
            out << '\n';
            for (const auto& a : i.articulations) {
                out << std::quoted(a.id.value) << ' ' << std::quoted(a.name) << ' ' << a.note.value_or(-1)
                    << ' ' << a.channel.value_or(-1) << ' ' << a.aliases.size();
                for (const auto& alias : a.aliases) out << ' ' << std::quoted(alias);
                if (extended) {
                    out << ' ' << a.alternate_notes.size();
                    for (int alt : a.alternate_notes) out << ' ' << alt;
                }
                out << '\n';
            }
        }
    }
    auto bytes = out.str();
    require(bytes.size() <= max_drum_map_bytes, "drum map byte limit exceeded");
    return bytes;
}
DrumMapProfile deserialize_drum_map(std::string_view bytes) {
    require(bytes.size() <= max_drum_map_bytes, "drum map byte limit exceeded");
    Reader r(bytes);
    require(r.number<std::string>() == "MRS_DRUM_MAP", "invalid drum map header");
    const auto version = r.number<std::uint32_t>();
    require(version >= 1 && version <= 4, "unsupported drum map schema");
    DrumMapProfile p;
    for (auto* field : {&p.id.value, &p.name, &p.description, &p.vendor, &p.plugin, &p.library, &p.kit, &p.technique})
        *field = r.quoted();
    const auto categories = r.count(max_drum_map_groups);
    auto groups_left = max_drum_map_groups - categories;
    auto articulations_left = max_articulations;
    for (std::size_t c = 0; c < categories; ++c) {
        DrumMapCategory category{{r.quoted()}, r.quoted(), {}};
        const auto instruments = r.count(groups_left);
        groups_left -= instruments;
        if(version >= 3){const int rgb=r.number<int>();require(rgb>=-1 && rgb<=0xffffff,"invalid drum group color");if(rgb>=0)category.color=static_cast<std::uint32_t>(rgb);}
        for (std::size_t i = 0; i < instruments; ++i) {
            DrumMapInstrument instrument{{r.quoted()}, r.quoted(), {}};
            const auto articulations = r.count(articulations_left);
            articulations_left -= articulations;
            if(version >= 4){const int rgb=r.number<int>();require(rgb>=-1 && rgb<=0xffffff,"invalid drum instrument color");if(rgb>=0)instrument.color=static_cast<std::uint32_t>(rgb);}
            for (std::size_t a = 0; a < articulations; ++a) {
                DrumMapArticulation articulation{{r.quoted()}, r.quoted(), {}, {}, {}};
                const auto note = r.number<int>(), channel = r.number<int>();
                require(note >= -1 && channel >= -1, "invalid drum map assignment");
                if (note != -1) articulation.note = note;
                if (channel != -1) articulation.channel = channel;
                const auto aliases = r.count(max_aliases);
                for (std::size_t j = 0; j < aliases; ++j) articulation.aliases.push_back(r.quoted());
                if (version >= 2) {
                    const auto count = r.count(127);
                    for (std::size_t j = 0; j < count; ++j) articulation.alternate_notes.push_back(r.number<int>());
                }
                instrument.articulations.push_back(std::move(articulation));
            }
            category.instruments.push_back(std::move(instrument));
        }
        p.categories.push_back(std::move(category));
    }
    r.in >> std::ws;
    require(r.in.eof(), "unknown trailing drum map data");
    p.validate();
    return p;
}
DrumMapProfile import_drum_map_table(std::string_view input, char delimiter, std::string name) {
    require(input.size() <= max_drum_map_bytes && input.find('\0') == input.npos, "invalid drum map table size or encoding");
    require(delimiter == ',' || delimiter == '\t', "unsupported drum map delimiter");
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    const auto trim = [](std::string_view value) {
        const auto first = value.find_first_not_of(" \t");
        return first == value.npos ? std::string_view{} : value.substr(first, value.find_last_not_of(" \t") - first + 1);
    };
    DrumMapProfile result; result.id = new_id(); result.name = std::move(name);
    constexpr std::array<std::string_view, 5> keys{"midi_note", "instrument", "articulation", "group", "channel"};
    std::array<std::size_t, keys.size()> columns{}; columns.fill(keys.size());
    std::size_t offset = 0, line = 1, assignments = 0, groups = 0, width = 0;
    while (offset < input.size()) {
        const auto row_line = line;
        const auto fail = [row_line](const char* why) { throw std::invalid_argument("Line " + std::to_string(row_line) + ": " + why); };
        std::vector<std::string> row;
        for (;;) {
            std::string cell;
            if (offset < input.size() && input[offset] == '"') {
                ++offset;
                bool closed = false;
                while (offset < input.size()) {
                    const char ch = input[offset++];
                    if (ch == '"') {
                        if (offset == input.size() || input[offset] != '"') { closed = true; break; }
                        ++offset;
                    }
                    if (ch == '\n') ++line;
                    cell += ch;
                }
                if (!closed) fail("unterminated quoted field");
            } else {
                while (offset < input.size() && input[offset] != delimiter && input[offset] != '\r' && input[offset] != '\n') {
                    if (input[offset] == '"') fail("unexpected quote in unquoted field");
                    cell += input[offset++];
                }
            }
            if (row.size() == keys.size()) fail("too many columns");
            row.push_back(std::move(cell));
            if (offset == input.size()) break;
            const auto next = input[offset++];
            if (next == delimiter) continue;
            if (next != '\r' && next != '\n') fail("expected separator after quoted field");
            if (next == '\r' && offset < input.size() && input[offset] == '\n') ++offset;
            ++line; break;
        }
        if (row.size() == 1 && trim(row.front()).empty()) continue;
        if (!width) {
            width = row.size();
            for (std::size_t i = 0; i < width; ++i) {
                const auto key = std::find(keys.begin(), keys.end(), trim(row[i]));
                if (key == keys.end()) fail("unknown header column");
                auto& column = columns[static_cast<std::size_t>(key - keys.begin())];
                if (column != keys.size()) fail("duplicate header column");
                column = i;
            }
            if (std::any_of(columns.begin(), columns.end() - 1, [&](auto col) { return col == keys.size(); }))
                fail("expected midi_note, instrument, articulation and group columns; channel is optional");
            continue;
        }
        if (row.size() != width) fail("wrong number of fields");
        if (++assignments > max_articulations) fail("too many articulations");
        const auto value = [&](std::size_t key) -> std::string_view {
            return columns[key] < width ? trim(row[columns[key]]) : std::string_view{};
        };
        const auto number = [&](std::size_t key, int low, int high, const char* error) -> std::optional<int> {
            const auto v = value(key);
            if (v.empty()) return {};
            int n{}; const auto end = v.data() + v.size();
            const auto parsed = std::from_chars(v.data(), end, n);
            if (parsed.ec != std::errc{} || parsed.ptr != end || n < low || n > high) fail(error);
            return n - low;
        };
        const auto note = number(0, 0, 127, "midi_note must be 0–127 or blank");
        const auto channel = number(4, 1, 16, "channel must be 1–16 or blank");
        for (const auto k : {1U, 2U, 3U}) if (value(k).empty() || value(k).size() > 256) fail("invalid or missing group, instrument or articulation");
        auto category = std::find_if(result.categories.begin(), result.categories.end(), [&](const auto& c) { return c.name == value(3); });
        if (category == result.categories.end()) {
            if (++groups > max_drum_map_groups) fail("too many groups");
            result.categories.push_back({new_id(), std::string(value(3)), {}});
            category = std::prev(result.categories.end());
        }
        auto instrument = std::find_if(category->instruments.begin(), category->instruments.end(), [&](const auto& i) { return i.name == value(1); });
        if (instrument == category->instruments.end()) {
            if (++groups > max_drum_map_groups) fail("too many groups");
            category->instruments.push_back({new_id(), std::string(value(1)), {}});
            instrument = std::prev(category->instruments.end());
        }
        // Keep all authored rows. Conflicts are advisory and can be resolved in the editor.
        instrument->articulations.push_back({new_id(), std::string(value(2)), note, channel, {}});
    }
    require(assignments != 0, "drum map table has no data rows");
    result.validate();
    return result;
}
void SetTrackDrumMap::apply(Project& project) const {
    const auto track = std::find_if(project.tracks.begin(), project.tracks.end(), [this](const auto& t) { return t.id == track_; });
    require(track != project.tracks.end() && track->kind == TrackKind::instrument, "drum map requires an instrument track");
    track->drum_map = binding_;
}
void SetTrackMidiDrumMode::apply(Project& project) const {
    const auto track = std::find_if(project.tracks.begin(), project.tracks.end(), [this](const auto& t) { return t.id == track_; });
    require(track != project.tracks.end() && track->kind == TrackKind::instrument, "MIDI editor presentation requires an instrument track");
    track->midi_drum_mode = active_;
}
} // namespace mrs
