#include <mrs/audio.hpp>
#include <mrs/read_ahead.hpp>
#include <mrs/recording.hpp>
#include <mrs/processing.hpp>
#include <mrs/no_denormals.hpp>
#include <mrs/channel_workers.hpp>
#include <mrs/ahead_renderer.hpp>
#include <mrs/mixed_renderer.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
#include <stdexcept>

namespace mrs::audio {
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<std::size_t>::is_always_lock_free);
static_assert(std::atomic<Sample>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
Sample AudioData::frames() const {
    return file ? file->frame_count : (channels ? static_cast<Sample>(samples.size() / channels) : 0);
}
void AudioData::validate() const {
    if (file) {
        if (!samples.empty() || file->sample_rate != sample_rate || file->channels != channels ||
            sample_rate < 8000 || sample_rate > 768000 || !channels || channels > max_channels || frames() <= 0 || frames() > max_sample)
            throw std::invalid_argument("invalid disk audio asset");
        return;
    }
    if (sample_rate < 8000 || sample_rate > 768000 || channels == 0 || channels > max_channels ||
        samples.empty() || samples.size() % channels != 0 || frames() > max_sample)
        throw std::invalid_argument("invalid audio asset");
    for (const auto sample : samples)
        if (!std::isfinite(sample)) throw std::invalid_argument("non-finite audio sample");
}
AudioEngine::AudioEngine() { prepare(RenderConfig{}, {}); }
AudioEngine::~AudioEngine() {workers_.reset();}
void AudioEngine::quiesce() noexcept {if(workers_)workers_->quiesce();}
void AudioEngine::channel_job(void* context,std::uint32_t index) noexcept {
    auto& engine=*static_cast<AudioEngine*>(context);
    const auto track=engine.channel_levels_[engine.job_level_][index];
    engine.process_channel(track);
}
void AudioEngine::process_channel(std::size_t t) noexcept {
    const auto started=(measure_channels_||profile_block_)?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    const bool owned=domain_owned_[t];
    if(owned&&!graph_.inserts.empty()&&graph_.inserts[t])graph_.inserts[t]->process(track_block_[t].data(),job_frames_,job_position_,job_playing_,job_tempo_,job_quarter_,job_base_==0?live_midi_buffers_[t].view():std::span<const processing::MidiEvent>{});
    channel_peaks_[t]=owned?std::array<float,2>{}:std::array<float,2>{domain_meters_[t].left,domain_meters_[t].right};
    for(std::uint32_t local=0;local<job_frames_;++local) {
        const auto at=static_cast<std::size_t>(local)*config_.output_channels;
        for(std::uint32_t c=0;c<config_.output_channels;++c) {
            const auto sample=track_block_[t][at+c]*mix_gain_[t][c%2];
            const auto boundary=[&](std::size_t edge){return domain_input_?domain_input_[edge*static_cast<std::size_t>(domain_stride_)*config_.output_channels+static_cast<std::size_t>(job_base_+local)*config_.output_channels+c]:0.f;};
            route_contributions_[t][at+c]=owned?route_delays_[t].sample(sample,c):boundary(domain_edges_[t]);
            for(std::size_t j=0;j<graph_.sends[t].size();++j) {
                const auto& send=graph_.sends[t][j];
                send_contributions_[t][j][at+c]=owned?send_delays_[t][j].sample((send.pre_fader?track_block_[t][at+c]*gate_[t]:sample)*send_gain_[t][j],c):boundary(domain_edges_[t]+1+j);
            }
            if(owned&&c<2)channel_peaks_[t][c]=std::max(channel_peaks_[t][c],std::abs(sample));
        }
        if(local<job_ramp_) {
            for(std::size_t c=0;c<2;++c)mix_gain_[t][c]+=mix_step_[t][c];
            gate_[t]+=gate_step_[t];
            for(std::size_t j=0;j<graph_.sends[t].size();++j)send_gain_[t][j]+=send_step_[t][j];
            if(local+1==job_ramp_){mix_gain_[t]=mix_target_[t];gate_[t]=gate_target_[t];send_gain_[t]=send_target_[t];}
        }
    }
    if(profile_block_){const auto ns=elapsed_ns(started);profile_duration_[t]=ns;
        auto& channels=speculative_?profile_->ahead:profile_->device;channels[t].record(ns,job_frames_);
        auto& participants=speculative_?profile_->ahead_workers:profile_->device_workers;participants[ChannelWorkers::participant()].record(ns,job_frames_);}
    if(measure_channels_){const auto elapsed=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count())/job_frames_;
        channel_cost_ns_[t]=channel_cost_ns_[t]?(channel_cost_ns_[t]*3+elapsed)/4:elapsed;}
}
void AudioEngine::reset_compensation() noexcept {legacy_delay_.reset();for(std::size_t t=0;t<route_delays_.size();++t)if(domain_owned_[t]){route_delays_[t].reset();for(auto& delay:send_delays_[t])delay.reset();}}
ProcessingDomains AudioEngine::compile_domains(const RenderGraph& graph,
    const std::vector<std::size_t>& order, const CompensationReport& compensation,
    const std::vector<std::uint64_t>& arrivals) const {
    using D = ProcessingDomains;
    D plan;
    plan.channels.resize(graph.mixer.size());
    for(std::size_t t=0;t<graph.live_midi.size();++t)if(graph.live_midi[t])plan.channels[t].reasons|=D::live_input;
    plan.raw_capture = !graph.recordings.empty();
    plan.monitoring_compensation_frames = graph.monitor.empty()&&std::none_of(graph.live_midi.begin(),graph.live_midi.end(),[](bool v){return v;}) ? 0 : compensation.output;
    const auto unsupported = [](const auto& chain) { return chain && !chain->anticipation_safe(); };
    if (unsupported(graph.processors) || unsupported(graph.master_inserts))
        plan.master.reasons |= D::unsupported_processor;
    for (std::size_t t = 0; t < graph.inserts.size(); ++t)
        if (unsupported(graph.inserts[t])) plan.channels[t].reasons |= D::unsupported_processor;
    // Reserve ALL prepared input routes, even disabled ones. Monitor/mixer
    // toggles must not migrate a running plugin or its PDC history between owners.
    for (const auto& input : graph.monitor) {
        const auto t = input.mixer_track != no_mixer_track ? input.mixer_track : graph.monitor_track;
        if (t == no_mixer_track) plan.master.reasons |= D::live_input;
        else plan.channels[t].reasons |= D::live_input;
    }
    for (const auto t : order) {
        auto& channel = plan.channels[t];
        channel.owner = channel.reasons ? D::Owner::device : D::Owner::ahead;
        const auto propagate = [&](std::size_t destination) {
            plan.channels[destination].reasons |= channel.reasons;
        };
        if (graph.outputs[t] != no_mixer_track) propagate(graph.outputs[t]);
        else if (graph.hardware_outputs[t].empty()) plan.master.reasons |= channel.reasons;
        for (const auto& send : graph.sends[t]) propagate(send.destination);
    }
    plan.master.owner = plan.master.reasons ? D::Owner::device : D::Owner::ahead;
    const auto master_input = compensation.output - compensation.master;
    for (const auto t : order) {
        if (plan.channels[t].owner != D::Owner::ahead) continue;
        const auto dest = graph.outputs[t];
        if (!graph.hardware_outputs[t].empty())
            plan.merges.push_back({t, no_mixer_track, 0, D::MergeKind::hardware,
                compensation.output - compensation.track_paths[t]});
        else if (dest == no_mixer_track) {
            if (plan.master.owner == D::Owner::device)
                plan.merges.push_back({t, no_mixer_track, 0, D::MergeKind::master,
                    master_input - compensation.track_paths[t]});
        } else if (plan.channels[dest].owner == D::Owner::device)
            plan.merges.push_back({t, dest, 0, D::MergeKind::main,
                arrivals[dest] - compensation.track_paths[t]});
        for (std::size_t j = 0; j < graph.sends[t].size(); ++j) {
            const auto destination = graph.sends[t][j].destination;
            if (plan.channels[destination].owner == D::Owner::device)
                plan.merges.push_back({t, destination, j, D::MergeKind::send,
                    arrivals[destination] - compensation.track_paths[t]});
        }
    }
    if (plan.master.owner == D::Owner::ahead)
        plan.merges.push_back({no_mixer_track, no_mixer_track, 0, D::MergeKind::master_output, 0});
    else if (std::any_of(graph.voices.begin(), graph.voices.end(), [](const auto& voice) {
        return voice.mixer_track == no_mixer_track && !voice.routes.empty();
    }))
        plan.merges.push_back({no_mixer_track, no_mixer_track, 0, D::MergeKind::legacy, master_input});
    return plan;
}
void AudioEngine::prepare(RenderConfig config, RenderGraph graph, RealtimeState initial) {
    if(speculative_||mixed_owner_)throw std::logic_error("stop the process producer before preparing audio");
    // Caller has stopped the device; this also joins a timed-out worker batch.
    workers_.reset();
    ++midi_generation_; // old route indices must never address a replacement graph
    if ((initial.playback != PlaybackState::stopped && initial.playback != PlaybackState::paused) ||
        initial.sample < 0 || initial.sample > max_sample || initial.play_start < 0 || initial.play_start > max_sample ||
        (initial.loop && (initial.loop->start < 0 || initial.loop->start >= initial.loop->end || initial.loop->end > max_sample)))
        throw std::invalid_argument("invalid quiescent transport state");

    if (config.sample_rate < 8000 || config.sample_rate > 768000 ||
        config.input_channels > max_channels || config.output_channels == 0 ||
        config.output_channels > max_channels || config.max_block == 0 || config.max_block > 65536 || config.processing_block>config.max_block ||
        config.processing_workers<1 || config.processing_workers>8 ||
        config.worker_wait_ms<1 || config.worker_wait_ms>1000 ||
        graph.voices.size() > max_voices || graph.monitor.size() > max_channels * max_channels)
        throw std::invalid_argument("invalid render configuration");
    if (graph.mixer.size() > max_mixer_tracks || !std::isfinite(graph.master_gain) || graph.master_gain < 0 || graph.master_gain > 16 ||
        (graph.monitor_track != no_mixer_track && graph.monitor_track >= graph.mixer.size()))
        throw std::invalid_argument("invalid mixer configuration");
    for (const auto& mix : graph.mixer) mix.validate();
    if (graph.outputs.empty()) graph.outputs.assign(graph.mixer.size(),no_mixer_track);
    if (graph.buses.empty()) graph.buses.assign(graph.mixer.size(),false);
    if(graph.live_midi.empty())graph.live_midi.assign(graph.mixer.size(),false);
    if(graph.live_midi.size()!=graph.mixer.size())throw std::invalid_argument("MIDI channel size mismatch");
    live_midi_buffers_.resize(graph.mixer.size());
    if (graph.outputs.size() != graph.mixer.size() || graph.buses.size() != graph.mixer.size())
        throw std::invalid_argument("mixer routing size mismatch");
    if (graph.sends.empty()) graph.sends.resize(graph.mixer.size());
    if (graph.sends.size() != graph.mixer.size()) throw std::invalid_argument("send routing size mismatch");
    if (graph.hardware_outputs.empty()) graph.hardware_outputs.resize(graph.mixer.size());
    if (graph.hardware_outputs.size() != graph.mixer.size()) throw std::invalid_argument("hardware routing size mismatch");
    const auto physical = [&](const std::vector<std::size_t>& outputs) {
        if (outputs.size() > 2 || (outputs.size() == 2 && outputs[0] == outputs[1])) throw std::invalid_argument("invalid mono/stereo hardware route");
        for (const auto c : outputs) if (c >= config.output_channels) throw std::invalid_argument("missing hardware output");
    };
    physical(graph.master_outputs);
    for (std::size_t i=0; i<graph.hardware_outputs.size(); ++i) {
        physical(graph.hardware_outputs[i]);
        if (!graph.hardware_outputs[i].empty() && graph.outputs[i] != no_mixer_track) throw std::invalid_argument("bus/hardware route conflict");
    }
    std::vector<std::size_t> order, incoming(graph.mixer.size());
    const auto edge = [&](std::size_t destination) {
        if (destination == no_mixer_track) return;
        if (destination >= graph.mixer.size() || !graph.buses[destination]) throw std::invalid_argument("invalid output/send bus");
        ++incoming[destination];
    };
    for (std::size_t i=0; i<graph.mixer.size(); ++i) {
        edge(graph.outputs[i]);
        if (graph.sends[i].size() > 8) throw std::invalid_argument("send limit exceeded");
        for (std::size_t j=0; j<graph.sends[i].size(); ++j) {
            const auto& send = graph.sends[i][j];
            if (send.destination == no_mixer_track || !std::isfinite(send.gain) || send.gain < 0 || send.gain > 16) throw std::invalid_argument("invalid send");
            for (std::size_t k=0; k<j; ++k) if (graph.sends[i][k].destination == send.destination) throw std::invalid_argument("duplicate send destination");
            edge(send.destination);
        }
    }
    for (std::size_t i=0; i<incoming.size(); ++i) if (!incoming[i]) order.push_back(i);
    const auto release = [&](std::size_t destination) {
        if (destination != no_mixer_track && --incoming[destination] == 0) order.push_back(destination);
    };
    for (std::size_t i=0; i<order.size(); ++i) {
        release(graph.outputs[order[i]]);
        for (const auto& send : graph.sends[order[i]]) release(send.destination);
    }
    if (order.size() != graph.mixer.size()) throw std::invalid_argument("audio routing cycle");
    std::vector<std::size_t> levels(graph.mixer.size());
    std::vector<std::vector<std::size_t>> channel_levels;
    std::vector<const processing::PreparedGraph*> owners;
    const auto register_owner=[&](const std::shared_ptr<processing::PreparedGraph>& chain) {
        if(!chain)return;
        if(std::find(owners.begin(),owners.end(),chain.get())!=owners.end())
            throw std::invalid_argument("processor graph must have a single channel owner");
        owners.push_back(chain.get());
    };
    register_owner(graph.processors);register_owner(graph.master_inserts);
    for(const auto& chain:graph.inserts)register_owner(chain);
    for(const auto t:order) {
        if(channel_levels.size()<=levels[t])channel_levels.resize(levels[t]+1);
        channel_levels[levels[t]].push_back(t);
        const auto advance=[&](std::size_t dest){if(dest!=no_mixer_track)levels[dest]=std::max(levels[dest],levels[t]+1);};
        advance(graph.outputs[t]);for(const auto& send:graph.sends[t])advance(send.destination);
    }
    if (graph.monitor_track != no_mixer_track && graph.buses[graph.monitor_track]) throw std::invalid_argument("input requires an audio track");
    for (const auto& voice : graph.voices) {
        if (voice.mixer_track != no_mixer_track && voice.mixer_track >= graph.mixer.size()) throw std::invalid_argument("invalid voice mixer track");
        if (voice.mixer_track != no_mixer_track && graph.buses[voice.mixer_track]) throw std::invalid_argument("voice requires an audio track");
        if (!voice.asset) throw std::invalid_argument("missing asset");
        voice.asset->validate();
        if (voice.asset->sample_rate != config.sample_rate)
            throw std::invalid_argument("asset/device sample-rate mismatch (no realtime resampling)");
        if (voice.start < 0 || voice.start > max_sample || voice.length <= 0 ||
            voice.length > max_sample - voice.start || voice.source_offset < 0 ||
            voice.source_offset > voice.asset->frames() || voice.length > voice.asset->frames() - voice.source_offset ||
            voice.routes.size() > max_channels * max_channels)
            throw std::invalid_argument("invalid voice range");
        for (const auto& route : voice.routes)
            if (route.source_channel >= voice.asset->channels || route.output_channel >= config.output_channels ||
                !std::isfinite(route.gain) || std::abs(route.gain) > 16)
                throw std::invalid_argument("invalid playback route");
    }
    for (const auto& route : graph.monitor)
        if (route.input_channel >= config.input_channels || route.output_channel >= config.output_channels ||
            !std::isfinite(route.gain) || std::abs(route.gain) > 16)
            throw std::invalid_argument("invalid monitoring route");
    if (graph.processors) {
        const auto processor = graph.processors->config();
        if (processor.sample_rate != config.sample_rate || processor.channels != config.output_channels ||
            processor.max_block < config.max_block)
            throw std::invalid_argument("processor/audio render configuration mismatch");
    }
    if (graph.recording) {
        if (!graph.recordings.empty()) throw std::invalid_argument("duplicate legacy capture sink");
        graph.recordings.push_back(graph.recording);
    }
    if (graph.recordings.size() > 32) throw std::invalid_argument("recording supports up to 32 tracks");
    for (std::size_t i=0; i<graph.recordings.size(); ++i) {
        const auto& recorder=graph.recordings[i];
        if (!recorder || recorder->rate() != config.sample_rate || recorder->start() != initial.sample || initial.loop ||
            std::any_of(recorder->selectors().begin(),recorder->selectors().end(),[&](auto c) { return c >= config.input_channels; }) ||
            std::find(graph.recordings.begin(),graph.recordings.begin()+static_cast<std::ptrdiff_t>(i),recorder) != graph.recordings.begin()+static_cast<std::ptrdiff_t>(i))
            throw std::invalid_argument("invalid recording start/rate/selectors/sink");
    }
    if (!graph.input_monitoring.empty() && graph.input_monitoring.size() != graph.mixer.size()) throw std::invalid_argument("input monitoring count mismatch");
    for (const auto& route : graph.monitor) if (route.mixer_track != no_mixer_track && (route.mixer_track >= graph.mixer.size() || graph.buses[route.mixer_track])) throw std::invalid_argument("invalid input track");
    if (!graph.inserts.empty() && graph.inserts.size() != graph.mixer.size()) throw std::invalid_argument("insert channel count mismatch");
    for (const auto& chain : graph.inserts) if (chain) {
        const auto c=chain->config(); if (c.sample_rate != config.sample_rate || c.channels != config.output_channels || c.max_block < 1) throw std::invalid_argument("insert render config mismatch");
    }
    if (graph.master_inserts) { const auto c=graph.master_inserts->config(); if (c.sample_rate != config.sample_rate || c.channels != config.output_channels || c.max_block < config.max_block) throw std::invalid_argument("master insert config mismatch"); }
    // Keep muted paths in the plan: gates must never change prepared latency.
    CompensationReport compensation;
    std::vector<std::uint64_t> arrivals(graph.mixer.size());
    compensation.track_paths.resize(graph.mixer.size());
    for(const auto t:order){const auto own=graph.inserts.empty()||!graph.inserts[t]?0:graph.inserts[t]->latency().output;
        const auto path=arrivals[t]+own;if(path>262144)throw std::invalid_argument("PDC mixer path exceeds 262144 samples");
        compensation.track_paths[t]=path;
        if(graph.outputs[t]!=no_mixer_track)arrivals[graph.outputs[t]]=std::max(arrivals[graph.outputs[t]],path);
        for(const auto& send:graph.sends[t])arrivals[send.destination]=std::max(arrivals[send.destination],path);
        compensation.output=std::max(compensation.output,path);
    }
    const auto master_input=compensation.output;
    compensation.master=(graph.processors?graph.processors->latency().output:0)+(graph.master_inserts?graph.master_inserts->latency().output:0);
    compensation.output+=compensation.master;
    if(compensation.output>262144)throw std::invalid_argument("PDC output path exceeds 262144 samples");
    std::size_t delay_budget=128*1024*1024;
    std::vector<CompensationDelay> routes(graph.mixer.size());
    std::vector<std::vector<CompensationDelay>> sends(graph.mixer.size());
    for(std::size_t t=0;t<graph.mixer.size();++t){const auto destination=graph.outputs[t];
        const auto target=!graph.hardware_outputs[t].empty()?compensation.output:destination==no_mixer_track?master_input:arrivals[destination];
        routes[t].prepare(target-compensation.track_paths[t],config.output_channels,delay_budget);
        sends[t].resize(graph.sends[t].size());
        for(std::size_t j=0;j<sends[t].size();++j)sends[t][j].prepare(arrivals[graph.sends[t][j].destination]-compensation.track_paths[t],config.output_channels,delay_budget);
    }
    CompensationDelay legacy;legacy.prepare(master_input,config.output_channels,delay_budget);
    compensation.memory_bytes=128*1024*1024-delay_budget;
    auto domains = compile_domains(graph, order, compensation, arrivals);
    std::size_t stream_bytes{}, stream_count{};
    for (auto& voice : graph.voices) if (voice.asset->file) {
        if (config.max_block > static_cast<std::uint32_t>(ReadAhead::page_frames))
            throw std::invalid_argument("streaming supports callback blocks up to 8192 frames");
        stream_bytes += ReadAhead::pages*static_cast<std::size_t>(ReadAhead::page_frames)*voice.asset->channels*sizeof(float);
        if (++stream_count > 32 || stream_bytes > 256*1024*1024)
            throw std::invalid_argument("disk voice budget exceeded (32 voices / 256 MiB)");
    }
    for (auto& voice : graph.voices) if (voice.asset->file) {
        voice.stream = std::make_shared<ReadAhead>(voice.asset->file);
        voice.stream->prime(voice.source_offset+std::clamp(initial.sample-voice.start,Sample{0},voice.length-1));
        if (initial.loop && initial.loop->start < voice.start+voice.length && initial.loop->end > voice.start) {
            const auto source = voice.source_offset+std::max(Sample{0},initial.loop->start-voice.start);
            voice.stream->loop(source); voice.stream->prime(source);
        }
    }
    for(std::size_t i=0;i<graph.tempos.size();++i){const auto& t=graph.tempos[i];if(t.sample<0 || !std::isfinite(t.bpm) || t.bpm<1 || t.bpm>1000 || !std::isfinite(t.quarter) || (i && t.sample<=graph.tempos[i-1].sample))throw std::invalid_argument("invalid prepared tempo map");}
    auto block_size=config.processing_block?config.processing_block:config.max_block;
    for(const auto& chain:graph.inserts)if(chain)block_size=std::min(block_size,chain->config().max_block);
    std::size_t scratch_blocks=graph.mixer.size()*2;
    for(const auto& sends_for_track:graph.sends)scratch_blocks+=sends_for_track.size();
    if(static_cast<std::uint64_t>(block_size)*config.output_channels*scratch_blocks*sizeof(float)>128ULL*1024*1024)
        throw std::invalid_argument("mixer scratch budget exceeded");
    const bool profiling=profile_&&profile_->enabled.load(std::memory_order_relaxed);
    profile_=std::make_shared<ProfileStorage>();profile_->enabled=profiling;
    config_ = config;
    mix_levels_=std::move(levels);channel_levels_=std::move(channel_levels);
    compensation_=std::move(compensation);route_delays_=std::move(routes);send_delays_=std::move(sends);legacy_delay_=std::move(legacy);
    domains_=std::move(domains);
    master_envelope_.resize(config.max_block);
    direct_output_.resize(static_cast<std::size_t>(config.max_block)*config.output_channels);
    graph_ = std::move(graph);set_profiling(profiling);
    domain_owned_.fill(true);domain_input_=nullptr;domain_output_=nullptr;domain_meters_={};
    insert_block_size_=block_size;
    track_block_.assign(graph_.mixer.size(),std::vector<float>(static_cast<std::size_t>(insert_block_size_)*config.output_channels));
    route_contributions_=track_block_;send_contributions_.clear();send_contributions_.resize(graph_.mixer.size());
    for(std::size_t t=0;t<graph_.mixer.size();++t)send_contributions_[t].assign(graph_.sends[t].size(),std::vector<float>(static_cast<std::size_t>(insert_block_size_)*config.output_channels));
    mix_order_ = std::move(order);
    std::size_t width{};for(const auto& level:channel_levels_)width=std::max(width,level.size());
    if(config.processing_workers>1&&width>1)
        workers_=std::make_unique<ChannelWorkers>(static_cast<std::uint32_t>(std::min<std::size_t>(config.processing_workers-1,width-1)),config.worker_mmcss);
    scheduler_overhead_ns_=0;channel_cost_ns_.fill(0);
    measure_channels_=workers_!=nullptr;
    if(workers_) {
        std::array<std::uint64_t,32> samples{};
        const auto empty_job=[](void*,std::uint32_t) noexcept {};
        for(auto& sample:samples){const auto started=std::chrono::steady_clock::now();
            if(!workers_->run(nullptr,empty_job,static_cast<std::uint32_t>(width),1000))throw std::runtime_error("audio worker calibration timed out");
            sample=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count());}
        std::sort(samples.begin(),samples.end());scheduler_overhead_ns_=std::max<std::uint64_t>(1,samples[samples.size()/2]);
    }
    processing_fault_=false;parallel_batches_=0;worker_timeouts_=0;
    MixerUpdate mix; mix.count = graph_.mixer.size(); mix.master_gain = graph_.master_gain;
    std::copy(graph_.mixer.begin(),graph_.mixer.end(),mix.tracks.begin());
    std::copy(graph_.input_monitoring.begin(),graph_.input_monitoring.end(),mix.input_monitoring.begin());
    MixerUpdate discarded_mix; while (mixer_controls_.pop(discarded_mix)) {}
    for (std::size_t i=0; i<graph_.sends.size(); ++i) for (std::size_t j=0; j<graph_.sends[i].size(); ++j) mix.send_gains[i][j] = graph_.sends[i][j].gain;
    set_mix(mix,false);
    for (auto& channel : meter_peaks_) for (auto& p : channel) p.store(0,std::memory_order_relaxed);
    monitor_enabled_ = graph_.monitoring; input_peak_ = 0;
    Control discarded;
    while (controls_.pop(discarded)) {}
    rt_ = initial; pending_seek_.reset(); pending_play_anchor_=false;
    callbacks_ = 0; input_overflows_ = 0; input_underflows_ = 0;
    output_underflows_ = 0; output_overflows_ = 0; deadlines_ = 0;
    invalid_blocks_ = 0; clipped_ = 0; missing_inputs_ = 0; disk_underruns_ = 0;
    max_ns_ = 0; measured_ = 0; min_frames_ = 0; max_frames_ = 0;
    for (auto& bin : load_histogram_) bin = 0;
    publish();
}
void AudioEngine::prime_streams(Sample position) {
    for (auto& voice : graph_.voices) if (voice.stream)
        voice.stream->prime(voice.source_offset+std::clamp(position-voice.start,Sample{0},voice.length-1));
}
void AudioEngine::prime_loop(std::optional<LoopRange> loop) {
    for (auto& voice : graph_.voices) if (voice.stream) {
        if (loop && loop->start < voice.start+voice.length && loop->end > voice.start) {
            const auto source = voice.source_offset+std::max(Sample{0},loop->start-voice.start);
            voice.stream->loop(source); voice.stream->prime(source);
        } else voice.stream->loop(-1);
    }
}
bool AudioEngine::enqueue(Control control) noexcept { control.serial=control_revision_.fetch_add(1,std::memory_order_acq_rel)+1;const auto ok=controls_.push(control);++control_revision_;return ok; }
bool AudioEngine::enqueue_mix(const MixerUpdate& update) noexcept {
    if (update.count != graph_.mixer.size() || !std::isfinite(update.master_gain) || update.master_gain < 0 || update.master_gain > 16) return false;
    for (std::size_t i=0; i<update.count; ++i) {
        const auto& m = update.tracks[i];
        for (std::size_t j=0; j<graph_.sends[i].size(); ++j) if (!std::isfinite(update.send_gains[i][j]) || update.send_gains[i][j] < 0 || update.send_gains[i][j] > 16) return false;
        if (!std::isfinite(m.gain) || m.gain < 0 || m.gain > 16 || !std::isfinite(m.pan) || m.pan < -1 || m.pan > 1) return false;
    }
    auto message=update;message.serial=control_revision_.fetch_add(1,std::memory_order_acq_rel)+1;const auto ok=mixer_controls_.push(message);++control_revision_;return ok;
}
void AudioEngine::set_mix(const MixerUpdate& update, bool ramp) noexcept {
    input_monitoring_=update.input_monitoring;
    bool solo{}; for (std::size_t i=0; i<update.count; ++i) solo = solo || update.tracks[i].solo;
    // A solo bus admits its descendants; a solo track admits its downstream buses.
    std::array<bool,max_mixer_tracks> admitted{};
    for (std::size_t i=0; i<update.count; ++i) admitted[i] = update.tracks[i].solo;
    for (auto it = mix_order_.rbegin(); it != mix_order_.rend(); ++it) {
        const auto destination = graph_.outputs[*it];
        if (destination != no_mixer_track) admitted[*it] = admitted[*it] || admitted[destination];
        for (const auto& send : graph_.sends[*it]) admitted[*it] = admitted[*it] || admitted[send.destination];
    }
    for (const auto i : mix_order_) if (admitted[i]) {
        if (graph_.outputs[i] != no_mixer_track) admitted[graph_.outputs[i]] = true;
        for (const auto& send : graph_.sends[i]) admitted[send.destination] = true;
    }
    mix_ramp_ = ramp ? std::max(1U,config_.sample_rate/200) : 0;
    for (std::size_t i=0; i<update.count; ++i) {
        const auto& m = update.tracks[i];
        gate_target_[i] = m.mute || (solo && !admitted[i]) ? 0.0f : 1.0f;
        if (!ramp) gate_[i] = gate_target_[i];
        gate_step_[i] = ramp ? (gate_target_[i]-gate_[i])/static_cast<float>(mix_ramp_) : 0;
        for (std::size_t j=0; j<graph_.sends[i].size(); ++j) {
            send_target_[i][j] = update.send_gains[i][j];
            if (!ramp) send_gain_[i][j] = send_target_[i][j];
            send_step_[i][j] = ramp ? (send_target_[i][j]-send_gain_[i][j])/static_cast<float>(mix_ramp_) : 0;
        }
        const float gain = gate_target_[i]*m.gain;
        // Unity center preserves 0.1e levels; stereo balance and mono main-pair pan.
        mix_target_[i] = {gain*(m.pan > 0 ? 1-m.pan : 1),gain*(m.pan < 0 ? 1+m.pan : 1)};
        if (config_.output_channels == 1) mix_target_[i] = {gain,gain};
        if (!ramp) mix_gain_[i] = mix_target_[i];
        for (std::size_t c=0; c<2; ++c) mix_step_[i][c] = ramp ? (mix_target_[i][c]-mix_gain_[i][c])/static_cast<float>(mix_ramp_) : 0;
    }
    master_target_ = update.master_gain;
    if (!ramp) master_gain_ = master_target_;
    master_step_ = ramp ? (master_target_-master_gain_)/static_cast<float>(mix_ramp_) : 0;
}
MixerMeters AudioEngine::take_meters() noexcept {
    MixerMeters result;
    for (std::size_t i=0; i<max_mixer_tracks; ++i) result.tracks[i] = {meter_peaks_[i][0].exchange(0),meter_peaks_[i][1].exchange(0)};
    result.master = {meter_peaks_[max_mixer_tracks][0].exchange(0),meter_peaks_[max_mixer_tracks][1].exchange(0)};
    return result;
}
void AudioEngine::publish() noexcept {
    if(speculative_)return;
    publish_delivered(rt_,{});
}
void AudioEngine::publish_delivered(const RealtimeState& delivered,const MixerMeters& meters) noexcept {
    sequence_.fetch_add(1, std::memory_order_acq_rel);
    published_sample_.store(delivered.sample, std::memory_order_relaxed);
    published_play_start_.store(delivered.play_start, std::memory_order_relaxed);
    published_loop_start_.store(delivered.loop ? delivered.loop->start : 0, std::memory_order_relaxed);
    published_loop_end_.store(delivered.loop ? delivered.loop->end : 0, std::memory_order_relaxed);
    published_playback_.store(static_cast<int>(delivered.playback), std::memory_order_relaxed);
    sequence_.fetch_add(1, std::memory_order_release);
    if(speculative_) {
        for(std::size_t t=0;t<max_mixer_tracks;++t)for(std::size_t c=0;c<2;++c){const auto value=c?meters.tracks[t].right:meters.tracks[t].left;meter_peaks_[t][c].store(std::max(value,meter_peaks_[t][c].load(std::memory_order_relaxed)),std::memory_order_relaxed);}
        for(std::size_t c=0;c<2;++c){const auto value=c?meters.master.right:meters.master.left;meter_peaks_[max_mixer_tracks][c].store(std::max(value,meter_peaks_[max_mixer_tracks][c].load(std::memory_order_relaxed)),std::memory_order_relaxed);}
    }
}
bool AudioEngine::try_state(RealtimeState& result) const noexcept {
    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto before = sequence_.load(std::memory_order_acquire);
        if (before & 1U) continue;
        result.play_start=published_play_start_.load(std::memory_order_relaxed);
        result.sample = published_sample_.load(std::memory_order_relaxed);
        result.playback = static_cast<PlaybackState>(published_playback_.load(std::memory_order_relaxed));
        const auto start = published_loop_start_.load(std::memory_order_relaxed);
        const auto end = published_loop_end_.load(std::memory_order_relaxed);
        if (end > start) result.loop = LoopRange{start, end};
        std::atomic_thread_fence(std::memory_order_acquire);
        if (before == sequence_.load(std::memory_order_acquire)) return true;
    }
    return false;
}
RealtimeState AudioEngine::state() const {
    RealtimeState result;if(try_state(result))return result;
    throw std::runtime_error("audio state busy; poll again");
}
bool AudioEngine::anticipation_safe() const noexcept {
    for(bool live:graph_.live_midi)if(live)return false;
    if(!graph_.monitor.empty() || !graph_.recordings.empty())return false;
    const auto safe=[](const auto& chain){return !chain||chain->anticipation_safe();};
    if(!safe(graph_.processors)||!safe(graph_.master_inserts))return false;
    for(const auto& chain:graph_.inserts)if(!safe(chain))return false;
    return true;
}
std::uint64_t AudioEngine::control_revision() const noexcept {
    auto revision=control_revision_.load(std::memory_order_acquire);
    if(graph_.processors)revision+=graph_.processors->control_revision();
    if(graph_.master_inserts)revision+=graph_.master_inserts->control_revision();
    for(const auto& chain:graph_.inserts)if(chain)revision+=chain->control_revision();
    return revision;
}
void AudioEngine::rebase_head(const RealtimeState& head) noexcept {
    rt_=head;pending_seek_.reset();pending_play_anchor_=false;reset_compensation();
    if(graph_.processors)graph_.processors->reset_anticipation();if(graph_.master_inserts)graph_.master_inserts->reset_anticipation();
    for(std::size_t t=0;t<graph_.inserts.size();++t)if(domain_owned_[t]&&graph_.inserts[t])graph_.inserts[t]->reset_anticipation();
}
AudioEngine::MixHead AudioEngine::mix_head() const noexcept {
    return {mix_gain_,mix_target_,mix_step_,send_gain_,send_target_,send_step_,gate_,gate_target_,gate_step_,master_gain_,master_target_,master_step_,mix_ramp_};
}
void AudioEngine::restore_mix(const MixHead& mix) noexcept {
    mix_gain_=mix.gain;mix_target_=mix.target;mix_step_=mix.step;send_gain_=mix.send;send_target_=mix.send_target;send_step_=mix.send_step;
    gate_=mix.gate;gate_target_=mix.gate_target;gate_step_=mix.gate_step;master_gain_=mix.master;master_target_=mix.master_target;master_step_=mix.master_step;mix_ramp_=mix.ramp;
}
void AudioEngine::apply_control(const Control& control) noexcept {
    switch (control.kind) {
        case ControlKind::monitor: monitor_enabled_ = control.a != 0; break;
        case ControlKind::play: if (rt_.playback != PlaybackState::playing) { rt_.play_start=rt_.sample; pending_play_anchor_=pending_seek_.has_value(); } rt_.playback = PlaybackState::playing; break;
        case ControlKind::pause:
            if (rt_.playback == PlaybackState::playing) rt_.playback = PlaybackState::paused;
            break;
        case ControlKind::stop:
            reset_compensation();
            pending_seek_.reset(); pending_play_anchor_=false; rt_.playback = PlaybackState::stopped; rt_.sample = rt_.play_start;
            if (graph_.processors) graph_.processors->panic();
            for(std::size_t t=0;t<graph_.inserts.size();++t)if(domain_owned_[t]&&graph_.inserts[t])graph_.inserts[t]->panic();
            if (graph_.master_inserts) graph_.master_inserts->panic();
            break;
        case ControlKind::prepared_seek:
            if (!graph_.recordings.empty()) { for (const auto& recorder : graph_.recordings) recorder->discontinuity(); break; }
            if (control.a >= 0 && control.a <= max_sample) pending_seek_ = control.a;
            break;
        case ControlKind::seek:
            reset_compensation();
            pending_seek_.reset();
            if (!graph_.recordings.empty()) { for (const auto& recorder : graph_.recordings) recorder->discontinuity(); break; }
            if (control.a >= 0 && control.a <= max_sample) {
                rt_.sample = control.a;
                if (graph_.processors) graph_.processors->panic();
                for(std::size_t t=0;t<graph_.inserts.size();++t)if(domain_owned_[t]&&graph_.inserts[t])graph_.inserts[t]->panic();
                if (graph_.master_inserts) graph_.master_inserts->panic();
            }
            break;
        case ControlKind::loop:
            if (!graph_.recordings.empty()) { for (const auto& recorder : graph_.recordings) recorder->discontinuity(); break; }
            if (control.a >= 0 && control.a < control.b && control.b <= max_sample)
                rt_.loop = LoopRange{control.a, control.b};
            else if (control.a == 0 && control.b == 0) rt_.loop.reset();
            break;
        }
 }
