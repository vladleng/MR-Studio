#include <mrs/desktop.hpp>
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
}
std::string_view workspace_name(Workspace w) {
    switch (w) { case Workspace::arrange: return "Arrange"; case Workspace::edit: return "Edit";
        case Workspace::mix: return "Mix"; case Workspace::live: return "Live"; }
    throw std::invalid_argument("unknown workspace");
}
void Preferences::validate() const {
    (void)workspace_name(workspace); require(rate >= 8000 && rate <= 768000,"invalid sample rate");
    require(buffer >= 8 && buffer <= 8192,"invalid buffer"); require(monitor_input >= -1 && monitor_input < 64,"invalid monitor input");
    require(!outputs.empty() && outputs.size() <= 64 && device_name.size() <= 4096,"invalid device preferences");
    std::set<int> seen; for (auto o : outputs) require(o >= 0 && o < 64 && seen.insert(o).second,"invalid/duplicate output");
}
std::string encode_preferences(const Preferences& p) {
    p.validate(); std::ostringstream out;
    out << "MRS_DESKTOP_CONFIG 2\n" << static_cast<int>(p.workspace) << ' ' << p.rate << ' ' << p.buffer << ' ' << p.monitor_input
        << ' ' << std::quoted(p.device_name) << ' ' << p.outputs.size();
    for (auto o : p.outputs) out << ' ' << o;
    out << ' ' << p.reconnect_audio << "\n"; return out.str();
}
Preferences decode_preferences(std::string_view bytes) {
    require(bytes.size() <= 16384,"config too large"); std::istringstream in{std::string(bytes)};
    std::string magic; int version{},workspace{}; std::size_t count{}; Preferences p;
    require(static_cast<bool>(in >> magic >> version) && magic == "MRS_DESKTOP_CONFIG" && (version == 1 || version == 2),"unsupported config");
    require(static_cast<bool>(in >> workspace >> p.rate >> p.buffer >> p.monitor_input >> std::quoted(p.device_name) >> count) && workspace >= 0 && workspace <= 3 && count > 0 && count <= 64,"invalid config");
    p.workspace = static_cast<Workspace>(workspace); p.outputs.clear();
    for (std::size_t i = 0; i < count; ++i) { int o{}; require(static_cast<bool>(in >> o),"truncated config"); p.outputs.push_back(o); }
    if (version == 2) {
        int enabled{}; require(static_cast<bool>(in >> enabled) && (enabled == 0 || enabled == 1),"invalid reconnect preference");
        p.reconnect_audio = enabled != 0;
    } else p.reconnect_audio = !p.device_name.empty();
    in >> std::ws; require(in.eof(),"extra config data"); p.validate(); return p;
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
Application::~Application() { if (device_) device_->close(); }
void Application::workspace(Workspace w) { (void)workspace_name(w); workspace_ = w; }
void Application::replace(persistence::ProjectDocument next) {
    next.validate();
    // Build application models before releasing the old session.
    auto projects = std::make_shared<ProjectStore>(next.project);
    auto graphs = std::make_shared<processing::GraphStore>(next.graph);
    disconnect();
    engine_->prepare({next.project.sample_rate,0,2,8192},{});
    auto transport = std::make_shared<audio::EngineTransport>(engine_,Timeline(next.project.time,next.project.sample_rate));
    auto musical = std::make_unique<MusicalTimeline>(Services{projects,transport});
    assets_.clear();
    document_ = std::move(next);
    services_ = {projects,transport}; graphs_ = std::move(graphs);
    transport_ = std::move(transport); musical_ = std::move(musical);
    saved_project_revision_ = saved_graph_revision_ = 0; unsaved_ = true;
    engine_->prepare({document_.project.sample_rate,0,2,8192},{});
    start_empty_clock(); transport_->poll(); musical_->refresh();
}
void Application::start_empty_clock() {
    auto device = audio::make_offline_device();
    audio::DeviceConfig c{0,document_.project.sample_rate,128,{}, {0,1}};
    device->open(c,engine_); device->start(); device_ = std::move(device); device_config_ = c; audio_name_ = "Offline clock (no sound)";
}
void Application::demo() {
    path_.clear(); asset_root_.clear(); replace(foundation_demo());
}
void Application::open_project(const std::filesystem::path& path) {
    auto next = persistence::load_project(path); replace(std::move(next));
    path_ = path; asset_root_ = path.parent_path(); unsaved_ = false;
}
void Application::import_wav(const std::filesystem::path& path) {
    const auto asset = audio::load_wav(path); // validate before touching the current session
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
    // Base channel state stays available for Undo; only extant tracks serialize.
    std::erase_if(result.mixer,[&](const auto& m) {
        return std::none_of(result.project.tracks.begin(),result.project.tracks.end(),[&](const auto& t) { return t.id == m.track; });
    });
    for (const auto& t : result.project.tracks) if (std::none_of(result.mixer.begin(),result.mixer.end(),[&](const auto& m) { return m.track == t.id; }))
        result.mixer.push_back({t.id,1,0,false,false});
    result.validate(); return result;
}
void Application::save_project(const std::filesystem::path& path) {
    // No runtime parameter editor yet: GraphStore is authoritative, processor state unchanged.
    auto document = snapshot();
    if (std::filesystem::absolute(path.parent_path().empty() ? "." : path.parent_path()) !=
        std::filesystem::absolute(asset_root_.empty() ? "." : asset_root_)) {
        for (const auto& clip : document.project.clips) if (clip.source != "mrs:demo-tone" &&
            std::filesystem::path(std::u8string(clip.source.begin(),clip.source.end())).is_relative())
            throw std::invalid_argument("Save As with relative media requires the original project folder in this foundation build");
    }
    persistence::save_project(path,document); path_ = path;
    saved_project_revision_ = services_.projects->state().revision;
    saved_graph_revision_ = graphs_->state().revision; unsaved_ = false;
}
bool Application::dirty() const {
    return unsaved_ || services_.projects->state().revision != saved_project_revision_ || graphs_->state().revision != saved_graph_revision_;
}
void Application::rename_track(const Id& id, std::string name) {
    require(!name.empty() && name.size() <= 4096,"enter a track name");
    services_.projects->execute(RenameTrack{id,std::move(name)});
}
void Application::require_stopped() const {
    require(engine_->state().playback == PlaybackState::stopped,"Stop playback before changing tracks or importing audio");
}
void Application::new_project(std::uint32_t rate) {
    auto d = foundation_demo(); d.project.id = new_id(); d.project.title = "Untitled";
    d.project.sample_rate = rate; d.project.tracks.clear(); d.project.clips.clear();
    d.project.chords.clear(); d.project.sections.clear(); d.project.markers.clear(); d.mixer.clear(); d.live_notes.clear();
    d.validate(); replace(std::move(d)); path_.clear(); asset_root_.clear();
}
void Application::cache_asset(std::string source, std::shared_ptr<const audio::AudioData> data) {
    if (assets_.contains(source)) return;
    std::size_t bytes = data->samples.size()*sizeof(float);
    for (const auto& [key,cached] : assets_) { (void)key; bytes += cached.data->samples.size()*sizeof(float); }
    require(bytes <= 512*1024*1024,"project preload/cache exceeds 512 MiB");
    CachedAsset value; value.data = data;
    value.pending = std::async(std::launch::async,[data] { return audio::Waveform(*data); });
    assets_.emplace(std::move(source),std::move(value));
}
std::shared_ptr<const audio::AudioData> Application::asset(const std::string& source) {
    if (const auto it = assets_.find(source); it != assets_.end()) return it->second.data;
    audio::AudioData data;
    if (source == "mrs:demo-tone") data = audio::sine_fixture(document_.project.sample_rate,2,32*static_cast<Sample>(document_.project.sample_rate),220);
    else {
        require(!source.empty(),"clip has no audio source");
        auto path = std::filesystem::path(std::u8string(source.begin(),source.end()));
        if (path.is_relative()) path = asset_root_ / path;
        data = audio::load_wav(path);
    }
    auto ptr = std::make_shared<const audio::AudioData>(std::move(data)); cache_asset(source,ptr); return ptr;
}
void Application::prepare_waveforms() {
    for (const auto& clip : services_.projects->state().project->clips) (void)asset(clip.source);
}
const audio::Waveform* Application::waveform(std::string_view source) const {
    const auto it = assets_.find(std::string(source));
    return it != assets_.end() && it->second.peaks ? &*it->second.peaks : nullptr;
}
void Application::rebuild_audio() {
    if (!device_ || !device_config_) return;
    const auto c = *device_config_;
    // Retain the same open hardware handle; rebuild ONLY after callbacks stop.
    device_->stop();
    try {
        auto graph = render(c);
        engine_->prepare({c.sample_rate,static_cast<std::uint32_t>(c.inputs.size()),static_cast<std::uint32_t>(c.outputs.size()),8192},std::move(graph));
        device_->start(); poll();
    } catch (...) { disconnect(); throw; }
}
void Application::edit(const ICommand& command) {
    require_stopped(); services_.projects->execute(command); rebuild_audio();
}
Id Application::add_audio_track(std::string name) {
    require(!name.empty() && name.size() <= 4096,"enter a track name");
    auto id = new_id(); edit(AddTrack{{id,std::move(name),TrackKind::audio,{}}}); return id;
}
void Application::remove_track(const Id& id) { edit(RemoveTrack{id}); }
void Application::reorder_track(const Id& id, std::size_t index) { edit(ReorderTrack{id,index}); }
bool Application::undo() { require_stopped(); const auto changed = services_.projects->undo(); if (changed) rebuild_audio(); return changed; }
bool Application::redo() { require_stopped(); const auto changed = services_.projects->redo(); if (changed) rebuild_audio(); return changed; }
void Application::import_wavs(const std::vector<std::filesystem::path>& paths) {
    require_stopped(); require(!paths.empty() && paths.size() <= audio::max_voices,"select 1..128 WAV files");
    const auto current = services_.projects->state().project;
    require(current->clips.size()+paths.size() <= audio::max_voices,"too many playback clips");
    std::vector<Track> tracks; std::vector<Clip> clips;
    std::map<std::string,std::shared_ptr<const audio::AudioData>> decoded;
    std::size_t bytes{};
    for (const auto& [key,cached] : assets_) { (void)key; bytes += cached.data->samples.size()*sizeof(float); }
    // Decode/validate the WHOLE batch before committing one shared command.
    for (const auto& path : paths) {
        const auto source = utf8(std::filesystem::absolute(path).lexically_normal());
        std::shared_ptr<const audio::AudioData> data;
        if (assets_.contains(source)) data = assets_.at(source).data;
        else if (decoded.contains(source)) data = decoded.at(source);
        else {
            require(bytes < 512*1024*1024,"project preload/cache exceeds 512 MiB");
            data = std::make_shared<const audio::AudioData>(audio::load_wav(path,std::min<std::size_t>(256*1024*1024,512*1024*1024-bytes)));
            bytes += data->samples.size()*sizeof(float); decoded.emplace(source,data);
        }
        require(data->sample_rate == current->sample_rate,"WAV/project sample-rate mismatch; import WAVs at the project rate");
        const auto id = new_id();
        tracks.push_back({id,utf8(path.stem()),TrackKind::audio,{}});
        clips.push_back({new_id(),id,utf8(path.filename()),0,data->frames(),0,source});
    }
    for (auto& [source,data] : decoded) cache_asset(source,std::move(data));
    edit(ImportAudio{std::move(tracks),std::move(clips)});
}
audio::RenderGraph Application::render(const audio::DeviceConfig& c) {
    require(c.sample_rate == services_.projects->state().project->sample_rate,"device/project rate mismatch; choose the project rate (resampling is a later stage)");
    require(!c.outputs.empty() && c.outputs.size() <= audio::max_channels && c.inputs.size() <= 1,"invalid channel selection");
    audio::RenderGraph result;
    const auto config = processing::ProcessConfig{c.sample_rate,static_cast<std::uint32_t>(c.outputs.size()),8192,c.buffer_frames};
    prepared_ = std::make_shared<processing::PreparedGraph>(graphs_->state(),config);
    result.processors = prepared_;
    const auto project = services_.projects->state().project;
    std::size_t decoded_bytes{};
    for (const auto& clip : project->clips) {
        require(result.voices.size() < audio::max_voices,"too many playback voices");
        auto asset_data = asset(clip.source);
        require(asset_data->sample_rate == c.sample_rate,"WAV/project sample-rate mismatch");
        require(asset_data->samples.size()*sizeof(float) <= 512*1024*1024-decoded_bytes,"project preload exceeds 512 MiB");
        decoded_bytes += asset_data->samples.size()*sizeof(float);
        audio::Voice voice{asset_data,clip.start,clip.source_offset,clip.length,{}};
        for (std::uint32_t channel = 0; channel < asset_data->channels; ++channel)
            voice.routes.push_back({channel,channel % static_cast<std::uint32_t>(c.outputs.size()),clip.source == "mrs:demo-tone" ? 0.15f : 1.0f});
        result.voices.push_back(std::move(voice));
    }
    if (!c.inputs.empty()) for (std::uint32_t channel = 0; channel < c.outputs.size(); ++channel) result.monitor.push_back({0,channel,1});
    return result;
}
void Application::connect(std::unique_ptr<audio::IAudioDevice> device, audio::DeviceConfig c) {
    require(static_cast<bool>(device),"missing audio backend");
    auto infos = device->enumerate();
    const auto info = std::find_if(infos.begin(),infos.end(),[&](const auto& v) { return v.index == c.device; });
    require(info != infos.end(),"audio device no longer available"); audio::validate_device_config(*info,c);
    // Existing callbacks must be stopped before preparing or releasing graphs.
    disconnect();
    try {
        auto graph = render(c);
        engine_->prepare({c.sample_rate,static_cast<std::uint32_t>(c.inputs.size()),static_cast<std::uint32_t>(c.outputs.size()),8192},std::move(graph));
        device->open(c,engine_); device->start(); audio_name_ = info->name; device_ = std::move(device); device_config_ = c; poll();
    } catch (...) {
        device->close(); prepared_.reset();
        engine_->prepare({services_.projects->state().project->sample_rate,0,2,8192},{});
        transport_->poll(); throw;
    }
}
void Application::disconnect() {
    if (device_) { device_->close(); device_.reset(); }
    prepared_.reset(); device_config_.reset(); audio_name_ = "Disconnected";
    if (transport_) { engine_->prepare({engine_->config().sample_rate,0,2,8192},{}); transport_->poll(); }
}
void Application::poll() {
    transport_->poll();
    for (auto& [key,value] : assets_) {
        (void)key;
        if (value.pending.valid() && value.pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            value.peaks = value.pending.get();
    }
}
audio::DeviceStatus Application::device_status() { return device_ ? device_->status() : audio::DeviceStatus{}; }
bool Application::audio_running() { return device_status().phase == audio::DevicePhase::running; }
void Application::play() { require(audio_running(),"connect audio or choose Offline clock first"); transport_->play(); }
void Application::pause() { transport_->pause(); }
void Application::stop() { transport_->stop(); }
void Application::seek(Sample sample) { transport_->seek(sample); }
} // namespace mrs::desktop
