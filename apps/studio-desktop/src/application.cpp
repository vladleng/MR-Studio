#include <mrs/desktop.hpp>
#include <mrs/midi_import.hpp>
#ifdef MRS_HAS_VST3
#include <mrs/vst3.hpp>
#endif
#include <mrs/offline_device.hpp>
#ifdef MRS_HAS_ARA
#include <mrs/read_ahead.hpp>
#endif
#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
namespace mrs::desktop {
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
bool channel_order_only(Project a,Project b){
    const auto byId=[](const Track& x,const Track& y){return x.id.value<y.id.value;};
    std::sort(a.tracks.begin(),a.tracks.end(),byId);std::sort(b.tracks.begin(),b.tracks.end(),byId);
    return a==b;
}
bool channel_color_only(Project a,const Project& b){
    if(a.tracks.size()!=b.tracks.size())return false;
    for(std::size_t i=0;i<a.tracks.size();++i)a.tracks[i].color=b.tracks[i].color;
    return a==b;
}
audio::ClickSettings mapped_click(audio::ClickSettings settings, const std::vector<int>& outputs) {
    for (auto* channel : {&settings.output_left, &settings.output_right}) {
        if (*channel < 0) continue;
        const auto found = std::find(outputs.begin(), outputs.end(), *channel);
        require(found != outputs.end(), "Click output is unavailable; enable it in Audio settings");
        *channel = static_cast<int>(found - outputs.begin());
    }
    return settings;
}
void configure_count(audio::RenderGraph& graph,const Project& project,const audio::ClickSettings& click,Sample sample){
    if(!click.count_bars)return;
    const Timeline timeline(project.time,project.sample_rate);const auto tick=timeline.to_ticks(sample);const auto musical=timeline.musical_position(tick);
    auto meter=project.time.meters.front();for(const auto& m:project.time.meters){if(m.bar>musical.bar)break;meter=m;}
    auto tempo=project.time.tempos.front();for(const auto& t:project.time.tempos){if(t.tick>tick)break;tempo=t;}
    graph.count_beats=meter.numerator;graph.count_beat_frames=60.*project.sample_rate/tempo.bpm*4/meter.denominator;
    graph.count_frames=static_cast<Sample>(std::llround(graph.count_beat_frames*meter.numerator*click.count_bars));
}
std::string utf8(const std::filesystem::path& path) {
    auto value = path.u8string(); return {reinterpret_cast<const char*>(value.data()),value.size()};
}
std::string media_ref(const std::filesystem::path& path) {
    const auto value = path.generic_u8string(); return {reinterpret_cast<const char*>(value.data()),value.size()};
}
}
std::string_view workspace_name(Workspace w) {
    switch (w) { case Workspace::arrange: return "Arrange"; case Workspace::edit: return "Edit";
        case Workspace::mix: return "Mix"; case Workspace::live: return "Live"; }
    throw std::invalid_argument("unknown workspace");
}
void DeviceProfile::validate() const {
    require(!name.empty() && name.size() <= 128 && !device_name.empty() && device_name.size() <= 4096,"invalid profile name/device");
    require(rate >= 8000 && rate <= 768000 && buffer >= 8 && buffer <= 8192,"invalid profile rate/buffer");
    require(monitor_input >= -1 && monitor_input < 64 && input_label.size() <= 256,"invalid profile input");
    require(!outputs.empty() && outputs.size() <= 64 && output_labels.size() == outputs.size(),"invalid profile outputs");
    std::set<int> seen;
    for (std::size_t i=0; i<outputs.size(); ++i) require(outputs[i] >= 0 && outputs[i] < 64 && seen.insert(outputs[i]).second && output_labels[i].size() <= 256,"invalid profile channel");
    require(monitor_input >= 0 || input_label.empty(),"disabled profile input has a label");
}
DeviceProfile capture_profile(std::string name, const audio::DeviceInfo& info, const audio::DeviceConfig& config) {
    audio::validate_device_config(info,config);
    require(config.inputs.size() <= 1,"profiles support one monitor input");
    DeviceProfile result; result.name = std::move(name); result.device_name = info.name;
    result.rate = config.sample_rate; result.buffer = config.buffer_frames; result.outputs = config.outputs;
    for (const auto channel : config.outputs) result.output_labels.push_back(info.outputs[static_cast<std::size_t>(channel)]);
    if (!config.inputs.empty()) { result.monitor_input = config.inputs.front(); result.input_label = info.inputs[static_cast<std::size_t>(result.monitor_input)]; }
    result.validate(); return result;
}
audio::DeviceConfig resolve_profile(const DeviceProfile& profile, const audio::DeviceInfo& info) {
    profile.validate(); require(profile.device_name == info.name,"profile device is unavailable; select its original device");
    audio::DeviceConfig result{info.index,profile.rate,profile.buffer,{},profile.outputs};
    if (profile.monitor_input >= 0) result.inputs = {profile.monitor_input};
    audio::validate_device_config(info,result);
    for (std::size_t i=0; i<profile.outputs.size(); ++i) require(info.outputs[static_cast<std::size_t>(profile.outputs[i])] == profile.output_labels[i],"profile output labels changed; review channels and save a new profile");
    if (profile.monitor_input >= 0) require(info.inputs[static_cast<std::size_t>(profile.monitor_input)] == profile.input_label,"profile input label changed; review channels and save a new profile");
    return result;
}
void Preferences::validate() const {
    ai.validate();
    click.validate();
    (void)workspace_name(workspace); require(rate >= 8000 && rate <= 768000,"invalid sample rate");
    require(buffer >= 8 && buffer <= 8192,"invalid buffer"); require(monitor_input >= -1 && monitor_input < 64,"invalid monitor input");
    require(processing_workers>=1&&processing_workers<=8,"invalid audio worker limit (1..8)");
    require(process_buffer_frames<=8192&&(!process_buffer_frames||process_buffer_frames>=buffer),"Process Buffer must be off or at least Device Buffer (max 8192)");
    require(!outputs.empty() && outputs.size() <= 64 && device_name.size() <= 4096,"invalid device preferences");
    require(recent_projects.size() <= 10,"too many recent projects");
    require(profiles.size() <= 16,"too many device profiles (maximum 16)");
    std::set<std::string> names;
    for (const auto& profile : profiles) { profile.validate(); require(names.insert(profile.name).second,"duplicate profile name"); }
    std::set<std::string> paths;
    for (const auto& path : recent_projects) require(!path.empty() && path.size() <= 4096 && paths.insert(path).second,"invalid recent project");
    std::set<int> seen; for (auto o : outputs) require(o >= 0 && o < 64 && seen.insert(o).second,"invalid/duplicate output");
}
std::string encode_preferences(const Preferences& p) {
    p.validate(); std::ostringstream out;
    out << "MRS_DESKTOP_CONFIG 8\n" << static_cast<int>(p.workspace) << ' ' << p.rate << ' ' << p.buffer << ' ' << p.monitor_input
        << ' ' << std::quoted(p.device_name) << ' ' << p.outputs.size();
    for (auto o : p.outputs) out << ' ' << o;
    out << ' ' << p.reconnect_audio << "\n" << p.recent_projects.size() << '\n';
    for (const auto& path : p.recent_projects) out << std::quoted(path) << '\n';
    out << p.profiles.size() << '\n';
    for (const auto& v : p.profiles) {
        out << std::quoted(v.name) << ' ' << std::quoted(v.device_name) << ' ' << v.rate << ' ' << v.buffer << ' ' << v.monitor_input << ' ' << std::quoted(v.input_label) << ' ' << v.outputs.size();
        for (std::size_t i=0; i<v.outputs.size(); ++i) out << ' ' << v.outputs[i] << ' ' << std::quoted(v.output_labels[i]);
        out << '\n';
    }
    out << p.processing_workers << ' ' << p.process_buffer_frames << '\n';
    out << p.click.playback << ' ' << p.click.recording << ' ' << p.click.accent << ' ' << p.click.level << ' ' << p.click.count_bars << '\n';
    out << static_cast<int>(p.ai.mode) << ' ' << p.ai.enabled << ' ' << p.ai.stored_plugin_parameters << ' ' << p.ai.automatic_tools << '\n';
    const auto bytes = out.str(); require(bytes.size() <= 524288,"config too large"); return bytes;
}
Preferences decode_preferences(std::string_view bytes) {
    require(bytes.size() <= 524288,"config too large"); std::istringstream in{std::string(bytes)};
    std::string magic; int version{},workspace{}; std::size_t count{}; Preferences p;
    require(static_cast<bool>(in >> magic >> version) && magic == "MRS_DESKTOP_CONFIG" && (version >= 1 && version <= 8),"unsupported config");
    require(static_cast<bool>(in >> workspace >> p.rate >> p.buffer >> p.monitor_input >> std::quoted(p.device_name) >> count) && workspace >= 0 && workspace <= 3 && count > 0 && count <= 64,"invalid config");
    p.workspace = static_cast<Workspace>(workspace); p.outputs.clear();
    for (std::size_t i = 0; i < count; ++i) { int o{}; require(static_cast<bool>(in >> o),"truncated config"); p.outputs.push_back(o); }
    if (version >= 2) {
        int enabled{}; require(static_cast<bool>(in >> enabled) && (enabled == 0 || enabled == 1),"invalid reconnect preference");
        p.reconnect_audio = enabled != 0;
    } else p.reconnect_audio = !p.device_name.empty();
    if (version >= 3) {
        require(static_cast<bool>(in >> count) && count <= 10,"invalid recent project count");
        for (std::size_t i=0; i<count; ++i) {
            std::string path; in >> std::ws;
            require(in.peek() == '"' && static_cast<bool>(in >> std::quoted(path)),"invalid recent project path");
            p.recent_projects.push_back(std::move(path));
        }
    }
    if (version >= 4) {
        require(static_cast<bool>(in >> count) && count <= 16,"invalid profile count");
        const auto quoted = [&](std::string& value) { in >> std::ws; require(in.peek() == '"' && static_cast<bool>(in >> std::quoted(value)),"invalid profile string"); };
        for (std::size_t i=0; i<count; ++i) {
            DeviceProfile v; std::size_t channels{}; quoted(v.name); quoted(v.device_name);
            require(static_cast<bool>(in >> v.rate >> v.buffer >> v.monitor_input),"truncated profile"); quoted(v.input_label);
            require(static_cast<bool>(in >> channels) && channels > 0 && channels <= 64,"invalid profile channels");
            v.outputs.clear();
            for (std::size_t j=0; j<channels; ++j) { int channel{}; std::string label; require(static_cast<bool>(in >> channel),"truncated profile output"); quoted(label); v.outputs.push_back(channel); v.output_labels.push_back(std::move(label)); }
            p.profiles.push_back(std::move(v));
        }
    }
    if(version>=5)require(static_cast<bool>(in>>p.processing_workers),"truncated audio worker limit");
    if(version>=6)require(static_cast<bool>(in>>p.process_buffer_frames),"truncated Process Buffer");
    if(version>=7){int play{},record{},accent{};require(static_cast<bool>(in>>play>>record>>accent>>p.click.level>>p.click.count_bars)&&play>=0&&play<=1&&record>=0&&record<=1&&accent>=0&&accent<=1,"invalid metronome preferences");p.click.playback=play!=0;p.click.recording=record!=0;p.click.accent=accent!=0;}
    if(version>=8){int mode{},enabled{},parameters{};require(static_cast<bool>(in>>mode>>enabled>>parameters>>p.ai.automatic_tools)&&mode>=0&&mode<=3&&enabled>=0&&enabled<=1&&parameters>=0&&parameters<=1,"invalid AI permission preferences");p.ai.mode=static_cast<ai::PermissionMode>(mode);p.ai.enabled=enabled!=0;p.ai.stored_plugin_parameters=parameters!=0;}
    in >> std::ws; require(in.eof(),"extra config data"); p.validate(); return p;
}
void remember_project(Preferences& p, const std::filesystem::path& path) {
    const auto name = utf8(std::filesystem::absolute(path).lexically_normal());
    require(!name.empty() && name.size() <= 4096,"recent project path too long");
    auto next = p;
    std::erase(next.recent_projects,name); next.recent_projects.insert(next.recent_projects.begin(),name);
    if (next.recent_projects.size() > 10) next.recent_projects.resize(10);
    next.validate(); p = std::move(next);
}
std::vector<int> parse_outputs(std::string_view bytes) {
    require(!bytes.empty() && bytes.size() <= 256,"enter output numbers, e.g. 1,2");
    std::vector<int> result; std::set<int> seen;
    while (!bytes.empty()) {
        const auto comma = bytes.find(','); auto part = bytes.substr(0,comma);
        while (!part.empty() && part.front() == ' ') part.remove_prefix(1);
        while (!part.empty() && part.back() == ' ') part.remove_suffix(1);
        int n{}; const auto parsed = std::from_chars(part.data(),part.data()+part.size(),n);
        require(parsed.ec == std::errc{} && parsed.ptr == part.data()+part.size() && n >= 1 && n <= 64 && seen.insert(n).second,"invalid/duplicate physical output");
        result.push_back(n-1);
        if (comma == std::string_view::npos) break;
        bytes.remove_prefix(comma+1); require(!bytes.empty(),"trailing output comma");
    }
    return result;
}
Logger::Logger(const std::filesystem::path& path) : stream_(path,std::ios::app) {}
void Logger::write(std::string_view text) {
    if (stream_) { stream_ << text << '\n'; stream_.flush(); }
}
persistence::ProjectDocument foundation_demo() {
    auto d = persistence::demo_document(); d.project = musical_demo_project();
    d.project.folders.clear(); d.project.tracks = {{{"demo-track"},"Demo tone",TrackKind::audio,{}}};
    d.project.clips = {{{"demo-clip"},{"demo-track"},"Demo tone 220 Hz",0,32*48000,0,"mrs:demo-tone"}};
    d.project.title = "Moon River — Foundation Demo";
    d.mixer = {{{"demo-track"},1,0,false,false}}; d.actions.clear();
    d.live_notes = "Demo harmony and sections; not inferred from audio.";
    d.validate(); return d;
}
Application::Application(std::unique_ptr<IMidiOutputBackend> backend) : engine_(std::make_shared<audio::AudioEngine>()),external_midi_(std::make_shared<audio::ExternalMidiQueue>()) {if(backend)midi_outputs_=std::make_unique<MidiOutputs>(external_midi_,std::move(backend));demo();}
Application::~Application() {
    midi_inputs_.reset(); // joins bridge/closes driver callbacks before engine/device retirement
    if (device_) device_->close();
    stop_wav_imports();
    stop_stretch();
    ++*ai_permission_revision_; // invalidate captured reads before worker shutdown
    if (ai_worker_) ai_worker_->shutdown(); // no audio callback waits for AI
    stop_reverb_loaders();
    external_midi_->panic();midi_outputs_.reset();
    // Finalize on clean shutdown; a completed file remains recoverable even if
    // the UI did not attach/save its project reference.
    for (auto& capture : captures_) try { (void)capture.recorder->finish(); } catch (const std::exception&) {}
    for (auto& [key,value] : assets_) { (void)key; *value.cancel = true; }
}
void Application::workspace(Workspace w) { (void)workspace_name(w); workspace_ = w; }
void Application::replace(persistence::ProjectDocument next) {
    cancel_wav_import();
    require_ai_owner();
    require_not_recording(); next.validate(); armed_.reset(); armed_tracks_.clear();
    // Existing MIXR channels hydrate the shared command model, including legacy archives.
    for (const auto& m : next.mixer) for (auto& t : next.project.tracks) if (t.id == m.track)
        t.mix = {m.gain,m.pan,m.mute,m.solo};
    // Build application models before releasing the old session.
    {std::vector<Id> tracks;for(const auto& t:next.project.tracks)if(t.kind!=TrackKind::midi)tracks.push_back(t.id);(void)audio::compile_midi_clips(next.project,tracks);(void)audio::compile_midi_events(next.project,tracks);}
    auto projects = std::make_shared<ProjectStore>(next.project);
    auto graphs = std::make_shared<processing::GraphStore>(next.graph);
    const auto ai_session = new_id();
    if(ai_worker_)ai_worker_->cancel_all();
    disconnect();performance_plan_.reset();performance_token_=0;staged_playback_.reset();
    stop_stretch();
    engine_->prepare({next.project.sample_rate,0,2,8192},{});
    auto transport = std::make_shared<audio::EngineTransport>(engine_,Timeline(next.project.time,next.project.sample_rate));
    auto musical = std::make_unique<MusicalTimeline>(Services{projects,transport},[this]{require_not_recording();require(!engine_->state().count_remaining,"End count-in before navigating");});
    for (auto& [key,value] : assets_) { (void)key; *value.cancel = true; }
    assets_.clear(); import_staging_.clear(); owned_media_.clear(); waveform_error_.clear();
#ifdef MRS_HAS_ARA
    ara_.reset();ara_cycle_.reset();
#endif
    document_ = std::move(next);
    mixer_tracks_.clear(); applied_mix_revision_ = 0;
    services_ = {projects,transport}; graphs_ = std::move(graphs);
    ai_session_ = ai_session;
    ++*ai_permission_revision_;
    ai_receipts_.clear();ai_operations_.clear();for(auto& preview:ai_previews_)if(preview->state_==ai::PreviewState::preview)preview->state_=ai::PreviewState::stale;ai_previews_.clear();
    ai_applied_operations_.clear();
    ai_project_changed_=projects->subscribe([this](const ProjectState&){if(ai_worker_)ai_worker_->cancel_all();});
    ai_graph_changed_=graphs_->subscribe([this](const processing::GraphSnapshot&){if(ai_worker_)ai_worker_->cancel_all();});
    transport_ = std::move(transport); musical_ = std::move(musical);
    saved_project_revision_ = saved_graph_revision_ = 0; unsaved_ = true;
    engine_->prepare({document_.project.sample_rate,0,2,8192},{});
    start_empty_clock(); transport_->poll(); musical_->refresh();
}
void Application::start_empty_clock(bool prepare_plugins) {
    auto device = audio::make_offline_device();
    audio::DeviceConfig c{0,document_.project.sample_rate,128,{}, {0,1}};
    audio::RenderGraph graph;
    const auto p = services_.projects->state().project;
    prepare_mixer(graph);
    prepare_midi_clips(graph);
    if(prepare_plugins)prepare_inserts(graph,c);
    engine_->prepare({p->sample_rate,0,2,8192},std::move(graph));
    const auto clock_infos = device->enumerate(); device_info_ = clock_infos.front();
    device->open(c,engine_); device->start(); device_ = std::move(device); device_config_ = c; default_inputs_.clear(); audio_name_ = "Offline clock (no sound)";
}
void Application::demo() {
    require_not_recording();
    path_.clear(); asset_root_.clear(); replace(foundation_demo());
}
void Application::open_project(const std::filesystem::path& path) {
    require_not_recording();
    auto next = persistence::load_project(path); replace(std::move(next));
    path_ = std::filesystem::absolute(path).lexically_normal(); asset_root_ = path_.parent_path(); unsaved_ = false;
}
void Application::import_wav(const std::filesystem::path& path) {
    require_not_recording();
    const auto asset = audio::open_wav(path); // validate before touching the current session
    auto d = persistence::demo_document(); d.project = Project{};
    d.project.id = new_id(); d.project.title = utf8(path.stem()); d.project.sample_rate = asset.sample_rate;
    const auto track = new_id();
    d.project.tracks = {{track,"Audio",TrackKind::audio,{}}};
    d.project.clips = {{new_id(),track,utf8(path.filename()),0,asset.frames(),0,utf8(std::filesystem::absolute(path))}};
    d.mixer = {{track,1,0,false,false}}; d.actions.clear(); d.live_notes.clear();
    replace(std::move(d)); path_.clear(); asset_root_.clear();
}
persistence::ProjectDocument Application::snapshot() const {
    auto result = document_; const auto p = services_.projects->state(); const auto g = graphs_->state();
    require(p.revision <= std::numeric_limits<std::uint64_t>::max()-result.generation,"generation overflow");
    result.generation += p.revision;
    require(g.revision <= std::numeric_limits<std::uint64_t>::max()-result.generation,"generation overflow");
    result.generation += g.revision; result.project = *p.project; result.graph = *g.graph;
    result.mixer.clear();
#ifdef MRS_HAS_ARA
    if(ara_)for(auto& binding:result.project.ara_bindings)if(ara_->status(binding.source)!="unbound"){auto cached=ara_->binding(binding.source);cached.bypass=binding.bypass;binding=std::move(cached);}
#endif
    for (const auto& t : result.project.tracks)
        result.mixer.push_back({t.id,t.mix.gain,t.mix.pan,t.mix.mute,t.mix.solo});
    for (auto& clip : result.project.clips) {
        if(clip.midi)continue;
        if (const auto found = owned_media_.find(clip.source); found != owned_media_.end())
            clip.source = media_ref(found->second.lexically_relative(asset_root_));
        else if (!asset_root_.empty()) {
            const auto source = std::filesystem::path(std::u8string(clip.source.begin(),clip.source.end()));
            const auto relative = source.lexically_normal().lexically_relative(asset_root_/"Media");
            if (source.is_absolute() && !relative.empty() && !relative.is_absolute() && *relative.begin() != "..")
                clip.source = media_ref(std::filesystem::path("Media")/relative);
        }
    }
    for(auto& binding:result.project.ara_bindings){
        const auto original=std::find_if(p.project->clips.begin(),p.project->clips.end(),[&](const auto& c){return !c.midi&&c.source==binding.source;});
        if(original!=p.project->clips.end())binding.source=result.project.clips[static_cast<std::size_t>(original-p.project->clips.begin())].source;
        else if(const auto owned=owned_media_.find(binding.source);owned!=owned_media_.end())binding.source=media_ref(owned->second.lexically_relative(asset_root_));
    }
    result.validate(); return result;
}
void Application::require_ai_owner() const {
    if(std::this_thread::get_id()!=ai_owner_)throw std::logic_error("AI API requires application thread");
}
ai::ContextKey Application::ai_key() const {
    require_ai_owner();const auto p=services_.projects->state();
    return {p.project->id,ai_session_,p.revision,graphs_->state().revision};
}
ai::ContextResult Application::ai_context(ai::Selection selection) const {
    require_ai_owner();if(!ai_permissions_.enabled)return {ai::Error::permission_denied,{}};
    require_ai_owner();const auto p=services_.projects->state();
    return ai::make_context(p,ai_key(),services_.transport->state(),std::move(selection));
}
ai::ViewResult Application::ai_read_view(ai::Selection selection) const {
    require_ai_owner();if(!ai_permissions_.enabled)return {ai::Error::permission_denied,{}};
    require_ai_owner();const auto project=services_.projects->state();const auto graph=graphs_->state();
    return ai::capture_view(project,graph,document_,ai_key(),services_.transport->state(),std::move(selection),
        {ai_permission_revision_,ai_permission_revision_->load(),ai_permissions_.stored_plugin_parameters},ai_ara_read_state());
}
ai::Page Application::ai_query(const ai::ContextKey& expected,const ai::Query& query,ai::Selection selection,ai::ReadPolicy policy) const {
    require_ai_owner();ai::Page error;error.kind=query.kind;error.key=expected;
    if(expected!=ai_key()){error.error=ai::Error::stale_context;return error;}
    const auto view=ai_read_view(std::move(selection));
    if(view.error!=ai::Error::none){error.error=view.error;return error;}
    return view.value->query(query,policy);
}
ai::SnapshotResult Application::ai_snapshot(const ai::ContextKey& expected,const std::vector<ai::Query>& queries,
    ai::Selection selection,ai::SnapshotBudget budget,ai::ReadPolicy policy) const {
    require_ai_owner();if(expected!=ai_key())return {ai::Error::stale_context,{}};
    const auto view=ai_read_view(std::move(selection));if(view.error!=ai::Error::none)return {view.error,{}};
    return view.value->snapshot(queries,budget,policy);
}
std::vector<Application::AnalysisSource> Application::ai_analysis_sources() const {
    require_ai_owner();std::vector<AnalysisSource> result;if(!ai_permissions_.enabled)return result;
    for(const auto& clip:services_.projects->state().project->clips)if(!clip.midi){auto at=assets_.find(clip.source);result.push_back({clip.id,clip.source_offset,audio_source_span(clip),at==assets_.end()?nullptr:at->second.data});if(result.size()==16)break;}
    return result;
}
ai::Submitted Application::request_ai(ai::Request request,ai::Task task) {
    require_ai_owner();
    if(!ai_permissions_.enabled || (request.snapshot && !request.snapshot->access_valid()))return {ai::Error::permission_denied,0};
    if(ai_receipts_.size()>=128)return {ai::Error::busy,0};
    if(request.context.version!=ai::contract_version)return {ai::Error::unsupported,0};
    if(request.context.key!=ai_key())return {ai::Error::stale_context,0};
    if(request.snapshot && request.snapshot->context.key!=ai_key())return {ai::Error::stale_context,0};
    // Only Application-authored domain data enters a job; caller supplies selection.
    auto context=ai_context(request.snapshot?request.snapshot->context.selection:std::move(request.context.selection));
    if(context.error!=ai::Error::none)return {context.error,0};
    request.context=request.snapshot?request.snapshot->context:std::move(*context.value);
    request.policy_revision=ai_permission_revision_->load();const auto key=request.context.key;const auto snapshot=request.snapshot;
    if(!ai_worker_)ai_worker_=std::make_unique<ai::Worker>();
    auto submitted=ai_worker_->submit(std::move(request),std::move(task));
    if(submitted.error==ai::Error::none)ai_receipts_.emplace(submitted.request_id,AiReceipt{key,ai_permission_revision_->load(),snapshot,snapshot?snapshot->context.selection:ai::Selection{}});
    return submitted;
}
bool Application::cancel_ai(std::uint64_t id) {
    require_ai_owner();if(!ai_worker_||!ai_worker_->cancel(id))return false;
    ai_receipts_.erase(id);std::erase_if(ai_operations_,[&](const auto& p){return p.second->call.request_id==id;});
    for(auto& p:ai_previews_)if(p->state_==ai::PreviewState::preview&&std::any_of(p->plans.begin(),p->plans.end(),[&](const auto& plan){return plan->call.request_id==id;}))p->state_=ai::PreviewState::rejected;
    return true;
}
std::optional<ai::Result> Application::take_ai_result() {
    require_ai_owner();if(!ai_worker_)return {};
    const auto key=ai_key();
    while(auto r=ai_worker_->take()) {
        if(r->key==key && r->policy_revision==ai_permission_revision_->load() && ai_permissions_.enabled)return r;
        // Drop late replies, including reopened projects with the same ID/revision.
    }
    return {};
}
void Application::set_ai_permissions(ai::PermissionPolicy policy){
    require_ai_owner();policy.validate();if(policy==ai_permissions_)return;
    require(ai_permission_revision_->load()<std::numeric_limits<std::uint64_t>::max(),"AI permission revision overflow");
    ai_permissions_=policy;++*ai_permission_revision_;if(ai_worker_)ai_worker_->cancel_all();
    ai_receipts_.clear();ai_operations_.clear();for(auto& preview:ai_previews_)if(preview->state_==ai::PreviewState::preview)preview->state_=ai::PreviewState::stale;ai_previews_.clear();
}
ai::RuntimeSafety Application::ai_safety() const {
    const auto state=engine_->state();return {state.playback==PlaybackState::playing,recording(),state.count_remaining!=0,workspace_==Workspace::live};
}
ai::Error Application::ai_receipt(const ai::ToolCall& call,const std::shared_ptr<const ai::Snapshot>& snapshot) const {
    if(!ai_permissions_.enabled || !snapshot || !snapshot->access_valid())return ai::Error::permission_denied;
    const auto at=ai_receipts_.find(call.request_id);
    if(at==ai_receipts_.end() || at->second.snapshot.lock()!=snapshot || at->second.policy!=ai_permission_revision_->load())return ai::Error::permission_denied;
    if(call.key!=snapshot->context.key || at->second.key!=call.key || call.key!=ai_key())return ai::Error::stale_context;
    if((ai::ara_action(call.tool)||(call.tool>=ai::ToolKind::list_ara_capabilities&&call.tool<=ai::ToolKind::get_ara_status))&&ai_selection_&&at->second.selection!=*ai_selection_)return ai::Error::stale_context;
    return ai::Error::none;
}
ai::PreparedTool Application::ai_propose(const ai::ToolCall& call,std::shared_ptr<const ai::Snapshot> snapshot){
    require_ai_owner();const auto error=ai_receipt(call,snapshot);if(error!=ai::Error::none)return {error,ai::Block::none,{}};
    const auto access=ai::authorize(ai_permissions_,call.tool,ai::Intent::suggest,ai_safety());if(!access.allowed())return {access.error,access.block,{}};
    if(const auto at=ai_operations_.find(call.operation_id);at!=ai_operations_.end()){
        if(at->second->call!=call)return {ai::Error::invalid_input,ai::Block::none,{}};
        return {ai::Error::none,ai::Block::none,at->second,true};
    }
    if(ai_operations_.size()>=128)return {ai::Error::busy,ai::Block::none,{}};
    auto result=ai::prepare_tool(call,services_.projects->state(),ai_key(),ai_permissions_,ai_permission_revision_->load(),ai_safety());
    if(result.value&&ai::ara_action(call.tool)){const auto valid=ai_ara_validate(call);if(valid!=ai::Error::none)return {valid,ai::Block::none,{}};}
    if(result.value)ai_operations_.emplace(call.operation_id,result.value);
    return result;
}
ai::Decision Application::ai_apply(const std::shared_ptr<const ai::ToolPlan>& plan){
    require_ai_owner();if(!plan)return {ai::Error::invalid_input,ai::Block::none};
    if(plan->policy_revision!=ai_permission_revision_->load())return {ai::Error::permission_denied,ai::Block::stale_policy};
    const auto at=ai_operations_.find(plan->call.operation_id);
    if(at==ai_operations_.end() || at->second!=plan)return {ai::Error::permission_denied,ai::Block::confirmation};
    if(plan->call.key!=ai_key())return {ai::Error::stale_context,ai::Block::none};
    // No model-controlled flags open these gates. Trusted preview/confirmation,
    // atomic Undo and MIDI application are subsequent #163/#164/#165 slices.
    return ai::authorize(ai_permissions_,plan->call.tool,ai::Intent::apply,ai_safety(),{});
}
ai::PreviewResult Application::build_ai_preview(const std::vector<std::shared_ptr<const ai::ToolPlan>>& plans){
    std::erase_if(ai_previews_,[](const auto& preview){return preview->state()!=ai::PreviewState::preview && preview.use_count()==1;});
    if(plans.empty()||plans.size()>8)return {ai::Error::invalid_input,{}};
    if(ai_previews_.size()>=8)return {ai::Error::busy,{}};
    if(std::any_of(plans.begin(),plans.end(),[](const auto& p){return p&&ai::ara_action(p->call.tool);}))return ai_ara_preview(plans);
    const auto before=services_.projects->state().project;std::shared_ptr<const ai::ProjectEdit> edit;std::vector<ai::Change> diff;std::size_t bytes{};
    std::set<std::pair<std::string,std::string>> fields;
    auto change=[&](Id id,std::string property,std::string oldValue,std::string newValue,std::string scope){if(!fields.emplace(id.value,property).second)return;bytes+=id.value.size()+property.size()+oldValue.size()+newValue.size()+scope.size();if(bytes>65536||diff.size()>=1024)throw std::runtime_error("AI preview exceeds diff budget");diff.push_back({std::move(id),std::move(property),std::move(oldValue),std::move(newValue),std::move(scope)});};
    try{std::vector<std::shared_ptr<const ICommand>> commands;for(const auto& plan:plans){if(!plan||!plan->command_)return {ai::Error::unsupported,{}};commands.push_back(plan->command_);}edit=std::make_shared<const ai::ProjectEdit>(before,commands);const auto& after=edit->after();
        for(const auto& marker:after->markers){const auto old=std::find_if(before->markers.begin(),before->markers.end(),[&](const auto& value){return value.id==marker.id;});
            const auto scope="Marker at tick "+std::to_string(marker.tick);
            if(old==before->markers.end()){change(marker.id,"marker.name","(absent)",marker.name,scope);change(marker.id,"marker.tick","(absent)",std::to_string(marker.tick),scope);}
            else if(old->name!=marker.name)change(marker.id,"marker.name",old->name,marker.name,scope);}
        for(const auto& plan:plans)if(plan->call.tool==ai::ToolKind::transpose_notes||plan->call.tool==ai::ToolKind::set_note_velocity){const auto& args=std::get<ai::MidiArgs>(plan->call.arguments);
            const auto oldClip=std::find_if(before->clips.begin(),before->clips.end(),[&](const auto& clip){return clip.id==args.clip;});const auto newClip=std::find_if(after->clips.begin(),after->clips.end(),[&](const auto& clip){return clip.id==args.clip;});
            for(const auto& id:args.notes){const auto old=std::find_if(oldClip->midi->notes.begin(),oldClip->midi->notes.end(),[&](const auto& note){return note.id==id;});const auto value=std::find_if(newClip->midi->notes.begin(),newClip->midi->notes.end(),[&](const auto& note){return note.id==id;});
                const auto first=std::max(old->start,oldClip->midi->source_offset),last=std::min(old->start+old->length,oldClip->midi->source_offset+oldClip->midi->length);
                const auto tick=oldClip->midi->start+first-oldClip->midi->source_offset;
                const auto scope="Clip "+oldClip->name+", note "+id.value+", visible ticks ["+std::to_string(tick)+", "+std::to_string(tick+last-first)+"), channel "+std::to_string(old->channel+1);
                if(old->pitch!=value->pitch)change(id,"note.pitch",std::to_string(old->pitch),std::to_string(value->pitch),scope);
                if(old->velocity!=value->velocity)change(id,"note.velocity",std::to_string(old->velocity),std::to_string(value->velocity),scope);}}
        if(std::any_of(plans.begin(),plans.end(),[](const auto& plan){return plan->call.tool==ai::ToolKind::midi_draft;})){
            auto noteValue=[](const auto& n){return "pitch "+std::to_string(n.pitch)+", velocity "+std::to_string(n.velocity)+", channel "+std::to_string(n.channel+1)+", source start "+std::to_string(n.start)+", duration "+std::to_string(n.length);};
            auto eventValue=[](const auto& e){return "CC "+std::to_string(e.data1)+", value "+std::to_string(e.data2)+", channel "+std::to_string(e.channel+1)+", source tick "+std::to_string(e.start);};
            for(const auto& clip:after->clips)if(clip.midi){const auto old=std::find_if(before->clips.begin(),before->clips.end(),[&](const auto& c){return c.id==clip.id;});if(old!=before->clips.end()&&*old==clip)continue;const MidiClip* previous=old==before->clips.end()?nullptr:&*old->midi;
                std::map<std::string,const MidiNote*> oldNotes,newNotes;std::map<std::string,const MidiChannelEvent*> oldEvents,newEvents;if(previous){for(const auto& n:previous->notes)oldNotes.emplace(n.id.value,&n);for(const auto& e:previous->events)oldEvents.emplace(e.id.value,&e);}for(const auto& n:clip.midi->notes)newNotes.emplace(n.id.value,&n);for(const auto& e:clip.midi->events)newEvents.emplace(e.id.value,&e);
                auto scope="MIDI clip "+clip.name+", target "+clip.id.value+", project ticks ["+std::to_string(clip.midi->start)+", "+std::to_string(clip.midi->start+clip.midi->length)+")";
                for(const auto& plan:plans)if(plan->call.tool==ai::ToolKind::midi_draft){const auto& a=std::get<ai::MidiDraftArgs>(plan->call.arguments);if(a.clip==clip.id||(!a.clip&&a.track==clip.track&&a.start==clip.midi->start))scope+="; "+std::string(a.mode==ai::MidiDraftMode::insert?"INSERT":a.mode==ai::MidiDraftMode::replace?"REPLACE":"EDIT")+" confirmed range ["+std::to_string(a.start)+", "+std::to_string(a.end)+")";}
                if(!previous)change(clip.id,"midi.clip","(absent)","Create on track "+clip.track.value,scope);
                for(const auto& n:clip.midi->notes){const auto at=oldNotes.find(n.id.value);if(at==oldNotes.end())change(n.id,"midi.note","(absent)",noteValue(n),scope);else if(*at->second!=n)change(n.id,"midi.note",noteValue(*at->second),noteValue(n),scope);}
                if(previous)for(const auto& n:previous->notes)if(!newNotes.contains(n.id.value))change(n.id,"midi.note",noteValue(n),"(removed)",scope);
                for(const auto& e:clip.midi->events){const auto at=oldEvents.find(e.id.value);if(at==oldEvents.end())change(e.id,"midi.cc","(absent)",eventValue(e),scope);else if(*at->second!=e)change(e.id,"midi.cc",eventValue(*at->second),eventValue(e),scope);}
                if(previous)for(const auto& e:previous->events)if(!newEvents.contains(e.id.value))change(e.id,"midi.cc",eventValue(e),"(removed)",scope);
            }
        }
    }catch(...){return {ai::Error::invalid_input,{}};}
    if(std::any_of(plans.begin(),plans.end(),[](const auto& plan){return plan->call.tool==ai::ToolKind::midi_draft||plan->call.tool==ai::ToolKind::transpose_notes||plan->call.tool==ai::ToolKind::set_note_velocity;}))try{const auto& candidate=*edit->after();require(candidate.clips.size()<=audio::max_voices,"Too many playback clips");std::vector<Id> tracks;for(const auto& t:candidate.tracks)if(t.kind!=TrackKind::midi)tracks.push_back(t.id);(void)audio::compile_midi_clips(candidate,tracks);(void)audio::compile_midi_events(candidate,tracks);}catch(...){return {ai::Error::invalid_input,{}};}
    auto result=std::shared_ptr<ai::Preview>(new ai::Preview(ai_key(),ai_permission_revision_->load(),std::move(diff),plans,std::move(edit)));ai_previews_.push_back(result);return {ai::Error::none,std::move(result)};
}
ai::PreviewResult Application::ai_preview(const std::vector<std::shared_ptr<const ai::ToolPlan>>& plans){
    require_ai_owner();std::set<std::string> unique;for(const auto& plan:plans){if(!plan||!unique.insert(plan->call.operation_id).second)return {ai::Error::invalid_input,{}};
        const auto at=ai_operations_.find(plan->call.operation_id);if(at==ai_operations_.end()||at->second!=plan||plan->policy_revision!=ai_permission_revision_->load())return {ai::Error::permission_denied,{}};
        if(plan->call.key!=ai_key())return {ai::Error::stale_context,{}};
        const auto decision=ai::authorize(ai_permissions_,plan->call.tool,ai::Intent::suggest,ai_safety());if(!decision.allowed())return {decision.error,{}};}
    return build_ai_preview(plans);
}
ai::Decision Application::ai_preview_status(const std::shared_ptr<ai::Preview>& preview){
    require_ai_owner();if(!preview||std::find(ai_previews_.begin(),ai_previews_.end(),preview)==ai_previews_.end())return {ai::Error::permission_denied,ai::Block::preview};
    if(preview->policy_revision!=ai_permission_revision_->load()||preview->key!=ai_key()){if(preview->state_==ai::PreviewState::preview)preview->state_=ai::PreviewState::stale;return {ai::Error::stale_context,ai::Block::stale_policy};}
    if(preview->state_!=ai::PreviewState::preview||preview->changes.empty())return {ai::Error::invalid_input,ai::Block::preview};
    for(const auto& plan:preview->plans){if(ai_applied_operations_.contains({plan->call.request_id,plan->call.operation_id}))return {ai::Error::invalid_input,ai::Block::undo};auto decision=ai::authorize(ai_permissions_,plan->call.tool,ai::Intent::apply,ai_safety(),{true,true,true,true});if(!decision.allowed())return decision;
        if(ai::ara_action(plan->call.tool)){const auto valid=ai_ara_validate(plan->call);if(valid!=ai::Error::none){if(valid==ai::Error::stale_context)preview->state_=ai::PreviewState::stale;return {valid,ai::Block::none};}}
        if(plan->call.tool!=ai::ToolKind::add_marker&&plan->call.tool!=ai::ToolKind::rename_marker){const auto head=engine_->state();if(head.playback==PlaybackState::playing||head.loop||head.count_remaining)return {ai::Error::permission_denied,ai::Block::midi_playback};}}
    return {};
}
ai::PreviewResult Application::ai_rebuild_preview(const std::shared_ptr<ai::Preview>& preview){
    require_ai_owner();if(!preview||std::find(ai_previews_.begin(),ai_previews_.end(),preview)==ai_previews_.end())return {ai::Error::permission_denied,{}};
    if(preview->state_==ai::PreviewState::accepted||preview->state_==ai::PreviewState::rejected)return {ai::Error::invalid_input,{}};
    if(preview->policy_revision!=ai_permission_revision_->load()||preview->key.project!=ai_key().project||preview->key.session!=ai_key().session)return {ai::Error::permission_denied,{}};
    std::vector<std::shared_ptr<const ai::ToolPlan>> refreshed;for(const auto& plan:preview->plans){if(ai_applied_operations_.contains({plan->call.request_id,plan->call.operation_id}))return {ai::Error::invalid_input,{}};auto call=plan->call;call.key=ai_key();auto result=ai::prepare_tool(call,services_.projects->state(),ai_key(),ai_permissions_,ai_permission_revision_->load(),ai_safety());if(!result.value)return {result.error,{}};refreshed.push_back(result.value);}
    preview->state_=ai::PreviewState::stale;return build_ai_preview(refreshed);
}
void Application::ai_reject_preview(const std::shared_ptr<ai::Preview>& preview){require_ai_owner();if(preview&&std::find(ai_previews_.begin(),ai_previews_.end(),preview)!=ai_previews_.end()&&preview->state_!=ai::PreviewState::accepted)preview->state_=ai::PreviewState::rejected;}
ai::Decision Application::ai_accept_preview(const std::shared_ptr<ai::Preview>& preview){
    const auto decision=ai_preview_status(preview);if(!decision.allowed())return decision;
    try{if(ai_applied_operations_.size()+preview->plans.size()>1024)return {ai::Error::busy,ai::Block::undo};auto applied=ai_applied_operations_;for(const auto& plan:preview->plans)applied.emplace(plan->call.request_id,plan->call.operation_id);
        const bool midi=std::any_of(preview->plans.begin(),preview->plans.end(),[](const auto& p){return p->call.tool==ai::ToolKind::transpose_notes||p->call.tool==ai::ToolKind::set_note_velocity||p->call.tool==ai::ToolKind::midi_draft;});
        if(preview->plans.size()==1&&ai::ara_action(preview->plans.front()->call.tool))ai_ara_execute(preview->plans.front()->call);
        else if(midi)edit_timing(*preview->edit_->after(),preview->edit_.get(),{},true);else edit_metadata(*preview->edit_,true);
        ai_applied_operations_.swap(applied);preview->state_=ai::PreviewState::accepted;return {};}catch(...){preview->state_=ai::PreviewState::failed;return {ai::Error::backend_failure,ai::Block::none};}
}
ai::Page Application::ai_tool_query(const ai::ToolCall& call,std::shared_ptr<const ai::Snapshot> snapshot) const {
    require_ai_owner();ai::Page error;error.key=call.key;
    const auto receipt=ai_receipt(call,snapshot);if(receipt!=ai::Error::none){error.error=receipt;return error;}
    if(call.version!=1 || !call.request_id || call.operation_id.empty() || call.operation_id.size()>128 || !std::holds_alternative<ai::Query>(call.arguments)){error.error=ai::Error::invalid_input;return error;}
    const auto access=ai::authorize(ai_permissions_,call.tool,ai::Intent::read,ai_safety());if(!access.allowed()){error.error=access.error;return error;}
    const auto kind=std::get<ai::Query>(call.arguments).kind;
    if((call.tool==ai::ToolKind::list_ara_capabilities&&kind!=ai::QueryKind::ara_capabilities)||(call.tool==ai::ToolKind::query_ara_binding&&kind!=ai::QueryKind::ara_binding)||(call.tool==ai::ToolKind::get_ara_status&&kind!=ai::QueryKind::ara_status)){error.error=ai::Error::invalid_input;return error;}
    return ai_query(call.key,std::get<ai::Query>(call.arguments),snapshot->context.selection,{ai_permissions_.stored_plugin_parameters});
}
void Application::save_project(const std::filesystem::path& requested) {
    poll_reverbs();
    require_not_recording();
    const auto path = std::filesystem::absolute(requested).lexically_normal();
    require(!path.filename().empty(),"choose a project filename");
#ifdef MRS_HAS_ARA
    if(ara_){if(ara_->poll())unsaved_=true;(void)ara_->save_bindings();}
#endif
    auto document = snapshot();
    capture_insert_state(document.project);
    // Validate identity BEFORE adding content to an existing project directory.
    if (std::filesystem::exists(path)) {
        const auto previous = persistence::load_project(path);
        require(previous.project.id == document.project.id && previous.generation <= document.generation,"project identity/generation mismatch");
    }
    const auto root = path.parent_path();
    const bool relocating = !asset_root_.empty() && root != asset_root_;
    std::set<std::string> keys, current;
    for (const auto& clip : services_.projects->state().project->clips) if (!clip.midi && clip.source != "mrs:demo-tone") { keys.insert(clip.source); current.insert(clip.source); }
    for(const auto& b:services_.projects->state().project->ara_bindings)if(b.source!="mrs:demo-tone")keys.insert(b.source);
    for (const auto& [key,value] : assets_) { (void)value; if (key != "mrs:demo-tone") keys.insert(key); }
    const auto resolve = [&](const std::string& key) {
        if (const auto found = owned_media_.find(key); found != owned_media_.end()) return found->second;
        auto source = std::filesystem::path(std::u8string(key.begin(),key.end()));
        return source.is_absolute() ? source : asset_root_/source;
    };
    bool needs_copy = relocating;
    for (const auto& key : keys) {
        const auto relative = resolve(key).lexically_normal().lexically_relative(root/"Media");
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") needs_copy = true;
    }
    if (needs_copy) require_not_playing(); // media/Save As rebuilds need quiescent callbacks
    MediaCopy copies(root);
    if (relocating) { copies.content(asset_root_,"Media"); copies.content(asset_root_,"Mixdown"); }
    std::map<std::string,std::filesystem::path> owned;
    for (const auto& key : keys) {
        const auto source = resolve(key);
        const bool ara_source=std::any_of(services_.projects->state().project->ara_bindings.begin(),services_.projects->state().project->ara_bindings.end(),[&](const auto& b){return b.source==key;});
        if (!std::filesystem::exists(source)&&(!current.contains(key)||ara_source)) continue; // preserve missing ARA/Undo references
        owned[key] = copies.media(source);
    }
    for (std::size_t i=0; i<document.project.clips.size(); ++i) {
        const auto& original = services_.projects->state().project->clips[i].source;
        if (!document.project.clips[i].midi && original != "mrs:demo-tone") {
            const auto at=owned.find(original);document.project.clips[i].source=at!=owned.end()?media_ref(at->second.lexically_relative(root)):utf8(resolve(original));
        }
    }
    for(auto& b:document.project.ara_bindings){const auto& bindings=services_.projects->state().project->ara_bindings;const auto original=std::find_if(bindings.begin(),bindings.end(),[&](const auto& item){return item.persistent_id==b.persistent_id;});
        if(original!=bindings.end()){if(original->source=="mrs:demo-tone"){b.source=original->source;continue;}const auto at=owned.find(original->source);b.source=at!=owned.end()?media_ref(at->second.lexically_relative(root)):utf8(resolve(original->source));}}
    document.validate();
    if (copies.copied()) require_not_playing();
    persistence::save_project(path,document); copies.commit();
    path_ = path; asset_root_ = root; owned_media_ = std::move(owned);
    // Runtime keys/Undo are stable; move only their physical disk backing.
    if (copies.copied()) {
        for (auto& [key,cached] : assets_) if (cached.data->file) {
            const auto found = owned_media_.find(key); if (found == owned_media_.end()) continue;
            auto data = *cached.data; auto file = std::make_shared<audio::WavFile>(*data.file); file->path = found->second; data.file = std::move(file);
            *cached.cancel = true; cached.pending = {}; // cancelled worker stops before the old source can be retired
            cached.cancel = std::make_shared<std::atomic<bool>>(false);
            cached.data = std::make_shared<const audio::AudioData>(std::move(data));
#ifdef MRS_HAS_ARA
            if(ara_)ara_->relocate_source(key,cached.data);
#endif
            if (!cached.peaks) cached.pending = std::async(std::launch::async,[data=cached.data,cancel=cached.cancel] { return audio::Waveform(*data,cancel); });
        }
        const bool retain=insert_runtime_.contains(std::string{})&&std::all_of(document.project.tracks.begin(),document.project.tracks.end(),[&](const auto& t){return t.kind==TrackKind::midi||insert_runtime_.contains(t.id.value);});
        rebuild_audio(retain); // retain prepared processors; bootstrap an empty offline graph if needed
    }
    saved_project_revision_ = services_.projects->state().revision;
    saved_graph_revision_ = graphs_->state().revision; unsaved_ = false;
}
bool Application::dirty() const {
    return unsaved_ || services_.projects->state().revision != saved_project_revision_ || graphs_->state().revision != saved_graph_revision_;
}
void Application::rename_track(const Id& id, std::string name) {
    require_not_recording();
    require(!name.empty() && name.size() <= 4096,"enter a track name");
    services_.projects->execute(RenameTrack{id,std::move(name)});
}
void Application::require_not_recording() const {
    require(!recording(),"End recording before changing project, files or device");
}
void Application::sync_arm() {
    const auto p=services_.projects->state().project;
    std::erase_if(armed_tracks_,[&](const auto& id) { return std::none_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t) { return t.id == id && (t.kind == TrackKind::audio||t.kind==TrackKind::instrument); }); });
    armed_=armed_tracks_.empty() ? std::nullopt : std::optional<Id>{armed_tracks_.front()};
}
void Application::require_not_playing() const {
    require_not_recording();
    require(engine_->state().playback != PlaybackState::playing,"Pause or stop playback before changing tracks or importing audio");
}
void Application::new_project(std::uint32_t rate, std::string title) {
    require_not_recording();
    auto d = foundation_demo(); d.project.id = new_id(); d.project.title = std::move(title);
    d.project.sample_rate = rate; d.project.time = TimeMap{}; d.project.tracks.clear(); d.project.clips.clear();
    d.project.chords.clear(); d.project.sections.clear(); d.project.markers.clear(); d.mixer.clear(); d.live_notes.clear();
    d.validate(); replace(std::move(d)); path_.clear(); asset_root_.clear();
}
void Application::cache_asset(std::string source, std::shared_ptr<const audio::AudioData> data) {
    if (assets_.contains(source)) return;
    require(assets_.size() < 128,"retained source limit reached (128); start a new project to release Undo media");
    std::size_t bytes = data->samples.size()*sizeof(float);
    for (const auto& [key,cached] : assets_) { (void)key; bytes += cached.data->samples.size()*sizeof(float); }
    require(bytes <= 512*1024*1024,"project preload/cache exceeds 512 MiB");
    CachedAsset value; value.data = data;
    value.pending = std::async(std::launch::async,[data,cancel = value.cancel] { return audio::Waveform(*data,cancel); });
    assets_.emplace(std::move(source),std::move(value));
}
std::shared_ptr<const audio::AudioData> Application::asset(const std::string& source) {
    if (const auto it = assets_.find(source); it != assets_.end()) return it->second.data;
    audio::AudioData data;
    if (source == "mrs:demo-tone") data = audio::sine_fixture(document_.project.sample_rate,2,32*static_cast<Sample>(document_.project.sample_rate),220);
    else {
        require(!source.empty(),"clip has no audio source");
        auto path = std::filesystem::path(std::u8string(source.begin(),source.end()));
        if (const auto owned = owned_media_.find(source); owned != owned_media_.end()) path = owned->second;
        else if (path.is_relative()) path = asset_root_ / path;
        data = audio::open_wav(path);
    }
    auto ptr = std::make_shared<const audio::AudioData>(std::move(data)); cache_asset(source,ptr); return ptr;
}
void Application::prepare_waveforms() {
    const auto project=services_.projects->state().project;
    for (const auto& clip : project->clips) if(!clip.midi)try{(void)asset(clip.source);}catch(const std::exception& e){if(std::none_of(project->ara_bindings.begin(),project->ara_bindings.end(),[&](const auto& b){return b.source==clip.source;}))throw;waveform_error_=e.what();}
}
const audio::Waveform* Application::waveform(std::string_view source) const {
    const auto it = assets_.find(std::string(source));
    return it != assets_.end() && it->second.peaks ? &*it->second.peaks : nullptr;
}
namespace {
std::vector<int> track_inputs(const Track& track, const std::vector<int>& defaults) {
    if (track.input == -1) return {};
    if (track.input == -2) return defaults.empty() ? std::vector<int>{} : std::vector<int>{defaults.front()};
    return track.input_stereo ? std::vector<int>{track.input,track.input+1} : std::vector<int>{track.input};
}
}
std::vector<int> Application::selected_inputs(const Project& p) const { return selected_inputs(p,default_inputs_); }
std::vector<int> Application::selected_inputs(const Project& p, const std::vector<int>& defaults) const {
    std::vector<int> result; bool assigned{};
    for (const auto& track : p.tracks) if (track.kind == TrackKind::audio && (track_armed(track.id) || track.input_monitor)) {
        assigned=true; for (const auto c : track_inputs(track,defaults)) if (std::find(result.begin(),result.end(),c) == result.end()) result.push_back(c);
    }
    return assigned ? result : defaults;
}
bool Application::track_armed(const Id& id) const { return std::find(armed_tracks_.begin(),armed_tracks_.end(),id) != armed_tracks_.end(); }
bool Application::stereo_track(const Id& id) const {
    const auto p=services_.projects->state().project;
    for (const auto& t : p->tracks) if (t.id == id && (t.input_stereo || t.kind == TrackKind::bus || t.kind == TrackKind::instrument)) return true;
    for (const auto& clip : p->clips) if (clip.track == id) { const auto found=assets_.find(clip.source); if (found != assets_.end() && found->second.data->channels > 1) return true; }
    return false;
}
std::vector<std::string> Application::input_names() { return device_info_ ? device_info_->inputs : std::vector<std::string>{}; }
std::vector<std::string> Application::output_names() const { return device_info_ ? device_info_->outputs : std::vector<std::string>{}; }
std::vector<int> Application::active_outputs() const { return device_config_ ? device_config_->outputs : std::vector<int>{}; }
void Application::validate_hardware(const Project& p, const audio::DeviceConfig& c) const {
    const auto route = [&](const std::vector<int>& outputs, const std::string& owner) {
        for (const auto channel : outputs) if (std::find(c.outputs.begin(),c.outputs.end(),channel) == c.outputs.end())
            throw std::invalid_argument(owner+": physical output "+std::to_string(channel+1)+" is not active; enable it in Audio settings");
    };
    route(p.master_outputs,"Master"); for (const auto& track : p.tracks) route(track.hardware_outputs,track.name);
}
void Application::set_hardware_output(std::optional<Id> track, std::vector<int> outputs) { edit(SetHardwareOutput{std::move(track),std::move(outputs)}); }
namespace {
bool insert_order_only(Project before,const Project& after){
    const auto permutation=[](const auto& a,const auto& b){return a.size()==b.size()&&std::all_of(a.begin(),a.end(),[&](const auto& fx){return std::find(b.begin(),b.end(),fx)!=b.end();});};
    if(!permutation(before.master_inserts,after.master_inserts)||before.tracks.size()!=after.tracks.size())return false;
    before.master_inserts=after.master_inserts;
    for(std::size_t i=0;i<before.tracks.size();++i){if(!permutation(before.tracks[i].inserts,after.tracks[i].inserts))return false;before.tracks[i].inserts=after.tracks[i].inserts;}
    return before==after;
}
bool parameter_only(const std::vector<NativeInsert>& before,const std::vector<NativeInsert>& after){
    if(before.size()!=after.size())return false;
    for(std::size_t i=0;i<before.size();++i){auto a=before[i],b=after[i];a.gain=b.gain;a.frequency=b.frequency;a.q=b.q;a.bands=b.bands;a.ir.mix=b.ir.mix;a.ir.low_cut=b.ir.low_cut;a.ir.high_cut=b.ir.high_cut;a.ir.invert=b.ir.invert;
        // The native-only bounded rack is preprepared inside the Rig processor.
        // Its complete ordered snapshot travels through the host parameter batch.
        if(a.kind==InsertKind::rig)a.rig=b.rig;
        a.preset_name=b.preset_name; // Host provenance never changes processor structure.
        if(wav_core(a.kind)){a.cab_metadata=b.cab_metadata;a.cab_collection_id=b.cab_collection_id;a.cab_variant_id=b.cab_variant_id;}
        a.reverb.pre_delay=b.reverb.pre_delay;a.reverb.tone=b.reverb.tone;a.reverb.mix=b.reverb.mix;
        a.reverb.gate_enabled=b.reverb.gate_enabled;a.reverb.gate_threshold=b.reverb.gate_threshold;
        // Native EQ/gain/filter parameters and bypass may change without rebuilding.
        a.bypass=b.bypass;if(a.kind==InsertKind::vst3)a.parameters=b.parameters;
        if(a!=b)return false;
    }return true;
}
const std::vector<NativeInsert>& insert_chain(const Project& p,const std::optional<Id>& track){if(!track)return p.master_inserts;for(const auto& t:p.tracks)if(t.id==track)return t.inserts;throw std::invalid_argument("unknown insert track");}
bool insert_parameters_only(Project before,const Project& after){
    if(!parameter_only(before.master_inserts,after.master_inserts)||before.tracks.size()!=after.tracks.size())return false;
    before.master_inserts=after.master_inserts;for(std::size_t i=0;i<before.tracks.size();++i){if(!parameter_only(before.tracks[i].inserts,after.tracks[i].inserts))return false;before.tracks[i].inserts=after.tracks[i].inserts;}
    return before==after;
}
}
void Application::publish_inserts(){for(auto& [key,r]:insert_runtime_){(void)key;if(r.pending && r.graph && r.graph->enqueue_parameters(*r.pending))r.pending.reset();}}
void Application::stop_reverb_loaders(){for(auto& r:reverb_runtime_)r.loader->shutdown();reverb_runtime_.clear();}
void Application::set_cab_root(std::filesystem::path root,std::string manifest){
    cab_root_=std::move(root);cab_library_->scan(cab_root_,"cab-user-library",manifest.empty()?*cab_library_->snapshot():audio::deserialize_cab_manifest(manifest));
    const auto project=services_.projects->state().project;
    for(auto& r:reverb_runtime_)for(const auto& fx:insert_chain(*project,r.track))if(fx.id==r.slot&&wav_core(fx.kind)){r.loader->cancel();if(!fx.reverb.first.digest.empty())r.loader->restore(fx.reverb,cab_root_);}
}
void Application::map_cab_assets(std::optional<Id> track,const Id& slot,const std::vector<std::string>& paths,const std::map<std::string,std::string>& metadata,std::string suffix){
    auto catalog=*cab_library_->snapshot();require(!catalog.collection_id.empty()&&!catalog.root.empty(),"Choose a scanned collection first");
    require(catalog.root==std::filesystem::absolute(cab_root_),"Collection changed; wait for its scan before mapping");
    audio::map_cab_assets(catalog,paths,metadata,suffix);
    auto effects=insert_chain(*services_.projects->state().project,track);
    auto fx=std::find_if(effects.begin(),effects.end(),[&](const auto& f){return f.id==slot&&wav_core(f.kind);});
    require(fx!=effects.end(),"Unknown MR Cab insert");
    auto relative=fx->reverb.first.relative;
    if(auto loader=reverb_loader(track,slot)){const auto s=loader->snapshot();if(s->active){const auto p=s->active->first.relative.generic_u8string();relative={p.begin(),p.end()};}}
    if(std::find(paths.begin(),paths.end(),relative)!=paths.end())for(const auto& a:catalog.assets)if(a.relative==relative&&a.digest==fx->reverb.first.digest){
        fx->cab_metadata=audio::cab_asset_metadata(a);fx->cab_variant_id=a.variant_id;fx->cab_collection_id=catalog.collection_id;
        fx->validate();break;
    }
    cab_library_->publish_mapping(std::move(catalog));
    if(effects!=insert_chain(*services_.projects->state().project,track)){set_inserts(track,std::move(effects));unsaved_=true;}
}
void Application::set_reverb_root(std::filesystem::path root){
    if(reverb_library_->snapshot()->root!=root)reverb_library_->set_root(root);
    const auto project=services_.projects->state().project;
    for(auto& r:reverb_runtime_){const auto status=r.loader->snapshot();if(!status->active){
        for(const auto& fx:insert_chain(*project,r.track))if(fx.id==r.slot&&fx.kind==InsertKind::reverb&&!fx.reverb.first.digest.empty())r.loader->restore(fx.reverb,root);}}
}
std::shared_ptr<audio::ReverbLoader> Application::reverb_loader(std::optional<Id> track,const Id& slot,bool ensure){
    if(ensure&&!device_)start_empty_clock(true);
    for(auto& r:reverb_runtime_)if(r.track==track&&r.slot==slot)return r.loader;
    if(ensure){require_not_playing();rebuild_audio();for(auto& r:reverb_runtime_)if(r.track==track&&r.slot==slot)return r.loader;}
    if(ensure)throw std::invalid_argument("MR Reverb instance is not prepared");
    return {};
}
void Application::poll_reverbs(){
    for(auto& r:reverb_runtime_){const auto status=r.loader->snapshot();
        if(r.tone&&(status->revision!=r.tone->revision||status->phase==audio::ReverbLoadPhase::error||status->phase==audio::ReverbLoadPhase::missing))r.tone.reset();
        // A newer request can already be Loading while the previous completed
        // kernel still needs acknowledgement with transport stopped.
        if(!status->active||status->active_revision==r.observed)continue;
        if(device_&&!recording()&&engine_->state().playback!=PlaybackState::playing){device_->stop();try{r.loader->activate_quiescent();device_->start();}catch(...){device_->start();throw;}}
        r.observed=status->active_revision;
        auto effects=insert_chain(*services_.projects->state().project,r.track);
        auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& fx){return fx.id==r.slot;});if(at==effects.end())continue;
        auto selected=audio::reverb_selection_state(*status->active,at->reverb);
        // Restoring the same verified content may resolve a new root. It must not
        // add a history entry or overwrite the durable original fallback paths.
        const bool tone=r.tone&&r.tone->revision==status->active_revision;
        if(!tone&&selected.first.digest==at->reverb.first.digest&&selected.second.digest==at->reverb.second.digest)continue;
        const auto before=*at;
        const auto rigPreset=tone?r.tone->rig:std::nullopt;
        if(rigPreset){const auto id=at->id;*at=*rigPreset;at->id=id;}
        if(tone){at->cab_metadata=r.tone->metadata;if(!rigPreset){at->cab_collection_id="tone-library";at->cab_variant_id=r.tone->asset;}r.tone.reset();}
        else if(wav_core(at->kind)){
            const auto catalog=cab_library_->snapshot();at->cab_metadata={{"ir_role","unknown"}};
            const auto p=status->active->first.relative.generic_u8string();const std::string relative(p.begin(),p.end());
            for(const auto& a:catalog->assets)if(a.relative==relative&&a.digest==selected.first.digest){at->cab_collection_id=catalog->collection_id;at->cab_variant_id=a.variant_id;at->cab_metadata=audio::cab_asset_metadata(a);break;}
        }
        at->reverb=rigPreset?rigPreset->reverb:std::move(selected);if(*at!=before){
            if(rigPreset){auto runtime=insert_runtime_.find(r.track?r.track->value:std::string{});if(runtime!=insert_runtime_.end())runtime->second.pending=processing::insert_graph(effects);}
            services_.projects->execute(SetInserts{r.track,std::move(effects)});publish_inserts();unsaved_=true;}
    }
}
bool Application::model_load_pending(std::optional<Id> track,const Id& slot)const{for(const auto& runtime:reverb_runtime_)if(runtime.track==track&&runtime.slot==slot)return runtime.tone.has_value();return false;}
std::array<float,2> Application::rig_levels(std::optional<Id> track,const Id& slot)const{
    const auto at=insert_runtime_.find(track?track->value:std::string{});return at!=insert_runtime_.end()&&at->second.graph?at->second.graph->reverb_levels(slot):std::array<float,2>{};
}
void Application::select_rig_preset(std::optional<Id> track,const Id& slot,const RigPreset& preset){
    preset.validate();const auto chain=insert_chain(*services_.projects->state().project,track);
    const auto target=std::find_if(chain.begin(),chain.end(),[&](const auto& fx){return fx.id==slot&&fx.kind==InsertKind::rig;});require(target!=chain.end(),"Rig insert no longer exists");
    auto runtime=std::find_if(reverb_runtime_.begin(),reverb_runtime_.end(),[&](const auto& r){return r.track==track&&r.slot==slot;});require(runtime!=reverb_runtime_.end(),"Rig is not prepared");
    const auto& fx=preset.state;runtime->loader->restore(fx.reverb,cab_root_);
    const auto value=[&](const char* key){const auto at=fx.cab_metadata.find(key);return at==fx.cab_metadata.end()?std::string{}:at->second;};
    runtime->tone=ToneRequest{runtime->loader->snapshot()->revision,fx.cab_metadata,fx.cab_variant_id,value("tone_model_id"),fx};
}
void Application::select_tone_model(std::optional<Id> track,const Id& slot,const audio::ToneModel& model,const audio::SourceAsset& source){
    require(model.kind==audio::ToneKind::wav_ir,"NAM runtime is not available");
    require(std::find(model.sources.begin(),model.sources.end(),source.id)!=model.sources.end(),"Source does not belong to model");
    auto selectedModel=model;selectedModel.sources={source.id};audio::ToneLibrary{{source},{selectedModel}}.validate();
    auto at=std::find_if(reverb_runtime_.begin(),reverb_runtime_.end(),[&](const auto& r){return r.track==track&&r.slot==slot;});
    require(at!=reverb_runtime_.end(),"Model insert is not prepared");
    const auto chain=insert_chain(*services_.projects->state().project,track);const auto fx=std::find_if(chain.begin(),chain.end(),[&](const auto& n){return n.id==slot;});require(fx!=chain.end()&&wav_core(fx->kind),"Model selection requires WAV core");
    auto role=model.role=="cabinet_only"?"cabinet":model.role;
    auto metadata=std::map<std::string,std::string>{{"ir_role",role},{"tone_model_id",model.id},{"tone_model_title",model.title},{"source_asset_id",source.id},{"amp_make",model.maker},{"capture_variant",model.variant},{"provenance",model.provenance},{"capture_notes",model.notes}};
    validate_cab_metadata(metadata);at->loader->restore(audio::tone_source_state(source,model.title),std::filesystem::path(std::u8string(source.root.begin(),source.root.end())));
    at->tone=ToneRequest{at->loader->snapshot()->revision,std::move(metadata),source.id,model.id,std::nullopt};
}
void Application::preview_inserts(std::optional<Id> track,const std::vector<NativeInsert>& inserts){
    const auto p=services_.projects->state().project;require(parameter_only(insert_chain(*p,track),inserts),"Pause/Stop to change insert structure");
    const auto found=insert_runtime_.find(track?track->value:std::string{});if(found!=insert_runtime_.end()){found->second.pending=processing::insert_graph(inserts);publish_inserts();}
}
void Application::cancel_insert_preview(){const auto p=services_.projects->state().project;for(auto& [key,r]:insert_runtime_){r.pending=processing::insert_graph(insert_chain(*p,key.empty()?std::nullopt:std::optional<Id>{Id{key}}));}publish_inserts();}
void Application::set_inserts(std::optional<Id> track, std::vector<NativeInsert> inserts) {
    const auto p=services_.projects->state().project;
    const auto& old=insert_chain(*p,track);
    if(old==inserts){cancel_insert_preview();return;}
    const bool permutation=old.size()==inserts.size()&&std::all_of(old.begin(),old.end(),[&](const auto& fx){return std::find(inserts.begin(),inserts.end(),fx)!=inserts.end();});
    const auto runtime=insert_runtime_.find(track?track->value:std::string{});
    if(permutation&&runtime!=insert_runtime_.end()&&runtime->second.graph){
        require_not_playing();auto candidate=*p;SetInserts{track,inserts}.apply(candidate);candidate.validate();
        std::vector<Id> ids,previous;for(const auto& fx:inserts)ids.push_back(fx.id);for(const auto& fx:old)previous.push_back(fx.id);
        if(device_)device_->stop();
        engine_->quiesce();
        try{runtime->second.graph->reorder_serial(ids);edit(SetInserts{track,std::move(inserts)},{},true);}
        catch(...){if(services_.projects->state().project==p){runtime->second.graph->reorder_serial(previous);if(device_)device_->start();}throw;}
        return;
    }
    if(parameter_only(insert_chain(*p,track),inserts)){auto candidate=*p;SetInserts{track,inserts}.apply(candidate);candidate.validate();preview_inserts(track,inserts);services_.projects->execute(SetInserts{track,std::move(inserts)});}
    else {
        require_not_playing();
#ifndef MRS_HAS_VST3
        require(std::none_of(inserts.begin(),inserts.end(),[](const auto& fx){return fx.kind==InsertKind::vst3;}),"This build has no VST3 support");
#endif
        auto candidate=*p;SetInserts{track,inserts}.apply(candidate);candidate.validate();
        const auto revision=services_.projects->state().revision;
        staged_insert_target_=track?track->value:std::string{};
        try {
            // Stop every DSP owner before capturing or replacing a chain.
            if(device_){device_->stop();engine_->quiesce();}
            capture_insert_state(candidate,{},true);
            inserts=insert_chain(candidate,track);
            const auto channels=device_config_?static_cast<std::uint32_t>(device_config_->outputs.size()):2;
            const auto block=device_config_?device_config_->buffer_frames:128;
            const bool instrument=track&&std::any_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==*track&&t.kind==TrackKind::instrument;});
            if(!inserts.empty())staged_insert_graph_=std::make_shared<processing::PreparedGraph>(
                processing::GraphSnapshot{std::make_shared<const processing::GraphState>(processing::insert_graph(inserts)),0,false,false},
                processing::ProcessConfig{p->sample_rate,channels,track?block:8192,block,instrument}
#ifdef MRS_HAS_VST3
                ,processing::hosted_factory
#endif
            );
            // This prepared graph becomes the actual runtime; no throwaway VST3 preflight.
            edit(SetInserts{track,std::move(inserts)});
            staged_insert_target_.reset();staged_insert_graph_.reset();
        }catch(...){
            const auto error=std::current_exception();staged_insert_target_.reset();staged_insert_graph_.reset();
            // Failed validation must leave the previous document and its DSP usable.
            if(device_){if(services_.projects->state().revision==revision){try{rebuild_audio(true);}catch(...){disconnect();}}else disconnect();}
            std::rethrow_exception(error);
        }
    }
}
void Application::transfer_insert(std::optional<Id> source,const Id& slot,std::optional<Id> destination,std::size_t boundary,bool move){
    require_not_playing();auto candidate=*services_.projects->state().project;
    auto from=insert_chain(candidate,source);auto at=std::find_if(from.begin(),from.end(),[&](const auto& fx){return fx.id==slot;});
    require(at!=from.end(),"Insert no longer exists");
    const auto instrument=[&](std::optional<Id> id){return id&&std::any_of(candidate.tracks.begin(),candidate.tracks.end(),[&](const auto& t){return t.id==*id&&t.kind==TrackKind::instrument;});};
    const auto index=static_cast<std::size_t>(at-from.begin());
    require(!instrument(source)||index>0,"Drag effects only; instrument stays on its channel");
    auto to=insert_chain(candidate,destination);require(boundary<=to.size(),"Invalid insert position");
    require(!instrument(destination)||boundary>0,"Instrument must remain first");
    if(source==destination){const auto position=boundary-(index<boundary?1:0);if(position==index)return;auto fx=*at;from.erase(at);from.insert(from.begin()+static_cast<std::ptrdiff_t>(position),std::move(fx));set_inserts(source,std::move(from));return;}
    require(to.size()<8,"Insert chain supports up to eight effects");
    const auto revision=services_.projects->state().revision;
    if(device_)device_->stop();
    engine_->quiesce();
    try{
        capture_insert_state(candidate,{},true);from=insert_chain(candidate,source);to=insert_chain(candidate,destination);
        auto fx=from[index];if(!move)fx.id=new_id();to.insert(to.begin()+static_cast<std::ptrdiff_t>(boundary),fx);
        SetInserts{destination,to}.apply(candidate);if(move){from.erase(from.begin()+static_cast<std::ptrdiff_t>(index));SetInserts{source,from}.apply(candidate);}candidate.validate();
        const auto prepare=[&](std::optional<Id> id,const std::vector<NativeInsert>& chain){const auto block=device_config_?device_config_->buffer_frames:128;
            std::shared_ptr<processing::PreparedGraph> graph;
            if(!chain.empty())graph=std::make_shared<processing::PreparedGraph>(processing::GraphSnapshot{std::make_shared<const processing::GraphState>(processing::insert_graph(chain)),0,false,false},
                processing::ProcessConfig{candidate.sample_rate,device_config_?static_cast<std::uint32_t>(device_config_->outputs.size()):2u,id?block:8192u,block,instrument(id)}
#ifdef MRS_HAS_VST3
                ,processing::hosted_factory
#endif
            );staged_insert_graphs_[id?id->value:std::string{}]=std::move(graph);};
        prepare(destination,to);if(move)prepare(source,from);
        class Transfer final:public ICommand{Project next_;bool move_;public:Transfer(Project p,bool m):next_(std::move(p)),move_(m){}std::string_view name()const override{return move_?"Move insert":"Copy insert";}void apply(Project& p)const override{p=next_;}};
        edit(Transfer{std::move(candidate),move});staged_insert_graphs_.clear();
    }catch(...){staged_insert_graphs_.clear();if(device_&&services_.projects->state().revision==revision)device_->start();throw;}
}
void Application::load_insert_preset(std::optional<Id> track,NativeInsert preset) {
    require_not_playing();preset.validate();
    auto effects=insert_chain(*services_.projects->state().project,track);
    auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& effect){return effect.id==preset.id;});
    require(at!=effects.end()&&at->kind==preset.kind,"unknown preset insert");
    require(preset.kind!=InsertKind::vst3||(at->class_id==preset.class_id&&at->plugin_path==preset.plugin_path),"preset plugin mismatch");
    const auto id=preset.id;
    if(preset.kind==InsertKind::vst3){
        auto runtime=insert_runtime_.find(track?track->value:std::string{});
        require(device_&&runtime!=insert_runtime_.end()&&runtime->second.graph,"Plugin is not prepared; connect audio or offline clock");
        auto candidate=*services_.projects->state().project;*at=preset;SetInserts{track,effects}.apply(candidate);candidate.validate();
        device_->stop();
        try {
            auto previous=runtime->second.graph->capture();auto requested=processing::insert_graph(std::array{preset}).nodes.front();
            runtime->second.graph->restore_node(requested);runtime->second.pending.reset();
            try{services_.projects->execute(SetInserts{track,std::move(effects)});}catch(...){auto old=std::find_if(previous.nodes.begin(),previous.nodes.end(),[&](const auto& n){return n.id==id;});if(old!=previous.nodes.end())runtime->second.graph->restore_node(*old);throw;}
            unsaved_=true;device_->start();return;
        }catch(...){device_->start();throw;}
    }
    *at=std::move(preset);
    // An explicit load must restore even identical serialized state: the active
    // controller/DSP may have been edited since the last project snapshot.
    edit(SetInserts{track,std::move(effects)},id);
}
void Application::name_insert_preset(std::optional<Id> track,const Id& slot,std::string name) {
    auto effects=insert_chain(*services_.projects->state().project,track);
    auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& fx){return fx.id==slot;});
    require(at!=effects.end(),"unknown preset insert");at->preset_name=std::move(name);at->validate();
    // Metadata only: never restore/rebuild a processor or query it from a callback.
    services_.projects->execute(SetInserts{track,std::move(effects)});unsaved_=true;
}
bool Application::open_plugin_editor(std::optional<Id> track,const Id& slot,void* parent,int& w,int& h){
    if(!device_){require_not_playing();start_empty_clock(true);}
    auto it=insert_runtime_.find(track?track->value:std::string{});
    if(it==insert_runtime_.end()){require_not_playing();rebuild_audio();it=insert_runtime_.find(track?track->value:std::string{});}
    if(it==insert_runtime_.end()||!it->second.graph)throw std::runtime_error("Plugin is not prepared: "+(track?midi_status(*track):std::string{"connect audio or offline clock"}));
    return it->second.graph->open_editor(slot,parent,w,h);
}
void Application::close_plugin_editors(){for(auto& [key,r]:insert_runtime_){(void)key;if(r.graph)r.graph->close_editors();}}
void Application::close_plugin_editor(std::optional<Id> track,const Id& slot){auto it=insert_runtime_.find(track?track->value:std::string{});if(it!=insert_runtime_.end()&&it->second.graph)it->second.graph->close_editor(slot);}
std::uint32_t Application::plugin_latency(std::optional<Id> track,const Id& slot) const{auto it=insert_runtime_.find(track?track->value:std::string{});return it==insert_runtime_.end()||!it->second.graph?0:it->second.graph->node_latency(slot);}
bool Application::plugin_failed() const{for(const auto& [key,r]:insert_runtime_){(void)key;if(r.graph && r.graph->failed())return true;}return false;}
std::vector<processing::ParameterInfo> Application::plugin_parameters(std::optional<Id> track,const Id& slot) const{
    auto it=insert_runtime_.find(track?track->value:std::string{});if(it==insert_runtime_.end()||!it->second.graph)return {};
    // Metadata belongs to the prepared instance; no processing-thread calls.
    return it->second.graph->parameter_infos(slot);
}
void Application::set_plugin_parameter(std::optional<Id> track,const Id& slot,std::uint32_t id,float value){auto chain=insert_chain(*services_.projects->state().project,track);auto it=std::find_if(chain.begin(),chain.end(),[&](const auto& fx){return fx.id==slot;});if(it!=chain.end()&&wav_core(it->kind)){require(id<(it->kind==InsertKind::rig?7u:5u),"unknown WAV core parameter");if(id==5)it->rig.input_gain=value;else if(id==6)it->rig.output_gain=value;else if(id==0)it->gain=value;else if(id==1)it->ir.mix=value;else if(id==2)it->ir.low_cut=value;else if(id==3)it->ir.high_cut=value;else {require(value==0||value==1,"invalid Cab polarity");it->ir.invert=value==1;}set_inserts(track,std::move(chain));return;}if(it!=chain.end()&&it->kind==InsertKind::reverb){require(id<5,"unknown MR Reverb parameter");if(id==0)it->reverb.pre_delay=value;else if(id==1)it->reverb.tone=value;else if(id==2)it->reverb.mix=value;else if(id==3){require(value==0||value==1,"invalid MR Reverb gate enable");it->reverb.gate_enabled=value==1;}else it->reverb.gate_threshold=value;set_inserts(track,std::move(chain));return;}require(it!=chain.end() && it->kind==InsertKind::vst3,"unknown VST3 insert");auto p=std::find_if(it->parameters.begin(),it->parameters.end(),[&](const auto& p){return p.id==id;});if(p==it->parameters.end())it->parameters.push_back({id,value});else p->value=value;set_inserts(track,std::move(chain));}
NativeInsert Application::capture_insert(std::optional<Id> track,const Id& slot){
    poll_reverbs();
    auto candidate=*services_.projects->state().project;capture_insert_state(candidate);
    const auto& effects=insert_chain(candidate,track);auto found=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});
    require(found!=effects.end(),"unknown insert");return *found;
}
void Application::capture_insert_state(Project& project,std::optional<Id> authoritative,bool callbacks_stopped){
    bool has_vst=false;for(const auto& [key,r]:insert_runtime_){(void)key;if(r.graph)for(const auto& n:r.graph->snapshot().graph->nodes)has_vst=has_vst||n.format==processing::ProcessorFormat::vst3;}if(!has_vst || !device_)return;
    if(!callbacks_stopped){require_not_playing();device_->stop();}
    try{const auto original=services_.projects->state().project;
        for(const auto& [key,r]:insert_runtime_)if(r.graph&&(!staged_insert_target_||key==*staged_insert_target_)){auto* chain=&project.master_inserts;if(!key.empty()){chain=nullptr;for(auto& t:project.tracks)if(t.id.value==key)chain=&t.inserts;}if(!chain)continue;
            const auto original_track=key.empty()?std::optional<Id>{}:std::optional<Id>{Id{key}};
            const auto& old_chain=insert_chain(*original,original_track);
            // No state serialization is needed for a removed/replaced plugin.
            if(std::none_of(chain->begin(),chain->end(),[&](const auto& fx){return fx.kind==InsertKind::vst3&&(!authoritative||fx.id!=*authoritative)&&std::any_of(old_chain.begin(),old_chain.end(),[&](const auto& old){return old.id==fx.id&&old.class_id==fx.class_id&&old.plugin_path==fx.plugin_path&&old.component_state==fx.component_state&&old.controller_state==fx.controller_state;});}))continue;
            const auto saved=r.graph->capture();
            for(auto& fx:*chain)if(fx.kind==InsertKind::vst3 && (!authoritative || fx.id!=*authoritative))for(const auto& node:saved.nodes)if(node.id==fx.id && node.processor_id==fx.plugin_path && node.plugin.class_id==fx.class_id){
                const auto old=std::find_if(old_chain.begin(),old_chain.end(),[&](const auto& f){return f.id==fx.id;});if(old==old_chain.end() || fx.component_state!=old->component_state || fx.controller_state!=old->controller_state)continue;
                const auto overrides=fx.parameters;fx.component_state=node.plugin.component;fx.controller_state=node.plugin.controller;fx.parameters.clear();const auto infos=r.graph->parameter_infos(node.id);
                for(const auto& p:node.parameters)if(std::any_of(infos.begin(),infos.end(),[&](const auto& info){return info.id==p.id&&info.automatable;}))fx.parameters.push_back({p.id,p.value});
                for(const auto& p:overrides)if(std::find(old->parameters.begin(),old->parameters.end(),p)==old->parameters.end()){auto at=std::find_if(fx.parameters.begin(),fx.parameters.end(),[&](const auto& v){return v.id==p.id;});if(at==fx.parameters.end())fx.parameters.push_back(p);else *at=p;}
            }
        }if(!callbacks_stopped)device_->start();
    }catch(...){if(!callbacks_stopped)device_->start();throw;}
}
void Application::set_track_input(const Id& id, int input, bool stereo) {
    require_not_playing();
    const auto names = input_names();
    require(input < 0 || static_cast<std::size_t>(input+(stereo ? 1 : 0)) < names.size(),"physical input is unavailable; connect the intended device first");
    edit(SetTrackInput{id,input,stereo});
}
void Application::set_track_sends(const Id& id, std::vector<Track::Send> sends) { edit(SetTrackSends{id,std::move(sends)}); }
void Application::set_send_gain(const Id& id, std::size_t index, float gain) {
    const auto p = services_.projects->state().project;
    const auto t = std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& track) { return track.id == id; });
    require(t != p->tracks.end() && index < t->sends.size(),"unknown send");
    auto sends = t->sends; sends[index].gain = gain;
    services_.projects->execute(SetTrackSends{id,std::move(sends)}); cancel_mix_preview();
}
void Application::set_send_bypassed(const Id& id,std::size_t index,bool bypass) {
    const auto p=services_.projects->state().project;
    const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& track){return track.id==id;});
    require(t!=p->tracks.end()&&index<t->sends.size(),"unknown send");
    auto sends=t->sends;sends[index].bypass=bypass;
    services_.projects->execute(SetTrackSends{id,std::move(sends)});cancel_mix_preview();
}
// Performance projection, not a user project edit. Preparation happens while the old
// graph owns its immutable assets; only stop/quiesce/prepare/start touches the device.
void Application::install_playback(Project candidate,
    std::map<std::string,std::shared_ptr<const audio::AudioData>> decoded,
    std::shared_ptr<PreparedWavImport> media,audio::ClickSettings click,std::optional<PerformancePlan> performance,
    std::uint64_t token,bool staged,std::vector<Id> live_tracks) {
    require(workspace_==Workspace::live&&path_.empty(),"Playback handoff requires a Live runtime");
    require_not_recording();
    if(!live_tracks.empty())
        require(device_ && device_config_ && audio_name_!="Offline clock (no sound)" &&
                audio_name_!="Disconnected" && audio_running(),
                "Live Input + Playback requires a connected physical device; offline handoff is disabled");
    if(!device_||!device_config_){
        // Editing/preparing a show remains available after explicit Disconnect.
        // Reuse the normal Core offline clock; never reopen a live hardware handle here.
        const auto savedClick=click_settings_;click_settings_={};
        try{start_empty_clock();}catch(...){click_settings_=savedClick;throw;}
        click_settings_=savedClick;
    }
    const auto previous=services_.projects->state().project;
    const bool keepLive=!live_tracks.empty();
    const auto retainedTrack=[&](const Id& id){return std::find(live_tracks.begin(),live_tracks.end(),id)!=live_tracks.end();};
    if(keepLive) {
        // Manual quiescent-only bridge. The insert graph is borrowed by the new
        // project only after the device and ahead workers have stopped.
        require(!staged && engine_->state().playback!=PlaybackState::playing,
                "Stop Playback before switching items with Live Input; seamless staging awaits #301");
        require(live_tracks.size()<=64,"Too many persistent Live channels");
        for(std::size_t i=0;i<live_tracks.size();++i) {
        const auto& live_track=live_tracks[i];
        require(std::find(live_tracks.begin(),live_tracks.begin()+i,live_track)==live_tracks.begin()+i,
                "Duplicate persistent Live channel");
        const auto anchor=std::find_if(previous->tracks.begin(),previous->tracks.end(),
            [&](const Track& track){return track.id==live_track;});
        require(anchor!=previous->tracks.end() &&
                ((anchor->kind==TrackKind::audio && anchor->input>=0) || anchor->kind==TrackKind::instrument),
                "Live Input anchor must be a configured audio or instrument channel");
        require(anchor->kind!=TrackKind::audio || !anchor->input_monitor || (std::find(device_config_->inputs.begin(),device_config_->inputs.end(),anchor->input)
                    !=device_config_->inputs.end() &&
                (!anchor->input_stereo ||
                 std::find(device_config_->inputs.begin(),device_config_->inputs.end(),anchor->input+1)
                    !=device_config_->inputs.end())),
                "Physical Live Input is no longer assigned to active ASIO input streams");
        require(std::none_of(candidate.tracks.begin(),candidate.tracks.end(),
                [&](const Track& track){return track.id==live_track;}),
                "Live Input anchor collides with Playback track");
        candidate.tracks.push_back(*anchor); // stable VST3 IDs and hardware/input routing
        }
    }
    require(previous->master_inserts.empty()&&previous->ara_bindings.empty()&&
        std::all_of(previous->tracks.begin(),previous->tracks.end(),[&](const auto& track){
            return retainedTrack(track.id) || (track.kind==TrackKind::audio &&
                 track.inserts.empty()&&!track.input_monitor);
        }),"Prepared Playback handoff does not replace unrelated live instruments/plugins");
    require(std::all_of(graphs_->state().graph->nodes.begin(),graphs_->state().graph->nodes.end(),[](const auto& node){return node.format==processing::ProcessorFormat::native;}),"Prepared Playback handoff does not replace hosted graph plugins");
    candidate.validate();click.validate();const auto config=*device_config_;
    require(candidate.sample_rate==config.sample_rate,"Prepared Playback rate no longer matches the device");
    validate_hardware(candidate,config);(void)mapped_click(click,config.outputs);
    auto head=engine_->state();require(!head.count_remaining,"End count-in before switching items");
    auto nextProjects=std::make_shared<ProjectStore>(candidate);
    auto nextTransport=std::make_shared<audio::EngineTransport>(engine_,Timeline(candidate.time,candidate.sample_rate));
    auto nextMusical=std::make_unique<MusicalTimeline>(Services{nextProjects,nextTransport},[this]{require_not_recording();require(!engine_->state().count_remaining,"End count-in before navigating");});
    auto nextObserver=nextProjects->subscribe([this](const ProjectState&){if(ai_worker_)ai_worker_->cancel_all();});
    auto nextSession=new_id();
    const auto oldServices=services_;const auto oldTransport=transport_;const auto oldClick=click_settings_;
    const auto oldApplied=applied_mix_revision_;auto oldPrepared=std::move(prepared_);
    auto oldAssets=std::move(assets_);auto oldStaging=std::move(import_staging_);
    auto oldTracks=std::move(mixer_tracks_);auto oldInserts=std::move(insert_runtime_);
    std::unique_ptr<MusicalTimeline> oldMusical;
    std::optional<audio::RenderGraph> rollbackGraph;
    bool stopped=false,published=false;
    try {
        // The default WAV-only fast path remains unchanged. With a live anchor
        // this stop/quiesce MUST precede reusing the mutable PreparedGraph.
        if(keepLive) {
            device_->stop();stopped=true;engine_->quiesce();
            head=engine_->state();rollbackGraph=engine_->stopped_graph();
            const auto master=oldInserts.find(std::string{});
            require(master!=oldInserts.end(),
                    "Prepared Live Input insert runtime is missing");
            for(const auto& id:live_tracks) {
                const auto retained=oldInserts.find(id.value);
                require(retained!=oldInserts.end(),"Prepared Live channel insert runtime is missing");
                insert_runtime_.emplace(id.value,retained->second);
            }
            insert_runtime_.emplace(std::string{},master->second);
            // WAV stems have no inserts: they still reserve ordered Core slots.
            for(const auto& track:candidate.tracks)
                if(!retainedTrack(track.id))insert_runtime_.emplace(track.id.value,InsertRuntime{});
        }
        for(auto& [source,data]:decoded)cache_asset(source,std::move(data));
        import_staging_.push_back(std::move(media));
        click_settings_=click;preparing_project_=&candidate;
        auto graph=render(config,keepLive); // one shared graph; plugin identity retained
        preparing_project_=nullptr;
        graph.performance=performance;graph.performance_token=token;
        if(staged){
            auto pending=std::make_shared<StagedPlayback>();
            pending->transition=engine_->prepare_transition(std::move(graph),performance_token_);
            pending->project=std::move(candidate);pending->assets=std::move(assets_);pending->media=std::move(import_staging_);
            pending->tracks=std::move(mixer_tracks_);pending->inserts=std::move(insert_runtime_);pending->prepared=std::move(prepared_);
            pending->click=click;pending->performance=std::move(performance);pending->token=token;
            require(engine_->stage_transition(pending->transition),"Playback changed while staging transition");
            staged_playback_=std::move(pending);
            assets_=std::move(oldAssets);import_staging_=std::move(oldStaging);mixer_tracks_=std::move(oldTracks);
            insert_runtime_=std::move(oldInserts);prepared_=std::move(oldPrepared);click_settings_=oldClick;applied_mix_revision_=oldApplied;return;
        }
        if(!stopped) {device_->stop();stopped=true;engine_->quiesce();
                      head=engine_->state();rollbackGraph=engine_->stopped_graph();}
        // Scalar mixer controls may be newer than the graph's original construction.
        rollbackGraph->master_gain=previous->master_gain;
        for(std::size_t i=0;i<previous->tracks.size();++i)rollbackGraph->mixer[i]=previous->tracks[i].mix;
        auto initial=audio::RealtimeState{};initial.playback=PlaybackState::stopped;
        for(const auto& track:candidate.tracks)if(track.kind==TrackKind::instrument) {
            const auto& runtime=insert_runtime_.at(track.id.value);
            if(runtime.graph)runtime.graph->panic();
        }
        engine_->prepare({config.sample_rate,static_cast<std::uint32_t>(config.inputs.size()),
            static_cast<std::uint32_t>(config.outputs.size()),8192,config.buffer_frames,config.processing_workers},std::move(graph),initial);
        oldMusical=std::move(musical_);services_={nextProjects,nextTransport};transport_=nextTransport;musical_=std::move(nextMusical);published=true;
        device_->start(); // same open hardware handle: no close/open/enumeration on handoff
    } catch (...) {
        const auto error=std::current_exception();preparing_project_=nullptr;
        if(stopped){try{device_->stop();}catch(...){}engine_->quiesce();}
        for(auto& [source,cached]:assets_){(void)source;*cached.cancel=true;}
        assets_=std::move(oldAssets);import_staging_=std::move(oldStaging);
        mixer_tracks_=std::move(oldTracks);insert_runtime_=std::move(oldInserts);
        prepared_=std::move(oldPrepared);applied_mix_revision_=oldApplied;click_settings_=oldClick;
        if(published){services_=oldServices;transport_=oldTransport;musical_=std::move(oldMusical);}
        if(stopped&&rollbackGraph){
            auto restore=head;if(restore.playback==PlaybackState::playing)restore.playback=PlaybackState::paused;
            try {
                engine_->prepare({config.sample_rate,static_cast<std::uint32_t>(config.inputs.size()),
                    static_cast<std::uint32_t>(config.outputs.size()),8192,config.buffer_frames,config.processing_workers},std::move(*rollbackGraph),restore);
                if(head.playback==PlaybackState::playing)require(engine_->enqueue({audio::ControlKind::play}),"Cannot resume previous Playback");
                device_->start();
            }catch(...){disconnect();throw std::runtime_error("Playback handoff rolled back; audio could not restart");}
        }else if(stopped){try{device_->start();}catch(...){disconnect();}}
        std::rethrow_exception(error);
    }
    performance_plan_=std::move(performance);performance_token_=token;
    // Commit application metadata only after the prepared graph/device successfully started.
    for(auto& [source,cached]:oldAssets){(void)source;*cached.cancel=true;}
    cancel_wav_import();if(ai_worker_)ai_worker_->cancel_all();
    ai_project_changed_=std::move(nextObserver);
    ai_session_=std::move(nextSession);++*ai_permission_revision_;ai_receipts_.clear();ai_operations_.clear();ai_applied_operations_.clear();
    document_.project=std::move(candidate);document_.mixer.clear();
    armed_.reset();armed_tracks_.clear();owned_media_.clear();waveform_error_.clear();
    applied_mix_revision_=nextProjects->state().revision;saved_project_revision_=saved_graph_revision_=0;unsaved_=true;
    try{transport_->poll();musical_->refresh();}catch(const std::exception&){/* UI poll retries projections; audio/model already committed. */}
}
bool Application::cancel_staged_playback(){
    if(!staged_playback_)return true;
    if(engine_->cancel_transition(staged_playback_->transition)){staged_playback_.reset();return true;}
    poll_staged_playback();return !staged_playback_;
}
void Application::poll_staged_playback(){
    if(!staged_playback_)return;
    const auto phase=engine_->transition_phase(staged_playback_->transition);
    if(phase==audio::AudioEngine::TransitionPhase::cancelled){staged_playback_.reset();return;}
    if(phase!=audio::AudioEngine::TransitionPhase::adopted)return;
    auto& next=*staged_playback_;
    // Construct control-side services before retiring the outgoing render state.
    auto projects=std::make_shared<ProjectStore>(next.project);
    auto transport=std::make_shared<audio::EngineTransport>(engine_,Timeline(next.project.time,next.project.sample_rate));
    auto musical=std::make_unique<MusicalTimeline>(Services{projects,transport},[this]{require_not_recording();});
    auto observer=projects->subscribe([this](const ProjectState&){if(ai_worker_)ai_worker_->cancel_all();});
    require(engine_->acknowledge_transition(next.transition),"Playback transition acknowledgement failed");
    for(auto& [key,value]:assets_){(void)key;*value.cancel=true;}
    assets_=std::move(next.assets);import_staging_=std::move(next.media);mixer_tracks_=std::move(next.tracks);
    insert_runtime_=std::move(next.inserts);prepared_=std::move(next.prepared);click_settings_=next.click;
    performance_plan_=std::move(next.performance);performance_token_=next.token;
    services_={projects,transport};transport_=std::move(transport);musical_=std::move(musical);ai_project_changed_=std::move(observer);
    playback_sources_=std::move(next.sources);playback_source_project_=next.project.id;
    document_.project=std::move(next.project);document_.mixer.clear();armed_.reset();armed_tracks_.clear();owned_media_.clear();
    applied_mix_revision_=projects->state().revision;staged_playback_.reset();
    transport_->poll();musical_->refresh();
}
void Application::set_performance_plan(PerformancePlan plan){
    plan.validate();require_not_playing();require_not_recording();const auto expected=performance_token_;
    require(cancel_staged_playback()&&expected==performance_token_,"Playback transition in progress; retry policy edit");
    auto previous=performance_plan_;performance_plan_=std::move(plan);
    try{rebuild_audio(true);}catch(...){performance_plan_=std::move(previous);throw;}
}
void Application::rebuild_audio(bool retain_inserts) {
    require(cancel_staged_playback(),"Playback transition in progress; retry edit");
    if (!device_ || !device_config_) return;
    // A freshly opened offline document can deliberately have no plugin runtime
    // yet. Bootstrap that runtime once; subsequent I/R transitions retain it.
    const auto project=services_.projects->state().project;
    const bool staged=!staged_insert_graphs_.empty();
    retain_inserts=retain_inserts&&insert_runtime_.contains(std::string{})&&
        std::all_of(project->tracks.begin(),project->tracks.end(),[&](const auto& t){return t.kind==TrackKind::midi||insert_runtime_.contains(t.id.value);});
    auto c = *device_config_;
    c.inputs = audio_name_ == "Offline clock (no sound)" ? std::vector<int>{} : selected_inputs(*services_.projects->state().project);
    const bool reopen = c.inputs != device_config_->inputs;
    if (reopen) {
        require(device_info_.has_value(),"audio device unavailable"); audio::validate_device_config(*device_info_,c);
    }
    // Retain the same open hardware handle; rebuild ONLY after callbacks stop.
    device_->stop();
    try {
        engine_->quiesce();
#ifdef MRS_HAS_ARA
        synchronize_ara();
#endif
        const auto position = engine_->state();
        if (reopen) device_->close();
        audio::RenderGraph graph;
        if (audio_name_ == "Offline clock (no sound)") { prepare_mixer(graph); prepare_midi_clips(graph); prepare_inserts(graph,c,retain_inserts); }
        else graph = render(c,retain_inserts);
        // Reprepare clears live MIDI tracking/generation. With every processing
        // owner stopped and joined, release retained instruments before that
        // reset; new-generation ingress must not be swallowed by a later panic.
        if(retain_inserts||staged_insert_target_||staged){
            for(const auto& t:project->tracks)if(t.kind==TrackKind::instrument){const auto& runtime=insert_runtime_.at(t.id.value);if(runtime.graph)runtime.graph->panic();}
        }
        engine_->prepare({c.sample_rate,static_cast<std::uint32_t>(c.inputs.size()),static_cast<std::uint32_t>(c.outputs.size()),8192,c.buffer_frames,c.processing_workers},std::move(graph),position);
        if (reopen) device_->open(c,engine_);
        device_config_ = c; device_->start(); poll();
    } catch (...) { disconnect(); throw; }
}
void Application::edit(const ICommand& command,std::optional<Id> authoritative,bool retain_inserts) {
    require_not_playing();
    auto candidate = *services_.projects->state().project;
    command.apply(candidate); candidate.validate();
    {std::vector<Id> tracks;for(const auto& t:candidate.tracks)if(t.kind!=TrackKind::midi)tracks.push_back(t.id);(void)audio::compile_midi_clips(candidate,tracks);(void)audio::compile_midi_events(candidate,tracks);}
    if(!retain_inserts&&!staged_insert_target_&&staged_insert_graphs_.empty())capture_insert_state(candidate,authoritative);
    candidate.validate();
    if (device_config_ && audio_name_ != "Offline clock (no sound)") validate_hardware(candidate,*device_config_);
    if (device_ && device_config_ && audio_name_ != "Offline clock (no sound)") {
        auto c = *device_config_; c.inputs = selected_inputs(candidate);
        require(device_info_.has_value(),"audio device unavailable"); audio::validate_device_config(*device_info_,c);
    }
    require(candidate.clips.size() <= audio::max_voices,"too many playback clips");
    require(std::count_if(candidate.tracks.begin(),candidate.tracks.end(),[](const auto& t) { return t.kind != TrackKind::midi; }) <= static_cast<std::ptrdiff_t>(audio::max_mixer_tracks),"mixer supports up to 128 audio tracks and buses");
    std::size_t streamed{}, bytes{};
    for (const auto& clip : candidate.clips) {
        if(clip.midi)continue;
        const auto data = asset(clip.source);
        require(data->sample_rate == candidate.sample_rate,"WAV/project sample-rate mismatch");
        require(clip.source_offset <= data->frames() && audio_source_span(clip) <= data->frames()-clip.source_offset,"clip exceeds source audio");
        if (data->file || (clip.stretch&&!stretch::neutral(clip.stretch->parameters))) {
            ++streamed; bytes += 8*8192*static_cast<std::size_t>(data->channels)*sizeof(float);
        }
    }
    require(streamed <= 32 && bytes <= 256*1024*1024,"disk voice budget exceeded (32 voices / 256 MiB)");
    class PreparedEdit final:public ICommand{Project project_;std::string name_;public:PreparedEdit(Project p,std::string_view name):project_(std::move(p)),name_(name){}std::string_view name() const override{return name_;}void apply(Project& p) const override{p=project_;}};
    services_.projects->execute(PreparedEdit{std::move(candidate),command.name()}); sync_arm(); rebuild_audio(retain_inserts);
}
#ifdef MRS_HAS_ARA
void Application::synchronize_ara(const Project* candidate) {
    const auto& project=candidate?*candidate:*services_.projects->state().project;
    if(!ara_&&!project.ara_bindings.empty()){ara_=std::make_unique<ara::Arrangement>();ara_->set_preparation(ara_cancel_,ara_wait_);ara_->prefer_live(true);}
    if (ara_) {ara_->prefer_live(true);ara_->restore_bindings(project.ara_bindings);ara_->synchronize(project,[this](const auto& source) { return asset(source); });}
}
void Application::change_ara_bindings(std::vector<AraBinding> bindings,const std::string& strict_source){
    require_not_playing();require_not_recording();
    const auto before=services_.projects->state().project;SetAraBindings command(std::move(bindings));auto candidate=*before;command.apply(candidate);candidate.validate();
    auto transaction=services_.projects->prepare_transaction(&command);if(!transaction)return;
    if(!ara_){ara_=std::make_unique<ara::Arrangement>();ara_->set_preparation(ara_cancel_,ara_wait_);ara_->prefer_live(true);}
    try{ara_->restore_bindings(candidate.ara_bindings);ara_->synchronize(candidate,[this](const auto& source){return asset(source);});
        if(!strict_source.empty()){const auto state=ara_->status(strict_source);require(state.starts_with("ready")||state.starts_with("analysing")||state.starts_with("pending refresh (plugin still updating)"),state.c_str());}
        transaction->commit();rebuild_audio(true);transaction->finish();
    }catch(...){transaction->rollback();try{ara_->restore_bindings(before->ara_bindings);ara_->synchronize(*before,[this](const auto& source){return asset(source);});rebuild_audio(true);}catch(const std::exception& e){waveform_error_=std::string("ARA rollback: ")+e.what();}throw;}
}
void Application::attach_ara_source(const std::string& source,const std::string& module,const std::string& class_id) {
    require_not_playing(); require_not_recording();
    const auto project = services_.projects->state().project;
    require(std::any_of(project->clips.begin(),project->clips.end(),[&](const auto& c) { return !c.midi && c.source == source; }),"ARA requires an audio source in this project");
    if (!ara_) { ara_ = std::make_unique<ara::Arrangement>(); ara_->set_preparation(ara_cancel_,ara_wait_);ara_->prefer_live(true); }
    require(ara_->status(source) == "unbound","Detach the existing ARA source binding before replacing it");
    ara_->attach(source,module,class_id);auto bindings=project->ara_bindings;bindings.push_back(ara_->binding(source));
    try{change_ara_bindings(std::move(bindings),source);}catch(...){ara_->detach(source);throw;}
}
void Application::detach_ara_source(const std::string& source) {
    require_not_playing(); require_not_recording();
    auto bindings=services_.projects->state().project->ara_bindings;std::erase_if(bindings,[&](const auto& b){return b.source==source;});change_ara_bindings(std::move(bindings));
}
void Application::relink_ara_source(const std::string& source,const std::string& module,const std::string& class_id){
    auto bindings=services_.projects->state().project->ara_bindings;auto at=std::find_if(bindings.begin(),bindings.end(),[&](const auto& b){return b.source==source;});require(at!=bindings.end(),"No saved ARA binding");
    const auto document=at->document_id;
    const auto selected=[&](const auto& b){return b.source==source||(!document.empty()&&b.document_id==document);};
    if(ara_){const auto current=ara_->save_bindings();for(auto& b:bindings)if(selected(b)){const auto cached=std::find_if(current.begin(),current.end(),[&](const auto& c){return c.persistent_id==b.persistent_id;});if(cached!=current.end())b=*cached;}}
    for(auto& b:bindings)if(selected(b)){b.module=module;b.class_id=class_id;}change_ara_bindings(std::move(bindings),source);
}
void Application::retry_ara_source(const std::string& source){
    require_not_playing();require_not_recording();synchronize_ara();require(ara_!=nullptr,"No ARA binding");
    const auto state=ara_->status(source);require(state.starts_with("unavailable")||state.starts_with("failed"),"ARA binding is already available");
    ara_->retry(source);synchronize_ara();rebuild_audio(true);
}
void Application::relink_ara_media(const std::string& source,const std::filesystem::path& path){
    require_not_playing();require_not_recording();const auto before=services_.projects->state().project;
    const auto at=std::find_if(before->ara_bindings.begin(),before->ara_bindings.end(),[&](const auto& b){return b.source==source;});require(at!=before->ara_bindings.end(),"No ARA source binding");
    const auto input=audio::open_wav(path);require(input.sample_rate==before->sample_rate,"Relink WAV sample rate mismatch");
    const auto replacement=utf8(std::filesystem::absolute(path));
    const bool previously_cached=assets_.contains(replacement);
    struct Relink final:ICommand{std::string source,replacement;Relink(std::string a,std::string b):source(std::move(a)),replacement(std::move(b)){}std::string_view name()const override{return "Relink ARA source media";}void apply(Project& p)const override{for(auto& c:p.clips)if(!c.midi&&c.source==source)c.source=replacement;for(auto& b:p.ara_bindings)if(b.source==source)b.source=replacement;}} command(source,replacement);
    auto candidate=*before;command.apply(candidate);candidate.validate();auto transaction=services_.projects->prepare_transaction(&command);if(!transaction)return;
    if(!ara_)synchronize_ara();
    try{ara_->restore_bindings(candidate.ara_bindings);ara_->synchronize(candidate,[this](const auto& s){return asset(s);});const auto state=ara_->status(replacement);require(state.starts_with("ready")||state.starts_with("analysing"),state.c_str());transaction->commit();rebuild_audio(true);transaction->finish();}
    catch(...){transaction->rollback();try{synchronize_ara();rebuild_audio(true);}catch(const std::exception& e){waveform_error_=std::string("ARA media rollback: ")+e.what();}
        if(!previously_cached){const auto cached=assets_.find(replacement);if(cached!=assets_.end()){*cached->second.cancel=true;assets_.erase(cached);}}throw;}
}
void Application::refresh_ara(bool force) {
    require_not_playing(); require_not_recording();
    if (ara_) { if(ara_->poll())unsaved_=true;if(!force&&!ara_->pending_revision())return;if(force)ara_->invalidate(); synchronize_ara(); rebuild_audio(true);require(!ara_cancel_||!ara_cancel_->load(),"ARA playback preparation cancelled"); }
}
std::optional<std::uint64_t> Application::ara_pending_revision() const {return ara_?ara_->pending_revision():std::nullopt;}
std::string Application::ara_status(const std::string& source) const { if(ara_)return ara_->status(source);for(const auto& b:services_.projects->state().project->ara_bindings)if(b.source==source)return "pending (saved ARA binding)";return "unbound"; }
std::string Application::ara_document_key(const std::string& source) const {for(const auto& b:services_.projects->state().project->ara_bindings)if(b.source==source&&!b.document_id.empty())return "document:"+b.document_id;return "source:"+source;}
std::vector<ara::ClipCache> Application::ara_caches() const { return ara_ ? ara_->caches() : std::vector<ara::ClipCache>{}; }
void Application::bypass_ara_source(const std::string& source,bool disabled) {
    auto bindings=services_.projects->state().project->ara_bindings;auto at=std::find_if(bindings.begin(),bindings.end(),[&](const auto& b){return b.source==source;});require(at!=bindings.end(),"No ARA binding");at->bypass=disabled;change_ara_bindings(std::move(bindings));
}
bool Application::ara_bypassed(const std::string& source) const {for(const auto& b:services_.projects->state().project->ara_bindings)if(b.source==source)return b.bypass;return false;}
bool Application::poll_ara() {
    if(!ara_)return false;
    const bool dirty=ara_->poll();
    if(dirty)unsaved_=true;
    auto state=engine_->state();if(state.loop)ara_cycle_=state.loop;
    bool cycling=state.loop.has_value();
    const auto rate=services_.projects->state().project->sample_rate;
    auto sample=[&](double seconds){require(std::isfinite(seconds)&&seconds>=0&&seconds<=static_cast<double>(max_sample)/rate,"Invalid ARA transport position");return static_cast<Sample>(std::llround(seconds*rate));};
    for(const auto& request:ara_->take_transport_requests()){
        switch(request.action){
            case ara::TransportAction::play:play();break;
            case ara::TransportAction::stop:pause();break;
            case ara::TransportAction::seek:seek(sample(request.position));break;
            case ara::TransportAction::cycle_range:{require_not_recording();const LoopRange range{sample(request.position),sample(request.position+request.duration)};require(std::isfinite(request.duration)&&request.duration>0&&range.end>range.start,"Invalid ARA loop range");ara_cycle_=range;if(cycling)services_.transport->set_loop(range);break;}
            case ara::TransportAction::cycle_enabled:require_not_recording();cycling=request.enabled;if(cycling)require(ara_cycle_.has_value(),"ARA loop range is not set");services_.transport->set_loop(cycling?ara_cycle_:std::nullopt);break;
        }
    }
    if(engine_->try_state(state))ara_->transport(*services_.projects->state().project,state);
    return dirty;
}
bool Application::open_ara_editor(const std::string& source,const Id& clip,void* parent,int& width,int& height) {
    require(ara_!=nullptr,"No ARA binding"); return ara_->open_editor(source,clip,parent,width,height);
}
void Application::close_ara_editor(const std::string& source) { if (ara_) ara_->close_editor(source); }
bool Application::resize_ara_editor(const std::string& source,int& width,int& height) {return ara_&&ara_->resize_editor(source,width,height);}
void Application::close_ara_editors() { if (ara_) ara_->close_editors(); }
void Application::select_ara_clip(const std::string& source,const Id& clip) { if(!ara_)synchronize_ara();require(ara_!=nullptr,"No ARA binding");ara_->select_clip(source,clip); }
void Application::set_ara_preparation(std::shared_ptr<std::atomic<bool>> cancel,std::function<void()> wait) {
    ara_cancel_=std::move(cancel);ara_wait_=std::move(wait);if(ara_)ara_->set_preparation(ara_cancel_,ara_wait_);
}
#endif
Id Application::add_audio_track(std::string name) {
    require(!name.empty() && name.size() <= 4096,"enter a track name");
    auto id = new_id(); edit(AddTrack{{id,std::move(name),TrackKind::audio,{}}}); return id;
}
Id Application::add_instrument_track(std::string name) {
    require(!name.empty()&&name.size()<=4096,"enter a track name");
    auto id=new_id();edit(AddTrack{{id,std::move(name),TrackKind::instrument,{}}});return id;
}
Id Application::add_instrument_track(std::string name,NativeInsert instrument) {
    require_not_playing();require_not_recording();
    require(!name.empty()&&name.size()<=4096,"enter a track name");
    require(instrument.kind==InsertKind::vst3,"Choose a VST3 instrument");instrument.validate();
#ifndef MRS_HAS_VST3
    throw std::invalid_argument("This build has no VST3 support");
#else
    auto candidate=*services_.projects->state().project;
    require(std::count_if(candidate.tracks.begin(),candidate.tracks.end(),[](const auto& t){return t.kind!=TrackKind::midi;})<static_cast<std::ptrdiff_t>(audio::max_mixer_tracks),"mixer supports up to 128 audio tracks and buses");
    const auto id=new_id();Track track{id,std::move(name),TrackKind::instrument,{}};track.inserts={std::move(instrument)};
    candidate.tracks.push_back(track);candidate.validate();
    const auto c=device_config_.value_or(audio::DeviceConfig{0,candidate.sample_rate,128,{}, {0,1}});
    if(device_)device_->stop();
    const auto position=engine_->state();
    bool prepared_engine=false;
    try {
        capture_insert_state(candidate,{},true);
        auto state=std::make_shared<const processing::GraphState>(processing::insert_graph(track.inserts));
        auto synth=std::make_shared<processing::PreparedGraph>(processing::GraphSnapshot{state,0,false,false},processing::ProcessConfig{c.sample_rate,static_cast<std::uint32_t>(c.outputs.size()),c.buffer_frames,c.buffer_frames,true},processing::vst3_instrument_factory);
        // Reuse this exact validated instance; do not load the plugin again after commit.
        if(device_){
            insert_runtime_.emplace(id.value,InsertRuntime{synth,{}});prepared_engine=true;preparing_project_=&candidate;
            audio::RenderGraph graph;
            if(audio_name_=="Offline clock (no sound)"){prepare_mixer(graph);prepare_midi_clips(graph);prepare_inserts(graph,c,true);}
            else graph=render(c,true);
            engine_->prepare({c.sample_rate,static_cast<std::uint32_t>(c.inputs.size()),static_cast<std::uint32_t>(c.outputs.size()),8192,c.buffer_frames,c.processing_workers},std::move(graph),position);
            preparing_project_=nullptr;
            device_->start();
        }
        class Create final:public ICommand {
            Project candidate_;public:explicit Create(Project p):candidate_(std::move(p)){}
            std::string_view name()const override{return "Create instrument track";}
            void apply(Project& p)const override{p=candidate_;}
        };
        services_.projects->execute(Create{std::move(candidate)});unsaved_=true;
    } catch(...) {
        preparing_project_=nullptr;
        if(device_)device_->stop();
        insert_runtime_.erase(id.value);
        if(prepared_engine){try{rebuild_audio(true);}catch(...){disconnect();}}
        else if(device_)device_->start();
        throw;
    }
    sync_arm();publish_midi_routes();return id;
#endif
}
void Application::update_live_channels(const std::vector<Id>& owned,std::vector<Track> channels) {
    require(workspace_==Workspace::live && path_.empty(),"Live channels require a show runtime");
    require_not_playing();require_not_recording();
    require(device_ && device_config_ && audio_running() && audio_name_!="Offline clock (no sound)",
            "Connect the intended audio device before activating a Live channel");
    require(cancel_staged_playback(),"Finish the pending Playback transition first");
    require(channels.size()<=64,"Live supports at most 64 input channels");
    const auto before=services_.projects->state().project;
    auto candidate=*before;
    const auto ownedTrack=[&](const Id& id){return std::find(owned.begin(),owned.end(),id)!=owned.end();};
    require(std::none_of(candidate.clips.begin(),candidate.clips.end(),[&](const auto& clip){return ownedTrack(clip.track);}),
            "A Live input cannot own Playback clips");
    std::erase_if(candidate.tracks,[&](const auto& track){return ownedTrack(track.id);});
    std::vector<Id> changed;
    for(auto& track:channels) {
        require(track.kind==TrackKind::audio || track.kind==TrackKind::instrument,"Unsupported Live channel type");
        require(std::none_of(candidate.tracks.begin(),candidate.tracks.end(),[&](const auto& t){return t.id==track.id;}),
                "Duplicate Live channel or Playback ID collision");
        const auto old=std::find_if(before->tracks.begin(),before->tracks.end(),[&](const auto& t){return t.id==track.id;});
        if(old==before->tracks.end() || old->kind!=track.kind || old->inserts!=track.inserts)
            changed.push_back(track.id);
        candidate.tracks.push_back(std::move(track));
    }
    candidate.validate();
    require(candidate.tracks.size()<=audio::max_mixer_tracks,"Live + Playback exceeds the mixer channel budget");
    if(candidate==*before)return;
    require(device_info_.has_value(),"Audio device metadata unavailable");
    auto config=*device_config_;config.inputs=selected_inputs(candidate);
    audio::validate_device_config(*device_info_,config);validate_hardware(candidate,config);
    const auto oldConfig=*device_config_;
    const bool reopen=config.inputs!=oldConfig.inputs;
    class Projection final:public ICommand {
        Project next_;public:explicit Projection(Project p):next_(std::move(p)){}
        std::string_view name()const override{return "Configure Live channels";}
        void apply(Project& p)const override{p=next_;}
    };
    device_->stop();engine_->quiesce();
    const auto head=engine_->state();auto rollback=engine_->stopped_graph();
    auto oldRuntime=insert_runtime_;auto oldMixer=mixer_tracks_;auto oldPrepared=prepared_;
    bool closed=false;
    try {
        capture_insert_state(candidate,{},true);
        for(const auto& track:candidate.tracks) {
            if(std::find(changed.begin(),changed.end(),track.id)==changed.end())continue;
            std::shared_ptr<processing::PreparedGraph> graph;
            if(!track.inserts.empty())graph=std::make_shared<processing::PreparedGraph>(
                processing::GraphSnapshot{std::make_shared<const processing::GraphState>(processing::insert_graph(track.inserts)),0,false,false},
                processing::ProcessConfig{config.sample_rate,static_cast<std::uint32_t>(config.outputs.size()),config.buffer_frames,config.buffer_frames,track.kind==TrackKind::instrument}
#ifdef MRS_HAS_VST3
                ,processing::hosted_factory
#endif
            );
            insert_runtime_[track.id.value]={std::move(graph),{}};
        }
        std::erase_if(insert_runtime_,[&](const auto& entry){return !entry.first.empty() &&
            std::none_of(candidate.tracks.begin(),candidate.tracks.end(),[&](const auto& track){return track.id.value==entry.first;});});
        insert_runtime_.try_emplace(std::string{},InsertRuntime{});
        preparing_project_=&candidate;auto graph=render(config,true);preparing_project_=nullptr;
        Projection command{candidate};auto transaction=services_.projects->prepare_transaction(&command);
        require(static_cast<bool>(transaction),"Live channel transaction is empty");
        if(reopen){device_->close();closed=true;}
        for(const auto& track:candidate.tracks)if(track.kind==TrackKind::instrument) {
            const auto& runtime=insert_runtime_.at(track.id.value);if(runtime.graph)runtime.graph->panic();
        }
        engine_->prepare({config.sample_rate,static_cast<std::uint32_t>(config.inputs.size()),static_cast<std::uint32_t>(config.outputs.size()),8192,config.buffer_frames,config.processing_workers},std::move(graph),head);
        if(reopen)device_->open(config,engine_);
        try {
            transaction->commit();device_config_=config;device_->start();transaction->finish();
        } catch(...) {transaction->rollback();throw;}
    } catch(...) {
        const auto error=std::current_exception();preparing_project_=nullptr;
        try {
            device_->stop();engine_->quiesce();
            insert_runtime_=std::move(oldRuntime);mixer_tracks_=std::move(oldMixer);prepared_=std::move(oldPrepared);
            if(closed)device_->close();
            engine_->prepare({oldConfig.sample_rate,static_cast<std::uint32_t>(oldConfig.inputs.size()),static_cast<std::uint32_t>(oldConfig.outputs.size()),8192,oldConfig.buffer_frames,oldConfig.processing_workers},std::move(rollback),head);
            if(closed)device_->open(oldConfig,engine_);
            device_config_=oldConfig;device_->start();
        }catch(...){disconnect();}
        std::rethrow_exception(error);
    }
    sync_arm();publish_midi_routes();unsaved_=true;
}
void Application::set_midi_input(const Id& id,std::string port,int channel,bool monitor){
    require_not_recording();
    const SetMidiInput command{id,std::move(port),channel,monitor};auto candidate=*services_.projects->state().project;
    command.apply(candidate);candidate.validate();services_.projects->execute(command);
    publish_mix();publish_midi_routes();
}
void Application::set_midi_output(const Id& id,std::string port,int channel){edit(SetMidiOutput{id,std::move(port),channel},{},true);publish_midi_routes();}
void Application::reconnect_midi(){require_not_recording();require_not_playing();midi_inputs_.reset();midi_route_revision_=~std::uint64_t{};rebuild_audio(true);if(midi_outputs_)midi_outputs_->reconnect();publish_midi_routes();}
std::string Application::midi_output_status(const Id& id)const{
    const auto p=services_.projects->state().project;const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& track){return track.id==id;});
    if(t==p->tracks.end()||t->midi_output.empty())return "MIDI out: off";
    const auto channel=t->midi_output_channel<0?std::string{"Original"}:std::to_string(t->midi_output_channel+1);
    return (device_config_?(midi_outputs_?midi_outputs_->status(t->midi_output):"MIDI out: opening"):"MIDI out: audio disconnected")+" / "+channel;
}
std::string Application::midi_status(const Id& id)const{
    const auto p=services_.projects->state().project;const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& track){return track.id==id;});
    if(t==p->tracks.end()||t->kind!=TrackKind::instrument)return {};
    if(!t->midi_output.empty())return midi_output_status(id)+" | "+(t->midi_input.empty()?"Input off":t->midi_monitor?"Thru on":"Thru off");
    if(auto error=instrument_errors_.find(id.value);error!=instrument_errors_.end())return "Instrument unavailable: "+error->second;
    if(t->inserts.empty())return "Choose instrument in Mix";
    if(!t->midi_monitor&&!track_armed(id))return "MIDI: monitor off";
    if(t->midi_input.empty())return "MIDI: off";
    auto name=t->midi_input;
    if(name.starts_with("winmm:")){auto begin=std::size_t{0};for(int part=0;part<3&&begin!=std::string::npos;++part){const auto colon=name.find(':',begin);begin=colon==std::string::npos?colon:colon+1;}const auto end=name.rfind(':');if(begin!=std::string::npos&&end>=begin)name=name.substr(begin,end-begin);}
    const auto channel=t->midi_channel<0?std::string{"All"}:std::to_string(t->midi_channel+1);
    if(!device_config_)return "MIDI: "+name+" / "+channel+" | audio disconnected";
    return (midi_inputs_?midi_inputs_->status(t->midi_input):"MIDI: opening")+" | "+name+" / "+channel;
}
void Application::publish_midi_routes(){
    const auto snapshot=services_.projects->state();const auto generation=engine_->midi_generation();
    if(snapshot.revision==midi_route_revision_&&generation==midi_route_generation_)return;
    std::vector<MidiInputRoute> routes;std::vector<MidiOutputRoute> outputs;std::size_t index=0;
    for(const auto& track:snapshot.project->tracks)if(track.kind!=TrackKind::midi){if(track.kind==TrackKind::instrument&&!track.midi_input.empty())routes.push_back({track.midi_input,index,track.midi_channel});if(!track.midi_output.empty())outputs.push_back({track.midi_output,index,track.midi_output_channel});++index;}
    if(!device_config_){routes.clear();outputs.clear();}
    if(!routes.empty()&&!midi_inputs_)midi_inputs_=std::make_unique<MidiInputs>(engine_);
    if(midi_inputs_)midi_inputs_->routes(std::move(routes),generation);
    if(!outputs.empty()&&!midi_outputs_)midi_outputs_=std::make_unique<MidiOutputs>(external_midi_);
    if(midi_outputs_)midi_outputs_->routes(std::move(outputs),generation);
    midi_route_revision_=snapshot.revision;midi_route_generation_=generation;
}
Id Application::add_bus(std::string name) {
    require(!name.empty() && name.size() <= 4096,"enter a bus name");
    auto id = new_id(); edit(AddTrack{{id,std::move(name),TrackKind::bus,{}}}); return id;
}
void Application::set_track_output(const Id& id, std::optional<Id> output) { edit(SetTrackOutput{id,std::move(output)}); }
Id Application::add_fx_channel(std::string name) {
    require(!name.empty()&&name.size()<=4096,"enter an FX name");
    Track track{new_id(),std::move(name),TrackKind::bus,{}};track.fx_channel=true;
    const auto id=track.id;edit(AddTrack{std::move(track)});return id;
}
Id Application::add_return_send(const Id& source, std::string name) {
    require(!name.empty() && name.size() <= 4096,"enter a return name");
    class CreateReturn final : public ICommand {
    public:
        Id source; Track bus;
        CreateReturn(Id from, Track destination) : source(std::move(from)), bus(std::move(destination)) {bus.fx_channel=true;}
        std::string_view name() const override { return "Create return and send"; }
        void apply(Project& p) const override {
            auto t = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& track) { return track.id == source; });
            require(t != p.tracks.end() && t->kind != TrackKind::midi,"send requires an audio channel");
            t->sends.push_back({bus.id,1,false}); p.tracks.push_back(bus);
        }
    };
    const auto id = new_id(); edit(CreateReturn{source,{id,std::move(name),TrackKind::bus,{}}}); return id;
}
void Application::edit_track_folders(const ICommand& command){
    require_not_recording();const auto before=services_.projects->state().project;auto candidate=*before;
    command.apply(candidate);candidate.validate();if(candidate==*before)return;auto presentation=candidate;presentation.folders=before->folders;
    if(presentation.tracks.size()==before->tracks.size())for(std::size_t i=0;i<presentation.tracks.size();++i)presentation.tracks[i].folder=before->tracks[i].folder;
    if(presentation==*before){auto document=snapshot();document.project=candidate;document.validate();services_.projects->execute(command);}
    else edit(command,{},true); // routing changes use the stopped/quiesced graph lifecycle
}
Id Application::create_track_folder(std::string name,const std::vector<Id>& tracks){
    Folder folder;folder.id=new_id();folder.name=std::move(name);edit_track_folders(CreateTrackFolder{folder,tracks});return folder.id;
}
void Application::move_tracks_to_folder(const std::vector<Id>& tracks,std::optional<Id> folder){edit_track_folders(MoveTracksToFolder{tracks,std::move(folder)});}
void Application::update_track_folder(const Id& id,std::string name,bool collapsed){edit_track_folders(UpdateTrackFolder{id,std::move(name),collapsed});}
void Application::remove_track_folder(const Id& id){edit_track_folders(RemoveTrackFolder{id});}
void Application::link_folder_bus(const Id& id,std::optional<Id> bus,bool create){
    std::optional<Track> channel;
    if(create){const auto p=services_.projects->state().project;const auto f=std::find_if(p->folders.begin(),p->folders.end(),[&](const auto& item){return item.id==id;});require(f!=p->folders.end(),"Unknown folder");
        channel.emplace();channel->id=new_id();channel->name=f->name+" Bus";channel->kind=TrackKind::bus;channel->folder=f->parent;bus=channel->id;}
    edit_track_folders(LinkFolderBus{id,std::move(bus),std::move(channel)});
}
void Application::remove_track(const Id& id) { edit(RemoveTrack{id}); }
void Application::reorder_track(const Id& id, std::size_t index) {
    require_not_recording();auto candidate=*services_.projects->state().project;
    ReorderTrack{id,index}.apply(candidate);if(candidate==*services_.projects->state().project)return;
    edit(ReorderTrack{id,index},{},true);
}
void Application::set_track_color(const Id& id,std::optional<std::uint32_t> color){
    require_not_recording();require(!color||*color<=0xFFFFFF,"Invalid channel RGB");
    struct Color final:ICommand{Id id;std::optional<std::uint32_t> color;std::string_view name()const override{return "Channel color";}void apply(Project& p)const override{auto t=std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& t){return t.id==id;});require(t!=p.tracks.end(),"Unknown channel");t->color=color;}} command;
    command.id=id;command.color=color;auto candidate=*services_.projects->state().project;command.apply(candidate);candidate.validate();
    if(candidate!=*services_.projects->state().project)services_.projects->execute(command);
}
bool Application::history_preserves_plugins(bool redo) const {
    if(history_changes_only_inputs(redo))return true;
    const auto target=services_.projects->history_target(redo);if(!target)return true;
    if(channel_order_only(*services_.projects->state().project,*target))return true;
    if(channel_color_only(*services_.projects->state().project,*target))return true;
    if(insert_order_only(*services_.projects->state().project,*target))return true;
    if(insert_parameters_only(*services_.projects->state().project,*target))return true;
    const auto name=services_.projects->history_name(redo);
    if(name=="Audio clip fades"){auto prior=*services_.projects->state().project;if(prior.clips.size()==target->clips.size())for(std::size_t i=0;i<prior.clips.size();++i){prior.clips[i].fade_in=target->clips[i].fade_in;prior.clips[i].fade_out=target->clips[i].fade_out;}if(prior==*target)return true;}
    if(name=="Edit MIDI notes"||name=="Musical note edit"||name=="Delete clip"||name=="Remove selected clips"||name=="Split MIDI clip"||name=="Merge MIDI clips"||name=="Duplicate clip"||name=="Split selected clips"||name=="Resize selected clips"||name.starts_with("AI proposal (")){auto prior=*services_.projects->state().project,after=*target;std::erase_if(prior.clips,[](const auto& c){return c.midi.has_value();});std::erase_if(after.clips,[](const auto& c){return c.midi.has_value();});prior.markers=after.markers;if(prior==after)return true;}
    auto time_before=*services_.projects->state().project;time_before.time=target->time;if(time_before==*target)return true;
    auto before=*services_.projects->state().project;before.folders=target->folders;
    if(before.tracks.size()==target->tracks.size())for(std::size_t i=0;i<before.tracks.size();++i)before.tracks[i].folder=target->tracks[i].folder;
    before.chords=target->chords;before.markers=target->markers;before.sections=target->sections;
    if(before.clips.size()==target->clips.size())for(std::size_t i=0;i<before.clips.size();++i){before.clips[i].color=target->clips[i].color;before.clips[i].name=target->clips[i].name;}
    return before==*target;
}
bool Application::history_changes_only_inputs(bool redo) const {
    const auto target=services_.projects->history_target(redo);if(!target)return true;
    auto before=*services_.projects->state().project,after=*target;
    for(auto& t:before.tracks){t.input_monitor=false;t.midi_input.clear();t.midi_channel=-1;t.midi_monitor=false;}
    for(auto& t:after.tracks){t.input_monitor=false;t.midi_input.clear();t.midi_channel=-1;t.midi_monitor=false;}
    return before==after;
}
namespace {
bool section_ranges_changed(const Project& a,const Project& b){if(a.sections.size()!=b.sections.size())return true;for(std::size_t i=0;i<a.sections.size();++i)if(a.sections[i].id!=b.sections[i].id||a.sections[i].start!=b.sections[i].start||a.sections[i].end!=b.sections[i].end)return true;return false;}
}
bool Application::history(bool redo) {
    require_not_recording();
    const auto target = services_.projects->history_target(redo);
    if (!target) return false;
    auto before = *services_.projects->state().project, after = *target;
#ifdef MRS_HAS_ARA
    if(before.ara_bindings!=after.ara_bindings){
        require_not_playing();auto transaction=services_.projects->prepare_transaction(nullptr,redo);if(!transaction)return false;
        if(!ara_){ara_=std::make_unique<ara::Arrangement>();ara_->set_preparation(ara_cancel_,ara_wait_);ara_->prefer_live(true);}
        try{ara_->restore_bindings(after.ara_bindings);transaction->commit();rebuild_audio(true);transaction->finish();return true;}
        catch(...){transaction->rollback();try{synchronize_ara();rebuild_audio(true);}catch(const std::exception& e){waveform_error_=std::string("ARA history rollback: ")+e.what();}throw;}
    }
#endif
    if(before!=after&&channel_order_only(before,after)){
        require_not_playing();const bool changed=redo?services_.projects->redo():services_.projects->undo();
        if(changed){sync_arm();rebuild_audio(true);}return changed;
    }
    if(before!=after&&insert_order_only(before,after)&&!insert_runtime_.empty()){
        require_not_playing();
        struct Order {std::shared_ptr<processing::PreparedGraph> graph;std::vector<Id> before,after;};std::vector<Order> orders;
        const auto add=[&](std::optional<Id> id){const auto& old=insert_chain(before,id);const auto& next=insert_chain(after,id);if(old==next)return;
            const auto at=insert_runtime_.find(id?id->value:std::string{});require(at!=insert_runtime_.end()&&at->second.graph,"Missing insert runtime");
            Order order{at->second.graph,{},{}};for(const auto& fx:old)order.before.push_back(fx.id);for(const auto& fx:next)order.after.push_back(fx.id);orders.push_back(std::move(order));};
        add({});for(const auto& t:before.tracks)if(t.kind!=TrackKind::midi)add(t.id);
        if(device_)device_->stop();
        engine_->quiesce();std::size_t applied{};
        try{for(auto& order:orders){order.graph->reorder_serial(order.after);++applied;}
            const bool changed=redo?services_.projects->redo():services_.projects->undo();rebuild_audio(true);return changed;
        }catch(...){if(*services_.projects->state().project==before){while(applied){auto& order=orders[--applied];order.graph->reorder_serial(order.before);}if(device_)device_->start();}throw;}
    }
    auto midi_before=before,midi_after=after;midi_before.markers=midi_after.markers;
    std::erase_if(midi_before.clips,[](const auto& clip){return clip.midi.has_value();});std::erase_if(midi_after.clips,[](const auto& clip){return clip.midi.has_value();});
    const auto historyName=services_.projects->history_name(redo);
    const bool midiEdit=historyName=="Delete clip"||historyName=="Remove selected clips"||historyName=="Split MIDI clip"||historyName=="Merge MIDI clips"||historyName=="Duplicate clip"||historyName=="Split selected clips"||historyName=="Resize selected clips";
    if((historyName.starts_with("AI proposal (")||midiEdit)&&midi_before==midi_after&&before.clips!=after.clips)return edit_timing(after,nullptr,redo,true,midiEdit);
    auto time_before=before;time_before.time=after.time;if(time_before==after&&before.time!=after.time)return edit_timing(after,nullptr,redo);
    // Chord/marker metadata has no DSP/MIDI/transport effect. Preserve processor state,
    // editors, playback and active loop for its history just as for its commands.
    auto metadata_before = before;
    metadata_before.folders = after.folders;
    metadata_before.chords = after.chords;
    metadata_before.markers = after.markers;metadata_before.sections=after.sections;
    if(metadata_before.tracks.size()==after.tracks.size())for(std::size_t i=0;i<after.tracks.size();++i){metadata_before.tracks[i].color=after.tracks[i].color;metadata_before.tracks[i].folder=after.tracks[i].folder;}
    if(metadata_before.clips.size()==after.clips.size())for(std::size_t i=0;i<after.clips.size();++i){metadata_before.clips[i].color=after.clips[i].color;metadata_before.clips[i].name=after.clips[i].name;}
    if (metadata_before == after){
        if(section_ranges_changed(before,after)){require_not_playing();require(!engine_->state().count_remaining,"End count-in before section history");require(!engine_->state().loop,"Turn off loop before section history");}
        auto document=snapshot();document.project=after;document.validate();
        return redo ? services_.projects->redo() : services_.projects->undo();
    }
    auto clip_before=before,clip_after=after;clip_before.clips.clear();clip_after.clips.clear();const bool clips_only=clip_before==clip_after;
    before.master_gain = after.master_gain = 1;
    const bool partitioned=device_config_&&device_config_->process_buffer_frames&&device_config_->processing_workers>=2&&audio_name_!="Offline clock (no sound)";
    auto route_before=before,route_after=after;
    for(auto& t:route_before.tracks){t.input_monitor=false;t.midi_input.clear();t.midi_channel=-1;t.midi_monitor=false;}
    for(auto& t:route_after.tracks){t.input_monitor=false;t.midi_input.clear();t.midi_channel=-1;t.midi_monitor=false;}
    const bool input_only=route_before==route_after;
    for (auto& t : before.tracks) { t.mix = {}; if(!partitioned||track_armed(t.id))t.input_monitor=false;t.midi_input.clear();t.midi_channel=-1;t.midi_monitor=false; for (auto& send : t.sends) {send.gain = 1;send.bypass=false;} }
    for (auto& t : after.tracks) { t.mix = {}; if(!partitioned||track_armed(t.id))t.input_monitor=false;t.midi_input.clear();t.midi_channel=-1;t.midi_monitor=false; for (auto& send : t.sends) {send.gain = 1;send.bypass=false;} }
    bool inserts_only=true;
    if(!parameter_only(before.master_inserts,after.master_inserts))inserts_only=false;
    before.master_inserts=after.master_inserts;
    if(before.tracks.size()!=after.tracks.size())inserts_only=false;
    else for(std::size_t i=0;i<before.tracks.size();++i){if(!parameter_only(before.tracks[i].inserts,after.tracks[i].inserts))inserts_only=false;before.tracks[i].inserts=after.tracks[i].inserts;}
    bool mix_only = inserts_only && before == after;
    if (mix_only && device_config_)for(const auto input:selected_inputs(*target))
        if(std::find(device_config_->inputs.begin(),device_config_->inputs.end(),input)==device_config_->inputs.end())mix_only=false;
    if (!mix_only) {
        require_not_playing();
        if (device_config_ && audio_name_ != "Offline clock (no sound)") validate_hardware(*target,*device_config_);
        if (device_ && device_config_ && audio_name_ != "Offline clock (no sound)") {
            auto c = *device_config_; c.inputs = selected_inputs(*target);
            require(device_info_.has_value(),"audio device unavailable"); audio::validate_device_config(*device_info_,c);
        }
    }
    const bool changed = redo ? services_.projects->redo() : services_.projects->undo();
    if (changed) { sync_arm(); if (mix_only) {publish_mix();cancel_insert_preview();publish_midi_routes();} else rebuild_audio(clips_only||input_only); }
    return changed;
}
bool Application::undo() { return history(false); }
bool Application::redo() { return history(true); }
void Application::set_track_mix(const Id& id, Track::Mix mix) {
    services_.projects->execute(SetTrackMix{id,mix}); cancel_mix_preview();
}
void Application::set_master_gain(float gain) {
    services_.projects->execute(SetMasterGain{gain}); cancel_mix_preview();
}
void Application::cancel_mix_preview() {
    applied_mix_revision_ = std::numeric_limits<std::uint64_t>::max(); publish_mix();
}
void Application::publish_mix() {
    const auto state = services_.projects->state();
    if (state.revision == applied_mix_revision_) return;
    audio::MixerUpdate update; update.master_gain = state.project->master_gain;
    for (const auto& t : state.project->tracks) if (t.kind != TrackKind::midi) {
        if (update.count >= mixer_tracks_.size() || mixer_tracks_[update.count] != t.id) return;
        for (std::size_t j=0; j<t.sends.size(); ++j) update.send_gains[update.count][j] = t.sends[j].bypass?0.f:t.sends[j].gain;
        update.midi_monitor[update.count]=t.midi_monitor;update.input_monitoring[update.count]=t.input_monitor; update.tracks[update.count++] = t.mix;
    }
    if (update.count != mixer_tracks_.size()) return;
    // A full queue keeps this revision pending; the next UI poll retries without blocking RT.
    if (engine_->enqueue_mix(update)) applied_mix_revision_ = state.revision;
}
bool Application::preview_mix(std::optional<Id> id, Track::Mix mix, float master) {
    mix.validate();
    audio::MixerUpdate update; update.master_gain = master;
    const auto p = services_.projects->state().project;
    for (const auto& t : p->tracks) if (t.kind != TrackKind::midi) {
        if (update.count >= mixer_tracks_.size() || mixer_tracks_[update.count] != t.id) return false;
        for (std::size_t j=0; j<t.sends.size(); ++j) update.send_gains[update.count][j] = t.sends[j].bypass?0.f:t.sends[j].gain;
        update.midi_monitor[update.count]=t.midi_monitor;update.input_monitoring[update.count]=t.input_monitor; update.tracks[update.count++] = id && t.id == *id ? mix : t.mix;
    }
    return engine_->enqueue_mix(update);
}
bool Application::preview_mixes(const std::vector<std::pair<Id, Track::Mix>>& mixes, float master) {
    // UI-thread preparation; one bounded, coherent update uses the existing RT queue.
    require(std::isfinite(master) && master >= 0 && master <= 16, "invalid master gain");
    const auto p = services_.projects->state().project;
    for (const auto& [id, mix] : mixes) {
        mix.validate();
        if (std::none_of(p->tracks.begin(), p->tracks.end(), [&](const auto& t) { return t.id == id && t.kind != TrackKind::midi; })) return false;
    }
    audio::MixerUpdate update; update.master_gain = master;
    for (const auto& t : p->tracks) if (t.kind != TrackKind::midi) {
        if (update.count >= mixer_tracks_.size() || mixer_tracks_[update.count] != t.id) return false;
        for (std::size_t j=0; j<t.sends.size(); ++j) update.send_gains[update.count][j] = t.sends[j].bypass?0.f:t.sends[j].gain;
        update.midi_monitor[update.count] = t.midi_monitor; update.input_monitoring[update.count] = t.input_monitor;
        auto mix = t.mix;
        for (const auto& entry : mixes) if (entry.first == t.id) { mix = entry.second; break; }
        update.tracks[update.count++] = mix;
    }
    return update.count == mixer_tracks_.size() && engine_->enqueue_mix(update);
}
bool Application::preview_send_gain(const Id& source,const Id& bus,float gain) {
    require(std::isfinite(gain)&&gain>=0&&gain<=16,"invalid send gain");
    audio::MixerUpdate update;const auto p=services_.projects->state().project;update.master_gain=p->master_gain;bool found=false;
    for(const auto& t:p->tracks)if(t.kind!=TrackKind::midi){
        if(update.count>=mixer_tracks_.size()||mixer_tracks_[update.count]!=t.id)return false;
        for(std::size_t j=0;j<t.sends.size();++j){const bool selected=t.id==source&&t.sends[j].bus==bus;found|=selected;update.send_gains[update.count][j]=t.sends[j].bypass?0.f:(selected?gain:t.sends[j].gain);}
        update.midi_monitor[update.count]=t.midi_monitor;update.input_monitoring[update.count]=t.input_monitor;update.tracks[update.count++]=t.mix;
    }
    if(!found||update.count!=mixer_tracks_.size())return false;
    return engine_->enqueue_mix(update);
}
const std::vector<Clip>& Application::playback_sources() const {
    static const std::vector<Clip> empty;
    return services_.projects->state().project->id==playback_source_project_?playback_sources_:empty;
}
Sample Application::source_frames(const Id& id) {
    const auto p = services_.projects->state().project;
    const auto it = std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip) { return clip.id == id; });
    if(it!=p->clips.end())return asset(it->source)->frames();
    const auto& sources=playback_sources();
    const auto source=std::find_if(sources.begin(),sources.end(),[&](const auto& clip){return clip.id==id;});
    require(source!=sources.end(),"unknown clip");return asset(source->source)->frames();
}
void Application::set_playback_regions(const std::vector<PlaybackStem>& stems) {configure_playback_regions(stems,true);}
void Application::validate_playback_regions(const std::vector<PlaybackStem>& stems) {configure_playback_regions(stems,false);}
void Application::configure_playback_regions(const std::vector<PlaybackStem>& stems,bool publish) {
    require_not_playing();require_not_recording();
    const auto current=services_.projects->state().project;
    require(workspace_==Workspace::live && path_.empty() && current->id==playback_source_project_,"Region editing requires prepared Live Playback");
    const bool liveTail=current->tracks.size()==playback_sources_.size()+1 &&
        current->tracks.back().kind==TrackKind::audio && current->tracks.back().input_monitor;
    require(current->tracks.size()==playback_sources_.size() || liveTail,
            "Playback tracks changed; reload the item");
    for(std::size_t i=0;i<playback_sources_.size();++i)require(current->tracks[i].id==playback_sources_[i].track,"Playback source mapping changed");
    const auto& sources=playback_sources();require(stems.size()==sources.size(),"Playback source mapping changed");
    auto nextSources=sources;
    struct Regions final:ICommand {
        std::vector<Clip> clips;
        std::string_view name()const override{return "Set Playback regions";}
        void apply(Project& p)const override{p.clips=clips;}
    } command;
    const auto rate=services_.projects->state().project->sample_rate;
    for(std::size_t i=0;i<stems.size();++i){
        const auto& settings=stems[i];auto& source=nextSources[i];const auto frames=source_frames(source.id);
        require(std::isfinite(settings.begin)&&std::isfinite(settings.end)&&settings.begin>=0&&settings.begin<=static_cast<double>(max_sample)/rate&&settings.end>=-1&&settings.end<=static_cast<double>(max_sample)/rate,"Invalid Playback trim");
        const auto start=static_cast<Sample>(std::llround(settings.begin*rate));
        const auto end=settings.end<0?frames:static_cast<Sample>(std::llround(settings.end*rate));
        require(start>=0&&end>start&&end<=frames,"Playback trim exceeds source WAV");
        source.start=start;source.source_offset=start;source.length=end-start;
        if(!settings.regions){command.clips.push_back(source);continue;}
        for(const auto& region:*settings.regions){
            auto clip=source;clip.id=region.id;clip.start=region.start;clip.length=region.length;clip.source_offset=region.offset;
            require(clip.source_offset>=0&&clip.length>0&&clip.source_offset<=frames&&clip.length<=frames-clip.source_offset,"Playback region exceeds source WAV");
            command.clips.push_back(std::move(clip));
        }
    }
    auto candidate=*services_.projects->state().project;candidate.clips=command.clips;candidate.validate();
    require(candidate.clips.size()<=audio::max_voices,"Too many Playback regions");
    std::size_t streamed{},bytes{};
    for(const auto& clip:candidate.clips){const auto data=asset(clip.source);if(data->file){++streamed;bytes+=8*8192*static_cast<std::size_t>(data->channels)*sizeof(float);}}
    require(streamed<=32 && bytes<=256*1024*1024,"Disk voice budget exceeded (32 voices / 256 MiB)");
    if(!publish)return;
    if(command.clips!=services_.projects->state().project->clips)edit(command);
    playback_sources_=std::move(nextSources);
}
void Application::move_clip(const Id& id, const Id& track, Sample start) {
    const auto p=services_.projects->state().project;const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==id;});require(c!=p->clips.end(),"unknown clip");
    if(c->midi)edit(MoveMidiClip{id,track,Timeline(p->time,p->sample_rate).to_ticks(start)});else edit(MoveAudioClip{id,track,start});
}
void Application::trim_clip(const Id& id, Sample start, Sample end) {
    require_not_playing();const auto p=services_.projects->state().project;const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==id;});require(c!=p->clips.end(),"unknown clip");
    if(c->midi){const Timeline t(p->time,p->sample_rate);edit(TrimMidiClip{id,t.to_ticks(start),t.to_ticks(end)});}else {const auto frames=source_frames(id);edit(TrimAudioClip{id,start,end,frames});}
}
Id Application::split_clip(const Id& id, Sample position) {
    require(services_.projects->state().project->clips.size() < audio::max_voices,"too many playback clips to split");
    const auto p=services_.projects->state().project;const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==id;});require(c!=p->clips.end(),"unknown clip");const auto right=new_id();
    if(c->midi){const SplitMidiClip command{id,Timeline(p->time,p->sample_rate).to_ticks(position),right};auto candidate=*p;command.apply(candidate);edit_timing(std::move(candidate),&command,{},true,true);}else edit(SplitAudioClip{id,position,right});return right;
}
std::vector<Id> Application::split_clips(std::vector<Id> ids,Sample position){
    const auto p=services_.projects->state().project;const Timeline time(p->time,p->sample_rate);
    struct Batch final:ICommand{std::vector<Id> ids,right;Sample position{};std::string_view name()const override{return "Split selected clips";}void apply(Project& p)const override{const Timeline t(p.time,p.sample_rate);for(std::size_t i=0;i<ids.size();++i){const auto c=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==ids[i];});require(c!=p.clips.end(),"Unknown split clip");if(c->midi)SplitMidiClip{ids[i],t.to_ticks(position),right[i]}.apply(p);else SplitAudioClip{ids[i],position,right[i]}.apply(p);}}} command;
    std::set<std::string> seen;bool onlyMidi=true;for(const auto& id:ids){require(seen.insert(id.value).second,"Duplicate split target");const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});require(c!=p->clips.end(),"Unknown split clip");if(position<=clip_start(*c,time)||position>=clip_end(*c,time))continue;if(c->midi){const auto tick=time.to_ticks(position);if(tick<=c->midi->start||tick>=c->midi->start+c->midi->length)continue;}onlyMidi&=c->midi.has_value();command.ids.push_back(id);command.right.push_back(new_id());}
    require(!command.ids.empty(),"Split point must be inside a selected clip");require(p->clips.size()+command.ids.size()<=audio::max_voices,"Too many playback clips");command.position=position;
    if(onlyMidi){auto candidate=*p;command.apply(candidate);edit_timing(std::move(candidate),&command,{},true,true);}else edit(command,{},true);return command.right;
}
void Application::trim_clips(const std::vector<Clip>& expected,Sample delta,bool left){
    require_not_playing();require(!expected.empty(),"Select clips to resize");if(!delta)return;
    struct Batch final:ICommand{std::vector<Clip> expected;std::vector<Sample> frames;Sample delta{};bool left{};std::string_view name()const override{return "Resize selected clips";}void apply(Project& p)const override{const Timeline t(p.time,p.sample_rate);for(std::size_t i=0;i<expected.size();++i){const auto& c=expected[i];require(std::find(p.clips.begin(),p.clips.end(),c)!=p.clips.end(),"Clip changed; retry resize");const auto start=clip_start(c,t),finish=clip_end(c,t);require(left?(delta>=-start&&delta<finish-start):(delta>start-finish&&delta<=max_sample-finish),"Resize exceeds timeline range");const auto begin=start+(left?delta:0),end=finish+(left?0:delta);if(c.midi)TrimMidiClip{c.id,t.to_ticks(begin),t.to_ticks(end)}.apply(p);else TrimAudioClip{c.id,begin,end,frames[i]}.apply(p);}}} command;
    command.expected=expected;command.delta=delta;command.left=left;bool onlyMidi=true;std::set<std::string> seen;for(const auto& c:expected){require(seen.insert(c.id.value).second,"Duplicate resize target");onlyMidi&=c.midi.has_value();command.frames.push_back(c.midi?0:source_frames(c.id));}
    if(onlyMidi){auto candidate=*services_.projects->state().project;command.apply(candidate);edit_timing(std::move(candidate),&command,{},true,true);}else edit(command,{},true);
}
void Application::set_clip_colors(std::vector<Id> ids,std::optional<std::uint32_t> color){
    require(!color||*color<=0xFFFFFF,"Invalid clip RGB");require(!ids.empty(),"Select clips to color");require_not_recording();
    struct Colors final:ICommand{std::vector<Id> ids;std::optional<std::uint32_t> color;std::string_view name()const override{return "Color selected clips";}void apply(Project& p)const override{for(const auto& id:ids){auto c=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});require(c!=p.clips.end(),"Unknown color target");c->color=color;}}} command;command.ids=std::move(ids);command.color=color;
    auto candidate=*services_.projects->state().project;command.apply(candidate);candidate.validate();if(candidate!=*services_.projects->state().project)services_.projects->execute(command);
}
void Application::rename_clips(std::vector<Id> ids,std::string base){
    require_not_recording();require(!ids.empty()&&!base.empty()&&base.size()<=4000,"Enter a clip base name (maximum 4000 bytes)");
    const auto p=services_.projects->state().project;const Timeline time(p->time,p->sample_rate);std::set<std::string> seen;
    std::vector<Clip> selected;for(const auto& id:ids){require(seen.insert(id.value).second,"Duplicate rename target");const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});require(at!=p->clips.end(),"Unknown rename target");selected.push_back(*at);}
    const auto track_index=[&](const Id& id){return std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;})-p->tracks.begin();};
    std::sort(selected.begin(),selected.end(),[&](const auto& a,const auto& b){if(a.track!=b.track)return track_index(a.track)<track_index(b.track);const auto x=clip_start(a,time),y=clip_start(b,time);return x!=y?x<y:a.id.value<b.id.value;});
    struct Rename final:ICommand{std::vector<std::pair<Id,std::string>> names;std::string_view name()const override{return "Rename selected clips";}void apply(Project& p)const override{for(const auto& [id,name]:names){auto at=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});require(at!=p.clips.end(),"Unknown rename target");at->name=name;}}} command;
    for(std::size_t i=0;i<selected.size();++i)command.names.emplace_back(selected[i].id,base+(selected.size()>1?" "+(i<9?std::string{"0"}:std::string{})+std::to_string(i+1):std::string{}));
    auto candidate=*p;command.apply(candidate);candidate.validate();if(candidate!=*p){services_.projects->execute(command);unsaved_=true;}
}
void Application::set_clip_fades(const Clip& expected,Sample fade_in,Sample fade_out){
    require(!expected.midi&&fade_in>=0&&fade_out>=0&&fade_in<=expected.length&&fade_out<=expected.length,"Invalid audio fades");
    if(expected.fade_in==fade_in&&expected.fade_out==fade_out)return;
    struct Fades final:ICommand{Clip expected;Sample in{},out{};Fades(Clip e,Sample a,Sample b):expected(std::move(e)),in(a),out(b){}std::string_view name()const override{return "Audio clip fades";}void apply(Project& p)const override{auto at=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==expected.id;});require(at!=p.clips.end()&&*at==expected,"Clip changed; retry fade gesture");at->fade_in=in;at->fade_out=out;}};
    edit(Fades{expected,fade_in,fade_out},{},true);
}
void Application::split_sections(const std::vector<ArrangerSection>& expected,Tick tick){auto values=expected;bool changed=false;for(std::size_t i=0;i<expected.size();++i)if(expected[i].start<tick&&tick<expected[i].end){values[i].end=tick;auto right=expected[i];right.id=new_id();right.start=tick;values.push_back(right);changed=true;}require(changed,"Split point must be inside a selected section");edit_metadata(EditSectionGroup{expected,std::move(values)});}
void Application::split_chords(const std::vector<Chord>& expected,Tick tick){auto values=expected;bool changed=false;for(std::size_t i=0;i<expected.size();++i)if(expected[i].start<tick&&tick<expected[i].end){values[i].end=tick;auto right=expected[i];right.id=new_id();right.start=tick;values.push_back(right);changed=true;}require(changed,"Split point must be inside a selected chord");edit_metadata(EditChordGroup{expected,std::move(values)});}
void Application::remove_clip(const Id& id) {const auto p=services_.projects->state().project;const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==id;});const RemoveClip command{id};if(c!=p->clips.end()&&c->midi){auto candidate=*p;command.apply(candidate);edit_timing(std::move(candidate),&command,{},true,true);}else edit(command); }
void Application::move_clips(std::vector<Id> ids,Sample delta,int track_delta){
    struct MoveClips final:ICommand{std::vector<Id> ids;Sample delta;int track_delta;std::string_view name()const override{return "Move selected clips";}void apply(Project& p)const override{const Timeline time(p.time,p.sample_rate);for(const auto& id:ids){const auto c=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& clip){return clip.id==id;});require(c!=p.clips.end(),"unknown selected clip");const auto t=std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& track){return track.id==c->track;});const auto index=static_cast<int>(t-p.tracks.begin())+track_delta;require(index>=0&&index<static_cast<int>(p.tracks.size()),"group exceeds track range");const auto start=clip_start(*c,time);require(delta>=-start&&delta<=max_sample-start,"group exceeds timeline range");if(c->midi)MoveMidiClip{id,p.tracks[static_cast<std::size_t>(index)].id,time.to_ticks(start+delta)}.apply(p);else MoveAudioClip{id,p.tracks[static_cast<std::size_t>(index)].id,start+delta}.apply(p);}}} command;require(!ids.empty(),"Select clips to move");command.ids=std::move(ids);command.delta=delta;command.track_delta=track_delta;if(delta||track_delta)edit(command,{},true);
}

