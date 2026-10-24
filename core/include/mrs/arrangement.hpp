#pragma once
#include <mrs/core.hpp>
namespace mrs {
double audio_time_ratio(const Clip&) noexcept;
Sample audio_output_offset(const Clip&); // offset in the processed span; raw offset for legacy clips
Sample audio_output_extent(const Clip&); // processed span length; legacy clip length
Sample audio_source_span(const Clip&) noexcept;
void validate_audio_stretch(const Clip&);
void trim_audio_source(Clip&,Sample start,Sample end,Sample raw_source_frames);
class SetAudioStretch final : public ICommand {
public:
    SetAudioStretch(Id id,stretch::Parameters parameters,bool reset=false)
        : id_(std::move(id)), parameters_(parameters), reset_(reset) {}
    std::string_view name() const override { return "Audio clip time / pitch"; }
    void apply(Project&) const override;
private: Id id_; stretch::Parameters parameters_; bool reset_{};
};
// Pure preview: keeps source window and the opposite edge fixed; clamps to supported ratios.
Clip stretch_audio_edge(const Clip&,Sample edge,bool left);
double project_tempo_at(const Project&,Sample position);
class SetAudioClipTiming final : public ICommand {
public:
    SetAudioClipTiming(Clip expected,stretch::Parameters parameters,std::optional<double> tempo,Sample start)
        : expected_(std::move(expected)),parameters_(parameters),tempo_(tempo),start_(start) {}
    std::string_view name() const override { return "Audio clip timing"; }
    void apply(Project&) const override;
private: Clip expected_;stretch::Parameters parameters_;std::optional<double> tempo_;Sample start_;
};
class RemoveTrack final : public ICommand {
public:
    explicit RemoveTrack(Id id) : id_(std::move(id)) {}
    std::string_view name() const override { return "Remove track"; }
    void apply(Project&) const override;
private: Id id_;
};
class ReorderTrack final : public ICommand {
public:
    ReorderTrack(Id id, std::size_t index) : id_(std::move(id)), index_(index) {}
    std::string_view name() const override { return "Reorder track"; }
    void apply(Project&) const override;
private: Id id_; std::size_t index_;
};
class ImportAudio final : public ICommand {
public:
    ImportAudio(std::vector<Track> tracks, std::vector<Clip> clips)
        : tracks_(std::move(tracks)), clips_(std::move(clips)) {}
    std::string_view name() const override { return "Import WAVs"; }
    void apply(Project&) const override;
private: std::vector<Track> tracks_; std::vector<Clip> clips_;
};
class AddRecordedClip final : public ICommand {
public:
    explicit AddRecordedClip(Clip clip) : clip_(std::move(clip)) {}
    std::string_view name() const override { return "Record audio take"; }
    void apply(Project&) const override;
private: Clip clip_;
};
// Audio clip commands change references/ranges only; never modify source files.
class MoveAudioClip final : public ICommand {
public:
    MoveAudioClip(Id id, Id track, Sample start) : id_(std::move(id)), track_(std::move(track)), start_(start) {}
    std::string_view name() const override { return "Move audio clip"; }
    void apply(Project&) const override;
private: Id id_, track_; Sample start_;
};
class TrimAudioClip final : public ICommand {
public:
    TrimAudioClip(Id id, Sample start, Sample end, Sample source_frames)
        : id_(std::move(id)), start_(start), end_(end), frames_(source_frames) {}
    std::string_view name() const override { return "Trim audio clip"; }
    void apply(Project&) const override;
private: Id id_; Sample start_, end_, frames_;
};
class SplitAudioClip final : public ICommand {
public:
    SplitAudioClip(Id id, Sample position, Id right)
        : id_(std::move(id)), right_(std::move(right)), position_(position) {}
    std::string_view name() const override { return "Split audio clip"; }
    void apply(Project&) const override;
private: Id id_, right_; Sample position_;
};
class RemoveAudioClip final : public ICommand {
public:
    explicit RemoveAudioClip(Id id) : id_(std::move(id)) {}
    std::string_view name() const override { return "Remove audio clip"; }
    void apply(Project&) const override;
private: Id id_;
};
Sample snap_to_grid(const Project&, Sample position, Tick grid = ppq/4);
} // namespace mrs
