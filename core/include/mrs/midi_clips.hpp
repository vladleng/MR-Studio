#pragma once
#include <mrs/core.hpp>
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