void AudioEngine::consume_controls() noexcept {
    Control control;
    for(int i=0;i<63&&controls_.pop(control);++i){if(ahead_owner_)ahead_owner_->record_control(control);apply_control(control);if(mixed_owner_)++control_revision_;}
}
void AudioEngine::process(const float* input, float* output, std::uint32_t frames, std::uint32_t input_flags) noexcept {
    const ScopedNoDenormals no_denormals;
    profile_block_=profile_->enabled.load(std::memory_order_relaxed);
    std::uint64_t critical_ns{};
    if(!speculative_)callbacks_.fetch_add(1, std::memory_order_relaxed);
    if (!output || frames == 0 || frames > config_.max_block) {
        for (const auto& recorder : graph_.recordings) recorder->input_dropout();
        invalid_blocks_.fetch_add(1, std::memory_order_relaxed);
        // Silence an oversized but valid device buffer without indexing assets.
        if (output) std::fill_n(output, static_cast<std::size_t>(frames) * config_.output_channels, 0.0F);
        return;
    }
    if(processing_fault_.load(std::memory_order_relaxed)) {
        if(mixed_owner_){rt_.playback=PlaybackState::paused;publish();}
        std::fill_n(output,static_cast<std::size_t>(frames)*config_.output_channels,0.f);
        for(const auto& recorder:graph_.recordings)recorder->input_dropout();
        return;
    }
    std::fill_n(direct_output_.data(),static_cast<std::size_t>(frames)*config_.output_channels,0.0f);
    auto minimum = min_frames_.load(std::memory_order_relaxed);
    if (!speculative_ && (minimum == 0 || frames < minimum)) min_frames_.store(frames, std::memory_order_relaxed);
    if (!speculative_ && frames > max_frames_.load(std::memory_order_relaxed)) max_frames_.store(frames, std::memory_order_relaxed);
    consume_controls();
    for(auto& buffer:live_midi_buffers_)buffer.clear();
    LiveMidi midi;
    for(int n=0;n<1023&&live_midi_queue_.pop(midi);++n){
        if(midi.generation!=midi_generation_.load(std::memory_order_relaxed))continue;
        if(midi.track>=graph_.live_midi.size()||!graph_.live_midi[midi.track]||midi.event.channel>15||midi.event.data1>127||midi.event.data2>127||static_cast<int>(midi.event.kind)<0||midi.event.kind>processing::MidiKind::poly_pressure){++midi_dropped_;continue;}
        midi.event.offset=0;
        if(!live_midi_buffers_[midi.track].push(midi.event)){++midi_dropped_;midi_panic_=true;}
    }
    if(midi_panic_.exchange(false)){for(auto& buffer:live_midi_buffers_)buffer.clear();for(std::size_t t=0;t<graph_.inserts.size();++t)if(domain_owned_[t]&&graph_.live_midi[t]&&graph_.inserts[t])graph_.inserts[t]->panic();}
    // A later control prime can replace an earlier queued seek's warm target.
    // Pin/verify the candidate before changing RT position. If unavailable, keep
    // rendering the current head and retry next block; callback never waits.
    bool seek_pinned = false;
    if (pending_seek_) {
        bool ready = true;
        for (auto& voice : graph_.voices) if (owns_voice(voice)&&voice.stream) {
            const auto source = voice.source_offset+std::clamp(*pending_seek_-voice.start,Sample{0},voice.length-1);
            if (!voice.stream->try_begin(source,frames)) ready = false;
        }
        if (ready) {
            reset_compensation();rt_.sample = *pending_seek_; pending_seek_.reset(); seek_pinned = true;
            if(mixed_owner_)++control_revision_;
            if (pending_play_anchor_) { rt_.play_start=rt_.sample; pending_play_anchor_=false; }
            for (auto& voice : graph_.voices) if (owns_voice(voice)&&voice.stream)
                voice.stream->accept_seek(voice.source_offset+std::clamp(rt_.sample-voice.start,Sample{0},voice.length-1));
            if (graph_.processors) graph_.processors->panic();
            for(std::size_t t=0;t<graph_.inserts.size();++t)if(domain_owned_[t]&&graph_.inserts[t])graph_.inserts[t]->panic();
            if (graph_.master_inserts) graph_.master_inserts->panic();
        } else {
            for (auto& voice : graph_.voices) if (owns_voice(voice)&&voice.stream) (void)voice.stream->end();
        }
    }
    std::fill_n(output, static_cast<std::size_t>(frames) * config_.output_channels, 0.0F);
    MixerUpdate mix;
    for(int n=0;n<7&&mixer_controls_.pop(mix);++n){if(ahead_owner_)ahead_owner_->record_mix(mix);set_mix(mix,true);if(mixed_owner_)++control_revision_;}
    if(mixed_owner_)mixed_owner_->begin(frames);
    if(speculative_)last_render_mix_=mix_head();
    std::array<std::array<float,2>,max_mixer_tracks+1> block_peaks{};
    float peak{};
    if (input) for (std::size_t n=0; n<static_cast<std::size_t>(frames)*config_.input_channels; ++n)
        if (std::isfinite(input[n])) peak = std::max(peak,std::abs(input[n]));
    input_peak_.store(peak,std::memory_order_relaxed);
    if (rt_.playback == PlaybackState::playing) for (const auto& recorder : graph_.recordings) {
        if (input_flags & 3U) recorder->input_dropout();
        recorder->capture(input,config_.input_channels,frames,rt_.sample);
    }
    if (!input && monitor_enabled_ && !graph_.monitor.empty()) missing_inputs_.fetch_add(1, std::memory_order_relaxed);
    if (!seek_pinned) for (auto& voice : graph_.voices) if (owns_voice(voice)&&voice.stream) {
        if (!pending_seek_) voice.stream->cancel_seek();
        voice.stream->begin(voice.source_offset+std::clamp(rt_.sample-voice.start,Sample{0},voice.length-1));
    }
    const auto tempo_at=[&](Sample sample){RenderGraph::TempoSegment result;for(const auto& t:graph_.tempos){if(t.sample>sample)break;result=t;}result.quarter+=static_cast<double>(sample-result.sample)*result.bpm/(60*config_.sample_rate);return result;};
    last_render_start_=rt_;
    const auto block_position=rt_.sample;const auto block_playing=rt_.playback==PlaybackState::playing;const auto block_tempo=tempo_at(block_position);
    for(std::uint32_t base=0;base<frames;){
        if(profile_block_){profile_duration_.fill(0);profile_path_.fill(0);}
        if(rt_.playback==PlaybackState::playing&&rt_.loop&&rt_.sample>=rt_.loop->end)
            rt_.sample=rt_.loop->start+(rt_.sample-rt_.loop->start)%(rt_.loop->end-rt_.loop->start);
        const auto position=rt_.sample;const auto playing=rt_.playback==PlaybackState::playing;const auto tempo=tempo_at(position);
        auto count=std::min(insert_block_size_,frames-base);
        if(playing&&rt_.loop)count=static_cast<std::uint32_t>(std::min<Sample>(count,rt_.loop->end-rt_.sample));
        for(auto& block:track_block_)std::fill_n(block.begin(),static_cast<std::size_t>(count)*config_.output_channels,0.f);
        for(std::uint32_t local=0;local<count;++local){
        const auto frame=base+local; const auto at=static_cast<std::size_t>(local)*config_.output_channels;
        const auto out = static_cast<std::size_t>(frame) * config_.output_channels;
        // Monitoring is independent of transport and playback source density.
        if (input && monitor_enabled_) {
            const auto in = static_cast<std::size_t>(frame) * config_.input_channels;
            for (const auto& route : graph_.monitor) {
                const float sample = std::isfinite(input[in+route.input_channel]) ? input[in+route.input_channel]*route.gain : 0;
                if (route.mixer_track != no_mixer_track) {
                    if (input_monitoring_[route.mixer_track]) track_block_[route.mixer_track][at+route.output_channel] += sample;
                } else if (graph_.monitor_track == no_mixer_track) output[out+route.output_channel] += sample;
                else track_block_[graph_.monitor_track][at+route.output_channel] += sample;
            }
        }
        if (rt_.playback == PlaybackState::playing) {
            if (rt_.loop && rt_.sample >= rt_.loop->end)
                rt_.sample = rt_.loop->start + (rt_.sample - rt_.loop->start) % (rt_.loop->end - rt_.loop->start);
            for (const auto& voice : graph_.voices) {
                if(!owns_voice(voice))continue;
                if (rt_.sample < voice.start || rt_.sample - voice.start >= voice.length) continue;
                const auto source_frame = rt_.sample - voice.start + voice.source_offset;
                const auto source = static_cast<std::size_t>(source_frame)*voice.asset->channels;
                for (const auto& route : voice.routes) {
                    float sample{};
                    if (voice.stream) (void)voice.stream->read(source_frame,route.source_channel,sample);
                    else sample = voice.asset->samples[source+route.source_channel];
                    if (voice.mixer_track == no_mixer_track) output[out+route.output_channel] += sample*route.gain;
                    else track_block_[voice.mixer_track][at+route.output_channel] += sample*route.gain;
                }
            }
            if (rt_.sample < max_sample) ++rt_.sample;
            else rt_.playback = PlaybackState::stopped;
        }
        }
        job_base_=base;job_ramp_=mix_ramp_;job_frames_=count;job_position_=position;
        job_playing_=playing;job_tempo_=tempo.bpm;job_quarter_=tempo.quarter;
        for(std::uint32_t local=0;local<count;++local)for(std::uint32_t c=0;c<config_.output_channels;++c){const auto out=static_cast<std::size_t>(base+local)*config_.output_channels+c;output[out]=legacy_delay_.sample(output[out],c);}
        for (const auto t : mix_order_) {
            if(workers_) {
                if(t==mix_order_.front() || mix_levels_[t]!=job_level_) {
                    job_level_=mix_levels_[t];job_frames_=count;job_position_=position;
                    job_playing_=playing;job_tempo_=tempo.bpm;job_quarter_=tempo.quarter;
                    const auto jobs=static_cast<std::uint32_t>(channel_levels_[job_level_].size());
                    std::uint64_t total{},largest{};
                    for(const auto channel:channel_levels_[job_level_]){const auto cost=channel_cost_ns_[channel]*count;total+=cost;largest=std::max(largest,cost);}
                    const auto predicted=std::max(largest,total/(workers_->count()+1));
                    // First chunk measures serial cost. Avoid dispatch when predicted savings cannot cover calibrated wake cost.
                    const bool dispatch=jobs>1&&(!config_.adaptive_parallel||total>predicted+scheduler_overhead_ns_*2);
                    if(dispatch)parallel_batches_.fetch_add(1,std::memory_order_relaxed);
                    bool completed=true;
                    if(dispatch)completed=workers_->run(this,channel_job,jobs,config_.worker_wait_ms);
                    else for(std::uint32_t index=0;index<jobs;++index)channel_job(this,index);
                    if(!completed) {
                        processing_fault_.store(true,std::memory_order_relaxed);worker_timeouts_.fetch_add(1,std::memory_order_relaxed);
                        rt_.playback=PlaybackState::paused;
                        std::fill_n(output,static_cast<std::size_t>(frames)*config_.output_channels,0.f);
                        for(const auto& recorder:graph_.recordings)recorder->input_dropout();
                        // Leave worker-owned buffers/plugins/context intact until control-thread join.
                        for(auto& voice:graph_.voices)if(owns_voice(voice)&&voice.stream)(void)voice.stream->end();
                        publish();return;
                    }
                }
            } else process_channel(t);
            for(std::size_t c=0;c<2;++c)block_peaks[t][c]=std::max(block_peaks[t][c],channel_peaks_[t][c]);
            if(domain_output_&&domain_owned_[t]) {
                const auto copy=[&](std::size_t edge,const auto& source){std::copy_n(source.data(),static_cast<std::size_t>(count)*config_.output_channels,
                    domain_output_+edge*static_cast<std::size_t>(domain_stride_)*config_.output_channels+static_cast<std::size_t>(base)*config_.output_channels);};
                copy(domain_edges_[t],route_contributions_[t]);
                for(std::size_t j=0;j<graph_.sends[t].size();++j)copy(domain_edges_[t]+1+j,send_contributions_[t][j]);
            }
            for(std::uint32_t local=0;local<count;++local){const auto at=static_cast<std::size_t>(local)*config_.output_channels;const auto out=static_cast<std::size_t>(base+local)*config_.output_channels;
            for (std::uint32_t c=0; c<config_.output_channels; ++c) {
                const float routed=route_contributions_[t][at+c];
                const auto destination = graph_.outputs[t];
                const auto& hardware = graph_.hardware_outputs[t];
                if (!hardware.empty()) {
                    if (hardware.size() == 1 && c < 2) direct_output_[out+hardware[0]] += routed*(config_.output_channels == 1 ? 1.0f : 0.5f);
                    else if (hardware.size() == 2 && c < 2) direct_output_[out+hardware[c]] += routed;
                }
                else if (destination == no_mixer_track) output[out+c] += routed;
                else track_block_[destination][at+c] += routed;
                for (std::size_t j=0; j<graph_.sends[t].size(); ++j) {
                    const auto& send = graph_.sends[t][j];
                    track_block_[send.destination][at+c] += send_contributions_[t][j][at+c];
                }
            }
            }
        }
        for(std::uint32_t local=0;local<count;++local){const auto frame=base+local;
        master_envelope_[frame] = master_gain_;
        if (mix_ramp_) {
            master_gain_ += master_step_;
            if (--mix_ramp_ == 0) { master_gain_ = master_target_; mix_gain_ = mix_target_; gate_ = gate_target_; send_gain_ = send_target_; }
        }
    }
        if(profile_block_){std::uint64_t longest{};for(const auto t:mix_order_){const auto path=profile_path_[t]+profile_duration_[t];longest=std::max(longest,path);
            const auto propagate=[&](std::size_t to){if(to!=no_mixer_track)profile_path_[to]=std::max(profile_path_[to],path);};propagate(graph_.outputs[t]);for(const auto& send:graph_.sends[t])propagate(send.destination);}
            critical_ns+=longest;}
        base+=count;
    }
    for (auto& voice : graph_.voices) if (owns_voice(voice)&&voice.stream && voice.stream->end())
        disk_underruns_.fetch_add(1,std::memory_order_relaxed);
    const auto output_samples = static_cast<std::size_t>(frames) * config_.output_channels;
    for (std::size_t i = 0; i < output_samples; ++i)
        if (!std::isfinite(output[i])) output[i] = 0;
    const auto master_start=profile_block_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    if (graph_.processors) graph_.processors->process(output,frames,block_position,block_playing,block_tempo.bpm,block_tempo.quarter);
    if (graph_.master_inserts) graph_.master_inserts->process(output,frames,block_position,block_playing,block_tempo.bpm,block_tempo.quarter);
    if(profile_block_){const auto ns=elapsed_ns(master_start);(speculative_?profile_->ahead_master:profile_->device_master).record(ns,frames);
        (speculative_?profile_->ahead_path:profile_->device_path).record(critical_ns+ns,frames);}
    for (std::uint32_t frame=0; frame<frames; ++frame) {
        const auto offset = static_cast<std::size_t>(frame)*config_.output_channels;
        std::array<float,max_channels> master{};
        for (std::uint32_t c=0; c<config_.output_channels; ++c) {
            auto sample = output[offset+c]*master_envelope_[frame];
            if (!std::isfinite(sample)) sample = 0;
            master[c] = sample;
            if (c<2) block_peaks[max_mixer_tracks][c] = std::max(block_peaks[max_mixer_tracks][c],std::abs(sample));
            output[offset+c] = direct_output_[offset+c];
        }
        if (graph_.master_outputs.empty()) {
            for (std::uint32_t c=0; c<config_.output_channels; ++c) output[offset+c] += master[c];
        } else if (graph_.master_outputs.size() == 1) {
            output[offset+graph_.master_outputs[0]] += config_.output_channels == 1 ? master[0] : (master[0]+master[1])*0.5f;
        } else {
            output[offset+graph_.master_outputs[0]] += master[0]; output[offset+graph_.master_outputs[1]] += master[1];
        }
        for (std::uint32_t c=0; c<config_.output_channels; ++c) {
            auto& sample = output[offset+c]; if (!std::isfinite(sample)) sample = 0;
            if (sample > 1 || sample < -1) { clipped_.fetch_add(1,std::memory_order_relaxed); sample = std::clamp(sample,-1.0f,1.0f); }
        }
    }
    last_render_meters_={};for(std::size_t t=0;t<max_mixer_tracks;++t)last_render_meters_.tracks[t]={block_peaks[t][0],block_peaks[t][1]};last_render_meters_.master={block_peaks[max_mixer_tracks][0],block_peaks[max_mixer_tracks][1]};
    if(!speculative_)for (std::size_t t=0; t<=max_mixer_tracks; ++t) for (std::size_t c=0; c<2; ++c)
        meter_peaks_[t][c].store(std::max(block_peaks[t][c],meter_peaks_[t][c].load(std::memory_order_relaxed)),std::memory_order_relaxed);
    // Normalize at an exact block boundary for coherent displayed loop position.
    if (rt_.loop && rt_.playback == PlaybackState::playing && rt_.sample >= rt_.loop->end)
        rt_.sample = rt_.loop->start + (rt_.sample - rt_.loop->start) % (rt_.loop->end - rt_.loop->start);
    if(mixed_owner_)mixed_owner_->end();
    publish();
}
void AudioEngine::observe(std::uint64_t ns, std::uint32_t frames, std::uint32_t flags) noexcept {
    // Backend-neutral bits: 1 input underflow, 2 input overflow,
    // 4 output underflow, 8 output overflow.
    if (flags & 1U) input_underflows_.fetch_add(1, std::memory_order_relaxed);
    if (flags & 2U) input_overflows_.fetch_add(1, std::memory_order_relaxed);
    if (flags & 4U) output_underflows_.fetch_add(1, std::memory_order_relaxed);
    if (flags & 8U) output_overflows_.fetch_add(1, std::memory_order_relaxed);
    measured_.fetch_add(1, std::memory_order_relaxed);
    if (ns > max_ns_.load(std::memory_order_relaxed)) max_ns_.store(ns, std::memory_order_relaxed);
    if (!frames) return;
    const double load = static_cast<double>(ns) * config_.sample_rate / (static_cast<double>(frames) * 1e9);
    if (load >= 1) deadlines_.fetch_add(1, std::memory_order_relaxed);
    const auto bin = static_cast<std::size_t>(std::clamp(std::ceil(load * 100), 0.0, 100.0));
    load_histogram_[bin].fetch_add(1, std::memory_order_relaxed);
}
void AudioEngine::set_profiling(bool enabled) noexcept {
    profile_->enabled.store(enabled,std::memory_order_relaxed);
    for(const auto& chain:graph_.inserts)if(chain)chain->set_profiling(enabled);
    if(graph_.processors)graph_.processors->set_profiling(enabled);if(graph_.master_inserts)graph_.master_inserts->set_profiling(enabled);
}
EngineProfile AudioEngine::profile() const noexcept {EngineProfile p;p.enabled=profile_->enabled.load(std::memory_order_relaxed);p.channels=graph_.mixer.size();
    for(std::size_t i=0;i<p.channels;++i){p.device[i]=profile_->device[i].read();p.ahead[i]=profile_->ahead[i].read();}
    for(std::size_t i=0;i<8;++i){p.device_workers[i]=profile_->device_workers[i].read();p.ahead_workers[i]=profile_->ahead_workers[i].read();}
    p.device_master=profile_->device_master.read();p.ahead_master=profile_->ahead_master.read();p.device_path=profile_->device_path.read();p.ahead_path=profile_->ahead_path.read();return p;
}
std::vector<std::shared_ptr<processing::PreparedGraph>> AudioEngine::profile_graphs() const {auto graphs=graph_.inserts;graphs.resize(graph_.mixer.size());graphs.push_back(graph_.processors);graphs.push_back(graph_.master_inserts);return graphs;}
Metrics AudioEngine::metrics() const {
    Metrics m;
    m.ahead_memory_bytes=ahead_memory_bytes_.load();
    m.anticipation_active=anticipation_active_.load();m.process_buffer_frames=process_buffer_frames_.load();m.ahead_buffered_frames=ahead_buffered_frames_.load();
    m.mixed_anticipation=mixed_anticipation_.load();m.monitoring_available=!graph_.monitor.empty()||std::any_of(graph_.live_midi.begin(),graph_.live_midi.end(),[](bool v){return v;});m.monitoring_compensation_frames=domains_.monitoring_compensation_frames;
    m.ahead_underruns=ahead_underruns_.load();m.ahead_invalidations=ahead_invalidations_.load();m.ahead_max_process_ns=ahead_max_process_ns_.load();
    m.processing_workers=(workers_?workers_->count()+1:1)+(m.mixed_anticipation?1:0);
    m.audio_scheduled_workers=workers_?workers_->audio_scheduled():0;
    m.parallel_batches=parallel_batches_.load();m.worker_timeouts=worker_timeouts_.load();m.processing_fault=processing_fault_.load();
    m.scheduler_overhead_ns=scheduler_overhead_ns_;
    m.input_peak = input_peak_.load();
    m.disk_underruns = disk_underruns_.load();
    for (const auto& voice : graph_.voices) if (voice.stream) m.disk_errors += voice.stream->errors();
    m.callbacks = callbacks_.load(); m.input_overflows = input_overflows_.load();
    m.input_underflows = input_underflows_.load(); m.output_underflows = output_underflows_.load();
    m.output_overflows = output_overflows_.load(); m.deadline_misses = deadlines_.load();
    m.invalid_blocks = invalid_blocks_.load(); m.clipped_samples = clipped_.load();
    m.missing_inputs = missing_inputs_.load(); m.max_callback_ns = max_ns_.load();
    m.measured_callbacks = measured_.load(); m.min_frames = min_frames_.load(); m.max_frames = max_frames_.load();
    const auto percentile = [this](double fraction) {
        std::array<std::uint64_t, 101> bins{};
        std::uint64_t total = 0;
        for (std::size_t i = 0; i < bins.size(); ++i) { bins[i] = load_histogram_[i].load(); total += bins[i]; }
        if (!total) return 0.0;
        const auto wanted = static_cast<std::uint64_t>(std::ceil(static_cast<double>(total) * fraction));
        std::uint64_t sum = 0;
        for (std::size_t i = 0; i < bins.size(); ++i) { sum += bins[i]; if (sum >= wanted) return static_cast<double>(i); }
        return 100.0;
    };
    m.p50_load_percent = percentile(0.5); m.p95_load_percent = percentile(0.95); m.p99_load_percent = percentile(0.99);
    return m;
}
EngineTransport::EngineTransport(std::shared_ptr<AudioEngine> engine, Timeline timeline)
    : engine_(std::move(engine)), timeline_(std::move(timeline)) {
    if (!engine_) throw std::invalid_argument("missing shared audio engine");
    last_ = state();
}
TransportState EngineTransport::state() const {
    const auto s = engine_->state();
    return {s.playback, s.sample, timeline_.musical_position(timeline_.to_ticks(s.sample)), s.loop};
}
void EngineTransport::rebind_timeline(Timeline timeline) {
    if (timeline.sample_rate() != engine_->config().sample_rate)
        throw std::invalid_argument("timeline sample rate must match audio device");
    (void)timeline.to_ticks(engine_->state().sample);
    timeline_ = std::move(timeline);
    poll();
}
void EngineTransport::send(Control control) {
    if (!engine_->enqueue(control)) throw std::runtime_error("audio command queue full; retry on control thread");
}
void EngineTransport::play() { send({ControlKind::play}); }
void EngineTransport::pause() { send({ControlKind::pause}); }
void EngineTransport::stop() {
    send({ControlKind::stop}); // stopping must work even when media disappears
    try { engine_->prime_streams(0); } catch (const std::exception&) {}
}
void EngineTransport::seek(Sample sample) {
    if (sample < 0 || sample > max_sample) throw std::invalid_argument("invalid seek");
    (void)timeline_.to_ticks(sample);
    engine_->prime_streams(sample);
    send({ControlKind::prepared_seek, sample});
}
void EngineTransport::set_loop(std::optional<LoopRange> loop) {
    if (loop && (loop->start < 0 || loop->end > max_sample || loop->start >= loop->end))
        throw std::invalid_argument("invalid loop");
    if (loop) { (void)timeline_.to_ticks(loop->start); (void)timeline_.to_ticks(loop->end); }
    engine_->prime_loop(loop);
    send({ControlKind::loop, loop ? loop->start : 0, loop ? loop->end : 0});
}
Connection EngineTransport::subscribe(std::function<void(const TransportState&)> callback) {
    return changes_.subscribe(std::move(callback));
}
void EngineTransport::poll() {
    auto next = state();
    if (next == last_) return;
    last_ = next;
    changes_.publish(last_);
}
} // namespace mrs::audio
