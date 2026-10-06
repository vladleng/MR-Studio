#include <mrs/desktop.hpp>
#ifdef MRS_HAS_VST3
#include <mrs/vst3.hpp>
#endif
#include <mrs/offline_device.hpp>
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
namespace mrs::desktop {
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
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
    out << "MRS_DESKTOP_CONFIG 6\n" << static_cast<int>(p.workspace) << ' ' << p.rate << ' ' << p.buffer << ' ' << p.monitor_input
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
    const auto bytes = out.str(); require(bytes.size() <= 524288,"config too large"); return bytes;
}
Preferences decode_preferences(std::string_view bytes) {
    require(bytes.size() <= 524288,"config too large"); std::istringstream in{std::string(bytes)};
    std::string magic; int version{},workspace{}; std::size_t count{}; Preferences p;
    require(static_cast<bool>(in >> magic >> version) && magic == "MRS_DESKTOP_CONFIG" && (version >= 1 && version <= 6),"unsupported config");
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
Application::Application() : engine_(std::make_shared<audio::AudioEngine>()) { demo(); }
Application::~Application() {
    midi_inputs_.reset(); // joins bridge/closes driver callbacks before engine/device retirement
    if (device_) device_->close();
    // Finalize on clean shutdown; a completed file remains recoverable even if
    // the UI did not attach/save its project reference.
    for (auto& capture : captures_) try { (void)capture.recorder->finish(); } catch (const std::exception&) {}
    for (auto& [key,value] : assets_) { (void)key; *value.cancel = true; }
}
void Application::workspace(Workspace w) { (void)workspace_name(w); workspace_ = w; }
void Application::replace(persistence::ProjectDocument next) {
    require_not_recording(); next.validate(); armed_.reset(); armed_tracks_.clear();
    // Existing MIXR channels hydrate the shared command model, including legacy archives.
    for (const auto& m : next.mixer) for (auto& t : next.project.tracks) if (t.id == m.track)
        t.mix = {m.gain,m.pan,m.mute,m.solo};
    // Build application models before releasing the old session.
    {std::vector<Id> tracks;for(const auto& t:next.project.tracks)if(t.kind!=TrackKind::midi)tracks.push_back(t.id);(void)audio::compile_midi_clips(next.project,tracks);(void)audio::compile_midi_events(next.project,tracks);}
    auto projects = std::make_shared<ProjectStore>(next.project);
    auto graphs = std::make_shared<processing::GraphStore>(next.graph);
    disconnect();
    engine_->prepare({next.project.sample_rate,0,2,8192},{});
    auto transport = std::make_shared<audio::EngineTransport>(engine_,Timeline(next.project.time,next.project.sample_rate));
    auto musical = std::make_unique<MusicalTimeline>(Services{projects,transport});
    for (auto& [key,value] : assets_) { (void)key; *value.cancel = true; }
    assets_.clear(); owned_media_.clear(); waveform_error_.clear();
    document_ = std::move(next);
    mixer_tracks_.clear(); applied_mix_revision_ = 0;
    services_ = {projects,transport}; graphs_ = std::move(graphs);
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
    result.validate(); return result;
}
void Application::save_project(const std::filesystem::path& requested) {
    require_not_recording();
    const auto path = std::filesystem::absolute(requested).lexically_normal();
    require(!path.filename().empty(),"choose a project filename");
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
        if (!std::filesystem::exists(source) && !current.contains(key)) continue; // already-missing Undo media stays a missing reference
        owned[key] = copies.media(source);
    }
    for (std::size_t i=0; i<document.project.clips.size(); ++i) {
        const auto& original = services_.projects->state().project->clips[i].source;
        if (!document.project.clips[i].midi && original != "mrs:demo-tone") document.project.clips[i].source = media_ref(owned.at(original).lexically_relative(root));
    }
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
            if (!cached.peaks) cached.pending = std::async(std::launch::async,[data=cached.data,cancel=cached.cancel] { return audio::Waveform(*data,cancel); });
        }
        rebuild_audio(); // same device handle, paused/stopped position and loop
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
    d.project.sample_rate = rate; d.project.tracks.clear(); d.project.clips.clear();
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
    for (const auto& clip : services_.projects->state().project->clips) if(!clip.midi)(void)asset(clip.source);
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
bool parameter_only(const std::vector<NativeInsert>& before,const std::vector<NativeInsert>& after){
    if(before.size()!=after.size())return false;
    for(std::size_t i=0;i<before.size();++i){auto a=before[i],b=after[i];a.gain=b.gain;a.frequency=b.frequency;a.q=b.q;a.bands=b.bands;a.ir.mix=b.ir.mix;a.ir.low_cut=b.ir.low_cut;a.ir.high_cut=b.ir.high_cut;a.ir.invert=b.ir.invert;
        // Native EQ/gain/filter parameters and bypass may change without rebuilding.
        if(a.kind!=InsertKind::vst3)a.bypass=b.bypass;else a.parameters=b.parameters;
        if(a!=b)return false;
    }return true;
}
const std::vector<NativeInsert>& insert_chain(const Project& p,const std::optional<Id>& track){if(!track)return p.master_inserts;for(const auto& t:p.tracks)if(t.id==track)return t.inserts;throw std::invalid_argument("unknown insert track");}
}
void Application::publish_inserts(){for(auto& [key,r]:insert_runtime_){(void)key;if(r.pending && r.graph && r.graph->enqueue_parameters(*r.pending))r.pending.reset();}}
void Application::preview_inserts(std::optional<Id> track,const std::vector<NativeInsert>& inserts){
    const auto p=services_.projects->state().project;require(parameter_only(insert_chain(*p,track),inserts),"Pause/Stop to change insert structure");
    const auto found=insert_runtime_.find(track?track->value:std::string{});if(found!=insert_runtime_.end()){found->second.pending=processing::insert_graph(inserts);publish_inserts();}
}
void Application::cancel_insert_preview(){const auto p=services_.projects->state().project;for(auto& [key,r]:insert_runtime_){r.pending=processing::insert_graph(insert_chain(*p,key.empty()?std::nullopt:std::optional<Id>{Id{key}}));}publish_inserts();}
void Application::set_inserts(std::optional<Id> track, std::vector<NativeInsert> inserts) {
    const auto p=services_.projects->state().project;
    if(parameter_only(insert_chain(*p,track),inserts)){auto candidate=*p;SetInserts{track,inserts}.apply(candidate);candidate.validate();preview_inserts(track,inserts);services_.projects->execute(SetInserts{track,std::move(inserts)});}
    else {
        require_not_playing();
        for(const auto& fx:inserts)if(fx.kind==InsertKind::cab_ir){auto state=processing::insert_graph(std::array{fx});const auto channels=device_config_?static_cast<std::uint32_t>(device_config_->outputs.size()):2;processing::PreparedGraph checked{{std::make_shared<const processing::GraphState>(state),0,false,false},{p->sample_rate,channels,64}};}
#ifdef MRS_HAS_VST3
        const auto& current=insert_chain(*p,track);
        for(const auto& fx:inserts)if(fx.kind==InsertKind::vst3 && std::none_of(current.begin(),current.end(),[&](const auto& old){return old.id==fx.id&&old.plugin_path==fx.plugin_path&&old.class_id==fx.class_id;})){
            auto state=processing::insert_graph(std::array{fx});const auto channels=device_config_?static_cast<std::uint32_t>(device_config_->outputs.size()):2;
            const bool instrument=track&&fx.id==inserts.front().id&&std::any_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==*track&&t.kind==TrackKind::instrument;});
            processing::PreparedGraph checked{{std::make_shared<const processing::GraphState>(state),0,false,false},{p->sample_rate,channels,64,128,instrument},processing::hosted_factory};
        }
