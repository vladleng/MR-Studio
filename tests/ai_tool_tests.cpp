#include <mrs/desktop.hpp>
#include <future>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mrs;using namespace mrs::ai;using namespace std::chrono_literals;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
template<class F> void eventually(F f){const auto end=std::chrono::steady_clock::now()+3s;while(std::chrono::steady_clock::now()<end){if(f())return;std::this_thread::sleep_for(1ms);}throw std::runtime_error("AI tools timeout");}
struct Directory {std::filesystem::path path=std::filesystem::temp_directory_path()/("mrs-ai-preview-"+new_id().value);Directory(){std::filesystem::create_directory(path);}~Directory(){std::error_code error;std::filesystem::remove_all(path,error);}};
std::string envelope(std::string tool,std::string args){return R"({"version":1,"request_id":"1","operation_id":"op","context_key":{"project_id":"project","session_id":"session","project_revision":"0","graph_revision":"0"},"tool":")"+tool+R"(","arguments":)"+args+"}";}
void registry(){
    CHECK(tool_registry().size()==15 && !tool_descriptor("system.execute") && !tool_descriptor("delete_project"));
    CHECK(decode_tool_call(envelope("attach_ara",R"({"clip_id":"clip","plugin_id":"host-plugin"})")).value);
    CHECK(decode_tool_call(envelope("attach_ara",R"({"clip_id":"clip","plugin_id":"host-plugin","module":"C:/secret.vst3"})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("set_ara_bypass",R"({"clip_id":"clip","bypass":true})")).value);
    CHECK(decode_tool_call(envelope("set_ara_bypass",R"({"clip_id":"clip","bypass":1})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("list_ara_capabilities",R"({})")).value);
    CHECK(decode_tool_call(envelope("get_ara_status",R"({"clip_id":"clip","limit":257})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("edit_ara_note",R"({"clip_id":"clip","note":3,"cents":20})")).error==Error::unsupported);
    CHECK(decode_tool_call(envelope("add_marker",R"({"name":"Verse","tick":0})")).value);
    auto unicode=decode_tool_call(envelope("add_marker",R"({"name":"\uD83C\uDFB5","tick":1})"));CHECK(unicode.value && std::get<MarkerAddArgs>(unicode.value->arguments).name=="🎵");
    const auto unpairedSurrogate=envelope("add_marker",R"({"name":"\uD800","tick":1})");
    CHECK(decode_tool_call(unpairedSurrogate).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("add_marker",R"({"name":"\u0000","tick":1})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("add_marker",R"({"name":"","tick":1})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("add_marker",R"({"name":"x","tick":-1})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("add_marker",R"({"name":"x","tick":1.0})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("add_marker",R"({"name":"x","tick":1,"confirmed":true})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("add_marker",R"({"name":"x","name":"y","tick":1})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("run_shell",R"({"command":"anything"})")).error==Error::unsupported);
    CHECK(decode_tool_call(envelope("add_marker",R"({"name":"x","tick":1})")+" trailing").error==Error::invalid_input);
    auto escalation=envelope("add_marker",R"({"name":"x","tick":1})");escalation.insert(1,"\"mode\":\"AUTO\",");CHECK(decode_tool_call(escalation).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("transpose_notes",R"({"clip_id":"clip","note_ids":["note"],"semitones":25})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("set_note_velocity",R"({"clip_id":"clip","note_ids":["note","note"],"velocity":100})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("set_selection",R"({"entities":[],"sample_start":5,"sample_end":4})")).error==Error::invalid_input);
    CHECK(decode_tool_call(envelope("query_project",R"({"query":"tracks","limit":256})")).value);
    CHECK(decode_tool_call(std::string(32769,'x')).error==Error::invalid_input);
    Project p;p.id={"project"};p.markers={{{"marker"},"Before",0}};ProjectStore store(p);ContextKey key{p.id,Id{"session"},0,0};
    auto call=*decode_tool_call(envelope("add_marker",R"({"name":"Verse","tick":0})")).value;
    auto plan=prepare_tool(call,store.state(),key,{},1);CHECK(plan.value && plan.value->command_name()=="Add marker" && plan.value->affected==1);
    call.tool=ToolKind::rename_marker;call.arguments=MarkerRenameArgs{{"marker"},"After"};plan=prepare_tool(call,store.state(),key,{},1);CHECK(plan.value && plan.value->command_name()=="Edit marker");
    call.arguments=MarkerRenameArgs{{"missing"},"After"};CHECK(prepare_tool(call,store.state(),key,{},1).error==Error::not_found);
    CHECK(store.state().revision==0 && *store.state().project==p);
}
void policy(){
    PermissionPolicy p;
    for(auto mode:{PermissionMode::read,PermissionMode::suggest,PermissionMode::edit,PermissionMode::automatic}){
        p.mode=mode;CHECK(authorize(p,ToolKind::query,Intent::read).allowed());
        CHECK(!authorize(p,ToolKind::add_marker,Intent::apply).allowed());
        CHECK(authorize(p,ToolKind::add_marker,Intent::suggest).allowed()==(mode!=PermissionMode::read));
        CHECK(authorize(p,static_cast<ToolKind>(900),Intent::apply).error==Error::unsupported);
    }
    p.mode=PermissionMode::edit;CHECK(authorize(p,ToolKind::add_marker,Intent::apply).block==Block::confirmation);
    CHECK(authorize(p,ToolKind::add_marker,Intent::apply,{}, {true,false,false,false}).block==Block::preview);
    CHECK(authorize(p,ToolKind::add_marker,Intent::apply,{}, {true,true,false,false}).block==Block::undo);
    CHECK(authorize(p,ToolKind::transpose_notes,Intent::apply,{}, {true,true,true,false}).block==Block::midi_contract);
    CHECK(authorize(p,ToolKind::add_marker,Intent::apply,{true,false,false,true},{true,true,true,true}).block==Block::live_performance);
    CHECK(authorize(p,ToolKind::add_marker,Intent::apply,{false,true,false,false},{true,true,true,true}).block==Block::recording);
    p.mode=PermissionMode::automatic;p.automatic_tools=1;CHECK(authorize(p,ToolKind::add_marker,Intent::apply).block==Block::preview);
    p.automatic_tools=31;CHECK(authorize(p,ToolKind::attach_ara,Intent::apply,{}, {true,true,true,true}).block==Block::automatic_allowlist);p.automatic_tools=1;
    p.mode=PermissionMode::read;CHECK(authorize(p,ToolKind::list_ara_capabilities,Intent::read).allowed());CHECK(!authorize(p,ToolKind::open_ara_editor,Intent::suggest).allowed());p.mode=PermissionMode::automatic;
    CHECK(authorize(p,ToolKind::rename_marker,Intent::apply).block==Block::automatic_allowlist);
    p.enabled=false;CHECK(authorize(p,ToolKind::query,Intent::read).block==Block::disabled);
    desktop::Preferences prefs;p.enabled=true;p.stored_plugin_parameters=true;p.automatic_tools=31;prefs.ai=p;
    const auto bytes=desktop::encode_preferences(prefs);CHECK(desktop::decode_preferences(bytes)==prefs);
    const auto legacy=desktop::decode_preferences("MRS_DESKTOP_CONFIG 7\n0 48000 128 -1 \"\" 2 0 1 0\n0\n0\n2 0\n0 0 1 50 0\n");CHECK(legacy.ai==PermissionPolicy{});
    auto invalid=bytes;invalid.replace(invalid.rfind("3 1 1 31"),8,"9 1 1 31");bool rejected{};try{(void)desktop::decode_preferences(invalid);}catch(const std::exception&){rejected=true;}CHECK(rejected);
}
std::pair<std::shared_ptr<const Snapshot>,std::uint64_t> issue(desktop::Application& app){
    auto key=app.ai_context().value->key;auto snapshot=app.ai_snapshot(key,{Query{}});CHECK(snapshot.value);
    auto submitted=app.request_ai({snapshot.value->context,"mock",2s,snapshot.value},[](const Request&,const JobControl&){return "reply";});CHECK(submitted.error==Error::none);
    std::optional<Result> reply;eventually([&]{reply=app.take_ai_result();return reply.has_value();});return {snapshot.value,submitted.request_id};
}
void previews(){
    desktop::Application app;app.new_project();auto policy=app.ai_permissions();policy.mode=PermissionMode::edit;app.set_ai_permissions(policy);
    auto [snapshot,request]=issue(app);const auto before=app.snapshot();const auto state=app.services().projects->state();
    ToolCall first{1,request,"one",snapshot->context.key,ToolKind::add_marker,MarkerAddArgs{"One",50}};
    auto second=first;second.operation_id="two";second.arguments=MarkerAddArgs{"Two",ppq};
    auto a=app.ai_propose(first,snapshot),b=app.ai_propose(second,snapshot);CHECK(a.value&&b.value);
    auto cancelled=app.ai_preview({a.value,b.value});CHECK(cancelled.value&&cancelled.value->changes.size()==4);CHECK(app.snapshot()==before);app.ai_reject_preview(cancelled.value);
    CHECK(!app.ai_accept_preview(cancelled.value).allowed());CHECK(app.snapshot()==before&&app.services().projects->state().can_undo==state.can_undo);
    auto preview=app.ai_preview({a.value,b.value});CHECK(preview.value&&app.ai_preview_status(preview.value).allowed());
    CHECK(app.ai_accept_preview(preview.value).allowed());CHECK(preview.value->state()==PreviewState::accepted);auto accepted=app.snapshot();CHECK(accepted.project.markers.size()==2);
    for(const auto& change:preview.value->changes){auto marker=std::find_if(accepted.project.markers.begin(),accepted.project.markers.end(),[&](const auto& value){return value.id==change.target;});CHECK(marker!=accepted.project.markers.end());CHECK(change.property=="marker.name"?marker->name==change.after:std::to_string(marker->tick)==change.after);}
    CHECK(!app.ai_accept_preview(preview.value).allowed()&&app.snapshot()==accepted);app.undo();CHECK(app.snapshot().project==before.project);app.redo();CHECK(app.snapshot().project==accepted.project);
    auto [fresh,freshRequest]=issue(app);first.request_id=freshRequest;first.key=fresh->context.key;first.operation_id="stale";auto p=app.ai_propose(first,fresh);auto stale=app.ai_preview({p.value});CHECK(stale.value);app.add_marker("Manual",100);
    CHECK(app.ai_preview_status(stale.value).error==Error::stale_context&&stale.value->state()==PreviewState::stale);CHECK(!app.ai_accept_preview(stale.value).allowed());
    auto rebuilt=app.ai_rebuild_preview(stale.value);CHECK(rebuilt.value&&rebuilt.value->key!=stale.value->key);CHECK(app.ai_accept_preview(rebuilt.value).allowed());
    CHECK(!app.ai_rebuild_preview(stale.value).value);app.undo();CHECK(!app.ai_rebuild_preview(stale.value).value);app.redo();
    // Dependent conflicting changes fail as a group before publishing anything.
    auto [groupSnapshot,groupRequest]=issue(app);const auto marker=app.snapshot().project.markers.front();
    ToolCall rename{1,groupRequest,"rename-a",groupSnapshot->context.key,ToolKind::rename_marker,MarkerRenameArgs{marker.id,"A"}};
    auto renameB=rename;renameB.operation_id="rename-b";renameB.arguments=MarkerRenameArgs{marker.id,"B"};auto ra=app.ai_propose(rename,groupSnapshot),rb=app.ai_propose(renameB,groupSnapshot);const auto unchanged=app.snapshot();
    CHECK(!app.ai_preview({ra.value,rb.value}).value&&app.snapshot()==unchanged);CHECK(!app.ai_preview({ra.value,ra.value}).value);
    auto renamePreview=app.ai_preview({ra.value});CHECK(renamePreview.value&&renamePreview.value->changes.size()==1&&renamePreview.value->changes.front().before==marker.name);CHECK(app.ai_accept_preview(renamePreview.value).allowed());
    Directory directory;app.save_project(directory.path/"Preview.mrsproject");CHECK(!app.dirty());const auto saved=app.snapshot().project;app.open_project(directory.path/"Preview.mrsproject");CHECK(app.snapshot().project==saved&&!app.dirty());
    auto [cleanSnapshot,cleanRequest]=issue(app);first.request_id=cleanRequest;first.key=cleanSnapshot->context.key;first.operation_id="clean-reject";auto cleanPlan=app.ai_propose(first,cleanSnapshot);auto cleanPreview=app.ai_preview({cleanPlan.value});CHECK(cleanPreview.value&&!app.dirty());app.ai_reject_preview(cleanPreview.value);CHECK(!app.dirty()&&!app.services().projects->state().can_undo);
    // MIDI diff can now be confirmed through the prepared playback transaction.
    auto track=app.add_instrument_track("Keys");auto clip=app.create_midi_clip(track,0);auto note=new_id();app.set_midi_notes(clip,{{note,0,ppq,60,100,0}});
    auto [midiSnapshot,midiRequest]=issue(app);ToolCall midi{1,midiRequest,"midi-preview",midiSnapshot->context.key,ToolKind::transpose_notes,MidiArgs{clip,{note},12}};auto midiPlan=app.ai_propose(midi,midiSnapshot);auto midiPreview=app.ai_preview({midiPlan.value});CHECK(midiPreview.value&&midiPreview.value->changes.front().before=="60"&&midiPreview.value->changes.front().after=="72");CHECK(app.ai_preview_status(midiPreview.value).allowed());CHECK(app.ai_accept_preview(midiPreview.value).allowed());CHECK(app.undo());
    auto [finalSnapshot,finalRequest]=issue(app);first.request_id=finalRequest;first.key=finalSnapshot->context.key;first.operation_id="disabled";auto finalPlan=app.ai_propose(first,finalSnapshot);auto finalPreview=app.ai_preview({finalPlan.value});CHECK(finalPreview.value);
    app.workspace(desktop::Workspace::live);app.play();eventually([&]{return app.engine()->state().playback==PlaybackState::playing;});CHECK(app.ai_preview_status(finalPreview.value).block==Block::live_performance);app.stop();eventually([&]{app.poll();return app.engine()->state().playback==PlaybackState::stopped;});
    policy.enabled=false;app.set_ai_permissions(policy);CHECK(!app.ai_accept_preview(finalPreview.value).allowed());CHECK(!app.ai_rebuild_preview(finalPreview.value).value);
    policy.enabled=true;app.set_ai_permissions(policy);app.new_project();CHECK(!app.ai_accept_preview(finalPreview.value).allowed());
}
void transactions(){
    Directory dir;desktop::Application app;app.new_project();auto track=app.add_instrument_track("Keys");auto clip=app.create_midi_clip(track,0);auto note=new_id();app.set_midi_notes(clip,{{note,0,ppq,60,100,0}});auto marker=app.add_marker("Before",ppq);
    const auto original=app.snapshot().project;ProjectStore store{original,2};int invocations{},notifications{};
    struct Counted final:ICommand {int& count;Marker before;Counted(int& n,Marker m):count(n),before(std::move(m)){}std::string_view name()const override{return "Rename";}void apply(Project& p)const override{++count;auto after=before;after.name="After";UpdateMarker{before,after}.apply(p);}};
    NoteEdit transpose;transpose.kind=NoteEditKind::transpose;transpose.value=12;
    auto mix=original.tracks.front().mix;mix.gain=.5;
    auto counted=std::make_shared<Counted>(invocations,original.markers.front());
    const ProjectEdit edit{store.state().project,{counted,std::make_shared<EditMidiNotes>(clip,std::vector<Id>{note},transpose),std::make_shared<SetTrackMix>(track,mix)}};
    CHECK(invocations==1&&*store.state().project==original);const auto initial=store.state();
    auto observer=store.subscribe([&](const auto&){++notifications;});
    // An abandoned or rolled-back commit restores model AND both history branches.
    {auto tx=store.prepare_transaction(&edit);CHECK(store.state().project==initial.project);}
    {auto tx=store.prepare_transaction(&edit);tx->commit();CHECK(*store.state().project==*edit.after()&&notifications==0);}
    CHECK(store.state().project==initial.project&&store.state().revision==initial.revision&&!store.state().can_undo&&!store.state().can_redo&&store.state().command==initial.command);
    {auto tx=store.prepare_transaction(&edit);tx->commit();tx->finish();}
    CHECK(notifications==1&&invocations==1&&store.state().command=="AI proposal (3 commands)");
    const auto accepted=*store.state().project;CHECK(store.history_name(false)=="AI proposal (3 commands)"&&store.history_name(true).empty());CHECK(accepted.markers.front().name=="After"&&accepted.clips.front().midi->notes.front().pitch==72&&accepted.tracks.front().mix.gain==.5);
    CHECK(store.undo()&&*store.state().project==original&&!store.state().can_undo&&store.state().can_redo);CHECK(store.redo()&&*store.state().project==accepted&&invocations==1);
    CHECK(deserialize(serialize(accepted))==accepted);
    // A late failing member leaves no partial rename/MIDI edit or history entry.
    auto rejects=[&](auto action){bool failed{};try{action();}catch(const std::exception&){failed=true;}CHECK(failed);};
    const auto unchanged=store.state();rejects([&]{ProjectEdit bad{store.state().project,{std::make_shared<AddMarker>(Marker{new_id(),"Partial",20}),std::make_shared<RenameTrack>(Id{"missing"},"Fail")}};});
    CHECK(store.state().project==unchanged.project&&store.state().revision==unchanged.revision&&store.state().can_undo==unchanged.can_undo&&store.state().can_redo==unchanged.can_redo);
    rejects([&]{store.execute(edit);});CHECK(store.state().project==unchanged.project);
    // No-op keeps the existing redo branch; rollback does too, even after commit.
    store.undo();const auto redoState=store.state();ProjectEdit noOp{redoState.project,{std::make_shared<SetTrackMix>(track,original.tracks.front().mix)}};CHECK(!store.prepare_transaction(&noOp)&&store.state().can_redo);
    {auto tx=store.prepare_transaction(&edit);tx->commit();tx->rollback();}CHECK(store.state().project==redoState.project&&store.state().revision==redoState.revision&&store.state().can_redo);
    store.execute(AddMarker{{new_id(),"Manual",40}});CHECK(!store.state().can_redo);rejects([&]{store.execute(edit);});
    // Application shares manual/AI history and preserves its revision-based dirty policy.
    auto policy=app.ai_permissions();policy.mode=PermissionMode::edit;app.set_ai_permissions(policy);app.save_project(dir.path/"History.mrsproject");CHECK(!app.dirty());
    auto [snapshot,request]=issue(app);CHECK(!app.dirty());ToolCall add{1,request,"history-add",snapshot->context.key,ToolKind::add_marker,MarkerAddArgs{"AI",100}};
    ToolCall rename{1,request,"history-rename",snapshot->context.key,ToolKind::rename_marker,MarkerRenameArgs{marker,"AI rename"}};
    auto preview=app.ai_preview({app.ai_propose(add,snapshot).value,app.ai_propose(rename,snapshot).value});CHECK(preview.value&&!app.dirty());CHECK(app.ai_accept_preview(preview.value).allowed()&&app.dirty());const auto aiProject=app.snapshot().project;
    app.add_marker("Human",200);const auto human=app.snapshot().project;CHECK(app.undo()&&app.snapshot().project==aiProject);CHECK(app.undo()&&app.snapshot().project==original&&app.dirty());
    policy.enabled=false;app.set_ai_permissions(policy);CHECK(app.redo()&&app.snapshot().project==aiProject);CHECK(app.redo()&&app.snapshot().project==human);CHECK(!app.take_ai_result());
    app.save_project(dir.path/"History.mrsproject");CHECK(!app.dirty());app.open_project(dir.path/"History.mrsproject");CHECK(app.snapshot().project==human&&!app.dirty()&&!app.undo()&&!app.redo());
}
void drafts(){
    const std::string json=R"({"mode":"insert","track_id":"track","name":"Bass","tick_start":960,"tick_end":4800,"notes":[{"start":0,"duration":960,"pitch":36,"velocity":100,"channel":0}],"cc":[{"start":0,"controller":11,"value":80,"channel":0}]})";
    CHECK(decode_tool_call(envelope("midi_draft",json)).value);
    for(const auto& [from,to]:std::vector<std::pair<std::string,std::string>>{{"\"pitch\":36","\"pitch\":128"},{"\"channel\":0","\"channel\":16"},{"\"duration\":960","\"duration\":0"},{"\"duration\":960","\"duration\":960.0"},{"\"start\":0","\"start\":3840"},{"\"tick_end\":4800","\"tick_end\":9223372036854775807"},{"\"value\":80","\"value\":-1"},{"\"mode\":\"insert\"","\"mode\":\"edit\""}}){auto bad=json;bad.replace(bad.find(from),from.size(),to);CHECK(!decode_tool_call(envelope("midi_draft",bad)).value);}
    CHECK(!decode_tool_call(envelope("midi_draft",json.substr(0,json.size()-2))).value);
    Directory dir;desktop::Application app;app.new_project();const auto track=app.add_instrument_track("Bass");auto time=app.snapshot().project.time;time.tempos={{0,120},{2*ppq,90}};time.meters={{1,4,4},{2,3,4}};app.set_time_map(time);auto policy=app.ai_permissions();policy.mode=PermissionMode::edit;app.set_ai_permissions(policy);
    struct Device final:audio::IAudioDevice{bool fail{};audio::DevicePhase phase{audio::DevicePhase::closed};std::vector<audio::DeviceInfo> enumerate()override{return {{0,"Draft fixture",{}, {"L","R"},16,8192,128,1}};}void control_panel(int)override{}void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>)override{phase=audio::DevicePhase::open;}void start()override{if(fail){fail=false;throw std::runtime_error("Draft restart failure");}phase=audio::DevicePhase::running;}void stop()override{phase=audio::DevicePhase::stopped;}void close()noexcept override{phase=audio::DevicePhase::closed;}audio::DeviceStatus status()override{return {phase,48000};}};
    auto device=std::make_unique<Device>();auto* driver=device.get();app.connect(std::move(device),{0,48000,128,{}, {0,1}});const auto generation=app.insert_generation(),midiGeneration=app.engine()->midi_generation();unsigned published{};auto observer=app.services().projects->subscribe([&](const auto&){++published;});
    MidiDraftArgs args;args.track=track;args.name="Bass";args.start=ppq;args.end=5*ppq;args.notes={{{},0,ppq,36,100,0},{{},ppq,ppq,43,80,0},{{},2*ppq,ppq,48,90,0}};args.cc={{{},0,11,80,0}};
    int counter{};auto prepare=[&](const MidiDraftArgs& value){auto [snapshot,request]=issue(app);ToolCall call{1,request,"draft-"+std::to_string(++counter),snapshot->context.key,ToolKind::midi_draft,value};return app.ai_propose(call,snapshot);};
    const auto before=app.services().projects->state();auto plan=prepare(args);CHECK(plan.value);auto [groupSnapshot,groupRequest]=issue(app);auto markerPlan=app.ai_propose({1,groupRequest,"draft-marker",groupSnapshot->context.key,ToolKind::add_marker,MarkerAddArgs{"Bass start",ppq}},groupSnapshot);auto preview=app.ai_preview({plan.value,markerPlan.value});CHECK(preview.value&&preview.value->changes.size()==7&&app.snapshot().project==*before.project);driver->fail=true;CHECK(app.ai_accept_preview(preview.value).error==Error::backend_failure);CHECK(app.services().projects->state().project==before.project&&app.services().projects->state().revision==before.revision&&published==0&&driver->phase==audio::DevicePhase::running);
    auto refreshed=app.ai_rebuild_preview(preview.value);CHECK(refreshed.value&&app.ai_accept_preview(refreshed.value).allowed()&&published==1);const auto inserted=app.snapshot().project;const auto clip=inserted.clips.front();CHECK(clip.midi->notes.size()==3&&clip.midi->events.size()==1&&clip.midi->notes.front().pitch==36);CHECK(app.insert_generation()==generation&&app.engine()->midi_generation()==midiGeneration);
    const std::array<Id,1> tracks{track};const auto notes=audio::compile_midi_clips(inserted,tracks);const auto events=audio::compile_midi_events(inserted,tracks);CHECK(notes[0].size()==3&&events[0].size()==1);CHECK(notes[0][0].start==Timeline(time,48000).to_samples(ppq)&&notes[0][1].end==Timeline(time,48000).to_samples(3*ppq));
    audio::MidiPlayback playback;playback.prepare(notes[0],events[0]);processing::MidiBuffer played;CHECK(playback.render(notes[0][0].start,128,true,played));CHECK(std::any_of(played.view().begin(),played.view().end(),[](const auto& e){return e.kind==processing::MidiKind::note_on&&e.data1==36&&e.data2==100;}));CHECK(std::any_of(played.view().begin(),played.view().end(),[](const auto& e){return e.kind==processing::MidiKind::cc&&e.data1==11&&e.data2==80;}));
    CHECK(app.undo()&&app.snapshot().project==*before.project);driver->fail=true;bool redoFailed{};try{app.redo();}catch(...){redoFailed=true;}CHECK(redoFailed&&app.snapshot().project==*before.project&&app.services().projects->state().can_redo);CHECK(app.redo()&&app.snapshot().project==inserted);
    // Replace is scoped: keep outer notes and non-CC events unchanged.
    args.mode=MidiDraftMode::replace;args.track.reset();args.clip=clip.id;args.name.clear();args.start=2*ppq;args.end=3*ppq;args.notes={{{},0,ppq,40,70,0}};args.cc={{{},0,1,42,0}};
    plan=prepare(args);preview=app.ai_preview({plan.value});CHECK(preview.value&&app.ai_accept_preview(preview.value).allowed());const auto replaced=app.snapshot().project;for(const auto index:{0,2})CHECK(std::any_of(replaced.clips.front().midi->notes.begin(),replaced.clips.front().midi->notes.end(),[&](const auto& n){return n==clip.midi->notes[static_cast<std::size_t>(index)];}));CHECK(app.undo()&&app.snapshot().project==inserted&&app.redo()&&app.snapshot().project==replaced);
    auto replacement=*replaced.clips.front().midi;const auto target=std::find_if(replacement.notes.begin(),replacement.notes.end(),[](const auto& n){return n.pitch==40;})->id;
    args.mode=MidiDraftMode::edit;args.notes={{{target},0,ppq,41,110,1}};args.cc.clear();plan=prepare(args);preview=app.ai_preview({plan.value});CHECK(preview.value&&app.ai_accept_preview(preview.value).allowed());const auto edited=app.snapshot().project;CHECK(std::any_of(edited.clips.front().midi->notes.begin(),edited.clips.front().midi->notes.end(),[&](const auto& n){return n.id==target&&n.pitch==41&&n.velocity==110&&n.channel==1;}));
    auto invalid=args;invalid.notes.front().duration=ppq+1;const auto stable=app.snapshot();CHECK(!prepare(invalid).value&&app.snapshot()==stable);invalid=args;invalid.notes.front().id=Id{"missing"};CHECK(!prepare(invalid).value&&app.snapshot()==stable);
    invalid=args;invalid.mode=MidiDraftMode::replace;invalid.notes.front().id.reset();invalid.start=2*ppq+1;CHECK(!prepare(invalid).value&&app.snapshot()==stable);
    args.start=ppq;args.end=5*ppq;args.notes.clear();args.cc={{{clip.midi->events.front().id},0,11,60,0}};plan=prepare(args);preview=app.ai_preview({plan.value});CHECK(preview.value&&app.ai_accept_preview(preview.value).allowed());CHECK(app.snapshot().project.clips.front().midi->events.front().id==clip.midi->events.front().id&&app.snapshot().project.clips.front().midi->events.front().data2==60);
    args.mode=MidiDraftMode::insert;args.clip.reset();args.track=track;args.name="Drums pitches explicitly chosen";args.start=8*ppq;args.end=9*ppq;args.notes={{{},0,ppq,36,100,9},{{},0,ppq,38,90,9}};args.cc.clear();
    plan=prepare(args);preview=app.ai_preview({plan.value});CHECK(preview.value);std::array<float,256> output{};app.play();app.engine()->process(nullptr,output.data(),128);CHECK(app.ai_preview_status(preview.value).block==Block::midi_playback);app.stop();app.engine()->process(nullptr,output.data(),128);app.poll();CHECK(app.ai_accept_preview(preview.value).allowed());
    invalid=args;invalid.notes[1].pitch=36;CHECK(!prepare(invalid).value);invalid=args;invalid.notes.assign(257,invalid.notes.front());CHECK(!prepare(invalid).value);invalid=args;invalid.track=Id{"missing"};CHECK(!prepare(invalid).value);
    // Trimmed source: replace visible data without touching hidden source or non-CC.
    const auto cropped=app.create_midi_clip(track,12*ppq,4*ppq);const MidiNote hidden{new_id(),0,ppq,60,90,0},inside{new_id(),2*ppq,ppq,62,90,0},outside{new_id(),3*ppq,ppq,65,90,0};app.set_midi_notes(cropped,{hidden,inside,outside});const MidiChannelEvent retained{new_id(),2*ppq,4,0,5,0};app.set_midi_events(cropped,{retained});const Timeline timeline(time,48000);app.trim_clip(cropped,timeline.to_samples(14*ppq),timeline.to_samples(16*ppq));
    args.mode=MidiDraftMode::replace;args.track.reset();args.clip=cropped;args.name.clear();args.start=14*ppq;args.end=15*ppq;args.notes={{{},0,ppq,70,100,0}};args.cc.clear();plan=prepare(args);preview=app.ai_preview({plan.value});CHECK(preview.value&&app.ai_accept_preview(preview.value).allowed());auto trimProject=app.snapshot().project;auto trim=std::find_if(trimProject.clips.begin(),trimProject.clips.end(),[&](const auto& c){return c.id==cropped;});CHECK(trim->midi->source_offset==2*ppq&&trim->midi->notes[0]==hidden&&trim->midi->notes[1]==outside&&trim->midi->notes[2].start==2*ppq&&trim->midi->events==std::vector<MidiChannelEvent>{retained});
    args.notes.clear();plan=prepare(args);preview=app.ai_preview({plan.value});CHECK(preview.value&&app.ai_accept_preview(preview.value).allowed());trimProject=app.snapshot().project;trim=std::find_if(trimProject.clips.begin(),trimProject.clips.end(),[&](const auto& c){return c.id==cropped;});CHECK(trim->midi->notes==std::vector<MidiNote>({hidden,outside})&&trim->midi->events==std::vector<MidiChannelEvent>{retained});
    auto full=app.snapshot().project;full.clips.clear();Clip capacity;capacity.id=new_id();capacity.track=track;capacity.name="Limit";capacity.midi=MidiClip{0,32768,0,{}};for(int i=0;i<32768;++i)capacity.midi->notes.push_back({new_id(),i,1,60,100,0});full.clips.push_back(capacity);ProjectStore limit{full};const ContextKey limitKey{full.id,new_id(),0,0};args.mode=MidiDraftMode::insert;args.clip.reset();args.track=track;args.name="Overflow";args.start=32768;args.end=32769;args.notes={{{},0,1,61,100,0}};CHECK(!prepare_tool({1,1,"limit",limitKey,ToolKind::midi_draft,args},limit.state(),limitKey,policy,1).value&&limit.state().revision==0);
    // An explicitly empty INSERT is still a real clip change, with exact history.
    const auto beforeEmpty=app.snapshot().project;args.mode=MidiDraftMode::insert;args.clip.reset();args.track=track;args.name="Empty Piano";args.start=0;args.end=7680;args.notes.clear();args.cc.clear();
    CHECK(decode_tool_call(envelope("midi_draft",R"({"mode":"insert","track_id":"track","name":"Empty","tick_start":0,"tick_end":7680,"notes":[],"cc":[]})")).value);
    plan=prepare(args);CHECK(plan.value);preview=app.ai_preview({plan.value});CHECK(preview.value&&preview.value->changes.size()==1);
    driver->fail=true;CHECK(app.ai_accept_preview(preview.value).error==Error::backend_failure&&app.snapshot().project==beforeEmpty);
    preview=app.ai_rebuild_preview(preview.value);CHECK(preview.value&&app.ai_accept_preview(preview.value).allowed());const auto emptyProject=app.snapshot().project;const auto empty=emptyProject.clips.back();
    CHECK(empty.track==track&&empty.midi->start==0&&empty.midi->length==7680&&empty.midi->notes.empty()&&empty.midi->events.empty());
    CHECK(app.undo()&&app.snapshot().project==beforeEmpty&&app.redo()&&app.snapshot().project==emptyProject);
    const auto removeGeneration=app.insert_generation(),removeMidiGeneration=app.engine()->midi_generation();driver->fail=true;bool deleteFailed{};try{app.remove_clip(empty.id);}catch(...){deleteFailed=true;}CHECK(deleteFailed&&app.snapshot().project==emptyProject);
    app.services().transport->set_loop(LoopRange{0,1000});app.engine()->process(nullptr,output.data(),128);
    const auto deleteStart=std::chrono::steady_clock::now();app.remove_clips({empty.id});const auto removed=app.snapshot().project;CHECK(removed.clips.size()+1==emptyProject.clips.size()&&app.history_preserves_plugins(false));CHECK(app.undo()&&app.snapshot().project==emptyProject&&app.history_preserves_plugins(true)&&app.redo()&&app.snapshot().project==removed);CHECK(app.insert_generation()==removeGeneration&&app.engine()->midi_generation()==removeMidiGeneration&&app.engine()->state().loop==std::optional<LoopRange>{{0,1000}});std::cout<<"MIDI remove+Undo+Redo fixture ms: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-deleteStart).count()<<'\n';app.services().transport->set_loop({});app.engine()->process(nullptr,output.data(),128);
    app.save_project(dir.path/"Draft.mrsproject");const auto saved=app.snapshot().project;app.open_project(dir.path/"Draft.mrsproject");CHECK(app.snapshot().project==saved&&!app.dirty()&&!app.undo());
}
void authorization(){
    std::atomic<bool> running{},cancelled{};desktop::Application app;app.new_project();auto track=app.add_instrument_track("Keys");auto clip=app.create_midi_clip(track,0);const auto note=new_id();app.set_midi_notes(clip,{{note,0,ppq,60,100,0}});
    auto [snapshot,request]=issue(app);const auto before=app.snapshot();auto view=app.ai_read_view();
    Query parameters;parameters.kind=QueryKind::graph_parameters;parameters.plugin=Id{"any"};
    CHECK(view.value->query(parameters,{true}).error==Error::permission_denied);
    CHECK(snapshot->json.find("policy_revision")==std::string::npos && snapshot->json.find("automatic_tools")==std::string::npos);
    ToolCall call{1,request,"marker-op",snapshot->context.key,ToolKind::add_marker,MarkerAddArgs{"Verse",0}};
    auto plan=app.ai_propose(call,snapshot);CHECK(plan.value && plan.value->command_name()=="Add marker");
    auto repeated=app.ai_propose(call,snapshot);CHECK(repeated.duplicate && repeated.value==plan.value);
    CHECK(!app.ai_apply(plan.value).allowed());CHECK(app.snapshot()==before);
    auto conflict=call;conflict.arguments=MarkerAddArgs{"Different",0};CHECK(app.ai_propose(conflict,snapshot).error==Error::invalid_input);
    auto midi=call;midi.tool=ToolKind::transpose_notes;midi.operation_id="midi-op";midi.arguments=MidiArgs{clip,{note},12};auto midi_plan=app.ai_propose(midi,snapshot);CHECK(midi_plan.value && midi_plan.value->command_name()=="Musical note edit");CHECK(!app.ai_apply(midi_plan.value).allowed());CHECK(app.snapshot()==before);
    ToolCall read{1,request,"read-op",snapshot->context.key,ToolKind::query,Query{}};CHECK(app.ai_tool_query(read,snapshot).returned==1);
    auto unissued=call;unissued.request_id=999;CHECK(app.ai_propose(unissued,snapshot).error==Error::permission_denied);
    auto policy=app.ai_permissions();policy.mode=PermissionMode::edit;app.set_ai_permissions(policy);
    CHECK(app.ai_apply(plan.value).block==Block::stale_policy);CHECK(!snapshot->access_valid());CHECK(view.value->query(Query{}).error==Error::permission_denied);
    CHECK(app.ai_tool_query(read,snapshot).error==Error::permission_denied);
    auto [fresh,fresh_id]=issue(app);call.request_id=fresh_id;call.operation_id="fresh-op";call.key=fresh->context.key;auto fresh_plan=app.ai_propose(call,fresh);CHECK(fresh_plan.value && app.ai_apply(fresh_plan.value).block==Block::confirmation);
    app.workspace(desktop::Workspace::live);app.play();eventually([&]{return app.engine()->state().playback==PlaybackState::playing;});CHECK(app.ai_apply(fresh_plan.value).block==Block::live_performance);app.stop();eventually([&]{app.poll();return app.engine()->state().playback==PlaybackState::stopped;});
    auto request_slow=app.request_ai({fresh->context,"slow",2s,fresh},[&](const Request&,const JobControl& c){running=true;while(!c.should_stop())std::this_thread::sleep_for(1ms);cancelled=true;return "late";});CHECK(request_slow.error==Error::none);eventually([&]{return running.load();});
    policy.enabled=false;app.set_ai_permissions(policy);eventually([&]{return cancelled.load();});CHECK(!app.take_ai_result());CHECK(app.ai_context().error==Error::permission_denied);CHECK(app.ai_read_view().error==Error::permission_denied);CHECK(app.ai_tool_query(read,snapshot).error==Error::permission_denied);CHECK(app.snapshot()==before);
    policy.enabled=true;policy.mode=PermissionMode::read;app.set_ai_permissions(policy);auto [readonly,read_id]=issue(app);call.request_id=read_id;call.key=readonly->context.key;CHECK(app.ai_propose(call,readonly).error==Error::permission_denied);
    read.request_id=read_id;read.key=readonly->context.key;CHECK(app.ai_tool_query(read,readonly).returned==1);
    app.new_project();CHECK(!readonly->access_valid());CHECK(app.ai_tool_query(read,readonly).error==Error::permission_denied);
    auto wrong=std::async(std::launch::async,[&]{try{app.set_ai_permissions({});}catch(const std::logic_error&){return true;}return false;});CHECK(wrong.get());
    std::shared_ptr<const ReadView> retired;{desktop::Application another;retired=another.ai_read_view().value;}CHECK(retired->query(Query{}).error==Error::permission_denied);
}
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("suite required");const std::string suite=argv[1];
    if(suite=="registry")registry();else if(suite=="policy")policy();else if(suite=="authorization")authorization();else if(suite=="preview")previews();else if(suite=="transactions")transactions();else if(suite=="midi_draft")drafts();else if(suite=="schema"){std::cout<<tool_envelope_schema();return 0;}
    else throw std::runtime_error("unknown suite");std::cout<<"PASS AI "<<suite<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
