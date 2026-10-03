#pragma once
#include <mrs/core.hpp>

namespace mrs {
// Atomic musical edit through the existing ProjectStore/Undo contract.
class SetMusicalData final : public ICommand {
public:
    SetMusicalData(TimeMap, std::vector<Chord>, std::vector<ArrangerSection>, std::vector<Marker>);
    std::string_view name() const override { return "Set musical data"; }
    void apply(Project&) const override;
private:
    TimeMap time_;
    std::vector<Chord> chords_;
    std::vector<ArrangerSection> sections_;
    std::vector<Marker> markers_;
};
struct MusicalContext {
    std::shared_ptr<const Project> project; // same immutable snapshot for every workspace
    std::uint64_t revision{};
    TransportState transport;
    Tick tick{};
    std::optional<Chord> current_chord, next_chord;
    std::optional<ArrangerSection> current_section, next_section;
    // Last marker at/before position and first strictly later marker.
    std::optional<Marker> current_marker, next_marker;
    bool operator==(const MusicalContext&) const = default;
};
// One control-thread service shared by workspaces; NEVER used in audio callback.
// Project edits refresh automatically; EngineTransport requires its usual poll pump.
// Marker ties use project order. Gaps do not extend a chord/section.
class MusicalTimeline {
public:
    explicit MusicalTimeline(Services);
    MusicalTimeline(const MusicalTimeline&) = delete;
    MusicalTimeline& operator=(const MusicalTimeline&) = delete;
    const MusicalContext& state() const { return state_; }
    Connection subscribe(std::function<void(const MusicalContext&)>);
    void refresh(); // retry transient mailbox errors on control thread
    void seek_section(const Id&);
    void seek_marker(const Id&);
    bool next_section();
    bool previous_section();
    bool next_marker();
    bool previous_marker();
    void loop_section(const Id&);
    void clear_loop();
private:
    Services services_;
    std::optional<Timeline> timeline_;
    MusicalContext state_;
    std::vector<std::size_t> marker_order_;
    Signal<MusicalContext> changes_;
    bool refreshing_{};
    Connection project_connection_, transport_connection_;
    bool navigate(bool sections, bool forward);
};
Project musical_demo_project();
} // namespace mrs
