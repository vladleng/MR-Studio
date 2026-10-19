#pragma once
#include <mrs/core.hpp>

namespace mrs {
class UpdateMeterMap final:public ICommand {
public:
    UpdateMeterMap(TimeMap expected,std::vector<MeterPoint> value):expected_(std::move(expected)),value_(std::move(value)){}
    std::string_view name()const override{return "Edit meter map";}
    void apply(Project&)const override;
private:TimeMap expected_;std::vector<MeterPoint> value_;
};
class UpdateTempoMap final:public ICommand {
public:
    UpdateTempoMap(TimeMap expected,std::vector<TempoPoint> value):expected_(std::move(expected)),value_(std::move(value)){}
    std::string_view name()const override{return "Edit tempo map";}
    void apply(Project&)const override;
private:TimeMap expected_;std::vector<TempoPoint> value_;
};
class AddSection final : public ICommand {
public:
    explicit AddSection(ArrangerSection value):value_(std::move(value)){}
    std::string_view name() const override{return "Add section";}
    void apply(Project&) const override;
private: ArrangerSection value_;
};
class UpdateSection final : public ICommand {
public:
    UpdateSection(ArrangerSection expected,ArrangerSection value):expected_(std::move(expected)),value_(std::move(value)){}
    std::string_view name() const override{return "Edit section appearance";}
    void apply(Project&) const override;
private: ArrangerSection expected_,value_;
};
class EditSectionGroup final:public ICommand {
public:
    EditSectionGroup(std::vector<ArrangerSection> expected,std::vector<ArrangerSection> values):expected_(std::move(expected)),values_(std::move(values)){}
    std::string_view name()const override{return "Edit section group";}
    void apply(Project&)const override;
private:std::vector<ArrangerSection> expected_,values_;
};
class EditChordGroup final:public ICommand {
public:
    EditChordGroup(std::vector<Chord> expected,std::vector<Chord> values):expected_(std::move(expected)),values_(std::move(values)){}
    std::string_view name()const override{return "Edit chord group";}
    void apply(Project&)const override;
private:std::vector<Chord> expected_,values_;
};
class EditMarkerGroup final:public ICommand {
public:
    EditMarkerGroup(std::vector<Marker> expected,std::vector<Marker> values):expected_(std::move(expected)),values_(std::move(values)){}
    std::string_view name()const override{return "Edit marker group";}
    void apply(Project&)const override;
private:std::vector<Marker> expected_,values_;
};
class CopySection final : public ICommand {
public:
    CopySection(ArrangerSection expected,Id created,Tick start):expected_(std::move(expected)),created_(std::move(created)),start_(start){}
    std::string_view name()const override{return "Copy section";}
    void apply(Project&)const override;
private: ArrangerSection expected_;Id created_;Tick start_;
};
class RemoveSection final : public ICommand {
public:
    explicit RemoveSection(ArrangerSection expected):expected_(std::move(expected)){}
    std::string_view name() const override{return "Remove section";}
    void apply(Project&) const override;
private: ArrangerSection expected_;
};
// ID-targeted chord commands leave all other musical lanes intact. Expected values
// reject stale dialogs/gestures instead of overwriting a newer edit.
class AddChord final : public ICommand {
public:
    explicit AddChord(Chord value) : value_(std::move(value)) {}
    std::string_view name() const override { return "Add chord"; }
    void apply(Project&) const override;
private:
    Chord value_;
};
class UpdateChord final : public ICommand {
public:
    UpdateChord(Chord expected, Chord value) : expected_(std::move(expected)), value_(std::move(value)) {}
    std::string_view name() const override { return "Edit chord"; }
    void apply(Project&) const override;
private:
    Chord expected_, value_;
};
class CopyChord final : public ICommand {
public:
    CopyChord(Chord expected, Id created, Tick start)
        : expected_(std::move(expected)), created_(std::move(created)), start_(start) {}
    std::string_view name() const override { return "Copy chord"; }
    void apply(Project&) const override;
private:
    Chord expected_;
    Id created_;
    Tick start_;
};
class RemoveChord final : public ICommand {
public:
    explicit RemoveChord(Chord expected) : expected_(std::move(expected)) {}
    std::string_view name() const override { return "Remove chord"; }
    void apply(Project&) const override;
private:
    Chord expected_;
};
// Atomic musical edit through the existing ProjectStore/Undo contract.
class AddMarker final : public ICommand {
public:
    explicit AddMarker(Marker value) : value_(std::move(value)) {}
    std::string_view name() const override { return "Add marker"; }
    void apply(Project&) const override;
private: Marker value_;
};
class UpdateMarker final : public ICommand {
public:
    UpdateMarker(Marker expected, Marker value) : expected_(std::move(expected)), value_(std::move(value)) {}
    std::string_view name() const override { return "Edit marker"; }
    void apply(Project&) const override;
private: Marker expected_, value_;
};
class RemoveMarker final : public ICommand {
public:
    explicit RemoveMarker(Marker expected) : expected_(std::move(expected)) {}
    std::string_view name() const override { return "Remove marker"; }
    void apply(Project&) const override;
private: Marker expected_;
};
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
    explicit MusicalTimeline(Services, std::function<void()> navigation_guard = {});
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
    std::function<void()> navigation_guard_;
    Connection project_connection_, transport_connection_;
    bool navigate(bool sections, bool forward);
};
Project musical_demo_project();
} // namespace mrs
