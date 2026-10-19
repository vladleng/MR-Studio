#pragma once
#include <mrs/ai_query.hpp>
#include <mrs/midi_clips.hpp>
#include <variant>

namespace mrs::desktop { class Application; }
namespace mrs::ai {
enum class PermissionMode { read, suggest, edit, automatic };
enum class ToolKind { query, add_marker, rename_marker, set_selection, transpose_notes, set_note_velocity, midi_draft,
    list_ara_capabilities, query_ara_binding, get_ara_status, open_ara_editor, attach_ara, set_ara_bypass, remove_ara, reanalyse_ara };
bool read_tool(ToolKind);
bool ara_action(ToolKind);
enum class Intent { read, suggest, apply };
enum class Block { none, disabled, mode, unknown_tool, live_performance, recording,
    confirmation, preview, undo, midi_contract, automatic_allowlist, stale_policy, midi_playback };
struct PermissionPolicy {
    PermissionMode mode{PermissionMode::suggest};
    bool enabled{true}, stored_plugin_parameters{};
    std::uint32_t automatic_tools{}; // explicit bits, default none
    void validate() const;
    bool operator==(const PermissionPolicy&) const = default;
};
struct RuntimeSafety { bool playing{}, recording{}, count_in{}, live{}; };
struct ExecutionGates { bool confirmed{}, preview{}, undo{}, midi{}; }; // trusted host only
struct Decision { Error error{Error::none}; Block block{Block::none}; bool allowed() const {return error==Error::none;} };
std::string_view permission_name(PermissionMode);
Decision authorize(const PermissionPolicy&,ToolKind,Intent,RuntimeSafety = {},ExecutionGates = {});

struct MarkerAddArgs { std::string name; Tick tick{}; bool operator==(const MarkerAddArgs&) const = default; };
struct MarkerRenameArgs { Id marker; std::string name; bool operator==(const MarkerRenameArgs&) const = default; };
struct SelectionArgs {
    std::vector<EntityRef> entities; std::optional<LoopRange> range;
    bool operator==(const SelectionArgs&) const = default;
};
struct MidiArgs {
    Id clip;std::vector<Id> notes;int value{};
    bool operator==(const MidiArgs&) const = default;
};
enum class MidiDraftMode { insert, replace, edit };
struct DraftNote {std::optional<Id> id;Tick start{},duration{};int pitch{},velocity{},channel{};bool operator==(const DraftNote&)const=default;};
struct DraftCC {std::optional<Id> id;Tick start{};int controller{},value{},channel{};bool operator==(const DraftCC&)const=default;};
struct MidiDraftArgs {
    MidiDraftMode mode{MidiDraftMode::insert};std::optional<Id> track,clip;std::string name;
    Tick start{},end{}; // project ticks; note/CC starts are relative to this range
    std::vector<DraftNote> notes;std::vector<DraftCC> cc;
    bool operator==(const MidiDraftArgs&)const=default;
};
struct AraArgs {Id clip;std::optional<Id> plugin;bool bypass{};bool operator==(const AraArgs&)const=default;};
using ToolArguments=std::variant<Query,MarkerAddArgs,MarkerRenameArgs,SelectionArgs,MidiArgs,MidiDraftArgs,AraArgs>;
struct ToolCall {
    std::uint32_t version{1};
    std::uint64_t request_id{};
    std::string operation_id;
    ContextKey key;
    ToolKind tool{ToolKind::query};
    ToolArguments arguments{Query{}};
    bool operator==(const ToolCall&) const = default;
};
struct ToolDescriptor {
    ToolKind kind;
    std::string_view name, argument_schema, application_route;
    bool project_write{}, transient_selection{};
};
const std::array<ToolDescriptor,15>& tool_registry();
const ToolDescriptor* tool_descriptor(std::string_view);
const ToolDescriptor* tool_descriptor(ToolKind);
std::string tool_envelope_schema();
struct DecodedTool { Error error{Error::none}; std::optional<ToolCall> value; };
DecodedTool decode_tool_call(std::string_view); // strict bounded JSON, unknown/duplicate fields rejected

struct PreparedTool;
class ToolPlan {
public:
    const ToolCall call;
    const std::uint64_t policy_revision;
    const std::size_t affected;
    const Decision apply_decision;
    std::string_view command_name() const {return command_?command_->name():std::string_view{};}
private:
    const std::shared_ptr<const ICommand> command_;
    ToolPlan(ToolCall c,std::uint64_t epoch,std::size_t count,Decision decision,std::shared_ptr<const ICommand> command)
        :call(std::move(c)),policy_revision(epoch),affected(count),apply_decision(decision),command_(std::move(command)){}
    friend struct PreparedTool;
    friend PreparedTool prepare_tool(const ToolCall&,const ProjectState&,const ContextKey&,
        const PermissionPolicy&,std::uint64_t,RuntimeSafety);
    friend class mrs::desktop::Application;
};
enum class PreviewState { preview, accepted, rejected, stale, failed };
struct Change {Id target;std::string property,before,after,scope;};
// Trusted host composition. Preflight runs commands once on a private candidate;
// history keeps the exact result, never a network request or a tool callback.
class ProjectEdit final : public ICommand {
public:
    ProjectEdit(std::shared_ptr<const Project>,const std::vector<std::shared_ptr<const ICommand>>&);
    std::string_view name() const override {return name_;}
    void apply(Project&) const override;
    const std::shared_ptr<const Project>& before() const {return before_;}
    const std::shared_ptr<const Project>& after() const {return after_;}
private:
    std::shared_ptr<const Project> before_,after_;
    std::string name_;
};
class Preview {
public:
    const ContextKey key;
    const std::uint64_t policy_revision;
    const std::vector<Change> changes;
    const std::vector<std::shared_ptr<const ToolPlan>> plans;
    PreviewState state() const {return state_;}
private:
    Preview(ContextKey k,std::uint64_t epoch,std::vector<Change> diff,
        std::vector<std::shared_ptr<const ToolPlan>> commands,std::shared_ptr<const ProjectEdit> edit)
        :key(std::move(k)),policy_revision(epoch),changes(std::move(diff)),plans(std::move(commands)),edit_(std::move(edit)){}
    PreviewState state_{PreviewState::preview};
    std::shared_ptr<const ProjectEdit> edit_;
    friend class mrs::desktop::Application;
};
struct PreviewResult {Error error{Error::none};std::shared_ptr<Preview> value;};
struct PreparedTool { Error error{Error::none}; Block block{Block::none}; std::shared_ptr<const ToolPlan> value; bool duplicate{}; };
PreparedTool prepare_tool(const ToolCall&,const ProjectState&,const ContextKey&,const PermissionPolicy&,
    std::uint64_t policy_revision,RuntimeSafety = {});
} // namespace mrs::ai
