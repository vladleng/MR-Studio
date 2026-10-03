#include <mrs/core.hpp>

namespace mrs {
Project demo_project() {
    Project project;
    project.id = {"project-moon-river"};
    project.title = "Moon River";
    project.artist = "Duet Moon River";
    project.time.tempos = {{0, 112.0}, {32 * 4 * ppq, 116.0}};
    project.time.meters = {{1, 4, 4}, {49, 3, 4}};
    project.folders = {{{"folder-rhythm"}, "Rhythm", std::nullopt}};
    project.tracks = {
        {{"track-drums"}, "Drums", TrackKind::audio, Id{"folder-rhythm"}},
        {{"track-bass"}, "Upright bass", TrackKind::audio, Id{"folder-rhythm"}},
        {{"track-keys"}, "Keys", TrackKind::midi, std::nullopt},
        {{"track-guitar"}, "Guitar", TrackKind::audio, std::nullopt}};
    project.clips = {
        {{"clip-drums"}, {"track-drums"}, "Playback", 0, 48000 * 30, 0, "audio/drums.wav"},
        {{"clip-bass"}, {"track-bass"}, "Playback", 0, 48000 * 30, 0, "audio/bass.wav"}};
    project.markers = {
        {{"marker-vocal"}, "Vocal entry", 16 * 4 * ppq, MarkerKind::cue},
        {{"marker-solo"}, "Guitar solo", 32 * 4 * ppq, MarkerKind::navigation}};
    project.validate();
    return project;
}
Services demo_services() {
    auto project = demo_project();
    auto transport = std::make_shared<MockTransport>(Timeline{project.time, project.sample_rate});
    return {std::make_shared<ProjectStore>(std::move(project)), std::move(transport)};
}
} // namespace mrs
