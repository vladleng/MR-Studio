#pragma once
#include <mrs/ai.hpp>
#include <mrs/persistence.hpp>

namespace mrs::ai {
enum class QueryKind { tracks, folders, clips, midi_notes, midi_events, chords, sections,
    markers, tempos, meters, mixer, routing, plugins, plugin_parameters, graph_nodes,
    graph_edges, graph_outputs, midi_routes, live_bindings, live_actions, live_patches,
    automation, analysis, live_performance, graph_parameters, midi_ports, ara_capabilities, ara_binding, ara_status };
// Sanitized host projection. Never holds module/media paths or opaque plugin state.
struct AraCapability {Id id;std::string name,version;bool available{};};
struct AraClipState {Id id,track;std::optional<Id> plugin;std::string status;bool bound{},bypass{},can_open{},can_attach{},can_remove{};};
struct AraReadState {bool enabled{};std::vector<AraCapability> capabilities;std::vector<AraClipState> clips;};
struct Query {
    QueryKind kind{QueryKind::tracks};
    std::optional<Id> id, track, clip, plugin;
    bool master{}, selected_only{};
    std::optional<LoopRange> samples; // project sample range, half-open
    std::optional<std::pair<Tick,Tick>> ticks; // project tick range, half-open
    std::size_t cursor{}, limit{64}, scan_limit{512}, byte_budget{64*1024};
    bool operator==(const Query&) const = default;
};
struct ReadPolicy {
    bool stored_plugin_parameters{}; // explicit opt-in, no DSP/controller calls
};
struct ReadAccess {
    std::shared_ptr<const std::atomic<std::uint64_t>> authority;
    std::uint64_t revision{};
    bool stored_plugin_parameters{};
    bool valid() const {return !authority || authority->load()==revision;}
};
struct Page {
    Error error{Error::none};
    ContextKey key;
    QueryKind kind{};
    std::string json; // versioned allowlist projection, never a project archive
    std::size_t source_items{}, scanned{}, returned{}, next_cursor{};
    bool has_more{}, byte_limited{};
};
class Snapshot {
public:
    const Context context;
    const std::string json;
    const std::size_t token_upper_bound; // UTF-8 bytes: conservative budget, not tokenizer estimate
    const bool truncated;
    const std::vector<Page> pages; // per-query cursors and truncation, same ContextKey
    bool access_valid() const {return access_.valid();}
private:
    const ReadAccess access_;
    Snapshot(Context c,std::string payload,bool partial,std::vector<Page> values,ReadAccess access)
        :context(std::move(c)),json(std::move(payload)),token_upper_bound(json.size()),truncated(partial),pages(std::move(values)),access_(std::move(access)){}
    friend class ReadView;
};
struct SnapshotResult { Error error{Error::none}; std::shared_ptr<const Snapshot> value; };
struct SnapshotBudget { std::size_t bytes{256*1024}, tokens{256*1024}; };
struct ViewResult;
// Capture on the application thread. The frozen view may then be queried off-thread:
// it retains ONLY immutable model ownership and bounded, sanitized Live descriptors.
// No device/plugin calls; no publicly accessible raw model or path-bearing data.
class ReadView {
public:
    const Context& context() const { return context_; }
    bool access_valid() const {return access_.valid();}
    std::string analysis(const JobControl&,std::string_view media_statistics="[]") const;
    Page query(const Query&, ReadPolicy = {}) const;
    SnapshotResult snapshot(const std::vector<Query>&, SnapshotBudget = {}, ReadPolicy = {}) const;
private:
    ReadView() = default;
    Context context_;
    std::shared_ptr<const Project> project_;
    std::shared_ptr<const processing::GraphState> graph_;
    ReadAccess access_;
    std::vector<persistence::SectionPatch> bindings_;
    std::vector<persistence::LiveAction> actions_;
    struct Patch { Id id, graph; std::string name; };
    std::vector<Patch> patches_;
    struct Port { Id id; std::string name; bool input{},output{}; };
    std::vector<Port> ports_;
    AraReadState ara_;
    friend struct ViewResult;
    friend ViewResult capture_view(const ProjectState&,const processing::GraphSnapshot&,
        const persistence::ProjectDocument&,const ContextKey&,TransportState,Selection,ReadAccess,AraReadState);
};
struct ViewResult { Error error{Error::none}; std::shared_ptr<const ReadView> value; };
ViewResult capture_view(const ProjectState&,const processing::GraphSnapshot&,
    const persistence::ProjectDocument&,const ContextKey&,TransportState,Selection = {},ReadAccess = {},AraReadState = {});
std::string_view query_name(QueryKind);
} // namespace mrs::ai
