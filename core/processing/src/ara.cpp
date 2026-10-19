#include <mrs/ara.hpp>
#include <mrs/audio.hpp>
#include <mrs/no_denormals.hpp>
#include <ARA_API/ARAVST3.h>
#include <ARA_Library/Dispatch/ARAHostDispatch.h>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/gui/iplugview.h"
#include <windows.h>
#include <map>
#include <mutex>
#include <stdexcept>
#include <future>
#include <cmath>
#include <algorithm>
#include <optional>
#include <condition_variable>
#include <thread>
#include <cstring>
#include <bit>

namespace ARA {
// ARAVST3.h declares these IDs but the host must provide their definitions.
DEF_CLASS_IID(IMainFactory)
DEF_CLASS_IID(IPlugInEntryPoint)
DEF_CLASS_IID(IPlugInEntryPoint2)
}
namespace mrs::ara {
namespace {
using namespace Steinberg;
using Module = VST3::Hosting::Module;
thread_local bool rendering_host_audio{};
std::uint64_t pcm_checksum(const audio::AudioData& pcm){
    std::uint64_t hash=14695981039346656037ull;const auto* bytes=reinterpret_cast<const unsigned char*>(pcm.samples.data());
    for(std::size_t i=0;i<pcm.samples.size()*sizeof(float);++i){hash^=bytes[i];hash*=1099511628211ull;}
    hash^=pcm.channels;hash*=1099511628211ull;hash^=pcm.sample_rate;return hash;
}
constexpr auto generation = ARA::kARAAPIGeneration_2_0_Final;
constexpr ARA::ARAPlugInInstanceRoleFlags roles = ARA::kARAPlaybackRendererRole |
    ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole;
ARA::ARAAssertFunction assert_function{}; // stable address required by ARA
void require(bool ok, const char* error) { if (!ok) throw std::runtime_error(error); }
void control_thread() {
    // The first call must be on the application's UI/control thread. Reject
    // later cross-thread calls before loading code or invoking vendor APIs.
    static const DWORD owner = GetCurrentThreadId();
    require(GetCurrentThreadId() == owner, "ARA hosting requires its UI/control thread");
}
void cancelled(const std::shared_ptr<std::atomic<bool>>& flag) {
    if (flag && flag->load()) throw std::runtime_error("ARA operation cancelled");
}
Module::Ptr load(const std::string& path) {
    std::string error;
    auto module = Module::create(path, error);
    if (!module) throw std::runtime_error("ARA module load failed: " + error);
    return module;
}
bool valid(const ARA::ARAFactory* f) {
    return f && f->structSize >= ARA::kARAFactoryMinSize &&
        f->lowestSupportedApiGeneration <= generation && f->highestSupportedApiGeneration >= generation &&
        f->factoryID && *f->factoryID && f->documentArchiveID && *f->documentArchiveID &&
        f->initializeARAWithConfiguration && f->uninitializeARA && f->createDocumentControllerWithDocument;
}
struct Match { IPtr<ARA::IMainFactory> main; const ARA::ARAFactory* factory{}; };
class Provider final : public Steinberg::Vst::PlugProvider {
public:
    using PlugProvider::PlugProvider;
    bool initialize_with_context(Steinberg::FUnknown* context) { return setupPlugin(context); }
};
Match match(const Module::Ptr& module, const VST3::Hosting::ClassInfo& effect) {
    const auto classes = module->getFactory().classInfos();
    std::size_t effects{}, factories{};
    for (const auto& c : classes) {
        if (c.category() == kVstAudioEffectClass) ++effects;
        if (c.category() == kARAMainFactoryClass) ++factories;
    }
    Match result;
    for (const auto& c : classes) {
        if (c.category() != kARAMainFactoryClass ||
            (c.name() != effect.name() && !(effects == 1 && factories == 1))) continue;
        require(!result.main, "Ambiguous ARA factory class mapping");
        result.main = module->getFactory().createInstance<ARA::IMainFactory>(c.ID());
        require(result.main != nullptr, "ARA main factory creation failed");
        result.factory = result.main->getFactory();
    }
    return result;
}
Capability describe(const VST3::Hosting::ClassInfo& effect, const Match& m) {
    Capability info;
    info.class_id = effect.ID().toString(); info.name = effect.name();
    if (!m.main) { info.diagnostic = "No matching ARA main factory"; return info; }
    if (!valid(m.factory)) {
        info.status = CapabilityStatus::incompatible;
        info.diagnostic = "Invalid factory or unsupported ARA 2 API generation"; return info;
    }
    const auto* f = m.factory;
    info.status = CapabilityStatus::available;
    info.factory_id = f->factoryID; info.archive_id = f->documentArchiveID;
    info.version = f->version ? f->version : "";
    info.api_generation = generation;
    return info;
}
struct Factory {
    Module::Ptr module;
    Match match;
    DWORD owner{GetCurrentThreadId()};
    bool initialized{};
    Factory(Module::Ptr m, Match found) : module(std::move(m)), match(std::move(found)) {
        ARA::ARAInterfaceConfiguration config{};
        config.structSize = sizeof(config); config.desiredApiGeneration = generation;
        config.assertFunctionAddress = &assert_function;
        match.factory->initializeARAWithConfiguration(&config); initialized = true;
    }
    ~Factory() {
        if (GetCurrentThreadId() != owner) std::terminate();
        if (initialized) match.factory->uninitializeARA();
    }
};
std::shared_ptr<Factory> acquire(Module::Ptr module, Match found) {
    // Control thread only; never referenced by process(). Share initialization
    // across concurrent documents using the very same factory in this binary.
    static std::mutex mutex;
    static std::map<const ARA::ARAFactory*, std::weak_ptr<Factory>> active;
    std::lock_guard lock(mutex);
    for (auto it = active.begin(); it != active.end();) {
        if (it->second.expired()) it = active.erase(it); else ++it;
    }
    auto& weak = active[found.factory];
    if (auto factory = weak.lock()) {
        require(factory->owner == GetCurrentThreadId(), "ARA factory belongs to another control thread");
        return factory;
    }
    auto factory = std::make_shared<Factory>(std::move(module), std::move(found));
    weak = factory; return factory;
}
struct DirtyRange {
    bool changed{},full{};double first{},last{};
    void add(const ARA::ARAContentTimeRange* range) noexcept {
        if(!range||!std::isfinite(range->start)||!std::isfinite(range->duration)||range->duration<0||!std::isfinite(range->start+range->duration)){changed=full=true;return;}
        if(!changed){first=range->start;last=range->start+range->duration;}
        else{first=std::min(first,range->start);last=std::max(last,range->start+range->duration);}changed=true;
    }
};
struct Reader {std::shared_ptr<const audio::AudioData> pcm;std::atomic<bool> used{};bool doubles{};};
struct SourceNode {
    DirtyRange dirty;
    std::uint64_t checksum{};
    SourceSnapshot snapshot;
    std::shared_ptr<const audio::AudioData> pcm;
    std::array<Reader,64> reader_pool;
    ARA::ARAAudioSourceRef source{};
    ARA::ARAAudioModificationRef modification{};
    bool enabled{};
    bool dormant{};
    bool analysis_requested{};
};
struct RegionNode { Clip clip; ARA::ARAPlaybackRegionRef region{};DirtyRange dirty; };
struct SequenceNode { ARA::ARARegionSequenceRef ref{}; bool assigned{}; };
std::uint64_t state_checksum(const AraBinding&);
class Host final : public ARA::Host::AudioAccessControllerInterface,
                   public ARA::Host::ArchivingControllerInterface,
                   public ARA::Host::ModelUpdateControllerInterface,
                   public ARA::Host::PlaybackControllerInterface {
public:
    std::mutex request_mutex;
    std::array<TransportRequest,32> requests{};
    std::size_t request_count{};
    std::atomic<bool> requests_enabled{};
    void request(TransportRequest r) noexcept {if(rendering_host_audio)return;try{std::lock_guard lock(request_mutex);if(requests_enabled.load()&&request_count<requests.size())requests[request_count++]=r;}catch(...){}}
    void requestStartPlayback() noexcept override {request({TransportAction::play});}
    void requestStopPlayback() noexcept override {request({TransportAction::stop});}
    void requestSetPlaybackPosition(double p) noexcept override {request({TransportAction::seek,p});}
    void requestSetCycleRange(double p,double d) noexcept override {request({TransportAction::cycle_range,p,d});}
    void requestEnableCycle(bool on) noexcept override {request({TransportAction::cycle_enabled,0,0,on});}
    std::atomic<std::size_t> readers{};
    std::atomic<std::uint64_t> reads{}, errors{};
    std::atomic<std::uint64_t> content_revision{};
    std::atomic<std::uint64_t> state_revision{};
    std::atomic<std::uint64_t> global_audio_revision{};
    // Monotonic counters retain notifications arriving during a failed/cancelled
    // preparation. Only a successful cache publication advances the baseline.
    std::array<std::atomic<std::uint64_t>,5> invalidations{};
    const DWORD model_thread{GetCurrentThreadId()};
    void invalidate_audio(unsigned cause=3) noexcept {++invalidations[cause];++global_audio_revision;++content_revision;}
    void changed(DirtyRange* target,const ARA::ARAContentTimeRange* range,ARA::ContentUpdateScopes scopes) noexcept {
        ++state_revision;if(!scopes.affectSamples())return;
        if(target&&GetCurrentThreadId()==model_thread){target->add(range);++content_revision;}
        else invalidate_audio(4);
    }
    std::atomic<bool> analysing{};
    void notifyAudioSourceAnalysisProgress(ARA::ARAAudioSourceHostRef,ARA::ARAAnalysisProgressState state,float) noexcept override {
        analysing.store(state != ARA::kARAAnalysisProgressCompleted); invalidate_audio(1);
    }
    void notifyAudioSourceContentChanged(ARA::ARAAudioSourceHostRef ref,const ARA::ARAContentTimeRange* range,ARA::ContentUpdateScopes scopes) noexcept override {auto* node=reinterpret_cast<SourceNode*>(ref);changed(node?&node->dirty:nullptr,range,scopes);}
    void notifyAudioModificationContentChanged(ARA::ARAAudioModificationHostRef ref,const ARA::ARAContentTimeRange* range,ARA::ContentUpdateScopes scopes) noexcept override {auto* node=reinterpret_cast<SourceNode*>(ref);changed(node?&node->dirty:nullptr,range,scopes);}
    void notifyPlaybackRegionContentChanged(ARA::ARAPlaybackRegionHostRef ref,const ARA::ARAContentTimeRange* range,ARA::ContentUpdateScopes scopes) noexcept override {auto* node=reinterpret_cast<RegionNode*>(ref);changed(node?&node->dirty:nullptr,range,scopes);}
    void notifyDocumentDataChanged() noexcept override { ++state_revision; }
    ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef ref, bool doubles) noexcept override {
        auto* source = reinterpret_cast<SourceNode*>(ref);
        if (!source || !source->pcm) { ++errors; return nullptr; }
        for(auto& reader:source->reader_pool){bool expected=false;if(reader.used.compare_exchange_strong(expected,true)){reader.doubles=doubles;++readers;return reinterpret_cast<ARA::ARAAudioReaderHostRef>(&reader);}}
        ++errors;return nullptr;
    }
    bool readAudioSamples(ARA::ARAAudioReaderHostRef ref, ARA::ARASamplePosition position,
                          ARA::ARASampleCount count, void* const buffers[]) noexcept override {
        auto* reader = reinterpret_cast<Reader*>(ref);
        if (!reader || !buffers || count < 0 || count > 64*1024*1024 || position > max_sample-count || position < -max_sample) {
            ++errors; return false;
        }
        const auto& pcm = *reader->pcm;
        for (std::uint32_t c = 0; c < pcm.channels; ++c) if (!buffers[c]) { ++errors; return false; }
        for (Sample f = 0; f < count; ++f) {
            const auto at = position+f;
            for (std::uint32_t c = 0; c < pcm.channels; ++c) {
                const float v = at < 0 || at >= pcm.frames() ? 0.f : pcm.samples[static_cast<std::size_t>(at)*pcm.channels+c];
                if (reader->doubles) static_cast<double*>(buffers[c])[f] = v;
                else static_cast<float*>(buffers[c])[f] = v;
            }
        }
        ++reads; return true;
    }
    void destroyAudioReader(ARA::ARAAudioReaderHostRef ref) noexcept override {
        if (ref) {reinterpret_cast<Reader*>(ref)->used=false;--readers;}
    }
    struct Archive {std::vector<std::byte> bytes;std::string id;bool failed{};};
    Archive* archive{};
    ARA::ARASize getArchiveSize(ARA::ARAArchiveReaderHostRef ref) noexcept override {return archive&&reinterpret_cast<void*>(ref)==archive?archive->bytes.size():0;}
    bool readBytesFromArchive(ARA::ARAArchiveReaderHostRef ref, ARA::ARASize at, ARA::ARASize size, ARA::ARAByte out[]) noexcept override {
        if(!archive||reinterpret_cast<void*>(ref)!=archive)return false;
        if(at>archive->bytes.size()||size>archive->bytes.size()-at||(!out&&size)){archive->failed=true;return false;}
        if(size)std::memcpy(out,archive->bytes.data()+at,size);return true;
    }
    bool writeBytesToArchive(ARA::ARAArchiveWriterHostRef ref, ARA::ARASize at, ARA::ARASize size, const ARA::ARAByte in[]) noexcept override {
        if(!archive||reinterpret_cast<void*>(ref)!=archive)return false;
        if(at>1024*1024||size>1024*1024-at||(!in&&size)){archive->failed=true;return false;}
        try{archive->bytes.resize(std::max<std::size_t>(archive->bytes.size(),at+size));if(size)std::memcpy(archive->bytes.data()+at,in,size);return true;}catch(...){archive->failed=true;return false;}
    }
    void notifyDocumentArchivingProgress(float) noexcept override {}
    void notifyDocumentUnarchivingProgress(float) noexcept override {}
    ARA::ARAPersistentID getDocumentArchiveID(ARA::ARAArchiveReaderHostRef ref) noexcept override {return archive&&reinterpret_cast<void*>(ref)==archive?archive->id.c_str():nullptr;}
};
}
struct Session::Impl : Steinberg::IPlugFrame, Steinberg::Vst::IComponentHandler {
    std::string persistent_id;
    std::vector<AraBinding> restore;
    bool restore_document{};
    std::map<std::string,std::string> source_ids;
    DWORD owner{GetCurrentThreadId()};
    Capability info;
    std::shared_ptr<Factory> factory;
    Host host;
    ARA::Host::DocumentControllerHostInstance host_instance{&host, &host, nullptr, &host, &host};
    const ARA::ARADocumentControllerInstance* document{};
    std::unique_ptr<Provider> provider;
    const ARA::ARAPlugInExtensionInstance* extension{};
    bool editing{};
    bool model_failed{};
    std::map<std::string, std::unique_ptr<SourceNode>> sources;
    std::map<std::string, std::unique_ptr<RegionNode>> regions;
    std::map<std::string,ClipCache> caches;
    std::vector<Clip> model_clips;
    std::vector<SourceSnapshot> model_sources;
    std::vector<std::pair<Id,std::string>> model_tracks;
    TimeMap model_time;
    std::uint32_t model_rate{};
    std::uint64_t cached_global{};
    std::array<std::uint64_t,5> cached_invalidations{};
    bool model_valid{},cache_valid{};
    PreparationStats stats;
    ARA::ARAMusicalContextRef context{};
    std::map<std::string,std::unique_ptr<SequenceNode>> sequences;
    std::optional<Id> selected_clip;
    Steinberg::IPtr<Steinberg::IPlugView> view;
    bool view_attached{};
    HWND parent{};
    std::thread transport_worker;
    std::mutex transport_mutex;
    std::condition_variable transport_condition;
    Steinberg::Vst::ProcessContext transport_context{};
    bool transport_pending{},transport_stopping{},transport_active{};
    std::atomic<bool> transport_error{};
    std::atomic<std::uint64_t> transport_blocks{};
    bool warming_editor{};
    std::weak_ptr<audio::EditorRenderer> editor_audio;
    void stop_transport() {
        if(auto editor=editor_audio.lock())editor->suspend();
        if(transport_worker.joinable()){{std::lock_guard lock(transport_mutex);transport_stopping=true;}transport_condition.notify_one();transport_worker.join();}
        if(transport_active){provider->getComponentPtr()->setActive(false);transport_active=false;}
        transport_pending=false;
    }
    std::shared_ptr<std::atomic<bool>> preparation_cancel;
    std::function<void()> preparation_wait;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID requested,void** out) override {
        if (Steinberg::FUnknownPrivate::iidEqual(requested,Steinberg::IPlugFrame::iid)) *out=static_cast<Steinberg::IPlugFrame*>(this);
        else if (Steinberg::FUnknownPrivate::iidEqual(requested,Steinberg::Vst::IComponentHandler::iid) || Steinberg::FUnknownPrivate::iidEqual(requested,Steinberg::FUnknown::iid)) *out=static_cast<Steinberg::Vst::IComponentHandler*>(this);
        else { *out=nullptr; return Steinberg::kNoInterface; } return Steinberg::kResultOk;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* candidate,Steinberg::ViewRect* rect) override {
        if (!parent || candidate!=view.get() || !rect || rect->getWidth()<1 || rect->getHeight()<1 || rect->getWidth()>8192 || rect->getHeight()>8192) return Steinberg::kInvalidArgument;
        SetWindowPos(parent,nullptr,0,0,rect->getWidth(),rect->getHeight(),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        return view->onSize(rect);
    }
    Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID) override { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID,Steinberg::Vst::ParamValue) override { host.invalidate_audio(0); return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID) override { ++host.state_revision; return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override { ++host.state_revision;if(flags&(Steinberg::Vst::kReloadComponent|Steinberg::Vst::kIoChanged|Steinberg::Vst::kParamValuesChanged|Steinberg::Vst::kLatencyChanged))host.invalidate_audio(2);return Steinberg::kResultOk; }
    void close_view() {
        host.requests_enabled=false;
        stop_transport();
        if (view) { if(view_attached)view->removed(); view->setFrame(nullptr); view=nullptr; } view_attached=false;parent=nullptr;
        {std::lock_guard lock(host.request_mutex);host.request_count=0;}
    }
    void check_thread() const { require(GetCurrentThreadId() == owner, "ARA session belongs to another control thread"); }
    void request_analysis() {
        check_thread();require(!editing,"ARA analysis requested during editing");
        const auto* api=factory->match.factory;
        require(api->analyzeableContentTypesCount<=128,"ARA analysis capability budget exceeded");
        if(!api->analyzeableContentTypesCount)return;
        require(api->analyzeableContentTypes!=nullptr,"Invalid ARA analysis capabilities");
        ARA::Host::DocumentController dc(document);
        require(dc.isLicensedForCapabilities(false,api->analyzeableContentTypesCount,api->analyzeableContentTypes,ARA::kARAPlaybackTransformationNoChanges),
            "ARA analysis is not licensed; activate the plugin and reopen its editor");
        for(auto& [id,s]:sources){(void)id;if(!s->enabled||s->dormant||s->analysis_requested)continue;
            std::vector<ARA::ARAContentType> needed;
            for(ARA::ARASize i=0;i<api->analyzeableContentTypesCount;++i)
                if(!dc.isAudioSourceContentAvailable(s->source,api->analyzeableContentTypes[i]))needed.push_back(api->analyzeableContentTypes[i]);
            if(!needed.empty())dc.requestAudioSourceContentAnalysis(s->source,needed.size(),needed.data());
            s->analysis_requested=true;
        }
    }
    ~Impl() {
        if (GetCurrentThreadId() != owner) std::terminate();
        close_view();
        if (provider && provider->getControllerPtr()) provider->getControllerPtr()->setComponentHandler(nullptr);
        // Release binding/processor/controller before document, then factory,
        // then module. ARA model and host callbacks remain alive throughout.
        // No processing task outlives synchronize(). Destroy dependent model
        // objects explicitly while host refs/readers and the provider are alive.
        if (document && (context || !sources.empty())) {
            ARA::Host::DocumentController dc(document);
            dc.beginEditing();
            for(auto& [id,s]:sequences){(void)id;if(s->assigned)ARA::Host::EditorRenderer(extension).removeRegionSequence(s->ref);}
            for (auto& [id, r] : regions) { (void)id; if (r->region) dc.destroyPlaybackRegion(r->region); }
            regions.clear();
            for (auto& [id, s] : sources) {
                (void)id;
                if (s->enabled) dc.enableAudioSourceSamplesAccess(s->source, false);
                if (s->modification) dc.destroyAudioModification(s->modification);
                if (s->source) dc.destroyAudioSource(s->source);
            }
            sources.clear();
            for(auto& [id,s]:sequences){(void)id;dc.destroyRegionSequence(s->ref);}sequences.clear();
            if (context) dc.destroyMusicalContext(context);
            dc.endEditing();
        }
        provider.reset(); extension = nullptr;
        if (document && document->structSize >= ARA::kARADocumentControllerInstanceMinSize &&
            document->documentControllerRef && document->documentControllerInterface &&
            document->documentControllerInterface->destroyDocumentController)
            document->documentControllerInterface->destroyDocumentController(document->documentControllerRef);
    }
};
std::vector<Capability> probe(const std::string& path) {
    control_thread();
    auto module = load(path);
    std::vector<Capability> result;
    for (const auto& c : module->getFactory().classInfos())
        if (c.category() == kVstAudioEffectClass) result.push_back(describe(c, match(module, c)));
    return result;
}
namespace {bool process_offline(Steinberg::Vst::IAudioProcessor*,Steinberg::Vst::ProcessData*) noexcept;}
struct Session::Live {
    struct Editor final:audio::EditorRenderer {
        std::shared_ptr<Impl> document;
        Steinberg::FUnknownPtr<Steinberg::Vst::IAudioProcessor> processor;
        Steinberg::Vst::HostProcessData data;
        Steinberg::Vst::ProcessContext context{};
        Steinberg::Vst::ParameterChanges parameters;
        std::vector<audio::RenderGraph::TempoSegment> tempos;
        std::atomic<bool> enabled{},audible{true};
        std::atomic<unsigned> users{};
        bool active{},processing{},first{};
        std::uint32_t rate{},channels{};
        explicit Editor(std::shared_ptr<Impl> doc):document(std::move(doc)){}
        ~Editor() override {suspend();}
        void suspend() override {
            document->check_thread();enabled=false;
            while(users.load())std::this_thread::yield(); // control only; RT never waits
            if(processing){processor->setProcessing(false);processing=false;}
            if(active){document->provider->getComponentPtr()->setActive(false);active=false;}
        }
        void prepare(const Project& project) {
            using namespace Steinberg::Vst;
            if(enabled.load())return;
            document->stop_transport();
            auto component=document->provider->getComponentPtr();processor=Steinberg::FUnknownPtr<IAudioProcessor>(component);
            require(processor!=nullptr,"ARA editor has no audio processor");
            ProcessSetup setup{kRealtime,kSample32,8192,static_cast<double>(project.sample_rate)};
            require(processor->setupProcessing(setup)==kResultOk&&data.prepare(*component,8192,kSample32),"ARA audition setup failed");
            require(data.numOutputs>0&&(data.outputs[0].numChannels==1||data.outputs[0].numChannels==2),"ARA audition requires mono/stereo output");
            channels=data.outputs[0].numChannels;rate=project.sample_rate;
            tempos.clear();const Timeline timeline(project.time,rate);for(const auto& t:project.time.tempos)tempos.push_back({timeline.to_samples(t.tick),t.bpm,static_cast<double>(t.tick)/ppq});
            data.processMode=kRealtime;data.processContext=&context;context.sampleRate=rate;
            parameters.clearQueue();
            if(auto controller=document->provider->getControllerPtr()){
                require(controller->getParameterCount()<=4096,"ARA audition parameter budget exceeded");
                for(int32 n=0;n<controller->getParameterCount();++n){ParameterInfo info{};if(controller->getParameterInfo(n,info)!=kResultOk)continue;int32 index{};auto* queue=parameters.addParameterData(info.id,index);if(queue)queue->addPoint(0,controller->getParamNormalized(info.id),index);}
            }
            first=true;
            require(component->setActive(true)==kResultOk,"ARA audition activation failed");active=true;
            const auto result=processor->setProcessing(true);require(result==kResultOk||result==kNotImplemented,"ARA audition processing failed");processing=true;enabled=true;
        }
        double music(Sample at)const noexcept {const auto* found=&tempos.front();for(const auto& t:tempos){if(t.sample>at)break;found=&t;}return found->quarter+static_cast<double>(at-found->sample)*found->bpm/(60.*rate);}
        bool mix(float* output,std::uint32_t frames,std::uint32_t outputs,Sample at,bool playing,double tempo,double quarter,processing::MeterContext meter,std::optional<LoopRange> loop,bool allow)noexcept override {
            using namespace Steinberg::Vst;
            if(!enabled.load())return true;
            unsigned idle=0;if(!users.compare_exchange_strong(idle,1))return false;
            struct Exit{std::atomic<unsigned>& users;~Exit(){--users;}} exit{users};
            if(!enabled.load())return true;
            if(!frames||frames>8192)return false;
            context.projectTimeSamples=at;context.projectTimeMusic=quarter;context.tempo=tempo;
            context.timeSigNumerator=meter.numerator;context.timeSigDenominator=meter.denominator;context.barPositionMusic=meter.bar_quarter;
            context.systemTime=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            context.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kTimeSigValid|ProcessContext::kBarPositionValid|ProcessContext::kSystemTimeValid;
            if(playing)context.state|=ProcessContext::kPlaying;
            if(loop){context.state|=ProcessContext::kCycleValid|ProcessContext::kCycleActive;context.cycleStartMusic=music(loop->start);context.cycleEndMusic=music(loop->end);}
            data.numSamples=static_cast<int32>(frames);
            data.inputParameterChanges=first?&parameters:nullptr;first=false;
            for(int32 b=0;b<data.numInputs;++b){data.inputs[b].silenceFlags=~Steinberg::uint64{};for(int32 c=0;c<data.inputs[b].numChannels;++c)std::fill_n(data.inputs[b].channelBuffers32[c],frames,0.f);}
            for(int32 b=0;b<data.numOutputs;++b){data.outputs[b].silenceFlags=0;for(int32 c=0;c<data.outputs[b].numChannels;++c)std::fill_n(data.outputs[b].channelBuffers32[c],frames,0.f);}
            if(!process_offline(processor,&data)){enabled=false;document->transport_error=true;return false;}
            ++document->transport_blocks;
            for(std::uint32_t c=0;c<channels;++c)if(!(data.outputs[0].silenceFlags&(Steinberg::uint64{1}<<c)))for(std::uint32_t f=0;f<frames;++f)if(!std::isfinite(data.outputs[0].channelBuffers32[c][f])){enabled=false;document->transport_error=true;return false;}
            if(allow&&audible.load())for(std::uint32_t f=0;f<frames;++f)for(std::uint32_t c=0;c<std::min(2U,outputs);++c){
                const auto source=channels==1?0:c;
                const auto sample=[&](std::uint32_t channel){return (data.outputs[0].silenceFlags&(Steinberg::uint64{1}<<channel))?0.f:data.outputs[0].channelBuffers32[channel][f];};
                const auto value=outputs==1&&channels==2?(sample(0)+sample(1))*.5f:sample(source);
                output[static_cast<std::size_t>(f)*outputs+c]+=value;
            }
            return true;
        }
    };
    std::shared_ptr<Editor> editor;
    struct Renderer final:audio::VoiceRenderer,Steinberg::Vst::IComponentHandler {
        // The document/host/source readers outlive every graph reference. Impl
        // does not own renderers, so this is not a reference-count cycle.
        std::shared_ptr<Impl> document;
        std::unique_ptr<Provider> provider;
        Steinberg::FUnknownPtr<Steinberg::Vst::IAudioProcessor> processor;
        const ARA::ARAPlugInExtensionInstance* extension{};
        ARA::ARAPlaybackRegionRef region{};
        Steinberg::Vst::HostProcessData data;
        Steinberg::Vst::ProcessContext context{};
        Steinberg::Vst::ParameterChanges parameters;
        std::vector<audio::RenderGraph::TempoSegment> tempos;
        std::atomic<bool> enabled{},fault{};
        std::atomic<unsigned> users{};
        std::uint32_t channel_count{},rate{};
        bool assigned{},active{},processing{},first{true};
        Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID requested,void** out) override {if(!out)return Steinberg::kInvalidArgument;*out=nullptr;if(!Steinberg::FUnknownPrivate::iidEqual(requested,Steinberg::Vst::IComponentHandler::iid)&&!Steinberg::FUnknownPrivate::iidEqual(requested,Steinberg::FUnknown::iid))return Steinberg::kNoInterface;*out=static_cast<Steinberg::Vst::IComponentHandler*>(this);return Steinberg::kResultOk;}
        Steinberg::uint32 PLUGIN_API addRef() override{return 1;}
        Steinberg::uint32 PLUGIN_API release() override{return 1;}
        Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID) override{return Steinberg::kResultOk;}
        Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID,Steinberg::Vst::ParamValue) override{if(enabled.load())document->host.invalidate_audio(0);return Steinberg::kResultOk;}
        Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID) override{return Steinberg::kResultOk;}
        Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override{if(enabled.load()&&(flags&(Steinberg::Vst::kLatencyChanged|Steinberg::Vst::kIoChanged|Steinberg::Vst::kReloadComponent))){fault=true;document->host.invalidate_audio(2);}return Steinberg::kResultOk;}
        Renderer(std::shared_ptr<Impl> doc,const RegionNode& node,const Project& project):document(std::move(doc)),region(node.region),channel_count(document->sources.at(node.clip.source)->pcm->channels),rate(project.sample_rate){
            using namespace Steinberg::Vst;
            try {
                auto& factory=document->factory->module->getFactory();
                static HostApplication application;
                for(const auto& info:factory.classInfos())if(info.ID().toString()==document->info.class_id){provider=std::make_unique<Provider>(factory,info,true);break;}
                require(provider&&provider->initialize_with_context(&application),"ARA live instance initialization failed");
                if(auto controller=provider->getControllerPtr())controller->setComponentHandler(this);
                auto component=provider->getComponentPtr();processor=Steinberg::FUnknownPtr<IAudioProcessor>(component);
                Steinberg::FUnknownPtr<ARA::IPlugInEntryPoint2> entry(component);
                require(entry&&processor,"ARA live entry point unavailable");
                extension=entry->bindToDocumentControllerWithRoles(document->document->documentControllerRef,roles,ARA::kARAPlaybackRendererRole);
                require(extension&&extension->playbackRendererRef&&extension->playbackRendererInterface,"ARA live playback role unavailable");
                const auto layout=channel_count==1?SpeakerArr::kMono:SpeakerArr::kStereo;
                std::vector<SpeakerArrangement> inputs(component->getBusCount(kAudio,kInput)),outputs(component->getBusCount(kAudio,kOutput));
                require(!outputs.empty(),"ARA live output missing");
                for(const auto direction:{kInput,kOutput}){auto& buses=direction==kInput?inputs:outputs;for(int32 b=0;b<static_cast<int32>(buses.size());++b){require(processor->getBusArrangement(direction,b,buses[b])==kResultOk,"ARA live bus query failed");if(!b)buses[b]=layout;}}
                require(processor->setBusArrangements(inputs.data(),static_cast<int32>(inputs.size()),outputs.data(),static_cast<int32>(outputs.size()))==kResultOk,"ARA live channel layout unsupported");
                for(const auto direction:{kInput,kOutput}){for(int32 b=0;b<component->getBusCount(kAudio,direction);++b)require(component->activateBus(kAudio,direction,b,b==0)==kResultOk,"ARA live bus activation failed");for(int32 b=0;b<component->getBusCount(kEvent,direction);++b)component->activateBus(kEvent,direction,b,false);}
                ProcessSetup setup{kRealtime,kSample32,8192,static_cast<double>(rate)};
                require(processor->setupProcessing(setup)==kResultOk&&data.prepare(*component,8192,kSample32),"ARA realtime setup unsupported");
                require(data.outputs[0].numChannels==static_cast<int32>(channel_count),"ARA live output mismatch");
                data.processMode=kRealtime;data.processContext=&context;context.sampleRate=rate;
                if(auto controller=document->provider->getControllerPtr())for(int32 n=0;n<controller->getParameterCount();++n){ParameterInfo info{};if(controller->getParameterInfo(n,info)!=kResultOk)continue;int32 index{};auto* queue=parameters.addParameterData(info.id,index);if(queue)queue->addPoint(0,controller->getParamNormalized(info.id),index);}
                const Timeline timeline(project.time,rate);for(const auto& tempo:project.time.tempos)tempos.push_back({timeline.to_samples(tempo.tick),tempo.bpm,static_cast<double>(tempo.tick)/ppq});
                ARA::Host::PlaybackRenderer(extension).addPlaybackRegion(region);assigned=true;
                require(component->setActive(true)==kResultOk,"ARA live activation failed");active=true;
                require(processor->getLatencySamples()==0,"ARA live path requires zero latency; use compensated PCM");
                const auto result=processor->setProcessing(true);require(result==kResultOk||result==kNotImplemented,"ARA live processing start failed");processing=true;enabled=true;
            }catch(...){retire();if(provider&&provider->getControllerPtr())provider->getControllerPtr()->setComponentHandler(nullptr);throw;}
        }
        ~Renderer() override {retire();if(provider&&provider->getControllerPtr())provider->getControllerPtr()->setComponentHandler(nullptr);}
        void retire(){
            if(GetCurrentThreadId()!=document->owner)std::terminate();
            enabled=false;
            // UI/control only. The callback never waits: it enters, rechecks the
            // gate, and exits before any provider/model object can be retired.
            while(users.load())std::this_thread::yield();
            if(processing){processor->setProcessing(false);processing=false;}
            if(active){provider->getComponentPtr()->setActive(false);active=false;}
            if(assigned){ARA::Host::PlaybackRenderer(extension).removePlaybackRegion(region);assigned=false;}
        }
        double music(Sample at) const noexcept {const audio::RenderGraph::TempoSegment* found=&tempos.front();for(const auto& t:tempos){if(t.sample>at)break;found=&t;}return found->quarter+static_cast<double>(at-found->sample)*found->bpm/(60.*rate);}
        bool render(Sample at,std::uint32_t frames,double tempo,double quarter,processing::MeterContext meter,std::optional<LoopRange> loop) noexcept override {
            using namespace Steinberg::Vst;
            if(!enabled.load()||fault.load())return false;
            unsigned idle=0;if(!users.compare_exchange_strong(idle,1)){fault=true;return false;}
            struct Exit{std::atomic<unsigned>& users;~Exit(){--users;}} exit{users};
            if(!enabled.load())return false;
            if(!frames||frames>8192){fault=true;return false;}
            context.projectTimeSamples=at;context.projectTimeMusic=quarter;context.tempo=tempo;
            context.timeSigNumerator=meter.numerator;context.timeSigDenominator=meter.denominator;context.barPositionMusic=meter.bar_quarter;
            context.systemTime=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            context.state=ProcessContext::kPlaying|ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kTimeSigValid|ProcessContext::kBarPositionValid|ProcessContext::kSystemTimeValid;
            if(loop){context.state|=ProcessContext::kCycleValid|ProcessContext::kCycleActive;context.cycleStartMusic=music(loop->start);context.cycleEndMusic=music(loop->end);}
            data.numSamples=static_cast<int32>(frames);data.inputParameterChanges=first?&parameters:nullptr;first=false;
            for(int32 b=0;b<data.numInputs;++b){data.inputs[b].silenceFlags=~Steinberg::uint64{};for(int32 c=0;c<data.inputs[b].numChannels;++c)std::fill_n(data.inputs[b].channelBuffers32[c],frames,0.f);}
            for(int32 b=0;b<data.numOutputs;++b){data.outputs[b].silenceFlags=0;for(int32 c=0;c<data.outputs[b].numChannels;++c)std::fill_n(data.outputs[b].channelBuffers32[c],frames,0.f);}
            if(!process_offline(processor,&data)){fault=true;return false;}
            for(std::uint32_t c=0;c<channel_count;++c)for(std::uint32_t f=0;f<frames;++f)if(!std::isfinite(data.outputs[0].channelBuffers32[c][f])){fault=true;return false;}
            return true;
        }
        float sample(std::uint32_t frame,std::uint32_t channel) const noexcept override {return fault.load()||!enabled.load()||frame>=static_cast<std::uint32_t>(data.numSamples)||channel>=channel_count||(data.outputs[0].silenceFlags&(Steinberg::uint64{1}<<channel))?0.f:data.outputs[0].channelBuffers32[channel][frame];}
        std::uint32_t channels() const noexcept override {return channel_count;}
        bool failed() const noexcept override {return fault.load();}
    };
    bool preferred{};
    bool unsupported{};
    std::map<std::string,std::shared_ptr<Renderer>> renderers;
    void retire(){for(auto& [id,r]:renderers){(void)id;r->retire();}renderers.clear();}
};
Session::Session(std::unique_ptr<Impl> impl) : impl_(std::move(impl)),live_(std::make_unique<Live>()) {live_->editor=std::make_shared<Live::Editor>(impl_);impl_->editor_audio=live_->editor;}
std::shared_ptr<audio::EditorRenderer> Session::editor_renderer() const {return live_->editor;}
Session::~Session() {live_->retire();impl_->close_view();}
void Session::prefer_live(bool enabled){impl_->check_thread();if(live_->preferred!=enabled){impl_->cache_valid=false;if(!enabled)live_->retire();}live_->preferred=enabled;}
bool Session::live_current()const{impl_->check_thread();return !live_->renderers.empty()&&live_->renderers.size()==impl_->regions.size()&&impl_->host.global_audio_revision.load()==impl_->cached_global&&!analysing()&&std::all_of(live_->renderers.begin(),live_->renderers.end(),[](const auto& entry){return !entry.second->failed();});}
bool Session::live_failed()const{impl_->check_thread();return std::any_of(live_->renderers.begin(),live_->renderers.end(),[](const auto& entry){return entry.second->failed();});}
std::shared_ptr<audio::VoiceRenderer> Session::live_renderer(const Id& id)const{impl_->check_thread();const auto at=live_->renderers.find(id.value);return at==live_->renderers.end()?nullptr:at->second;}
std::unique_ptr<Session> Session::open(const std::string& path, const std::string& id,
                                     std::shared_ptr<std::atomic<bool>> cancel) {
    control_thread();
    cancelled(cancel);
    auto module = load(path);
    auto impl = std::make_unique<Impl>();
    for (const auto& c : module->getFactory().classInfos()) {
        if (c.category() != kVstAudioEffectClass || c.ID().toString() != id) continue;
        auto found = match(module, c);
        impl->info = describe(c, found);
        require(impl->info.status == CapabilityStatus::available, "Selected VST3 class does not support compatible ARA 2");
        cancelled(cancel);
        impl->factory = acquire(module, std::move(found));
        ARA::ARADocumentProperties properties{};
        properties.structSize = sizeof(properties); properties.name = "MR Studio ARA Foundation";
        impl->document = impl->factory->match.factory->createDocumentControllerWithDocument(&impl->host_instance, &properties);
        require(impl->document && impl->document->structSize >= ARA::kARADocumentControllerInstanceMinSize &&
            impl->document->documentControllerRef && impl->document->documentControllerInterface &&
            impl->document->documentControllerInterface->structSize >= ARA::kARADocumentControllerInterfaceMinSize &&
            impl->document->documentControllerInterface->destroyDocumentController &&
            impl->document->documentControllerInterface->beginEditing &&
            impl->document->documentControllerInterface->endEditing &&
            impl->document->documentControllerInterface->notifyModelUpdates,
            "ARA document creation failed");
        cancelled(cancel);
        auto& factory = module->getFactory();
        static Steinberg::Vst::HostApplication application;
        factory.setHostContext(&application);
        // Use the existing SDK provider lifecycle with an explicit context;
        // do not overwrite PluginContextFactory used by ordinary inserts.
        impl->provider = std::make_unique<Provider>(factory, c, true);
        require(impl->provider->initialize_with_context(&application), "ARA VST3 instance initialization failed");
        if (auto controller=impl->provider->getControllerPtr()) controller->setComponentHandler(impl.get());
        auto component = impl->provider->getComponentPtr();
        Steinberg::FUnknownPtr<ARA::IPlugInEntryPoint> entry(component);
        Steinberg::FUnknownPtr<ARA::IPlugInEntryPoint2> entry2(component);
        const auto* entry_factory = entry ? entry->getFactory() : nullptr;
        require(entry && entry2 && valid(entry_factory) &&
            std::string(entry_factory->factoryID) == impl->info.factory_id,
            "ARA component/factory mismatch or missing ARA 2 entry point");
        impl->extension = entry2->bindToDocumentControllerWithRoles(impl->document->documentControllerRef, roles, roles);
        require(impl->extension && impl->extension->structSize >=
            ARA_IMPLEMENTED_STRUCT_SIZE(ARAPlugInExtensionInstance, editorViewInterface) &&
            impl->extension->playbackRendererRef && impl->extension->playbackRendererInterface &&
            impl->extension->editorRendererRef && impl->extension->editorRendererInterface &&
            impl->extension->editorViewRef && impl->extension->editorViewInterface,
            "ARA role binding failed");
        cancelled(cancel);
        return std::unique_ptr<Session>(new Session(std::move(impl)));
    }
    throw std::runtime_error("Selected VST3 class was not found");
}
const Capability& Session::capability() const { impl_->check_thread(); return impl_->info; }
bool Session::roles_bound() const { impl_->check_thread(); return impl_->extension != nullptr; }
void Session::edit(const std::function<void()>& operation) {
    live_->retire();
    impl_->check_thread();impl_->stop_transport();
    impl_->check_thread(); require(!impl_->editing, "Nested ARA edit session");
    const auto* api = impl_->document->documentControllerInterface;
    const auto ref = impl_->document->documentControllerRef;
    impl_->editing = true; api->beginEditing(ref);
    try { operation(); }
    catch (...) { api->endEditing(ref); impl_->editing = false; throw; }
    api->endEditing(ref); impl_->editing = false;
}
void Session::notify() {
    impl_->check_thread(); require(!impl_->editing, "ARA notification during edit session");
    require(!impl_->model_failed,"ARA model failed; detach/relink explicitly");
    impl_->document->documentControllerInterface->notifyModelUpdates(impl_->document->documentControllerRef);
    const auto* api=impl_->factory->match.factory;
    bool tracked=false,pending=false;
    ARA::Host::DocumentController dc(impl_->document);
    for(const auto& [id,s]:impl_->sources){(void)id;if(!s->enabled||s->dormant||!s->analysis_requested)continue;tracked=true;
        for(ARA::ARASize i=0;i<api->analyzeableContentTypesCount;++i)
            pending|=dc.isAudioSourceContentAnalysisIncomplete(s->source,api->analyzeableContentTypes[i]);
    }
    if(tracked){const bool was=impl_->host.analysing.exchange(pending);if(was&&!pending)impl_->host.invalidate_audio(1);}
}

