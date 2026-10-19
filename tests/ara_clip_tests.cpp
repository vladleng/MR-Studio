#include <mrs/ara.hpp>
#include <mrs/desktop.hpp>
#include <mrs/audio.hpp>
#include <mrs/device.hpp>
#include <mrs/ahead_renderer.hpp>
#include <mrs/recording.hpp>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <chrono>
#include <fstream>
#include <thread>
#include <windows.h>

namespace {
thread_local bool probe_alloc{};
std::atomic<std::uint64_t> allocations{}, frees{};
}
void* operator new(std::size_t n) { if (probe_alloc) ++allocations; if (auto p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc{}; }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if (probe_alloc && p) ++frees; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
using namespace mrs;
namespace {
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
#define CHECK(...) check((__VA_ARGS__),#__VA_ARGS__)
template<class F> void rejects(F f) { bool bad{}; try { f(); } catch (const std::exception&) { bad = true; } CHECK(bad); }
std::shared_ptr<audio::AudioData> pcm(std::uint32_t channels) {
    auto data = std::make_shared<audio::AudioData>(); data->channels = channels; data->samples.resize(4096*channels);
    for (std::size_t f = 0; f < 4096; ++f) for (std::uint32_t c = 0; c < channels; ++c)
        data->samples[f*channels+c] = (c ? -.2f : .1f)+static_cast<float>(f)*.00001f;
    return data;
}
Project project() {
    Project p; p.id = {"ara-project"}; p.title = "ARA fixture";
    p.tracks.push_back({Id{"track"},"Track",TrackKind::audio,{}});
    p.clips.push_back({Id{"clip"},Id{"track"},"Clip",64,1024,127,"source"}); p.validate(); return p;
}
void compare(const ara::ClipCache& cache,const audio::AudioData& source,float gain) {
    CHECK(cache.pcm->frames() == cache.clip.length);
    for (Sample f = 0; f < cache.clip.length; ++f) for (std::uint32_t c = 0; c < source.channels; ++c) {
        const auto expected = source.samples[static_cast<std::size_t>(f+cache.clip.source_offset)*source.channels+c]*gain;
        CHECK(std::abs(cache.pcm->samples[static_cast<std::size_t>(f)*source.channels+c]-expected) < .00001f);
    }
}
audio::RenderGraph graph(const Project& p,const std::shared_ptr<const audio::AudioData>& source) {
    audio::RenderGraph g;
    for (const auto& c : p.clips) if (!c.midi) {
        audio::Voice voice{source,c.start,c.source_offset,c.length,{}};
        for (std::uint32_t ch = 0; ch < source->channels; ++ch) voice.routes.push_back({ch,ch,1});
        voice.fade_in = c.fade_in; voice.fade_out = c.fade_out; g.voices.push_back(std::move(voice));
    }
    return g;
}
void engine_test(const Project& p,const std::shared_ptr<audio::AudioData>& source,ara::Arrangement& arrangement,float gain) {
    auto g = graph(p,source); arrangement.apply(p,g);
    auto engine = std::make_shared<audio::AudioEngine>();
    engine->prepare({48000,0,source->channels,512,128},g);
    CHECK(engine->compensation().output == 0); // ARA latency already removed from the cache
    audio::EngineTransport transport(engine,Timeline(p.time,48000)); transport.play();
    std::vector<float> output(512*source->channels);
    std::vector<float> rendered;
    for (int i = 0; i < 9; ++i) { engine->process(nullptr,output.data(),128); rendered.insert(rendered.end(),output.begin(),output.begin()+128*source->channels); }
    for (Sample f = 0; f < 1152; ++f) for (std::uint32_t c = 0; c < source->channels; ++c) {
        const auto relative = f-p.clips.front().start;
        const float expected = relative < 0 || relative >= p.clips.front().length ? 0.f :
            source->samples[static_cast<std::size_t>(relative+p.clips.front().source_offset)*source->channels+c]*gain;
        CHECK(std::abs(rendered[static_cast<std::size_t>(f)*source->channels+c]-expected)<.00001f);
    }
    transport.seek(80); transport.set_loop(LoopRange{80,96}); transport.play();
    for (int i = 0; i < 2; ++i) engine->process(nullptr,output.data(),128);
    for (std::uint32_t f = 0; f < 128; ++f) for (std::uint32_t c = 0; c < source->channels; ++c) {
        const auto source_at = 80+f%16-p.clips.front().start+p.clips.front().source_offset;
        CHECK(std::abs(output[f*source->channels+c]-source->samples[static_cast<std::size_t>(source_at)*source->channels+c]*gain)<.00001f);
    }
    for (const auto frames : {1U,7U,511U,32U,129U}) {
        const auto position = engine->state().sample;
        engine->process(nullptr,output.data(),frames);
        for (std::uint32_t f = 0; f < frames; ++f) for (std::uint32_t c = 0; c < source->channels; ++c) {
            const auto at = 80+(position-80+f)%16-p.clips.front().start+p.clips.front().source_offset;
            CHECK(std::abs(output[f*source->channels+c]-source->samples[static_cast<std::size_t>(at)*source->channels+c]*gain)<.00001f);
        }
    }
    allocations = frees = 0; const auto before = std::chrono::steady_clock::now();
    probe_alloc = true;
    std::uint64_t max_ns{};
    for (int i = 0; i < 1000; ++i) {
        const auto start = std::chrono::steady_clock::now();
        engine->process(nullptr,output.data(),128);
        const auto ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
        if (ns > max_ns) max_ns = ns;
        engine->observe(ns,128,0);
    }
    probe_alloc = false;
    CHECK(allocations == 0 && frees == 0);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-before).count();
    CHECK(engine->metrics().measured_callbacks == 1000);
    std::cout << "1000x128 callbacks " << elapsed << " us, max=" << max_ns << " ns, deadline misses=" << engine->metrics().deadline_misses << ", alloc/free=0\n";
    transport.stop(); engine->process(nullptr,output.data(),128);
    for (std::uint32_t i = 0; i < 128*source->channels; ++i) CHECK(output[i] == 0.f);
}
void model_test(const std::string& path,const std::string& id,float gain,std::uint32_t channels) {
    auto p = project(); auto source = pcm(channels); const auto original = source->samples;
    auto session = ara::Session::open(path,id);
    auto caches = session->synchronize(p,{{"source",1,source}});
    CHECK(caches.size() == 1); compare(caches.front(),*source,gain);
    CHECK(session->model_stats().sources == 1 && session->model_stats().regions == 1);
    ProjectStore store(p);
    store.execute(SplitAudioClip{Id{"clip"},512,Id{"right"}});
    caches = session->synchronize(*store.state().project,{{"source",1,source}});
    CHECK(caches.size() == 2 && session->model_stats().modifications == 1);
    for (const auto& c : caches) compare(c,*source,gain);
    store.execute(MoveAudioClip{Id{"right"},Id{"track"},1600});
    store.execute(TrimAudioClip{Id{"right"},1616,2080,source->frames()});
    caches = session->synchronize(*store.state().project,{{"source",1,source}});
    for (const auto& c : caches) compare(c,*source,gain);
    CHECK(store.undo() && store.undo() && store.undo());
    caches = session->synchronize(*store.state().project,{{"source",1,source}}); compare(caches.front(),*source,gain);
    CHECK(caches.size() == 1 && caches.front().clip == p.clips.front());
    store.execute(RemoveAudioClip{Id{"clip"}});
    CHECK(session->synchronize(*store.state().project,{}).empty());
    CHECK(session->model_stats().regions == 0 && session->model_stats().sources == 1);
    CHECK(session->model_stats().readers == 0);
    CHECK(store.undo());
    caches = session->synchronize(*store.state().project,{{"source",1,source}}); compare(caches.front(),*source,gain);
    rejects([&] { session->synchronize(p,{{"source",2,source}}); });
    auto mismatch = pcm(channels); mismatch->sample_rate = 44100;
    rejects([&] { session->synchronize(p,{{"different",1,mismatch}}); });
    auto cancel = std::make_shared<std::atomic<bool>>(true);
    rejects([&] { session->synchronize(p,{{"source",1,source}},cancel); });
    CHECK(source->samples == original && session->model_stats().read_errors == 0);
    auto multi = p; auto second = pcm(channels); second->samples[0] = .375f;
    for (int i = 0; i < 16; ++i) {
        auto c = p.clips.front(); c.id = {"extra-"+std::to_string(i)}; c.start = 4096+i*2048;
        c.length = 256; c.source_offset = i; c.source = i%2 ? "source" : "second"; multi.clips.push_back(c);
    }
    auto many = session->synchronize(multi,{{"source",1,source},{"second",3,second}});
    CHECK(many.size() == 17 && session->model_stats().sources == 2 && session->model_stats().modifications == 2);
    for (const auto& c : many) compare(c,c.clip.source == "source" ? *source : *second,gain);
    auto rate_project = p; rate_project.sample_rate = 96000;
    auto rate_source = pcm(channels); rate_source->sample_rate = 96000;
    auto rate_session = ara::Session::open(path,id);
    const auto at96 = rate_session->synchronize(rate_project,{{"source",1,rate_source}}); compare(at96.front(),*rate_source,gain);
    ara::Arrangement arrangement; arrangement.attach("source",path,id);
    arrangement.synchronize(p,[&](const auto&) { return source; });
    CHECK(arrangement.status("source").starts_with("ready"));
    engine_test(p,source,arrangement,gain);
    auto stale = p; ++stale.clips.front().source_offset;
    auto stale_graph = graph(stale,source); arrangement.apply(stale,stale_graph); CHECK(stale_graph.voices[0].asset == source);
    stale = p; stale.time.tempos.front().bpm = 95;
    stale_graph = graph(stale,source); arrangement.apply(stale,stale_graph); CHECK(stale_graph.voices[0].asset == source);
    arrangement.synchronize(p,[](const auto&) -> std::shared_ptr<const audio::AudioData> { throw std::runtime_error("missing source"); });
    CHECK(arrangement.caches().empty() && arrangement.status("source").starts_with("failed"));
    auto fallback = graph(p,source); arrangement.apply(p,fallback); CHECK(fallback.voices[0].asset == source);
}
struct FileFixture {
    std::filesystem::path path = std::filesystem::temp_directory_path()/("mrs-ara178-"+new_id().value+".wav");
    FileFixture() {
        std::ofstream f(path,std::ios::binary);
        auto u16 = [&](std::uint16_t n) { f.put(static_cast<char>(n&255)); f.put(static_cast<char>(n>>8)); };
        auto u32 = [&](std::uint32_t n) { for (int i = 0; i < 4; ++i) f.put(static_cast<char>((n>>(8*i))&255)); };
        f.write("RIFF",4); u32(36+4096*4); f.write("WAVEfmt ",8); u32(16); u16(1); u16(2);
        u32(48000); u32(48000*4); u16(4); u16(16); f.write("data",4); u32(4096*4);
        for (int i = 0; i < 4096; ++i) { u16(8192); u16(static_cast<std::uint16_t>(-4096)); }
    }
    ~FileFixture() { std::error_code ec; std::filesystem::remove(path,ec); }
    std::string bytes() const { std::ifstream f(path,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
};
class ManualDevice final : public audio::IAudioDevice {
    audio::DevicePhase phase_{audio::DevicePhase::closed};
    std::shared_ptr<std::atomic<bool>> fail_start_;
public:
    explicit ManualDevice(std::shared_ptr<std::atomic<bool>> fail = {}) : fail_start_(std::move(fail)) {}
    std::vector<audio::DeviceInfo> enumerate() override { return {{0,"ARA manual",{}, {"L","R"},32,8192,128,-1}}; }
    void control_panel(int) override {}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>) override { phase_ = audio::DevicePhase::open; }
    void start() override { if (fail_start_ && fail_start_->exchange(false)) throw std::runtime_error("synthetic backend restart failure"); phase_ = audio::DevicePhase::running; }
    void stop() override { phase_ = audio::DevicePhase::stopped; }
    void close() noexcept override { phase_ = audio::DevicePhase::closed; }
    audio::DeviceStatus status() override { return {phase_,48000,0,0,0,{}}; }
};
void application_test(const std::string& path,const std::string& id,float gain) {
    FileFixture file; const auto original = file.bytes();
    auto metadata = std::make_shared<audio::WavFile>(audio::inspect_wav(file.path));
    auto streamed = std::make_shared<audio::AudioData>(); streamed->sample_rate = 48000; streamed->channels = 2; streamed->file = metadata;
    auto p = project(); auto reader_session = ara::Session::open(path,id);
    const auto cache = reader_session->synchronize(p,{{"source",7,streamed}});
    CHECK(cache.size() == 1 && std::abs(cache[0].pcm->samples[0]-.25f*gain)<.00001f);
    CHECK(std::abs(cache[0].pcm->samples[1]+.125f*gain)<.00001f);
    desktop::Application app; app.new_project(48000); app.import_wavs({file.path});
    const auto clip = app.services().projects->state().project->clips.front();
    auto fail_start = std::make_shared<std::atomic<bool>>(false);
    app.connect(std::make_unique<ManualDevice>(fail_start),{0,48000,128,{}, {0,1},1});
    std::array<float,256> output{}, dry{};
    app.services().transport->play(); app.engine()->process(nullptr,dry.data(),128);
    CHECK(dry[0] != 0.f && dry[1] != 0.f);
    app.services().transport->stop(); app.engine()->process(nullptr,output.data(),128); app.poll();
    app.attach_ara_source(clip.source,path,id);
    CHECK(app.ara_status(clip.source).starts_with("ready") && app.ara_caches().size() == 1);
    app.services().transport->play(); app.engine()->process(nullptr,output.data(),128);
    for (std::size_t i = 0; i < output.size(); ++i) CHECK(std::abs(output[i]-dry[i]*gain)<.00001f);
    rejects([&] { app.detach_ara_source(clip.source); });
    app.services().transport->stop(); app.engine()->process(nullptr,output.data(),128); app.poll();
    const auto before_bypass=*app.services().projects->state().project;
    app.bypass_ara_source(clip.source,true);CHECK(app.ara_bypassed(clip.source)&&app.ara_status(clip.source).starts_with("disabled"));
    app.services().transport->play();app.engine()->process(nullptr,output.data(),128);
    for(std::size_t i=0;i<output.size();++i)CHECK(std::abs(output[i]-dry[i])<.00001f);
    app.services().transport->stop();app.engine()->process(nullptr,output.data(),128);app.poll();
    app.bypass_ara_source(clip.source,false);CHECK(app.ara_status(clip.source).starts_with("ready")&&*app.services().projects->state().project==before_bypass);
    app.move_clips({clip.id},128);
    CHECK(app.ara_caches().front().clip.start == 128);
    CHECK(app.undo() && app.ara_caches().front().clip.start == 0);
    CHECK(app.redo() && app.ara_caches().front().clip.start == 128);
    app.split_clips({clip.id},1024); CHECK(app.ara_caches().size() == 2);
    const auto copy = app.duplicate_clip(clip.id); CHECK(app.ara_caches().size() == 3);
    app.remove_clip(copy); CHECK(app.ara_caches().size() == 2);
    CHECK(app.undo() && app.ara_caches().size() == 3);
    const bool live=app.ara_status(clip.source).find("live ARA playback")!=std::string::npos;
    for (const auto& c : app.ara_caches()) CHECK(c.clip.source == clip.source && (live||std::abs(c.pcm->samples[0]-.25f*gain)<.00001f));
    if(live){app.services().transport->seek(0);app.services().transport->play();app.engine()->process(nullptr,output.data(),128);for(std::size_t i=0;i<output.size();++i){const auto frame=static_cast<Sample>(i/2);const auto& clips=app.services().projects->state().project->clips;const auto voices=std::count_if(clips.begin(),clips.end(),[&](const auto& c){return !c.midi&&frame>=c.start&&frame<c.start+c.length;});CHECK(std::abs(output[i]-dry[i]*gain*voices)<.00001f);}app.services().transport->stop();app.engine()->process(nullptr,output.data(),128);app.poll();}
    const auto previous = app.ara_caches().front().pcm; app.refresh_ara();
    CHECK(live?app.ara_caches().front().pcm==previous:app.ara_caches().front().pcm!=previous);
    const auto reusable=app.ara_caches().front().pcm;CHECK(!app.ara_pending_revision());app.refresh_ara(false);CHECK(app.ara_caches().front().pcm==reusable);
    const auto preparationCancel=std::make_shared<std::atomic<bool>>(true);app.set_ara_preparation(preparationCancel,{});rejects([&]{app.refresh_ara();});CHECK(app.ara_caches().front().pcm==reusable);CHECK(app.engine()->state().playback!=PlaybackState::playing);app.set_ara_preparation({},{});app.refresh_ara();
    const auto before_timing = *app.services().projects->state().project;
    const auto cache_before_timing = app.ara_caches().front().pcm;
    app.set_tempos(before_timing.time,{{0,90}});
    CHECK(live||app.ara_caches().front().pcm != cache_before_timing);
    if (gain == .5f) CHECK(std::abs(app.ara_caches().front().pcm->samples[0]-.25f*.375f)<.00001f);
    CHECK(app.undo() && *app.services().projects->state().project == before_timing);
    fail_start->store(true);
    rejects([&] { app.set_tempos(before_timing.time,{{0,95}}); });
    CHECK(*app.services().projects->state().project == before_timing);
    CHECK(app.ara_status(clip.source).starts_with("ready"));
    CHECK(live||std::abs(app.ara_caches().front().pcm->samples[0]-.25f*gain)<.00001f);
    app.detach_ara_source(clip.source); CHECK(app.ara_status(clip.source) == "unbound" && app.ara_caches().empty());
    app.attach_ara_source(clip.source,path,id); app.new_project(48000); CHECK(app.ara_caches().empty());
    CHECK(file.bytes() == original);
}
void cancellation_test(const std::string& path,const std::string& id) {
    auto source = pcm(1); source->samples.resize(65536,.125f);
    auto p = project(); p.clips[0].length = 60000;
    auto session = ara::Session::open(path,id);
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",L"1");
    std::jthread worker([&] { std::this_thread::sleep_for(std::chrono::milliseconds(20)); cancel->store(true); });
    bool rejected{};
    try { session->synchronize(p,{{"source",1,source}},cancel); } catch (const std::exception&) { rejected = true; }
    worker.join(); SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",nullptr);
    CHECK(rejected);
    cancel->store(false);unsigned ticks{};const auto control=GetCurrentThreadId();
    session->set_preparation(cancel,[&]{CHECK(GetCurrentThreadId()==control);++ticks;cancel->store(true);});
    SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",L"1");rejected=false;
    try {session->synchronize(p,{{"source",1,source}});}catch(const std::exception&){rejected=true;}
    SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",nullptr);session->set_preparation({},{});CHECK(rejected&&ticks>0);
    const auto recovered = session->synchronize(p,{{"source",1,source}});
    CHECK(recovered.size() == 1); compare(recovered.front(),*source,.5f);
}
void shared_document_test(const std::string& path,const std::string& id,float gain){
    auto p=project();auto secondTrack=p.tracks.front();secondTrack.id=new_id();secondTrack.name="Dub";p.tracks.push_back(secondTrack);p.tracks.front().name="Guide";
    auto c=p.clips.front();c.id=new_id();c.track=secondTrack.id;c.source="second";c.name="Dub clip";p.clips.push_back(c);
    auto first=pcm(1),second=pcm(2);auto resolve=[&](const std::string& source){return source=="source"?first:second;};
    ara::Arrangement shared;shared.attach("source",path,id);shared.synchronize(p,resolve);shared.attach("second",path,id);shared.synchronize(p,resolve);
    const auto group=shared.binding("source").document_id;CHECK(!group.empty()&&shared.binding("second").document_id==group);
    CHECK(shared.model_stats("source").sources==2&&shared.model_stats("second").sources==2&&shared.model_stats("source").regions==2&&shared.model_stats("source").sequences==2);
    {auto previewGraph=graph(p,first);previewGraph.voices[1].asset=second;shared.apply(p,previewGraph,true);CHECK(previewGraph.editor_renderers.size()==1);}
    auto checkCaches=[&](ara::Arrangement& a){const auto caches=a.caches();CHECK(caches.size()==2);for(const auto& cache:caches)compare(cache,*resolve(cache.clip.source),gain);};checkCaches(shared);
    shared.bypass("source",true);CHECK(shared.bypassed("source")&&!shared.bypassed("second"));shared.synchronize(p,resolve);CHECK(shared.model_stats("second").sources==2);shared.bypass("source",false);
    auto saved=shared.save_bindings();p.ara_bindings=saved;CHECK(deserialize(serialize(p))==p);
    ara::Arrangement reopened;reopened.restore_bindings(saved);reopened.synchronize(p,resolve);CHECK(reopened.model_stats("source").sources==2);checkCaches(reopened);
    auto one=saved;std::erase_if(one,[](const auto& b){return b.source=="source";});reopened.restore_bindings(one);reopened.synchronize(p,resolve);CHECK(reopened.caches().size()==1&&reopened.model_stats("second").regions==1);
    reopened.restore_bindings(saved);reopened.synchronize(p,resolve);checkCaches(reopened);
    reopened.restore_bindings({});reopened.synchronize(p,resolve);CHECK(reopened.caches().empty());
    reopened.restore_bindings(one);reopened.synchronize(p,resolve);CHECK(reopened.caches().size()==1);
    reopened.restore_bindings(saved);reopened.synchronize(p,resolve);checkCaches(reopened);
    auto corrupt=saved;corrupt.front().archive.front()^=std::byte{1};ara::Arrangement fault;fault.restore_bindings(corrupt);fault.synchronize(p,resolve);
    CHECK(fault.status(corrupt.front().source).starts_with("unavailable")&&fault.status(corrupt.back().source).starts_with("ready"));
    fault.retry(corrupt.front().source);CHECK(fault.status(corrupt.back().source).starts_with("ready")&&fault.caches().size()==1);
    const auto preserved=fault.save_bindings();CHECK(preserved.front().archive==corrupt.front().archive);fault.restore_bindings({});
    auto missing=saved;for(auto& b:missing)b.module+=".missing";ara::Arrangement absent;absent.restore_bindings(missing);absent.synchronize(p,resolve);CHECK(absent.save_bindings()==missing);
    auto legacy=saved;for(auto& b:legacy)b.document_id.clear();ara::Arrangement isolated;isolated.restore_bindings(legacy);isolated.synchronize(p,resolve);CHECK(isolated.model_stats("source").sources==1&&isolated.model_stats("second").sources==1);
}
void timing_context_test(const std::string& path,const std::string& id) {
    auto p = project(); p.clips.front().length = 60000;
    p.time.tempos = {{0,120},{4,90}}; p.time.meters = {{1,1,4},{2,3,4}};
    auto source = pcm(1); source->samples.resize(65536,.125f);
    auto session = ara::Session::open(path,id);
    const auto cache = session->synchronize(p,{{"source",1,source}}).front();
    for (Sample f = 0; f < cache.clip.length; ++f) {
        const auto position = cache.clip.start+f;
        const float tempo_gain = position < 100 ? .5f : .375f;
        const float meter_gain = position < 31967 ? .25f : .75f;
        const auto expected = source->samples[static_cast<std::size_t>(f+cache.clip.source_offset)]*tempo_gain*meter_gain;
        CHECK(std::abs(cache.pcm->samples[static_cast<std::size_t>(f)]-expected)<.00001f);
    }
}
void incremental_test(const std::string& path,const std::string& id) {
    auto p=project();p.clips.front().length=400000;
    auto sibling=p.clips.front();sibling.id={"sibling"};sibling.start=600000;sibling.source_offset=450000;sibling.length=20000;p.clips.push_back(sibling);
    auto other=sibling;other.id={"neighbour"};other.source="second";other.source_offset=0;p.clips.push_back(other);
    p.time.tempos={{0,120},{4,90}};p.time.meters={{1,1,4},{2,3,4}};p.validate();
    auto a=pcm(2),b=pcm(1);a->samples.resize(500000*2,.2f);b->samples.resize(500000,.125f);
    auto session=ara::Session::open(path,id);AraBinding binding;binding.source="source";binding.persistent_id="increment-a";session->configure_source(binding);binding.source="second";binding.persistent_id="increment-b";session->configure_source(binding);
    const std::vector<ara::SourceSnapshot> sources{{"source",1,a},{"second",1,b}};
    auto baseline=session->synchronize(p,sources);CHECK(session->preparation_stats().full==3);CHECK(session->preparation_stats().causes&ara::preparation_model);
    using Patch=bool(*)(const char*,double,double,float,int);const auto patch=reinterpret_cast<Patch>(GetProcAddress(GetModuleHandleW(std::filesystem::path(path).c_str()),"MRSAraPatch"));CHECK(patch);
    using Settings=bool(*)(std::uint32_t,std::uint32_t);const auto settings=reinterpret_cast<Settings>(GetProcAddress(GetModuleHandleW(std::filesystem::path(path).c_str()),"MRSAraRenderSettings"));CHECK(settings);
    struct Reset{Patch patch;Settings settings;~Reset(){patch("",0,0,1,-1);settings(17,0);}} reset{patch,settings};
    const auto find=[](const auto& caches,const char* name)->const ara::ClipCache&{const auto at=std::find_if(caches.begin(),caches.end(),[&](const auto& c){return c.clip.id.value==name;});CHECK(at!=caches.end());return *at;};
    const auto equal=[&](const auto& left,const auto& right){CHECK(left.size()==right.size());for(const auto& cache:left){const auto& expected=find(right,cache.clip.id.value.c_str());CHECK(cache.pcm->samples.size()==expected.pcm->samples.size());for(std::size_t n=0;n<cache.pcm->samples.size();++n)CHECK(std::abs(cache.pcm->samples[n]-expected.pcm->samples[n])<.000001f);}};
    const double first=100000./48000.,last=100120./48000.;
    CHECK(patch("increment-a",first,last,2.f,0));auto updated=session->synchronize(p,sources);auto stats=session->preparation_stats();CHECK(stats.partial==1&&stats.reused==2&&stats.full==0&&stats.rendered_frames<2000);
    CHECK(find(updated,"sibling").pcm==find(baseline,"sibling").pcm&&find(updated,"neighbour").pcm==find(baseline,"neighbour").pcm);
    CHECK(std::abs(find(updated,"clip").pcm->samples[(100010-127)*2]-find(baseline,"clip").pcm->samples[(100010-127)*2]*2.f)<.000001f);
    CHECK(stats.causes==0);
    session->invalidate();auto full=session->synchronize(p,sources);equal(updated,full);CHECK(session->preparation_stats().causes&ara::preparation_explicit);
    CHECK(patch("increment-a",first,last,3.f,1));updated=session->synchronize(p,sources);CHECK(session->preparation_stats().partial==1&&session->preparation_stats().reused==2);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);
    CHECK(patch("increment-a",first,last,4.f,2));updated=session->synchronize(p,sources);CHECK(session->preparation_stats().full==2&&session->preparation_stats().reused==1);CHECK(session->preparation_stats().causes&ara::preparation_unknown_range);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);
    CHECK(patch("increment-a",first,last,5.f,0));const auto cancel=std::make_shared<std::atomic<bool>>(true);rejects([&]{session->synchronize(p,sources,cancel);});cancel->store(false);updated=session->synchronize(p,sources,cancel);CHECK(session->preparation_stats().partial==1);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);
    // A range outside all occurrences must not process or replace their PCM.
    CHECK(patch("increment-a",first,last,1.f,0));session->synchronize(p,sources);CHECK(patch("increment-a",9.,9.1,1.f,0));updated=session->synchronize(p,sources);CHECK(session->preparation_stats().reused==3&&session->preparation_stats().rendered_frames==0);
    for(const auto boundary:{127.,400110.}){CHECK(patch("increment-a",boundary/48000.,(boundary+10)/48000.,2.f,0));updated=session->synchronize(p,sources);CHECK(session->preparation_stats().partial==1);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);CHECK(patch("increment-a",boundary/48000.,(boundary+10)/48000.,1.f,0));session->synchronize(p,sources);}
    CHECK(settings(31,0)&&patch("increment-a",first,last,2.f,0));updated=session->synchronize(p,sources);CHECK(session->preparation_stats().full==1&&find(updated,"clip").compensated_latency==31);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);
    CHECK(settings(31,(std::numeric_limits<std::uint32_t>::max)())&&patch("increment-a",first,last,3.f,0));updated=session->synchronize(p,sources);CHECK(session->preparation_stats().partial==0&&session->preparation_stats().full==1);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);
    CHECK(settings(17,0)&&patch("increment-a",first,last,1.f,0));session->synchronize(p,sources);session->invalidate();session->synchronize(p,sources);
    bool offThread{};std::jthread notifier([&]{offThread=patch("increment-a",first,last,2.f,0);});notifier.join();CHECK(offThread);updated=session->synchronize(p,sources);CHECK(session->preparation_stats().full==3);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);
    CHECK(patch("increment-a",std::numeric_limits<double>::quiet_NaN(),1.,1.f,0));updated=session->synchronize(p,sources);CHECK(session->preparation_stats().full==2&&session->preparation_stats().reused==1);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);
    CHECK(patch("increment-b",10000./48000.,10120./48000.,2.f,0));updated=session->synchronize(p,sources);CHECK(session->preparation_stats().partial==1&&session->preparation_stats().reused==2);session->invalidate();full=session->synchronize(p,sources);equal(updated,full);
    // Model/timing changes have cross-region implications: conservative full rebuild.
    p.clips.front().start+=128;session->synchronize(p,sources);CHECK(session->preparation_stats().full==3);
    std::cout<<"ARA incremental: 120-frame edit rendered "<<stats.rendered_frames<<" frames vs 440051 full; PCM equivalent\n";
}
void live_test(const std::string& path,const std::string& id){
    auto keepAlive=ara::Session::open(path,id);
    const auto module=GetModuleHandleW(std::filesystem::path(path).c_str());
    using Settings=bool(*)(std::uint32_t,std::uint32_t);using Patch=bool(*)(const char*,double,double,float,int);
    const auto settings=reinterpret_cast<Settings>(GetProcAddress(module,"MRSAraRenderSettings"));const auto patch=reinterpret_cast<Patch>(GetProcAddress(module,"MRSAraPatch"));CHECK(settings&&patch);
    using Fault=void(*)(bool);const auto fault=reinterpret_cast<Fault>(GetProcAddress(module,"MRSAraLiveFault"));CHECK(fault);
    using Restart=void(*)();const auto restart=reinterpret_cast<Restart>(GetProcAddress(module,"MRSAraLatencyChanged"));CHECK(restart);
    CHECK(settings(0,0));
    struct Reset{Settings settings;Patch patch;~Reset(){patch("",0,0,1,-1);settings(17,0);}} reset{settings,patch};
    for(auto channels:{1U,2U}){
        auto p=project();auto source=pcm(channels);ara::Arrangement arrangement;arrangement.prefer_live(true);arrangement.attach("source",path,id);arrangement.synchronize(p,[&](const auto&){return source;});
        CHECK(arrangement.status("source").find("live ARA playback")!=std::string::npos);CHECK(arrangement.status("source").find("frames=0")!=std::string::npos);
        auto prepared=graph(p,source);arrangement.apply(p,prepared);auto engine=std::make_shared<audio::AudioEngine>();engine->prepare({48000,0,channels,512,128},prepared);
        std::vector<float> output(128*channels);CHECK(engine->enqueue({audio::ControlKind::play}));allocations=frees=0;probe_alloc=true;engine->process(nullptr,output.data(),128);probe_alloc=false;CHECK(allocations==0&&frees==0);
        CHECK(engine->enqueue({audio::ControlKind::stop}));engine->process(nullptr,output.data(),128);
        engine_test(p,source,arrangement,.5f);
        // NULL range edits change the shared document, heard by the SAME
        // prepared renderer. There is no synchronize/full PCM render here.
        const auto identity=arrangement.caches().front().pcm;const auto persistent=arrangement.binding("source").persistent_id;
        CHECK(patch(persistent.c_str(),0.,1.,2.f,2));CHECK(arrangement.poll());CHECK(!arrangement.pending_revision());CHECK(arrangement.caches().front().pcm==identity);
        engine_test(p,source,arrangement,1.f);
        fault(true);CHECK(engine->enqueue({audio::ControlKind::play}));engine->process(nullptr,output.data(),128);CHECK(engine->metrics().processing_fault);CHECK(arrangement.status("source").starts_with("failed"));engine->process(nullptr,output.data(),128);CHECK(std::all_of(output.begin(),output.end(),[](float v){return v==0.f;}));fault(false);
        CHECK(engine->enqueue({audio::ControlKind::stop}));
        CHECK(patch(persistent.c_str(),0.,1.,1.f,2));arrangement.poll();
        CHECK(settings(17,0));restart();arrangement.synchronize(p,[&](const auto&){return source;});CHECK(arrangement.status("source").find("prepared PCM cache")!=std::string::npos);CHECK(arrangement.caches().front().compensated_latency==17);compare(arrangement.caches().front(),*source,.5f);engine_test(p,source,arrangement,.5f);
        CHECK(settings(0,0));restart();arrangement.synchronize(p,[&](const auto&){return source;});CHECK(arrangement.status("source").find("live ARA playback")!=std::string::npos);engine_test(p,source,arrangement,.5f);
        arrangement.prefer_live(false);arrangement.synchronize(p,[&](const auto&){return source;});CHECK(arrangement.status("source").find("prepared PCM cache")!=std::string::npos);compare(arrangement.caches().front(),*source,.5f);
    }
    std::cout<<"ARA live: mono/stereo NULL-range edits, seek/loop/variable blocks, PCM fallback PASS\n";
}
}
void editor_audition_test(const std::string& path,const std::string& classId) {
    using Audition=void(*)(float);
    for(const auto rate:{44100U,48000U,96000U}){
        auto p=project();p.sample_rate=rate;auto source=pcm(2);source->sample_rate=rate;
        ara::Arrangement arrangement;arrangement.prefer_live(true);arrangement.attach("source",path,classId);arrangement.synchronize(p,[&](const auto&){return source;});
        const auto module=GetModuleHandleW(std::filesystem::path(path).c_str());const auto audition=reinterpret_cast<Audition>(GetProcAddress(module,"MRSAraAudition"));CHECK(audition);
        struct Reset{Audition audition;~Reset(){audition(0.f);}} reset{audition};
        const auto parent=CreateWindowExW(0,L"STATIC",L"ARA audition test",WS_POPUP,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(parent);
        struct Window{HWND parent;~Window(){DestroyWindow(parent);}} window{parent};
        int width{},height{};CHECK(arrangement.open_editor("source",p.clips.front().id,parent,width,height));
        for(int n=0;n<1000&&arrangement.status("source").starts_with("analysing");++n){arrangement.poll();Sleep(1);}
        if(arrangement.pending_revision())arrangement.synchronize(p,[&](const auto&){return source;});
        auto offline=graph(p,source);arrangement.apply(p,offline);CHECK(offline.editor_renderers.empty());
        auto g=graph(p,source);arrangement.apply(p,g,true);CHECK(g.editor_renderers.size()==1);g.master_gain=.5f;
        auto engine=std::make_shared<audio::AudioEngine>();engine->prepare({rate,0,2,8192,128},g);CHECK(!engine->anticipation_safe()&&engine->processing_domains().master.owner==audio::ProcessingDomains::Owner::device);arrangement.transport(p,engine->state());
        std::array<float,16384> output{};audition(.125f);
        const auto& preview=g.editor_renderers.front();
        CHECK(preview->mix(output.data(),128,2,0,false,120.,0.,{4,4,0.},{},false));
        for(std::size_t i=0;i<256;++i)CHECK(output[i]==0.f); // recording suppresses preview, processor still advances
        allocations=frees=0;std::uint64_t maxNs{};probe_alloc=true;
        for(int block=0;block<1000;++block){const auto start=std::chrono::steady_clock::now();engine->process(nullptr,output.data(),128);maxNs=(std::max)(maxNs,static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count()));}
        probe_alloc=false;CHECK(allocations==0&&frees==0&&!engine->metrics().processing_fault);
        for(std::size_t i=0;i<256;++i)CHECK(std::abs(output[i]-.0625f)<.00001f);
        for(const auto frames:{1U,17U,256U,1024U,8192U}){engine->process(nullptr,output.data(),frames);for(std::size_t i=0;i<frames*2;++i)CHECK(std::abs(output[i]-.0625f)<.00001f);}
        CHECK(engine->enqueue({audio::ControlKind::pause}));engine->process(nullptr,output.data(),128);CHECK(std::abs(output[0]-.0625f)<.00001f);
        audition(0.f);engine->process(nullptr,output.data(),128);for(std::size_t i=0;i<256;++i)CHECK(output[i]==0.f);
        // No document or provider may be reclaimed by a stale graph callback.
        audition(.125f);arrangement.close_editor("source");allocations=frees=0;probe_alloc=true;engine->process(nullptr,output.data(),128);probe_alloc=false;CHECK(allocations==0&&frees==0);
        for(std::size_t i=0;i<256;++i)CHECK(output[i]==0.f);
        CHECK(arrangement.open_editor("source",p.clips.front().id,parent,width,height));arrangement.transport(p,engine->state());engine->process(nullptr,output.data(),128);CHECK(std::abs(output[0]-.0625f)<.00001f);
        if(rate==48000){
            // A prerecorded channel may run ahead, while preview/Master remain
            // on the device owner. The speculative clone must not touch preview.
            auto mixedGraph=g;mixedGraph.mixer.resize(1);auto ordinary=mixedGraph.voices.front();ordinary.renderer.reset();ordinary.mixer_track=0;mixedGraph.voices.push_back(ordinary);
            engine->prepare({rate,0,2,8192,128,2},mixedGraph);audio::AheadRenderer ahead(engine,128,512);ahead.start();CHECK(ahead.active());
            ahead.process(nullptr,output.data(),128);CHECK(!engine->metrics().processing_fault);
            for(std::size_t i=0;i<256;++i)CHECK(std::abs(output[i]-.0625f)<.00001f);ahead.stop();
            const auto destination=std::filesystem::temp_directory_path()/("mrs-ara-preview-capture-"+new_id().value+".wav");
            struct Remove{std::filesystem::path path;~Remove(){std::error_code error;std::filesystem::remove(path,error);}} remove{destination};
            auto recorder=std::make_shared<audio::Recorder>(destination,rate,0);
            auto recordingGraph=g;recordingGraph.voices.clear();recordingGraph.recordings.push_back(recorder);engine->prepare({rate,1,2,8192,128},recordingGraph);
            std::array<float,128> input{};input.fill(.25f);CHECK(engine->enqueue({audio::ControlKind::play}));engine->process(input.data(),output.data(),128);
            for(std::size_t i=0;i<256;++i)CHECK(output[i]==0.f);
            const auto recorded=recorder->finish();CHECK(recorded.frames==128&&recorded.status.fault==audio::RecordFault::none);
            const auto raw=audio::load_wav(destination);for(const auto value:raw.samples)CHECK(std::abs(value-.25f)<.00001f);
            engine->prepare({rate,0,2,8192,128},g);
        }
        arrangement.close_editors();audition(0.f);
        std::cout<<"ARA audition "<<rate<<" 1000x128 max="<<maxNs<<"ns, misses="<<engine->metrics().deadline_misses<<", host alloc/free=0\n";
    }
}
int main(int argc,char** argv) {
    if (argc < 2) return 2;
    try {
        const std::string path = argv[1]; const float gain = argc > 2 ? std::stof(argv[2]) : 1.f;
        const auto info = ara::probe(path).front(); CHECK(info.status == ara::CapabilityStatus::available);
        model_test(path,info.class_id,gain,1); model_test(path,info.class_id,gain,2);
        application_test(path,info.class_id,gain);
        shared_document_test(path,info.class_id,gain);
        if (gain == .5f) { cancellation_test(path,info.class_id); timing_context_test(path,info.class_id);incremental_test(path,info.class_id);live_test(path,info.class_id);editor_audition_test(path,info.class_id); }
        std::cout << "ARA clip/model/cache/engine PASS\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
