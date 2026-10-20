#include <mrs/reverb_library.hpp>
#include <mrs/audio.hpp>
#include <algorithm>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

namespace mrs::audio {
namespace {
namespace fs=std::filesystem;
std::string utf8(const fs::path& path){const auto s=path.generic_u8string();return {s.begin(),s.end()};}
std::string lower(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c+'a'-'A');return value;}
void cancelled(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("IR operation cancelled");}
struct PairName {std::string base;char side{};};
PairName pair_name(const fs::path& file){auto stem=utf8(file.stem());if(stem.size()>2){const auto side=stem.back();const auto separator=stem[stem.size()-2];
    if((side=='L'||side=='R')&&(separator==' '||separator=='_'||separator=='-'))return {stem.substr(0,stem.size()-2),side};}return {};}
ReverbLibraryEntry inspect_entry(const fs::path& file,const fs::path& root){
    ReverbLibraryEntry entry;entry.first=file;entry.name=utf8(file.stem());const auto relative=file.lexically_relative(root);
    entry.folder=utf8(relative.parent_path());entry.key=utf8(relative);
    try{const auto info=inspect_wav(file);entry.sample_rate=info.sample_rate;entry.channels=info.channels;entry.frames=static_cast<std::uint64_t>(info.frame_count);
        if(info.sample_rate>192000||(info.channels!=1&&info.channels!=2&&info.channels!=4))throw std::runtime_error("Unsupported IR channels/rate");
        if(entry.frames*entry.channels>64*1024*1024/sizeof(float))throw std::runtime_error("IR exceeds 64 MiB decoded memory budget");
        entry.kind=info.channels==1?ReverbFileKind::mono:info.channels==2?ReverbFileKind::stereo:ReverbFileKind::quad;
    }catch(const std::exception& e){entry.error=e.what();entry.kind=ReverbFileKind::unsupported;}return entry;
}
struct Stamp {std::uintmax_t bytes;fs::file_time_type time;bool operator==(const Stamp&) const=default;};
Stamp stamp(const fs::path& path){return {fs::file_size(path),fs::last_write_time(path)};}
ReverbFileIdentity identity(const fs::path& path,const fs::path& root,std::stop_token stop){
    const auto before=stamp(path);if(before.bytes>128*1024*1024)throw std::runtime_error("IR file exceeds 128 MiB byte budget");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("IR missing — choose folder/relink");
    std::array<char,65536> buffer{};std::uint64_t digest=14695981039346656037ULL,bytes{};
    while(input){cancelled(stop);input.read(buffer.data(),buffer.size());const auto count=input.gcount();bytes+=static_cast<std::uint64_t>(count);
        for(std::streamsize i=0;i<count;++i){digest^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);digest*=1099511628211ULL;}}
    if(!input.eof()||bytes!=before.bytes||stamp(path)!=before)throw std::runtime_error("IR changed during loading; select again");
    std::ostringstream encoded;encoded<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<digest;
    auto relative=path.lexically_relative(root);if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")relative.clear();
    return {relative,path,encoded.str(),bytes};
}
}
ReverbLibraryIndex index_reverb_library(const fs::path& requested,std::stop_token stop){
    ReverbLibraryIndex result;result.root=requested;
    if(requested.empty())return result;
    std::error_code error;result.root=fs::absolute(requested,error).lexically_normal();
    if(error||!fs::is_directory(result.root,error)){result.phase=ReverbLibraryPhase::missing;result.message="IR folder missing — choose folder/relink";return result;}
    std::vector<fs::path> files;fs::recursive_directory_iterator at(result.root,fs::directory_options::skip_permission_denied,error),end;
    if(error){result.phase=ReverbLibraryPhase::error;result.message="Cannot read IR folder — choose folder/relink";return result;}
    for(;at!=end;at.increment(error)){
        cancelled(stop);if(error){result.message="Some folders could not be read";error.clear();continue;}
        // Do not follow symlinks/junctions into another library or a traversal cycle.
        if(at->is_symlink(error)){at.disable_recursion_pending();continue;}
        if(at->is_regular_file(error)&&lower(utf8(at->path().extension()))==".wav"){
          if (files.size() == 10000)
            throw std::runtime_error("IR library exceeds 10000 WAV files; choose a smaller folder");
          files.push_back(at->path());
        }
    }
    std::sort(files.begin(),files.end());std::vector<ReverbLibraryEntry> entries;entries.reserve(files.size());
    for(const auto& file:files){cancelled(stop);entries.push_back(inspect_entry(file,result.root));}
    std::map<std::string,std::vector<std::size_t>> pairs;
    for(std::size_t i=0;i<entries.size();++i){const auto name=pair_name(entries[i].first);if(name.side&&entries[i].kind==ReverbFileKind::stereo)
        pairs[lower(utf8(entries[i].first.parent_path())+"/"+name.base)].push_back(i);}
    std::vector<bool> consumed(entries.size());
    for(const auto& [key,members]:pairs){(void)key;cancelled(stop);std::size_t l=entries.size(),r=l;
        for(auto i:members){if(pair_name(entries[i].first).side=='L')l=i;else r=i;}
        const bool matching=members.size()==2&&l<entries.size()&&r<entries.size()&&entries[l].sample_rate==entries[r].sample_rate&&entries[l].frames==entries[r].frames;
        if(matching&&entries[l].frames<=64*1024*1024/(4*sizeof(float))){auto pair=entries[l];pair.kind=ReverbFileKind::pair;pair.second=entries[r].first;
            pair.name=pair_name(pair.first).base;pair.channels=4;pair.key+="|"+entries[r].key;result.entries.push_back(std::move(pair));
        }else{for(auto i:members){entries[i].kind=ReverbFileKind::unsupported;entries[i].error="L/R pair missing, ambiguous, mismatched rate/length or too large";result.entries.push_back(entries[i]);}}
        for(auto i:members)consumed[i]=true;
    }
    for(std::size_t i=0;i<entries.size();++i)if(!consumed[i])result.entries.push_back(std::move(entries[i]));
    std::sort(result.entries.begin(),result.entries.end(),[](const auto& a,const auto& b){return a.key<b.key;});result.phase=ReverbLibraryPhase::ready;return result;
}

