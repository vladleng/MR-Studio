#pragma once
#include <cstring>
#ifdef MRS_HAS_VST3

namespace ui::live::test {
// Real Live command path, two hosted VST3 instances and a native insert.
template<class Host,class Reconnect>
void subPatchRuntime(Host& host,const juce::String& channel,const juce::String& other,
                     const juce::File& folder,Reconnect reconnect) {
    const auto path=juce::SystemStats::getEnvironmentVariable("MRS_LIVE_TEST_VST3",{});
    if(path.isEmpty())return; // Fixture is supplied by CTest in VST3-enabled builds.
    const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    const auto plugins=mrs::processing::probe_vst3(path.toStdString());
    check(!plugins.empty(),"#374 runtime: real VST3 fixture is available");
    for(const auto& plugin:plugins)host.catalog.push_back(plugin);
    struct Module {
        HMODULE handle;
        ~Module(){if(handle)FreeLibrary(handle);}
    } module{LoadLibraryW(path.toWideCharPointer())};
    using Counter=unsigned(*)();using Toggle=void(*)(unsigned);
    const auto creations=reinterpret_cast<Counter>(GetProcAddress(module.handle,"mrsFixtureCreations"));
    const auto latencyChanges=reinterpret_cast<Toggle>(GetProcAddress(module.handle,"mrsFixtureLatencyChanges"));
    const auto rejectRestores=reinterpret_cast<Toggle>(GetProcAddress(module.handle,"mrsFixtureRejectRestores"));
    check(creations && latencyChanges && rejectRestores,"#374 runtime: fixture instrumentation is available");
    mrs::NativeInsert native;native.id={"runtime-native"};native.kind=mrs::InsertKind::gain;native.gain=.8f;
    mrs::NativeInsert first;first.id={"runtime-vst-a"};first.kind=mrs::InsertKind::vst3;
    first.plugin_path=path.toStdString();first.class_id=plugins.front().class_id;first.plugin_name=plugins.front().name;
    auto second=first;second.id={"runtime-vst-b"};
    auto otherNative=native;otherNative.id={"runtime-other-native"};otherNative.gain=.35f;
    host.setLiveInputEffects(other,{otherNative});
    host.setLiveInputEffects(channel,{native,first,second});
    host.captureLiveChannelPatch(channel,"Runtime settings","blue");
    const auto bank=live::find(host.liveDocument->interfaceState,"channels",channel)["patches"];
    const auto patchId=bank[bank.size()-1]["id"].toString();
    const auto defaultId=bank[bank.size()-1]["subPatches"][0]["id"].toString();
    host.loadLiveChannelPatch(channel,patchId,defaultId);
    host.editLiveInterface("Runtime Sub-patch fixtures",[&](auto& state){
        auto& patch=live::require(live::require(state,"channels",channel),"patches",patchId);
        auto base=live::decodeLivePatchInserts(state,patch);
        auto baseline=base;live::applyLivePatchParameterBaseline(baseline,patch["parameterBaseline"]);
        const auto add=[&](juce::var value,const char* id){
            live::put(value,"id",id);live::put(value,"name",id);live::put(value,"color","violet");
            live::array(patch,"subPatches").add(value);
        };
        const auto setGain=[](auto& fx,float value){
            for(auto& p:fx.parameters)if(p.id==100)p.value=value;
        };
        auto a=baseline;setGain(a[1],.75f);
        add(live::subPatchChain(state,baseline,a),"runtime-a");
        auto b=baseline;setGain(b[2],.25f);
        add(live::subPatchChain(state,baseline,b),"runtime-b");
        const auto bytes=[](float gain){std::vector<std::byte> result(sizeof(gain));std::memcpy(result.data(),&gain,sizeof(gain));return result;};
        auto snapshot=base;
        snapshot[1].component_state=snapshot[1].controller_state=bytes(.625f);
        snapshot[2].component_state=snapshot[2].controller_state=bytes(.875f);
        add(live::captureSubPatchVariant(state,base,snapshot,live::SubPatchCaptureMode::snapshot),"runtime-snapshot");
        snapshot[2].component_state={std::byte{0}};
        add(live::captureSubPatchVariant(state,base,snapshot,live::SubPatchCaptureMode::snapshot),"runtime-invalid");
    });
    const auto itemId=host.liveDocument->interfaceState["currentItem"].toString();
    host.editLiveInterface("Sub-patch Timeline fixture",[&](auto& state){
        for(auto& t:live::array(live::require(state,"items",itemId),"tracks"))
            if(t["channelId"].toString()==channel){live::put(t,"spans",live::object());live::put(t,"patchOrder",juce::var(juce::Array<juce::var>{}));}
    });
    host.addLivePatchBlock(channel,patchId,0.,itemId);
    const auto timeline=live::channelItemTrack(host.liveDocument->interfaceState,itemId,channel)["spans"];
    const auto segment=timeline[juce::Identifier(patchId)]["subs"].getDynamicObject()->getProperties().getName(0).toString();
    const auto choose=[&](const juce::String& id){
        const auto spans=live::channelItemTrack(host.liveDocument->interfaceState,itemId,channel)["spans"];
        const auto changed=live::replaceSubPatchSegment(spans,patchId,segment,id);
        host.applyLiveChannelSubPatch(channel,patchId,id,changed,itemId);
    };
    const auto track=*host.liveInputTrack(channel);
    struct Window {
        HWND handle=CreateWindowW(L"STATIC",L"Sub-patch editor test",WS_OVERLAPPED,0,0,400,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        ~Window(){if(handle)DestroyWindow(handle);}
    } window;
    int width{},height{};
    check(host.app.open_plugin_editor(track,first.id,window.handle,width,height),"#374 runtime: fixture editor opens");
    const auto child=GetWindow(window.handle,GW_CHILD);
    check(child && IsWindow(child),"#374 runtime: editor HWND exists before switching");
    const auto generation=host.app.insert_generation();
    const auto instanceCount=creations();
    const auto otherBefore=host.captureLiveChannelInserts(other,false,true);
    const auto verify=[&](float a,float b){
        const auto inserts=host.captureLiveChannelInserts(channel,true,true);
        const auto gain=[](const auto& fx){for(const auto& p:fx.parameters)if(p.id==100)return p.value;return -1.f;};
        check(inserts.size()==3 && std::abs(gain(inserts[1])-a)<1.e-5f && std::abs(gain(inserts[2])-b)<1.e-5f,
              "#374 runtime: complete target values, not accumulated sparse overrides");
        check(host.app.insert_generation()==generation && creations()==instanceCount && IsWindow(child) && GetWindow(window.handle,GW_CHILD)==child,
              "#374 runtime: instances and open editor survive Sub-patch switching");
        check(host.captureLiveChannelInserts(other,false,true)==otherBefore,"#374 runtime: other channel unchanged");
    };
    for(const auto& id:juce::StringArray{"runtime-a","runtime-b",defaultId,"runtime-snapshot",defaultId}) {
        choose(id);
        verify(id=="runtime-a"?.75f:id=="runtime-snapshot"?.625f:.5f,
               id=="runtime-b"?.25f:id=="runtime-snapshot"?.875f:.5f);
    }
    choose("runtime-a");
    host.app.set_plugin_parameter(track,first.id,100,.9f);
    choose("runtime-a");verify(.75f,.5f);
    choose("runtime-b");verify(.5f,.25f);
    host.undoLiveInterface(false);verify(.75f,.5f);
    host.undoLiveInterface(true);verify(.5f,.25f);
    const auto showBefore=live::canonical(host.liveDocument->interfaceState);
    bool rejected=false;
    try{choose("runtime-invalid");}catch(const std::exception&){rejected=true;}
    check(rejected && live::canonical(host.liveDocument->interfaceState)==showBefore,
          "#374 runtime: failed second VST3 state leaves show unchanged");
    verify(.5f,.25f);
    latencyChanges(1);rejected=false;
    try{choose("runtime-snapshot");}catch(const std::exception&){rejected=true;}
    latencyChanges(0);
    check(rejected && live::canonical(host.liveDocument->interfaceState)==showBefore,
          "#374 runtime: changed VST3 latency rejects without committing show");
    verify(.5f,.25f);
    const auto loaded=host.liveLoadedPatches.at(channel);
    host.liveLoadedPatches.erase(channel);rejected=false;
    try{host.applyLiveChannelSubPatch(channel,patchId,"runtime-a");}catch(const std::exception&){rejected=true;}
    check(rejected,"#374 runtime: unknown parent refuses Sub-patch without a full-load fallback");
    host.liveLoadedPatches[channel]=loaded;verify(.5f,.25f);
    std::array<float,256> input{},output{};
    host.app.play();host.app.engine()->process(input.data(),output.data(),128);
    rejected=false;
    try{host.applyLiveChannelSubPatch(channel,patchId,"runtime-a");}catch(const std::exception&){rejected=true;}
    check(rejected,"#374 runtime: Play refuses stopped-only Sub-patch state restoration");
    host.app.stop();host.app.engine()->process(input.data(),output.data(),128);
    host.app.close_plugin_editor(track,first.id);
    const auto saved=folder.getChildFile("runtime-subpatch-settings.mrlive");
    host.saveFile(saved);
    const auto document=ui::LiveDocument::read(saved);
    live::validate(document.interfaceState);
    const auto persisted=live::find(live::find(document.interfaceState,"channels",channel),"patches",patchId);
    check(live::find(persisted,"subPatches","runtime-snapshot").isObject(),"#374 runtime: Snapshot survives Save/Reopen");
    host.openFile(saved);reconnect(host);
    host.loadLiveChannelPatch(channel,patchId,"runtime-b");
    const auto reopened=host.captureLiveChannelInserts(channel,true,true);
    const auto readGain=[](const auto& fx){for(const auto& p:fx.parameters)if(p.id==100)return p.value;return -1.f;};
    check(reopened.size()==3 && std::abs(readGain(reopened[1])-.5f)<1.e-5f &&
          std::abs(readGain(reopened[2])-.25f)<1.e-5f,
          "#374 runtime: reopened show applies its saved Sub-patch sound");
    host.app.arm_track(*host.liveInputTrack(channel));
    host.app.start_recording(std::filesystem::path(folder.getChildFile("subpatch-guard.wav").getFullPathName().toStdString()));
    host.app.engine()->process(input.data(),output.data(),128);rejected=false;
    try{host.applyLiveChannelSubPatch(channel,patchId,"runtime-a");}catch(const std::exception&){rejected=true;}
    check(rejected,"#374 runtime: recording refuses Sub-patch state restoration");
    host.app.stop_recording();host.app.stop();host.app.engine()->process(input.data(),output.data(),128);
    rejectRestores(1);rejected=false;bool explicitRollbackFailure=false;
    try{host.loadLiveChannelPatch(channel,patchId,"runtime-snapshot");}
    catch(const std::exception& error){rejected=true;explicitRollbackFailure=std::string(error.what()).find("rollback failed")!=std::string::npos;}
    rejectRestores(0);
    check(rejected && explicitRollbackFailure && host.app.device_status().phase==mrs::audio::DevicePhase::stopped,
          "#374 runtime: failed rollback keeps audio stopped with an explicit error");
    (void)host.captureLiveChannelInserts(channel);
    host.saveFile(saved);
    check(host.app.device_status().phase==mrs::audio::DevicePhase::stopped,
          "#374 runtime: Capture/Save cannot restart audio after failed rollback");
}
}
#endif
