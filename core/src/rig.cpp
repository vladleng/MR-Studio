#include <mrs/rig.hpp>
#include <iomanip>
#include <sstream>
#include <set>
namespace mrs {
namespace {
void check(bool value,const char* why){if(!value)throw std::invalid_argument(why);}
constexpr std::size_t budget=16*1024*1024;
}
void RigPreset::validate() const {
    check(!id.value.empty()&&id.value.size()<=256&&id.value.find_first_of("\r\n")==std::string::npos&&id.value.find('\0')==std::string::npos,"Invalid RigPreset identity");
    state.validate();
    check(state.kind==InsertKind::rig&&!state.reverb.first.digest.empty(),"RigPreset needs an external WAV core");
    check(state.preset_name==id.value,"RigPreset snapshot identity mismatch");
    check(!state.rig.name.empty(),"RigPreset needs a name");
}
std::string serialize_rig_preset(const RigPreset& preset){
    preset.validate();auto project=demo_project();project.clips.clear();project.tracks.resize(1);project.tracks.front().inserts={preset.state};project.master_inserts.clear();
    std::ostringstream out;out<<"MRS_RIG_PRESET 1 "<<std::quoted(preset.id.value)<<'\n'<<serialize(project);
    auto result=out.str();check(result.size()<=budget,"RigPreset exceeds size limit");return result;
}
RigPreset deserialize_rig_preset(std::string_view bytes){
    check(bytes.size()<=budget,"RigPreset exceeds size limit");std::istringstream in{std::string(bytes)};std::string magic;unsigned version{};RigPreset result;
    in>>magic>>version>>std::quoted(result.id.value);check(bool(in)&&magic=="MRS_RIG_PRESET"&&version==1,"Unsupported RigPreset");
    std::string rest{std::istreambuf_iterator<char>(in),{}};auto p=deserialize(rest);
    check(p.tracks.size()==1&&p.tracks.front().inserts.size()==1&&p.clips.empty()&&p.master_inserts.empty(),"Invalid RigPreset contents");
    result.state=p.tracks.front().inserts.front();result.validate();return result;
}
std::string serialize_rig_library(const std::vector<RigPreset>& presets){
    check(presets.size()<=128,"Too many saved Rigs");std::set<std::string> ids;std::ostringstream out;out<<"MRS_RIG_LIBRARY 1 "<<presets.size()<<'\n';
    for(const auto& p:presets){check(ids.insert(p.id.value).second,"Duplicate RigPreset identity");out<<std::quoted(serialize_rig_preset(p))<<'\n';check(out.tellp()<=static_cast<std::streamoff>(budget),"Rig library exceeds size limit");}return out.str();
}
std::vector<RigPreset> deserialize_rig_library(std::string_view bytes){
    check(bytes.size()<=budget,"Rig library exceeds size limit");std::istringstream in{std::string(bytes)};std::string magic;unsigned version{};std::size_t count{};
    in>>magic>>version>>count;check(bool(in)&&magic=="MRS_RIG_LIBRARY"&&version==1&&count<=128,"Unsupported rig library");std::vector<RigPreset> result;
    for(std::size_t i=0;i<count;++i){std::string item;in>>std::quoted(item);check(bool(in),"Truncated Rig library");result.push_back(deserialize_rig_preset(item));}
    in>>std::ws;check(in.eof(),"Trailing Rig library data");(void)serialize_rig_library(result);return result;
}
}
