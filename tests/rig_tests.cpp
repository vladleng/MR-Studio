#include <mrs/rig.hpp>
#include <mrs/processing.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mrs;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void rejects(F f){bool threw{};try{f();}catch(const std::exception&){threw=true;}check(threw,"Invalid Rig accepted");}
int main(){try{
    NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::rig;fx.rig.name="Warm Jazz";fx.rig.input_gain=.5f;fx.rig.output_gain=2;fx.gain=.75f;fx.ir.mix=.8f;fx.ir.low_cut=80;fx.ir.high_cut=12000;fx.ir.invert=true;fx.bypass=true;
    fx.reverb.name="External capture";fx.reverb.first={"capture.wav","C:/fixture/capture.wav","fnv1a64:0000000000000001",1000};fx.reverb.sample_rate=48000;fx.reverb.channels=1;fx.reverb.frames=9600;fx.cab_metadata={{"ir_role","full_combo"},{"tone_model_id","test-capture"}};fx.cab_variant_id="capture";fx.rig.artwork="C:/fixture/own.png";fx.rig.artwork_rights="Synthetic fixture";
    RigPreset preset{new_id(),fx};preset.state.preset_name=preset.id.value;preset.validate();
    check(deserialize_rig_preset(serialize_rig_preset(preset))==preset,"RigPreset exact roundtrip");
    check(deserialize_rig_library(serialize_rig_library({preset}))==std::vector<RigPreset>{preset},"Rig library exact roundtrip");
    auto project=demo_project();project.tracks.front().inserts={preset.state};check(deserialize(serialize(project))==project,"Project RigState roundtrip");
    for(const auto& d:rig_modules){processing::NodeState n;n.processor_id=d.type_id;auto processor=processing::native_factory(n);const auto infos=processor->parameters();check(infos.size()==d.parameters.size(),"Descriptor matches existing native DSP");for(const auto& p:d.parameters){const auto it=std::find_if(infos.begin(),infos.end(),[&](const auto& v){return v.id==p.id;});check(it!=infos.end()&&it->minimum==p.minimum&&it->maximum==p.maximum&&it->initial==p.initial,"Descriptor range/default matches native DSP");}}
    auto services=demo_services();services.projects=std::make_shared<ProjectStore>(project);auto changed=preset.state;changed.rig.input_gain=1;changed.bypass=false;services.projects->execute(SetInserts{project.tracks.front().id,{changed}});check(services.projects->undo()&&services.projects->state().project->tracks.front().inserts.front()==preset.state,"Rig Undo restores every durable parameter");check(services.projects->redo()&&services.projects->state().project->tracks.front().inserts.front()==changed,"Rig Redo restores every durable parameter");
    const auto graph=processing::insert_graph(std::array{preset.state});check(graph.nodes.front().processor_id=="mrs.rig"&&graph.nodes.front().parameters.size()==7+rig_lane_capacity*2*rig_slot_fields,"Single Rig node with real input/core/output and bounded rack snapshot");auto dsp=processing::native_factory(graph.nodes.front());dsp->prepare({48000,2,128});dsp->restore(graph.nodes.front().plugin);check(dsp->capture().class_id=="mrs.rig"&&dsp->latency()==0,"Distinct processor identity, neutral latency");
    auto chain=preset;chain.state.rig.pre_fx={make_rig_module("mrs.gain"),make_rig_module("mrs.highpass"),make_rig_module("mrs.compressor"),make_rig_module("mrs.drive")};chain.state.rig.post_fx={make_rig_module("mrs.eq"),make_rig_module("mrs.lowpass")};
    chain.state.rig.pre_fx[0].parameters[0].value=.25f;chain.state.rig.post_fx[1].bypass=true;
    check(deserialize_rig_preset(serialize_rig_preset(chain))==chain,"Ordered chain exact preset roundtrip");
    RigModule unknown;unknown.id=new_id();unknown.type_id="future.delay";unknown.version=99;unknown.parameters={{512,.8f}};unknown.opaque_state="recoverable bytes";chain.state.rig.post_fx.push_back(unknown);
    check(!rig_module_descriptor(unknown)&&deserialize_rig_preset(serialize_rig_preset(chain))==chain,"Unknown type/version/state retained as unsupported placeholder");
    auto invalid=chain;invalid.state.rig.pre_fx.push_back(invalid.state.rig.post_fx.front());invalid.state.rig.post_fx.pop_back();rejects([&]{invalid.validate();});
    invalid=chain;invalid.state.rig.pre_fx[0].parameters[0].value=5;rejects([&]{invalid.validate();});
    invalid=chain;invalid.state.rig.pre_fx.resize(9);rejects([&]{invalid.validate();});
    std::vector<NativeInsert> eight(8,chain.state);for(auto& n:eight)n.id=new_id();auto graph8=processing::insert_graph(eight);processing::PreparedGraph prepared({std::make_shared<const processing::GraphState>(graph8),0,false,false},{48000,2,128});check(prepared.enqueue_parameters(graph8),"Eight complete Rig snapshots fit one coherent host transaction");
    // Force every field to differ across all eight fully occupied racks.
    for(auto& rig:eight)for(auto* lane:{&rig.rig.pre_fx,&rig.rig.post_fx}){lane->clear();for(std::size_t slot=0;slot<rig_lane_capacity;++slot){auto m=make_rig_module("mrs.compressor");for(const auto& info:rig_compressor_parameters)m.parameters[info.id].value=info.maximum;lane->push_back(m);}}
    const auto full8=processing::insert_graph(eight);check(prepared.enqueue_parameters(full8),"Eight 16-pedal complete snapshots fit coherent 1024-change transaction");
    auto foreign=fx;foreign.kind=InsertKind::gain;foreign.cab_metadata.clear();rejects([&]{foreign.validate();});
    auto mismatch=preset;mismatch.state.preset_name="another-preset";rejects([&]{serialize_rig_preset(mismatch);});
    auto bad=preset;bad.state.rig.version=3;rejects([&]{serialize_rig_preset(bad);});bad=preset;bad.state.rig.pre_fx={RigModule{}};rejects([&]{serialize_rig_preset(bad);});bad=preset;bad.state.rig.output_gain=std::numeric_limits<float>::infinity();rejects([&]{serialize_rig_preset(bad);});bad=preset;bad.state.ir.samples={.5f};rejects([&]{serialize_rig_preset(bad);});bad=preset;bad.state.reverb={};rejects([&]{serialize_rig_preset(bad);});bad=preset;bad.state.rig.artwork_rights.clear();rejects([&]{serialize_rig_preset(bad);});rejects([&]{serialize_rig_library({preset,preset});});rejects([&]{deserialize_rig_library("MRS_RIG_LIBRARY 1 129");});rejects([&]{deserialize_rig_preset("MRS_RIG_PRESET 2 bad");});
    std::cout<<"Rig schema/preset/library/project/Undo/processor validation PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
