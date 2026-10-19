// Included in desktop_tests.cpp fixture namespace.
void cab_combo(){
    Directory dir;auto root=dir.path/"IR";std::filesystem::create_directory(root);
    for(auto rate:{44100u,96000u}){std::vector<float> samples(static_cast<std::size_t>(rate)*3/2);samples[0]=.5f;samples[1]=.25f;audio::Recorder writer(root/(rate==44100?"Combo_441.wav":"Combo.wav"),rate,0,{0,1});writer.capture(samples.data(),2,rate*3/4,0);writer.finish();}
    Application app;app.new_project(48000);auto track=app.add_audio_track("Combo");app.set_track_input(track,0);app.set_track_monitoring(track,true);NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::cab_collection;fx.ir.mix=.7f;app.set_inserts(track,{fx});
    auto device=std::make_unique<ManualDevice>();auto* driver=device.get();app.connect(std::move(device),{0,48000,128,{0},{0,1}});app.set_cab_root(root);eventually([&]{return app.cab_library()->snapshot()->assets.size()==2;});
    CHECK(serialize(*app.services().projects->state().project).find("CAB_SELECTION 1")!=std::string::npos);
    auto loader=app.reverb_loader(track,fx.id);loader->select(audio::index_reverb_library(root).entries.front(),root);eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready;});app.poll();
    CHECK(app.capture_insert(track,fx.id).cab_metadata.at("ir_role")=="unknown");const auto generation=app.insert_generation();
    app.play();std::array<float,128> input{};input.fill(.8f);std::array<float,256> output{};for(int i=0;i<200;++i)app.engine()->process(input.data(),output.data(),128);const auto wet=output.back();
    std::map<std::string,std::string> metadata{{"ir_role","full_combo"},{"amp_make","Synthetic"},{"amp_model","Combo"},{"capture_notes","Unknown mic"},{"provenance","user"}};
    const auto rev=app.services().projects->state().revision;app.map_cab_assets(track,fx.id,{"Combo.wav","Combo_441.wav"},metadata,"_441");
    CHECK(app.services().projects->state().revision==rev+1);CHECK(app.insert_generation()==generation&&driver->opens==1);
    const auto saved=app.capture_insert(track,fx.id);CHECK(saved.cab_metadata.at("ir_role")=="full_combo"&&saved.ir.mix==.7f);CHECK(saved.reverb.frames==72000||saved.reverb.frames==33075);
    for(int i=0;i<100;++i)app.engine()->process(input.data(),output.data(),128);CHECK(std::abs(output.back()-wet)<1e-5f);
    CHECK(app.undo()&&app.capture_insert(track,fx.id).cab_metadata.at("ir_role")=="unknown");CHECK(app.redo()&&app.capture_insert(track,fx.id).cab_metadata==saved.cab_metadata);CHECK(app.insert_generation()==generation);
    app.stop();app.engine()->process(input.data(),output.data(),128);eventually([&]{return app.cab_library()->snapshot()->overrides.size()==2;});
    auto manifest=audio::serialize_cab_manifest(*app.cab_library()->snapshot());auto snapshot=serialize(*app.services().projects->state().project);CHECK(snapshot.find("CAB_SELECTION 2")!=std::string::npos);CHECK(serialize(deserialize(snapshot))==snapshot);
    auto invalid=saved;invalid.cab_metadata["ir_role"]="nonlinear";rejects([&]{invalid.validate();});invalid=saved;invalid.cab_metadata["notes"]=std::string(4097,'x');rejects([&]{invalid.validate();});
    auto file=dir.path/"Combo.mrsproject";app.save_project(file);CHECK(persistence::load_project(file).project.tracks.front().inserts.front()==saved);
    auto moved=dir.path/"Moved";std::filesystem::rename(root,moved);app.set_cab_root(moved,manifest);app.open_project(file);loader=app.reverb_loader(track,fx.id,true);eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready;});CHECK(app.capture_insert(track,fx.id)==saved);
    app.load_insert_preset(track,saved);loader=app.reverb_loader(track,fx.id,true);eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready;});CHECK(app.capture_insert(track,fx.id).cab_metadata==saved.cab_metadata);
    std::filesystem::remove(moved/saved.reverb.first.relative);app.open_project(file);loader=app.reverb_loader(track,fx.id,true);eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::missing;});CHECK(app.capture_insert(track,fx.id)==saved);app.save_project(file);CHECK(persistence::load_project(file).project.tracks.front().inserts.front().cab_metadata==saved.cab_metadata);app.disconnect();
}
void cab_combo_real(const std::filesystem::path& root){
    for(const auto rate:{44100u,48000u,96000u}){
        Application app;app.new_project(rate);auto track=app.add_audio_track("Polytone audit");app.set_track_input(track,0);app.set_track_monitoring(track,true);NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::cab_collection;app.set_inserts(track,{fx});app.connect(std::make_unique<ManualDevice>(),{0,rate,128,{0},{0,1}});app.set_cab_root(root);
        eventually([&]{return app.cab_library()->snapshot()->assets.size()==20;});std::vector<std::string> paths;for(const auto& a:app.cab_library()->snapshot()->assets){CHECK(a.error.empty());paths.push_back(a.relative);}app.map_cab_assets(track,fx.id,paths,{{"ir_role","full_combo"},{"amp_make","Polytone"},{"amp_model","Mini Brute"},{"provenance","supplier PDF + user profile"}},"_441");eventually([&]{return app.cab_library()->snapshot()->overrides.size()==20;});
        auto m=app.cab_library()->snapshot();const auto* a=audio::select_cab_asset(*m,m->assets.front().variant_id,m->assets.front().duration_ms(),rate);CHECK(a);if(rate!=48000)CHECK(a->rate==rate);
        ReverbState expected;expected.name=a->relative;expected.sample_rate=a->rate;expected.channels=a->channels;expected.frames=a->frames;expected.first={a->relative,{},a->digest,a->bytes};auto loader=app.reverb_loader(track,fx.id);loader->restore(expected,root);
        eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready;});app.poll();auto selected=app.capture_insert(track,fx.id);CHECK(selected.cab_metadata.at("ir_role")=="full_combo"&&selected.reverb.frames==a->frames&&selected.reverb.sample_rate==a->rate&&selected.ir.samples.empty());
        app.play();std::array<float,128> input{};input.fill(.001f);std::array<float,256> output{};for(int i=0;i<400;++i){app.engine()->process(input.data(),output.data(),128);for(float s:output)CHECK(std::isfinite(s));}app.stop();app.disconnect();
    }
    std::cout<<"real Polytone Application load/process 44.1/48/96k PASS (listening not tested)\n";
}
void cab_collection(){
    Directory dir;auto root=dir.path/"IR";std::filesystem::create_directory(root);
    std::array<float,128> impulse{};impulse[0]=.5f;
    {audio::Recorder writer(root/"Test Hi-Gn 121+57 Celestion.wav",48000,0,{0});writer.capture(impulse.data(),1,128,0);writer.finish();}
    {audio::Recorder writer(root/"Test Hi-Gn 421+57 Celestion.wav",48000,0,{0});impulse[0]=.25f;writer.capture(impulse.data(),1,128,0);writer.finish();}
    for(const auto rate:{44100u,48000u,96000u}){
        Application app;app.new_project(rate);auto track=app.add_audio_track("Cab");app.set_track_input(track,0);app.set_track_monitoring(track,true);
        NativeInsert effect;effect.id=new_id();effect.kind=InsertKind::cab_collection;app.set_inserts(track,{effect});
        auto device=std::make_unique<ManualDevice>();auto* driver=device.get();app.connect(std::move(device),{0,rate,128,{0},{0,1}});app.set_cab_root(root);
        eventually([&]{return !app.cab_library()->snapshot()->assets.empty();});
        const auto entries=audio::index_reverb_library(root).entries;auto loader=app.reverb_loader(track,effect.id);loader->select(entries.front(),root);
        eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready;});app.poll();
        auto saved=app.capture_insert(track,effect.id);CHECK(!saved.cab_variant_id.empty()&&!saved.reverb.first.digest.empty());CHECK(saved.ir.samples.empty());
        CHECK(serialize(deserialize(serialize(*app.services().projects->state().project)))==serialize(*app.services().projects->state().project));
        loader->select(entries.back(),root);eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready&&loader->snapshot()->active->entry.key==entries.back().key;});app.poll();
        CHECK(app.undo());CHECK(app.capture_insert(track,effect.id).reverb.first.digest==saved.reverb.first.digest);CHECK(app.redo());
        loader=app.reverb_loader(track,effect.id,true);eventually([&]{app.poll();return loader->snapshot()->phase==audio::ReverbLoadPhase::ready;});
        const auto opens=driver->opens;app.play();std::array<float,128> input{};input.fill(.8f);std::array<float,256> output{};
        for(int i=0;i<200;++i)app.engine()->process(input.data(),output.data(),128);CHECK(std::isfinite(output.back())&&std::abs(output.back())>.01f);
        auto controls=app.capture_insert(track,effect.id);controls.ir.mix=0;app.set_inserts(track,{controls});for(int i=0;i<200;++i)app.engine()->process(input.data(),output.data(),128);CHECK(std::abs(output.back()-.4f)<.001f);CHECK(driver->opens==opens);
        CHECK(app.undo());for(int i=0;i<200;++i)app.engine()->process(input.data(),output.data(),128);CHECK(std::abs(output.back()-.4f)>.01f);app.stop();
        auto reference=app.capture_insert(track,effect.id).reverb;reference.first.digest="fnv1a64:0000000000000000";loader->restore(reference,root);eventually([&]{return loader->snapshot()->phase==audio::ReverbLoadPhase::error;});CHECK(loader->snapshot()->active.has_value());
        app.disconnect();CHECK(!app.reverb_loader(track,effect.id));
    }
}
