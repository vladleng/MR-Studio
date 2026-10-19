#include <mrs/cab_collection.hpp>
#include <mrs/audio.hpp>
#include <algorithm>
#include <condition_variable>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <thread>

namespace mrs::audio {
namespace {
namespace fs=std::filesystem;
std::string utf8(const fs::path& p){auto s=p.generic_u8string();return {s.begin(),s.end()};}
void cancel(std::stop_token s){if(s.stop_requested())throw std::runtime_error("Cab scan cancelled");}
std::string hash(std::string_view s){std::uint64_t h=14695981039346656037ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}std::ostringstream o;o<<std::hex<<h;return o.str();}
bool safe(std::string_view s){auto p=fs::path(std::u8string(s.begin(),s.end()));return !s.empty()&&!p.is_absolute()&&!p.has_root_name()&&std::none_of(p.begin(),p.end(),[](const auto& v){return v=="..";});}
std::string fingerprint(const fs::path& p,std::stop_token stop){
    const auto bytes=fs::file_size(p);const auto stamp=fs::last_write_time(p);
    if(bytes>128*1024*1024)throw std::runtime_error("Cab file exceeds byte budget");
    std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing Cab WAV");
    std::array<char,65536> buffer{};std::uint64_t h=14695981039346656037ULL,count{};
    while(f){cancel(stop);f.read(buffer.data(),buffer.size());for(std::streamsize j=0;j<f.gcount();++j){h^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(j)]);h*=1099511628211ULL;}count+=static_cast<std::uint64_t>(f.gcount());}
    if(!f.eof()||count!=bytes||bytes!=fs::file_size(p)||stamp!=fs::last_write_time(p))throw std::runtime_error("Cab WAV changed during scan");
    std::ostringstream o;o<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<h;return o.str();
}
bool capture_metadata(std::string_view key){return key=="ir_role"||key=="vendor"||key=="amp_make"||key=="amp_model"||key=="amp_type"||key=="capture_variant"||key=="capture_group"||key=="capture_notes"||key=="captured_chain"||key=="cabinet_included"||key=="room_included"||key=="provenance";}
std::string semantic(const CabMapping& m){std::ostringstream o;for(const auto* s:{&m.cabinet,&m.mic,&m.position,&m.mix,&m.voicing,&m.room,&m.channel_mode})o<<std::quoted(*s);o<<m.premixed;for(const auto& [k,v]:m.tags)if(!capture_metadata(k))o<<std::quoted(k)<<std::quoted(v);return o.str();}
std::string variant_id(const CabManifest& catalog,const CabAsset& a){
    if(const auto it=a.mapping.tags.find("capture_group");it!=a.mapping.tags.end()&&!it->second.empty())
        return "variant:"+hash(catalog.collection_id+"/capture/"+it->second+"/"+a.mapping.channel_mode);
    return "variant:"+hash(catalog.collection_id+"/"+(a.mapping.classified?semantic(a.mapping):a.relative));
}
void validate_duplicates(CabManifest& catalog){
    std::map<std::string,std::size_t> counts;
    const auto key=[](const CabAsset& a){return a.variant_id+"/"+std::to_string(a.rate)+"/"+std::to_string(a.frames);};
    for(auto& a:catalog.assets){if(a.error=="Ambiguous duplicate variant/rate/length")a.error.clear();if(a.present&&a.error.empty())++counts[key(a)];}
    for(auto& a:catalog.assets)if(a.present&&a.error.empty()&&counts[key(a)]>1)a.error="Ambiguous duplicate variant/rate/length";
}
void mapping_out(std::ostream& o,const CabMapping& m){o<<std::quoted(m.cabinet)<<' '<<std::quoted(m.mic)<<' '<<std::quoted(m.position)<<' '<<std::quoted(m.mix)<<' '<<std::quoted(m.voicing)<<' '<<std::quoted(m.room)<<' '<<std::quoted(m.channel_mode)<<' '<<std::quoted(m.source)<<' '<<std::quoted(m.confidence)<<' '<<m.premixed<<' '<<m.classified<<' '<<m.tags.size();for(const auto& [k,v]:m.tags)o<<' '<<std::quoted(k)<<' '<<std::quoted(v);}
void mapping_in(std::istream& i,CabMapping& m){i>>std::quoted(m.cabinet)>>std::quoted(m.mic)>>std::quoted(m.position)>>std::quoted(m.mix)>>std::quoted(m.voicing)>>std::quoted(m.room)>>std::quoted(m.channel_mode)>>std::quoted(m.source)>>std::quoted(m.confidence)>>m.premixed>>m.classified;std::size_t n{};i>>n;if(n>128)throw std::invalid_argument("Too many Cab tags");for(std::size_t j=0;j<n;++j){std::string k,v;i>>std::quoted(k)>>std::quoted(v);if(!i||!m.tags.emplace(k,v).second)throw std::invalid_argument("Invalid Cab tags");}validate_cab_metadata(m.tags);}
}
CabMapping parse_celestion(std::string_view filename,std::uint32_t channels){
    CabMapping m;m.channel_mode=channels==1?"mono":channels==2?"stereo":"unsupported";
    std::string name(filename);std::replace(name.begin(),name.end(),'_', ' ');
    std::smatch match;
    static const std::regex suffix(R"(^(.+) Celestion\.wav$)",std::regex::icase);
    if(!std::regex_match(name,match,suffix))return m;
    name=match[1];
    static const std::regex factory(R"(^(.+)[ ]+(Hi[- ]Gn|Lo[- ]Gn)[ ]+(121\+57|421\+121|421\+57|All|All\+Room Mono|All\+Room Stereo)$)",std::regex::icase);
    static const std::regex mic(R"(^(.+)[ ]+(R-121|MD421|SM57)[ ]+(Balanced|Bright|Dark 2|Dark|Fat|Thin|Rear)$)",std::regex::icase);
    static const std::regex room(R"(^(.+)[ ]+TLM 107 (Room Stereo|RoomL|RoomR)$)",std::regex::icase);
    if(std::regex_match(name,match,factory)){m.cabinet=match[1];m.voicing=match[2];m.mix=match[3];m.premixed=true;m.room=m.mix.find('+')!=std::string::npos&&m.mix.find("Room")!=std::string::npos?"included":"none";}
    else if(std::regex_match(name,match,mic)){m.cabinet=match[1];m.mic=match[2];m.position=match[3];m.room="none";}
    else if(std::regex_match(name,match,room)){m.cabinet=match[1];m.mic="TLM 107";m.position=match[2];m.room="component";}
    else return m;
    // Fold recognized ASCII tokens so case variants group consistently; keep raw path separately.
    for(auto* s:{&m.cabinet,&m.mic,&m.position,&m.mix,&m.voicing})for(auto& c:*s)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);
    std::replace(m.voicing.begin(),m.voicing.end(),' ','-');
    if(m.mix.find("room")!=std::string::npos)m.room="included";
    if((m.mix.ends_with("stereo")&&channels!=2)||(m.mix.ends_with("mono")&&channels!=1)||(m.position=="room stereo"&&channels!=2)||((m.position=="rooml"||m.position=="roomr")&&channels!=1))return CabMapping{};
    m.classified=true;m.confidence="exact-token";m.source="filename:celestion-v1";return m;
}
std::string cab_ir_role(const CabMapping& m){
    if(const auto it=m.tags.find("ir_role");it!=m.tags.end())return it->second;
    return m.classified&&!m.cabinet.empty()?"cabinet":"unknown";
}
std::map<std::string,std::string> cab_asset_metadata(const CabAsset& a){
    auto metadata=a.mapping.tags;metadata["ir_role"]=cab_ir_role(a.mapping);
    metadata["provenance"]=a.mapping.tags.contains("provenance")?a.mapping.tags.at("provenance"):a.mapping.source;
    metadata["confidence"]=a.mapping.confidence;
    if(!a.mapping.cabinet.empty())metadata["cabinet"]=a.mapping.cabinet;
    validate_cab_metadata(metadata);return metadata;
}
void map_cab_assets(CabManifest& catalog,const std::vector<std::string>& paths,const std::map<std::string,std::string>& metadata,std::string_view suffix){
    validate_cab_metadata(metadata);
    if(suffix.size()>128||suffix.find_first_of("/\\")!=std::string_view::npos)throw std::invalid_argument("Invalid rate suffix");
    auto candidate=catalog;
    for(const auto& path:paths){
        auto it=std::find_if(candidate.assets.begin(),candidate.assets.end(),[&](const auto& a){return a.relative==path;});
        if(it==candidate.assets.end())throw std::invalid_argument("Unknown Cab mapping asset");
        auto& m=it->mapping;
        for(const auto& [k,v]:metadata)m.tags[k]=v;
        // Only explicit suffix input groups source rates. Parent folders remain independent.
        if(!suffix.empty()){
            const auto p=fs::path(std::u8string(path.begin(),path.end()));auto stem=utf8(p.stem());
            if(stem.ends_with(suffix))stem.resize(stem.size()-suffix.size());
            if(stem.empty())throw std::invalid_argument("Empty capture variant");
            m.tags["capture_variant"]=stem;m.tags["capture_group"]=utf8(p.parent_path()/fs::path(std::u8string(stem.begin(),stem.end())));
        }
        m.source="manual";m.confidence="user-confirmed";validate_cab_metadata(m.tags);
        (void)cab_asset_metadata(*it);candidate.overrides[path]=m;it->variant_id=variant_id(candidate,*it);
    }
    validate_duplicates(candidate);catalog=std::move(candidate);
}
CabManifest scan_cab_collection(const fs::path& root,std::string id,const CabManifest* previous,std::stop_token stop){
    if(id.empty())throw std::invalid_argument("Cab collection ID required");
    CabManifest out;out.root=root;out.collection_id=std::move(id);
    if(previous){if(previous->collection_id!=out.collection_id)throw std::invalid_argument("Collection identity mismatch");out.overrides=previous->overrides;}
    const auto index=index_reverb_library(root,stop); // reuse bounded, cancellable, symlink-safe traversal
    std::set<fs::path> files;for(const auto& e:index.entries){files.insert(e.first);if(!e.second.empty())files.insert(e.second);}
    if(index.phase!=ReverbLibraryPhase::ready)out.error=index.message;
    for(const auto& file:files){cancel(stop);CabAsset a;a.relative=utf8(file.lexically_relative(fs::absolute(root)));if(!safe(a.relative))throw std::runtime_error("Unsafe Cab relative path");a.id="asset:"+hash(out.collection_id+"/"+a.relative);
        try{const auto info=inspect_wav(file);a.rate=info.sample_rate;a.channels=info.channels;a.bits=info.bits;a.frames=static_cast<std::uint64_t>(info.frame_count);a.bytes=fs::file_size(file);a.modified=fs::last_write_time(file).time_since_epoch().count();
            if(a.channels>2||a.rate>192000||a.frames>a.rate)throw std::runtime_error("Unsupported Cab WAV channels/rate/length");
            a.mapping=parse_celestion(utf8(file.filename()),a.channels);a.digest=fingerprint(file,stop);
        }catch(const std::exception& e){a.error=e.what();}
        if(auto it=out.overrides.find(a.relative);it!=out.overrides.end()){a.mapping=it->second;a.mapping.source="manual";a.mapping.confidence="user-confirmed";}
        a.variant_id=variant_id(out,a);
        out.assets.push_back(std::move(a));
    }
    if(previous)for(auto a:previous->assets)if(std::none_of(out.assets.begin(),out.assets.end(),[&](const auto& v){return v.relative==a.relative;})){a.present=false;a.error="Missing WAV";out.assets.push_back(std::move(a));}
    std::sort(out.assets.begin(),out.assets.end(),[](const auto& a,const auto& b){return a.relative<b.relative;});
    validate_duplicates(out);
    cancel(stop);if(out.assets.size()>10000)throw std::runtime_error("Cab manifest exceeds 10000 assets");return out;
}
bool verify_cab_asset(const CabManifest& m,const CabAsset& a,std::stop_token stop){try{if(!safe(a.relative)||!a.present||!a.error.empty()||a.digest.empty())return false;auto p=m.root/fs::path(std::u8string(a.relative.begin(),a.relative.end()));const auto actual=fs::canonical(p);const auto base=fs::canonical(m.root);if(!safe(utf8(actual.lexically_relative(base))))return false;return fs::file_size(actual)==a.bytes&&fingerprint(actual,stop)==a.digest;}catch(...){return false;}}
std::string serialize_cab_manifest(const CabManifest& m){std::ostringstream o;o<<"MRS_CAB_COLLECTION 1 "<<std::quoted(m.collection_id)<<' '<<m.assets.size()<<' '<<m.overrides.size()<<'\n';for(const auto& a:m.assets){o<<std::quoted(a.id)<<' '<<std::quoted(a.variant_id)<<' '<<std::quoted(a.relative)<<' '<<std::quoted(a.digest)<<' '<<std::quoted(a.error)<<' '<<a.bytes<<' '<<a.modified<<' '<<a.frames<<' '<<a.rate<<' '<<a.channels<<' '<<a.bits<<' '<<a.present<<' ';mapping_out(o,a.mapping);o<<'\n';}for(const auto& [key,v]:m.overrides){o<<std::quoted(key)<<' ';mapping_out(o,v);o<<'\n';}return o.str();}
CabManifest deserialize_cab_manifest(std::string_view text){if(text.size()>16*1024*1024)throw std::invalid_argument("Cab manifest too large");std::istringstream i{std::string(text)};std::string tag;unsigned version{};std::size_t n{},k{};CabManifest m;i>>tag>>version>>std::quoted(m.collection_id)>>n>>k;if(!i||tag!="MRS_CAB_COLLECTION"||version!=1||n>10000||k>10000||m.collection_id.empty())throw std::invalid_argument("Invalid Cab manifest");std::set<std::string> paths;
    for(std::size_t j=0;j<n;++j){CabAsset a;i>>std::quoted(a.id)>>std::quoted(a.variant_id)>>std::quoted(a.relative)>>std::quoted(a.digest)>>std::quoted(a.error)>>a.bytes>>a.modified>>a.frames>>a.rate>>a.channels>>a.bits>>a.present;mapping_in(i,a.mapping);if(!i||!safe(a.relative)||!paths.insert(a.relative).second)throw std::invalid_argument("Invalid Cab asset");m.assets.push_back(std::move(a));}
    for(std::size_t j=0;j<k;++j){std::string key;CabMapping v;i>>std::quoted(key);mapping_in(i,v);if(!i||!safe(key)||!m.overrides.emplace(key,std::move(v)).second)throw std::invalid_argument("Invalid Cab override");}i>>std::ws;if(!i.eof())throw std::invalid_argument("Trailing Cab data");return m;
}
const CabAsset* select_cab_asset(const CabManifest& m,std::string_view variant,double ms,std::uint32_t rate){const CabAsset* chosen{};for(const auto& a:m.assets)if(a.present&&a.error.empty()&&a.variant_id==variant&&std::abs(a.duration_ms()-ms)<.01){if(a.rate==rate)return &a;if(!chosen||(a.rate==48000&&chosen->rate!=48000))chosen=&a;}return chosen;}
struct CabCollection::Impl {
    mutable std::mutex mutex;std::condition_variable_any wake;std::stop_source cancellation;fs::path root;std::string id;CabManifest previous;std::uint64_t generation{};bool pending{};std::shared_ptr<const CabManifest> status=std::make_shared<CabManifest>();std::jthread worker;
    Impl():worker([this](std::stop_token lifetime){while(!lifetime.stop_requested()){fs::path r;std::string key;CabManifest old;std::stop_token stop;std::uint64_t ticket{};{std::unique_lock l(mutex);if(!wake.wait(l,lifetime,[&]{return pending;}))return;r=root;key=id;old=previous;stop=cancellation.get_token();ticket=generation;pending=false;}CabManifest next;try{next=scan_cab_collection(r,key,&old,stop);}catch(const std::exception& e){next.error=e.what();}next.revision=ticket;auto ready=std::make_shared<const CabManifest>(std::move(next));{std::lock_guard l(mutex);if(ticket==generation&&!stop.stop_requested())status=std::move(ready);}}}){}
    ~Impl(){worker.request_stop();{std::lock_guard l(mutex);cancellation.request_stop();}wake.notify_all();worker.join();}
};
CabCollection::CabCollection():impl_(std::make_unique<Impl>()){} CabCollection::~CabCollection()=default;
void CabCollection::scan(fs::path root,std::string id,CabManifest old){if(old.collection_id.empty())old.collection_id=id;{std::lock_guard l(impl_->mutex);impl_->cancellation.request_stop();impl_->cancellation=std::stop_source{};impl_->root=std::move(root);impl_->id=std::move(id);impl_->previous=std::move(old);++impl_->generation;impl_->pending=true;}impl_->wake.notify_all();}
std::shared_ptr<const CabManifest> CabCollection::snapshot()const{std::lock_guard l(impl_->mutex);return impl_->status;}
void CabCollection::publish_mapping(CabManifest catalog){
    std::lock_guard l(impl_->mutex);
    if(catalog.revision!=impl_->generation||catalog.root!=impl_->status->root||catalog.collection_id!=impl_->status->collection_id)
        throw std::invalid_argument("Collection is scanning or changed; wait for scan and retry mapping");
    catalog.revision=impl_->generation+1;auto ready=std::make_shared<const CabManifest>(std::move(catalog));
    ++impl_->generation;impl_->status=std::move(ready);
}
}
