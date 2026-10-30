#define NOMINMAX
#include <mrs/desktop.hpp>
#include <mrs/vst3.hpp>
#include <windows.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <new>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
namespace {std::atomic<bool> probing{};std::atomic<std::size_t> allocations{};}
void* operator new(std::size_t size){if(probing)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc{};}
void* operator new[](std::size_t size){return ::operator new(size);}void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
using namespace mrs;
using namespace mrs::processing;
namespace {
void check(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
#define CHECK(...) check((__VA_ARGS__),#__VA_ARGS__)
template<class F> void rejects(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}CHECK(threw);}
std::string utf8(const std::filesystem::path& p){auto s=p.u8string();return {s.begin(),s.end()};}
GraphSnapshot snapshot(const NodeState& node){GraphState s;s.id={"test-graph"};s.nodes={node};s.edges={{std::nullopt,node.id,1}};s.outputs={node.id};return {std::make_shared<const GraphState>(s),0,false,false};}
struct Directory{std::filesystem::path path=std::filesystem::temp_directory_path()/L"mrs-vst3-tests"/std::to_wstring(GetCurrentProcessId());Directory(){std::filesystem::create_directories(path);}~Directory(){std::error_code ec;std::filesystem::remove_all(path,ec);}};
class ManualDevice final:public audio::IAudioDevice{
    audio::DevicePhase phase_=audio::DevicePhase::closed;
public:
    unsigned stops{},starts{},opens{};
    std::vector<audio::DeviceInfo> enumerate() override{return {{0,"VST manual",{"L","R"},{"L","R"},32,2048,128,-1}};}
    void control_panel(int) override{}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>) override{++opens;phase_=audio::DevicePhase::open;}
    void start() override{++starts;phase_=audio::DevicePhase::running;}
    void stop() override{++stops;phase_=audio::DevicePhase::stopped;}
    void close() noexcept override{phase_=audio::DevicePhase::closed;}
    audio::DeviceStatus status() override{return {phase_,48000,0,0,0,{}};}
};
void processing_test(const NodeState& node){PreparedGraph graph{snapshot(node),{48000,2,64},hosted_factory};std::array<float,128> audio;audio.fill(1);graph.process(audio.data(),64);for(auto v:audio)CHECK(v==0.5f);CHECK(graph.latency().output==7&&!graph.latency().live_safe);
    CHECK(graph.enqueue_parameter(node.id,{0,100,0.25f}));audio.fill(1);graph.process(audio.data(),64);for(auto v:audio)CHECK(v==0.25f);
    audio.fill(0);for(std::size_t i=0;i<audio.size();i+=2)audio[i]=1;graph.process(audio.data(),64);CHECK(audio[0]==0.25f&&audio[1]==0);
    auto bypass=node;bypass.bypass=true;PreparedGraph dry{snapshot(bypass),{48000,2,64},hosted_factory};audio.fill(0.75f);dry.process(audio.data(),64);CHECK(dry.latency().output==7);for(std::size_t i=0;i<audio.size();++i)CHECK(audio[i]==(i<14?0.f:0.75f));
    rejects([&]{auto bad=node;bad.plugin.class_id="00000000000000000000000000000000";PreparedGraph fail{snapshot(bad),{48000,2,64},hosted_factory};});
}
void realtime_test(const NodeState& node){PreparedGraph graph{snapshot(node),{48000,2,128},hosted_factory};std::array<float,256> audio;CHECK(graph.enqueue_parameter(node.id,{0,100,0.25f}));allocations=0;probing=true;for(int n=0;n<128;++n){audio.fill(0.1f);graph.process(audio.data(),128);}probing=false;CHECK(allocations==0 && !graph.failed());}
void large_state_test(const std::filesystem::path& module,const NodeState& node){
    Directory directory;const auto large=directory.path/L"large-state.vst3";
    std::filesystem::copy_file(module,large,std::filesystem::copy_options::overwrite_existing);
    auto largeNode=node;largeNode.processor_id=utf8(large);
    PreparedGraph graph{snapshot(largeNode),{48000,2,64},hosted_factory};
    const auto saved=graph.capture();
    CHECK(saved.nodes.front().plugin.component.size()==2u*1024u*1024u);
    CHECK(saved.nodes.front().plugin.controller.size()==2u*1024u*1024u);
    PreparedGraph restored{{std::make_shared<const GraphState>(saved),0,false,false},{48000,2,64},hosted_factory};
    const auto recaptured=restored.capture();
    CHECK(recaptured.nodes.front().plugin==saved.nodes.front().plugin);
}
void state(const NodeState& node){PreparedGraph graph{snapshot(node),{48000,2,64},hosted_factory};std::array<float,128> audio{};CHECK(graph.enqueue_parameter(node.id,{0,100,0.3f}));graph.process(audio.data(),64);CHECK(graph.enqueue_parameter(node.id,{0,100,0.4f}));auto saved=graph.capture();CHECK(!saved.nodes.front().plugin.component.empty());PreparedGraph restored{{std::make_shared<const GraphState>(saved),0,false,false},{48000,2,64},hosted_factory};audio.fill(1);restored.process(audio.data(),64);for(auto v:audio)CHECK(std::abs(v-0.4f)<0.000001f);
    auto project=demo_project();NativeInsert fx;fx.id=node.id;fx.kind=InsertKind::vst3;fx.plugin_path=node.processor_id;fx.class_id=node.plugin.class_id;fx.component_state=saved.nodes.front().plugin.component;fx.controller_state=saved.nodes.front().plugin.controller;fx.parameters={{100,0.3f}};project.tracks.front().inserts={fx};CHECK(deserialize(serialize(project))==project);
    Directory dir;auto legacy=node;auto copy=dir.path/L"legacy-state.vst3";std::filesystem::copy_file(std::filesystem::path(std::u8string(node.processor_id.begin(),node.processor_id.end())),copy);legacy.processor_id=utf8(copy);
    PreparedGraph source{snapshot(legacy),{48000,2,64},hosted_factory};CHECK(source.enqueue_parameter(node.id,{0,100,0.625f}));audio.fill(1);source.process(audio.data(),64);auto old=source.capture();
    for(const auto& info:source.parameter_infos(node.id))if(info.automatable)old.nodes.front().parameters.push_back({info.id,info.id==100?0.5f:0.f});
    CHECK(old.nodes.front().parameters.size()>256);
    PreparedGraph migrated{{std::make_shared<const GraphState>(old),0,false,false},{48000,2,64},hosted_factory};audio.fill(1);migrated.process(audio.data(),64);CHECK(audio[0]==0.625f);
    auto activationCopy=dir.path/L"activation-state.vst3";std::filesystem::copy_file(std::filesystem::path(std::u8string(node.processor_id.begin(),node.processor_id.end())),activationCopy);
    auto activationSaved=old;activationSaved.nodes.front().processor_id=utf8(activationCopy);activationSaved.nodes.front().parameters.clear();
    PreparedGraph activated{{std::make_shared<const GraphState>(activationSaved),0,false,false},{48000,2,64},hosted_factory};audio.fill(1);activated.process(audio.data(),64);
    for(auto v:audio)CHECK(std::abs(v-0.625f)<0.000001f); // DSP reset on activation cannot overwrite restored sound.

}
void editor(const NodeState& node){PreparedGraph graph{snapshot(node),{48000,2,64},hosted_factory};const auto parent=CreateWindowW(L"STATIC",L"test",WS_OVERLAPPED,0,0,400,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(parent!=nullptr);int w{},h{};CHECK(graph.open_editor(node.id,parent,w,h)&&w==320&&h==120);auto child=GetWindow(parent,GW_CHILD);CHECK(child!=nullptr);SendMessageW(child,WM_COMMAND,MAKEWPARAM(1,BN_CLICKED),0);CHECK(graph.consume_edits());std::array<float,128> audio;audio.fill(1);graph.process(audio.data(),64);CHECK(audio[0]==0.75f);
    // A preset changes processor/controller state without performEdit for each parameter.
    SendMessageW(child,WM_COMMAND,MAKEWPARAM(2,BN_CLICKED),0);CHECK(graph.consume_edits());
    auto saved=graph.capture();CHECK(saved.nodes.front().parameters.empty());
    PreparedGraph restored{{std::make_shared<const GraphState>(saved),0,false,false},{48000,2,64},hosted_factory};
    audio.fill(1);restored.process(audio.data(),64);CHECK(audio[0]==0.625f);
    SendMessageW(child,WM_COMMAND,MAKEWPARAM(1,BN_CLICKED),0);saved=graph.capture();PreparedGraph immediate{{std::make_shared<const GraphState>(saved),0,false,false},{48000,2,64},hosted_factory};audio.fill(1);immediate.process(audio.data(),64);CHECK(audio[0]==0.75f);
    graph.close_editors();CHECK(GetWindow(parent,GW_CHILD)==nullptr);DestroyWindow(parent);}
void scan(const std::filesystem::path& module,const std::filesystem::path& helper){Directory dir;auto folder=dir.path/L"plugins";std::filesystem::create_directories(folder);auto copy=folder/L"fixture.vst3";std::filesystem::copy_file(module,copy);{std::ofstream out(folder/L"invalid.vst3");out<<"not a DLL";}std::filesystem::copy_file(module,folder/L"crash.vst3");std::filesystem::copy_file(module,folder/L"hang.vst3");auto cache=dir.path/L"cache";auto list=scan_vst3(folder,helper,cache);CHECK(list.size()==1&&list.front().name=="MRS Test Gain");CHECK(load_vst3_cache(cache)==list);auto legacy=dir.path/L"legacy-cache";{std::ofstream out(legacy);const auto& p=list.front();out<<"MRS_VST3_CACHE 1 1\n"<<std::quoted(p.path)<<' '<<std::quoted(p.class_id)<<' '<<std::quoted(p.name)<<' '<<std::quoted(p.vendor)<<' '<<std::quoted(p.version)<<'\n';}auto loaded=load_vst3_cache(legacy);CHECK(loaded.size()==1&&!loaded.front().instrument.has_value());save_vst3_probe(legacy,loaded);CHECK(load_vst3_cache(legacy)==loaded);CHECK(list.front().instrument.has_value()&&!list.front().instrument.value());auto start=std::chrono::steady_clock::now();auto again=scan_vst3(folder,helper,cache);CHECK(again==list && std::chrono::steady_clock::now()-start<std::chrono::seconds(2));
    // Existing installations may still have a v1 catalog and a current
    // fingerprint index: a normal rescan must upgrade class metadata without
    // requiring users to delete caches or their saved plugin descriptors.
    {std::ofstream out(cache,std::ios::trunc);const auto& p=list.front();out<<"MRS_VST3_CACHE 1 1\n"<<std::quoted(p.path)<<' '<<std::quoted(p.class_id)<<' '<<std::quoted(p.name)<<' '<<std::quoted(p.vendor)<<' '<<std::quoted(p.version)<<'\n';}
    const auto refreshed=scan_vst3(folder,helper,cache);
    CHECK(refreshed==list&&refreshed.front().instrument.has_value()&&!refreshed.front().instrument.value());
    // The second read must retain the fast unchanged-plugin scan path.
    start=std::chrono::steady_clock::now();CHECK(scan_vst3(folder,helper,cache)==list&&std::chrono::steady_clock::now()-start<std::chrono::seconds(2));
    std::filesystem::remove(copy);CHECK(scan_vst3(folder,helper,cache).empty());rejects([&]{scan_vst3(folder,dir.path/L"missing.exe",cache);});}
void desktop_test(const NodeState& node){Directory dir;desktop::Application app;const auto id=app.services().projects->state().project->tracks.front().id;NativeInsert fx;fx.id={"vst-slot"};fx.kind=InsertKind::vst3;fx.class_id=node.plugin.class_id;fx.plugin_path=node.processor_id;fx.plugin_name="MRS Test Gain";app.set_inserts(id,{fx});auto device=std::make_unique<ManualDevice>();auto* driver=device.get();app.connect(std::move(device),{0,48000,128,{}, {0,1}});const auto stops=driver->stops;app.play();std::array<float,256> output{};app.engine()->process(nullptr,output.data(),128);CHECK(app.engine()->state().playback==PlaybackState::playing);app.set_plugin_parameter(id,fx.id,100,0.25f);app.engine()->process(nullptr,output.data(),128);CHECK(driver->stops==stops&&driver->opens==1);CHECK(app.undo());app.engine()->process(nullptr,output.data(),128);CHECK(driver->stops==stops);app.pause();app.engine()->process(nullptr,output.data(),128);auto parent=CreateWindowW(L"STATIC",L"editor",WS_OVERLAPPED,0,0,400,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);int w{},h{};CHECK(app.open_plugin_editor(id,fx.id,parent,w,h));SendMessageW(GetWindow(parent,GW_CHILD),WM_COMMAND,MAKEWPARAM(1,BN_CLICKED),0);app.engine()->process(nullptr,output.data(),128);app.close_plugin_editors();DestroyWindow(parent);
    const auto before_bad=app.services().projects->state();auto unavailable=fx;unavailable.id=new_id();unavailable.plugin_path+=".missing";const auto stops_before_bad=driver->stops;rejects([&]{app.set_inserts(id,{unavailable});});CHECK(app.services().projects->state().revision==before_bad.revision&&driver->stops>stops_before_bad&&driver->status().phase==audio::DevicePhase::running);
    auto heldModule=LoadLibraryW(std::filesystem::path(node.processor_id).c_str());CHECK(heldModule);
    const auto creations=reinterpret_cast<unsigned(*)()>(GetProcAddress(heldModule,"mrsFixtureCreations"));CHECK(creations);
    const auto captures=reinterpret_cast<unsigned(*)()>(GetProcAddress(heldModule,"mrsFixtureCaptures"));CHECK(captures);
    const auto other=app.add_audio_track("Unchanged chain proof"); // structural track creation is outside the optimized insert path
    const auto parent2=CreateWindowW(L"STATIC",L"retained",WS_OVERLAPPED,0,0,400,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);int w2{},h2{};
    CHECK(app.open_plugin_editor(id,fx.id,parent2,w2,h2));const auto child2=GetWindow(parent2,GW_CHILD);CHECK(child2);
    auto beforeCreates=creations();NativeInsert eq;eq.id=new_id();eq.kind=InsertKind::channel_eq;
    app.set_inserts(other,{eq});CHECK(creations()==beforeCreates&&IsWindow(child2)&&GetWindow(parent2,GW_CHILD)==child2);
    app.set_inserts(other,{});CHECK(creations()==beforeCreates&&IsWindow(child2));
    app.set_inserts(std::nullopt,{eq});CHECK(creations()==beforeCreates&&IsWindow(child2));
    app.set_inserts(std::nullopt,{});CHECK(creations()==beforeCreates&&IsWindow(child2));
    auto secondFx=fx;secondFx.id=new_id();app.set_inserts(other,{secondFx});CHECK(creations()==beforeCreates+1&&IsWindow(child2));
    beforeCreates=creations();const auto beforeCaptures=captures();app.set_inserts(other,{});CHECK(creations()==beforeCreates&&captures()==beforeCaptures&&IsWindow(child2));
    {
        desktop::Application transfer;const auto source=transfer.services().projects->state().project->tracks.front().id;const auto destination=transfer.add_audio_track("Copy target");
        auto original=fx;original.id=new_id();auto gain=eq;gain.id=new_id();transfer.set_inserts(source,{original,gain});transfer.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
        const auto host=CreateWindowW(L"STATIC",L"#210 retained view",WS_OVERLAPPED,0,0,400,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);int width{},height{};CHECK(transfer.open_plugin_editor(source,original.id,host,width,height));const auto child=GetWindow(host,GW_CHILD);
        SendMessageW(child,WM_COMMAND,MAKEWPARAM(2,BN_CLICKED),0);transfer.engine()->process(nullptr,output.data(),128);
        const auto count=creations();const auto generation=transfer.insert_generation();transfer.transfer_insert(source,original.id,source,2,false);
        transfer.reorder_track(source,1);CHECK(creations()==count&&transfer.insert_generation()==generation&&GetWindow(host,GW_CHILD)==child&&transfer.history_preserves_plugins(false));
        CHECK(transfer.undo()&&creations()==count&&GetWindow(host,GW_CHILD)==child);CHECK(transfer.redo()&&creations()==count&&GetWindow(host,GW_CHILD)==child);CHECK(transfer.undo());
        CHECK(creations()==count&&transfer.insert_generation()==generation&&GetWindow(host,GW_CHILD)==child);
        CHECK(transfer.undo()&&creations()==count&&GetWindow(host,GW_CHILD)==child);CHECK(transfer.redo()&&creations()==count&&GetWindow(host,GW_CHILD)==child);
        transfer.transfer_insert(source,original.id,destination,0,false);const auto copy=transfer.services().projects->state().project->tracks.back().inserts.front();
        CHECK(copy.id!=original.id&&creations()==count+1&&GetWindow(host,GW_CHILD)==child);
        CHECK(std::abs(transfer.plugin_parameters(destination,copy.id).front().initial-.625f)<1e-6f);
        transfer.set_plugin_parameter(destination,copy.id,100,.3f);transfer.engine()->process(nullptr,output.data(),128);
        CHECK(std::abs(transfer.plugin_parameters(source,original.id).front().initial-.625f)<1e-6f);
        transfer.transfer_insert(source,original.id,destination,1,true);CHECK(transfer.services().projects->state().project->tracks.front().inserts.size()==1);
        CHECK(std::abs(transfer.plugin_parameters(destination,original.id).front().initial-.625f)<1e-6f);CHECK(transfer.undo());CHECK(transfer.redo());
        transfer.save_project(dir.path/L"transfer.mrsproject");transfer.open_project(dir.path/L"transfer.mrsproject");transfer.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
        CHECK(std::abs(transfer.plugin_parameters(destination,copy.id).front().initial-.3f)<1e-6f);CHECK(std::abs(transfer.plugin_parameters(destination,original.id).front().initial-.625f)<1e-6f);
        transfer.close_plugin_editors();DestroyWindow(host);
    }
    app.close_plugin_editors();DestroyWindow(parent2);FreeLibrary(heldModule);
    auto chain=app.services().projects->state().project->tracks.front().inserts;chain.push_back({new_id(),InsertKind::gain,0.5f});app.set_inserts(id,chain);CHECK(app.plugin_parameters(id,fx.id).front().initial==0.75f);
    app.set_plugin_parameter(id,fx.id,100,0.4f);app.save_project(dir.path/L"song.mrsproject");auto saved=persistence::load_project(dir.path/L"song.mrsproject");CHECK(!saved.project.tracks.front().inserts.front().component_state.empty());app.open_project(dir.path/L"song.mrsproject");app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});CHECK(app.plugin_parameters(id,fx.id).size()==2);CHECK(std::abs(app.plugin_parameters(id,fx.id).front().initial-0.4f)<1e-6f);CHECK(app.plugin_latency(id,fx.id)==7);app.set_inserts(id,{});CHECK(app.services().projects->state().project->tracks.front().inserts.empty());}
}
int wmain(int argc,wchar_t** argv){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);try{if(argc!=4)throw std::runtime_error("suite fixture helper expected");const auto module=std::filesystem::path(argv[2]);auto plugins=probe_vst3(utf8(module));CHECK(!plugins.empty());NodeState node;node.id={"vst-node"};node.format=ProcessorFormat::vst3;node.processor_id=plugins.front().path;node.plugin.class_id=plugins.front().class_id;std::wstring suite=argv[1];if(suite==L"compat"){
    PreparedGraph graph{snapshot(node),{48000,2,128},hosted_factory};std::array<float,256> audio{};
    for(int n=0;n<32;++n){for(std::size_t i=0;i<audio.size();++i)audio[i]=0.01f*std::sin(static_cast<float>(i+n*128)*0.07f);graph.process(audio.data(),128);for(auto v:audio)CHECK(std::isfinite(v));}CHECK(!graph.failed());
    auto saved=graph.capture();CHECK(!saved.nodes.front().plugin.component.empty());CHECK(saved.nodes.front().parameters.empty());
    PreparedGraph restored{{std::make_shared<const GraphState>(saved),0,false,false},{48000,2,128},hosted_factory};audio.fill(0.01f);restored.process(audio.data(),128);CHECK(!restored.failed());for(auto v:audio)CHECK(std::isfinite(v));
    const auto original=graph.parameter_infos(node.id);auto legacy=saved;for(const auto& p:original)if(p.automatable)legacy.nodes.front().parameters.push_back({p.id,1.f-p.initial});
    if(legacy.nodes.front().parameters.size()>1){PreparedGraph migrated{{std::make_shared<const GraphState>(legacy),0,false,false},{48000,2,128},hosted_factory};const auto restored_infos=migrated.parameter_infos(node.id),reference=restored.parameter_infos(node.id);for(std::size_t i=0;i<reference.size();++i)if(reference[i].automatable)CHECK(std::abs(reference[i].initial-restored_infos[i].initial)<0.00001f);std::cout<<"Legacy complete parameter snapshot migrated: "<<legacy.nodes.front().parameters.size()<<" controls\n";}
    std::cout<<plugins.front().name<<": load/process/state restore OK; component="<<saved.nodes.front().plugin.component.size()<<" controller="<<saved.nodes.front().plugin.controller.size()<<" parameters="<<original.size()<<" automatable="<<std::count_if(original.begin(),original.end(),[](const auto& p){return p.automatable;})<<" latency="<<graph.latency().output<<"\n";
}else if(suite==L"external"){
    PreparedGraph graph{snapshot(node),{48000,2,128},hosted_factory};std::array<float,256> audio;audio.fill(0.01f);graph.process(audio.data(),128);const auto baseline=audio[0];for(auto v:audio)CHECK(std::isfinite(v));
    const auto infos=graph.parameter_infos(node.id);bool changed=false;for(const auto& p:infos)if(p.automatable && (p.name.find("Gain")!=std::string::npos || p.name.find("gain")!=std::string::npos)){CHECK(graph.enqueue_parameter(node.id,{0,p.id,std::clamp(p.initial+0.1f,0.f,1.f)}));changed=true;break;}CHECK(changed);
    for(int i=0;i<128;++i){audio.fill(0.01f);graph.process(audio.data(),128);for(auto v:audio)CHECK(std::isfinite(v));}const auto edited=audio[0];CHECK(std::abs(edited-baseline)>0.000001f);
    auto saved=graph.capture();PreparedGraph restored{{std::make_shared<const GraphState>(saved),0,false,false},{48000,2,128},hosted_factory};for(int i=0;i<128;++i){audio.fill(0.01f);restored.process(audio.data(),128);}CHECK(std::abs(audio[0]-edited)<0.00001f);
    auto parent=CreateWindowW(L"STATIC",L"external-test",WS_OVERLAPPED,0,0,1200,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);int w{},h{};const auto view=graph.open_editor(node.id,parent,w,h);if(view){CHECK(w>0&&h>0);graph.close_editors();}DestroyWindow(parent);
    std::cout<<plugins.front().name<<": processing / gain automation / state roundtrip / editor="<<view<<" latency="<<graph.latency().output<<"\n";
}else if(suite==L"probe"){CHECK(plugins.front().name=="MRS Test Gain");rejects([&]{probe_vst3(utf8(module)+".missing");});}else if(suite==L"processing")processing_test(node);else if(suite==L"realtime")realtime_test(node);else if(suite==L"state"){state(node);large_state_test(module,node);}else if(suite==L"editor")editor(node);else if(suite==L"scan")scan(module,argv[3]);else if(suite==L"desktop")desktop_test(node);else throw std::runtime_error("unknown suite");std::cout<<"VST3 passed\n";CoUninitialize();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';CoUninitialize();return 1;}}
