#include <mrs/processing.hpp>
#include <mrs/audio.hpp>
#include <mrs/delay.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <map>

namespace mrs::processing {
namespace {
template<class T> void sort_offsets(T* events, std::size_t size) noexcept {
    // Stable bounded insertion sort; std::stable_sort may allocate.
    for (std::size_t i = 1; i < size; ++i) {
        auto value = events[i];
        auto j = i;
        while (j && events[j-1].offset > value.offset) { events[j] = events[j-1]; --j; }
        events[j] = value;
    }
}
struct MidiCommand { std::uint16_t node{}; MidiEvent event; };
struct ParamCommand { std::uint16_t node{}; ParameterChange event; };
struct ParamBatch {std::array<ParamCommand,256> changes{};std::size_t size{};};
}
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // intentional SPSC alignment propagated into Impl
#endif
struct PreparedGraph::Impl {
    struct Edge { std::optional<std::size_t> from; float gain; CompensationDelay delay; };
    struct Node {
        std::unique_ptr<IProcessor> processor;
        std::vector<ParameterInfo> infos;
        std::vector<float> audio;
        std::vector<Edge> inputs;
        MidiBuffer midi, output;
        std::array<ParameterChange,event_capacity> parameters{};
        std::size_t parameter_count{};
        bool bypass{};
    };
    GraphSnapshot snapshot;
    std::atomic<bool> profiling{};
    std::array<TimingCounter,max_nodes> timings;
    std::map<std::string,std::vector<ParameterValue>> last_parameters;
    ProcessConfig config;
    LatencyReport report;
    std::vector<Node> nodes;
    std::vector<std::size_t> order, outputs;
    std::vector<CompensationDelay> output_delays;
    std::unordered_map<std::string,std::size_t> index;
    audio::SpscQueue<MidiCommand,1024> midi_queue;
    audio::SpscQueue<ParamCommand,1024> parameter_queue;
    audio::SpscQueue<ParamBatch,64> batch_queue;
    audio::SpscQueue<MidiOutput,4096> output_queue;
    std::atomic<bool> panic_requested{};
    std::atomic<std::uint64_t> control_revision{};
    std::atomic<bool> external_schedule{};
    std::atomic<std::uint64_t> dropped_midi{}, dropped_parameters{}, invalid{}, output_overflows{}, panics{};
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif
PreparedGraph::PreparedGraph(GraphSnapshot snapshot, ProcessConfig config, ProcessorFactory factory)
    : impl_(std::make_unique<Impl>()) {
    if (!snapshot.graph || !factory || config.sample_rate < 8000 || config.sample_rate > 768000 ||
        !config.channels || config.channels > 64 || !config.max_block || config.max_block > 65536)
        throw std::invalid_argument("invalid processor preparation");
    snapshot.graph->validate();
    const auto& state = *snapshot.graph;
    const auto samples = static_cast<std::size_t>(config.max_block) * config.channels;
    if (samples * sizeof(float) * state.nodes.size() > 128 * 1024 * 1024)
        throw std::invalid_argument("processor graph scratch budget exceeded");
    auto& p = *impl_; p.snapshot = std::move(snapshot); p.config = config;
    p.nodes.resize(state.nodes.size());
    for (std::size_t i = 0; i < state.nodes.size(); ++i) p.index.emplace(state.nodes[i].id.value,i);
    for (std::size_t i = 0; i < state.nodes.size(); ++i) {
        auto& node = p.nodes[i]; const auto& saved = state.nodes[i];
        node.bypass = saved.bypass;
        p.last_parameters[saved.id.value]=saved.parameters;
        node.processor = factory(saved);
        if (!node.processor) throw std::invalid_argument("processor factory returned null");
        auto node_config=config;node_config.instrument=config.instrument&&i==0;
        node.processor->prepare(node_config);
        node.processor->restore(saved.plugin);
        node.infos = node.processor->parameters();
        if (node.infos.size() > 4096) throw std::invalid_argument("too many processor parameters");
        for (std::size_t a = 0; a < node.infos.size(); ++a) {
            const auto& info = node.infos[a];
            if (!std::isfinite(info.minimum) || !std::isfinite(info.maximum) ||
                !std::isfinite(info.initial) || info.minimum > info.maximum ||
                info.initial < info.minimum || info.initial > info.maximum)
                throw std::invalid_argument("invalid processor parameter metadata");
            for (std::size_t b = 0; b < a; ++b)
                if (node.infos[b].id == info.id) throw std::invalid_argument("duplicate processor parameter ID");
        }
        // Older hosts appended a complete parameter snapshot to opaque state.
        // Complete lists are legacy snapshots, not sparse host overrides;
        // replaying them (notably TH-U) can reset a restored preset over many blocks.
        const bool legacy_full_state=saved.format==ProcessorFormat::vst3&&!saved.plugin.component.empty()&&saved.parameters.size()>1&&
            static_cast<std::size_t>(std::count_if(node.infos.begin(),node.infos.end(),[](const auto& p){return p.automatable;}))==saved.parameters.size()&&
            std::all_of(saved.parameters.begin(),saved.parameters.end(),[&](const auto& value){return std::any_of(node.infos.begin(),node.infos.end(),[&](const auto& p){return p.id==value.id&&p.automatable;});});
        for (const auto& value : saved.parameters) {
            const auto it = std::find_if(node.infos.begin(),node.infos.end(),[&](const auto& info) { return info.id == value.id; });
            if (it == node.infos.end() || value.value < it->minimum || value.value > it->maximum ||
                (!legacy_full_state && !(saved.format==ProcessorFormat::vst3&&value.value==it->initial) && !node.processor->set_parameter(value.id,value.value)))
                throw std::invalid_argument("unsupported or out-of-range processor parameter");
        }
        node.audio.resize(samples);
        node.processor->warm();
    }
    std::vector<std::size_t> degrees(state.nodes.size());
    for (const auto& edge : state.edges) {
        const auto to = p.index.at(edge.to.value);
        const auto from = edge.from ? std::optional<std::size_t>{p.index.at(edge.from->value)} : std::nullopt;
        p.nodes[to].inputs.push_back({from,edge.gain});
        if (from) ++degrees[to];
    }
    for (std::size_t i = 0; i < degrees.size(); ++i) if (!degrees[i]) p.order.push_back(i);
    for (std::size_t i = 0; i < p.order.size(); ++i)
        for (const auto& edge : state.edges)
            if (edge.from && p.index.at(edge.from->value) == p.order[i])
                if (--degrees[p.index.at(edge.to.value)] == 0) p.order.push_back(p.index.at(edge.to.value));
    std::size_t delay_budget=64*1024*1024;
    std::vector<std::uint64_t> paths(state.nodes.size());
    for (auto index : p.order) {
        auto& node = p.nodes[index];
        std::uint64_t upstream = 0;
        std::optional<std::uint64_t> first;
        for (const auto& edge : node.inputs) {
            const auto latency = edge.from ? paths[*edge.from] : 0;
            if (first && *first != latency) p.report.parallel_paths_need_compensation = true;
            first = latency; upstream = std::max(upstream,latency);
        }
        const auto own = node.bypass ? 0 : node.processor->latency();
        for(auto& edge:node.inputs)edge.delay.prepare(upstream-(edge.from?paths[*edge.from]:0),config.channels,delay_budget);
        paths[index] = upstream + own;
        if(paths[index]>262144)throw std::invalid_argument("PDC processor path exceeds 262144 samples");
        const bool safe = node.bypass || node.processor->live_safe();
        p.report.nodes.push_back({state.nodes[index].id,own,paths[index],safe});
        p.report.live_safe = p.report.live_safe && safe;
    }
    std::optional<std::uint64_t> first;
    for (const auto& output : state.outputs) {
        const auto index = p.index.at(output.value); p.outputs.push_back(index);
        if (first && *first != paths[index]) p.report.parallel_paths_need_compensation = true;
        first = paths[index]; p.report.output = std::max(p.report.output,paths[index]);
    }
    p.output_delays.resize(p.outputs.size());
    for(std::size_t i=0;i<p.outputs.size();++i)p.output_delays[i].prepare(p.report.output-paths[p.outputs[i]],config.channels,delay_budget);
    p.report.compensation_applied=p.report.parallel_paths_need_compensation;
    p.report.parallel_paths_need_compensation=false;
    if (p.report.output > config.live_latency_budget)
        p.report.live_safe = false;
}
PreparedGraph::~PreparedGraph() = default;
const GraphSnapshot& PreparedGraph::snapshot() const { return impl_->snapshot; }
ProcessConfig PreparedGraph::config() const { return impl_->config; }
const LatencyReport& PreparedGraph::latency() const { return impl_->report; }
bool PreparedGraph::enqueue_midi(const Id& source, MidiEvent event) {
    event.validate();
    if (event.offset >= impl_->config.max_block) throw std::invalid_argument("MIDI offset exceeds prepared block");
    impl_->external_schedule=true;
    ++impl_->control_revision;
    if (event.kind == MidiKind::note_on && event.data2 == 0) event.kind = MidiKind::note_off;
    bool accepted = true;
    for (const auto& route : impl_->snapshot.graph->midi_routes) {
        if (route.source_port != source || (route.input_channel >= 0 && route.input_channel != event.channel)) continue;
        auto routed = event;
        if (route.output_channel >= 0) routed.channel = static_cast<std::uint8_t>(route.output_channel);
        if (event.kind == MidiKind::note_on || event.kind == MidiKind::note_off || event.kind == MidiKind::poly_pressure) {
            const int note = event.data1 + route.transpose;
            if (note < 0 || note > 127) continue;
            routed.data1 = static_cast<std::uint8_t>(note);
        }
        const auto index = impl_->index.at(route.target_node.value);
        if (!impl_->midi_queue.push({static_cast<std::uint16_t>(index),routed})) {
            ++impl_->dropped_midi; impl_->panic_requested = true; accepted = false;
        }
    }
    return accepted; // unmatched routes/filter drops are intentional
}
bool PreparedGraph::enqueue_parameter(const Id& node, ParameterChange change) {
    if (!std::isfinite(change.value) || change.offset >= impl_->config.max_block)
        throw std::invalid_argument("invalid parameter change");
    const auto found = impl_->index.find(node.value);
    if (found == impl_->index.end()) throw std::invalid_argument("unknown parameter node");
    const auto& infos = impl_->nodes[found->second].infos;
    const auto it = std::find_if(infos.begin(),infos.end(),[&](const auto& p) { return p.id == change.id; });
    if (it == infos.end() || !it->automatable || change.value < it->minimum || change.value > it->maximum)
        throw std::invalid_argument("unsupported automation parameter");
    if(change.offset)impl_->external_schedule=true;
    ++impl_->control_revision;
    if (!impl_->parameter_queue.push({static_cast<std::uint16_t>(found->second),change})) {
        ++impl_->control_revision;++impl_->dropped_parameters; return false;
    }
    ++impl_->control_revision;return true;
}
bool PreparedGraph::enqueue_parameters(const GraphState& state) {
    ParamBatch batch;
    for(const auto& saved:state.nodes){const auto at=impl_->index.find(saved.id.value);if(at==impl_->index.end())throw std::invalid_argument("unknown insert node");
        if(saved.bypass!=impl_->snapshot.graph->nodes[at->second].bypass&&impl_->nodes[at->second].processor->latency())throw std::invalid_argument("Pause/Stop and rebuild to bypass a latency-bearing processor");
        if(batch.size>=batch.changes.size())throw std::invalid_argument("parameter transaction too large");
        batch.changes[batch.size++]={static_cast<std::uint16_t>(at->second),{0,UINT32_MAX,saved.bypass?1.f:0.f}};
        for(const auto& v:saved.parameters){
            if(saved.format==ProcessorFormat::vst3){const auto& old=impl_->last_parameters.at(saved.id.value);if(std::find(old.begin(),old.end(),v)!=old.end())continue;}
            const auto& infos=impl_->nodes[at->second].infos;const auto it=std::find_if(infos.begin(),infos.end(),[&](const auto& p){return p.id==v.id;});
            if(it==infos.end() || !std::isfinite(v.value) || v.value<it->minimum || v.value>it->maximum)throw std::invalid_argument("invalid insert parameter update");
            if(batch.size>=batch.changes.size())throw std::invalid_argument("parameter transaction too large");batch.changes[batch.size++]={static_cast<std::uint16_t>(at->second),{0,v.id,v.value}};
        }
        if(saved.format==ProcessorFormat::vst3)for(const auto& old:impl_->last_parameters.at(saved.id.value)){
            if(std::none_of(saved.parameters.begin(),saved.parameters.end(),[&](const auto& p){return p.id==old.id;})){
                const auto& infos=impl_->nodes[at->second].infos;const auto it=std::find_if(infos.begin(),infos.end(),[&](const auto& p){return p.id==old.id;});
                if(it!=infos.end()){if(batch.size>=batch.changes.size())throw std::invalid_argument("parameter transaction too large");batch.changes[batch.size++]={static_cast<std::uint16_t>(at->second),{0,old.id,it->initial}};}
            }
        }
    }
    ++impl_->control_revision;
    if(!impl_->batch_queue.push(batch)){++impl_->control_revision;++impl_->dropped_parameters;return false;}
    ++impl_->control_revision;
    for(std::size_t i=0;i<batch.size;++i){const auto& c=batch.changes[i];if(c.event.id!=UINT32_MAX && impl_->snapshot.graph->nodes[c.node].format==ProcessorFormat::vst3)impl_->nodes[c.node].processor->sync_controller(c.event.id,c.event.value);}
    for(const auto& n:state.nodes)impl_->last_parameters[n.id.value]=n.parameters;
    return true;
}
void PreparedGraph::panic() noexcept { impl_->panic_requested = true; }
void PreparedGraph::reset_anticipation() noexcept {
    for(auto& node:impl_->nodes){node.processor->reset_anticipation();for(auto& edge:node.inputs)edge.delay.reset();}
    for(auto& delay:impl_->output_delays)delay.reset();
}
void PreparedGraph::process(float* audio, std::uint32_t frames,Sample position,bool playing,double tempo,double quarter,std::span<const MidiEvent> live_midi) noexcept {
    auto& p = *impl_;
    const bool profiling=p.profiling.load(std::memory_order_relaxed);
    if (!audio || !frames || frames > p.config.max_block) { ++p.invalid; return; }
    for (auto& node : p.nodes) { node.midi.clear(); node.output.clear(); node.parameter_count = 0; }
    bool incoming_valid=live_midi.size()<=event_capacity;
    for(const auto& event:live_midi.first(std::min(live_midi.size(),event_capacity)))if(event.offset>=frames||event.channel>15||event.data1>127||event.data2>127||static_cast<int>(event.kind)<0||event.kind>MidiKind::poly_pressure)incoming_valid=false;
    if(!incoming_valid){++p.dropped_midi;p.panic_requested=true;}
    MidiCommand midi;
    // Bound the drain even if producer keeps writing.
    for (int i = 0; i < 1023 && p.midi_queue.pop(midi); ++i) {
        if (midi.event.offset >= frames) { ++p.invalid; p.panic_requested = true; continue; }
        if (!p.nodes[midi.node].midi.push(midi.event)) { ++p.dropped_midi; p.panic_requested = true; }
    }
    if (p.panic_requested.exchange(false)) {
        ++p.panics;
        for (auto& node : p.nodes) {
            node.processor->reset(); node.midi.clear();
            for(auto& edge:node.inputs)edge.delay.reset();
            for (std::uint8_t ch = 0; ch < 16; ++ch) {
                (void)node.midi.push({0,MidiKind::cc,ch,120,0});
                (void)node.midi.push({0,MidiKind::cc,ch,123,0});
            }
        }
        for(auto& delay:p.output_delays)delay.reset();
    }
    // New timeline/chased notes belong AFTER a pending Seek/Stop reset.
    // Queued old graph MIDI is still cleared by panic above.
    if(incoming_valid&&p.config.instrument&&!p.nodes.empty())for(const auto& event:live_midi)if(!p.nodes.front().midi.push(event)){++p.dropped_midi;p.panic_requested=true;p.nodes.front().midi.clear();break;}
    ParamCommand param;
    for (int i = 0; i < 1023 && p.parameter_queue.pop(param); ++i) {
        auto& node = p.nodes[param.node];
        if (param.event.offset >= frames) { ++p.invalid; continue; }
        if (node.parameter_count == event_capacity) { ++p.dropped_parameters; continue; }
        node.parameters[node.parameter_count++] = param.event;
    }
    ParamBatch batch;
    // Last complete transaction wins at the block boundary. No partial EQ state.
    for(int i=0;i<63 && p.batch_queue.pop(batch);++i) {
        for(std::size_t j=0;j<batch.size;++j){const auto& c=batch.changes[j];auto& node=p.nodes[c.node];if(c.event.id==UINT32_MAX)node.bypass=c.event.value>=0.5f;else {std::size_t k=0;for(;k<node.parameter_count;++k)if(node.parameters[k].id==c.event.id)break;if(k<event_capacity){node.parameters[k]=c.event;if(k==node.parameter_count)++node.parameter_count;}else ++p.dropped_parameters;}}
    }
    const auto count = static_cast<std::size_t>(frames) * p.config.channels;
    for (auto index : p.order) {
        auto& node = p.nodes[index];
        std::fill_n(node.audio.data(),count,0.0F);
        for (auto& edge : node.inputs) {
            const auto* input = edge.from ? p.nodes[*edge.from].audio.data() : audio;
            for (std::size_t i = 0; i < count; ++i) node.audio[i] += edge.delay.sample(input[i],static_cast<std::uint32_t>(i%p.config.channels)) * edge.gain;
        }
        sort_offsets(node.midi.events.data(),node.midi.size);
        sort_offsets(node.parameters.data(),node.parameter_count);
        if (node.bypass) {
            for (std::size_t i = 0; i < node.parameter_count; ++i)
                (void)node.processor->set_parameter(node.parameters[i].id,node.parameters[i].value);
            for (auto event : node.midi.view()) (void)node.output.push(event);
        } else {
            const auto start=profiling?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
            node.processor->process({{node.audio.data(),count},frames,p.config.channels,
                node.midi.view(),{node.parameters.data(),node.parameter_count},node.output,position,playing,tempo,quarter});
            if(profiling)p.timings[index].record(elapsed_ns(start),frames);
        }
        // Sanitize each node before its output feeds other nodes.
        for (std::size_t i = 0; i < count; ++i) if (!std::isfinite(node.audio[i])) node.audio[i] = 0;
        for (auto event : node.output.view()) {
            if (event.offset >= frames || event.channel > 15 || event.data1 > 127 || event.data2 > 127 ||
                static_cast<int>(event.kind) < 0 || static_cast<int>(event.kind) > static_cast<int>(MidiKind::poly_pressure)) {
                ++p.invalid; continue;
            }
            if (!p.output_queue.push({static_cast<std::uint16_t>(index),event})) ++p.output_overflows;
        }
    }
    if (!p.nodes.empty()) {
        std::fill_n(audio,count,0.0F);
        for(std::size_t j=0;j<p.outputs.size();++j)
            for (std::size_t i = 0; i < count; ++i) audio[i] += p.output_delays[j].sample(p.nodes[p.outputs[j]].audio[i],static_cast<std::uint32_t>(i%p.config.channels));
    }
}
bool PreparedGraph::pop_midi_output(MidiOutput& event) noexcept { return impl_->output_queue.pop(event); }
void PreparedGraph::set_profiling(bool enabled) noexcept {impl_->profiling.store(enabled,std::memory_order_relaxed);}
std::array<TimingSample,max_nodes> PreparedGraph::profile() const noexcept {std::array<TimingSample,max_nodes> result{};for(std::size_t i=0;i<impl_->nodes.size();++i)result[i]=impl_->timings[i].read();return result;}
GraphMetrics PreparedGraph::metrics() const noexcept {
    return {impl_->dropped_midi.load(),impl_->dropped_parameters.load(),impl_->invalid.load(),
        impl_->output_overflows.load(),impl_->panics.load()};
}
GraphState PreparedGraph::capture() const {
    // Capture is quiescent. Apply queued controls before taking opaque state so
    // Save immediately after a gesture cannot lose an edit waiting for a callback.
    const auto apply=[&](const auto& c){auto& n=impl_->nodes[c.node];if(c.event.id==UINT32_MAX)n.bypass=c.event.value>=0.5f;else {n.processor->set_parameter(c.event.id,c.event.value);n.processor->sync_controller(c.event.id,c.event.value);}};
    ParamCommand command;for(int i=0;i<1023&&impl_->parameter_queue.pop(command);++i)apply(command);
    ParamBatch batch;for(int i=0;i<63&&impl_->batch_queue.pop(batch);++i)for(std::size_t j=0;j<batch.size;++j)apply(batch.changes[j]);
    auto saved = *impl_->snapshot.graph;
    for (std::size_t i = 0; i < saved.nodes.size(); ++i) {
        const auto& processor = impl_->nodes[i].processor;
        saved.nodes[i].plugin = processor->capture();
        saved.nodes[i].parameters.clear();
        // Opaque VST3 state is authoritative; replaying every normalized parameter
        // can reset program/preset selectors after setState.
        if(saved.nodes[i].format==ProcessorFormat::vst3)continue;
        for (const auto& info : impl_->nodes[i].infos) {
            const auto value = processor->parameter_value(info.id);
            if (value) saved.nodes[i].parameters.push_back({info.id,*value});
        }
    }
    saved.validate(); return saved;
}
std::uint32_t PreparedGraph::node_latency(const Id& id) const{auto it=impl_->index.find(id.value);return it==impl_->index.end()?0:impl_->nodes[it->second].processor->latency();}
std::vector<ParameterInfo> PreparedGraph::parameter_infos(const Id& id) const{auto it=impl_->index.find(id.value);return it==impl_->index.end()?std::vector<ParameterInfo>{}:[&]{auto infos=impl_->nodes[it->second].infos;if(impl_->snapshot.graph->nodes[it->second].format==ProcessorFormat::vst3)for(auto& p:infos)if(auto v=impl_->nodes[it->second].processor->parameter_value(p.id))p.initial=*v;return infos;}();}
void PreparedGraph::restore_node(const NodeState& state) {
    auto& p=*impl_;const auto found=p.index.find(state.id.value);
    if(found==p.index.end())throw std::invalid_argument("unknown preset processor");
    const auto index=found->second;const auto& original=p.snapshot.graph->nodes[index];
    if(state.format!=ProcessorFormat::vst3||state.processor_id!=original.processor_id||state.plugin.class_id!=original.plugin.class_id||state.bypass!=original.bypass)throw std::invalid_argument("preset changes processor structure");
    auto saved=capture();auto next=std::make_shared<GraphState>(*p.snapshot.graph);next->nodes[index]=state;next->validate();
    auto& node=p.nodes[index];const auto latency=node.processor->latency();
    try {
        node.processor->restore(state.plugin);
        auto infos=node.processor->parameters();
        const bool legacyFull=!state.plugin.component.empty()&&state.parameters.size()>1&&state.parameters.size()==static_cast<std::size_t>(std::count_if(infos.begin(),infos.end(),[](const auto& info){return info.automatable;}))&&std::all_of(state.parameters.begin(),state.parameters.end(),[&](const auto& value){return std::any_of(infos.begin(),infos.end(),[&](const auto& info){return info.id==value.id&&info.automatable;});});
        if(!legacyFull)for(const auto& value:state.parameters){auto at=std::find_if(infos.begin(),infos.end(),[&](const auto& info){return info.id==value.id&&info.automatable;});
            if(at==infos.end()||value.value<at->minimum||value.value>at->maximum)throw std::invalid_argument("invalid preset parameter");
            if(node.processor->parameter_value(value.id)!=value.value){if(!node.processor->set_parameter(value.id,value.value))throw std::invalid_argument("preset parameter rejected");node.processor->sync_controller(value.id,value.value);}
        }
        (void)node.processor->capture(); // flush sparse overrides without advancing audio
        if(node.processor->latency()!=latency)throw std::runtime_error("Preset changes plugin latency; reload the insert before using this mode");
        node.infos=std::move(infos);p.last_parameters[state.id.value]=state.parameters;p.snapshot.graph=std::move(next);
    }catch(...){node.processor->restore(saved.nodes[index].plugin);throw;}
}
bool PreparedGraph::open_editor(const Id& id,void* parent,int& w,int& h){const auto it=impl_->index.find(id.value);if(it==impl_->index.end())return false;return impl_->nodes[it->second].processor->open_editor(parent,w,h);}
void PreparedGraph::close_editors() noexcept {for(auto& n:impl_->nodes)n.processor->close_editor();}
void PreparedGraph::close_editor(const Id& id) noexcept {const auto it=impl_->index.find(id.value);if(it!=impl_->index.end())impl_->nodes[it->second].processor->close_editor();}
bool PreparedGraph::consume_edits() noexcept{bool any=false;for(auto& n:impl_->nodes)any=n.processor->edited()||any;return any;}
bool PreparedGraph::failed() const noexcept{for(const auto& n:impl_->nodes)if(n.processor->failed())return true;return false;}
bool PreparedGraph::anticipation_safe() const noexcept {
    if(impl_->config.instrument)return false;
    if(impl_->external_schedule.load(std::memory_order_acquire))return false;
    for(const auto& node:impl_->nodes)if(!node.processor->anticipation_safe())return false;
    return true;
}
std::uint64_t PreparedGraph::control_revision() const noexcept {return impl_->control_revision.load(std::memory_order_acquire);}
} // namespace mrs::processing
