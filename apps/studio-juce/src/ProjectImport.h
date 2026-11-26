#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <stop_token>

namespace mrs::project_import {
enum class Support { supported, partial, unsupported };
enum class Kind { transport, track, channel, send, device, lane, clip, note, marker, media, automation, point, parameter, warp, section, chord };
enum class TimeUnit { unspecified, beats, seconds };

struct Object {
    Kind kind{};
    std::optional<std::size_t> parent; // Index in source order; independent of optional source IDs.
    std::string id, name, type, trackId; // trackId is stored only where the source declares it.
    std::optional<std::size_t> trackScope; // Inherited declaration, if any (no repeated ID allocation).
    TimeUnit timeUnit{}, contentTimeUnit{}, durationUnit{};
    std::optional<double> time, duration;
    // Source properties without defaulting or converting source data (e.g. colour,
    // MIDI velocity, routing, note expression, play offsets). Never interpreted as code.
    std::vector<std::pair<std::string, std::string>> properties;
    Support support{}; // Preview compatibility, not proof of playback or state restore.
    std::string detail;
};

struct AssetReference {
    enum class Location { embedded, missing, externalUnchecked };
    std::size_t owner{};
    std::string path;
    bool pluginState{};
    Location location{};
};

struct ImportSessionDTO {
    static constexpr unsigned schemaVersion = 1;
    std::string sourceFormat, sourceVersion, application, applicationVersion, title;
    std::vector<Object> objects;
    std::vector<AssetReference> assets;
    std::vector<std::string> warnings;
};

struct Preview {
    std::optional<ImportSessionDTO> session;
    std::string error;
    std::size_t projects{}, tracks{}, clips{}, plugins{}, devices{};
};

struct EmbeddedAudioCopy { std::string member; std::filesystem::path destination; };

class DAWprojectReader {
public:
    // Read only; never extract archive entries or open referenced paths.
    static ImportSessionDTO read(const std::filesystem::path&);
    static void copyEmbeddedAudio(const std::filesystem::path&, const std::vector<EmbeddedAudioCopy>&, std::stop_token);
};

// Studio One source adapter for musical structure; audio/MIDI are not read yet.
class StudioOneSongReader {
public:
    static ImportSessionDTO read(const std::filesystem::path&);
};

class MRSProjectImport {
public:
    static ImportSessionDTO read(const std::filesystem::path&);
    static Preview preview(const std::filesystem::path&);
};
} // namespace mrs::project_import
