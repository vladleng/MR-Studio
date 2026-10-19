#include <mrs/desktop.hpp>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mrs;using namespace mrs::ai;using namespace std::chrono_literals;
#define CHECK(v) do{if(!(v))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#v);}while(false)
struct Fixture {
    std::filesystem::path root=std::filesystem::temp_directory_path()/("mrs-ai-ara181-"+new_id().value),wav=root/"source.wav";
    Fixture(){std::filesystem::create_directory(root);std::ofstream f(wav,std::ios::binary);auto u16=[&](std::uint16_t n){f.put(static_cast<char>(n&255));f.put(static_cast<char>(n>>8));};auto u32=[&](std::uint32_t n){for(int i=0;i<4;++i)f.put(static_cast<char>((n>>(8*i))&255));};f.write("RIFF",4);u32(36+4096*4);f.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);f.write("data",4);u32(4096*4);for(int i=0;i<4096;++i){u16(8192);u16(4096);}}
    ~Fixture(){if(root.parent_path()==std::filesystem::temp_directory_path()&&root.filename().string().starts_with("mrs-ai-ara181-")){std::error_code ec;std::filesystem::remove_all(root,ec);}}
};
struct Device:audio::IAudioDevice {
    bool fail{};audio::DevicePhase phase{audio::DevicePhase::closed};
    std::vector<audio::DeviceInfo> enumerate()override{return {{0,"AI fixture",{}, {"L","R"},32,8192,128,-1}};}
    void control_panel(int)override{}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>)override{phase=audio::DevicePhase::open;}
    void start()override{if(fail){fail=false;throw std::runtime_error("fixture start failure");}phase=audio::DevicePhase::running;}
    void stop()override{phase=audio::DevicePhase::stopped;}
    void close()noexcept override{phase=audio::DevicePhase::closed;}
    audio::DeviceStatus status()override{return {phase,48000,0,0,0,{}};}
};
struct Ticket {std::shared_ptr<const Snapshot> snapshot;std::uint64_t id;};
Ticket ticket(desktop::Application& app,Selection selection={}){
    app.set_ai_selection(selection);const auto key=app.ai_context(selection).value->key;Query q;q.kind=QueryKind::ara_status;auto snapshot=app.ai_snapshot(key,{q},selection);CHECK(snapshot.value);
    auto request=app.request_ai({snapshot.value->context,"fixture",2s,snapshot.value},[](const Request&,const JobControl&){return "fixture reply";});CHECK(request.error==Error::none);
    bool done{};for(int n=0;n<1000&&!done;++n){if(const auto reply=app.take_ai_result())done=reply->request_id==request.request_id;std::this_thread::sleep_for(1ms);}CHECK(done);return {snapshot.value,request.request_id};
}
std::string field(std::string_view json,std::string_view name){const auto label="\""+std::string(name)+"\":\"";const auto at=json.find(label);CHECK(at!=std::string_view::npos);const auto begin=at+label.size(),end=json.find('"',begin);CHECK(end!=std::string_view::npos);return std::string(json.substr(begin,end-begin));}
void run(const std::string& module){
    Fixture fixture;const auto capability=ara::probe(module).front();CHECK(capability.status==ara::CapabilityStatus::available);
    desktop::Application app;app.new_project(48000);app.import_wavs({fixture.wav});const auto clip=app.services().projects->state().project->clips.front();app.copy_clips({clip.id},5000);
    auto device=std::make_unique<Device>();auto* backend=device.get();app.connect(std::move(device),{0,48000,128,{}, {0,1},1});
    app.set_ai_ara_catalog({{module,capability.class_id,"ARA fixture",capability.version}});PermissionPolicy policy;policy.mode=PermissionMode::edit;app.set_ai_permissions(policy);
    Query caps;caps.kind=QueryKind::ara_capabilities;const auto page=app.ai_read_view().value->query(caps);CHECK(page.returned==1);CHECK(page.json.find(module)==std::string::npos&&page.json.find(fixture.wav.string())==std::string::npos);const Id plugin{field(page.json,"plugin_id")};
    int sequence{},opened{},closed{};app.set_ai_ara_editor_handler([&](Id id){CHECK(id==clip.id);++opened;},[&](Id){++closed;});
    auto call=[&](ToolKind kind,AraArgs args,Ticket& t){return ToolCall{1,t.id,"ara-"+std::to_string(++sequence),t.snapshot->context.key,kind,std::move(args)};};
    auto propose=[&](ToolKind kind,AraArgs args){auto t=ticket(app);return app.ai_propose(call(kind,std::move(args),t),t.snapshot);};
    auto preview=[&](ToolKind kind,AraArgs args){auto p=propose(kind,std::move(args));CHECK(p.value);return app.ai_preview({p.value});};
    const auto initial=app.services().projects->state();
    {auto t=ticket(app);ToolCall read{1,t.id,"read",t.snapshot->context.key,ToolKind::list_ara_capabilities,caps};CHECK(app.ai_tool_query(read,t.snapshot).returned==1);read.tool=ToolKind::get_ara_status;CHECK(app.ai_tool_query(read,t.snapshot).error==Error::invalid_input);}
    CHECK(propose(ToolKind::attach_ara,{clip.id,Id{"invented"}}).error==Error::not_found);
    CHECK(propose(ToolKind::open_ara_editor,{clip.id}).error==Error::invalid_input);
    auto attach=propose(ToolKind::attach_ara,{clip.id,plugin});CHECK(attach.value&&attach.value->affected==2);CHECK(app.ai_apply(attach.value).block==Block::confirmation);
    {auto t=ticket(app);const ToolCall marker{1,t.id,"mixed-marker",t.snapshot->context.key,ToolKind::add_marker,MarkerAddArgs{"No partial batch",0}};auto p=app.ai_propose(marker,t.snapshot);CHECK(p.value&&app.ai_preview({attach.value,p.value}).error==Error::unsupported&&app.services().projects->state().project==initial.project);}
    auto attached=app.ai_preview({attach.value});CHECK(attached.value&&attached.value->changes.front().scope.find("All 2 clips")!=std::string::npos);CHECK(app.services().projects->state().project==initial.project);
    backend->fail=true;CHECK(app.ai_accept_preview(attached.value).error==Error::backend_failure);CHECK(app.services().projects->state().project==initial.project&&app.services().projects->state().revision==initial.revision);
    app.connect(std::make_unique<Device>(),{0,48000,128,{}, {0,1},1});
    attached=app.ai_rebuild_preview(attached.value);CHECK(attached.value&&app.ai_accept_preview(attached.value).allowed());CHECK(app.services().projects->state().project->ara_bindings.size()==1);
    const auto bound=*app.services().projects->state().project;
    auto openedPreview=preview(ToolKind::open_ara_editor,{clip.id});CHECK(openedPreview.value&&opened==0);CHECK(app.ai_accept_preview(openedPreview.value).allowed()&&opened==1);CHECK(*app.services().projects->state().project==bound);CHECK(!app.ai_accept_preview(openedPreview.value).allowed()&&opened==1);
    CHECK(app.undo()&&app.services().projects->state().project->ara_bindings.empty());CHECK(app.redo()&&app.services().projects->state().project->ara_bindings.size()==1);
    auto bypass=preview(ToolKind::set_ara_bypass,{clip.id,{},true});CHECK(bypass.value);CHECK(app.ai_accept_preview(bypass.value).allowed()&&app.ara_bypassed(clip.source));CHECK(app.undo()&&!app.ara_bypassed(clip.source));CHECK(app.redo()&&app.ara_bypassed(clip.source));CHECK(app.undo());
    auto rejected=preview(ToolKind::remove_ara,{clip.id});CHECK(rejected.value);app.ai_reject_preview(rejected.value);CHECK(!app.ai_accept_preview(rejected.value).allowed()&&app.services().projects->state().project->ara_bindings.size()==1);
    {const auto key=app.ai_context().value->key;const auto snapshot=app.ai_snapshot(key,{caps}).value;CHECK(snapshot);const auto request=app.request_ai({snapshot->context,"cancel fixture",2s,snapshot},[](const Request&,const JobControl& control){while(!control.should_stop())std::this_thread::sleep_for(1ms);return "late reply";});CHECK(request.error==Error::none&&app.cancel_ai(request.request_id));Ticket cancelled{snapshot,request.request_id};CHECK(app.ai_propose(call(ToolKind::remove_ara,{clip.id},cancelled),snapshot).error==Error::permission_denied);}
    auto stale=preview(ToolKind::set_ara_bypass,{clip.id,{},true});Selection other;other.entities.push_back({EntityKind::track,clip.track});app.set_ai_selection(other);CHECK(app.ai_preview_status(stale.value).error==Error::stale_context);CHECK(!app.ai_accept_preview(stale.value).allowed()&&!app.ara_bypassed(clip.source));
    auto playing=preview(ToolKind::set_ara_bypass,{clip.id,{},true});std::array<float,256> audio{};app.play();app.engine()->process(nullptr,audio.data(),128);CHECK(!app.ai_accept_preview(playing.value).allowed());app.stop();app.engine()->process(nullptr,audio.data(),128);app.poll();CHECK(app.ai_accept_preview(playing.value).allowed());CHECK(app.undo());
    CHECK(propose(ToolKind::reanalyse_ara,{clip.id}).error==Error::unsupported);
    auto revoked=preview(ToolKind::remove_ara,{clip.id});policy.mode=PermissionMode::read;app.set_ai_permissions(policy);CHECK(!app.ai_accept_preview(revoked.value).allowed());CHECK(propose(ToolKind::remove_ara,{clip.id}).error==Error::permission_denied);
    policy.mode=PermissionMode::suggest;app.set_ai_permissions(policy);auto suggestion=preview(ToolKind::remove_ara,{clip.id});CHECK(suggestion.value&&!app.ai_accept_preview(suggestion.value).allowed());
    policy.mode=PermissionMode::automatic;policy.automatic_tools=31;app.set_ai_permissions(policy);auto automatic=preview(ToolKind::remove_ara,{clip.id});CHECK(automatic.value&&app.ai_accept_preview(automatic.value).block==Block::automatic_allowlist);
    policy.mode=PermissionMode::edit;app.set_ai_permissions(policy);auto remove=preview(ToolKind::remove_ara,{clip.id});CHECK(remove.value&&app.ai_accept_preview(remove.value).allowed());CHECK(closed==1&&app.services().projects->state().project->ara_bindings.empty());CHECK(app.undo()&&app.services().projects->state().project->ara_bindings.size()==1);
    auto catalogStale=preview(ToolKind::set_ara_bypass,{clip.id,{},true});app.set_ai_ara_catalog({{module,capability.class_id,"Changed name",capability.version}});CHECK(!app.ai_accept_preview(catalogStale.value).allowed());
    const auto save=fixture.root/"saved"/"project.mrs";app.save_project(save);auto old=preview(ToolKind::remove_ara,{clip.id});app.new_project();CHECK(!app.ai_accept_preview(old.value).allowed());app.open_project(save);CHECK(app.services().projects->state().project->ara_bindings.size()==1);
    auto wrong=std::async(std::launch::async,[&]{try{(void)app.ai_read_view();return false;}catch(const std::logic_error&){return true;}});CHECK(wrong.get());
    {const auto c=app.services().projects->state().project->clips.front();app.detach_ara_source(c.source);app.set_ai_ara_catalog({{module,"invalid-class","Changed factory", "1"}});const Id invalid{field(app.ai_read_view().value->query(caps).json,"plugin_id")};const auto before=app.services().projects->state();auto p=preview(ToolKind::attach_ara,{c.id,invalid});CHECK(p.value&&app.ai_accept_preview(p.value).error==Error::backend_failure);CHECK(app.services().projects->state().project==before.project&&app.services().projects->state().revision==before.revision);}
    app.set_ai_ara_catalog({{(fixture.root/"missing.vst3").string(),"missing","Missing", "1"}});const auto missing=app.ai_read_view().value->query(caps);CHECK(missing.json.find("\"available\":false")!=std::string::npos);
    const Id missingId{field(missing.json,"plugin_id")};const auto now=app.services().projects->state().project->clips.front();app.detach_ara_source(now.source);CHECK(propose(ToolKind::attach_ara,{now.id,missingId}).error==Error::not_found);
}
}
int main(int argc,char** argv){try{CHECK(argc==2);run(argv[1]);std::cout<<"PASS AI ARA host tools\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
