#include <mrs/cab_collection.hpp>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <set>
#include <algorithm>
using namespace mrs::audio;
namespace fs=std::filesystem;
void check(bool b){if(!b)throw std::runtime_error("Cab collection assertion failed");}
void wav(const fs::path& p,unsigned frames,unsigned channels=1,unsigned rate=48000){fs::create_directories(p.parent_path());std::ofstream o(p,std::ios::binary);auto u16=[&](unsigned v){for(int i=0;i<2;++i)o.put(static_cast<char>(v>>(8*i)));};auto u32=[&](unsigned v){for(int i=0;i<4;++i)o.put(static_cast<char>(v>>(8*i)));};o.write("RIFF",4);u32(36+frames*channels*2);o.write("WAVEfmt ",8);u32(16);u16(1);u16(channels);u32(rate);u32(rate*channels*2);u16(channels*2);u16(16);o.write("data",4);u32(frames*channels*2);for(unsigned j=0;j<frames*channels;++j)u16(j==0?10000:0);}
int main(int argc,char** argv){try{
    if(argc==3&&std::string_view(argv[1])=="--combo"){
        auto m=scan_cab_collection(fs::path(std::u8string(reinterpret_cast<const char8_t*>(argv[2]))),"combo-audit");check(m.assets.size()==20);
        std::vector<std::string> paths;for(const auto& a:m.assets){check(a.error.empty()&&a.channels==2&&a.bits==24&&a.duration_ms()>600&&a.duration_ms()<900&&cab_ir_role(a.mapping)=="unknown");paths.push_back(a.relative);}
        map_cab_assets(m,paths,{{"ir_role","full_combo"},{"amp_make","Polytone"},{"amp_model","Mini Brute"},{"provenance","supplier PDF + explicit user profile"}},"_441");
        std::set<std::string> variants;for(const auto& a:m.assets){variants.insert(a.variant_id);check(cab_ir_role(a.mapping)=="full_combo");check(select_cab_asset(m,a.variant_id,a.duration_ms(),44100)->rate==44100);check(select_cab_asset(m,a.variant_id,a.duration_ms(),96000)->rate==96000);}
        check(variants.size()==10);check(scan_cab_collection(m.root,m.collection_id,&m).overrides==m.overrides);std::cout<<"real Polytone: 20 WAV / 10 rate-paired captures PASS\n";return 0;
    }
    if(argc==2){auto m=scan_cab_collection(fs::path(std::u8string(reinterpret_cast<const char8_t*>(argv[1]))),"local-celestion");check(m.assets.size()==68);for(const auto& a:m.assets)check(a.error.empty()&&a.mapping.classified);std::cout<<"real library: 68 classified WAV\n";return 0;}
    auto root=fs::temp_directory_path()/("mrs-cab-"+mrs::new_id().value);fs::create_directories(root);
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{root};
    const std::string name="Test SM57 Dark 2 Celestion.wav";
    wav(root/"200ms"/name,9600);wav(root/"500ms"/name,24000);wav(root/"rate"/name,8820,1,44100);
    wav(root/"Room TLM 107 RoomL Celestion.wav",9600);wav(root/"Test Hi-Gn All+Room Stereo Celestion.wav",24000,2);wav(root/"unknown.wav",9600);
    auto m=scan_cab_collection(root,"test");check(m.assets.size()==6);auto n=scan_cab_collection(root,"test");check(serialize_cab_manifest(m)==serialize_cab_manifest(n));
    check(verify_cab_asset(m,m.assets.front()));
    auto moved=root/"relocated";fs::create_directories(moved);for(const auto& a:m.assets){fs::create_directories((moved/fs::path(a.relative)).parent_path());fs::copy_file(root/fs::path(a.relative),moved/fs::path(a.relative));}
    auto relink=m;relink.root=moved;check(verify_cab_asset(relink,m.assets.front()));fs::remove_all(moved);
    wav(root/fs::path(m.assets.front().relative),9601);check(!verify_cab_asset(m,m.assets.front()));wav(root/fs::path(m.assets.front().relative),9600);
    auto r=deserialize_cab_manifest(serialize_cab_manifest(m));check(serialize_cab_manifest(r)==serialize_cab_manifest(m));
    const auto dark=parse_celestion(name,1);check(dark.classified&&dark.position=="dark 2");check(parse_celestion("Test SM57 Dark Celestion.wav",1).position!=dark.position);
    check(parse_celestion("Test hi gn all+room stereo CELESTION.WAV",2).room=="included");check(!parse_celestion("unknown.wav",1).classified);check(!parse_celestion("Test Hi-Gn All+Room Stereo Celestion.wav",1).classified);
    std::string variant;for(const auto& a:m.assets)if(a.mapping.position=="dark 2")variant=a.variant_id;
    check(select_cab_asset(m,variant,200,44100)->rate==44100);check(select_cab_asset(m,variant,500,44100)->rate==48000);check(!select_cab_asset(m,variant,300,48000));
    auto manual=dark;manual.cabinet="manual cabinet";manual.tags["custom axis"]="value";m.overrides["unknown.wav"]=manual;n=scan_cab_collection(root,"test",&m);check(n.overrides.size()==1);r=deserialize_cab_manifest(serialize_cab_manifest(n));check(r.overrides==n.overrides);
    check(cab_ir_role(dark)=="cabinet");check(cab_ir_role(parse_celestion("Fender amp.wav",2))=="unknown");
    wav(root/"Combo.wav",72000,2,96000);wav(root/"Combo_441.wav",33075,2,44100);
    auto combo=scan_cab_collection(root,"test");const auto originalId=combo.assets.front().variant_id;
    map_cab_assets(combo,{combo.assets.front().relative},{{"ir_role","cabinet"},{"capture_notes","Edited description"}});check(combo.assets.front().variant_id==originalId);
    map_cab_assets(combo,{"Combo.wav","Combo_441.wav"},{{"ir_role","full_combo"},{"amp_model","Manual combo"},{"capture_notes","Linear capture"}},"_441");
    const auto& first=*std::find_if(combo.assets.begin(),combo.assets.end(),[](const auto& a){return a.relative=="Combo.wav";});
    check(select_cab_asset(combo,first.variant_id,750,44100)->relative=="Combo_441.wav");check(select_cab_asset(combo,first.variant_id,750,96000)->relative=="Combo.wav");
    const auto mapped=deserialize_cab_manifest(serialize_cab_manifest(combo));check(mapped.overrides==combo.overrides);auto rescanned=scan_cab_collection(root,"test",&mapped);check(rescanned.overrides==mapped.overrides);
    const auto stableVariant=first.variant_id;map_cab_assets(combo,{"Combo.wav"},{{"ir_role","unknown"}});for(const auto& a:combo.assets)if(a.relative=="Combo.wav")check(a.variant_id==stableVariant);
    bool badRole=false;try{map_cab_assets(combo,{"Combo.wav"},{{"ir_role","amp"}});}catch(...){badRole=true;}check(badRole);
    fs::remove(root/"Combo.wav");rescanned=scan_cab_collection(root,"test",&mapped);for(const auto& a:rescanned.assets)if(a.relative=="Combo.wav")check(!a.present&&cab_ir_role(a.mapping)=="full_combo");fs::remove(root/"Combo_441.wav");
    wav(root/"duplicate"/name,9600);n=scan_cab_collection(root,"test");unsigned duplicates=0;for(const auto& a:n.assets)if(a.error.find("Ambiguous")!=std::string::npos)++duplicates;check(duplicates==2);fs::remove_all(root/"duplicate");
    wav(root/fs::path(std::u8string(u8"Кабинеты"))/"unknown.wav",9600);n=scan_cab_collection(root,"test");check(n.assets.size()==7);fs::remove_all(root/fs::path(std::u8string(u8"Кабинеты")));
    fs::remove(root/"200ms"/name);n=scan_cab_collection(root,"test",&n);bool missing=false;for(const auto& a:n.assets)missing|=!a.present;check(missing);
    std::stop_source stop;stop.request_stop();bool rejected=false;try{scan_cab_collection(root,"test",nullptr,stop.get_token());}catch(...){rejected=true;}check(rejected);
    CabCollection async;async.scan(root/"absent","test");async.scan(root,"test");for(int i=0;i<10000&&async.snapshot()->revision<2;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(async.snapshot()->revision==2&&async.snapshot()->error.empty());
    auto edit=*async.snapshot();const auto pathA=edit.assets.front().relative,pathB=edit.assets.back().relative;
    map_cab_assets(edit,{pathA},{{"ir_role","full_combo"}});async.publish_mapping(edit);
    rejected=false;try{async.publish_mapping(edit);}catch(...){rejected=true;}check(rejected);
    edit=*async.snapshot();map_cab_assets(edit,{pathB},{{"ir_role","unknown"}});async.publish_mapping(edit);check(async.snapshot()->overrides.contains(pathA)&&async.snapshot()->overrides.contains(pathB));
    rejected=false;try{deserialize_cab_manifest("MRS_CAB_COLLECTION 2 \"test\" 0 0");}catch(...){rejected=true;}check(rejected);
    std::cout<<"Cab manifest/parser/selector/override/missing/async PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
