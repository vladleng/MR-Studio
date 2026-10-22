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
    bool& editing;bool transferred{};
    explicit EditGuard(bool& value) : editing(value) {
        if (editing) throw std::logic_error("reentrant project edit");
        editing = true;
    }
    ~EditGuard() { if(!transferred)editing = false; }
};
}
namespace {
Folder& track_folder(Project& p,const Id& id){
    const auto it=std::find_if(p.folders.begin(),p.folders.end(),[&](const auto& f){return f.id==id;});
    if(it==p.folders.end())throw std::invalid_argument("Unknown track folder");
    return *it;
}
void route_folder_member(Project& p,Track& track){
    if(track.kind==TrackKind::midi)return;
    auto parent=track.folder;
    for(std::size_t depth=0;parent&&depth<p.folders.size();++depth){
        const auto& folder=track_folder(p,*parent);
        if(folder.bus&&folder.bus!=track.id){track.output=folder.bus;track.hardware_outputs.clear();return;}
        parent=folder.parent;
    }
}
}
void MoveTracksToFolder::apply(Project& p) const{
    if(folder_)(void)track_folder(p,*folder_);
    for(const auto& id:tracks_){
        const auto it=std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t){return t.id==id;});
        if(it==p.tracks.end())throw std::invalid_argument("Unknown folder member");
        it->folder=folder_;route_folder_member(p,*it);
    }
}
void CreateTrackFolder::apply(Project& p) const{
    if(folder_.name.empty()||folder_.name.size()>4096)throw std::invalid_argument("Invalid folder name");
    if(folder_.bus)throw std::invalid_argument("Link the folder Bus separately");
    p.folders.push_back(folder_);MoveTracksToFolder{tracks_,folder_.id}.apply(p);
}
void UpdateTrackFolder::apply(Project& p) const{
    auto& folder=track_folder(p,id_);
    if(name_!=folder.name&&(name_.empty()||name_.size()>4096))throw std::invalid_argument("Invalid folder name");
    folder.name=name_;folder.collapsed=collapsed_;
}
void RemoveTrackFolder::apply(Project& p) const{
    const auto parent=track_folder(p,id_).parent;
    for(auto& track:p.tracks)if(track.folder==id_)track.folder=parent;
    for(auto& folder:p.folders)if(folder.parent==id_)folder.parent=parent;
    std::erase_if(p.folders,[&](const auto& folder){return folder.id==id_;});
    // Dissolving a folder preserves tracks, clips and the audible Bus routing.
}
void LinkFolderBus::apply(Project& p) const{
    (void)track_folder(p,folder_);
    if(create_){if(!bus_||create_->id!=*bus_||create_->kind!=TrackKind::bus)throw std::invalid_argument("Invalid new folder Bus");p.tracks.push_back(*create_);}
    if(bus_){const auto it=std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t){return t.id==*bus_;});
        if(it==p.tracks.end()||it->kind!=TrackKind::bus)throw std::invalid_argument("Folder output requires a Bus");
        for(const auto& f:p.folders)if(f.id!=folder_&&f.bus==bus_)throw std::invalid_argument("Bus already linked to another folder");}
    track_folder(p,folder_).bus=bus_;
    if(!bus_)return; // detach keeps the existing audible routing
    if(create_)route_folder_member(p,p.tracks.back());
    for(auto& track:p.tracks){auto parent=track.folder;bool member=false;
        for(std::size_t depth=0;parent&&depth<p.folders.size();++depth){if(*parent==folder_){member=true;break;}parent=track_folder(p,*parent).parent;}
        if(member)route_folder_member(p,track);
    }
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
std::string_view ProjectStore::history_name(bool redo) const {
    const auto& history=redo?redo_:undo_;
    return history.empty()?std::string_view{}:std::string_view(history.back().name);
}
void ProjectStore::publish() { changes_.publish(state()); }
class ProjectStore::Transaction final:public IProjectTransaction {
public:
    Transaction(ProjectStore& owner,std::shared_ptr<const Project> next,std::vector<Edit> undo,std::vector<Edit> redo,std::string name)
        :owner_(owner),project_(std::move(next)),undo_(std::move(undo)),redo_(std::move(redo)),name_(std::move(name)),revision_(owner.revision_+1),event_{project_,revision_,!undo_.empty(),!redo_.empty(),name_}{}
    ~Transaction()override{rollback();}
    void commit()noexcept override{if(committed_||finished_)return;swap();committed_=true;}
    void finish()override{if(!committed_||finished_)throw std::logic_error("uncommitted project transaction");owner_.changes_.publish(std::move(event_));finished_=true;owner_.editing_=false;}
    void rollback()noexcept override{if(finished_)return;if(committed_)swap();committed_=false;finished_=true;owner_.editing_=false;}
private:
    void swap()noexcept{project_.swap(owner_.project_);undo_.swap(owner_.undo_);redo_.swap(owner_.redo_);name_.swap(owner_.command_);std::swap(revision_,owner_.revision_);}
    ProjectStore& owner_;std::shared_ptr<const Project> project_;std::vector<Edit> undo_,redo_;std::string name_;std::uint64_t revision_;ProjectState event_;bool committed_{},finished_{};
};
std::unique_ptr<IProjectTransaction> ProjectStore::prepare_transaction(const ICommand* command,std::optional<bool> history){
    EditGuard guard(editing_);auto undo=undo_,redo=redo_;std::shared_ptr<const Project> next;std::string name;
    if(history){auto& from=*history?redo:undo;auto& to=*history?undo:redo;if(from.empty())return {};
        const auto item=from.back();next=*history?item.after:item.before;name=(*history?"Redo: ":"Undo: ")+item.name;to.push_back(item);from.pop_back();
    }else{if(!command)throw std::invalid_argument("missing project command");auto candidate=*project_;command->apply(candidate);candidate.validate();if(candidate==*project_)return {};
        next=std::make_shared<const Project>(std::move(candidate));name=command->name();if(undo.size()==history_limit_)undo.erase(undo.begin());undo.push_back({project_,next,name});redo.clear();}
    undo.reserve(history_limit_);redo.reserve(history_limit_);
    auto result=std::make_unique<Transaction>(*this,std::move(next),std::move(undo),std::move(redo),std::move(name));guard.transferred=true;return result;
}
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
