// Actual callback PCM checks, deliberately using boundaries inside a device block.
void performance(){
    const auto graph=[](std::uint64_t token,PerformancePlan plan,float value=0.25f){
        RenderGraph g;g.performance=std::move(plan);g.performance_token=token;
        auto asset=std::make_shared<const AudioData>(AudioData{48000,1,std::vector<float>(1024,value)});
        g.voices={{asset,0,0,1024,{{0,0,1}}}};return g;
    };
    std::array<float,32> out{};AudioEngine engine;
    auto process=[&](std::uint32_t frames){const auto allocations=allocation_check::count.load(),frees=allocation_check::frees.load();allocation_check::enabled=true;
        engine.process(nullptr,out.data(),frames);allocation_check::enabled=false;CHECK(allocation_check::count.load()==allocations);CHECK(allocation_check::frees.load()==frees);};
    PerformancePlan plan;plan.end=20;plan.sections={{3,7,PerformanceMode::skip},{9,13,PerformanceMode::stop}};
    engine.prepare({48000,0,1,32},graph(1,plan));CHECK(engine.enqueue({ControlKind::play}));process(16);
    CHECK(engine.state().sample==13&&engine.state().playback==PlaybackState::stopped);
    for(int i=0;i<9;++i)CHECK(std::abs(out[i]-.25f)<1e-6f);
    for(int i=9;i<16;++i)CHECK(out[i]==0);
    CHECK(engine.enqueue({ControlKind::seek,13}));CHECK(engine.enqueue({ControlKind::play}));process(16);
    CHECK(engine.state().sample==20&&engine.performance_status().action==PerformanceAction::next);
    plan.sections={{3,7,PerformanceMode::loop}};engine.prepare({48000,0,1,32},graph(2,plan));
    CHECK(engine.enqueue({ControlKind::play}));process(16);CHECK(engine.state().sample==4);
    CHECK(engine.enqueue({ControlKind::seek,7}));process(2);CHECK(engine.state().sample==9);
    plan.sections.clear();plan.end=5;engine.prepare({48000,0,1,32},graph(3,plan));
    std::weak_ptr<const AudioData> outgoing;
    {auto snapshot=engine.stopped_graph();outgoing=snapshot.voices.front().asset;}
    CHECK(engine.enqueue({ControlKind::play}));
    PerformancePlan next;next.end=11;next.mode=PerformanceMode::stop;
    auto prepared=engine.prepare_transition(graph(4,next,.5f),3);CHECK(engine.stage_transition(prepared));process(16);
    CHECK(engine.performance_status().token==4&&engine.state().sample==11&&engine.state().playback==PlaybackState::stopped);
    for(int i=0;i<5;++i)CHECK(std::abs(out[i]-.25f)<1e-6f);
    for(int i=5;i<16;++i)CHECK(std::abs(out[i]-.5f)<1e-6f);
    CHECK(engine.transition_phase(prepared)==AudioEngine::TransitionPhase::adopted);
    CHECK(!outgoing.expired());
    CHECK(engine.acknowledge_transition(prepared));CHECK(!outgoing.expired());prepared.reset();CHECK(outgoing.expired());
    // Preparation before Play remains valid; manual Stop itself cannot autoplay.
    engine.prepare({48000,0,1,32},graph(5,plan));
    prepared=engine.prepare_transition(graph(6,next),5);CHECK(engine.stage_transition(prepared));
    CHECK(engine.enqueue({ControlKind::play}));CHECK(engine.enqueue({ControlKind::stop}));process(2);
    CHECK(engine.performance_status().token==5&&engine.state().playback==PlaybackState::stopped);
    CHECK(engine.enqueue({ControlKind::play}));process(8);
    CHECK(engine.performance_status().token==6&&engine.state().sample==3);
    CHECK(engine.acknowledge_transition(prepared));prepared.reset();
    // Natural end can wait for late preparation, driven by the next device block.
    engine.prepare({48000,0,1,32},graph(12,plan));CHECK(engine.enqueue({ControlKind::play}));process(8);
    CHECK(engine.state().sample==5&&engine.state().playback==PlaybackState::stopped);
    prepared=engine.prepare_transition(graph(13,next),12);CHECK(engine.stage_transition(prepared));process(2);
    CHECK(engine.performance_status().token==13&&engine.state().sample==2&&engine.state().playback==PlaybackState::playing);
    CHECK(engine.acknowledge_transition(prepared));prepared.reset();
    // An item Stop advances to the next graph at zero and waits for an explicit Play.
    auto stopPlan=plan;stopPlan.mode=PerformanceMode::stop;
    engine.prepare({48000,0,1,32},graph(14,stopPlan));CHECK(engine.enqueue({ControlKind::play}));
    prepared=engine.prepare_transition(graph(15,next),14);CHECK(engine.stage_transition(prepared));process(8);
    CHECK(engine.performance_status().token==15&&engine.state().sample==0&&engine.state().playback==PlaybackState::stopped);
    for(int i=5;i<8;++i)CHECK(out[i]==0.f);
    CHECK(engine.acknowledge_transition(prepared));prepared.reset();
    CHECK(engine.enqueue({ControlKind::play}));process(2);CHECK(engine.state().sample==2);
    // Explicit Stop while waiting cancels a late automatic resume.
    engine.prepare({48000,0,1,32},graph(16,plan));CHECK(engine.enqueue({ControlKind::play}));process(8);
    CHECK(engine.enqueue({ControlKind::stop}));process(1);
    prepared=engine.prepare_transition(graph(17,next),16);CHECK(engine.stage_transition(prepared));process(2);
    CHECK(engine.performance_status().token==16&&engine.state().playback==PlaybackState::stopped);
    CHECK(engine.cancel_transition(prepared));prepared.reset();
    // A late successor of a Stop item is selected silently, not auto-started.
    engine.prepare({48000,0,1,32},graph(18,stopPlan));CHECK(engine.enqueue({ControlKind::play}));process(8);
    CHECK(engine.performance_status().action==PerformanceAction::next_stopped);
    prepared=engine.prepare_transition(graph(19,next),18);CHECK(engine.stage_transition(prepared));process(2);
    CHECK(engine.performance_status().token==19&&engine.state().sample==0&&engine.state().playback==PlaybackState::stopped);
    CHECK(out[0]==0&&out[1]==0);CHECK(engine.acknowledge_transition(prepared));prepared.reset();
    // Pause cancels a natural-end request just like manual Stop.
    engine.prepare({48000,0,1,32},graph(20,plan));CHECK(engine.enqueue({ControlKind::play}));process(8);
    CHECK(engine.enqueue({ControlKind::pause}));process(1);
    prepared=engine.prepare_transition(graph(21,next),20);CHECK(engine.stage_transition(prepared));process(2);
    CHECK(engine.performance_status().token==20&&engine.state().playback!=PlaybackState::playing);
    CHECK(engine.cancel_transition(prepared));prepared.reset();
    // Click switches with the graph, preserving the outgoing prefix of the device block.
    auto clickGraph=graph(8,next,0.f);clickGraph.click.playback=true;clickGraph.click.level=60;clickGraph.click.output_left=0;
    clickGraph.click_time.tempos={{0,90}};
    AudioEngine reference;reference.prepare({48000,0,1,32},clickGraph);CHECK(reference.enqueue({ControlKind::play}));
    std::array<float,32> expected{};reference.process(nullptr,expected.data(),11);
    engine.prepare({48000,0,1,32},graph(7,plan));CHECK(engine.enqueue({ControlKind::play}));
    prepared=engine.prepare_transition(std::move(clickGraph),7);CHECK(engine.stage_transition(prepared));process(16);
    for(int i=0;i<5;++i)CHECK(std::abs(out[i]-.25f)<1e-6f);
    for(int i=0;i<11;++i)CHECK(std::abs(out[i+5]-expected[i])<1e-6f);
    CHECK(engine.acknowledge_transition(prepared));prepared.reset();

    // Stop remains device-wide if it is clicked before the UI acknowledges a handoff.
    next.mode=PerformanceMode::normal;engine.prepare({48000,0,1,32},graph(9,plan));CHECK(engine.enqueue({ControlKind::play}));
    prepared=engine.prepare_transition(graph(10,next),9);CHECK(engine.stage_transition(prepared));process(8);
    CHECK(engine.state().playback==PlaybackState::playing&&engine.state().sample==3);
    CHECK(engine.enqueue({ControlKind::stop}));process(1);
    CHECK(engine.state().playback==PlaybackState::stopped&&engine.state().sample==3);
    CHECK(engine.acknowledge_transition(prepared));prepared.reset();

    // A held Stop resumes between beats; explicit seek establishes the click origin.
    plan.end=48000;auto heldClick=graph(11,plan,0.f);heldClick.click.playback=true;heldClick.click.output_left=0;
    engine.prepare({48000,0,1,32},std::move(heldClick));CHECK(engine.enqueue({ControlKind::play}));process(16);
    CHECK(std::any_of(out.begin(),out.begin()+16,[](float value){return std::abs(value)>.001f;}));
    CHECK(engine.enqueue({ControlKind::stop}));process(16);CHECK(engine.state().sample==16);
    CHECK(engine.enqueue({ControlKind::play}));process(16);
    CHECK(std::all_of(out.begin(),out.begin()+16,[](float value){return value==0.f;}));
    CHECK(engine.enqueue({ControlKind::seek,0}));process(16);
    CHECK(std::any_of(out.begin(),out.begin()+16,[](float value){return std::abs(value)>.001f;}));

}
