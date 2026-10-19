// Included in desktop_tests.cpp's fixture namespace.
void reverb() {
    Directory dir;const auto root=dir.path/"IR",moved=dir.path/"Moved";std::filesystem::create_directory(root);
    const auto file=root/"Hall.wav";std::array<float,128> impulse{};impulse[0]=.5f;
    {audio::Recorder writer(file,48000,0,{0});writer.capture(impulse.data(),1,128,0);writer.finish();}
    {audio::Recorder writer(root/"Hall2.wav",48000,0,{0});impulse[0]=.25f;writer.capture(impulse.data(),1,128,0);writer.finish();impulse[0]=.5f;}
    Application app;app.new_project(48000);const auto track=app.add_audio_track("Reverb"),bus=app.add_bus("Return");app.set_track_input(track,0);app.set_track_monitoring(track,true);
    NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::reverb;app.set_inserts(track,{fx});auto master=fx;master.id=new_id();app.set_inserts(std::nullopt,{master});auto returnfx=fx;returnfx.id=new_id();returnfx.reverb.mix=1;app.set_inserts(bus,{returnfx});
    auto device=std::make_unique<ManualDevice>();auto* driver=device.get();app.connect(std::move(device),{0,48000,128,{0},{0,1}});app.set_reverb_root(root);
    eventually([&]{return app.reverb_library()->snapshot()->phase==audio::ReverbLibraryPhase::ready;});const auto entry=app.reverb_library()->snapshot()->entries.front();
    auto loader=app.reverb_loader(track,fx.id);loader->select(entry,root);eventually([&]{return loader->snapshot()->phase==audio::ReverbLoadPhase::ready;});
    const auto second=app.reverb_library()->snapshot()->entries.back();loader->select(second,root);
    eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready&&loader->snapshot()->active->entry.name=="Hall2";});
    loader->select(entry,root);eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready&&loader->snapshot()->active->entry.name=="Hall";});app.poll();
    fx=app.capture_insert(track,fx.id);CHECK(fx.reverb.name=="Hall"&&fx.reverb.first.relative=="Hall.wav"&&driver->opens==1);
    CHECK(app.undo());CHECK(app.capture_insert(track,fx.id).reverb.name=="Hall2");CHECK(app.redo());(void)app.reverb_loader(track,fx.id,true);eventually([&]{app.poll();return app.reverb_loader(track,fx.id)->snapshot()->phase==audio::ReverbLoadPhase::ready;});
    const auto generation=app.insert_generation();std::array<float,128> input{};input.fill(.8f);std::array<float,256> out{};
    app.play();for(int i=0;i<50;++i)app.engine()->process(input.data(),out.data(),128);CHECK(std::abs(out.back()-.35f)<.001f); // foundation legacy master gain is 0.5
    fx.reverb.mix=1;app.set_inserts(track,{fx});for(int i=0;i<50;++i)app.engine()->process(input.data(),out.data(),128);CHECK(std::abs(out.back()-.2f)<.001f);
    CHECK(app.undo());app.engine()->process(input.data(),out.data(),128);CHECK(app.redo());app.set_plugin_parameter(track,fx.id,0,20);app.set_plugin_parameter(track,fx.id,1,3);CHECK(app.insert_generation()==generation&&driver->opens==1);
    rejects([&]{app.set_plugin_parameter(track,fx.id,2,2);});app.stop();app.engine()->process(input.data(),out.data(),128);fx=app.capture_insert(track,fx.id);
    app.set_plugin_parameter(track,fx.id,3,1);app.set_plugin_parameter(track,fx.id,4,-75.3f);
    CHECK(app.insert_generation()==generation&&driver->opens==1);
    CHECK(app.undo()&&app.capture_insert(track,fx.id).reverb.gate_threshold==-90.f);CHECK(app.redo());
    rejects([&]{app.set_plugin_parameter(track,fx.id,3,.5f);});rejects([&]{app.set_plugin_parameter(track,fx.id,4,-59.f);});
    fx=app.capture_insert(track,fx.id);CHECK(fx.reverb.gate_enabled&&fx.reverb.gate_threshold==-75.3f);
    const auto project=dir.path/"Reverb.mrsproject";app.save_project(project);auto persisted=persistence::load_project(project);CHECK(persisted.project.tracks.front().inserts.front()==fx);
    std::filesystem::rename(root,moved);app.set_reverb_root(moved);app.open_project(project);(void)app.reverb_loader(track,fx.id,true);eventually([&]{app.poll();return app.reverb_loader(track,fx.id)->snapshot()->phase==audio::ReverbLoadPhase::ready;});CHECK(app.capture_insert(track,fx.id)==fx);
    app.load_insert_preset(track,fx);eventually([&]{app.poll();return app.reverb_loader(track,fx.id)->snapshot()->phase==audio::ReverbLoadPhase::ready;});
    std::filesystem::remove(moved/"Hall.wav");app.open_project(project);(void)app.reverb_loader(track,fx.id,true);eventually([&]{app.poll();return app.reverb_loader(track,fx.id)->snapshot()->phase==audio::ReverbLoadPhase::missing;});CHECK(app.capture_insert(track,fx.id)==fx);
    app.save_project(project);CHECK(persistence::load_project(project).project.tracks.front().inserts.front()==fx);
    {audio::Recorder writer(moved/"Hall.wav",48000,0,{0});impulse[0]=.25f;writer.capture(impulse.data(),1,128,0);writer.finish();}
    app.open_project(project);(void)app.reverb_loader(track,fx.id,true);eventually([&]{app.poll();return app.reverb_loader(track,fx.id)->snapshot()->phase==audio::ReverbLoadPhase::error;});CHECK(app.capture_insert(track,fx.id)==fx);
    loader->shutdown();app.disconnect();CHECK(!app.reverb_loader(track,fx.id));
    // One 48 kHz external asset is restored by Application in projects at all
    // supported rates. Device mismatch must reject before replacing the engine.
    for(const auto rate:{44100u,48000u,96000u}) {
        app.new_project(rate);const auto target=app.add_audio_track("Rate transition");
        app.set_track_input(target,0);app.set_track_monitoring(target,true);
        NativeInsert effect;effect.id=new_id();effect.kind=InsertKind::reverb;effect.reverb.mix=1;
        app.set_inserts(target,{effect});app.set_reverb_root(moved);
        eventually([&]{return app.reverb_library()->snapshot()->phase==audio::ReverbLibraryPhase::ready;});
        const auto chosen=app.reverb_library()->snapshot()->entries.front();
        auto rate_loader=app.reverb_loader(target,effect.id);rate_loader->select(chosen,moved);
        eventually([&]{app.poll();return rate_loader->snapshot()->phase==audio::ReverbLoadPhase::ready;});
        app.connect(std::make_unique<ManualDevice>(),{0,rate,128,{0},{0,1}});
        app.play();for(int n=0;n<100;++n)app.engine()->process(input.data(),out.data(),128);
        CHECK(std::isfinite(out.back())&&std::abs(out.back())>.01f);
        const auto captured=app.capture_insert(target,effect.id);
        const auto saved=dir.path/("Rate-"+std::to_string(rate)+".mrsproject");app.stop();app.engine()->process(input.data(),out.data(),128);app.save_project(saved);app.open_project(saved);
        auto restored=app.reverb_loader(target,effect.id,true);
        eventually([&]{app.poll();return restored->snapshot()->phase==audio::ReverbLoadPhase::ready;});
        CHECK(app.capture_insert(target,effect.id)==captured);
        rejects([&]{app.connect(std::make_unique<ManualDevice>(),{0,rate==48000?44100u:48000u,128,{0},{0,1}});});
        CHECK(app.engine()->config().sample_rate==rate);app.disconnect();
    }
}