void Application::remove_clips(std::vector<Id> ids){struct RemoveClips final:ICommand{std::vector<Id> ids;std::string_view name()const override{return "Remove selected clips";}void apply(Project& p)const override{for(const auto& id:ids)RemoveClip{id}.apply(p);}} command;command.ids=std::move(ids);require(!command.ids.empty(),"Select clips to delete");const auto p=services_.projects->state().project;const bool onlyMidi=std::all_of(command.ids.begin(),command.ids.end(),[&](const auto& id){const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==id;});return c!=p->clips.end()&&c->midi.has_value();});if(onlyMidi){auto candidate=*p;command.apply(candidate);edit_timing(std::move(candidate),&command,{},true,true);}else edit(command);}
Id Application::create_midi_clip(const Id& track,Tick start,Tick length){Clip c;c.id=new_id();c.track=track;c.name="MIDI clip";c.midi=MidiClip{start,length,0,{}};edit(AddMidiClip{c});return c.id;}
bool Application::audition_note(const Id& track,int pitch,int velocity,int channel,bool on,bool soft_release){
    if(recording()||pitch<0||pitch>127||velocity<1||velocity>127||channel<0||channel>15)return false;
    const auto at=std::find(mixer_tracks_.begin(),mixer_tracks_.end(),track);if(at==mixer_tracks_.end())return false;
    return engine_->enqueue_audition_midi({engine_->midi_generation(),static_cast<std::size_t>(at-mixer_tracks_.begin()),{0,on?processing::MidiKind::note_on:processing::MidiKind::note_off,static_cast<std::uint8_t>(channel),static_cast<std::uint8_t>(pitch),static_cast<std::uint8_t>(velocity)},0,soft_release&&!on?70U:0U});
}
void Application::edit_midi_notes(const Id& id,std::vector<Id> selected,const NoteEdit& settings){
    require_not_recording();require_not_playing();const auto p=services_.projects->state().project;const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});require(at!=p->clips.end()&&at->midi.has_value(),"select a MIDI clip");
    if(transform_midi_notes(*at->midi,selected,settings)==at->midi->notes)return;
    edit(EditMidiNotes{id,std::move(selected),settings},{},true);
}
void Application::set_midi_events(const Id& id,std::vector<MidiChannelEvent> events){require_not_recording();require_not_playing();const auto p=services_.projects->state().project;const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});require(at!=p->clips.end()&&at->midi.has_value(),"select a MIDI clip");if(events==at->midi->events)return;edit(SetMidiEvents{id,std::move(events)});}
void Application::set_midi_notes(const Id& id,std::vector<MidiNote> notes){edit(SetMidiNotes{id,std::move(notes)},{},true);}
Id Application::duplicate_clip(const Id& id){const auto duplicate=new_id();const auto p=services_.projects->state().project;const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==id;});const DuplicateClip command{id,duplicate};if(c!=p->clips.end()&&c->midi){auto candidate=*p;command.apply(candidate);edit_timing(std::move(candidate),&command,{},true,true);}else edit(command,{},true);return duplicate;}
Id Application::merge_midi_clips(std::vector<Id> ids){const auto merged=new_id();const MergeMidiClips command{std::move(ids),merged};auto candidate=*services_.projects->state().project;command.apply(candidate);edit_timing(std::move(candidate),&command,{},true,true);return merged;}
std::vector<Id> Application::copy_clips(std::vector<Id> ids,Sample delta,int track_delta){
    struct Copies final:ICommand{std::vector<Id> source,created;Sample delta{};int track_delta{};std::string_view name()const override{return "Copy selected clips";}void apply(Project& p)const override{const Timeline time(p.time,p.sample_rate);for(std::size_t i=0;i<source.size();++i){const auto c=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& item){return item.id==source[i];});require(c!=p.clips.end(),"unknown source clip");const auto t=std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& item){return item.id==c->track;});const auto index=static_cast<int>(t-p.tracks.begin())+track_delta;require(index>=0&&index<static_cast<int>(p.tracks.size()),"copy exceeds track range");const auto start=clip_start(*c,time);require(delta>=-start&&delta<=max_sample-start,"copy exceeds timeline range");DuplicateClip{source[i],created[i],p.tracks[static_cast<std::size_t>(index)].id,start+delta}.apply(p);}}} command;
    require(!ids.empty(),"Select clips to copy");std::set<std::string> unique;for(const auto& id:ids)require(unique.insert(id.value).second,"duplicate source clip");
    command.source=std::move(ids);for(std::size_t i=0;i<command.source.size();++i)command.created.push_back(new_id());command.delta=delta;command.track_delta=track_delta;edit(command,{},true);return command.created;
}
void Application::set_musical_data(TimeMap time,std::vector<Chord> chords,std::vector<ArrangerSection> sections,std::vector<Marker> markers){
    require_not_playing();require_not_recording();require(!engine_->state().loop,"Turn off section loop before replacing musical data");
    const auto p=services_.projects->state().project;
    if(p->time==time&&p->chords==chords&&p->sections==sections&&p->markers==markers)return;
    edit(SetMusicalData{std::move(time),std::move(chords),std::move(sections),std::move(markers)});
}
void Application::set_time_map(TimeMap time){
    const auto p=services_.projects->state().project;const SetMusicalData command{std::move(time),p->chords,p->sections,p->markers};auto candidate=*p;command.apply(candidate);edit_timing(std::move(candidate),&command);
}
void Application::set_meters(const TimeMap& expected,std::vector<MeterPoint> values){
    const UpdateMeterMap command{expected,std::move(values)};auto candidate=*services_.projects->state().project;command.apply(candidate);edit_timing(std::move(candidate),&command);
}
void Application::set_tempos(const TimeMap& expected,std::vector<TempoPoint> values){
    const UpdateTempoMap command{expected,std::move(values)};auto candidate=*services_.projects->state().project;command.apply(candidate);edit_timing(std::move(candidate),&command);
}
bool Application::edit_timing(Project candidate,const ICommand* command,std::optional<bool> history,bool midi,bool allow_loop){
    require_not_playing();require_not_recording();const auto head=engine_->state();
    require(allow_loop||!head.loop,"Turn off loop before editing tempo/meter");require(!head.count_remaining,"End count-in before editing tempo/meter");
    const auto before=services_.projects->state().project;candidate.validate();if(candidate==*before)return false;
    auto only_time=candidate;only_time.time=before->time;
    if(midi){auto prior=*before;std::erase_if(only_time.clips,[](const auto& c){return c.midi.has_value();});std::erase_if(prior.clips,[](const auto& c){return c.midi.has_value();});only_time.markers=prior.markers;require(only_time==prior&&candidate.time==before->time,"MIDI command must only change MIDI clips and markers");require(candidate.clips.size()<=audio::max_voices,"Too many playback clips");}
    else require(only_time==*before,"Timing command must only change the time map");
    auto document=snapshot();document.project=candidate;document.validate();
    const Timeline prepared_time(candidate.time,candidate.sample_rate);(void)prepared_time.musical_position(prepared_time.to_ticks(head.sample));Sample prior=-1;for(const auto& point:candidate.time.tempos){const auto sample=prepared_time.to_samples(point.tick);require(sample>prior,"Tempo points must occupy distinct samples");prior=sample;}
    prior=-1;for(const auto& point:candidate.time.meters){const auto sample=prepared_time.to_samples(prepared_time.to_ticks({point.bar,1,0}));require(sample>prior,"Meter points must occupy distinct samples");prior=sample;}
    std::optional<audio::AudioEngine::TimingPlan> plan;
    bool ara_timing=false;
#ifdef MRS_HAS_ARA
    ara_timing=ara_&&!midi&&audio_name_!="Offline clock (no sound)";
#endif
    if(device_)plan=engine_->prepare_timing(candidate,mixer_tracks_,ara_timing);
    else{std::vector<Id> tracks;for(const auto& t:candidate.tracks)if(t.kind!=TrackKind::midi)tracks.push_back(t.id);(void)audio::compile_midi_clips(candidate,tracks);(void)audio::compile_midi_events(candidate,tracks);}
    auto transaction=services_.projects->prepare_transaction(command,history);require(transaction!=nullptr,"Project store cannot stage timing edits");
    bool swapped=false,stopped=false;
    try{
#ifdef MRS_HAS_ARA
        if(ara_&&!midi){
            ara_->synchronize(candidate,[this](const auto& source){return asset(source);});
            if(plan&&plan->voices){
                audio::RenderGraph preview;preview.voices.swap(*plan->voices);std::size_t at{};
                for(const auto& clip:candidate.clips)if(!clip.midi){auto& voice=preview.voices.at(at++);const auto original=asset(clip.source);
                    if(voice.asset!=original){voice.asset=original;voice.source_offset=clip.source_offset;voice.stream.reset();}}
                ara_->apply(candidate,preview);
                for(auto& voice:preview.voices)if(voice.asset->file&&!voice.stream){voice.stream=std::make_shared<audio::ReadAhead>(voice.asset->file);voice.stream->prime(voice.source_offset+std::clamp(head.sample-voice.start,Sample{0},voice.length-1));}
                plan->voices->swap(preview.voices);
            }
        }
#endif
        if(device_){device_->stop();stopped=true;engine_->quiesce();engine_->swap_timing(*plan,midi);swapped=true;}
        transaction->commit();
        if(device_)device_->start(); // no project observers see an edit whose backend cannot restart
        transaction->finish();
    }catch(...){
        const auto error=std::current_exception();bool reopen=false;
        if(device_&&stopped){try{device_->stop();}catch(...){device_->close();reopen=true;}engine_->quiesce();}
        if(swapped)engine_->swap_timing(*plan,midi);
        transaction->rollback();
#ifdef MRS_HAS_ARA
        if(ara_&&!midi)ara_->synchronize(*before,[this](const auto& source){return asset(source);});
#endif
        if(device_&&stopped){try{if(reopen)device_->open(*device_config_,engine_);device_->start();}catch(const std::exception& recovery){throw std::runtime_error(std::string("Timing edit rolled back; audio could not restart: ")+recovery.what());}}
        std::rethrow_exception(error);
    }
    // After publication the model and engine are committed; projection failures must not undo only audio.
    // MIDI-only edits keep timing/head unchanged. Store publication and the UI
    // poll refresh projections; no throwing work may turn a committed AI edit
    // into a reported failure before its replay ledger is marked.
    if(!midi){transport_->poll();musical_->refresh();}return true;
}
Id Application::add_section(std::string name,Tick start,Tick end,std::uint32_t color){const auto id=new_id();edit_metadata(AddSection{{id,std::move(name),start,end,color}});return id;}
std::vector<Id> Application::transform_sections(const std::vector<ArrangerSection>& expected,Tick delta,bool copy){
    require(!expected.empty(),"Select sections first");auto values=expected;std::vector<Id> ids;
    for(auto& s:values){require(s.start>=0&&s.end>s.start&&s.end<=max_tick&&delta>=-s.start&&delta<=max_tick-s.end,"Section group exceeds timeline range");s.start+=delta;s.end+=delta;if(copy)s.id=new_id();ids.push_back(s.id);}
    // Copy still checks all source snapshots in this same metadata transaction.
    if(copy){values.insert(values.end(),expected.begin(),expected.end());}
    edit_metadata(EditSectionGroup{expected,std::move(values)});return ids;
}
void Application::remove_sections(const std::vector<ArrangerSection>& expected){
    require_not_recording();for(const auto& s:expected)require(std::none_of(document_.section_patches.begin(),document_.section_patches.end(),[&](const auto& b){return b.section==s.id;}),"Section is linked to a Live patch; remove the binding before deleting the group");
    edit_metadata(EditSectionGroup{expected,{}});
}
Id Application::copy_section(const ArrangerSection& expected,Tick start){const auto id=new_id();edit_metadata(CopySection{expected,id,start});return id;}
void Application::update_section(const ArrangerSection& expected,ArrangerSection value){edit_metadata(UpdateSection{expected,std::move(value)});}
void Application::remove_section(const ArrangerSection& expected){
    require_not_recording();
    require(std::none_of(document_.section_patches.begin(),document_.section_patches.end(),[&](const auto& binding){return binding.section==expected.id;}),
        "Section is linked to a Live patch; remove the binding before deleting the section");
    edit_metadata(RemoveSection{expected});
}
void Application::edit_metadata(const ICommand& command,bool staged) {
    require_not_recording();
    const auto before = services_.projects->state().project;
    auto candidate = *before;
    command.apply(candidate); candidate.validate();
    if (candidate == *before) return;
    auto unchanged = candidate; unchanged.chords = before->chords; unchanged.markers = before->markers;unchanged.sections=before->sections;
    require(unchanged == *before, "Metadata command must only edit musical lanes");
    if(section_ranges_changed(*before,candidate)){require_not_playing();require(!engine_->state().count_remaining,"End count-in before editing section bounds");require(!engine_->state().loop,"Turn off loop before editing section bounds");}
    auto document=snapshot();document.project=candidate;document.validate();
    // Control-thread ProjectStore publication refreshes the shared musical service.
    // No plugin capture, graph preparation, device restart or audio mailbox update.
    if(staged){auto transaction=services_.projects->prepare_transaction(&command);require(transaction!=nullptr,"Project store cannot stage AI edits");transaction->commit();try{transaction->finish();}catch(...){transaction->rollback();throw;}}
    else services_.projects->execute(command);
}
Id Application::add_chord(std::string symbol, Tick start, Tick end) {
    const auto id = new_id();
    edit_metadata(AddChord{{id,std::move(symbol),start,end}}); return id;
}
void Application::update_chord(const Chord& expected, Chord value) {
    edit_metadata(UpdateChord{expected,std::move(value)});
}
void Application::remove_chord(const Chord& expected) { edit_metadata(RemoveChord{expected}); }
std::vector<Id> Application::transform_chords(const std::vector<Chord>& expected,Tick delta,bool copy){
    require(!expected.empty(),"Select chords first");auto values=expected;std::vector<Id> ids;
    for(auto& s:values){require(s.start>=0&&s.end>s.start&&s.end<=max_tick&&delta>=-s.start&&delta<=max_tick-s.end,"Chord group exceeds timeline range");s.start+=delta;s.end+=delta;if(copy)s.id=new_id();ids.push_back(s.id);}
    if(copy)values.insert(values.end(),expected.begin(),expected.end());
    edit_metadata(EditChordGroup{expected,std::move(values)});return ids;
}
void Application::remove_chords(const std::vector<Chord>& expected){require_not_recording();edit_metadata(EditChordGroup{expected,{}});}
std::vector<Id> Application::transform_markers(const std::vector<Marker>& expected,Tick delta,bool copy){
    require(!expected.empty(),"Select markers first");auto values=expected;std::vector<Id> ids;
    for(auto& s:values){require(s.tick>=0&&s.tick<=max_tick&&delta>=-s.tick&&delta<=max_tick-s.tick,"Marker group exceeds timeline range");s.tick+=delta;if(copy)s.id=new_id();ids.push_back(s.id);}
    if(copy)values.insert(values.end(),expected.begin(),expected.end());
    edit_metadata(EditMarkerGroup{expected,std::move(values)});return ids;
}
void Application::remove_markers(const std::vector<Marker>& expected){require_not_recording();for(const auto& s:expected)require(std::none_of(document_.actions.begin(),document_.actions.end(),[&](const auto& a){return a.marker==s.id;}),"Marker is linked to a LiveAction; remove the binding before deleting the group");edit_metadata(EditMarkerGroup{expected,{}});}
Id Application::copy_chord(const Chord& expected, Tick start) {
    const auto id = new_id(); edit_metadata(CopyChord{expected,id,start}); return id;
}
Id Application::add_marker(std::string name,Tick tick,MarkerKind kind) {
    const auto id=new_id();edit_metadata(AddMarker{{id,std::move(name),tick,kind}});return id;
}
void Application::update_marker(const Marker& expected,Marker value) { edit_metadata(UpdateMarker{expected,std::move(value)}); }
void Application::remove_marker(const Marker& expected) {
    require_not_recording();
    require(std::none_of(document_.actions.begin(),document_.actions.end(),[&](const auto& action){return action.marker==expected.id;}),
        "Marker is linked to a LiveAction; remove the binding before deleting the marker");
    edit_metadata(RemoveMarker{expected});
}
std::vector<Id> Application::import_midi(const std::vector<std::filesystem::path>& paths,std::optional<Id> target,Tick start){
    require_not_playing();require_not_recording();require(!paths.empty()&&paths.size()<=128,"Select 1..128 MIDI files");require(start>=0&&start<=max_tick,"Invalid MIDI drop position");
    const auto current=services_.projects->state().project;
    if(target){const auto t=std::find_if(current->tracks.begin(),current->tracks.end(),[&](const auto& t){return t.id==*target;});require(t!=current->tracks.end()&&t->kind==TrackKind::instrument,"Drop MIDI on an instrument track or empty area");}
    auto candidate=*current;std::vector<Id> clips;bool usedTarget=false;
    for(const auto& file:paths){auto parts=load_midi_file(file);for(auto& part:parts){
        const auto id=target&&!usedTarget?*target:new_id();if(!target||usedTarget)candidate.tracks.push_back({id,utf8(file.stem())+" — "+part.name,TrackKind::instrument,{}});usedTarget=true;
        require(part.midi.length<=max_tick-start,"MIDI drop exceeds timeline range");part.midi.start=start;
        Clip c;c.id=new_id();c.track=id;c.name=utf8(file.stem())+" — "+part.name;c.midi=std::move(part.midi);clips.push_back(c.id);candidate.clips.push_back(std::move(c));
    }}
    candidate.validate();
    class Import final:public ICommand {Project p_;public:explicit Import(Project p):p_(std::move(p)){}std::string_view name()const override{return "Import MIDI";}void apply(Project& p)const override{p=p_;}};
    require(candidate.clips.size()<=audio::max_voices,"Too many playback clips");
    std::vector<Id> tracks;for(const auto& t:candidate.tracks)if(t.kind!=TrackKind::midi)tracks.push_back(t.id);
    require(tracks.size()<=audio::max_mixer_tracks,"Too many mixer tracks");(void)audio::compile_midi_clips(candidate,tracks);(void)audio::compile_midi_events(candidate,tracks);
    if(device_){
        // Bootstrap a document opened without processors, then retain every existing chain.
        if(!insert_runtime_.contains(std::string{}))rebuild_audio();
        const auto c=*device_config_;device_->stop();engine_->quiesce();const auto position=engine_->state();std::vector<std::string> added;
        try {
            capture_insert_state(candidate,{},true);
            for(const auto& t:candidate.tracks)if(t.kind==TrackKind::instrument&&!insert_runtime_.contains(t.id.value)){insert_runtime_.emplace(t.id.value,InsertRuntime{});added.push_back(t.id.value);}
            for(const auto& t:current->tracks)if(t.kind==TrackKind::instrument){const auto& runtime=insert_runtime_.at(t.id.value);if(runtime.graph)runtime.graph->panic();}
            preparing_project_=&candidate;audio::RenderGraph graph;
            if(audio_name_=="Offline clock (no sound)"){prepare_mixer(graph);prepare_midi_clips(graph);prepare_inserts(graph,c,true);}else graph=render(c,true);
            engine_->prepare({c.sample_rate,static_cast<std::uint32_t>(c.inputs.size()),static_cast<std::uint32_t>(c.outputs.size()),8192,c.buffer_frames,c.processing_workers},std::move(graph),position);preparing_project_=nullptr;
            device_->start();
        }catch(...){const auto error=std::current_exception();preparing_project_=nullptr;device_->stop();for(const auto& key:added)insert_runtime_.erase(key);try{rebuild_audio(true);}catch(...){disconnect();}std::rethrow_exception(error);}
    }
    services_.projects->execute(Import{std::move(candidate)});unsaved_=true;sync_arm();publish_midi_routes();return clips;
}
void Application::prepare_mixer(audio::RenderGraph& result) {
    const auto project = preparing_project_?preparing_project_:services_.projects->state().project.get();
    mixer_tracks_.clear();
    for (const auto& t : project->tracks) if (t.kind != TrackKind::midi) {
        require(mixer_tracks_.size() < audio::max_mixer_tracks,"mixer supports up to 128 audio tracks and buses");
        mixer_tracks_.push_back(t.id); result.mixer.push_back(t.mix); result.input_monitoring.push_back(t.input_monitor); result.buses.push_back(t.kind == TrackKind::bus);result.live_midi.push_back(t.kind==TrackKind::instrument);result.midi_monitor.push_back(t.midi_monitor);
        result.external_midi_tracks.push_back(!t.midi_output.empty());result.external_midi_channels.push_back(t.midi_output_channel);if(!t.midi_output.empty())result.external_midi=external_midi_;
    }
    for (const auto& t : project->tracks) if (t.kind != TrackKind::midi) {
        const auto destination = t.output ? std::find(mixer_tracks_.begin(),mixer_tracks_.end(),*t.output) : mixer_tracks_.end();
        require(!t.output || destination != mixer_tracks_.end(),"missing output bus");
        result.sends.emplace_back();
        for (const auto& send : t.sends) {
            const auto bus = std::find(mixer_tracks_.begin(),mixer_tracks_.end(),send.bus);
            require(bus != mixer_tracks_.end(),"missing send bus");
            result.sends.back().push_back({static_cast<std::size_t>(bus-mixer_tracks_.begin()),send.bypass?0.f:send.gain,send.pre_fader});
        }
        result.outputs.push_back(destination == mixer_tracks_.end() ? audio::no_mixer_track : static_cast<std::size_t>(destination-mixer_tracks_.begin()));
    }
    result.master_gain = project->master_gain;
    applied_mix_revision_ = services_.projects->state().revision;
}
void Application::prepare_midi_clips(audio::RenderGraph& graph){
    const auto p=preparing_project_?preparing_project_:services_.projects->state().project.get();const Timeline time(p->time,p->sample_rate);graph.click_time=p->time;graph.click=click_settings_;graph.midi_events=audio::compile_midi_events(*p,mixer_tracks_);graph.midi_notes=audio::compile_midi_clips(*p,mixer_tracks_);
    for(const auto& tempo:p->time.tempos)graph.tempos.push_back({time.to_samples(tempo.tick),tempo.bpm,static_cast<double>(tempo.tick)/ppq});
}
audio::RenderGraph Application::render(const audio::DeviceConfig& c,bool retain_inserts) {
    const auto project=preparing_project_?preparing_project_:services_.projects->state().project.get();
    require(c.sample_rate == project->sample_rate,"device/project rate mismatch; choose the project rate (resampling is a later stage)");
    require(!c.outputs.empty() && c.outputs.size() <= audio::max_channels && c.inputs.size() <= audio::max_channels,"invalid channel selection");
    validate_hardware(*project,c);
#ifdef MRS_HAS_ARA
    synchronize_ara(project);
#endif
    audio::RenderGraph result;
    result.performance=performance_plan_;result.performance_token=performance_token_;
    result.click=click_settings_;result.click_time=project->time;
    if(recording())configure_count(result,*project,click_settings_,engine_->state().sample);
    for (const auto& capture : captures_) result.recordings.push_back(capture.recorder);
    for(const auto& capture:midi_captures_){const auto at=std::find(mixer_tracks_.begin(),mixer_tracks_.end(),capture.track);require(at!=mixer_tracks_.end(),"missing MIDI record track");result.midi_recordings.push_back({static_cast<std::size_t>(at-mixer_tracks_.begin()),capture.recorder});}
    result.monitoring = monitoring_;
    const auto config = processing::ProcessConfig{c.sample_rate,static_cast<std::uint32_t>(c.outputs.size()),8192,c.buffer_frames};
    prepared_ = std::make_shared<processing::PreparedGraph>(graphs_->state(),config);
    result.processors = prepared_;
    prepare_mixer(result);
    prepare_midi_clips(result);
    prepare_inserts(result,c,retain_inserts);
    const auto mapped = [&](const std::vector<int>& outputs) {
        std::vector<std::size_t> result;
        for (const auto channel : outputs) {
            const auto found = std::find(c.outputs.begin(),c.outputs.end(),channel);
            require(found != c.outputs.end(),"missing physical output in stream");
            result.push_back(static_cast<std::size_t>(found-c.outputs.begin()));
        }
        return result;
    };
    result.click = mapped_click(click_settings_, c.outputs);
    result.master_outputs = project->master_outputs.empty() ? std::vector<std::size_t>{0} : mapped(project->master_outputs);
    if (project->master_outputs.empty() && c.outputs.size()>1) result.master_outputs.push_back(1);
    for (const auto& track : project->tracks) if (track.kind != TrackKind::midi) result.hardware_outputs.push_back(mapped(track.hardware_outputs));
    std::set<std::string> missing_ara_sources,pending_stretch_clips;
    for (const auto& clip : project->clips) {
        if(clip.midi)continue;
        require(result.voices.size() < audio::max_voices,"too many playback voices");
        std::shared_ptr<const audio::AudioData> asset_data;
        try{asset_data=asset(clip.source);}catch(const std::exception& e){
            if(std::none_of(project->ara_bindings.begin(),project->ara_bindings.end(),[&](const auto& b){return b.source==clip.source;}))throw;
            missing_ara_sources.insert(clip.source);waveform_error_=e.what();continue;
        }
        require(asset_data->sample_rate == c.sample_rate,"WAV/project sample-rate mismatch");
        asset_data=clip_audio(clip,std::move(asset_data));
        if(!asset_data){pending_stretch_clips.insert(clip.id.value);continue;} // No wrong-speed PCM fallback.
        audio::Voice voice{asset_data,clip.start,clip_playback_offset(clip),clip.length,{}};voice.fade_in=clip.fade_in;voice.fade_out=clip.fade_out;
        const auto track = std::find(mixer_tracks_.begin(),mixer_tracks_.end(),clip.track);
        require(track != mixer_tracks_.end(),"audio clip requires an audio track");
        voice.mixer_track = static_cast<std::size_t>(track-mixer_tracks_.begin());
        const auto gain = clip.source == "mrs:demo-tone" ? 0.15f : 1.0f;
        if (asset_data->channels == 1) {
            // A mono track is centered in the selected main pair, without
            // spilling into extra click/cue outputs. Single-output remains unity.
            const auto outputs = std::min<std::size_t>(2,c.outputs.size());
            for (std::uint32_t output = 0; output < outputs; ++output) voice.routes.push_back({0,output,gain});
        } else for (std::uint32_t channel = 0; channel < asset_data->channels; ++channel)
            voice.routes.push_back({channel,channel % static_cast<std::uint32_t>(std::min<std::size_t>(2,c.outputs.size())),gain});
        result.voices.push_back(std::move(voice));
    }
#ifdef MRS_HAS_ARA
    if (ara_) {if(missing_ara_sources.empty()&&pending_stretch_clips.empty())ara_->apply(*project,result,true);else{auto available=*project;std::erase_if(available.clips,[&](const auto& clip){return !clip.midi&&(missing_ara_sources.contains(clip.source)||pending_stretch_clips.contains(clip.id.value));});ara_->apply(available,result,true);}}
#endif
    for (const auto& track : project->tracks) if (track.kind == TrackKind::audio) {
        // A connected physical selector alone is not a live DSP dependency.
        // Mixed ownership changes are rebuilt while stopped by Monitor/Arm.
        const bool livePlayback=workspace_==Workspace::live&&path_.empty()&&(preparing_project_||result.performance.has_value());
        // Enabled ASIO inputs do not make a muted Playback track a live-input
        // graph. Keep real Monitor/Arm routes and their handoff safety guards.
        if((livePlayback||(c.process_buffer_frames&&c.processing_workers>=2))&&!track.input_monitor&&!track_armed(track.id))continue;
        const auto inputs=track_inputs(track,default_inputs_);
        const auto found_track=std::find(mixer_tracks_.begin(),mixer_tracks_.end(),track.id);
        const auto index=static_cast<std::size_t>(found_track-mixer_tracks_.begin());
        for (std::size_t i=0; i<inputs.size(); ++i) {
            const auto input=std::find(c.inputs.begin(),c.inputs.end(),inputs[i]); if (input == c.inputs.end()) continue;
            const auto stream=static_cast<std::uint32_t>(input-c.inputs.begin());
            if (inputs.size() == 1) for (std::uint32_t out=0; out<std::min<std::size_t>(2,c.outputs.size()); ++out) result.monitor.push_back({stream,out,1,index});
            else result.monitor.push_back({stream,static_cast<std::uint32_t>(c.outputs.size() == 1 ? 0 : i),c.outputs.size() == 1 ? 0.5f : 1.0f,index});
        }
    }
    return result;
}
void Application::prepare_inserts(audio::RenderGraph& result,const audio::DeviceConfig& c,bool retain) {
    const auto project=preparing_project_?preparing_project_:services_.projects->state().project.get();
    // Recording and clip-only transitions retain instances: chains and device configuration are unchanged.
    if(retain){for(const auto& t:project->tracks)if(t.kind!=TrackKind::midi){const auto at=insert_runtime_.find(t.id.value);require(at!=insert_runtime_.end(),"missing retained insert runtime");result.inserts.push_back(at->second.graph);}const auto master=insert_runtime_.find(std::string{});require(master!=insert_runtime_.end(),"missing retained master runtime");result.master_inserts=master->second.graph;return;}
    std::vector<std::string> changed;
    if(staged_insert_target_||!staged_insert_graphs_.empty()){
        if(staged_insert_target_)staged_insert_graphs_[*staged_insert_target_]=staged_insert_graph_;
        // Unchanged channels retain their plugins, opaque state and IR loaders.
        for(auto at=reverb_runtime_.begin();at!=reverb_runtime_.end();){
            const auto key=at->track?at->track->value:std::string{};
            if(staged_insert_graphs_.contains(key)){at->loader->shutdown();at=reverb_runtime_.erase(at);}else ++at;
        }
        for(const auto& [key,graph]:staged_insert_graphs_){insert_runtime_[key]={graph,{}};instrument_errors_.erase(key);changed.push_back(key);}
        staged_insert_graphs_.clear();
    }else{stop_reverb_loaders();insert_runtime_.clear();instrument_errors_.clear();}
    ++insert_generation_;
    const auto chain=[&](const std::vector<NativeInsert>& effects,std::uint32_t block,bool instrument=false) -> std::shared_ptr<processing::PreparedGraph> {
        if (effects.empty()) return {};
        auto saved=std::make_shared<const processing::GraphState>(processing::insert_graph(effects));
        return std::make_shared<processing::PreparedGraph>(processing::GraphSnapshot{saved,0,false,false},processing::ProcessConfig{c.sample_rate,static_cast<std::uint32_t>(c.outputs.size()),block,c.buffer_frames,instrument}
#ifdef MRS_HAS_VST3
            ,processing::hosted_factory
#endif
        );
    };
    for (const auto& t : project->tracks) if (t.kind != TrackKind::midi) {
        if(!insert_runtime_.contains(t.id.value)){std::shared_ptr<processing::PreparedGraph> prepared;try{prepared=chain(t.inserts,c.buffer_frames,t.kind==TrackKind::instrument);}catch(const std::exception& e){if(t.kind!=TrackKind::instrument)throw;instrument_errors_[t.id.value]=e.what();}insert_runtime_[t.id.value]={prepared,{}};changed.push_back(t.id.value);}
        result.inserts.push_back(insert_runtime_.at(t.id.value).graph);
    }
    if(!insert_runtime_.contains(std::string{})){insert_runtime_[std::string{}]={chain(project->master_inserts,8192),{}};changed.push_back(std::string{});}
    result.master_inserts=insert_runtime_.at(std::string{}).graph;
    for(const auto& [key,r]:insert_runtime_)if(r.graph&&std::find(changed.begin(),changed.end(),key)!=changed.end()){const auto target=key.empty()?std::optional<Id>{}:std::optional<Id>{Id{key}};
        const auto block=key.empty()?8192:c.buffer_frames;const auto config=processing::ProcessConfig{c.sample_rate,static_cast<std::uint32_t>(c.outputs.size()),block,c.buffer_frames};
        for(const auto& fx:insert_chain(*project,target))if(fx.kind==InsertKind::reverb||wav_core(fx.kind)){auto loader=std::make_shared<audio::ReverbLoader>(r.graph,fx.id,config,fx.kind==InsertKind::reverb?reverb_library_:nullptr);
            if(!fx.reverb.first.digest.empty())loader->restore(fx.reverb,wav_core(fx.kind)?cab_root_:reverb_library_->snapshot()->root);
            reverb_runtime_.push_back({std::move(loader),target,fx.id,0,std::nullopt});}}
}
void Application::connect(std::unique_ptr<audio::IAudioDevice> device, audio::DeviceConfig c) {
    require_not_recording();
    require(static_cast<bool>(device),"missing audio backend");
    const auto defaults=c.inputs;
    auto infos = device->enumerate();
    const auto info = std::find_if(infos.begin(),infos.end(),[&](const auto& v) { return v.index == c.device; });
    require(info != infos.end(),"audio device no longer available");
    const bool offline=info->name == "Offline clock (no sound)";
    c.inputs=offline ? defaults : selected_inputs(*services_.projects->state().project,defaults);
    audio::validate_device_config(*info,c);
    if (!offline) validate_hardware(*services_.projects->state().project,c);
    require(c.sample_rate == services_.projects->state().project->sample_rate,"device/project rate mismatch; choose the project rate");
    // Capture live component/controller state before releasing the old instances.
    // In particular a buffer change must retain edits made inside native editors.
    if(device_){
        device_->stop();
        try {
            auto candidate=*services_.projects->state().project;
            capture_insert_state(candidate,{},true);
            struct Capture final:ICommand {
                Project saved;
                explicit Capture(Project p):saved(std::move(p)){}
                std::string_view name() const override{return "Capture plugin state before audio reconnect";}
                void apply(Project& p) const override{p=saved;}
            };
            services_.projects->execute(Capture{std::move(candidate)});
        }catch(...){device_->start();throw;}
    }
    auto position=engine_->state();
    if(position.playback==PlaybackState::playing)position.playback=PlaybackState::paused;
    // Existing callbacks must be stopped before preparing or releasing graphs.
    disconnect();
    const auto previous_defaults=default_inputs_; default_inputs_=defaults;
    try {
        audio::RenderGraph graph;
        if (offline) { prepare_mixer(graph); prepare_midi_clips(graph); graph.click=mapped_click(click_settings_,c.outputs); prepare_inserts(graph,c); }
        else graph = render(c);
        engine_->prepare({c.sample_rate,static_cast<std::uint32_t>(c.inputs.size()),static_cast<std::uint32_t>(c.outputs.size()),8192,c.buffer_frames,c.processing_workers},std::move(graph),position);
        device->open(c,engine_); device->start(); audio_name_ = info->name; device_ = std::move(device); device_config_ = c; device_info_ = *info; default_inputs_ = defaults; poll();
    } catch (...) {
        default_inputs_=previous_defaults; device->close(); prepared_.reset();
        engine_->prepare({services_.projects->state().project->sample_rate,0,2,8192},{});
        transport_->poll(); throw;
    }
}
void Application::disconnect() {
    require_not_recording();
    midi_inputs_.reset();midi_route_revision_=~std::uint64_t{};instrument_errors_.clear();
    if (device_) { device_->close(); device_.reset(); }
    external_midi_->panic();if(midi_outputs_)midi_outputs_->routes({});
    stop_reverb_loaders();++insert_generation_;insert_runtime_.clear(); prepared_.reset(); device_config_.reset(); device_info_.reset(); audio_name_ = "Disconnected";
    if (transport_) { engine_->prepare({engine_->config().sample_rate,0,2,8192},{}); transport_->poll(); }
    staged_playback_.reset();
}
void Application::poll() {
    poll_staged_playback();
    poll_reverbs();
    publish_midi_routes();
    publish_inserts();
    for(auto& [key,r]:insert_runtime_){(void)key;if(r.graph && r.graph->consume_edits()){unsaved_=true;++plugin_edit_revision_;}}
    publish_mix();
    // Hidden pre-record capture runs while pre-count deliberately holds playback.
    // Read the coherent transport snapshot once; only a real stop ends the take.
    const auto recording_transport = engine_->state();
    if (recording() && (std::any_of(captures_.begin(),captures_.end(),[](const auto& capture) { return capture.recorder->status().fault != audio::RecordFault::none; }) ||
        ((recording_?recording_->status().frames>0:!midi_captures_.empty()&&midi_captures_.front().recorder->end()>midi_captures_.front().recorder->start()) && recording_transport.playback != PlaybackState::playing && recording_transport.count_remaining == 0) || std::any_of(midi_captures_.begin(),midi_captures_.end(),[](const auto& c){return c.recorder->fault();}) ||
        device_status().phase != audio::DevicePhase::running)) {
        try { (void)stop_recording(); } catch (const std::exception& e) { recording_error_ = e.what(); }
    }
    transport_->poll();
    for (auto& [key,value] : assets_) {
        (void)key;
        if (value.pending.valid() && value.pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            try { value.peaks = value.pending.get(); } catch (const std::exception& e) { waveform_error_ = e.what(); }
    }
    poll_wav_import();
    poll_stretch();
}
audio::DeviceStatus Application::device_status() { return device_ ? device_->status() : audio::DeviceStatus{}; }
bool Application::audio_running() { return device_status().phase == audio::DevicePhase::running; }
bool Application::play_library_preview(const std::filesystem::path& file,double fraction) {
    if(!device_config_||audio_name_=="Offline clock (no sound)"||
       !audio_running()||recording())return false;
    engine_->play_library_preview(file,fraction);
    return true;
}
void Application::stop_library_preview() noexcept {engine_->stop_library_preview();}
audio::AudioEngine::LibraryPreviewState Application::library_preview_state() const noexcept {
    return engine_->library_preview_state();
}
void Application::play() { require(audio_running(),"connect audio or choose Offline clock first");
    require(stretch_ready(true),"Time / pitch is not ready. Wait for preparation, apply again after cancellation, or reopen a changed source.");
#ifdef MRS_HAS_ARA
    if (ara_ && engine_->state().playback!=PlaybackState::playing && ara_->poll()) {unsaved_=true;refresh_ara(false);}
#endif
    transport_->play(); }
void Application::set_click_settings(audio::ClickSettings settings){settings.validate();require_not_recording();if(settings==click_settings_)return;const auto mapped = mapped_click(settings, active_outputs());require(engine_->enqueue({audio::ControlKind::click,mapped.control_flags(),settings.level}),"transport control queue full");click_settings_=settings;}
void Application::pause() { if (recording()) (void)stop_recording(); else transport_->pause(); }
void Application::stop() { if (recording()) (void)stop_recording(); transport_->stop(); }
void Application::seek(Sample sample) { require_not_recording(); transport_->seek(sample); }

void Application::set_track_armed(const Id& id, bool enabled) {
    require_not_playing();
    const auto p=services_.projects->state().project;
    const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;});
    require(t!=p->tracks.end()&&(t->kind==TrackKind::audio||t->kind==TrackKind::instrument),"arm an audio or instrument track");
    const auto before=armed_tracks_;
    require(!enabled || track_armed(id) || armed_tracks_.size() < 32,"recording supports up to 32 armed tracks");
    if(enabled&&!track_armed(id))armed_tracks_.push_back(id);else if(!enabled)std::erase(armed_tracks_,id);
    try {
        if(device_config_&&audio_name_!="Offline clock (no sound)"){auto c=*device_config_;c.inputs=selected_inputs(*p);audio::validate_device_config(*device_info_,c);}
        sync_arm();
        // A monitored track already belongs to the live domain. Arm is a session
        // capture selector; no recorder is installed until Record starts.
        if(enabled){
            if(t->kind==TrackKind::instrument)set_midi_input(id,t->midi_input,t->midi_channel,true);
            else if(!t->input_monitor){
                services_.projects->execute(SetTrackMonitoring{id,true});
                rebuild_audio(true); // quiesce ahead/device ownership; keep every DSP instance/editor
            }
        } else if(t->kind==TrackKind::audio&&!t->input_monitor)rebuild_audio(true);
        publish_mix();midi_route_revision_=~std::uint64_t{};publish_midi_routes();
    } catch (...) {armed_tracks_=before;sync_arm();throw;}
}
void Application::arm_track(std::optional<Id> id) {
    require_not_playing();const auto before=armed_tracks_;armed_tracks_.clear();
    try {if(id)set_track_armed(*id,true);else {sync_arm();rebuild_audio(true);midi_route_revision_=~std::uint64_t{};publish_midi_routes();}}
    catch (...) {armed_tracks_=before;sync_arm();throw;}
}
void Application::set_track_monitoring(const Id& id, bool enabled) {
    const SetTrackMonitoring command{id,enabled}; auto candidate=*services_.projects->state().project;
    command.apply(candidate); candidate.validate();
    const auto track=std::find_if(services_.projects->state().project->tracks.begin(),services_.projects->state().project->tracks.end(),[&](const auto& t){return t.id==id;});
    const bool liveRouteChange=workspace_==Workspace::live && path_.empty() &&
        track->kind==TrackKind::audio && track->input_monitor!=enabled;
    // Live Playback intentionally omits unmonitored device routes at graph
    // preparation. Enabling a monitor must rebuild that route off callback,
    // even when its physical input is already selected/open in ASIO.
    if (liveRouteChange) require_not_playing();
    if(!liveRouteChange&&device_config_&&device_config_->process_buffer_frames&&device_config_->processing_workers>=2&&
        audio_name_!="Offline clock (no sound)"&&!track_armed(id)&&track->input_monitor!=enabled){
        edit(command,{},true);return;
    }
    auto next=device_config_; if (next) next->inputs=selected_inputs(candidate);
    const bool rebind=enabled && next && audio_name_ != "Offline clock (no sound)" &&
        std::any_of(next->inputs.begin(),next->inputs.end(),[&](int input) {
            return std::find(device_config_->inputs.begin(),device_config_->inputs.end(),input) == device_config_->inputs.end();
        });
    if (rebind) require_not_playing();
    if (rebind) audio::validate_device_config(*device_info_,*next);
    services_.projects->execute(command);
    if (rebind || liveRouteChange) rebuild_audio(true); else publish_mix();
}
void Application::monitoring(bool enabled) {
    require(engine_->enqueue({audio::ControlKind::monitor,enabled ? 1 : 0}),"audio command queue full");
    monitoring_ = enabled;
}
audio::RecordStatus Application::recording_status() const {if(recording_)return recording_->status();if(!midi_captures_.empty()){audio::RecordStatus status;status.frames=static_cast<std::uint64_t>(midi_captures_.front().recorder->end()-midi_captures_.front().recorder->start());if(midi_captures_.front().recorder->fault())status.fault=audio::RecordFault::overflow;return status;}return last_recording_status_;}
std::vector<Clip> Application::midi_recording_preview() const {
    if(engine_->state().count_remaining)return {};
    const auto p=services_.projects->state().project;const Timeline time(p->time,p->sample_rate);std::vector<Clip> result;
    for(const auto& capture:midi_captures_){Clip c;c.id=Id{"record-preview-"+capture.track.value};c.track=capture.track;c.name="Recording MIDI";c.midi=capture.recorder->preview(time);const auto visible=time.to_ticks(recording_visible_start_);c.midi->source_offset=visible-c.midi->start;c.midi->length=std::max(Tick{1},c.midi->length-c.midi->source_offset);c.midi->start=visible;result.push_back(std::move(c));}return result;
}
std::vector<AudioRecordingPreview> Application::audio_recording_preview() const {
    if(engine_->state().count_remaining)return {};
    std::vector<AudioRecordingPreview> result;result.reserve(captures_.size());
    for(const auto& capture:captures_){const auto frames=static_cast<Sample>(capture.recorder->status().frames);
        Clip clip;clip.id=Id{"audio-record-preview-"+capture.track.value+"-"+utf8(capture.recorder->destination().filename())};clip.track=capture.track;clip.name="Recording audio";
        clip.start=recording_visible_start_;clip.source_offset=clip.start-capture.recorder->start();clip.length=std::max(Sample{1},frames-clip.source_offset);
        result.push_back({std::move(clip),capture.recorder->preview()});}
    return result;
}
void Application::start_project_recording(const std::filesystem::path& projects_folder) {
    require_not_recording(); sync_arm();
    const auto p=services_.projects->state().project;
    const bool audio=std::any_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.kind==TrackKind::audio&&track_armed(t.id);});
    if(!audio){start_recording({});return;}
    require(audio_running()&&device_config_,"connect audio before recording");
    require(audio_name_!="Offline clock (no sound)","recording needs a hardware input; Offline clock cannot record");
    require(!engine_->state().loop,"turn off loop before recording");
    for(const auto& t:p->tracks)if(track_armed(t.id)){
        if(t.kind==TrackKind::instrument){require(!t.midi_input.empty(),"select a MIDI input for the armed instrument track");continue;}
        const auto inputs=track_inputs(t,default_inputs_);require(!inputs.empty(),"armed track has no selected input");
        for(const auto input:inputs)require(std::find(device_config_->inputs.begin(),device_config_->inputs.end(),input)!=device_config_->inputs.end(),"armed input is not active");
    }
    if(path_.empty()) {
        // Saving imported media requires quiescent callbacks. Keep sample position,
        // processors and device configuration; Record then resumes from this point.
        const bool playing=engine_->state().playback==PlaybackState::playing;
        if(playing){device_->stop();auto position=engine_->state();position.playback=PlaybackState::paused;
            try{engine_->prepare({device_config_->sample_rate,static_cast<std::uint32_t>(device_config_->inputs.size()),static_cast<std::uint32_t>(device_config_->outputs.size()),8192,device_config_->buffer_frames,device_config_->processing_workers},render(*device_config_,true),position);device_->start();}
            catch(...){disconnect();throw;}}
        try {
            const auto name="Untitled-"+new_id().value;
            const auto file=projects_folder/name/(name+".mrsproject");
            require(!std::filesystem::exists(file.parent_path()),"automatic project directory already exists");
            save_project(file);
        } catch(...){if(playing)play();throw;}
    }
    ensure_project_folders(path_);
    start_recording(path_.parent_path()/"Media"/("Take-"+new_id().value+".wav"));
}
void Application::start_recording(const std::filesystem::path& destination) {
    stop_library_preview(); // never audition into a live or recording session
    require_not_recording();
    require(stretch_ready(true),"Wait for time / pitch preparation before recording");
    sync_arm(); require(!armed_tracks_.empty(),"Arm one or more audio/instrument tracks before recording");
    require(audio_running() && device_config_,"connect audio before recording");
    require(audio_name_ != "Offline clock (no sound)","recording needs a hardware input; Offline clock cannot record");
    auto position=engine_->state(); require(!position.loop,"turn off loop before recording");
    const auto p=services_.projects->state().project;
    require(p->clips.size()+armed_tracks_.size() <= audio::max_voices && assets_.size()+armed_tracks_.size() <= 128,"no free clip/source capacity for recording");
    std::size_t disk{}, bytes{};
    for (const auto& clip : p->clips) if(!clip.midi){ const auto data=asset(clip.source); if (data->file) { ++disk; bytes+=8*8192*static_cast<std::size_t>(data->channels)*sizeof(float); } }
    struct Pending { Id track; std::vector<std::uint32_t> selectors; std::filesystem::path path; };
    std::vector<Pending> pending;std::vector<Id> midi_pending;
    for (std::size_t i=0; i<armed_tracks_.size(); ++i) {
        const auto& id=armed_tracks_[i]; const auto track=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t) { return t.id == id; });
        if(track->kind==TrackKind::instrument){require(!track->midi_input.empty(),"select a MIDI input for the armed instrument track");midi_pending.push_back(id);continue;}
        const auto inputs=track_inputs(*track,default_inputs_); require(!inputs.empty(),"armed track has no selected input");
        Pending take{id,{},destination};
        for (const auto input : inputs) { const auto found=std::find(device_config_->inputs.begin(),device_config_->inputs.end(),input); require(found != device_config_->inputs.end(),"armed input is not active"); take.selectors.push_back(static_cast<std::uint32_t>(found-device_config_->inputs.begin())); }
        if (std::count_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.kind==TrackKind::audio&&track_armed(t.id);})>1) take.path=destination.parent_path()/(destination.stem().wstring()+L"-"+std::to_wstring(i+1)+L".wav");
        require(!std::filesystem::exists(take.path),"recording destination exists; choose a new take name");
        bytes+=8*8192*inputs.size()*sizeof(float); pending.push_back(std::move(take));
    }
    require(disk+pending.size() <= 32 && bytes <= 256*1024*1024,"no disk voice capacity for recording");
    device_->stop(); position=engine_->state();if(position.playback==PlaybackState::playing)position.playback=PlaybackState::paused;
    try {
        recording_error_.clear(); last_take_.clear(); last_takes_.clear(); last_recording_status_={};
        recording_visible_start_=position.sample;audio::RenderGraph count;configure_count(count,*p,click_settings_,position.sample);
        const auto source_start=position.sample-std::min({position.sample,count.count_frames,Sample{2}*p->sample_rate});
        for (const auto& take : pending) captures_.push_back({take.track,std::make_shared<audio::Recorder>(take.path,p->sample_rate,source_start,take.selectors)});
        std::size_t project_notes{},project_events{};for(const auto& clip:p->clips)if(clip.midi){project_notes+=clip.midi->notes.size();project_events+=clip.midi->events.size();}
        for(const auto& id:midi_pending){std::size_t track_notes{},track_events{};std::vector<std::uint16_t> keys;for(const auto& clip:p->clips)if(clip.midi&&clip.track==id){track_notes+=clip.midi->notes.size();track_events+=clip.midi->events.size()+32;for(const auto& event:clip.midi->events)keys.push_back(static_cast<std::uint16_t>((event.kind*16+event.channel)*128+((event.kind==2||event.kind==6)?event.data1:0)));}
            const auto note_limit=std::min<std::size_t>(4096-std::min<std::size_t>(track_notes,4096),(32768-project_notes)/midi_pending.size());
            const auto event_limit=std::min<std::size_t>(8192-std::min<std::size_t>(track_events,8192),(65536-project_events)/midi_pending.size());require(event_limit>48,"no MIDI controller recording capacity");
            midi_captures_.push_back({id,std::make_shared<audio::MidiRecorder>(source_start,note_limit,std::min<std::size_t>(8144,event_limit-48),std::move(keys))});
        }
        if(!captures_.empty())recording_=captures_.front().recorder;
        engine_->prepare({device_config_->sample_rate,static_cast<std::uint32_t>(device_config_->inputs.size()),static_cast<std::uint32_t>(device_config_->outputs.size()),8192,device_config_->buffer_frames,device_config_->processing_workers},render(*device_config_,true),position);
        publish_midi_routes();transport_->play(); device_->start();
    } catch (...) {
        if (device_) device_->close();
        engine_->prepare({p->sample_rate,0,2,8192},{},position);
        for (auto& capture : captures_) try { (void)capture.recorder->finish(); } catch (...) {}
        captures_.clear(); midi_captures_.clear();recording_.reset(); disconnect(); throw;
    }
}
bool Application::stop_recording() {
    if (!recording()) return false;
    try { device_->stop(); }
    catch (const std::exception& e) {
        recording_error_=std::string("Audio device failed; takes retained, device disconnected: ")+e.what();
        device_->close(); device_.reset(); device_config_.reset(); device_info_.reset(); audio_name_="Disconnected";
    }
    auto position=engine_->state(); if (position.playback == PlaybackState::playing) position.playback=PlaybackState::paused;
    if(!recording_)last_recording_status_=recording_status();
    auto sessions=std::move(captures_);captures_.clear();auto midi_sessions=std::move(midi_captures_);midi_captures_.clear();recording_.reset();
    struct AddTakes final : ICommand {
        std::vector<Clip> clips;
        std::string_view name() const override { return "Add recorded takes"; }
        void apply(Project& p) const override { for (const auto& clip : clips) {if(clip.midi)AddMidiClip{clip}.apply(p);else AddRecordedClip{clip}.apply(p);} }
    } command;
    std::string failure;
    try {
        engine_->prepare({services_.projects->state().project->sample_rate,device_config_ ? static_cast<std::uint32_t>(device_config_->inputs.size()) : 0U,
            device_config_ ? static_cast<std::uint32_t>(device_config_->outputs.size()) : 2U,8192},{},position);
        for (const auto& capture : sessions) {
            try {
                const auto result=capture.recorder->finish();
                if (capture.track == sessions.front().track) last_recording_status_=result.status;
                if (result.status.fault != audio::RecordFault::none) recording_error_="Recording ended early; valid prefixes retained (input/dropout/disk/seek/size limit)";
                else if (result.status.nonfinite_samples) recording_error_="Non-finite input replaced with silence";
                const auto hidden=recording_visible_start_-capture.recorder->start();
                if (result.frames<=hidden) continue;
                last_takes_.push_back(result.path); if (last_take_.empty()) last_take_=result.path;
                command.clips.push_back({new_id(),capture.track,utf8(result.path.stem()),recording_visible_start_,result.frames-hidden,hidden,utf8(result.path)});
            } catch (const std::exception& e) { failure=e.what(); }
        }
        const auto project=services_.projects->state().project;const Timeline time(project->time,project->sample_rate);
        for(const auto& capture:midi_sessions){if(capture.recorder->end()<=recording_visible_start_)continue;auto data=capture.recorder->finish(time);if(capture.recorder->fault())recording_error_="MIDI recording interrupted (input/queue/capacity); captured prefix retained";if(data.notes.empty()&&data.events.empty())continue;const auto visible=time.to_ticks(recording_visible_start_);data.source_offset=visible-data.start;data.length=std::max(Tick{1},data.length-data.source_offset);data.start=visible;Clip clip;clip.id=new_id();clip.track=capture.track;clip.name="MIDI take";clip.midi=std::move(data);command.clips.push_back(std::move(clip));}
        if (!command.clips.empty()) edit(command,{},true); else rebuild_audio(true);
        if (!failure.empty()) throw std::runtime_error(failure);
        return !command.clips.empty();
    } catch (const std::exception& e) {
        recording_error_=e.what(); if (!last_take_.empty()) recording_error_+="; retained file: "+utf8(last_take_);
        if (device_ && device_config_) try { rebuild_audio(); } catch (...) { disconnect(); }
        throw;
    }
}
} // namespace mrs::desktop
