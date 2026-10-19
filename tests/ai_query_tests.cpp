#include <mrs/desktop.hpp>
#include <mrs/midi_clips.hpp>
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mrs;using namespace mrs::ai;using namespace std::chrono_literals;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
template<class F> void eventually(F f){const auto deadline=std::chrono::steady_clock::now()+3s;
    while(std::chrono::steady_clock::now()<deadline){if(f())return;std::this_thread::sleep_for(1ms);}throw std::runtime_error("query test timed out");}
persistence::ProjectDocument fixture(){
    persistence::ProjectDocument d;auto& p=d.project;p.id={"project"};p.title="Песня \"été\" 🎵\nline";p.artist="Artist";
    p.folders={{{"folder"},"Folder",{}}};
    p.tracks={{{"audio"},"Audio",TrackKind::audio,Id{"folder"}},{{"instrument"},"Keys",TrackKind::instrument,{}},{{"bus"},"Bus",TrackKind::bus,{}}};
    p.tracks[0].output=Id{"bus"};p.tracks[1].sends={{{"bus"},.25f,true,true}};
    p.tracks[1].midi_input="SECRET_DEVICE_KEY";
    NativeInsert fx;fx.id={"plugin"};fx.kind=InsertKind::vst3;fx.plugin_path="C:/SECRET/plugin.vst3";
    fx.class_id="0123456789ABCDEF0123456789ABCDEF";fx.plugin_name="Synthetic";fx.parameters={{7,.5f}};
    fx.component_state={std::byte{0xA1},std::byte{0xB2}};p.tracks[1].inserts={fx};
    NativeInsert gain;gain.id={"native"};gain.gain=.5f;gain.class_id="SECRET_NATIVE_UNUSED";p.master_inserts={gain};
    p.clips={{{"wav"},{"audio"},"Audio clip",100,100,0,"C:/SECRET/audio.wav"},{{"midi"},{"instrument"},"MIDI clip",0,1,0,"",MidiClip{1920,1920,960,
        {{{"note"},480,960,60,100,0},{{"hidden"},2880,960,64,100,0}},{{{"cc"},960,2,0,1,64},{{"hiddencc"},2880,2,0,1,32}}}}};
    p.chords={{{"chord"},"C",0,1920},{{"chord2"},"G",1920,3840}};
    p.sections={{{"section"},"Verse",0,3840,0x4056D6}};
    p.markers={{{"marker"},"Start",0},{{"marker2"},"Middle",1920},{{"marker3"},"End",3840}};
    p.time.tempos={{0,120},{1920,90}};p.time.meters={{1,4,4},{3,6,8}};
    d.graph.id={"graph"};processing::NodeState node;node.id={"node"};node.processor_id="C:/SECRET/processor";
    node.parameters={{3,2.f}};node.plugin.class_id="SECRET_NATIVE_GRAPH_UNUSED";d.graph.nodes={node};d.graph.edges={{{},{"node"},1}};d.graph.outputs={{"node"}};
    d.midi={{{"port"},"SECRET_DEVICE", "Port",false,true}};
    d.patches={{{"patch"},d.graph}};d.section_patches={{{"section"},{"patch"}}};
    d.actions={{{"action"},{"marker"},{"port"},{0,processing::MidiKind::cc,0,1,64}}};
    d.live_notes="SECRET_LIVE_NOTES";d.extensions={{"SECR","SECRET_API_KEY"}};
    d.validate();return d;
}
struct ViewFixture {
    persistence::ProjectDocument document{fixture()};ProjectStore projects{document.project};processing::GraphStore graphs{document.graph};
    ContextKey key{document.project.id,Id{"session"},projects.state().revision,graphs.state().revision};
    std::shared_ptr<const ReadView> view(Selection selection={}){
        auto v=capture_view(projects.state(),graphs.state(),document,key,{},std::move(selection),{{},0,true});CHECK(v.error==Error::none && v.value);return v.value;
    }
};
Query query(QueryKind kind){Query q;q.kind=kind;return q;}
std::vector<Query> all_queries(){std::vector<Query> result;
    for(auto kind:{QueryKind::tracks,QueryKind::folders,QueryKind::clips,QueryKind::chords,QueryKind::sections,QueryKind::markers,
        QueryKind::tempos,QueryKind::meters,QueryKind::mixer,QueryKind::routing,QueryKind::plugins,QueryKind::graph_nodes,
        QueryKind::graph_edges,QueryKind::graph_outputs,QueryKind::midi_routes,QueryKind::midi_ports,QueryKind::live_bindings,QueryKind::live_actions,QueryKind::live_patches})result.push_back(query(kind));
    auto notes=query(QueryKind::midi_notes);notes.clip=Id{"midi"};result.push_back(notes);
    auto events=query(QueryKind::midi_events);events.clip=Id{"midi"};result.push_back(events);return result;
}
void analysis_test(){
    ViewFixture f;auto view=f.view();JobControl control{{},std::chrono::steady_clock::now()+3s,65536};const auto report=view->analysis(control);
    CHECK(report.find("\"visible_note_intersections\":1")!=std::string::npos&&report.find("\"note_activity_ticks\":480")!=std::string::npos&&report.find("\"pitch_min\":60")!=std::string::npos);
    CHECK(report.find("SECRET")==std::string::npos&&report.find("authored_gain_linear")!=std::string::npos&&report.find("unavailable_no_offline_render_requested")!=std::string::npos);
    std::stop_source stop;stop.request_stop();control.stop=stop.get_token();bool cancelled{};try{(void)view->analysis(control);}catch(...){cancelled=true;}CHECK(cancelled);
    desktop::Application app;app.new_project();auto a=app.ai_read_view();control.stop={};const auto empty=a.value->analysis(control);const auto track=app.add_instrument_track("Keys");app.create_midi_clip(track,0,ppq);const auto fresh=app.ai_read_view();CHECK(a.value->context().key!=fresh.value->context().key&&empty!=fresh.value->analysis(control));
}
void queries_test(){
    ViewFixture f;const auto view=f.view();const auto before=f.projects.state();
    for(const auto& q:all_queries()){const auto page=view->query(q);CHECK(page.error==Error::none && page.key==f.key && !page.has_more && page.json.size()<=q.byte_budget);}
    auto q=query(QueryKind::tracks);CHECK(view->query(q).returned==3);q.id=Id{"missing"};CHECK(view->query(q).error==Error::not_found);
    q.id=Id{"audio"};CHECK(view->query(q).returned==1);q.limit=0;CHECK(view->query(q).error==Error::invalid_input);
    q=query(QueryKind::plugin_parameters);q.plugin=Id{"plugin"};CHECK(view->query(q).error==Error::permission_denied);
    const auto parameters=view->query(q,{true});CHECK(parameters.returned==1 && parameters.json.find("normalized_value")!=std::string::npos);
    q.plugin=Id{"native"};auto native=view->query(q,{true});CHECK(native.returned==1 && native.json.find("linear")!=std::string::npos);
    q.plugin=Id{"missing"};CHECK(view->query(q,{true}).error==Error::not_found);
    q=query(QueryKind::plugins);q.plugin=Id{"missing"};CHECK(view->query(q).error==Error::not_found);
    q=query(QueryKind::automation);CHECK(view->query(q).error==Error::unsupported);
    for(const auto kind:{QueryKind::ara_capabilities,QueryKind::ara_binding,QueryKind::ara_status})CHECK(view->query(query(kind)).error==Error::unsupported);
    q=query(QueryKind::graph_parameters);q.plugin=Id{"node"};CHECK(view->query(q).error==Error::permission_denied);
    CHECK(view->query(q,{true}).returned==1);q.plugin=Id{"missing"};CHECK(view->query(q,{true}).error==Error::not_found);
    q=query(QueryKind::graph_nodes);q.selected_only=true;CHECK(view->query(q).error==Error::invalid_input);
    q=query(QueryKind::midi_notes);q.clip=Id{"wav"};CHECK(view->query(q).error==Error::unsupported);
    q.clip=Id{"midi"};auto notes=view->query(q);CHECK(notes.returned==1);CHECK(notes.json.find("\"project_start_ticks\":1920")!=std::string::npos);
    CHECK(notes.json.find("\"project_end_ticks\":2400")!=std::string::npos);CHECK(notes.json.find("hidden")==std::string::npos);
    q.ticks={{2400,2880}};CHECK(view->query(q).returned==0);q.ticks={{1920,2400}};CHECK(view->query(q).returned==1);
    q=query(QueryKind::midi_events);q.clip=Id{"midi"};CHECK(view->query(q).returned==1);q.ticks={{1921,2400}};CHECK(view->query(q).returned==0);
    q=query(QueryKind::chords);q.ticks={{1920,3840}};auto chords=view->query(q);CHECK(chords.returned==1 && chords.json.find("chord2")!=std::string::npos);
    q=query(QueryKind::markers);q.ticks={{1920,3840}};CHECK(view->query(q).returned==1);
    q=query(QueryKind::clips);q.samples=LoopRange{200,300};CHECK(view->query(q).returned==0);q.samples=LoopRange{100,200};CHECK(view->query(q).returned==1);
    auto selected_view=f.view({{{EntityKind::track,Id{"instrument"}}},{}});q=query(QueryKind::clips);q.selected_only=true;CHECK(selected_view->query(q).returned==1);
    auto range_view=f.view({{},LoopRange{100,200}});CHECK(range_view->query(q).returned==1);
    q=query(QueryKind::tempos);q.ticks={{1921,3840}};auto tempo=view->query(q);CHECK(tempo.returned==1 && tempo.json.find("\"bpm\":90")!=std::string::npos);
    CHECK(f.projects.state().revision==before.revision && *f.projects.state().project==*before.project);
    q=query(QueryKind::tracks);q.limit=1;q.scan_limit=1;std::size_t cursor{},count{};
    do{q.cursor=cursor;auto page=view->query(q);CHECK(page.scanned<=1 && page.returned<=1);count+=page.returned;cursor=page.next_cursor;if(!page.has_more)break;}while(true);CHECK(count==3);
    q.cursor=4;CHECK(view->query(q).error==Error::invalid_input);
}
void snapshot_test(){
    ViewFixture f;const auto view=f.view();const auto queries=all_queries();
    const auto first=view->snapshot(queries),second=view->snapshot(queries);
    CHECK(first.value && second.value && first.value->json==second.value->json && !first.value->truncated);
    CHECK(first.value->json.find("SECRET")==std::string::npos);
    for(const char* excluded:{"plugin_path","processor_id","component_state","controller_state","live_notes","device_key","\"source\":"})CHECK(first.value->json.find(excluded)==std::string::npos);
    CHECK(first.value->token_upper_bound==first.value->json.size());
    const auto small=view->snapshot(queries,{2048,2048});CHECK(small.value && small.value->truncated && small.value->json.size()<=2048);
    const auto token_limited=view->snapshot(queries,{8192,2048});CHECK(token_limited.value && token_limited.value->json.size()<=2048);
    CHECK(view->snapshot(queries,{512,512}).error==Error::invalid_input);
    for(const auto& p:first.value->pages)CHECK(p.key==f.key);
    auto large=f.document.project;large.tracks.clear();large.clips.clear();large.folders.clear();large.master_inserts.clear();
    for(int i=0;i<2000;++i)large.tracks.push_back({Id{"track-"+std::to_string(i)},"Name",TrackKind::audio,{}});
    ProjectStore projects(large);auto key=f.key;key.project_revision=projects.state().revision;
    auto big=capture_view(projects.state(),f.graphs.state(),f.document,key,{});CHECK(big.value);
    auto q=query(QueryKind::tracks);q.limit=256;q.scan_limit=256;q.byte_budget=4096;
    auto page=big.value->query(q);CHECK(page.error==Error::none && page.byte_limited && page.has_more && page.json.size()<=4096);
    const auto part=big.value->snapshot({q},{4096,4096});CHECK(part.value && part.value->truncated);
    q.id=Id{"track-1999"};q.scan_limit=5;page=big.value->query(q);CHECK(page.returned==0 && page.scanned==5 && page.next_cursor==5 && page.has_more);
    auto invalid=f.document.project;invalid.tracks[0].name=std::string(1,static_cast<char>(0xFF));ProjectStore invalid_store(invalid);
    auto invalid_view=capture_view(invalid_store.state(),f.graphs.state(),f.document,f.key,{});CHECK(invalid_view.value);
    CHECK(invalid_view.value->query(query(QueryKind::tracks)).error==Error::invalid_input);
    auto scoped=query(QueryKind::tracks);scoped.id=Id{"instrument"};CHECK(invalid_view.value->query(scoped).returned==1);
    f.projects.execute(RenameTrack({"audio"},"Changed"));CHECK(view->snapshot(queries).value->json==first.value->json);
    CHECK(capture_view(f.projects.state(),f.graphs.state(),f.document,f.key,{}).error==Error::stale_context);
}
void lifetime_test(){
    std::atomic<bool> started{},cancelled{};desktop::Application app;app.new_project();const auto track=app.add_audio_track("Original");
    const auto key=app.ai_context().value->key;auto view=app.ai_read_view();CHECK(view.value);
    auto snapshot=app.ai_snapshot(key,{query(QueryKind::tracks),query(QueryKind::tempos),query(QueryKind::meters)});CHECK(snapshot.value);
    const auto before=app.snapshot();const auto dirty=app.dirty();
    CHECK(app.ai_query(key,query(QueryKind::tracks)).returned==1);CHECK(app.snapshot()==before && app.dirty()==dirty);
    app.play();eventually([&]{return app.engine()->state().sample>0;});
    auto task=[&](const Request& r,const JobControl& control){CHECK(r.snapshot && r.snapshot->json.find("Original")!=std::string::npos);
        CHECK(r.context.observed_transport==r.snapshot->context.observed_transport);started=true;
        while(!control.should_stop())std::this_thread::sleep_for(1ms);cancelled=true;return "stale";};
    CHECK(app.request_ai({snapshot.value->context,"mock",2s,snapshot.value},task).error==Error::none);eventually([&]{return started.load();});
    app.stop();eventually([&]{app.poll();return app.services().transport->state().playback==PlaybackState::stopped;});
    app.rename_track(track,"Changed");eventually([&]{return cancelled.load();});CHECK(!app.take_ai_result());
    CHECK(app.ai_query(key,query(QueryKind::tracks)).error==Error::stale_context);
    CHECK(app.ai_snapshot(key,{query(QueryKind::tracks)}).error==Error::stale_context);
    CHECK(app.request_ai({snapshot.value->context,"stale",2s,snapshot.value},task).error==Error::stale_context);
    CHECK(view.value->query(query(QueryKind::tracks)).json.find("Original")!=std::string::npos);
    auto async=std::async(std::launch::async,[frozen=view.value]{return frozen->query(query(QueryKind::tracks));});
    app.rename_track(track,"Concurrent");CHECK(async.get().json.find("Original")!=std::string::npos);
    const auto changed=app.ai_context().value->key;CHECK(app.undo());CHECK(app.ai_query(changed,query(QueryKind::tracks)).error==Error::stale_context);
    const auto undone=app.ai_context().value->key;CHECK(app.redo());CHECK(app.ai_query(undone,query(QueryKind::tracks)).error==Error::stale_context);
    const auto old_view=app.ai_read_view();app.new_project();CHECK(old_view.value->query(query(QueryKind::tracks)).error==Error::permission_denied);
    auto wrong=std::async(std::launch::async,[&]{try{(void)app.ai_read_view();}catch(const std::logic_error&){return true;}return false;});CHECK(wrong.get());
    Worker worker({1,1024,1024});Request too_large;too_large.context=snapshot.value->context;too_large.snapshot=snapshot.value;
    CHECK(worker.submit(too_large,[](const Request&,const JobControl&){return "unused";}).error==Error::permission_denied);
    auto fresh=app.ai_snapshot(app.ai_context().value->key,{query(QueryKind::tracks),query(QueryKind::tempos),query(QueryKind::meters)});CHECK(fresh.value);
    too_large.context=fresh.value->context;too_large.snapshot=fresh.value;
    CHECK(worker.submit(too_large,[](const Request&,const JobControl&){return "unused";}).error==Error::invalid_input);
    auto graph_key=app.ai_context().value->key;auto graph=*app.graphs()->state().graph;graph.patch_name="Changed graph";app.graphs()->replace(graph);
    CHECK(app.ai_snapshot(graph_key,{query(QueryKind::graph_nodes)}).error==Error::stale_context);
}
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("suite required");const std::string suite=argv[1];
    if(suite=="queries")queries_test();else if(suite=="snapshot")snapshot_test();else if(suite=="lifetime")lifetime_test();else if(suite=="analysis")analysis_test();
    else if(suite=="export"){ViewFixture f;auto snapshot=f.view()->snapshot(all_queries());CHECK(snapshot.value);std::cout<<snapshot.value->json;return 0;}
    else throw std::runtime_error("unknown suite");std::cout<<"PASS AI "<<suite<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
