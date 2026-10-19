#pragma once
#include <atomic>
#include <functional>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <mrs/core.hpp>
namespace mrs::audio { struct AudioData; struct RenderGraph; struct RealtimeState; class VoiceRenderer; class EditorRenderer; }

namespace mrs::ara {
enum class CapabilityStatus { unsupported, available, incompatible };
struct Capability {
    std::string class_id, name, factory_id, version, archive_id, diagnostic;
    CapabilityStatus status{CapabilityStatus::unsupported};
    int api_generation{};
};
struct SourceSnapshot {
    std::string id;
    std::uint64_t revision{};
    std::shared_ptr<const audio::AudioData> pcm;
};
struct ClipCache {
    Clip clip;
    std::uint64_t source_revision{};
    std::shared_ptr<const audio::AudioData> pcm;
    std::uint32_t compensated_latency{};
};
struct ModelStats { std::size_t sources{}, modifications{}, regions{}, readers{}; std::uint64_t reads{}, read_errors{}; std::size_t sequences{}; };
enum PreparationCause : std::uint32_t {
    preparation_model=1, preparation_parameter=2, preparation_analysis=4,
    preparation_restart=8, preparation_explicit=16, preparation_off_thread=32,
    preparation_unknown_range=64, preparation_context=128, preparation_latency=256
};
struct PreparationStats {std::size_t reused{},full{},partial{};std::uint64_t rendered_frames{},milliseconds{};std::uint32_t causes{};std::size_t live{};};
enum class TransportAction { play, stop, seek, cycle_range, cycle_enabled };
struct TransportRequest { TransportAction action{}; double position{},duration{}; bool enabled{}; };
// Control/UI thread only. Untrusted modules must be probed in mrs_ara_scan,
// never from the audio callback. Does not initialize ARA or create audio DSP.
std::vector<Capability> probe(const std::string& module_path);
// Worker/control thread, never RT. Hidden child, bounded output, cancellation
// and timeout; no persistent VST3 cache or user project changes.
std::vector<Capability> probe_isolated(const std::string& module_path,
    const std::filesystem::path& helper, unsigned timeout_ms = 10000,
    std::shared_ptr<std::atomic<bool>> cancel = {});

// Runtime ARA document and stopped PCM preparation, separate from inserts.
// Create/use/destroy on the same UI thread; processing workers are joined
// before model mutation/destruction. Project/Undo remain owned by Core;
// document archiving belongs to P4.
class Session final {
public:
    static std::unique_ptr<Session> open(const std::string& module_path,
        const std::string& class_id, std::shared_ptr<std::atomic<bool>> cancel = {});
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    const Capability& capability() const;
    // Always balances endEditing, including exceptions. Nested edits rejected.
    void edit(const std::function<void()>& operation);
    void notify();
    bool roles_bound() const;
    // Stopped/control-thread preparation. Default is compensated immutable PCM;
    // prefer_live opts into separate zero-latency playback-only instances whose
    // lifetime is retained by the prepared graph, with no RT reclamation.
    std::vector<ClipCache> synchronize(const Project&, const std::vector<SourceSnapshot>&,
        std::shared_ptr<std::atomic<bool>> cancel = {});
    ModelStats model_stats() const;
    std::uint64_t content_revision() const;
    std::uint64_t state_revision() const;
    void invalidate();
    PreparationStats preparation_stats() const;
    void prefer_live(bool);
    bool live_current() const;
    bool live_failed() const;
    std::shared_ptr<audio::VoiceRenderer> live_renderer(const Id&) const;
    std::shared_ptr<audio::EditorRenderer> editor_renderer() const;
    bool open_editor(void* parent,int& width,int& height);
    bool resize_editor(int& width,int& height);
    void close_editor();
    void select_clip(const Id&);
    bool analysing() const;
    void set_preparation(std::shared_ptr<std::atomic<bool>>,std::function<void()>);
    void transport(const Project&,const audio::RealtimeState&);
    std::vector<TransportRequest> take_transport_requests();
    bool transport_failed() const;
    void persistent_id(std::string);
    void configure_source(const AraBinding&);
    void save_state(AraBinding&);
    void restore_state(const AraBinding&);
    void relocate_source(const std::string&,std::shared_ptr<const audio::AudioData>);
private:
    struct Impl;
    explicit Session(std::unique_ptr<Impl>);
    std::shared_ptr<Impl> impl_;
    struct Live;
    std::unique_ptr<Live> live_;
};
// P2 runtime source bindings; persistence/editor UX belong to P4/P3. All clips
// referencing a bound source share its modification (including split/copy/Undo).
class Arrangement final {
public:
    using Resolver = std::function<std::shared_ptr<const audio::AudioData>(const std::string&)>;
    Arrangement();
    ~Arrangement();
    void attach(std::string source, std::string module, std::string class_id);
    void detach(const std::string& source);
    void synchronize(const Project&, const Resolver&);
    void invalidate();
    void apply(const Project&, audio::RenderGraph&,bool editor_preview=false) const;
    std::string status(const std::string& source) const;
    std::vector<ClipCache> caches() const;
    void bypass(const std::string& source,bool);
    bool bypassed(const std::string& source) const;
    ModelStats model_stats(const std::string& source) const;
    bool poll(); // notify vendor updates; true when cached audio needs refresh
    std::optional<std::uint64_t> pending_revision() const;
    bool open_editor(const std::string&,const Id&,void*,int&,int&);
    bool resize_editor(const std::string&,int&,int&);
    void close_editor(const std::string&);
    void close_editors();
    void select_clip(const std::string&,const Id&);
    void set_preparation(std::shared_ptr<std::atomic<bool>>,std::function<void()>);
    void prefer_live(bool);
    void transport(const Project&,const audio::RealtimeState&);
    std::vector<TransportRequest> take_transport_requests();
    void restore_bindings(const std::vector<AraBinding>&);
    void retry(const std::string&);
    std::vector<AraBinding> save_bindings();
    AraBinding binding(const std::string&) const;
    void relocate_source(const std::string&,std::shared_ptr<const audio::AudioData>);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
