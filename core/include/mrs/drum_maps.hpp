#pragma once
#include <mrs/core.hpp>

namespace mrs {
inline constexpr std::size_t max_drum_map_bytes = 512 * 1024;
inline constexpr std::size_t max_drum_map_groups = 512;

// Returns the IDs of all conflicting articulations in authored order. A channel-
// agnostic assignment overlaps every explicit channel for that note.
std::vector<Id> drum_map_conflicts(const DrumMapProfile&);
DrumMapProfile duplicate_drum_map(const DrumMapProfile&, std::string name);
std::string serialize_drum_map(const DrumMapProfile&);
DrumMapProfile deserialize_drum_map(std::string_view);
// UTF-8 CSV/TSV with a header. Blank note = Unmapped, blank channel = any;
// explicit channels are 1–16 in the table, zero-based in the model.
DrumMapProfile import_drum_map_table(std::string_view bytes, char delimiter, std::string name);

class SetTrackDrumMap final : public ICommand {
public:
    SetTrackDrumMap(Id track, std::optional<DrumMapBinding> binding)
        : track_(std::move(track)), binding_(std::move(binding)) {}
    std::string_view name() const override { return "Set track drum map"; }
    void apply(Project&) const override;
private:
    Id track_;
    std::optional<DrumMapBinding> binding_;
};
} // namespace mrs
