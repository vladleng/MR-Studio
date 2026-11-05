#include <mrs/transcription_models.hpp>
#include <mrs/sha256.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <mutex>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mrs::transcription {
namespace {
namespace fs=std::filesystem;
void need(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
void active(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Model operation cancelled");}
void plain(const fs::path& path,bool missing=false){
    need(path.is_absolute(),"Model paths must be absolute");fs::path at;
    for(const auto& c:path){
        need(c!="..","Invalid model path");at/=c;
        std::error_code ec;const auto status=fs::symlink_status(at,ec);
        if(missing&&status.type()==fs::file_type::not_found)continue;
        need(!ec&&!fs::is_symlink(status)&&fs::exists(status),"Linked or missing model path");
#ifdef _WIN32
        const auto attr=GetFileAttributesW(at.c_str());need(attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT),"Linked model path");
#endif
    }
}
std::string receipt(const ModelSpec& spec){return "MR-MUSCRIPTOR-INSTALL 1\n"+spec.id+"\n"+spec.revision+"\n"+spec.license+"\nterms-acknowledged\n";}
void metadata(const fs::path& folder,const ModelSpec& spec,bool sizes=true){
    plain(folder);need(fs::is_directory(folder),"Model directory missing");
    for(const auto& f:spec.files){const auto p=folder/f.name;plain(p);need(fs::is_regular_file(p)&&(!sizes||fs::file_size(p)==f.bytes),"Model size mismatch");}
    std::size_t entries{};for(const auto& e:fs::directory_iterator(folder)){
        const auto name=e.path().filename().string();need(name=="receipt.txt"||std::any_of(spec.files.begin(),spec.files.end(),[&](const auto& f){return f.name==name;}),"Unexpected file in model directory");++entries;
    }need(entries==4,"Model files missing");
    const auto p=folder/"receipt.txt";plain(p);need(fs::is_regular_file(p)&&fs::file_size(p)<=4096,"Model receipt missing");
    std::ifstream in(p,std::ios::binary);std::array<char,4097> buffer{};in.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
    need(in.eof()&&in.gcount()<=4096,"Cannot read bounded model receipt");
    const std::string text(buffer.data(),static_cast<std::size_t>(in.gcount()));need(text==receipt(spec),"Model receipt mismatch");
}
void verify_files(const fs::path& folder,const ModelSpec& spec,std::stop_token stop){
    plain(folder);for(const auto& f:spec.files){active(stop);const auto p=folder/f.name;plain(p);need(fs::is_regular_file(p)&&fs::file_size(p)==f.bytes,"Model size mismatch");need(sha256_file(p,stop)==f.sha256,"Model hash mismatch");}active(stop);
}
void copy(const fs::path& source,const fs::path& target,std::uint64_t expected,std::stop_token stop){
    std::ifstream in(source,std::ios::binary);std::ofstream out(target,std::ios::binary);need(in.good()&&out.good(),"Cannot copy model");std::array<char,65536> bytes{};
    std::uint64_t copied{};while(in){active(stop);in.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));const auto count=static_cast<std::uint64_t>(in.gcount());need(count<=expected-copied,"Model source grew during copy");copied+=count;out.write(bytes.data(),in.gcount());need(out.good(),"Cannot write model (disk full?)");}
    need(in.eof()&&copied==expected,"Cannot read complete model");out.close();need(out.good(),"Cannot finish model copy");
}
}
struct ModelStore::Data {fs::path root;ModelSpec spec;mutable std::mutex mutex;std::atomic<std::size_t> leases{};};
const ModelSpec& official_medium(){
    static const ModelSpec spec{"muscriptor-medium","f32236969308476e01fd3aae67357de5feb05a2d","CC BY-NC 4.0 + official HF specific conditions",{
        {"config.json","43e13a70fc9ae0af36b7447c06f3eac2282daeb69d79c1ff840ede7fdaa26a3b",126},
        {"model.safetensors","ac80adbdf85d87231735fd948af7013441c0afced316c4e9067fd5d8a7fb97ec",1228144472},
        {"README.md","8ef1a507afdb13355982d2f828f9e21fb403b2f56152eb00fd76d603ea1bcc8c",10198}}};
    return spec;
}
ModelStore::ModelStore(fs::path root,ModelSpec spec):data_(std::make_shared<Data>()){
    need(root.is_absolute()&&!root.empty(),"Model root must be absolute");
    need(!spec.id.empty()&&spec.id.size()<=64&&spec.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")==spec.id.npos,"Invalid model ID");
    need(spec.revision.size()==40&&spec.revision.find_first_not_of("abcdef0123456789")==spec.revision.npos,"Invalid model revision");
    need(!spec.license.empty()&&spec.license.size()<=256&&spec.license.find_first_of("\r\n") ==spec.license.npos,"Invalid model license");
    need(spec.files.size()==3,"Invalid model files");
    std::vector<std::string> names;std::uint64_t total{};
    for(const auto& f:spec.files){
        need(f.name=="config.json"||f.name=="model.safetensors"||f.name=="README.md","Invalid model filename");
        need(std::find(names.begin(),names.end(),f.name)==names.end(),"Duplicate model file");names.push_back(f.name);
        need(f.sha256.size()==64&&f.sha256.find_first_not_of("abcdef0123456789")==f.sha256.npos&&f.bytes>0&&f.bytes<=2ULL*1024*1024*1024,"Invalid model digest/size");total+=f.bytes;
    }
    need(total<=2ULL*1024*1024*1024,"Model disk cap exceeded");data_->root=std::move(root);data_->spec=std::move(spec);
}
fs::path ModelStore::directory()const{return data_->root/(data_->spec.id+"-"+data_->spec.revision);}
void ModelStore::install(const fs::path& source,bool acknowledge,std::stop_token stop)const{
    need(acknowledge,"Explicit acknowledgement of model terms required");
    std::unique_lock lock(data_->mutex,std::try_to_lock);need(lock.owns_lock()&&data_->leases.load()==0,"Model busy");active(stop);
    plain(data_->root,true);fs::create_directories(data_->root);plain(data_->root);
    need(!fs::exists(directory()),"Model already installed; remove explicitly before reinstalling");
    verify_files(source,data_->spec,stop);std::uint64_t required=16*1024*1024;for(const auto& f:data_->spec.files)required+=f.bytes;
    need(fs::space(data_->root).available>=required,"Insufficient model disk space");
    struct Staging{fs::path path;~Staging(){if(!path.empty()){std::error_code ec;fs::remove_all(path,ec);}}} staging;
    const auto path=data_->root/(".install-"+new_id().value);need(fs::create_directory(path),"Cannot create model staging");staging.path=path;
    fs::permissions(path,fs::perms::owner_all,fs::perm_options::replace);
    for(const auto& f:data_->spec.files)copy(source/f.name,path/f.name,f.bytes,stop);
    verify_files(path,data_->spec,stop);verify_files(source,data_->spec,stop);
    {std::ofstream out(path/"receipt.txt",std::ios::binary);out<<receipt(data_->spec);out.close();need(out.good(),"Cannot write model receipt");}
    active(stop);metadata(path,data_->spec);fs::rename(path,directory());staging.path.clear();
}
ModelLease ModelStore::acquire()const{
    std::unique_lock lock(data_->mutex,std::try_to_lock);need(lock.owns_lock(),"Model busy");
    struct Guard{std::shared_ptr<Data> owner;explicit Guard(std::shared_ptr<Data> p):owner(std::move(p)){++owner->leases;}~Guard(){--owner->leases;}};
    auto guard=std::make_shared<Guard>(data_);metadata(directory(),data_->spec);return {directory(),data_->spec,std::move(guard)};
}
void ModelStore::verify(std::stop_token stop)const{const auto lease=acquire();verify_files(lease.directory,lease.spec,stop);}
void ModelStore::remove()const{
    std::unique_lock lock(data_->mutex,std::try_to_lock);need(lock.owns_lock()&&data_->leases.load()==0,"Model busy");
    if(!fs::exists(directory()))return;
    metadata(directory(),data_->spec,false); // Corrupt weights are removable; foreign/linked folders are not.
    fs::remove_all(directory());
}
void ModelLease::configure(Request& request)const{
    need(lifetime!=nullptr,"Model lease required");request.model_directory=directory;request.model_id=spec.id;request.model_revision=spec.revision;request.model_license=spec.license;
    for(const auto& f:spec.files){if(f.name=="config.json")request.config_sha256=f.sha256;if(f.name=="model.safetensors")request.weights_sha256=f.sha256;}
}
}