struct ReverbLibrary::Impl {
    mutable std::mutex mutex;std::condition_variable_any wake;std::stop_source cancellation;
    fs::path root;std::uint64_t revision{};bool pending{};
    std::shared_ptr<const ReverbLibraryIndex> status{std::make_shared<ReverbLibraryIndex>()};
    std::jthread worker;
    Impl():worker([this](std::stop_token lifetime){run(lifetime);}){}
    ~Impl(){worker.request_stop();{std::lock_guard lock(mutex);cancellation.request_stop();}wake.notify_all();worker.join();}
    void run(std::stop_token lifetime){while(!lifetime.stop_requested()){
        fs::path requested;std::uint64_t ticket{};std::stop_token stop;
        {std::unique_lock lock(mutex);if(!wake.wait(lock,lifetime,[&]{return pending;}))return;requested=root;ticket=revision;stop=cancellation.get_token();pending=false;}
        ReverbLibraryIndex next;
        try{next=index_reverb_library(requested,stop);}catch(const std::exception& e){next.root=requested;next.phase=ReverbLibraryPhase::error;next.message=e.what();}
        next.revision=ticket;auto published=std::make_shared<const ReverbLibraryIndex>(std::move(next));
        {std::lock_guard lock(mutex);if(ticket==revision&&!stop.stop_requested())status=std::move(published);}
    }}
};
ReverbLibrary::ReverbLibrary():impl_(std::make_unique<Impl>()){}
ReverbLibrary::~ReverbLibrary()=default;
void ReverbLibrary::set_root(fs::path root){auto next=std::make_shared<ReverbLibraryIndex>();next->root=root;next->phase=root.empty()?ReverbLibraryPhase::empty:ReverbLibraryPhase::scanning;
    {std::lock_guard lock(impl_->mutex);impl_->cancellation.request_stop();impl_->cancellation=std::stop_source{};impl_->root=std::move(root);next->revision=++impl_->revision;impl_->status=std::move(next);impl_->pending=true;}impl_->wake.notify_all();}
