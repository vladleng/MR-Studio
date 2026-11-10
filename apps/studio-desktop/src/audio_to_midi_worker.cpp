#include <mrs/audio_to_midi_worker.hpp>
#include <mrs/ace_midi_protocol.hpp>
#include <mrs/audio.hpp>
#include <mrs/midi_import.hpp>
#include <mrs/sha256.hpp>
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cmath>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <locale>
#include <sstream>
#include <thread>
#include <tuple>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace mrs::transcription {
namespace {
namespace fs=std::filesystem;
struct Failure { std::string code; };
void need(bool ok,const char* error){if(!ok)throw Failure{error};}
void stopped(std::stop_token stop){need(!stop.stop_requested(),"stopped");}
bool digest(std::string_view s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==s.npos;}
void plain(const fs::path& path,bool directory=false){
    need(path.is_absolute(),"invalid_path");fs::path at;
    for(const auto& component:path){
        need(component!="..","invalid_path");at/=component;
        need(!fs::is_symlink(fs::symlink_status(at)),"invalid_path");
#ifdef _WIN32
        const auto attr=GetFileAttributesW(at.c_str());
        need(attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT),"invalid_path");
#endif
    }
    need(directory?fs::is_directory(path):fs::is_regular_file(path),"invalid_path");
}
void verify_hash(const fs::path& path,const std::string& expected,std::stop_token stop){
    plain(path);need(digest(expected),"invalid_hash");stopped(stop);
    need(sha256_file(path,stop)==expected,"integrity_failure");stopped(stop);
}
struct Work {
    fs::path path;
    bool* cleanup_failed{};
    ~Work(){if(!path.empty()){std::error_code ec;fs::remove_all(path,ec);if(ec&&cleanup_failed)*cleanup_failed=true;}}
};
void snapshot(const fs::path& source,const fs::path& destination,std::stop_token stop){
    std::ifstream in(source,std::ios::binary);std::ofstream out(destination,std::ios::binary|std::ios::trunc);
    need(in.good()&&out.good(),"io_error");std::array<char,65536> block{};
    while(in){stopped(stop);in.read(block.data(),static_cast<std::streamsize>(block.size()));out.write(block.data(),in.gcount());need(out.good(),"io_error");}
    need(in.eof(),"io_error");out.close();need(out.good(),"io_error");
}
void snapshot_range(const audio::WavFile& wav,Sample first,Sample frames,const fs::path& target,std::stop_token stop){
    const auto align=wav.channels*(wav.bits/8);const auto bytes=static_cast<std::uint64_t>(frames)*align;
    need(frames>0&&bytes<=2ULL*1024*1024*1024,"clip_too_large");
    std::ifstream in(wav.path,std::ios::binary);std::ofstream out(target,std::ios::binary);
    need(in.good()&&out.good(),"io_error");
    const auto put=[&](std::uint32_t n,int width){for(int i=0;i<width;++i)out.put(static_cast<char>((n>>(8*i))&255));};
    out.write("RIFF",4);put(static_cast<std::uint32_t>(36+bytes+(bytes&1)),4);out.write("WAVEfmt ",8);put(16,4);
    put(wav.format,2);put(wav.channels,2);put(wav.sample_rate,4);put(wav.sample_rate*align,4);put(align,2);put(wav.bits,2);out.write("data",4);put(static_cast<std::uint32_t>(bytes),4);
    in.seekg(static_cast<std::streamoff>(wav.data_offset+static_cast<std::uint64_t>(first)*align));std::array<char,65536> block{};auto left=bytes;
    while(left){stopped(stop);const auto n=static_cast<std::streamsize>(std::min<std::uint64_t>(left,block.size()));need(static_cast<bool>(in.read(block.data(),n)),"io_error");out.write(block.data(),n);need(out.good(),"io_error");left-=static_cast<std::uint64_t>(n);}
    if(bytes&1){out.put(0);}
    out.close();need(out.good(),"io_error");
}
[[maybe_unused]] std::vector<std::string> arguments(const Request& r,const fs::path& work){
    if(r.backend==Backend::native_experimental){
        auto args=ace_midi_arguments(r.model_directory.string(),(work/"source.wav").string(),(work/"output.mid").string());
        if(!r.stem_instrument.empty()){args.push_back("--instrument");args.push_back(r.stem_instrument);}return args;
    }
    auto args=std::vector<std::string>{"-I","-B","-u",r.worker_script.string(),"--model",r.model_directory.string(),"--transcribe",(work/"source.wav").string(),"--out",(work/"output.mid").string(),"--threads",std::to_string(r.cpu_threads),"--options",(work/"options.json").string()};
    if(!r.instruments.empty()){args.push_back("--instruments");for(const auto& instrument:r.instruments)args.push_back(instrument);}return args;
}
void options_file(const Request& r,const fs::path& work){
    const auto& m=r.musical;
    need(std::isfinite(m.bpm)&&m.bpm>=0&&m.bpm<=999&&std::isfinite(m.strength)&&m.strength>=0&&m.strength<=1&&std::isfinite(m.first_downbeat)&&m.first_downbeat>=-1&&m.first_downbeat<=14400&&std::isfinite(m.fill_gaps)&&m.fill_gaps>=0&&m.fill_gaps<=16&&m.meter<=32&&m.subdivision>=1&&m.subdivision<=64&&(m.beat_scale==.25||m.beat_scale==.5||m.beat_scale==1||m.beat_scale==2||m.beat_scale==4),"invalid_music_options");
    std::ofstream out(work/"options.json");out.imbue(std::locale::classic());out<<std::setprecision(17)<<std::boolalpha;
    out<<"{\"detect_tempo\":"<<m.detect_tempo<<",\"bpm\":";
    if(m.bpm>0)out<<m.bpm;else out<<"null";
    out<<",\"beats_per_bar\":";if(m.meter)out<<m.meter;else out<<"null";out<<",\"first_downbeat\":";if(m.first_downbeat>=0)out<<m.first_downbeat;else out<<"null";out<<",\"tempo_map\":"<<m.tempo_map<<",\"beat_scale\":"<<m.beat_scale<<",\"quantize\":"<<m.quantize<<",\"subdivision\":"<<m.subdivision<<",\"strength\":"<<m.strength<<",\"fill_gaps\":"<<m.fill_gaps<<",\"trim_leading_bars\":"<<m.trim_leading_bars<<"}";
    out.close();need(out.good(),"io_error");
}
#ifdef _WIN32
struct Handle {
    HANDLE value{};
    ~Handle(){close();}
    void close(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=nullptr;}
};
std::wstring quote(const std::wstring& value){
    // CommandLineToArgvW/CRT escaping: double trailing slashes before closing
    // quotes, and 2*n+1 slashes before literal quotes. Never invoke a shell.
    std::wstring out=L"\"";std::size_t slashes{};
    for(const auto c:value){
        if(c==L'\\'){++slashes;continue;}
        out.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;
        if(c==L'"')out+=L'\\';
        out+=c;
    }
    out.append(slashes*2,L'\\');out+=L'"';return out;
}
struct Child {
    Handle job,process,output,error;
    ~Child(){if(job.value)TerminateJobObject(job.value,1);if(process.value)WaitForSingleObject(process.value,INFINITE);}
    void launch(const Request& r,const fs::path& work){
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};Handle childOut,childError,input;
        need(CreatePipe(&output.value,&childOut.value,&sa,65536)!=FALSE,"process_start");
        need(CreatePipe(&error.value,&childError.value,&sa,65536)!=FALSE,"process_start");
        need(SetHandleInformation(output.value,HANDLE_FLAG_INHERIT,0)&&SetHandleInformation(error.value,HANDLE_FLAG_INHERIT,0),"process_start");
        input.value=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        need(input.value!=INVALID_HANDLE_VALUE,"process_start");
        job.value=CreateJobObjectW(nullptr,nullptr);need(job.value!=nullptr,"process_start");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS|JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_JOB_MEMORY;
        limits.BasicLimitInformation.ActiveProcessLimit=1;
        limits.ProcessMemoryLimit=static_cast<SIZE_T>(r.memory_bytes);limits.JobMemoryLimit=static_cast<SIZE_T>(r.memory_bytes);
        need(SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))!=FALSE,"process_start");
        SIZE_T count{};InitializeProcThreadAttributeList(nullptr,1,0,&count);std::vector<unsigned char> storage(count);
        auto* attrs=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        need(InitializeProcThreadAttributeList(attrs,1,0,&count)!=FALSE,"process_start");
        struct AttrCleanup{LPPROC_THREAD_ATTRIBUTE_LIST p;~AttrCleanup(){DeleteProcThreadAttributeList(p);}} cleanup{attrs};
        HANDLE inherit[]{input.value,childOut.value,childError.value};
        need(UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherit,sizeof(inherit),nullptr,nullptr)!=FALSE,"process_start");
        STARTUPINFOEXW start{};start.StartupInfo.cb=sizeof(start);start.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        start.StartupInfo.hStdInput=input.value;start.StartupInfo.hStdOutput=childOut.value;start.StartupInfo.hStdError=childError.value;start.lpAttributeList=attrs;
        std::wstring command=quote(r.executable.wstring());
        std::vector<std::wstring> args;
        if(r.backend==Backend::python_medium){
            args={L"-I",L"-B",L"-u",r.worker_script.wstring(),L"--model",r.model_directory.wstring(),L"--transcribe",(work/"source.wav").wstring(),L"--out",(work/"output.mid").wstring(),L"--threads",std::to_wstring(r.cpu_threads),L"--options",(work/"options.json").wstring()};
            if(!r.instruments.empty()){args.push_back(L"--instruments");for(const auto& instrument:r.instruments)args.emplace_back(instrument.begin(),instrument.end());}
        }else{
            args={L"--model",r.model_directory.wstring(),L"--transcribe",(work/"source.wav").wstring(),L"--out",(work/"output.mid").wstring(),L"--jsonl",L"--device",L"cpu"};
            if(!r.stem_instrument.empty()){args.push_back(L"--instrument");args.emplace_back(r.stem_instrument.begin(),r.stem_instrument.end());}
        }
        for(const auto& arg:args)command+=L" "+quote(arg);
        std::array<wchar_t,32768> system{};const auto n=GetEnvironmentVariableW(L"SystemRoot",system.data(),static_cast<DWORD>(system.size()));
        need(n>0&&n<system.size(),"process_environment");
        const std::wstring root(system.data());
        // Explicit allowlist: no inherited HF_TOKEN, proxy credentials, probe
        // variables, GPU overrides or Python paths reach the native child.
        std::vector<std::wstring> vars{L"GGML_BACKEND=CPU",L"GGML_N_THREADS="+std::to_wstring(r.cpu_threads),L"OMP_NUM_THREADS="+std::to_wstring(r.cpu_threads),L"PATH="+root+L"\\System32",L"SystemRoot="+root,L"TEMP="+work.wstring(),L"TMP="+work.wstring(),L"WINDIR="+root,L"ATEN_CPU_CAPABILITY="+std::wstring(r.compatibility_cpu||!optimized_cpu_available()?L"default":L"avx2"),L"ONEDNN_MAX_CPU_ISA="+std::wstring(r.compatibility_cpu||!optimized_cpu_available()?L"SSE41":L"AVX2"),L"MKL_ENABLE_INSTRUCTIONS="+std::wstring(r.compatibility_cpu||!optimized_cpu_available()?L"SSE4_2":L"AVX2"),L"HF_HUB_OFFLINE=1",L"HF_HUB_DISABLE_TELEMETRY=1",L"TORCH_HOME="+(r.executable.parent_path()/"torch-cache").wstring(),L"MKL_NUM_THREADS="+std::to_wstring(r.cpu_threads),L"OPENBLAS_NUM_THREADS="+std::to_wstring(r.cpu_threads)};
        std::vector<wchar_t> env;for(const auto& v:vars){env.insert(env.end(),v.begin(),v.end());env.push_back(0);}env.push_back(0);
        PROCESS_INFORMATION info{};
        need(CreateProcessW(r.executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|BELOW_NORMAL_PRIORITY_CLASS|CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT|EXTENDED_STARTUPINFO_PRESENT,env.data(),work.c_str(),&start.StartupInfo,&info)!=FALSE,"process_start");
        process.value=info.hProcess;Handle thread{info.hThread};
        if(!AssignProcessToJobObject(job.value,process.value)||ResumeThread(thread.value)==static_cast<DWORD>(-1)){TerminateProcess(process.value,1);throw Failure{"process_start"};}
    }
    std::string read(bool stderr_stream){
        const auto h=stderr_stream?error.value:output.value;DWORD available{};
        if(!PeekNamedPipe(h,nullptr,0,nullptr,&available,nullptr)){need(GetLastError()==ERROR_BROKEN_PIPE,"process_read");return {};}
        std::array<char,4096> buffer{};DWORD got{};
        if(available)need(ReadFile(h,buffer.data(),std::min(available,static_cast<DWORD>(buffer.size())),&got,nullptr)!=FALSE,"process_read");
        return {buffer.data(),got};
    }
    std::uint64_t memory()const{PROCESS_MEMORY_COUNTERS counters{};return GetProcessMemoryInfo(process.value,&counters,sizeof(counters))?static_cast<std::uint64_t>(counters.PeakWorkingSetSize):0;}
    std::int64_t exit_code(){
        const auto wait=WaitForSingleObject(process.value,0);if(wait==WAIT_TIMEOUT)return -1;
        need(wait==WAIT_OBJECT_0,"process_wait");DWORD code{};
        need(GetExitCodeProcess(process.value,&code)!=FALSE,"process_wait");return static_cast<std::int64_t>(code);
    }
};
#else
struct Fd {int value{-1};~Fd(){if(value>=0)::close(value);}void close(){if(value>=0)::close(value);value=-1;}};
struct Child {
    Fd output,error;pid_t pid{-1};int code{-1};bool reaped{};std::uint64_t peak{};
    ~Child(){if(pid>0){::kill(-pid,SIGKILL);if(!reaped){int status{};while(waitpid(pid,&status,0)<0&&errno==EINTR){}}}}
    void launch(const Request& r,const fs::path& work){
        int a[2],b[2];need(pipe(a)==0,"process_start");output.value=a[0];Fd childOut{a[1]};
        need(pipe(b)==0,"process_start");error.value=b[0];Fd childError{b[1]};
        for(const auto fd:{output.value,error.value,childOut.value,childError.value})need(fcntl(fd,F_SETFD,FD_CLOEXEC)==0,"process_start");
        need(fcntl(output.value,F_SETFL,O_NONBLOCK)==0&&fcntl(error.value,F_SETFL,O_NONBLOCK)==0,"process_start");
        auto args=arguments(r,work);args.insert(args.begin(),r.executable.string());
        std::vector<char*> argv;for(auto& arg:args)argv.push_back(arg.data());argv.push_back(nullptr);
        std::vector<std::string> vars{"GGML_BACKEND=CPU","GGML_N_THREADS="+std::to_string(r.cpu_threads),"OMP_NUM_THREADS="+std::to_string(r.cpu_threads),"PATH=/usr/bin:/bin","TMPDIR="+work.string(),std::string("ATEN_CPU_CAPABILITY=")+(r.compatibility_cpu||!optimized_cpu_available()?"default":"avx2"),std::string("ONEDNN_MAX_CPU_ISA=")+(r.compatibility_cpu||!optimized_cpu_available()?"SSE41":"AVX2"),std::string("MKL_ENABLE_INSTRUCTIONS=")+(r.compatibility_cpu||!optimized_cpu_available()?"SSE4_2":"AVX2"),"HF_HUB_OFFLINE=1","TORCH_HOME="+(r.executable.parent_path()/"torch-cache").string(),"MKL_NUM_THREADS="+std::to_string(r.cpu_threads),"OPENBLAS_NUM_THREADS="+std::to_string(r.cpu_threads)};
        std::vector<char*> env;for(auto& v:vars)env.push_back(v.data());env.push_back(nullptr);
        const auto exe=r.executable.string(),cwd=work.string();const rlimit limit{static_cast<rlim_t>(r.memory_bytes),static_cast<rlim_t>(r.memory_bytes)};
        pid=fork();need(pid>=0,"process_start");
        if(pid==0){
            // Only async-signal-safe operations between fork and exec.
            if(setpgid(0,0)||chdir(cwd.c_str())||setrlimit(RLIMIT_AS,&limit))_exit(126);
            (void)setpriority(PRIO_PROCESS,0,10);
            const int input=open("/dev/null",O_RDONLY);if(input<0||dup2(input,0)<0||dup2(childOut.value,1)<0||dup2(childError.value,2)<0)_exit(126);
            close(input);
#ifdef __linux__
            if(close_range(3,~0U,0))_exit(126);
#endif
            execve(exe.c_str(),argv.data(),env.data());_exit(127);
        }
        // Close the brief fork/setpgid race before cancellation can kill a tree.
        if(setpgid(pid,pid)<0)need(errno==EACCES||errno==ESRCH,"process_start");
    }
    std::string read(bool stderr_stream){
        std::array<char,4096> data{};const auto n=::read(stderr_stream?error.value:output.value,data.data(),data.size());
        if(n<0){need(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR,"process_read");return {};}
        return {data.data(),static_cast<std::size_t>(n)};
    }
    std::uint64_t memory()const{return peak;}
    int exit_code(){
        if(!reaped){int status{};rusage usage{};const auto result=wait4(pid,&status,WNOHANG,&usage);need(result>=0||errno==EINTR,"process_wait");
            if(result==pid){peak=static_cast<std::uint64_t>(usage.ru_maxrss)*1024;reaped=true;code=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);}}
        return reaped?code:-1;
    }
};
#endif
void verify_midi(const std::vector<MidiPart>& parts,const std::vector<TranscribedNote>& raw){
    using Signature=std::tuple<int,int,Tick,Tick>;
    std::vector<Signature> expected,actual;auto notes=raw;
    std::stable_sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return std::tie(a.instrument,a.pitch,a.start)<std::tie(b.instrument,b.pitch,b.start);});
    const auto ticks=[](double seconds){return static_cast<Tick>(std::nearbyint(seconds*960))*2;};
    for(std::size_t i=0;i<notes.size();++i){
        const auto& n=notes[i];double end=std::max(n.end,n.start+.01);
        const bool drums=n.instrument=="drums";
        if(i+1<notes.size()&&notes[i+1].instrument==n.instrument&&notes[i+1].pitch==n.pitch)end=std::min(end,notes[i+1].start);
        if(end<=n.start)continue;
        if(drums)end=n.start+.01; // pinned writer emits fixed drum note-offs
        int program=-1;for(const auto& role:ace_midi_instruments())if(role.role==n.instrument)program=role.program;
        const auto start=ticks(n.start);expected.emplace_back(program,n.pitch,start,std::max(Tick{1},ticks(end)-start));
    }
    for(const auto& part:parts){
        need(!part.midi.notes.empty()&&part.midi.events.size()==1,"invalid_midi");
        const auto& e=part.midi.events.front();need(e.kind==3&&e.start==0&&e.channel==part.channel,"invalid_midi");
        need(part.channel!=9||e.data1==0,"invalid_midi"); // pinned drums writer uses GM program 0
        const int program=part.channel==9?128:e.data1;
        for(const auto& n:part.midi.notes){need(n.channel==part.channel&&n.velocity==100,"invalid_midi");actual.emplace_back(program,n.pitch,n.start,n.length);}
    }
    std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());need(expected==actual,"invalid_midi");
}
}