#else
        require(std::none_of(inserts.begin(),inserts.end(),[](const auto& fx){return fx.kind==InsertKind::vst3;}),"This build has no VST3 support");
#endif
        edit(SetInserts{std::move(track),std::move(inserts)});
    }
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
void Application::set_plugin_parameter(std::optional<Id> track,const Id& slot,std::uint32_t id,float value){auto chain=insert_chain(*services_.projects->state().project,track);auto it=std::find_if(chain.begin(),chain.end(),[&](const auto& fx){return fx.id==slot;});require(it!=chain.end() && it->kind==InsertKind::vst3,"unknown VST3 insert");auto p=std::find_if(it->parameters.begin(),it->parameters.end(),[&](const auto& p){return p.id==id;});if(p==it->parameters.end())it->parameters.push_back({id,value});else p->value=value;set_inserts(track,std::move(chain));}
NativeInsert Application::capture_insert(std::optional<Id> track,const Id& slot){
    auto candidate=*services_.projects->state().project;capture_insert_state(candidate);
    const auto& effects=insert_chain(candidate,track);auto found=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});
    require(found!=effects.end(),"unknown insert");return *found;
}
void Application::capture_insert_state(Project& project,std::optional<Id> authoritative,bool callbacks_stopped){
    bool has_vst=false;for(const auto& [key,r]:insert_runtime_){(void)key;if(r.graph)for(const auto& n:r.graph->snapshot().graph->nodes)has_vst=has_vst||n.format==processing::ProcessorFormat::vst3;}if(!has_vst || !device_)return;
    if(!callbacks_stopped){require_not_playing();device_->stop();}
    try{const auto original=services_.projects->state().project;
        for(const auto& [key,r]:insert_runtime_)if(r.graph){auto saved=r.graph->capture();auto* chain=&project.master_inserts;if(!key.empty()){chain=nullptr;for(auto& t:project.tracks)if(t.id.value==key)chain=&t.inserts;}if(!chain)continue;
            for(auto& fx:*chain)if(fx.kind==InsertKind::vst3 && (!authoritative || fx.id!=*authoritative))for(const auto& node:saved.nodes)if(node.id==fx.id && node.processor_id==fx.plugin_path && node.plugin.class_id==fx.class_id){
                const auto& old_chain=insert_chain(*original,key.empty()?std::nullopt:std::optional<Id>{Id{key}});const auto old=std::find_if(old_chain.begin(),old_chain.end(),[&](const auto& f){return f.id==fx.id;});if(old==old_chain.end() || fx.component_state!=old->component_state || fx.controller_state!=old->controller_state)continue;
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
void Application::rebuild_audio() {
    if (!device_ || !device_config_) return;
    auto c = *device_config_;
    c.inputs = audio_name_ == "Offline clock (no sound)" ? std::vector<int>{} : selected_inputs(*services_.projects->state().project);
    const bool reopen = c.inputs != device_config_->inputs;
    if (reopen) {
        require(device_info_.has_value(),"audio device unavailable"); audio::validate_device_config(*device_info_,c);
    }
    // Retain the same open hardware handle; rebuild ONLY after callbacks stop.
    device_->stop();
    try {
        const auto position = engine_->state();
        if (reopen) device_->close();
        audio::RenderGraph graph;
        if (audio_name_ == "Offline clock (no sound)") { prepare_mixer(graph); prepare_midi_clips(graph); prepare_inserts(graph,c); }
        else graph = render(c);
        engine_->prepare({c.sample_rate,static_cast<std::uint32_t>(c.inputs.size()),static_cast<std::uint32_t>(c.outputs.size()),8192,c.buffer_frames,c.processing_workers},std::move(graph),position);
        if (reopen) device_->open(c,engine_);
        device_config_ = c; device_->start(); poll();
    } catch (...) { disconnect(); throw; }
}
void Application::edit(const ICommand& command,std::optional<Id> authoritative) {
    require_not_playing();
    auto candidate = *services_.projects->state().project;
    command.apply(candidate); candidate.validate();
    {std::vector<Id> tracks;for(const auto& t:candidate.tracks)if(t.kind!=TrackKind::midi)tracks.push_back(t.id);(void)audio::compile_midi_clips(candidate,tracks);(void)audio::compile_midi_events(candidate,tracks);}
    capture_insert_state(candidate,authoritative);candidate.validate();
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
        require(clip.source_offset <= data->frames() && clip.length <= data->frames()-clip.source_offset,"clip exceeds source audio");
        if (data->file) {
            ++streamed; bytes += 8*8192*static_cast<std::size_t>(data->channels)*sizeof(float);
        }
    }
    require(streamed <= 32 && bytes <= 256*1024*1024,"disk voice budget exceeded (32 voices / 256 MiB)");
    class PreparedEdit final:public ICommand{Project project_;std::string name_;public:PreparedEdit(Project p,std::string_view name):project_(std::move(p)),name_(name){}std::string_view name() const override{return name_;}void apply(Project& p) const override{p=project_;}};
    services_.projects->execute(PreparedEdit{std::move(candidate),command.name()}); sync_arm(); rebuild_audio();
}
Id Application::add_audio_track(std::string name) {
    require(!name.empty() && name.size() <= 4096,"enter a track name");
    auto id = new_id(); edit(AddTrack{{id,std::move(name),TrackKind::audio,{}}}); return id;
}
Id Application::add_instrument_track(std::string name) {
    require(!name.empty()&&name.size()<=4096,"enter a track name");
    auto id=new_id();edit(AddTrack{{id,std::move(name),TrackKind::instrument,{}}});return id;
}
void Application::set_midi_input(const Id& id,std::string port,int channel,bool monitor){edit(SetMidiInput{id,std::move(port),channel,monitor});publish_midi_routes();}
std::string Application::midi_status(const Id& id)const{
    const auto p=services_.projects->state().project;const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& track){return track.id==id;});
    if(t==p->tracks.end()||t->kind!=TrackKind::instrument)return {};
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
    std::vector<MidiInputRoute> routes;std::size_t index=0;
    for(const auto& track:snapshot.project->tracks)if(track.kind!=TrackKind::midi){if(track.kind==TrackKind::instrument&&(track.midi_monitor||track_armed(track.id))&&!track.midi_input.empty())routes.push_back({track.midi_input,index,track.midi_channel});++index;}
    if(!device_config_){routes.clear();}
    if(!routes.empty()&&!midi_inputs_)midi_inputs_=std::make_unique<MidiInputs>(engine_);
    if(midi_inputs_)midi_inputs_->routes(std::move(routes),generation);
    midi_route_revision_=snapshot.revision;midi_route_generation_=generation;
}
Id Application::add_bus(std::string name) {
    require(!name.empty() && name.size() <= 4096,"enter a bus name");
    auto id = new_id(); edit(AddTrack{{id,std::move(name),TrackKind::bus,{}}}); return id;
}
void Application::set_track_output(const Id& id, std::optional<Id> output) { edit(SetTrackOutput{id,std::move(output)}); }
Id Application::add_return_send(const Id& source, std::string name) {
    require(!name.empty() && name.size() <= 4096,"enter a return name");
    class CreateReturn final : public ICommand {
    public:
        Id source; Track bus;
        CreateReturn(Id from, Track destination) : source(std::move(from)), bus(std::move(destination)) {}
        std::string_view name() const override { return "Create return and send"; }
        void apply(Project& p) const override {
            auto t = std::find_if(p.tracks.begin(),p.tracks.end(),[&](const auto& track) { return track.id == source; });
            require(t != p.tracks.end() && t->kind != TrackKind::midi,"send requires an audio channel");
            t->sends.push_back({bus.id,1,false}); p.tracks.push_back(bus);
        }
    };
    const auto id = new_id(); edit(CreateReturn{source,{id,std::move(name),TrackKind::bus,{}}}); return id;
}
void Application::remove_track(const Id& id) { edit(RemoveTrack{id}); }
void Application::reorder_track(const Id& id, std::size_t index) { edit(ReorderTrack{id,index}); }
bool Application::history(bool redo) {
    require_not_recording();
    const auto target = services_.projects->history_target(redo);
    if (!target) return false;
    auto before = *services_.projects->state().project, after = *target;
    before.master_gain = after.master_gain = 1;
    const bool partitioned=device_config_&&device_config_->process_buffer_frames&&device_config_->processing_workers>=2&&audio_name_!="Offline clock (no sound)";
    for (auto& t : before.tracks) { t.mix = {}; if(!partitioned||track_armed(t.id))t.input_monitor=false; for (auto& send : t.sends) send.gain = 1; }
    for (auto& t : after.tracks) { t.mix = {}; if(!partitioned||track_armed(t.id))t.input_monitor=false; for (auto& send : t.sends) send.gain = 1; }
    bool inserts_only=true;
    if(!parameter_only(before.master_inserts,after.master_inserts))inserts_only=false;
    before.master_inserts=after.master_inserts;
    if(before.tracks.size()!=after.tracks.size())inserts_only=false;
    else for(std::size_t i=0;i<before.tracks.size();++i){if(!parameter_only(before.tracks[i].inserts,after.tracks[i].inserts))inserts_only=false;before.tracks[i].inserts=after.tracks[i].inserts;}
    bool mix_only = inserts_only && before == after;
    if (mix_only && device_config_ && selected_inputs(*target) != device_config_->inputs) mix_only=false;
    if (!mix_only) {
        require_not_playing();
        if (device_config_ && audio_name_ != "Offline clock (no sound)") validate_hardware(*target,*device_config_);
        if (device_ && device_config_ && audio_name_ != "Offline clock (no sound)") {
            auto c = *device_config_; c.inputs = selected_inputs(*target);
            require(device_info_.has_value(),"audio device unavailable"); audio::validate_device_config(*device_info_,c);
        }
    }
    const bool changed = redo ? services_.projects->redo() : services_.projects->undo();
    if (changed) { sync_arm(); if (mix_only) {publish_mix();cancel_insert_preview();} else rebuild_audio(); }
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
        for (std::size_t j=0; j<t.sends.size(); ++j) update.send_gains[update.count][j] = t.sends[j].gain;
        update.input_monitoring[update.count]=t.input_monitor; update.tracks[update.count++] = t.mix;
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
        for (std::size_t j=0; j<t.sends.size(); ++j) update.send_gains[update.count][j] = t.sends[j].gain;
        update.input_monitoring[update.count]=t.input_monitor; update.tracks[update.count++] = id && t.id == *id ? mix : t.mix;
    }
    return engine_->enqueue_mix(update);
}
Sample Application::source_frames(const Id& id) {
    const auto p = services_.projects->state().project;
    const auto it = std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip) { return clip.id == id; });
    require(it != p->clips.end(),"unknown clip");
    return asset(it->source)->frames();
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
    if(c->midi)edit(SplitMidiClip{id,Timeline(p->time,p->sample_rate).to_ticks(position),right});else edit(SplitAudioClip{id,position,right});return right;
}
void Application::remove_clip(const Id& id) { edit(RemoveClip{id}); }
Id Application::create_midi_clip(const Id& track,Tick start,Tick length){Clip c;c.id=new_id();c.track=track;c.name="MIDI clip";c.midi=MidiClip{start,length,0,{}};edit(AddMidiClip{c});return c.id;}
bool Application::audition_note(const Id& track,int pitch,int velocity,int channel,bool on){
    if(recording()||pitch<0||pitch>127||velocity<1||velocity>127||channel<0||channel>15)return false;
    const auto at=std::find(mixer_tracks_.begin(),mixer_tracks_.end(),track);if(at==mixer_tracks_.end())return false;
    return engine_->enqueue_audition_midi({engine_->midi_generation(),static_cast<std::size_t>(at-mixer_tracks_.begin()),{0,on?processing::MidiKind::note_on:processing::MidiKind::note_off,static_cast<std::uint8_t>(channel),static_cast<std::uint8_t>(pitch),static_cast<std::uint8_t>(velocity)},0});
}
void Application::set_midi_notes(const Id& id,std::vector<MidiNote> notes){edit(SetMidiNotes{id,std::move(notes)});}
Id Application::duplicate_clip(const Id& id){const auto duplicate=new_id();edit(DuplicateClip{id,duplicate});return duplicate;}
void Application::set_time_map(TimeMap time){const auto p=services_.projects->state().project;edit(SetMusicalData{std::move(time),p->chords,p->sections,p->markers});}
void Application::import_wavs(const std::vector<std::filesystem::path>& paths,std::optional<Id> target,Sample start) {
    require_not_playing(); require(!paths.empty() && paths.size() <= audio::max_voices,"select 1..128 WAV files");
    const auto current = services_.projects->state().project;
    require(current->clips.size()+paths.size() <= audio::max_voices,"too many playback clips");
    require(start>=0,"invalid WAV drop position");
    if(target){auto at=std::find_if(current->tracks.begin(),current->tracks.end(),[&](const auto& track){return track.id==*target;});require(at!=current->tracks.end()&&at->kind==TrackKind::audio,"Drop samples on an audio track or empty arrangement");}

    // Inspect the entire source batch before copying content.
    for (const auto& path : paths) require(audio::inspect_wav(path).sample_rate == current->sample_rate,"WAV/project sample-rate mismatch; import WAVs at the project rate");
    std::unique_ptr<MediaCopy> copies;
    if (!path_.empty()) copies = std::make_unique<MediaCopy>(asset_root_);
    std::vector<Track> tracks; std::vector<Clip> clips;
    std::map<std::string,std::shared_ptr<const audio::AudioData>> decoded;
    std::size_t bytes{};
    for (const auto& [key,cached] : assets_) { (void)key; bytes += cached.data->samples.size()*sizeof(float); }
    for (const auto& original : paths) {
        const auto path = copies ? copies->media(original) : std::filesystem::absolute(original).lexically_normal();
        const auto source = (copies ? media_ref(path.lexically_relative(asset_root_)) : utf8(path));
        std::shared_ptr<const audio::AudioData> data;
        if (assets_.contains(source)) data = assets_.at(source).data;
        else if (decoded.contains(source)) data = decoded.at(source);
        else {
            require(assets_.size()+decoded.size() < 128,"retained source limit reached (128); start a new project to release Undo media");
            require(bytes < 512*1024*1024,"project preload/cache exceeds 512 MiB");
            data = std::make_shared<const audio::AudioData>(audio::open_wav(path,std::min<std::size_t>(8*1024*1024,512*1024*1024-bytes)));
            bytes += data->samples.size()*sizeof(float); decoded.emplace(source,data);
        }
        const auto id = target ? *target : new_id();
        if(!target)tracks.push_back({id,utf8(original.stem()),TrackKind::audio,{}});
        require(data->frames()<=std::numeric_limits<Sample>::max()-start,"WAV drop exceeds timeline range");
        clips.push_back({new_id(),id,utf8(original.filename()),start,data->frames(),0,source});
        if(target)start+=data->frames();
    }
    const auto revision = services_.projects->state().revision;
    std::vector<std::string> cached;
    try {
        for (auto& [source,data] : decoded) { cache_asset(source,std::move(data)); cached.push_back(source); }
        edit(ImportAudio{std::move(tracks),std::move(clips)});
        if (copies) copies->commit();
    } catch (...) {
        if (services_.projects->state().revision != revision) { if (copies) copies->commit(); } // committed clips must retain their media
        else for (const auto& source : cached) { *assets_.at(source).cancel = true; assets_.erase(source); }
        throw;
    }
}
void Application::prepare_mixer(audio::RenderGraph& result) {
    const auto project = services_.projects->state().project;
    mixer_tracks_.clear();
    for (const auto& t : project->tracks) if (t.kind != TrackKind::midi) {
        require(mixer_tracks_.size() < audio::max_mixer_tracks,"mixer supports up to 128 audio tracks and buses");
        mixer_tracks_.push_back(t.id); result.mixer.push_back(t.mix); result.input_monitoring.push_back(t.input_monitor); result.buses.push_back(t.kind == TrackKind::bus);result.live_midi.push_back(t.kind==TrackKind::instrument);result.midi_monitor.push_back(t.midi_monitor);
    }
    for (const auto& t : project->tracks) if (t.kind != TrackKind::midi) {
        const auto destination = t.output ? std::find(mixer_tracks_.begin(),mixer_tracks_.end(),*t.output) : mixer_tracks_.end();
        require(!t.output || destination != mixer_tracks_.end(),"missing output bus");
        result.sends.emplace_back();
        for (const auto& send : t.sends) {
            const auto bus = std::find(mixer_tracks_.begin(),mixer_tracks_.end(),send.bus);
            require(bus != mixer_tracks_.end(),"missing send bus");
            result.sends.back().push_back({static_cast<std::size_t>(bus-mixer_tracks_.begin()),send.gain,send.pre_fader});
        }
        result.outputs.push_back(destination == mixer_tracks_.end() ? audio::no_mixer_track : static_cast<std::size_t>(destination-mixer_tracks_.begin()));
    }
    result.master_gain = project->master_gain;
    applied_mix_revision_ = services_.projects->state().revision;
}
void Application::prepare_midi_clips(audio::RenderGraph& graph){
    const auto p=services_.projects->state().project;const Timeline time(p->time,p->sample_rate);graph.midi_events=audio::compile_midi_events(*services_.projects->state().project,mixer_tracks_);graph.midi_notes=audio::compile_midi_clips(*p,mixer_tracks_);
    for(const auto& tempo:p->time.tempos)graph.tempos.push_back({time.to_samples(tempo.tick),tempo.bpm,static_cast<double>(tempo.tick)/ppq});
}
audio::RenderGraph Application::render(const audio::DeviceConfig& c) {
    require(c.sample_rate == services_.projects->state().project->sample_rate,"device/project rate mismatch; choose the project rate (resampling is a later stage)");
    require(!c.outputs.empty() && c.outputs.size() <= audio::max_channels && c.inputs.size() <= audio::max_channels,"invalid channel selection");
    validate_hardware(*services_.projects->state().project,c);
    audio::RenderGraph result;
    for (const auto& capture : captures_) result.recordings.push_back(capture.recorder);
    for(const auto& capture:midi_captures_){const auto at=std::find(mixer_tracks_.begin(),mixer_tracks_.end(),capture.track);require(at!=mixer_tracks_.end(),"missing MIDI record track");result.midi_recordings.push_back({static_cast<std::size_t>(at-mixer_tracks_.begin()),capture.recorder});}
    result.monitoring = monitoring_;
    const auto config = processing::ProcessConfig{c.sample_rate,static_cast<std::uint32_t>(c.outputs.size()),8192,c.buffer_frames};
    prepared_ = std::make_shared<processing::PreparedGraph>(graphs_->state(),config);
    result.processors = prepared_;
    const auto project = services_.projects->state().project;
    prepare_mixer(result);
    prepare_midi_clips(result);
    prepare_inserts(result,c);
    const auto mapped = [&](const std::vector<int>& outputs) {
        std::vector<std::size_t> result;
        for (const auto channel : outputs) {
            const auto found = std::find(c.outputs.begin(),c.outputs.end(),channel);
            require(found != c.outputs.end(),"missing physical output in stream");
            result.push_back(static_cast<std::size_t>(found-c.outputs.begin()));
        }
        return result;
    };
    result.master_outputs = project->master_outputs.empty() ? std::vector<std::size_t>{0} : mapped(project->master_outputs);
    if (project->master_outputs.empty() && c.outputs.size()>1) result.master_outputs.push_back(1);
    for (const auto& track : project->tracks) if (track.kind != TrackKind::midi) result.hardware_outputs.push_back(mapped(track.hardware_outputs));
    for (const auto& clip : project->clips) {
        if(clip.midi)continue;
        require(result.voices.size() < audio::max_voices,"too many playback voices");
        auto asset_data = asset(clip.source);
        require(asset_data->sample_rate == c.sample_rate,"WAV/project sample-rate mismatch");
        audio::Voice voice{asset_data,clip.start,clip.source_offset,clip.length,{}};
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
    for (const auto& track : project->tracks) if (track.kind == TrackKind::audio) {
        // A connected physical selector alone is not a live DSP dependency.
        // Mixed ownership changes are rebuilt while stopped by Monitor/Arm.
        if(c.process_buffer_frames&&c.processing_workers>=2&&!track.input_monitor&&!track_armed(track.id))continue;
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
void Application::prepare_inserts(audio::RenderGraph& result,const audio::DeviceConfig& c) {
    const auto project=services_.projects->state().project;
    ++insert_generation_;insert_runtime_.clear();instrument_errors_.clear();
    const auto chain=[&](const std::vector<NativeInsert>& effects,std::uint32_t block,bool instrument=false) -> std::shared_ptr<processing::PreparedGraph> {
        if (effects.empty()) return {};
        auto saved=std::make_shared<const processing::GraphState>(processing::insert_graph(effects));
        return std::make_shared<processing::PreparedGraph>(processing::GraphSnapshot{saved,0,false,false},processing::ProcessConfig{c.sample_rate,static_cast<std::uint32_t>(c.outputs.size()),block,c.buffer_frames,instrument}
#ifdef MRS_HAS_VST3
            ,processing::hosted_factory
#endif
        );
    };
    for (const auto& t : project->tracks) if (t.kind != TrackKind::midi) {std::shared_ptr<processing::PreparedGraph> prepared;try{prepared=chain(t.inserts,c.buffer_frames,t.kind==TrackKind::instrument);}catch(const std::exception& e){if(t.kind!=TrackKind::instrument)throw;instrument_errors_[t.id.value]=e.what();}result.inserts.push_back(prepared);insert_runtime_[t.id.value]={prepared,{}};}
    result.master_inserts=chain(project->master_inserts,8192);insert_runtime_[std::string{}]={result.master_inserts,{}};
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
        if (offline) { prepare_mixer(graph); prepare_midi_clips(graph); prepare_inserts(graph,c); }
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
    ++insert_generation_;insert_runtime_.clear(); prepared_.reset(); device_config_.reset(); device_info_.reset(); audio_name_ = "Disconnected";
    if (transport_) { engine_->prepare({engine_->config().sample_rate,0,2,8192},{}); transport_->poll(); }
}
void Application::poll() {
    publish_midi_routes();
    publish_inserts();
    for(auto& [key,r]:insert_runtime_){(void)key;if(r.graph && r.graph->consume_edits())unsaved_=true;}
    publish_mix();
    if (recording() && (std::any_of(captures_.begin(),captures_.end(),[](const auto& capture) { return capture.recorder->status().fault != audio::RecordFault::none; }) ||
        ((recording_?recording_->status().frames>0:!midi_captures_.empty()&&midi_captures_.front().recorder->end()>midi_captures_.front().recorder->start()) && engine_->state().playback != PlaybackState::playing) || std::any_of(midi_captures_.begin(),midi_captures_.end(),[](const auto& c){return c.recorder->fault();}) ||
        device_status().phase != audio::DevicePhase::running)) {
        try { (void)stop_recording(); } catch (const std::exception& e) { recording_error_ = e.what(); }
    }
    transport_->poll();
    for (auto& [key,value] : assets_) {
        (void)key;
        if (value.pending.valid() && value.pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            try { value.peaks = value.pending.get(); } catch (const std::exception& e) { waveform_error_ = e.what(); }
    }
}
audio::DeviceStatus Application::device_status() { return device_ ? device_->status() : audio::DeviceStatus{}; }
bool Application::audio_running() { return device_status().phase == audio::DevicePhase::running; }
void Application::play() { require(audio_running(),"connect audio or choose Offline clock first"); transport_->play(); }
void Application::pause() { if (recording()) (void)stop_recording(); else transport_->pause(); }
void Application::stop() { if (recording()) (void)stop_recording(); transport_->stop(); }
void Application::seek(Sample sample) { require_not_recording(); transport_->seek(sample); }

void Application::set_track_armed(const Id& id, bool enabled) {
    require_not_playing();
    const auto p=services_.projects->state().project;
    require(std::any_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t) { return t.id == id && (t.kind == TrackKind::audio||t.kind==TrackKind::instrument); }),"arm an audio or instrument track");
    const auto before=armed_tracks_;
    require(!enabled || track_armed(id) || armed_tracks_.size() < 32,"recording supports up to 32 armed tracks");
    if (enabled && !track_armed(id)) armed_tracks_.push_back(id); else if (!enabled) std::erase(armed_tracks_,id);
    try {
        if (device_config_ && audio_name_ != "Offline clock (no sound)") { auto c=*device_config_; c.inputs=selected_inputs(*p); audio::validate_device_config(*device_info_,c); }
        sync_arm(); rebuild_audio();
    } catch (...) { armed_tracks_=before; sync_arm(); throw; }
}
void Application::arm_track(std::optional<Id> id) {
    require_not_playing(); const auto before=armed_tracks_; armed_tracks_.clear();
    try { if (id) { set_track_armed(*id,true); set_track_monitoring(*id,true); } else { sync_arm(); rebuild_audio(); } }
    catch (...) { armed_tracks_=before; sync_arm(); throw; }
}
void Application::set_track_monitoring(const Id& id, bool enabled) {
    const SetTrackMonitoring command{id,enabled}; auto candidate=*services_.projects->state().project;
    command.apply(candidate); candidate.validate();
    const auto track=std::find_if(services_.projects->state().project->tracks.begin(),services_.projects->state().project->tracks.end(),[&](const auto& t){return t.id==id;});
    if(device_config_&&device_config_->process_buffer_frames&&device_config_->processing_workers>=2&&audio_name_!="Offline clock (no sound)"&&
        !track_armed(id)&&track->input_monitor!=enabled){edit(command);return;}
    auto next=device_config_; if (next) next->inputs=selected_inputs(candidate);
    const bool rebind=enabled && next && audio_name_ != "Offline clock (no sound)" &&
        std::any_of(next->inputs.begin(),next->inputs.end(),[&](int input) {
            return std::find(device_config_->inputs.begin(),device_config_->inputs.end(),input) == device_config_->inputs.end();
        });
    if (rebind) require_not_playing();
    if (rebind) audio::validate_device_config(*device_info_,*next);
    services_.projects->execute(command);
    if (rebind) rebuild_audio(); else publish_mix();
}
void Application::monitoring(bool enabled) {
    require(engine_->enqueue({audio::ControlKind::monitor,enabled ? 1 : 0}),"audio command queue full");
    monitoring_ = enabled;
}
audio::RecordStatus Application::recording_status() const {if(recording_)return recording_->status();if(!midi_captures_.empty()){audio::RecordStatus status;status.frames=static_cast<std::uint64_t>(midi_captures_.front().recorder->end()-midi_captures_.front().recorder->start());if(midi_captures_.front().recorder->fault())status.fault=audio::RecordFault::overflow;return status;}return last_recording_status_;}
void Application::start_recording(const std::filesystem::path& destination) {
    require_not_playing(); sync_arm(); require(!armed_tracks_.empty(),"Arm one or more audio/instrument tracks before recording");
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
        if(track->kind==TrackKind::instrument){const Timeline time(p->time,p->sample_rate);require(std::none_of(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.track==id&&clip.midi&&clip_end(clip,time)>position.sample;}),"Linear MIDI recording: move the cursor after existing clips or use a new instrument track");require(!track->midi_input.empty(),"select a MIDI input for the armed instrument track");midi_pending.push_back(id);continue;}
        const auto inputs=track_inputs(*track,default_inputs_); require(!inputs.empty(),"armed track has no selected input");
        Pending take{id,{},destination};
        for (const auto input : inputs) { const auto found=std::find(device_config_->inputs.begin(),device_config_->inputs.end(),input); require(found != device_config_->inputs.end(),"armed input is not active"); take.selectors.push_back(static_cast<std::uint32_t>(found-device_config_->inputs.begin())); }
        if (std::count_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.kind==TrackKind::audio&&track_armed(t.id);})>1) take.path=destination.parent_path()/(destination.stem().wstring()+L"-"+std::to_wstring(i+1)+L".wav");
        require(!std::filesystem::exists(take.path),"recording destination exists; choose a new take name");
        bytes+=8*8192*inputs.size()*sizeof(float); pending.push_back(std::move(take));
    }
    require(disk+pending.size() <= 32 && bytes <= 256*1024*1024,"no disk voice capacity for recording");
    device_->stop(); position=engine_->state();
    try {
        recording_error_.clear(); last_take_.clear(); last_takes_.clear(); last_recording_status_={};
        for (const auto& take : pending) captures_.push_back({take.track,std::make_shared<audio::Recorder>(take.path,p->sample_rate,position.sample,take.selectors)});
        std::size_t project_notes{},project_events{};for(const auto& clip:p->clips)if(clip.midi){project_notes+=clip.midi->notes.size();project_events+=clip.midi->events.size();}
        for(const auto& id:midi_pending){std::size_t track_notes{},track_events{};std::vector<std::uint16_t> keys;for(const auto& clip:p->clips)if(clip.midi&&clip.track==id){track_notes+=clip.midi->notes.size();track_events+=clip.midi->events.size()+16;for(const auto& event:clip.midi->events)keys.push_back(static_cast<std::uint16_t>((event.kind*16+event.channel)*128+((event.kind==2||event.kind==6)?event.data1:0)));}
            const auto note_limit=std::min<std::size_t>(4096-std::min<std::size_t>(track_notes,4096),(32768-project_notes)/midi_pending.size());
            const auto event_limit=std::min<std::size_t>(8192-std::min<std::size_t>(track_events,8192),(65536-project_events)/midi_pending.size());require(event_limit>32,"no MIDI controller recording capacity");
            midi_captures_.push_back({id,std::make_shared<audio::MidiRecorder>(position.sample,note_limit,std::min<std::size_t>(8176,event_limit-32),std::move(keys))});
        }
        if(!captures_.empty())recording_=captures_.front().recorder;
        engine_->prepare({device_config_->sample_rate,static_cast<std::uint32_t>(device_config_->inputs.size()),static_cast<std::uint32_t>(device_config_->outputs.size()),8192,device_config_->buffer_frames,device_config_->processing_workers},render(*device_config_),position);
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
                if (!result.frames) continue;
                last_takes_.push_back(result.path); if (last_take_.empty()) last_take_=result.path;
                command.clips.push_back({new_id(),capture.track,utf8(result.path.stem()),capture.recorder->start(),result.frames,0,utf8(result.path)});
            } catch (const std::exception& e) { failure=e.what(); }
        }
        const auto project=services_.projects->state().project;const Timeline time(project->time,project->sample_rate);
        for(const auto& capture:midi_sessions){auto data=capture.recorder->finish(time);if(capture.recorder->fault())recording_error_="MIDI recording interrupted (input/queue/capacity); captured prefix retained";if(data.notes.empty()&&data.events.empty())continue;Clip clip;clip.id=new_id();clip.track=capture.track;clip.name="MIDI take";clip.midi=std::move(data);command.clips.push_back(std::move(clip));}
        if (!command.clips.empty()) edit(command); else rebuild_audio();
        if (!failure.empty()) throw std::runtime_error(failure);
        return !command.clips.empty();
    } catch (const std::exception& e) {
        recording_error_=e.what(); if (!last_take_.empty()) recording_error_+="; retained file: "+utf8(last_take_);
        if (device_ && device_config_) try { rebuild_audio(); } catch (...) { disconnect(); }
        throw;
    }
}
} // namespace mrs::desktop
