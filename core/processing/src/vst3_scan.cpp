#include <mrs/vst3.hpp>
#include <windows.h>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <stdexcept>
namespace mrs::processing {
namespace {
std::string utf8(const std::filesystem::path& p){auto s=p.u8string();return {s.begin(),s.end()};}
std::filesystem::path path8(const std::string& s){return std::filesystem::path(std::u8string(s.begin(),s.end()));}
std::string fingerprint(const std::filesystem::path& p){
    std::error_code ec;auto value=std::to_string(std::filesystem::last_write_time(p,ec).time_since_epoch().count());
    if(std::filesystem::is_regular_file(p,ec))return value+":"+std::to_string(std::filesystem::file_size(p,ec));
    for(std::filesystem::recursive_directory_iterator it(p,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end && !ec;it.increment(ec)){
        if(it.depth()>6){it.disable_recursion_pending();continue;}
        if((GetFileAttributesW(it->path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT)!=0){it.disable_recursion_pending();continue;}
        if(it->is_regular_file(ec))value+=utf8(it->path().filename())+std::to_string(it->last_write_time(ec).time_since_epoch().count())+std::to_string(it->file_size(ec));
    }return value;
}
bool probe_child(const std::filesystem::path& helper,const std::filesystem::path& plugin,const std::filesystem::path& output){
    auto cmd=L"\""+helper.wstring()+L"\" \""+plugin.wstring()+L"\" \""+output.wstring()+L"\"";
    STARTUPINFOW start{};start.cb=sizeof(start);start.dwFlags=STARTF_USESHOWWINDOW;start.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
    if(!CreateProcessW(helper.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,helper.parent_path().c_str(),&start,&process))throw std::runtime_error("VST3 scanner helper cannot start (Win32 "+std::to_string(GetLastError())+")");
    const auto result=WaitForSingleObject(process.hProcess,10000);if(result!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,124);WaitForSingleObject(process.hProcess,1000);}
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);return result==WAIT_OBJECT_0&&code==0;
}
}
std::vector<VstPlugin> load_vst3_cache(const std::filesystem::path& path){
    if(!std::filesystem::exists(path))return {};if(std::filesystem::file_size(path)>4*1024*1024)throw std::runtime_error("VST3 cache too large");
    std::ifstream in(path);std::string magic;unsigned version{};std::size_t n{};if(!(in>>magic>>version>>n)||magic!="MRS_VST3_CACHE"||version!=1||n>4096)throw std::runtime_error("invalid VST3 cache");
    std::vector<VstPlugin> list;for(std::size_t i=0;i<n;++i){VstPlugin p;if(!(in>>std::quoted(p.path)>>std::quoted(p.class_id)>>std::quoted(p.name)>>std::quoted(p.vendor)>>std::quoted(p.version))||p.class_id.size()!=32||p.class_id.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw std::runtime_error("invalid VST3 cache entry");list.push_back(std::move(p));}return list;
}
void save_vst3_probe(const std::filesystem::path& path,const std::vector<VstPlugin>& list){
    auto temporary=path;temporary+=L".tmp";std::ofstream out(temporary,std::ios::trunc);out<<"MRS_VST3_CACHE 1 "<<list.size()<<'\n';for(const auto& p:list)out<<std::quoted(p.path)<<' '<<std::quoted(p.class_id)<<' '<<std::quoted(p.name)<<' '<<std::quoted(p.vendor)<<' '<<std::quoted(p.version)<<'\n';out.flush();if(!out)throw std::runtime_error("Cannot write VST3 cache");out.close();if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot publish VST3 cache");
}
std::vector<VstPlugin> scan_vst3(const std::filesystem::path& root,const std::filesystem::path& helper,const std::filesystem::path& cache,std::shared_ptr<std::atomic<bool>> cancel){
    if(!std::filesystem::exists(helper))throw std::runtime_error("Missing mrs_vst3_scan.exe beside MR Studio");
    std::filesystem::create_directories(cache.parent_path());auto old=load_vst3_cache(cache);std::map<std::string,std::string> stamps;auto index=cache;index+=L".index";std::ifstream in(index);std::string p,stamp;while(in>>std::quoted(p)>>std::quoted(stamp))stamps[p]=stamp;
    std::vector<std::filesystem::path> paths;std::error_code ec;
    if(root.extension()==L".vst3")paths.push_back(root);
    else for(std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end&&!ec;it.increment(ec)){
        if(it->path().extension()==L".vst3"){paths.push_back(it->path());if(it->is_directory(ec))it.disable_recursion_pending();if(paths.size()>2048)throw std::runtime_error("VST3 scan exceeds 2048 modules; select a smaller folder");}
        else if(it.depth()>12 || (GetFileAttributesW(it->path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT)!=0)it.disable_recursion_pending();
    }
    if(ec)throw std::runtime_error("VST3 scan folder unavailable: "+ec.message());
    auto list=old;std::ofstream errors(cache.parent_path()/L"vst3-scan.log",std::ios::app);
    auto output=cache;output+=L".probe-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    for(const auto& candidate:paths){if(cancel && cancel->load())break;const auto path=std::filesystem::absolute(candidate).lexically_normal();const auto key=utf8(path),stampNow=fingerprint(path);if(stamps.contains(key)&&stamps[key]==stampNow)continue;
        std::erase_if(list,[&](const auto& p){return p.path==key;});
        // Failed modules keep their fingerprint and are retried only after a change/cache reset.
        stamps[key]=stampNow;
        bool passed=probe_child(helper,path,output);
        if(passed){try{auto found=load_vst3_cache(output);list.insert(list.end(),found.begin(),found.end());}catch(const std::exception& e){errors<<key<<": "<<e.what()<<'\n';passed=false;}}
        if(!passed)errors<<key<<": rejected / crashed / timed out; cached until module changes or cache reset\n";
        std::filesystem::remove(output,ec);
        save_vst3_probe(cache,list);std::ofstream manifest(index,std::ios::trunc);for(const auto& [k,v]:stamps)manifest<<std::quoted(k)<<' '<<std::quoted(v)<<'\n';
    }
    std::erase_if(list,[](const auto& p){std::error_code ec;return !std::filesystem::exists(path8(p.path),ec);});save_vst3_probe(cache,list);return list;
}
}