Result run(const Request& r,std::stop_token external,const std::function<void(int,int)>& progress){
    Result result;result.backend=r.backend;result.musical=r.musical;result.project_id=r.project_id;result.clip_id=r.clip_id;result.project_revision=r.project_revision;
    std::stop_source stop;std::stop_callback forward(external,[&]{stop.request_stop();});
    const auto budget=r.timeout>std::chrono::milliseconds(0)&&r.timeout<=std::chrono::hours(4)?r.timeout:std::chrono::milliseconds(std::chrono::hours(4));
    const auto deadline=std::chrono::steady_clock::now()+budget;
    std::jthread timer([&](std::stop_token shutdown){std::mutex mutex;std::condition_variable_any cv;std::unique_lock lock(mutex);cv.wait_until(lock,shutdown,deadline,[]{return false;});if(!shutdown.stop_requested())stop.request_stop();});
    const auto token=stop.get_token();
    const auto began=std::chrono::steady_clock::now();
    bool cleanup_failed{};
    try{
        need(r.timeout>std::chrono::milliseconds(0)&&r.timeout<=std::chrono::hours(4)&&r.cpu_threads<=32&&r.memory_bytes>=256ULL*1024*1024&&r.memory_bytes<=16ULL*1024*1024*1024,"invalid_request");
        need(!r.model_id.empty()&&r.model_id.size()<=128&&!r.model_revision.empty()&&r.model_revision.size()<=128&&!r.model_license.empty()&&r.model_license.size()<=256,"invalid_request");
        stopped(token);plain(r.temporary_root,true);plain(r.model_directory,true);plain(r.source);
        need(r.stem_instrument.empty()||std::any_of(ace_midi_instruments().begin(),ace_midi_instruments().end(),[&](const auto& i){return i.role==r.stem_instrument;}),"invalid_instrument");
        need(r.backend==Backend::native_experimental||r.backend==Backend::python_medium,"invalid_backend");
        if(r.backend==Backend::python_medium){
            if(r.verify_runtime)try{r.verify_runtime(token);}catch(const std::exception& e){result.diagnostics=std::string("Python runtime verification: ")+std::string(e.what()).substr(0,256);throw Failure{"runtime_verification_failed"};}
            verify_hash(r.worker_script,r.worker_sha256,token);need(r.instruments.size()<=35,"invalid_instrument");
            auto instruments=r.instruments;std::sort(instruments.begin(),instruments.end());need(std::adjacent_find(instruments.begin(),instruments.end())==instruments.end(),"invalid_instrument");
            for(const auto& instrument:instruments)need(std::any_of(ace_midi_instruments().begin(),ace_midi_instruments().end(),[&](const auto& i){return i.role==instrument;}),"invalid_instrument");
        }
        const auto source_size=fs::file_size(r.source);const auto source_modified=fs::last_write_time(r.source);
        need(r.range.has_value()||source_size<=2ULL*1024*1024*1024,"source_too_large");
        auto resolved=r;resolved.cpu_threads=choose_cpu_threads(r.cpu_threads);result.cpu_threads=resolved.cpu_threads;
        verify_hash(r.executable,r.executable_sha256,token);
        verify_hash(r.model_directory/"config.json",r.config_sha256,token);
        verify_hash(r.model_directory/"model.safetensors",r.weights_sha256,token);
        Work work;work.cleanup_failed=&cleanup_failed;const auto candidate=r.temporary_root/("mrs-midi-"+new_id().value);
        need(fs::create_directory(candidate),"io_error");work.path=candidate;
        fs::permissions(work.path,fs::perms::owner_all,fs::perm_options::replace);
        if(r.backend==Backend::python_medium)options_file(r,work.path);
        Sample first{},frames{};std::uint64_t source_byte_offset{},source_byte_count{};std::string source_hash;
        if(r.range){
            const auto original=audio::inspect_wav(r.source);const auto range=*r.range;
            need(range.offset>=0&&range.frames>0&&range.rate>=8000&&range.rate<=768000&&original.channels<=2,"invalid_range");
            const auto begin=std::floor(static_cast<long double>(range.offset)*original.sample_rate/range.rate);
            const auto end=std::min(static_cast<long double>(original.frame_count),std::ceil((static_cast<long double>(range.offset)+range.frames)*original.sample_rate/range.rate));
            need(begin>=0&&begin<end&&end<=original.frame_count&&end-begin<=static_cast<long double>(original.sample_rate)*4*3600,"invalid_range");
            first=static_cast<Sample>(begin);frames=static_cast<Sample>(end)-first;
            source_byte_offset=original.data_offset+static_cast<std::uint64_t>(first)*original.channels*(original.bits/8);
            source_byte_count=static_cast<std::uint64_t>(frames)*original.channels*(original.bits/8);
            need(source_byte_count<=2ULL*1024*1024*1024,"clip_too_large");
            source_hash=sha256_file_range(r.source,source_byte_offset,source_byte_count,token);
            snapshot_range(original,first,frames,work.path/"source.wav",token);
            need(sha256_file_range(work.path/"source.wav",44,source_byte_count,token)==source_hash,"stale_source");
            result.source_offset_seconds=static_cast<double>(first)/original.sample_rate;
        }else snapshot(r.source,work.path/"source.wav",token);
        const auto input_hash=sha256_file(work.path/"source.wav",token);
        if(!r.range){source_hash=input_hash;need(sha256_file(r.source,token)==source_hash,"stale_source");}
        const auto wav=audio::inspect_wav(work.path/"source.wav");
        need(wav.sample_rate>0&&wav.frame_count>0&&(wav.channels==1||wav.channels==2),"invalid_audio");
        const auto frames_16k=wav.sample_rate==16000?wav.frame_count:
            static_cast<Sample>(static_cast<double>(wav.frame_count)*(16000./wav.sample_rate));
        need(frames_16k>0,"invalid_audio");
        AceMidiProtocol protocol(static_cast<double>(wav.frame_count)/wav.sample_rate,frames_16k,r.backend==Backend::python_medium);
        {
            Child child;child.launch(resolved,work.path);int completed=-1;std::size_t error_bytes{};
            for(;;){
                stopped(token);auto out=child.read(false);auto error=child.read(true);
                need(error.size()<=4*1024*1024-error_bytes,"diagnostic_limit");error_bytes+=error.size();result.diagnostics+=error;
                if(result.diagnostics.size()>65536)result.diagnostics.erase(0,result.diagnostics.size()-65536);
                if(!out.empty())protocol.feed(out);
                if(protocol.completed()!=completed){completed=protocol.completed();if(progress)progress(completed,protocol.total());}
                const auto exit=child.exit_code();
                if(exit!=-1&&out.empty()&&error.empty()){result.peak_memory_bytes=child.memory();need(exit==0,"process_exit");break;}
                if(out.empty()&&error.empty())std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } // child tree is terminated/reaped before touching/deleting outputs
        protocol.finish();stopped(token);
        const auto midi=work.path/"output.mid";plain(midi);need(fs::file_size(midi)==protocol.midi_bytes(),"invalid_midi");
        std::vector<std::byte> bytes(protocol.midi_bytes());std::ifstream in(midi,std::ios::binary);in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));need(in.good(),"io_error");
        const auto parts=validate_transcription_midi(bytes);
        // Account for pinned writer minimum duration/overlap trimming, and
        // compare pitch/program/channel/timing as well as count, not just MThd.
        verify_midi(parts,protocol.raw_notes());
        verify_hash(work.path/"source.wav",input_hash,token);plain(r.source);
        need(fs::file_size(r.source)==source_size&&fs::last_write_time(r.source)==source_modified,"stale_source");
        if(r.range)need(sha256_file_range(r.source,source_byte_offset,source_byte_count,token)==source_hash,"stale_source");else verify_hash(r.source,source_hash,token);
        verify_hash(r.executable,r.executable_sha256,token);
        verify_hash(r.model_directory/"config.json",r.config_sha256,token);verify_hash(r.model_directory/"model.safetensors",r.weights_sha256,token);
        result.notes=protocol.notes();
        need(r.stem_instrument.empty()||std::all_of(protocol.raw_notes().begin(),protocol.raw_notes().end(),[&](const auto& n){return n.instrument==r.stem_instrument;}),"instrument_mismatch");
        result.original_notes=result.notes;
        if(r.backend==Backend::python_medium){
            verify_hash(r.worker_script,r.worker_sha256,token);
            need(r.instruments.empty()||std::all_of(protocol.raw_notes().begin(),protocol.raw_notes().end(),[&](const auto& n){return std::find(r.instruments.begin(),r.instruments.end(),n.instrument)!=r.instruments.end();}),"instrument_mismatch");
            const auto processed=work.path/"processed.jsonl";plain(processed);need(fs::file_size(processed)<=16*1024*1024,"invalid_output");
            AceMidiProtocol post(static_cast<double>(wav.frame_count)/wav.sample_rate,frames_16k,true);
            std::ifstream stream(processed,std::ios::binary);std::array<char,4096> data{};
            while(stream){stopped(token);stream.read(data.data(),data.size());if(stream.gcount())post.feed({data.data(),static_cast<std::size_t>(stream.gcount())});}need(stream.eof(),"io_error");post.finish();need(post.raw_count()<=protocol.raw_count(),"invalid_output");result.notes=post.notes();
            need(std::all_of(result.notes.begin(),result.notes.end(),[&](const auto& n){return std::any_of(protocol.raw_notes().begin(),protocol.raw_notes().end(),[&](const auto& raw){return raw.instrument==n.instrument;});}),"instrument_mismatch");
            const auto meta=work.path/"music.txt";plain(meta);need(fs::file_size(meta)<=1024*1024,"invalid_output");
            std::ifstream music(meta);music.imbue(std::locale::classic());std::string magic;unsigned threads{};std::size_t count{};
            need(static_cast<bool>(music>>magic>>result.cpu_capability>>threads>>result.model_load_seconds>>result.inference_seconds>>result.detected_bpm>>result.detected_meter>>result.first_downbeat>>count),"invalid_output");
            need(magic=="MR-MUSCRIPTOR-2"&&threads==resolved.cpu_threads&&result.cpu_capability.size()<=32&&std::isfinite(result.model_load_seconds)&&result.model_load_seconds>=0&&std::isfinite(result.inference_seconds)&&result.inference_seconds>=0&&std::isfinite(result.detected_bpm)&&result.detected_bpm>=0&&result.detected_bpm<=999&&result.detected_meter<=32&&std::isfinite(result.first_downbeat)&&result.first_downbeat>=0&&result.first_downbeat<=14400&&count<=100000,"invalid_output");
            double previous=-1;for(std::size_t i=0;i<count;++i){double beat{};need(static_cast<bool>(music>>beat)&&std::isfinite(beat)&&beat>previous&&beat<=static_cast<double>(wav.frame_count)/wav.sample_rate+5,"invalid_output");result.beats.push_back(beat);previous=beat;}
            music>>std::ws;need(music.eof(),"invalid_output");
        }
        for(auto* notes:{&result.notes,&result.original_notes})for(auto& note:*notes){note.start+=result.source_offset_seconds;note.end+=result.source_offset_seconds;}
        result.analysis_seconds=static_cast<double>(wav.frame_count)/wav.sample_rate;result.source_rate=wav.sample_rate;result.source_first_frame=first;result.source_frame_count=wav.frame_count;result.input_sha256=input_hash;
        result.raw_notes=protocol.raw_count();result.midi_sha256=sha256_file(midi,token);
        result.source_sha256=source_hash;result.executable_sha256=r.executable_sha256;result.worker_sha256=r.worker_sha256;result.weights_sha256=r.weights_sha256;result.config_sha256=r.config_sha256;
        result.model_id=r.model_id;result.model_revision=r.model_revision;result.model_license=r.model_license;
        stopped(token);result.state=result.notes.empty()?State::empty:State::ready;
    }catch(const Failure& e){result.error=e.code;}
     catch(const std::invalid_argument&){result.error="invalid_output";}
     catch(...){result.error="io_or_backend_failure";}
    if(token.stop_requested()){
        result.state=external.stop_requested()?State::cancelled:State::timeout;result.error=external.stop_requested()?"cancelled":"timeout";
    }
    if(cleanup_failed){result.state=State::failed;result.error="cleanup_failed";}
    result.elapsed_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
    if(result.state!=State::ready&&result.state!=State::empty){result.notes.clear();result.original_notes.clear();result.beats.clear();}
    return result;
}
}
