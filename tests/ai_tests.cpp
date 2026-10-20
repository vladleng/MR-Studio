#include <mrs/desktop.hpp>
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mrs;
using namespace mrs::ai;
using namespace std::chrono_literals;
void check(bool value,const char* expression){if(!value)throw std::runtime_error(expression);}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
template<class F> void eventually(F f) {
    const auto until=std::chrono::steady_clock::now()+3s;
    while(std::chrono::steady_clock::now()<until){if(f())return;std::this_thread::sleep_for(1ms);}
    throw std::runtime_error("AI test timed out");
}
Context context() {
    Project p;p.id=new_id();p.title="Empty";
    ProjectStore store(p);const auto state=store.state();
    auto r=make_context(state,{p.id,new_id(),state.revision,0},{});
    CHECK(r.error==Error::none && r.value);return *r.value;
}
Request request(){return {context(),"prompt",3000ms};}
Result result(Worker& worker) {
    std::optional<Result> r;eventually([&]{r=worker.take();return r.has_value();});return std::move(*r);
}
void context_test() {
    auto c=context();CHECK(c.version==1 && c.tracks==0 && c.clips==0 && c.ticks_per_quarter==960);
    CHECK(c.supports(Capability::summary) && !c.supports(Capability::tools) && c.supports(Capability::entity_queries) && c.supports(Capability::snapshots));
    CHECK(c.seconds(48000)==1.0);
    Project p;p.id=new_id();p.title="Song";p.artist="Artist";
    p.time.tempos={{0,120},{8*ppq,90}};p.time.meters={{1,4,4},{3,6,8}};
    const auto track=new_id();p.tracks={{track,"Audio",TrackKind::audio,{}}};
    ProjectStore store(p);const auto state=store.state();const ContextKey key{p.id,new_id(),state.revision,7};
    Selection selection{{{EntityKind::track,track}},LoopRange{0,48000}};
    auto r=make_context(state,key,{},selection);CHECK(r.value && r.value->time==p.time && r.value->tracks==1);
    CHECK(r.value->key.graph_revision==7 && r.value->selection.entities[0].id==track);
    CHECK(store.state().revision==state.revision && *store.state().project==p);
    Timeline timeline(r.value->time,r.value->sample_rate);
    CHECK(timeline.to_ticks(timeline.to_samples(8*ppq))==8*ppq);
    CHECK(timeline.musical_position(8*ppq).bar==3);
    selection.entities.push_back(selection.entities.front());CHECK(make_context(state,key,{},selection).error==Error::invalid_input);
    selection.entities={{{EntityKind::clip},{"missing"}}};CHECK(make_context(state,key,{},selection).error==Error::invalid_input);
    selection.entities.clear();selection.sample_range=LoopRange{20,10};CHECK(make_context(state,key,{},selection).error==Error::invalid_input);
    auto stale=key;++stale.project_revision;CHECK(make_context(state,stale,{}).error==Error::stale_context);
    store.execute(RenameTrack(track,"Changed"));
    CHECK(r.value->title=="Song" && r.value->time==p.time); // detached value survives edits
    CHECK(make_context(store.state(),key,{}).error==Error::stale_context);
}
void worker_test() {
    Worker worker({2,16,32});std::atomic<bool> running{};std::atomic<int> calls{};
    const auto owner=std::this_thread::get_id();
    auto slow=[&](const Request&,const JobControl& control){CHECK(std::this_thread::get_id()!=owner);++calls;running=true;
        while(!control.should_stop())std::this_thread::sleep_for(1ms);
        return "late";};
    auto first=worker.submit(request(),slow);CHECK(first.error==Error::none);eventually([&]{return running.load();});
    auto second=worker.submit(request(),[&](const Request&,const JobControl&){++calls;return "queued";});CHECK(second.error==Error::none);
    CHECK(worker.submit(request(),slow).error==Error::busy);
    CHECK(worker.cancel(second.request_id));CHECK(worker.cancel(first.request_id));
    auto a=result(worker);auto b=result(worker);CHECK(a.error==Error::cancelled && b.error==Error::cancelled && calls==1);
    auto timed=request();timed.timeout=10ms;CHECK(worker.submit(timed,slow).error==Error::none);
    CHECK(result(worker).error==Error::timeout);
    auto huge=request();huge.input=std::string(17,'x');CHECK(worker.submit(huge,slow).error==Error::invalid_input);
    auto future=request();future.context.version=2;CHECK(worker.submit(future,slow).error==Error::unsupported);
    auto invalid=request();invalid.timeout=0ms;CHECK(worker.submit(invalid,slow).error==Error::invalid_input);
    auto oversized=worker.submit(request(),[](const Request&,const JobControl&){return std::string(33,'x');});CHECK(oversized.error==Error::none);
    auto over=result(worker);CHECK(over.error==Error::response_too_large && over.output.empty());
    CHECK(worker.submit(request(),[](const Request&,const JobControl&)->std::string{throw std::runtime_error("malformed backend response");}).error==Error::none);
    CHECK(result(worker).error==Error::backend_failure);
    // Completed results still count against queue capacity, even without UI polling.
    std::atomic<int> completed{};
    auto fast=[&](const Request&,const JobControl&){++completed;return "ok";};
    CHECK(worker.submit(request(),fast).error==Error::none);CHECK(worker.submit(request(),fast).error==Error::none);
    eventually([&]{return completed==2;});CHECK(worker.submit(request(),fast).error==Error::busy);
    CHECK(result(worker).output=="ok");CHECK(result(worker).output=="ok");
    auto completed_job=worker.submit(request(),fast);eventually([&]{return completed==3;});
    CHECK(worker.cancel(completed_job.request_id));CHECK(result(worker).error==Error::cancelled);
    // Deadlines include time spent queued; an expired job never invokes its backend.
    running=false;auto blocker=worker.submit(request(),slow);eventually([&]{return running.load();});
    auto expired=request();expired.timeout=1ms;const auto before=calls.load();
    CHECK(worker.submit(expired,[&](const Request&,const JobControl&){++calls;return "expired";}).error==Error::none);
    std::this_thread::sleep_for(5ms);CHECK(worker.cancel(blocker.request_id));
    CHECK(result(worker).error==Error::cancelled);CHECK(result(worker).error==Error::timeout);CHECK(calls==before);
    // Stop callbacks may call the queue API; cancellation must not hold its mutex.
    running=false;
    auto callback_task=[&](const Request&,const JobControl& control){
        std::stop_callback callback(control.stop,[&]{CHECK(worker.cancel(99999)==false);});
        running=true;while(!control.should_stop())std::this_thread::sleep_for(1ms);return "cancelled";};
    auto callback_job=worker.submit(request(),callback_task);eventually([&]{return running.load();});
    CHECK(worker.cancel(callback_job.request_id));CHECK(result(worker).error==Error::cancelled);
    for(int i=0;i<100;++i){CHECK(worker.submit(request(),fast).error==Error::none);CHECK(result(worker).output=="ok");}
    running=false;CHECK(worker.submit(request(),slow).error==Error::none);eventually([&]{return running.load();});
    auto start=std::chrono::steady_clock::now();worker.shutdown();CHECK(std::chrono::steady_clock::now()-start<1s);
    CHECK(worker.submit(request(),fast).error==Error::shutdown);worker.shutdown();
}
void application_test() {
    std::atomic<bool> running{};
    desktop::Application app;auto initial=app.ai_context();CHECK(initial.value);
    auto future=*initial.value;future.version=2;
    CHECK(app.request_ai({future,"future",2s},[](const Request&,const JobControl&){return "unused";}).error==Error::unsupported);
    for(auto workspace:{desktop::Workspace::arrange,desktop::Workspace::edit,desktop::Workspace::mix,desktop::Workspace::live}) {
        app.workspace(workspace);CHECK(app.ai_context().value->key==initial.value->key);
    }
    const auto slow=[&](const Request&,const JobControl& control){running=true;
        while(!control.should_stop())std::this_thread::sleep_for(1ms);
        return "late";};
    app.play();eventually([&]{return app.engine()->state().sample>0;});
    auto submitted=app.request_ai({*initial.value,"mock slow request",2s},slow);CHECK(submitted.error==Error::none);
    eventually([&]{return running.load();});const auto sample=app.engine()->state().sample;
    eventually([&]{return app.engine()->state().sample>sample+4096;}); // callback advances without UI/network wait
    CHECK(app.cancel_ai(submitted.request_id));
    std::optional<Result> reply;eventually([&]{reply=app.take_ai_result();return reply.has_value();});
    CHECK(reply->error==Error::cancelled && reply->output.empty());app.stop();
    // Same demo identity and revision reopened: session key still changes.
    running=false;submitted=app.request_ai({*app.ai_context().value,"old",2s},slow);
    eventually([&]{return running.load();});app.demo();
    auto reopened=app.ai_context();CHECK(reopened.value->key.project==initial.value->key.project);
    CHECK(reopened.value->key.project_revision==initial.value->key.project_revision);
    CHECK(reopened.value->key.session!=initial.value->key.session);
    CHECK(app.request_ai({*initial.value,"stale",2s},slow).error==Error::stale_context);
    CHECK(app.request_ai({*reopened.value,"new",2s},[](const Request&,const JobControl&){return "current";}).error==Error::none);
    eventually([&]{reply=app.take_ai_result();return reply.has_value();});CHECK(reply->output=="current");
    auto key=app.ai_context().value->key;
    auto changed=app.request_ai({*app.ai_context().value,"edit meanwhile",2s},[](const Request&,const JobControl&){return "old revision";});
    CHECK(changed.error==Error::none);app.rename_track(app.services().projects->state().project->tracks.front().id,"AI test");
    CHECK(app.ai_context().value->key!=key);
    CHECK(app.request_ai({*app.ai_context().value,"next",2s},[](const Request&,const JobControl&){return "new revision";}).error==Error::none);
    eventually([&]{reply=app.take_ai_result();return reply.has_value();});CHECK(reply->output=="new revision");
    bool rejected=std::async(std::launch::async,[&]{try{(void)app.ai_context();}catch(const std::logic_error&){return true;}return false;}).get();CHECK(rejected);
    app.new_project();CHECK(app.ai_context().value->tracks==0);
    running=false;CHECK(app.request_ai({*app.ai_context().value,"shutdown",2s},slow).error==Error::none);
    eventually([&]{return running.load();}); // destructor cancels and joins with captured data still alive
}
}
int main(int argc,char** argv) {
    try{if(argc!=2)throw std::runtime_error("suite required");const std::string suite=argv[1];
        if(suite=="context")context_test();else if(suite=="worker")worker_test();else if(suite=="application")application_test();
        else throw std::runtime_error("unknown suite");
    std::cout<<"PASS AI "<<suite<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
