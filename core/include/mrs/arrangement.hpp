#pragma once
#include <mrs/core.hpp>
namespace mrs {
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
