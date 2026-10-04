#include <mrs/processing.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace mrs::processing {
void MidiEvent::validate() const {
    if (static_cast<int>(kind) < 0 || static_cast<int>(kind) > static_cast<int>(MidiKind::poly_pressure) ||
        channel > 15 || data1 > 127 || data2 > 127)
        throw std::invalid_argument("invalid MIDI channel/data/kind");
}
bool MidiBuffer::push(MidiEvent event) noexcept {
    if (size == events.size()) return false;
    events[size++] = event;
    return true;
}
std::vector<MidiPort> MockMidiDevice::ports() const {
    return {{{"mock-midi"}, "Mock MIDI duplex", true, true}};
}
void MockMidiDevice::open(const Id& id) {
    if (id != Id{"mock-midi"}) throw std::invalid_argument("unknown MIDI port");
    open_ = true;
}
void MockMidiDevice::close() noexcept { open_ = false; input_.clear(); }
bool MockMidiDevice::receive(MidiEvent& event) {
    if (!open_) throw std::runtime_error("MIDI device closed");
    if (input_.empty()) return false;
    event = input_.front(); input_.pop_front(); return true;
}
void MockMidiDevice::send(MidiEvent event) {
    if (!open_) throw std::runtime_error("MIDI device closed");
    event.validate(); sent_.push_back(event);
}
void MockMidiDevice::inject(MidiEvent event) {
    if (!open_) throw std::runtime_error("MIDI device closed");
    event.validate(); input_.push_back(event);
}
void GraphState::validate() const {
    if (id.value.empty() || id.value.size() > 128 || nodes.size() > max_nodes ||
        edges.size() > max_nodes * max_nodes || midi_routes.size() > 256)
        throw std::invalid_argument("invalid graph size/ID");
    std::unordered_map<std::string,std::size_t> index;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        if (n.id.value.empty() || n.id.value.size() > 128 || n.id == id ||
            !index.emplace(n.id.value,i).second || n.processor_id.empty() ||
            (n.format != ProcessorFormat::native && n.format != ProcessorFormat::vst3) ||
            n.parameters.size() > 4096 || n.plugin.component.size() > 16 * 1024 * 1024 ||
            n.plugin.controller.size() > 16 * 1024 * 1024)
            throw std::invalid_argument("invalid processor node/state");
        if (n.format == ProcessorFormat::vst3) {
            if (n.plugin.class_id.size() != 32 ||
                n.plugin.class_id.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
                throw std::invalid_argument("invalid VST3 class ID");
        }
        std::unordered_set<std::uint32_t> params;
        for (const auto& p : n.parameters)
            if (!std::isfinite(p.value) || !params.insert(p.id).second)
                throw std::invalid_argument("invalid/duplicate parameter");
    }
    const auto lookup = [&](const Id& entity) {
        const auto it = index.find(entity.value);
        if (it == index.end()) throw std::invalid_argument("missing processor reference");
        return it->second;
    };
    std::vector<std::size_t> incoming(nodes.size());
    std::vector<std::vector<std::size_t>> outgoing(nodes.size());
    std::unordered_set<std::string> edge_ids;
    for (const auto& e : edges) {
        const auto to = lookup(e.to);
        if (!std::isfinite(e.gain) || std::abs(e.gain) > 16 ||
            !edge_ids.insert((e.from ? e.from->value : "") + "\n" + e.to.value).second)
            throw std::invalid_argument("invalid/duplicate audio edge");
        if (e.from) { const auto from = lookup(*e.from); outgoing[from].push_back(to); ++incoming[to]; }
    }
    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < nodes.size(); ++i) if (incoming[i] == 0) ready.push_back(i);
    for (std::size_t i = 0; i < ready.size(); ++i)
        for (auto to : outgoing[ready[i]]) if (--incoming[to] == 0) ready.push_back(to);
    if (ready.size() != nodes.size()) throw std::invalid_argument("processor graph cycle");
    std::unordered_set<std::string> output_ids;
    for (const auto& output : outputs) {
        (void)lookup(output);
        if (!output_ids.insert(output.value).second) throw std::invalid_argument("duplicate graph output");
    }
    if (!nodes.empty() && outputs.empty()) throw std::invalid_argument("nonempty graph needs output");
    for (const auto& route : midi_routes) {
        (void)lookup(route.target_node);
        if (route.source_port.value.empty() || route.input_channel < -1 || route.input_channel > 15 ||
            route.output_channel < -1 || route.output_channel > 15 ||
            route.transpose < -127 || route.transpose > 127)
            throw std::invalid_argument("invalid MIDI route");
    }
}
GraphStore::GraphStore(GraphState graph, std::size_t limit) : limit_(limit) {
    if (!limit_ || limit_ > 1024) throw std::invalid_argument("invalid graph history limit");
    graph.validate(); graph_ = std::make_shared<const GraphState>(std::move(graph));
}
GraphSnapshot GraphStore::state() const { return {graph_,revision_,!undo_.empty(),!redo_.empty()}; }
void GraphStore::replace(GraphState next) {
    next.validate();
    if (next == *graph_) return;
    auto candidate = std::make_shared<const GraphState>(std::move(next));
    undo_.push_back(graph_);
    if (undo_.size() > limit_) undo_.erase(undo_.begin());
    redo_.clear(); graph_ = std::move(candidate); ++revision_; changes_.publish(state());
}
bool GraphStore::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(graph_); graph_ = undo_.back(); undo_.pop_back();
    ++revision_; changes_.publish(state()); return true;
}
bool GraphStore::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(graph_); graph_ = redo_.back(); redo_.pop_back();
    ++revision_; changes_.publish(state()); return true;
}
Connection GraphStore::subscribe(std::function<void(const GraphSnapshot&)> callback) {
    return changes_.subscribe(std::move(callback));
}
GraphState insert_graph(std::span<const NativeInsert> inserts) {
    GraphState graph; graph.id={"native-chain"}; graph.patch_name="Inserts";
    std::optional<Id> previous;
    for (const auto& fx : inserts) {
        fx.validate(); NodeState node; node.id=fx.id; node.bypass=fx.bypass;
        if(fx.kind==InsertKind::vst3) {node.format=ProcessorFormat::vst3;node.processor_id=fx.plugin_path;node.plugin={fx.class_id,fx.component_state,fx.controller_state};for(const auto& p:fx.parameters)node.parameters.push_back({p.id,p.value});}
        else if(fx.kind==InsertKind::channel_eq) {node.processor_id="mrs.channel-eq";for(std::uint32_t i=0;i<5;++i){const auto& b=fx.bands[i];node.parameters.push_back({i*4,b.frequency});node.parameters.push_back({i*4+1,b.q});node.parameters.push_back({i*4+2,b.gain});node.parameters.push_back({i*4+3,b.enabled ? 1.f : 0.f});}}
        else if (fx.kind == InsertKind::gain) { node.processor_id="mrs.gain"; node.parameters={{0,fx.gain}}; }
        else { node.processor_id=fx.kind == InsertKind::highpass ? "mrs.highpass" : fx.kind == InsertKind::lowpass ? "mrs.lowpass" : "mrs.eq";
            node.parameters={{0,fx.frequency},{1,fx.q}}; if (fx.kind == InsertKind::eq) node.parameters.push_back({2,fx.gain}); }
        graph.edges.push_back({previous,node.id,1}); previous=node.id; graph.nodes.push_back(std::move(node));
    }
    if (previous) graph.outputs.push_back(*previous);
    graph.validate(); return graph;
}
GraphState demo_graph() {
    GraphState g;
    g.id = {"shared-graph"}; g.patch_name = "Clean";
    NodeState n; n.id = {"gain"}; n.processor_id = "mrs.gain"; n.parameters = {{0,0.5F}};
    g.nodes.push_back(n); g.edges.push_back({std::nullopt,n.id,1}); g.outputs.push_back(n.id);
    g.midi_routes.push_back({{"mock-midi"},n.id,-1,-1,0});
    g.validate(); return g;
}
} // namespace mrs::processing
