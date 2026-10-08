#include <mrs/midi_output.hpp>
#include <mrs/midi_recording.hpp>
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#endif
namespace mrs::desktop {
std::uint32_t encode_midi_short(processing::MidiEvent e,int channel) noexcept {
    static constexpr std::uint32_t types[]{0x80,0x90,0xB0,0xC0,0xD0,0xE0,0xA0};
    const auto kind=static_cast<unsigned>(e.kind);
    if(kind>=std::size(types)||e.channel>15||e.data1>127||e.data2>127||channel<-1||channel>15)return 0;
    return types[kind]|static_cast<std::uint32_t>(channel<0?e.channel:channel)|(e.data1<<8)|((kind==3||kind==4?0:e.data2)<<16);
}
namespace {
class WinOutputs final:public IMidiOutputBackend {
#ifdef _WIN32
    std::map<std::string,HMIDIOUT> handles;
#endif
public:
    ~WinOutputs()override{close();}
    std::vector<MidiOutputPort> ports()override{
        std::vector<MidiOutputPort> result;
#ifdef _WIN32
        std::map<std::string,unsigned> duplicates;
        for(UINT i=0;i<midiOutGetNumDevs();++i){MIDIOUTCAPSW caps{};if(midiOutGetDevCapsW(i,&caps,sizeof(caps))!=MMSYSERR_NOERROR)continue;
            const int n=WideCharToMultiByte(CP_UTF8,0,caps.szPname,-1,nullptr,0,nullptr,nullptr);if(n<=0)continue;
            std::string name(static_cast<std::size_t>(n),0);WideCharToMultiByte(CP_UTF8,0,caps.szPname,-1,name.data(),n,nullptr,nullptr);name.pop_back();
            auto key="winmm:"+std::to_string(caps.wMid)+":"+std::to_string(caps.wPid)+":"+name;
            key+=":"+std::to_string(duplicates[key]++);result.push_back({key,name,i});
        }
#endif
        return result;
    }
    bool open(const MidiOutputPort& port)override{
#ifdef _WIN32
        HMIDIOUT handle{};if(midiOutOpen(&handle,port.index,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)return false;
        handles.emplace(port.id,handle);return true;
#else
        return false;
#endif
    }
    bool send(const std::string& port,std::uint32_t raw)override{
#ifdef _WIN32
        const auto at=handles.find(port);return at!=handles.end()&&midiOutShortMsg(at->second,raw)==MMSYSERR_NOERROR;
#else
        return false;
#endif
    }
    void panic(const std::string& port)noexcept override{
#ifdef _WIN32
        const auto at=handles.find(port);if(at==handles.end())return;
        for(unsigned ch=0;ch<16;++ch){for(unsigned cc:{64U,66U,120U,123U})(void)midiOutShortMsg(at->second,0xB0U|ch|(cc<<8));(void)midiOutShortMsg(at->second,0xE0U|ch|(64U<<16));}
        (void)midiOutReset(at->second);
#endif
    }
    void close()noexcept override{
#ifdef _WIN32
        for(const auto& [id,handle]:handles){panic(id);(void)midiOutClose(handle);}handles.clear();
#endif
    }
};
}
struct MidiOutputs::Impl {
    std::shared_ptr<audio::ExternalMidiQueue> queue;std::unique_ptr<IMidiOutputBackend> backend;
    mutable std::mutex mutex;std::vector<MidiOutputRoute> requested;std::map<std::string,std::string> statuses;
    std::uint64_t serial{},generation{},published_serial{~std::uint64_t{}};std::atomic<bool> quit{};std::thread worker;
    Impl(std::shared_ptr<audio::ExternalMidiQueue> q,std::unique_ptr<IMidiOutputBackend> b):queue(std::move(q)),backend(b?std::move(b):std::make_unique<WinOutputs>()){worker=std::thread([this]{run();});}
    ~Impl(){quit=true;if(worker.joinable())worker.join();}
    void run()noexcept{
        try{
            std::vector<MidiOutputRoute> active;std::vector<MidiOutputPort> inventory;std::vector<std::string> opened;
            std::map<std::string,std::array<unsigned,2048>> held;
            std::uint64_t applied=~std::uint64_t{},epoch=queue->epoch(),activeGeneration{};auto check=std::chrono::steady_clock::time_point{};
            std::array<audio::ExternalMidiQueue::Packet,8192> pending{};std::size_t count{};
            while(!quit){
                bool changed=false,retry=false;
                {std::lock_guard lock(mutex);if(applied!=serial){active=requested;activeGeneration=generation;applied=serial;changed=true;}}
                const auto now=std::chrono::steady_clock::now();
                if(now>=check){auto ports=backend->ports();changed|=ports!=inventory;inventory=std::move(ports);check=now+std::chrono::seconds(1);
                    // Retry missing/busy ports without closing healthy ports every second.
                    for(const auto& r:active)if(std::find(opened.begin(),opened.end(),r.port)==opened.end())retry=true;
                }
                if(changed||retry){if(changed){backend->close();opened.clear();held.clear();count=0;queue->panic();epoch=queue->epoch();}std::map<std::string,std::string> state;
                    for(const auto& r:active){if(state.contains(r.port))continue;
                        if(std::find(opened.begin(),opened.end(),r.port)!=opened.end()){state[r.port]="MIDI out: connected";continue;}
                        const auto at=std::find_if(inventory.begin(),inventory.end(),[&](const auto& p){return p.id==r.port;});
                        if(at==inventory.end())state[r.port]="MIDI out: missing";
                        else if(!backend->open(*at))state[r.port]="MIDI out: unavailable";
                        else{state[r.port]="MIDI out: connected";opened.push_back(r.port);held[r.port]={};backend->panic(r.port);}
                    }
                    {std::lock_guard lock(mutex);statuses=std::move(state);published_serial=applied;}
                }
                const auto current=queue->epoch();
                if(current!=epoch){for(const auto& port:opened){backend->panic(port);held[port].fill(0);}count=0;epoch=current;}
                audio::ExternalMidiQueue::Packet packet;
                for(int n=0;n<8191&&queue->pop(packet);++n){if(packet.epoch!=epoch||packet.generation!=activeGeneration||queue->fault())continue;if(count==pending.size()){queue->fail();break;}pending[count++]=packet;}
                std::stable_sort(pending.begin(),pending.begin()+count,[](const auto& a,const auto& b){return a.due_ns<b.due_ns;});
                const auto clock=audio::midi_clock_ns();std::size_t sent{};
                while(sent<count&&pending[sent].due_ns<=clock){const auto& event=pending[sent++];if(event.epoch!=queue->epoch()||queue->fault())continue;
                    for(const auto& r:active)if(r.track==event.track&&std::find(opened.begin(),opened.end(),r.port)!=opened.end()){
                        const auto raw=encode_midi_short(event.event,r.channel);
                        if(!raw){queue->fail();continue;}
                        const auto channel=raw&15U,key=channel*128+event.event.data1;
                        auto& notes=held[r.port];
                        if(event.event.kind==processing::MidiKind::note_on&&notes[key]++>0)continue;
                        if(event.event.kind==processing::MidiKind::note_off){if(!notes[key])continue;if(--notes[key]>0)continue;}
                        if(event.event.kind==processing::MidiKind::cc&&(event.event.data1==120||event.event.data1==123))std::fill_n(notes.begin()+channel*128,128,0U);
                        if(!raw||!backend->send(r.port,raw)){queue->fail();std::lock_guard lock(mutex);statuses[r.port]="MIDI out: send failed; reconnect";}
                    }
                }
                std::move(pending.begin()+sent,pending.begin()+count,pending.begin());count-=sent;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }catch(...){queue->fail();std::lock_guard lock(mutex);for(const auto& r:requested)statuses[r.port]="MIDI out: worker failed";}
        backend->close();
    }
};
MidiOutputs::MidiOutputs(std::shared_ptr<audio::ExternalMidiQueue> q,std::unique_ptr<IMidiOutputBackend> b):impl_(std::make_unique<Impl>(std::move(q),std::move(b))){}
MidiOutputs::~MidiOutputs()=default;
std::vector<MidiOutputPort> MidiOutputs::ports(){return WinOutputs{}.ports();}
void MidiOutputs::routes(std::vector<MidiOutputRoute> routes,std::uint64_t generation){std::lock_guard lock(impl_->mutex);if(impl_->requested!=routes||impl_->generation!=generation){impl_->queue->panic();impl_->requested=std::move(routes);impl_->generation=generation;++impl_->serial;}}
void MidiOutputs::reconnect(){std::lock_guard lock(impl_->mutex);impl_->queue->panic();++impl_->serial;}
std::string MidiOutputs::status(const std::string& port)const{if(port.empty())return "MIDI out: off";if(impl_->queue->fault())return "MIDI out: fault; reconnect";std::lock_guard lock(impl_->mutex);if(impl_->published_serial!=impl_->serial)return "MIDI out: opening";const auto at=impl_->statuses.find(port);return at==impl_->statuses.end()?"MIDI out: opening":at->second;}
}
