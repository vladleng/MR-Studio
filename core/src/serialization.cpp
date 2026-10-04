#include <mrs/core.hpp>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace mrs {
namespace {
constexpr std::size_t max_bytes = 16 * 1024 * 1024;
constexpr std::size_t max_entities = 100000;
void tag(std::istream& in, std::string_view expected) {
    std::string value;
    if (!(in >> value) || value != expected) throw std::invalid_argument("invalid snapshot section");
}
std::size_t count(std::istream& in) {
    std::int64_t value{};
    if (!(in >> value) || value < 0 || value > static_cast<std::int64_t>(max_entities))
        throw std::invalid_argument("invalid snapshot count");
    return static_cast<std::size_t>(value);
}
std::string quoted(std::istream& in) {
    std::string value;
    in >> std::ws;
    if (in.peek() != '"' || !(in >> std::quoted(value)))
        throw std::invalid_argument("invalid snapshot string");
    return value;
}
std::optional<Id> parent(std::istream& in) {
    auto value = quoted(in);
    return value.empty() ? std::nullopt : std::optional<Id>{Id{std::move(value)}};
}
void check_stream(std::istream& in) {
    if (!in) throw std::invalid_argument("truncated or malformed snapshot");
}
template<class T> void section(std::ostream& out, std::string_view name, const std::vector<T>& values) {
    if (values.size() > max_entities) throw std::invalid_argument("snapshot entity limit exceeded");
    out << name << ' ' << values.size() << '\n';
}
}
std::string serialize(const Project& p) {
    p.validate();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "MRS_CORE_SNAPSHOT " << p.version << '\n';
    out << "PROJECT " << std::quoted(p.id.value) << ' ' << std::quoted(p.title) << ' '
        << std::quoted(p.artist) << ' ' << p.sample_rate << '\n';
    section(out, "TEMPOS", p.time.tempos);
    for (const auto& point : p.time.tempos) out << point.tick << ' ' << point.bpm << '\n';
    section(out, "METERS", p.time.meters);
    for (const auto& point : p.time.meters)
        out << point.bar << ' ' << point.numerator << ' ' << point.denominator << '\n';
    section(out, "FOLDERS", p.folders);
    for (const auto& folder : p.folders)
        out << std::quoted(folder.id.value) << ' ' << std::quoted(folder.name) << ' '
            << std::quoted(folder.parent ? folder.parent->value : "") << '\n';
    section(out, "TRACKS", p.tracks);
    for (const auto& track : p.tracks) {
        out << std::quoted(track.id.value) << ' ' << std::quoted(track.name) << ' '
            << static_cast<int>(track.kind) << ' ' << std::quoted(track.folder ? track.folder->value : "") << '\n';
        out << track.mix.gain << ' ' << track.mix.pan << ' ' << track.mix.mute << ' ' << track.mix.solo << '\n';
        out << std::quoted(track.output ? track.output->value : "") << '\n';
    }
    section(out, "CLIPS", p.clips);
    for (const auto& clip : p.clips)
        out << std::quoted(clip.id.value) << ' ' << std::quoted(clip.track.value) << ' '
            << std::quoted(clip.name) << ' ' << clip.start << ' ' << clip.length << ' '
            << clip.source_offset << ' ' << std::quoted(clip.source) << '\n';
    section(out, "MARKERS", p.markers);
    for (const auto& marker : p.markers)
        out << std::quoted(marker.id.value) << ' ' << std::quoted(marker.name) << ' '
            << marker.tick << ' ' << static_cast<int>(marker.kind) << '\n';
    section(out, "CHORDS", p.chords);
    for (const auto& chord : p.chords)
        out << std::quoted(chord.id.value) << ' ' << std::quoted(chord.symbol) << ' '
            << chord.start << ' ' << chord.end << '\n';
    section(out, "SECTIONS", p.sections);
    for (const auto& part : p.sections)
        out << std::quoted(part.id.value) << ' ' << std::quoted(part.name) << ' '
            << part.start << ' ' << part.end << ' ' << part.color << '\n';
    out << "MASTER " << p.master_gain << "\nEND\n";
    auto result = out.str();
    if (result.size() > max_bytes) throw std::invalid_argument("snapshot byte limit exceeded");
    return result;
}
Project deserialize(std::string_view bytes) {
    if (bytes.size() > max_bytes) throw std::invalid_argument("snapshot byte limit exceeded");
    std::istringstream in{std::string(bytes)};
    in.imbue(std::locale::classic());
    Project p;
    tag(in, "MRS_CORE_SNAPSHOT");
    in >> p.version;
    check_stream(in);
    const auto input_version = p.version;
    if (input_version < 1 || input_version > schema_version)
        throw std::invalid_argument("unsupported snapshot version");
    p.version = schema_version; // migrate v1 with empty musical lanes

    tag(in, "PROJECT");
    p.id = {quoted(in)};
    p.title = quoted(in);
    p.artist = quoted(in);
    in >> p.sample_rate;
    check_stream(in);
    tag(in, "TEMPOS");
    auto n = count(in);
    p.time.tempos.clear();
    for (std::size_t i = 0; i < n; ++i) {
        TempoPoint point;
        in >> point.tick >> point.bpm;
        check_stream(in);
        p.time.tempos.push_back(point);
    }
    tag(in, "METERS");
    n = count(in);
    p.time.meters.clear();
    for (std::size_t i = 0; i < n; ++i) {
        MeterPoint point;
        in >> point.bar >> point.numerator >> point.denominator;
        check_stream(in);
        p.time.meters.push_back(point);
    }
    tag(in, "FOLDERS");
    n = count(in);
    for (std::size_t i = 0; i < n; ++i) {
        Folder folder;
        folder.id = {quoted(in)};
        folder.name = quoted(in);
        folder.parent = parent(in);
        p.folders.push_back(std::move(folder));
    }
    tag(in, "TRACKS");
    n = count(in);
    for (std::size_t i = 0; i < n; ++i) {
        Track track;
        track.id = {quoted(in)};
        track.name = quoted(in);
        int kind{};
        in >> kind;
        check_stream(in);
        if (kind < 0 || kind > (input_version >= 4 ? 2 : 1)) throw std::invalid_argument("invalid track enum");
        track.kind = static_cast<TrackKind>(kind);
        track.folder = parent(in);
        if (input_version >= 3) {
            int mute{}, solo{};
            in >> track.mix.gain >> track.mix.pan >> mute >> solo;
            check_stream(in);
            if ((mute != 0 && mute != 1) || (solo != 0 && solo != 1)) throw std::invalid_argument("invalid mixer flags");
            track.mix.mute = mute != 0; track.mix.solo = solo != 0;
        }
        if (input_version >= 4) track.output = parent(in);
        p.tracks.push_back(std::move(track));
    }
    tag(in, "CLIPS");
    n = count(in);
    for (std::size_t i = 0; i < n; ++i) {
        Clip clip;
        clip.id = {quoted(in)};
        clip.track = {quoted(in)};
        clip.name = quoted(in);
        in >> clip.start >> clip.length >> clip.source_offset;
        check_stream(in);
        clip.source = quoted(in);
        p.clips.push_back(std::move(clip));
    }
    tag(in, "MARKERS");
    n = count(in);
    for (std::size_t i = 0; i < n; ++i) {
        Marker marker;
        marker.id = {quoted(in)};
        marker.name = quoted(in);
        int kind{};
        in >> marker.tick >> kind;
        check_stream(in);
        if (kind < 0 || kind > 5) throw std::invalid_argument("invalid marker enum");
        marker.kind = static_cast<MarkerKind>(kind);
        p.markers.push_back(std::move(marker));
    }
    if (input_version >= 2) {
        tag(in, "CHORDS");
        n = count(in);
        for (std::size_t i = 0; i < n; ++i) {
            Chord chord;
            chord.id = {quoted(in)}; chord.symbol = quoted(in);
            in >> chord.start >> chord.end;
            check_stream(in);
            p.chords.push_back(std::move(chord));
        }
        tag(in, "SECTIONS");
        n = count(in);
        for (std::size_t i = 0; i < n; ++i) {
            ArrangerSection part;
            part.id = {quoted(in)}; part.name = quoted(in);
            in >> part.start >> part.end >> part.color;
            check_stream(in);
            p.sections.push_back(std::move(part));
        }
    }
    if (input_version >= 3) { tag(in, "MASTER"); in >> p.master_gain; check_stream(in); }
    tag(in, "END");
    in >> std::ws;
    if (!in.eof()) throw std::invalid_argument("unknown trailing snapshot data");
    p.validate();
    return p;
}
} // namespace mrs
