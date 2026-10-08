#include <mrs/core.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace mrs {
void SetMidiOutput::apply(Project& project) const {
    auto it=std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t){return t.id==track_;});
    if(it==project.tracks.end()||it->kind!=TrackKind::instrument)throw std::invalid_argument("MIDI output requires an instrument track");
    it->midi_output=port_;it->midi_output_channel=channel_;
}
void SetMidiInput::apply(Project& project) const {
    auto it=std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t){return t.id==track_;});
    if(it==project.tracks.end()||it->kind!=TrackKind::instrument)throw std::invalid_argument("MIDI input requires an instrument track");
    it->midi_input=port_;it->midi_channel=channel_;it->midi_monitor=monitor_;
}
namespace {
struct EditGuard {
    bool& editing;
    explicit EditGuard(bool& value) : editing(value) {
        if (editing) throw std::logic_error("reentrant project edit");
        editing = true;
    }
    ~EditGuard() { editing = false; }
};
}
AddTrack::AddTrack(Track track) : track_(std::move(track)) {}
void SetTrackMix::apply(Project& project) const {
    auto track = std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t) { return t.id == track_; });
    if (track == project.tracks.end() || track->kind == TrackKind::midi) throw std::invalid_argument("unknown audio mixer channel");
    mix_.validate(); track->mix = mix_;
}
void SetTrackSends::apply(Project& project) const {
    auto track = std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t) { return t.id == track_; });
    if (track == project.tracks.end() || track->kind == TrackKind::midi) throw std::invalid_argument("unknown audio mixer channel");
    track->sends = sends_;
}
void SetInserts::apply(Project& project) const {
    if (!track_) { project.master_inserts=inserts_; return; }
    const auto track=std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t) { return t.id == *track_; });
    if (track == project.tracks.end() || track->kind == TrackKind::midi) throw std::invalid_argument("inserts require an existing audio track or bus");
    track->inserts=inserts_;
}
void SetTrackInput::apply(Project& project) const {
    auto track = std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t) { return t.id == track_; });
    if (track == project.tracks.end() || track->kind != TrackKind::audio) throw std::invalid_argument("input requires an audio track");
    track->input = input_; track->input_stereo = stereo_;
}
void SetTrackMonitoring::apply(Project& project) const {
    const auto track = std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t) { return t.id == track_; });
    if (track == project.tracks.end() || track->kind != TrackKind::audio) throw std::invalid_argument("monitor requires an audio track");
    track->input_monitor = enabled_;
}
void SetHardwareOutput::apply(Project& project) const {
    if (!track_) { project.master_outputs = outputs_; return; }
    auto track = std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t) { return t.id == *track_; });
    if (track == project.tracks.end() || track->kind == TrackKind::midi) throw std::invalid_argument("unknown audio mixer channel");
    track->output.reset(); track->hardware_outputs = outputs_;
}
void SetTrackOutput::apply(Project& project) const {
    auto track = std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t) { return t.id == track_; });
    if (track == project.tracks.end() || track->kind == TrackKind::midi) throw std::invalid_argument("unknown audio mixer channel");
    track->hardware_outputs.clear(); track->output = output_; // candidate validation checks type, references and cycles atomically
}
std::string_view AddTrack::name() const { return "Add track"; }
void AddTrack::apply(Project& project) const { project.tracks.push_back(track_); }
RenameTrack::RenameTrack(Id track, std::string name) : track_(std::move(track)), name_(std::move(name)) {}
std::string_view RenameTrack::name() const { return "Rename track"; }
void RenameTrack::apply(Project& project) const {
    auto found = std::find_if(project.tracks.begin(), project.tracks.end(),
                             [this](const auto& track) { return track.id == track_; });
    if (found == project.tracks.end()) throw std::invalid_argument("unknown track");
    found->name = name_;
}
ProjectStore::ProjectStore(Project project, std::size_t history_limit) : history_limit_(history_limit) {
    project.validate();
    if (history_limit_ == 0 || history_limit_ > 10000) throw std::invalid_argument("invalid history limit");
    project_ = std::make_shared<const Project>(std::move(project));
    undo_.reserve(history_limit_);
    redo_.reserve(history_limit_);
}
ProjectState ProjectStore::state() const {
    return {project_, revision_, !undo_.empty(), !redo_.empty(), command_};
}
std::shared_ptr<const Project> ProjectStore::history_target(bool redo) const {
    if (redo) return redo_.empty() ? nullptr : redo_.back().after;
    return undo_.empty() ? nullptr : undo_.back().before;
}
void ProjectStore::publish() { changes_.publish(state()); }
void ProjectStore::execute(const ICommand& command) {
    {
        EditGuard guard(editing_);
        auto candidate = *project_;
        command.apply(candidate);
        candidate.validate();
        if (candidate == *project_) return; // no-op preserves revision/redo
        auto after = std::make_shared<const Project>(std::move(candidate));
        auto name = std::string(command.name());
        auto current_name = name; // prepare all potentially throwing allocations
        Edit edit{project_, after, std::move(name)};
        if (undo_.size() == history_limit_) undo_.erase(undo_.begin());
        undo_.push_back(std::move(edit));
        redo_.clear();
        project_ = std::move(after);
        command_ = std::move(current_name);
        ++revision_;
    }
    publish();
}
bool ProjectStore::undo() {
    {
        EditGuard guard(editing_);
        if (undo_.empty()) return false;
        auto name = "Undo: " + undo_.back().name;
        const auto before = undo_.back().before;
        redo_.push_back(std::move(undo_.back()));
        undo_.pop_back();
        project_ = before;
        command_ = std::move(name);
        ++revision_;
    }
    publish();
    return true;
}
bool ProjectStore::redo() {
    {
        EditGuard guard(editing_);
        if (redo_.empty()) return false;
        auto name = "Redo: " + redo_.back().name;
        const auto after = redo_.back().after;
        undo_.push_back(std::move(redo_.back()));
        redo_.pop_back();
        project_ = after;
        command_ = std::move(name);
        ++revision_;
    }
    publish();
    return true;
}
Connection ProjectStore::subscribe(std::function<void(const ProjectState&)> callback) {
    return changes_.subscribe(std::move(callback));
}
} // namespace mrs