std::shared_ptr<const ReverbLibraryIndex> ReverbLibrary::snapshot() const {std::lock_guard lock(impl_->mutex);return impl_->status;}
bool ReverbLibrary::commit_if_current(std::uint64_t revision,const std::function<bool()>& commit){
    std::lock_guard lock(impl_->mutex);return revision==impl_->revision&&commit();
}

struct ReverbLoader::Impl {
    std::shared_ptr<processing::PreparedGraph> graph;Id node;processing::ProcessConfig config;
    std::shared_ptr<ReverbLibrary> library;std::uint64_t library_revision{};
    mutable std::mutex mutex;std::condition_variable_any wake;std::stop_source cancellation;
    std::mutex control;
    void collect(){std::lock_guard lock(control);graph->collect_reverb_retired(node);}
    ReverbLibraryEntry entry;fs::path root;processing::ReverbQuadMap map{};std::uint64_t revision{};bool pending{},closed{};std::optional<ReverbState> expected;
    std::shared_ptr<const ReverbLoadStatus> status{std::make_shared<ReverbLoadStatus>()};std::jthread worker;
    Impl(std::shared_ptr<processing::PreparedGraph> g,Id n,processing::ProcessConfig c,std::shared_ptr<ReverbLibrary> l):graph(std::move(g)),node(std::move(n)),config(c),library(std::move(l)){
      if (!graph)
        throw std::invalid_argument("MR Reverb loader needs a live graph lease");
      (void)graph->reverb_switching(node);
      worker = std::jthread([this](std::stop_token lifetime) { run(lifetime); });
    }
    void shutdown(){worker.request_stop();{std::lock_guard lock(mutex);closed=true;pending=false;cancellation.request_stop();}wake.notify_all();if(worker.joinable())worker.join();collect();}
    ~Impl(){shutdown();}
    void run(std::stop_token lifetime){while(!lifetime.stop_requested()){
        ReverbLibraryEntry requested;fs::path requested_root;processing::ReverbQuadMap requested_map{};std::uint64_t ticket{},catalogue_ticket{};std::stop_token stop;std::optional<ReverbState> restore_state;
        {std::unique_lock lock(mutex);if(!wake.wait_for(lock,lifetime,std::chrono::milliseconds(20),[&]{return pending;})){lock.unlock();collect();continue;}
          if (lifetime.stop_requested())
            return;
          requested = entry;
          requested_root = root;
          requested_map = map;
          ticket = revision;
          catalogue_ticket = library_revision;
          stop = cancellation.get_token();
          restore_state = expected;
          pending = false;
        }
        try{
            if(restore_state){const auto& saved=*restore_state;
                const auto resolve=[&](const ReverbReference& reference){
                    const auto path_of=[](const std::string& s){return fs::path(std::u8string(s.begin(),s.end()));};
                    const auto relative=path_of(reference.relative);std::vector<fs::path> candidates;
                    if(!requested_root.empty()&&!relative.empty()&&!relative.is_absolute()&&!relative.has_root_name()){
                        bool safe=true;for(const auto& part:relative)if(part=="..")safe=false;if(safe)candidates.push_back(requested_root/relative);}
                    const auto absolute=path_of(reference.absolute);if(absolute.is_absolute())candidates.push_back(absolute);
                    bool changed=false;for(const auto& file:candidates){cancelled(stop);if(!fs::is_regular_file(file))continue;
                        const auto actual=identity(file,requested_root,stop);if(actual.bytes==reference.bytes&&actual.digest==reference.digest)return file;changed=true;}
                    throw std::runtime_error(changed?"IR content changed — choose/relink the intended IR":"IR missing — choose folder/relink");
                };
                requested.first=resolve(saved.first);if(!saved.second.digest.empty())requested.second=resolve(saved.second);
                requested.name=saved.name;requested.key=saved.first.relative;requested.sample_rate=saved.sample_rate;requested.channels=saved.channels;requested.frames=saved.frames;
                requested.kind=!saved.second.digest.empty()?ReverbFileKind::pair:saved.channels==4?ReverbFileKind::quad:saved.channels==2?ReverbFileKind::stereo:ReverbFileKind::mono;
            }
            cancelled(stop);if(requested.kind==ReverbFileKind::unsupported)throw std::runtime_error(requested.error);
            if(!fs::is_regular_file(requested.first)||(!requested.second.empty()&&!fs::is_regular_file(requested.second)))throw std::runtime_error("IR missing — choose folder/relink");
            const auto first_stamp=stamp(requested.first);
            std::optional<Stamp> second_stamp;
            if(!requested.second.empty()){second_stamp.emplace(stamp(requested.second));}
            // Identity and metadata are validated before construction/publication. No UI/audio I/O.
            auto first=identity(requested.first,requested_root,stop);ReverbFileIdentity second;
            if(!requested.second.empty())second=identity(requested.second,requested_root,stop);
            if(restore_state&&(first.digest!=restore_state->first.digest||first.bytes!=restore_state->first.bytes||second.digest!=restore_state->second.digest||second.bytes!=restore_state->second.bytes))throw std::runtime_error("IR changed during restore — select again");
            auto ir=requested.kind==ReverbFileKind::pair?load_reverb_pair(requested.first,requested.second,stop):load_reverb_ir(requested.first,requested_map,stop);
            if(ir.sample_rate!=requested.sample_rate||ir.kernels[0].size()!=requested.frames)throw std::runtime_error("IR metadata changed; re-index folder");
            const auto actual_channels=ir.routing==processing::ReverbRouting::mono?1u:ir.routing==processing::ReverbRouting::stereo?2u:4u;
            if(actual_channels!=requested.channels)throw std::runtime_error("IR channel layout changed; re-index folder");
            auto kernel=processing::ReverbProcessor::prepare_kernel(ir,config,stop);
            if(stamp(requested.first)!=first_stamp||(second_stamp.has_value()&&stamp(requested.second)!=second_stamp.value()))throw std::runtime_error("IR changed during loading; select again");
            // One pending handoff at a time. Waiting is cancellable on this worker only;
            // never re-prepare graph/device or block the message/audio threads.
            while(!stop.stop_requested()&&!lifetime.stop_requested()){
                collect();std::unique_lock lock(mutex);
                if(ticket!=revision||stop.stop_requested())break;
                if(library&&library->snapshot()->revision!=catalogue_ticket){auto next=std::make_shared<ReverbLoadStatus>(*status);next->phase=next->active?ReverbLoadPhase::ready:ReverbLoadPhase::empty;next->message="Folder changed — select an IR";status=std::move(next);break;}
                auto next=std::make_shared<ReverbLoadStatus>(*status);next->phase=ReverbLoadPhase::ready;next->message="Ready";
                next->active=ReverbSelection{requested,first,second,requested_map}; // allocate before kernel publication
                next->active_revision=ticket;
                const auto commit=[&]{std::lock_guard guard(control);if(!graph->stage_reverb_kernel(node,kernel))return false;status=std::move(next);return true;};
                if(library?library->commit_if_current(catalogue_ticket,commit):commit())break;
                wake.wait_for(lock,lifetime,std::chrono::milliseconds(10),[&]{return stop.stop_requested();});
            }
        }catch(const std::exception& e){std::lock_guard lock(mutex);if(ticket==revision&&!stop.stop_requested()&&(!library||library->snapshot()->revision==catalogue_ticket)){
            auto next=std::make_shared<ReverbLoadStatus>(*status);next->message=e.what();next->phase=next->message.find("missing")!=std::string::npos?ReverbLoadPhase::missing:ReverbLoadPhase::error;status=std::move(next);}}
    }}
};
ReverbLoader::ReverbLoader(std::shared_ptr<processing::PreparedGraph> graph,Id node,processing::ProcessConfig config,std::shared_ptr<ReverbLibrary> library):impl_(std::make_unique<Impl>(std::move(graph),std::move(node),config,std::move(library))){}
ReverbLoader::~ReverbLoader()=default;
void ReverbLoader::shutdown(){impl_->shutdown();}
void ReverbLoader::activate_quiescent(){std::lock_guard lock(impl_->control);impl_->graph->warm_reverb(impl_->node);}
void ReverbLoader::restore(const ReverbState& state,const fs::path& root){state.validate();if(state.first.digest.empty())return;
    {std::lock_guard lock(impl_->mutex);if(impl_->closed)throw std::runtime_error("MR Reverb graph retired");impl_->cancellation.request_stop();impl_->cancellation=std::stop_source{};impl_->expected=state;impl_->entry={};impl_->root=root;impl_->map=static_cast<processing::ReverbQuadMap>(state.quad_map);
        if(impl_->library)impl_->library_revision=impl_->library->snapshot()->revision;
        auto next=std::make_shared<ReverbLoadStatus>(*impl_->status);next->phase=ReverbLoadPhase::loading;next->message="Restoring IR";next->requested_key=state.first.relative;next->revision=++impl_->revision;impl_->status=std::move(next);impl_->pending=true;}impl_->wake.notify_all();
}
ReverbState reverb_selection_state(const ReverbSelection& selection,const ReverbState& controls){auto result=controls;
    const auto reference=[](const ReverbFileIdentity& f){return ReverbReference{utf8(f.relative),utf8(f.absolute),f.digest,f.bytes};};
    result.first=reference(selection.first);result.second=reference(selection.second);result.name=selection.entry.name;
    result.sample_rate=selection.entry.sample_rate;result.channels=selection.entry.channels;result.frames=selection.entry.frames;result.quad_map=static_cast<std::uint32_t>(selection.map);result.validate();return result;
}
void ReverbLoader::select(const ReverbLibraryEntry& entry,const fs::path& root,processing::ReverbQuadMap map){
    {std::lock_guard lock(impl_->mutex);if(impl_->closed)throw std::runtime_error("MR Reverb graph retired");impl_->expected.reset();impl_->cancellation.request_stop();impl_->cancellation=std::stop_source{};impl_->entry=entry;impl_->root=root;impl_->map=map;
        const auto catalogue=impl_->library?impl_->library->snapshot():nullptr;if(catalogue)impl_->library_revision=catalogue->revision;
        auto next=std::make_shared<ReverbLoadStatus>(*impl_->status);next->phase=ReverbLoadPhase::loading;next->message="Loading";next->requested_key=entry.key;next->revision=++impl_->revision;
        impl_->pending=!catalogue||catalogue->root==root;
        if(!impl_->pending){next->phase=next->active?ReverbLoadPhase::ready:ReverbLoadPhase::empty;next->message="Folder changed — select an IR";}
        impl_->status=std::move(next);}impl_->wake.notify_all();
}
void ReverbLoader::cancel(){
    {std::lock_guard lock(impl_->mutex);impl_->cancellation.request_stop();impl_->pending=false;auto next=std::make_shared<ReverbLoadStatus>(*impl_->status);next->revision=++impl_->revision;
        next->phase=next->active?ReverbLoadPhase::ready:ReverbLoadPhase::empty;next->message=next->active?"Ready":"Choose an IR";impl_->status=std::move(next);}impl_->wake.notify_all();
}
std::shared_ptr<const ReverbLoadStatus> ReverbLoader::snapshot() const {std::lock_guard lock(impl_->mutex);return impl_->status;}
} // namespace mrs::audio