namespace {
template<class T> T await_preparation(std::future<T> pending,const std::function<void()>& tick) {
    while(pending.wait_for(std::chrono::milliseconds(15))!=std::future_status::ready) if(tick)tick();
    return pending.get();
}
std::shared_ptr<const audio::AudioData> preload(const SourceSnapshot& source, const std::shared_ptr<std::atomic<bool>>& cancel,const std::function<void()>& tick) {
    require(source.pcm != nullptr, "Missing ARA source PCM"); source.pcm->validate();
    const auto samples = static_cast<std::uint64_t>(source.pcm->frames())*source.pcm->channels;
    require(samples <= 64*1024*1024, "ARA source exceeds 256 MiB preparation budget");
    if (!source.pcm->file) return source.pcm;
    // Never borrow ReadAhead's RT cursor. A dedicated immutable reader snapshot
    // is decoded with its own file handle by a preparation worker.
    return await_preparation(std::async(std::launch::async, [source, cancel] {
        auto data = std::make_shared<audio::AudioData>();
        data->sample_rate = source.pcm->sample_rate; data->channels = source.pcm->channels;
        data->samples.resize(static_cast<std::size_t>(source.pcm->frames())*data->channels);
        const std::size_t block = 8192*data->channels;
        for (std::size_t at = 0; at < data->samples.size(); at += block) {
            cancelled(cancel);
            source.pcm->file->read(static_cast<Sample>(at/data->channels),
                std::span<float>(data->samples).subspan(at, std::min(block, data->samples.size()-at)));
        }
        data->validate(); return std::shared_ptr<const audio::AudioData>(data);
    }),tick);
}
bool process_offline(Steinberg::Vst::IAudioProcessor* processor, Steinberg::Vst::ProcessData* data) noexcept {
    const bool previous=rendering_host_audio;rendering_host_audio=data->processMode==Steinberg::Vst::kRealtime;
    bool result{};
#ifdef _MSC_VER
    __try { result=processor->process(*data) == Steinberg::kResultOk; }
    __except(EXCEPTION_EXECUTE_HANDLER) { result=false; }
#else
    result=processor->process(*data) == Steinberg::kResultOk;
#endif
    rendering_host_audio=previous;return result;
}
struct Activation {
    Steinberg::Vst::IComponent* component;
    Steinberg::Vst::IAudioProcessor* processor;
    bool active{}, processing{};
    ~Activation() {
        if (processing) processor->setProcessing(false);
        if (active) component->setActive(false);
    }
};
ClipCache bake(Provider& provider, const ARA::ARAPlugInExtensionInstance* extension,
    const RegionNode& region, const SourceNode& source, const Project& project,
    const std::shared_ptr<std::atomic<bool>>& cancel,const std::function<void()>& tick,Sample first,Sample last,const audio::AudioData* previous=nullptr) {
    using namespace Steinberg::Vst;
    auto component = provider.getComponentPtr();
    Steinberg::FUnknownPtr<IAudioProcessor> processor(component);
    require(processor != nullptr, "ARA instance has no audio processor");
    constexpr int32 block = 512;
    const auto channels = source.pcm->channels;
    require(channels == 1 || channels == 2, "ARA v1 renderer requires mono/stereo");
    const auto layout = channels == 1 ? SpeakerArr::kMono : SpeakerArr::kStereo;
    std::vector<SpeakerArrangement> inputs(component->getBusCount(kAudio,kInput)), outputs(component->getBusCount(kAudio,kOutput));
    require(!outputs.empty(), "ARA renderer has no output bus");
    for (const auto dir : {kInput,kOutput}) {
        auto& buses = dir == kInput ? inputs : outputs;
        for (int32 b = 0; b < static_cast<int32>(buses.size()); ++b) {
            require(processor->getBusArrangement(dir,b,buses[b]) == kResultOk, "ARA bus query failed");
            if (!b) buses[b] = layout;
        }
    }
    require(processor->setBusArrangements(inputs.data(),static_cast<int32>(inputs.size()), outputs.data(),static_cast<int32>(outputs.size())) == kResultOk,
        "ARA renderer does not support source channel layout");
    for (const auto dir : {kInput,kOutput}) {
        for (int32 b = 0; b < component->getBusCount(kAudio,dir); ++b)
            require(component->activateBus(kAudio,dir,b,b == 0) == kResultOk, "ARA bus activation failed");
        for (int32 b = 0; b < component->getBusCount(kEvent,dir); ++b) component->activateBus(kEvent,dir,b,false);
    }
    ProcessSetup setup{kOffline,kSample32,block,static_cast<double>(project.sample_rate)};
    require(processor->canProcessSampleSize(kSample32) == kResultOk && processor->setupProcessing(setup) == kResultOk,
        "ARA offline setup unsupported");
    HostProcessData data;
    require(data.prepare(*component,block,kSample32), "ARA process buffer allocation failed");
    require(data.outputs[0].numChannels == static_cast<int32>(channels), "ARA output channel mismatch");
    data.processMode = kOffline;
    ProcessContext context{}; context.sampleRate = project.sample_rate; data.processContext = &context;
    ParameterChanges parameters;
    if (auto controller=provider.getControllerPtr()) {
        require(controller->getParameterCount()<=4096,"ARA parameter budget exceeded");
        for (int32 i=0;i<controller->getParameterCount();++i) {
            ParameterInfo info{}; if (controller->getParameterInfo(i,info)!=kResultOk) continue;
            int32 index{}; auto* queue=parameters.addParameterData(info.id,index);
            if (queue) queue->addPoint(0,controller->getParamNormalized(info.id),index);
        }
    }
    ARA::Host::PlaybackRenderer renderer(extension);
    renderer.addPlaybackRegion(region.region);
    struct Unbind { ARA::Host::PlaybackRenderer& renderer; ARA::ARAPlaybackRegionRef region;
        ~Unbind() { renderer.removePlaybackRegion(region); } } unbind{renderer,region.region};
    Activation activation{component,processor};
    require(component->setActive(true) == kResultOk, "ARA renderer activation failed"); activation.active = true;
    const auto latency = processor->getLatencySamples();
    require(latency <= 262144, "ARA latency exceeds PDC budget");
    require(region.clip.start+region.clip.length <= max_sample-latency,"ARA render tail exceeds timeline bounds");
    auto pcm = std::make_shared<audio::AudioData>(); pcm->sample_rate = project.sample_rate; pcm->channels = channels;
    require(first>=0&&last>first&&last<=region.clip.length,"Invalid ARA patch range");const auto length=last-first;
    if(previous)pcm->samples=previous->samples;else pcm->samples.resize(static_cast<std::size_t>(length)*channels);
    // DSP and any plugin-side PCM access run on a preparation worker. The UI
    // owns the model and does not edit it until this task is joined.
    await_preparation(std::async(std::launch::async, [&] {
        const ScopedNoDenormals floating_point_mode;
        const auto started = processor->setProcessing(true);
        require(started == kResultOk || started == kNotImplemented, "ARA processing start failed"); activation.processing = true;
        Timeline timeline(project.time,project.sample_rate);
        const auto total = length+latency;
        for (Sample at = 0; at < total;) {
            cancelled(cancel);
            data.inputParameterChanges = at==0 ? &parameters : nullptr;
            data.numSamples = static_cast<int32>(std::min<Sample>(block,total-at));
            const auto position = region.clip.start+first+at;
            const auto tick = timeline.to_ticks(position);
            context.projectTimeSamples = position; context.projectTimeMusic = static_cast<double>(tick)/ppq;
            context.tempo = project.time.tempos.front().bpm;
            for (const auto& tempo : project.time.tempos) {
                const auto boundary = timeline.to_samples(tempo.tick);
                if (boundary <= position) {
                    context.tempo = tempo.bpm;
                    context.projectTimeMusic = static_cast<double>(tempo.tick)/ppq +
                        static_cast<double>(position-boundary)*tempo.bpm/(60.*project.sample_rate);
                }
                else data.numSamples = static_cast<int32>(std::min<Sample>(data.numSamples,boundary-position));
            }
            context.timeSigNumerator = project.time.meters.front().numerator;
            context.timeSigDenominator = project.time.meters.front().denominator;
            auto musical = timeline.musical_position(tick);
            if (musical.bar > 1 && timeline.to_samples(timeline.to_ticks(MusicalPosition{musical.bar,1,0})) > position) --musical.bar;
            for (const auto& meter : project.time.meters) {
                const auto boundary = timeline.to_samples(timeline.to_ticks(MusicalPosition{meter.bar,1,0}));
                if (boundary <= position) { context.timeSigNumerator = meter.numerator; context.timeSigDenominator = meter.denominator; }
                else data.numSamples = static_cast<int32>(std::min<Sample>(data.numSamples,boundary-position));
            }
            context.barPositionMusic = static_cast<double>(timeline.to_ticks(MusicalPosition{musical.bar,1,0}))/ppq;
            context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid | ProcessContext::kProjectTimeMusicValid |
                ProcessContext::kTimeSigValid | ProcessContext::kBarPositionValid;
            for (int32 b = 0; b < data.numInputs; ++b) {
                data.inputs[b].silenceFlags = ~Steinberg::uint64{};
                for (int32 c = 0; c < data.inputs[b].numChannels; ++c) std::fill_n(data.inputs[b].channelBuffers32[c],data.numSamples,0.f);
            }
            for (int32 b = 0; b < data.numOutputs; ++b) {
                data.outputs[b].silenceFlags = 0;
                for (int32 c = 0; c < data.outputs[b].numChannels; ++c) std::fill_n(data.outputs[b].channelBuffers32[c],data.numSamples,0.f);
            }
            require(process_offline(processor,&data), "ARA offline process failed");
            for (int32 f = 0; f < data.numSamples; ++f) {
                const Sample output = at+f-latency;
                if (output < 0 || output >= length) continue;
                for (std::uint32_t c = 0; c < channels; ++c) {
                    const auto value = (data.outputs[0].silenceFlags & (Steinberg::uint64{1}<<c)) ? 0.f : data.outputs[0].channelBuffers32[c][f];
                    require(std::isfinite(value), "ARA renderer returned non-finite PCM");
                    pcm->samples[static_cast<std::size_t>(output+(previous?first:0))*channels+c] = value;
                }
            }
            at += data.numSamples;
        }
        const auto stopped = processor->setProcessing(false);
        require(stopped == kResultOk || stopped == kNotImplemented, "ARA processing stop failed"); activation.processing = false;
    }),tick);
    require(processor->getLatencySamples() == latency,"ARA latency changed during preparation; retry/relink required");
    auto clip=region.clip;if(!previous){clip.start+=first;clip.source_offset+=first;clip.length=length;}
    return {clip,source.snapshot.revision,std::move(pcm),latency};
}
}
std::vector<ClipCache> Session::synchronize(const Project& project, const std::vector<SourceSnapshot>& sources,
    std::shared_ptr<std::atomic<bool>> cancel) {
    impl_->check_thread(); require(!impl_->editing,"ARA sync during edit session");
    const auto preparation_started=std::chrono::steady_clock::now();
    impl_->stop_transport();
    if(!cancel)cancel=impl_->preparation_cancel;
    require(!impl_->model_failed,"ARA model failed; detach/relink explicitly"); project.validate(); cancelled(cancel);
    require(sources.size() <= 32, "ARA supports up to 32 sources per session");
    std::map<std::string,SourceSnapshot> selected;
    std::map<std::string,std::shared_ptr<const audio::AudioData>> prepared;
    std::map<std::string,std::uint64_t> checksums;
    std::uint64_t source_samples{}, cache_samples{};
    for (const auto& s : sources) {
        require(!s.id.empty() && s.pcm && selected.emplace(s.id,s).second, "Invalid or duplicate ARA source");
        s.pcm->validate();
        require(s.pcm->sample_rate == project.sample_rate, "ARA source/project sample-rate mismatch (resampling not enabled)");
        require(s.pcm->channels >= 1 && s.pcm->channels <= 2,"ARA source renderer supports mono/stereo");
        source_samples += static_cast<std::uint64_t>(s.pcm->frames())*s.pcm->channels;
        const auto existing = impl_->sources.find(s.id);
        if (existing != impl_->sources.end()) {
            require(existing->second->snapshot.revision==s.revision,"ARA source revision changed; detach/relink explicitly");
            if(existing->second->snapshot.pcm!=s.pcm){
                auto loaded=preload(s,cancel,impl_->preparation_wait);
                const auto checksum=await_preparation(std::async(std::launch::async,[loaded]{return pcm_checksum(*loaded);}),impl_->preparation_wait);
                require(checksum==existing->second->checksum,"ARA source media changed; saved archive preserved");
                existing->second->snapshot=s;
            }
        }
    }
    std::size_t new_sources{};
    for (const auto& [id,node] : impl_->sources) if (!selected.contains(id))
        source_samples += static_cast<std::uint64_t>(node->pcm->frames())*node->pcm->channels;
    for (const auto& [id,s] : selected) { (void)s; if (!impl_->sources.contains(id)) ++new_sources; }
    require(source_samples <= 64*1024*1024,"ARA source memory budget exceeded");
    for (const auto& clip : project.clips) if (!clip.midi && selected.contains(clip.source)) {
        const auto& s = *selected.at(clip.source).pcm;
        require(clip.source_offset <= s.frames() && clip.length <= s.frames()-clip.source_offset,"ARA clip exceeds source");
        cache_samples += static_cast<std::uint64_t>(clip.length)*s.channels;
    }
    require(cache_samples <= 64*1024*1024,"ARA render cache exceeds 256 MiB budget");
    require(impl_->sources.size()+new_sources <= 32,"ARA undo source budget exceeded");
    // Disable owner/vendor windows before starting any worker (focus-loss may
    // itself complete an editor gesture). The hook only pumps its modal HWND.
    if(impl_->preparation_wait)impl_->preparation_wait();
    cancelled(cancel);
    for (const auto& [id,s] : selected) if (!impl_->sources.contains(id)) {
        auto pcm=preload(s,cancel,impl_->preparation_wait);
        checksums.emplace(id,await_preparation(std::async(std::launch::async,[pcm]{return pcm_checksum(*pcm);}),impl_->preparation_wait));
        prepared.emplace(id,std::move(pcm));
    }
    const auto errors = impl_->host.errors.load();
    ARA::Host::DocumentController dc(impl_->document);
    std::vector<Clip> modelClips;for(const auto& c:project.clips)if(!c.midi&&selected.contains(c.source))modelClips.push_back(c);
    std::vector<std::pair<Id,std::string>> modelTracks;for(const auto& t:project.tracks)modelTracks.emplace_back(t.id,t.name);
    const bool sameSources=sources.size()==impl_->model_sources.size()&&std::equal(sources.begin(),sources.end(),impl_->model_sources.begin(),[](const auto& a,const auto& b){return a.id==b.id&&a.revision==b.revision&&a.pcm==b.pcm;});
    const bool modelChanged=!impl_->model_valid||!sameSources||modelClips!=impl_->model_clips||modelTracks!=impl_->model_tracks||project.time!=impl_->model_time||project.sample_rate!=impl_->model_rate||!impl_->restore.empty();
    if(modelChanged){live_->retire();live_->unsupported=false;impl_->cache_valid=false;
    ARA::ARAViewSelection emptySelection{}; emptySelection.structSize=sizeof(emptySelection);
    ARA::Host::EditorView(impl_->extension).notifySelection(&emptySelection);
    try { edit([&] {
        if (!impl_->context) {
            ARA::ARAMusicalContextProperties p{}; p.structSize = sizeof(p);
            impl_->context = dc.createMusicalContext(reinterpret_cast<ARA::ARAMusicalContextHostRef>(impl_.get()),&p);
            require(impl_->context != nullptr,"ARA musical context creation failed");
        }
        for(std::size_t order=0;order<project.tracks.size();++order){const auto& track=project.tracks[order];
            const bool visible=std::any_of(project.clips.begin(),project.clips.end(),[&](const auto& c){return !c.midi&&c.track==track.id&&selected.contains(c.source);});
            auto at=impl_->sequences.find(track.id.value);
            if(!visible){if(at!=impl_->sequences.end()&&at->second->assigned){ARA::Host::EditorRenderer(impl_->extension).removeRegionSequence(at->second->ref);at->second->assigned=false;}continue;}
            ARA::ARARegionSequenceProperties seq{};seq.structSize=sizeof(seq);seq.musicalContextRef=impl_->context;seq.name=track.name.c_str();seq.orderIndex=static_cast<ARA::ARAInt32>(order);
            auto& node=impl_->sequences[track.id.value];if(!node){node=std::make_unique<SequenceNode>();node->ref=dc.createRegionSequence(reinterpret_cast<ARA::ARARegionSequenceHostRef>(node.get()),&seq);require(node->ref!=nullptr,"ARA region sequence creation failed");}
            else dc.updateRegionSequenceProperties(node->ref,&seq);
            if(!node->assigned){ARA::Host::EditorRenderer(impl_->extension).addRegionSequence(node->ref);node->assigned=true;}
        }
        for (auto it = impl_->regions.begin(); it != impl_->regions.end();) {
            const auto found = std::find_if(project.clips.begin(),project.clips.end(),[&](const auto& c) {
                return c.id.value == it->first && !c.midi && selected.contains(c.source) && c.source == it->second->clip.source;
            });
            if (found == project.clips.end()) { dc.destroyPlaybackRegion(it->second->region); it = impl_->regions.erase(it); }
            else ++it;
        }
        for (const auto& [id,s] : selected) {
            if (!impl_->sources.contains(id)) {
                auto node = std::make_unique<SourceNode>(); node->snapshot = s; node->pcm = prepared.at(id);node->checksum=checksums.at(id);
                for(auto& reader:node->reader_pool)reader.pcm=node->pcm;
                auto* ref = node.get(); impl_->sources.emplace(id,std::move(node));
                const auto configured=impl_->source_ids.find(id);
                const auto& persistent=configured!=impl_->source_ids.end()?configured->second:impl_->persistent_id.empty()?id:impl_->persistent_id;
                ARA::ARAAudioSourceProperties p{}; p.structSize = sizeof(p); p.persistentID = persistent.c_str();
                p.sampleCount = ref->pcm->frames(); p.sampleRate = ref->pcm->sample_rate; p.channelCount = static_cast<ARA::ARAChannelCount>(ref->pcm->channels);
                p.merits64BitSamples = false;
                ref->source = dc.createAudioSource(reinterpret_cast<ARA::ARAAudioSourceHostRef>(ref),&p);
                require(ref->source != nullptr,"ARA source creation failed");
                ARA::ARAAudioModificationProperties m{}; m.structSize = sizeof(m); m.persistentID = persistent.c_str();
                ref->modification = dc.createAudioModification(ref->source,reinterpret_cast<ARA::ARAAudioModificationHostRef>(ref),&m);
                require(ref->modification != nullptr,"ARA modification creation failed");
                dc.enableAudioSourceSamplesAccess(ref->source,true); ref->enabled = true;
            }
            auto& ref = *impl_->sources.at(id);
            if (ref.dormant) {
                dc.deactivateAudioSourceForUndoHistory(ref.source,false);
                dc.deactivateAudioModificationForUndoHistory(ref.modification,false); ref.dormant = false;
                dc.enableAudioSourceSamplesAccess(ref.source,true); ref.enabled = true;
            }
        }
        for (const auto& clip : project.clips) if (!clip.midi && selected.contains(clip.source)) {
            ARA::ARAPlaybackRegionProperties p{}; p.structSize = sizeof(p); p.name = clip.name.c_str();
            p.startInModificationTime = static_cast<double>(clip.source_offset)/project.sample_rate;
            p.durationInModificationTime = p.durationInPlaybackTime = static_cast<double>(clip.length)/project.sample_rate;
            p.startInPlaybackTime = static_cast<double>(clip.start)/project.sample_rate;
            p.musicalContextRef = impl_->context; p.regionSequenceRef = impl_->sequences.at(clip.track.value)->ref;
            auto& node = impl_->regions[clip.id.value];
            if (!node) node = std::make_unique<RegionNode>();
            if (node->region) dc.updatePlaybackRegionProperties(node->region,&p);
            else node->region = dc.createPlaybackRegion(impl_->sources.at(clip.source)->modification,
                reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(node.get()),&p);
            require(node->region != nullptr,"ARA playback region creation failed"); node->clip = clip;
        }
        for(auto it=impl_->restore.begin();it!=impl_->restore.end();){
            if(!selected.contains(it->source)){++it;continue;}
            require(it->source_checksum&&impl_->sources.at(it->source)->checksum==it->source_checksum,"ARA source media changed; saved archive preserved");
            Host::Archive archive{it->archive,it->archive_id};impl_->host.archive=&archive;
            const ARA::ARAPersistentID id=it->persistent_id.c_str();
            ARA::ARARestoreObjectsFilter filter{};filter.structSize=sizeof(filter);filter.audioSourceIDsCount=1;filter.audioSourceArchiveIDs=&id;filter.audioModificationIDsCount=1;filter.audioModificationArchiveIDs=&id;
            const auto restored=dc.restoreObjectsFromArchive(reinterpret_cast<ARA::ARAArchiveReaderHostRef>(&archive),&filter);impl_->host.archive=nullptr;
            require(restored&&!archive.failed,"ARA archive restore failed");
            if(impl_->restore_document && std::none_of(std::next(it),impl_->restore.end(),[&](const auto& b){return selected.contains(b.source);})){
                // Document-private data comes last, after all source states.
                filter={};filter.structSize=sizeof(filter);filter.documentData=true;impl_->host.archive=&archive;
                const auto restoredDocument=dc.restoreObjectsFromArchive(reinterpret_cast<ARA::ARAArchiveReaderHostRef>(&archive),&filter);impl_->host.archive=nullptr;
                require(restoredDocument&&!archive.failed,"ARA document archive restore failed");impl_->restore_document=false;
            }
            it=impl_->restore.erase(it);
        }
        for (auto& [id,s] : impl_->sources) if (!selected.contains(id) && !s->dormant) {
            if (s->enabled) { dc.enableAudioSourceSamplesAccess(s->source,false); s->enabled = false; }
            dc.deactivateAudioModificationForUndoHistory(s->modification,true);
            dc.deactivateAudioSourceForUndoHistory(s->source,true); s->dormant = true;
        }
    }); } catch (...) { impl_->model_failed = true; throw; }
    impl_->model_clips=std::move(modelClips);impl_->model_tracks=std::move(modelTracks);impl_->model_sources=sources;impl_->model_time=project.time;impl_->model_rate=project.sample_rate;impl_->model_valid=true;
    if(impl_->selected_clip){if(impl_->regions.contains(impl_->selected_clip->value))select_clip(*impl_->selected_clip);else impl_->selected_clip.reset();}
    if(impl_->view_attached)impl_->request_analysis();
    }
    notify(); cancelled(cancel);
    if(impl_->host.analysing.load())return {};
    if(live_->preferred&&!live_->unsupported&&impl_->transport_blocks.load()==0){
        // The persistent editor instance used to be initialized by bake().
        // Initialize it independently when playback no longer goes through it.
        // RePitch otherwise corrupts its heap during termination of an instance
        // that has displayed its UI but has never processed an editor block.
        try{
            impl_->warming_editor=true;
            struct Stop{Impl& impl;~Stop(){impl.stop_transport();impl.warming_editor=false;}} stop{*impl_};
            transport(project,audio::RealtimeState{});
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(!impl_->transport_blocks.load()&&!impl_->transport_error.load()&&std::chrono::steady_clock::now()<deadline){cancelled(cancel);if(impl_->preparation_wait)impl_->preparation_wait();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
            require(impl_->transport_blocks.load()!=0&&!impl_->transport_error.load(),"ARA editor renderer initialization failed");
        }catch(const std::exception&){cancelled(cancel);live_->unsupported=true;}
        notify();cancelled(cancel);if(impl_->host.analysing.load())return {};
    }
    const auto rendering_revision = impl_->host.content_revision.load();
    const auto globalRevision=impl_->host.global_audio_revision.load();
    if(globalRevision!=impl_->cached_global){live_->retire();live_->unsupported=false;}
    PreparationStats stats;std::map<std::string,ClipCache> preparedCaches;
    std::array<std::uint64_t,5> invalidations{};
    constexpr std::uint32_t causes[]{preparation_parameter,preparation_analysis,preparation_restart,preparation_explicit,preparation_off_thread};
    for(std::size_t n=0;n<invalidations.size();++n){invalidations[n]=impl_->host.invalidations[n].load();if(invalidations[n]!=impl_->cached_invalidations[n])stats.causes|=causes[n];}
    if(!impl_->cache_valid)stats.causes|=preparation_model;
    bool liveSupported=live_->preferred&&!live_->unsupported&&impl_->transport_blocks.load()!=0&&!impl_->regions.empty()&&impl_->regions.size()<=32;
    if(liveSupported&&live_->renderers.empty()){
        try{for(const auto& [id,r]:impl_->regions)live_->renderers.emplace(id,std::make_shared<Live::Renderer>(impl_,*r,project));}
        catch(const std::exception&){live_->retire();live_->unsupported=true;liveSupported=false;}
    }
    liveSupported=liveSupported&&!live_->renderers.empty();
    if(!liveSupported&&impl_->stats.live){impl_->cache_valid=false;stats.causes|=preparation_model;}
    std::vector<ClipCache> result;
    for (const auto& [id,r] : impl_->regions) {
        cancelled(cancel);const auto& source=*impl_->sources.at(r->clip.source);const auto old=impl_->caches.find(id);
        if(liveSupported){
            if(old!=impl_->caches.end()&&old->second.clip==r->clip) {result.push_back(old->second);preparedCaches.emplace(id,old->second);}
            else{auto placeholder=std::make_shared<audio::AudioData>();placeholder->sample_rate=project.sample_rate;placeholder->channels=source.pcm->channels;placeholder->samples.resize(static_cast<std::size_t>(r->clip.length)*placeholder->channels);ClipCache cache{r->clip,source.snapshot.revision,std::move(placeholder),0};result.push_back(cache);preparedCaches.emplace(id,std::move(cache));}
            ++stats.live;continue;
        }
        bool full=!impl_->cache_valid||globalRevision!=impl_->cached_global||old==impl_->caches.end()||old->second.clip!=r->clip||old->second.source_revision!=source.snapshot.revision;
        Sample first=r->clip.length,last{};
        auto affected=[&](const DirtyRange& dirty,double offset){if(!dirty.changed)return;if(dirty.full){stats.causes|=preparation_unknown_range;full=true;return;}
            const double rate=project.sample_rate;const auto begin=(dirty.first+offset)*rate,end=(dirty.last+offset)*rate;
            if(!std::isfinite(begin)||!std::isfinite(end)){full=true;return;}
            if(end<0||begin>=static_cast<double>(r->clip.length))return;
            first=std::min(first,static_cast<Sample>(std::floor(std::clamp(begin,0.,static_cast<double>(r->clip.length)))));
            last=std::max(last,static_cast<Sample>(std::ceil(std::clamp(end+1.,0.,static_cast<double>(r->clip.length)))));
        };
        affected(source.dirty,-static_cast<double>(r->clip.source_offset)/project.sample_rate);
        affected(r->dirty,-static_cast<double>(r->clip.start)/project.sample_rate);
        if(!full&&last<=first){result.push_back(old->second);preparedCaches.emplace(id,old->second);++stats.reused;continue;}
        if(!full){double head{},tail{};dc.getPlaybackRegionHeadAndTailTime(r->region,&head,&tail);
            Steinberg::FUnknownPtr<Steinberg::Vst::IAudioProcessor> processor(impl_->provider->getComponentPtr());
            const auto history=processor?processor->getTailSamples():Steinberg::Vst::kInfiniteTail;
            const double margin=(head+tail)*project.sample_rate+history+old->second.compensated_latency+512.;
            if(history==Steinberg::Vst::kInfiniteTail||!std::isfinite(head)||!std::isfinite(tail)||head<0||tail<0||!std::isfinite(margin)||margin>=static_cast<double>(r->clip.length)){stats.causes|=preparation_context;full=true;}
            else{const auto padding=static_cast<Sample>(std::ceil(margin));first=std::max<Sample>(0,first-padding);last+=std::min(padding,r->clip.length-last);}
        }
        if(full){first=0;last=r->clip.length;}
        const bool patch=first!=0||last!=r->clip.length;
        auto rendered=bake(*impl_->provider,impl_->extension,*r,source,project,cancel,impl_->preparation_wait,first,last,patch?old->second.pcm.get():nullptr);
        stats.rendered_frames+=static_cast<std::uint64_t>(last-first)+rendered.compensated_latency;
        // A changed latency invalidates the whole cache, including untouched samples.
        if(!full&&rendered.compensated_latency!=old->second.compensated_latency){stats.causes|=preparation_latency;first=0;last=r->clip.length;full=true;rendered.pcm.reset();rendered=bake(*impl_->provider,impl_->extension,*r,source,project,cancel,impl_->preparation_wait,first,last);stats.rendered_frames+=static_cast<std::uint64_t>(last)+rendered.compensated_latency;}
        if(first==0&&last==r->clip.length){++stats.full;}
        else ++stats.partial;
        result.push_back(rendered);preparedCaches.emplace(id,std::move(rendered));
    }
    notify();
    require(impl_->host.content_revision.load() == rendering_revision,
        "ARA content changed during preparation; refresh before publishing");
    if (impl_->host.errors.load() != errors) {
        impl_->model_failed = true; throw std::runtime_error("ARA source reader failed; detach/relink explicitly");
    }
    stats.milliseconds=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-preparation_started).count());
    impl_->caches=std::move(preparedCaches);impl_->cached_global=globalRevision;impl_->cached_invalidations=invalidations;impl_->cache_valid=true;impl_->stats=stats;
    for(auto& [id,s]:impl_->sources){(void)id;s->dirty={};}for(auto& [id,r]:impl_->regions){(void)id;r->dirty={};}
    return result;
}
ModelStats Session::model_stats() const {
    impl_->check_thread(); return {impl_->sources.size(),impl_->sources.size(),impl_->regions.size(),impl_->host.readers.load(),impl_->host.reads.load(),impl_->host.errors.load(),impl_->sequences.size()};
}
std::uint64_t Session::content_revision() const { impl_->check_thread(); return impl_->host.content_revision.load(); }
std::uint64_t Session::state_revision() const {impl_->check_thread();return impl_->host.state_revision.load()+impl_->host.content_revision.load();}
void Session::invalidate(){impl_->check_thread();impl_->host.invalidate_audio();}
PreparationStats Session::preparation_stats() const {impl_->check_thread();return impl_->stats;}
bool Session::analysing() const { impl_->check_thread(); return impl_->host.analysing.load(); }
void Session::set_preparation(std::shared_ptr<std::atomic<bool>> cancel,std::function<void()> wait) {
    impl_->check_thread();impl_->preparation_cancel=std::move(cancel);impl_->preparation_wait=std::move(wait);
}
bool Session::open_editor(void* parent,int& width,int& height) {
    impl_->check_thread(); impl_->close_view();
    impl_->transport_error=false;
    impl_->request_analysis();notify();
    const auto controller=impl_->provider->getControllerPtr();
    require(controller!=nullptr,"ARA editor: VST3 controller is unavailable");
    impl_->view=Steinberg::owned(controller->createView(Steinberg::Vst::ViewType::kEditor));
    require(impl_->view!=nullptr,"ARA editor: createView(editor) returned no view");
    const auto fail=[&](const std::string& reason) { impl_->close_view(); throw std::runtime_error("ARA editor: "+reason); };
    const auto platform=impl_->view->isPlatformTypeSupported(Steinberg::kPlatformTypeHWND);
    if(platform!=Steinberg::kResultOk)fail("HWND is unsupported (result "+std::to_string(platform)+")");
    impl_->parent=static_cast<HWND>(parent); impl_->view->setFrame(impl_.get());
    Steinberg::ViewRect size{};
    const auto validSize=[](const Steinberg::ViewRect& r){return r.getWidth()>0 && r.getHeight()>0 && r.getWidth()<=8192 && r.getHeight()<=8192;};
    const bool initialSize=impl_->view->getSize(&size)==Steinberg::kResultOk && validSize(size);
    impl_->host.requests_enabled=true;
    const auto attached=impl_->view->attached(parent,Steinberg::kPlatformTypeHWND);
    if(attached!=Steinberg::kResultOk)fail("attached(HWND) failed (result "+std::to_string(attached)+")");
    impl_->view_attached=true;
    // Some native editors (RePitch/VocAlign) report 0 x 0 until attached.
    // Keep a valid pre-attach size as a fallback, but never reject a lazy view
    // before it has had the opportunity to create its native controls.
    Steinberg::ViewRect attachedSize{};
    const auto sized=impl_->view->getSize(&attachedSize);
    if(sized==Steinberg::kResultOk && validSize(attachedSize))size=attachedSize;
    else if(!initialSize)fail("getSize after attached failed (result "+std::to_string(sized)+", "+std::to_string(attachedSize.getWidth())+" x "+std::to_string(attachedSize.getHeight())+")");
    width=size.getWidth(); height=size.getHeight();
    return true;
}
bool Session::resize_editor(int& width,int& height) {
    impl_->check_thread();
    if(!impl_->view_attached||!impl_->view||impl_->view->canResize()!=Steinberg::kResultOk)return false;
    Steinberg::ViewRect size{0,0,width,height};impl_->view->checkSizeConstraint(&size);
    if(size.getWidth()<1||size.getHeight()<1||size.getWidth()>8192||size.getHeight()>8192)return false;
    // IPlugView sizing contract: resize the host window to the constrained
    // rectangle before onSize. Native views may query their parent in onSize.
    RECT previous{};if(!GetClientRect(impl_->parent,&previous))return false;
    if(!SetWindowPos(impl_->parent,nullptr,0,0,size.getWidth(),size.getHeight(),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))return false;
    if(impl_->view->onSize(&size)!=Steinberg::kResultOk){SetWindowPos(impl_->parent,nullptr,0,0,previous.right,previous.bottom,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);return false;}
    width=size.getWidth();height=size.getHeight();return true;
}
void Session::close_editor() { impl_->check_thread(); impl_->close_view();impl_->selected_clip.reset(); }
void Session::select_clip(const Id& clip) {
    impl_->check_thread(); const auto at=impl_->regions.find(clip.value);
    require(at!=impl_->regions.end(),"ARA clip no longer exists");
    const auto region=at->second->region;
    const auto& clip_model=at->second->clip;
    ARA::ARAViewSelection selection{}; selection.structSize=sizeof(selection);
    selection.playbackRegionRefsCount=1; selection.playbackRegionRefs=&region;
    const auto sequence=impl_->sequences.at(clip_model.track.value)->ref;
    // Object selection, not a timeline-range selection (ARA 2 EditorView contract).
    selection.regionSequenceRefsCount=1;selection.regionSequenceRefs=&sequence;selection.timeRange=nullptr;
    ARA::Host::EditorView(impl_->extension).notifySelection(&selection);
    impl_->selected_clip=clip;
}
bool Session::transport_failed() const {impl_->check_thread();return impl_->transport_error.load();}
std::vector<TransportRequest> Session::take_transport_requests() {
    impl_->check_thread();std::lock_guard lock(impl_->host.request_mutex);
    std::vector<TransportRequest> result(impl_->host.requests.begin(),impl_->host.requests.begin()+impl_->host.request_count);
    impl_->host.request_count=0;return result;
}
namespace {
std::uint64_t state_checksum(const AraBinding& b){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto* blob:{&b.archive,&b.component_state,&b.controller_state}){
        auto size=blob->size();for(unsigned i=0;i<8;++i){hash^=(static_cast<std::uint64_t>(size)>>(i*8))&255;hash*=1099511628211ull;}
        for(auto byte:*blob){hash^=std::to_integer<unsigned>(byte);hash*=1099511628211ull;}}
    return hash;
}
}
void Session::persistent_id(std::string id){impl_->check_thread();require(impl_->sources.empty(),"ARA persistent ID already in use");impl_->persistent_id=std::move(id);}
void Session::configure_source(const AraBinding& b){
    impl_->check_thread();
    if(!impl_->sources.contains(b.source))for(const auto& [source,id]:impl_->source_ids)if(source!=b.source&&id==b.persistent_id&&impl_->sources.contains(source)){
        impl_->stop_transport();auto node=impl_->sources.extract(source);node.key()=b.source;node.mapped()->snapshot.id=b.source;impl_->sources.insert(std::move(node));break;
    }
    const auto old=impl_->source_ids.find(b.source);require(old==impl_->source_ids.end()||old->second==b.persistent_id,"ARA source identity changed");
    impl_->source_ids[b.source]=b.persistent_id;
    if(impl_->sources.contains(b.source)||b.archive.empty()||std::any_of(impl_->restore.begin(),impl_->restore.end(),[&](const auto& r){return r.source==b.source;}))return;
    require(b.factory_id==impl_->info.factory_id&&b.archive_id==impl_->info.archive_id&&b.version==impl_->info.version,"Incompatible ARA factory/archive/version; saved state preserved");
    require(b.checksum==state_checksum(b),"Corrupted ARA state checksum; saved state preserved");impl_->restore.push_back(b);
}
void Session::relocate_source(const std::string& id,std::shared_ptr<const audio::AudioData> data){
    impl_->check_thread();impl_->stop_transport();const auto at=impl_->sources.find(id);if(at==impl_->sources.end())return;
    const auto& pcm=*at->second->pcm;require(data&&data->frames()==pcm.frames()&&data->channels==pcm.channels&&data->sample_rate==pcm.sample_rate,"ARA media relocation geometry changed");
    // Only Application's verified MediaCopy path may rebind this immutable
    // descriptor. The ARA reader keeps its already decoded, owned PCM.
    at->second->snapshot.pcm=std::move(data);
}
void Session::save_state(AraBinding& b){
    impl_->check_thread();impl_->stop_transport();notify();require(!analysing(),"Wait for ARA analysis before saving");
    require(!impl_->model_failed,"Cannot archive a failed ARA model");
    if(impl_->sources.empty())return;
    Host::Archive archive{{},impl_->info.archive_id};impl_->host.archive=&archive;
    const auto stored=ARA::Host::DocumentController(impl_->document).storeObjectsToArchive(reinterpret_cast<ARA::ARAArchiveWriterHostRef>(&archive),nullptr);impl_->host.archive=nullptr;
    require(stored&&!archive.failed,"ARA archive capture failed (1 MiB limit); project was not saved");
    auto capture=[](auto* instance){std::vector<std::byte> bytes;if(!instance)return bytes;Steinberg::MemoryStream stream;const auto result=instance->getState(&stream);
        if(result==Steinberg::kNotImplemented)return bytes;
        require(result==Steinberg::kResultOk,"ARA companion state capture failed");require(stream.getSize()<=1024*1024,"ARA companion state exceeds 1 MiB");
        bytes.resize(static_cast<std::size_t>(stream.getSize()));if(!bytes.empty())std::memcpy(bytes.data(),stream.getData(),bytes.size());return bytes;};
    auto component=capture(impl_->provider->getComponentPtr().get());
    std::vector<std::byte> controller;
    if(auto edit=impl_->provider->getControllerPtr()){
        auto raw=capture(edit.get());std::vector<std::pair<Steinberg::Vst::ParamID,double>> parameters;
        require(edit->getParameterCount()>=0&&edit->getParameterCount()<=4096,"ARA parameter capture budget exceeded");
        for(Steinberg::int32 i=0;i<edit->getParameterCount();++i){Steinberg::Vst::ParameterInfo info{};if(edit->getParameterInfo(i,info)!=Steinberg::kResultOk)continue;const auto value=edit->getParamNormalized(info.id);require(std::isfinite(value)&&value>=0&&value<=1,"Invalid ARA parameter state");parameters.emplace_back(info.id,value);}
        for(char c:std::string_view("MRSCTL01"))controller.push_back(static_cast<std::byte>(c));
        auto put=[&](std::uint64_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)controller.push_back(static_cast<std::byte>((value>>(i*8))&255));};
        put(raw.size(),4);put(parameters.size(),4);controller.insert(controller.end(),raw.begin(),raw.end());
        for(const auto& [id,value]:parameters){put(id,4);put(std::bit_cast<std::uint64_t>(value),8);}
        require(controller.size()<=1024*1024,"ARA controller/parameters exceed 1 MiB");
    }
    b.archive=std::move(archive.bytes);b.component_state=std::move(component);b.controller_state=std::move(controller);b.checksum=state_checksum(b);
    const auto source=impl_->sources.find(b.source);b.source_checksum=source==impl_->sources.end()?impl_->sources.begin()->second->checksum:source->second->checksum;
}
void Session::restore_state(const AraBinding& b){
    impl_->check_thread();require(impl_->sources.empty(),"Restore ARA before creating sources");
    require(b.factory_id==impl_->info.factory_id&&b.archive_id==impl_->info.archive_id&&b.version==impl_->info.version,"Incompatible ARA factory/archive/version; saved state preserved");
    if(b.archive.empty()&&b.component_state.empty()&&b.controller_state.empty())return;
    require(b.checksum==state_checksum(b),"Corrupted ARA state checksum; saved state preserved");
    auto component=impl_->provider->getComponentPtr();auto controller=impl_->provider->getControllerPtr();
    if(!b.component_state.empty()){Steinberg::MemoryStream stream(const_cast<std::byte*>(b.component_state.data()),b.component_state.size());require(component->setState(&stream)==Steinberg::kResultOk,"ARA component restore failed");stream.seek(0,Steinberg::IBStream::kIBSeekSet,nullptr);if(controller)controller->setComponentState(&stream);}
    if(!b.controller_state.empty()){
        require(controller!=nullptr,"ARA saved controller is unavailable");const auto& bytes=b.controller_state;
        require(bytes.size()>=16&&std::memcmp(bytes.data(),"MRSCTL01",8)==0,"Unsupported ARA controller envelope; state preserved");std::size_t at=8;
        auto get=[&](unsigned count){require(count<=bytes.size()-at,"Truncated ARA controller envelope");std::uint64_t value{};for(unsigned i=0;i<count;++i)value|=static_cast<std::uint64_t>(std::to_integer<unsigned>(bytes[at++]))<<(i*8);return value;};
        const auto raw=get(4),count=get(4);require(count<=4096&&raw<=bytes.size()-at&&bytes.size()-at-raw==count*12,"Invalid ARA controller envelope");
        if(raw){Steinberg::MemoryStream stream(const_cast<std::byte*>(bytes.data()+at),raw);require(controller->setState(&stream)==Steinberg::kResultOk,"ARA controller restore failed");}at+=raw;
        for(std::uint64_t i=0;i<count;++i){const auto id=static_cast<Steinberg::Vst::ParamID>(get(4));const auto value=std::bit_cast<double>(get(8));require(std::isfinite(value)&&value>=0&&value<=1&&controller->setParamNormalized(id,value)==Steinberg::kResultOk,"ARA parameter restore failed");}
    }
    if(!b.archive.empty()){configure_source(b);impl_->restore_document=true;}
}
void Session::transport(const Project& project,const audio::RealtimeState& state) {
    using namespace Steinberg::Vst;
    impl_->check_thread();
    if(!impl_->view_attached&&!impl_->warming_editor)return;
    if(impl_->transport_error.load()){impl_->stop_transport();return;}
    if(analysing()){impl_->stop_transport();return;}
    if(impl_->view_attached&&!impl_->warming_editor){
        try {
            bool audible=true;
            if(impl_->selected_clip){const auto region=impl_->regions.find(impl_->selected_clip->value);if(region!=impl_->regions.end())for(const auto& b:project.ara_bindings)if(b.source==region->second->clip.source)audible=!b.bypass;}
            live_->editor->audible=audible;
            live_->editor->prepare(project);
        }catch(...){impl_->transport_error=true;impl_->stop_transport();}
        return;
    }
    const Timeline timeline(project.time,project.sample_rate);
    auto music=[&](Sample sample){double result{};for(const auto& tempo:project.time.tempos){const auto anchor=timeline.to_samples(tempo.tick);if(anchor>sample)break;result=static_cast<double>(tempo.tick)/ppq+static_cast<double>(sample-anchor)*tempo.bpm/(60.*project.sample_rate);}return result;};
    ProcessContext context{};context.sampleRate=project.sample_rate;context.projectTimeSamples=state.sample;context.projectTimeMusic=music(state.sample);
    context.tempo=project.time.tempos.front().bpm;
    for(const auto& tempo:project.time.tempos)if(timeline.to_samples(tempo.tick)<=state.sample)context.tempo=tempo.bpm;
    context.timeSigNumerator=project.time.meters.front().numerator;context.timeSigDenominator=project.time.meters.front().denominator;
    for(const auto& meter:project.time.meters)if(timeline.to_samples(timeline.to_ticks(MusicalPosition{meter.bar,1,0}))<=state.sample){context.timeSigNumerator=meter.numerator;context.timeSigDenominator=meter.denominator;}
    auto musical=timeline.musical_position(timeline.to_ticks(state.sample));
    if(musical.bar>1&&timeline.to_samples(timeline.to_ticks(MusicalPosition{musical.bar,1,0}))>state.sample)--musical.bar;
    context.barPositionMusic=static_cast<double>(timeline.to_ticks(MusicalPosition{musical.bar,1,0}))/ppq;
    context.systemTime=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    context.state=ProcessContext::kTempoValid|ProcessContext::kTimeSigValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kBarPositionValid|ProcessContext::kSystemTimeValid;
    if(state.playback==PlaybackState::playing&&state.count_remaining==0)context.state|=ProcessContext::kPlaying;
    if(state.loop){context.state|=ProcessContext::kCycleValid|ProcessContext::kCycleActive;context.cycleStartMusic=music(state.loop->start);context.cycleEndMusic=music(state.loop->end);}
    try {
        if(!impl_->transport_worker.joinable()){
            auto component=impl_->provider->getComponentPtr();Steinberg::FUnknownPtr<IAudioProcessor> processor(component);
            require(processor!=nullptr,"ARA editor has no context processor");
            ProcessSetup setup{kRealtime,kSample32,512,static_cast<double>(project.sample_rate)};
            require(processor->setupProcessing(setup)==Steinberg::kResultOk,"ARA editor context setup failed");
            auto buffers=std::make_shared<HostProcessData>();
            require(buffers->prepare(*component,64,kSample32),"ARA editor context buffer allocation failed");
            buffers->processMode=kRealtime;buffers->numSamples=64;
            require(component->setActive(true)==Steinberg::kResultOk,"ARA editor context activation failed");impl_->transport_active=true;
            impl_->transport_stopping=false;
            impl_->transport_worker=std::thread([self=impl_.get(),processor=std::move(processor),buffers=std::move(buffers)]{
                const ScopedNoDenormals floating_point_mode;bool processing=false;
                try {const auto result=processor->setProcessing(true);require(result==Steinberg::kResultOk||result==Steinberg::kNotImplemented,"ARA editor context processing failed");processing=true;
                    for(;;){ProcessContext current{};{std::unique_lock lock(self->transport_mutex);self->transport_condition.wait(lock,[&]{return self->transport_stopping||self->transport_pending;});if(self->transport_stopping)break;current=self->transport_context;self->transport_pending=false;}
                        auto& data=*buffers;data.processContext=&current;
                        for(int32 b=0;b<data.numInputs;++b){data.inputs[b].silenceFlags=~Steinberg::uint64{};
                            for(int32 c=0;c<data.inputs[b].numChannels;++c)std::fill_n(data.inputs[b].channelBuffers32[c],data.numSamples,0.f);}
                        for(int32 b=0;b<data.numOutputs;++b){data.outputs[b].silenceFlags=0;
                            for(int32 c=0;c<data.outputs[b].numChannels;++c)std::fill_n(data.outputs[b].channelBuffers32[c],data.numSamples,0.f);}
                        // Zero-frame flushes can skip the plugin audio callback,
                        // where editors read transport. Run a bounded silent-input
                        // block instead; discard ALL output, never feed the MR graph.
                        require(process_offline(processor,&data),"ARA editor context block rejected");
                        ++self->transport_blocks;
                    }
                }catch(...){self->transport_error=true;}
                if(processing)processor->setProcessing(false);
            });
        }
        {std::lock_guard lock(impl_->transport_mutex);impl_->transport_context=context;impl_->transport_pending=true;}
        impl_->transport_condition.notify_one();
    }catch(...){impl_->transport_error=true;impl_->stop_transport();}
}
}
