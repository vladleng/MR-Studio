// Included in reverb_library_tests.cpp's fixture namespace.
void restore_references(){
    Fixture f;auto original=f.root/"Original",relocated=f.root/"Relocated";
    wav(original/"quad.wav",4);wav(original/"pair L.wav",2);wav(original/"pair R.wav",2);
    const auto index=index_reverb_library(original);
    for(const auto& entry:index.entries){
        auto first_graph=graph();ReverbLoader first(first_graph,{"reverb"},{48000,2,128});first.select(entry,original,ReverbQuadMap::ll_lr_rl_rr);
        wait([&]{return first.snapshot()->phase==ReverbLoadPhase::ready;});first.activate_quiescent();
        const auto saved=reverb_selection_state(*first.snapshot()->active);check(!saved.first.digest.empty(),"external state identity");first.shutdown();
        fs::create_directories(relocated);fs::copy_file(entry.first,relocated/entry.first.filename(),fs::copy_options::overwrite_existing);if(!entry.second.empty())fs::copy_file(entry.second,relocated/entry.second.filename(),fs::copy_options::overwrite_existing);
        auto restored_graph=graph();ReverbLoader restored(restored_graph,{"reverb"},{48000,2,128});restored.restore(saved,relocated);
        wait([&]{return restored.snapshot()->phase!=ReverbLoadPhase::loading;});check(restored.snapshot()->phase==ReverbLoadPhase::ready,"quad/pair relative restore");restored.activate_quiescent();
        check(restored.snapshot()->active->first.absolute.parent_path()==relocated,"relative root first");check(restored_graph->capture().nodes.front().plugin.class_id=="mr_reverb","native capture after external load");
        restored.restore(saved,f.root/"MissingRoot");wait([&]{return restored.snapshot()->phase!=ReverbLoadPhase::loading;});check(restored.snapshot()->phase==ReverbLoadPhase::ready,"absolute fallback with missing root");restored.activate_quiescent();
        fs::remove(entry.first);if(!entry.second.empty())fs::remove(entry.second);wav(relocated/entry.first.filename(),entry.kind==ReverbFileKind::pair?2:4,48000,513);
        restored.restore(saved,relocated);wait([&]{return restored.snapshot()->phase!=ReverbLoadPhase::loading;});check(restored.snapshot()->phase==ReverbLoadPhase::error&&restored.snapshot()->active.has_value(),"changed fingerprint preserves previous kernel");
        fs::remove(relocated/entry.first.filename());restored.restore(saved,relocated);wait([&]{return restored.snapshot()->phase!=ReverbLoadPhase::loading;});check(restored.snapshot()->phase==ReverbLoadPhase::missing,"missing quad/pair explicit");restored.shutdown();rejects([&]{restored.select(entry,original);});
    }
    std::cout<<"Quad/pair state, relocated relative path, absolute fallback, changed/missing identity and retired loader PASS\n";
}
