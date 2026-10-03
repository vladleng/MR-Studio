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
} // namespace mrs
