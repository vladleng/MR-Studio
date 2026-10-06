#pragma once
#include <mrs/core.hpp>
#include <span>
namespace mrs {
class AddMidiClip final : public ICommand {
public:
    explicit AddMidiClip(Clip c):clip_(std::move(c)){}
    std::string_view name() const override{return "Create MIDI clip";}
    void apply(Project&) const override;
private: Clip clip_;
};
class SetMidiNotes final : public ICommand {
public:
    SetMidiNotes(Id id,std::vector<MidiNote> n):id_(std::move(id)),notes_(std::move(n)){}
    std::string_view name() const override{return "Edit MIDI notes";}
    void apply(Project&) const override;
private: Id id_;std::vector<MidiNote> notes_;
};
int midi_event_value(const MidiChannelEvent&);
void set_midi_event_value(MidiChannelEvent&,int value);
class SetMidiEvents final : public ICommand {
public:
    SetMidiEvents(Id clip,std::vector<MidiChannelEvent> events):clip_(std::move(clip)),events_(std::move(events)){}
    std::string_view name() const override{return "Edit MIDI controller events";}
    void apply(Project&) const override;
private:Id clip_;std::vector<MidiChannelEvent> events_;
};
enum class NoteEditKind {quantize,transpose,velocity,length};
enum class NoteValueMode {set,add,scale};
struct NoteEdit {
    NoteEditKind kind{NoteEditKind::quantize};
    Tick grid{ppq/4};double strength{100};bool starts{true},lengths{false};
    NoteValueMode mode{NoteValueMode::set};double value{};
};
// Non-RT, shared Studio/Live domain operation. Empty selection changes nothing.
std::vector<MidiNote> transform_midi_notes(const MidiClip&,std::span<const Id>,const NoteEdit&);
class EditMidiNotes final : public ICommand {
public:
    EditMidiNotes(Id clip,std::vector<Id> selected,NoteEdit edit):clip_(std::move(clip)),selected_(std::move(selected)),edit_(edit){}
    std::string_view name() const override{return "Musical note edit";}
    void apply(Project&) const override;
private:Id clip_;std::vector<Id> selected_;NoteEdit edit_;
};
class MoveMidiClip final : public ICommand {
public:
    MoveMidiClip(Id id,Id track,Tick start):id_(std::move(id)),track_(std::move(track)),start_(start){}
    std::string_view name() const override{return "Move MIDI clip";}
    void apply(Project&) const override;
private: Id id_,track_;Tick start_;
};
class TrimMidiClip final : public ICommand {
public:
    TrimMidiClip(Id id,Tick start,Tick end):id_(std::move(id)),start_(start),end_(end){}
    std::string_view name() const override{return "Trim MIDI clip";}
    void apply(Project&) const override;
private: Id id_;Tick start_,end_;
};
class SplitMidiClip final : public ICommand {
public:
    SplitMidiClip(Id id,Tick at,Id right):id_(std::move(id)),right_(std::move(right)),at_(at){}
    std::string_view name() const override{return "Split MIDI clip";}
    void apply(Project&) const override;
private: Id id_,right_;Tick at_;
};
class DuplicateClip final : public ICommand {
public:
    DuplicateClip(Id id,Id duplicate):id_(std::move(id)),duplicate_(std::move(duplicate)){}
    std::string_view name() const override{return "Duplicate clip";}
    void apply(Project&) const override;
private: Id id_,duplicate_;
};
class RemoveClip final : public ICommand {
public:
    explicit RemoveClip(Id id):id_(std::move(id)){}
    std::string_view name() const override{return "Delete clip";}
    void apply(Project&) const override;
private: Id id_;
};
}
