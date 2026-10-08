#include <mrs/midi_input.hpp>
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
bool decode_midi_short(std::uint32_t raw,processing::MidiEvent& e) noexcept {
    using K=processing::MidiKind;
    const auto status=raw&255U;const auto type=status>>4;
    if(status<0x80||type>0xE)return false;
    e={0,K::note_off,static_cast<std::uint8_t>(status&15),static_cast<std::uint8_t>((raw>>8)&127),static_cast<std::uint8_t>((raw>>16)&127)};
    switch(type){case 8:e.kind=K::note_off;break;case 9:e.kind=e.data2?K::note_on:K::note_off;break;case 10:e.kind=K::poly_pressure;break;case 11:e.kind=K::cc;break;case 12:e.kind=K::program;e.data2=0;break;case 13:e.kind=K::pressure;e.data2=0;break;case 14:e.kind=K::pitch_bend;break;default:return false;}
    return true;
}
std::vector<MidiInputPort> MidiInputs::ports(){
    std::vector<MidiInputPort> result;
#ifdef _WIN32
    std::map<std::string,unsigned> duplicates;
    for(UINT i=0;i<midiInGetNumDevs();++i){MIDIINCAPSW caps{};if(midiInGetDevCapsW(i,&caps,sizeof(caps))!=MMSYSERR_NOERROR)continue;
        const int n=WideCharToMultiByte(CP_UTF8,0,caps.szPname,-1,nullptr,0,nullptr,nullptr);if(n<=0)continue;
        std::string name(static_cast<std::size_t>(n),0);WideCharToMultiByte(CP_UTF8,0,caps.szPname,-1,name.data(),n,nullptr,nullptr);name.pop_back();
        auto key="winmm:"+std::to_string(caps.wMid)+":"+std::to_string(caps.wPid)+":"+name;key+=":"+std::to_string(duplicates[key]++);result.push_back({key,name,i});
    }
#endif
    return result;
}
struct MidiInputs::Impl {
    std::shared_ptr<audio::AudioEngine> engine;
    mutable std::mutex requests;
    std::vector<MidiInputRoute> requested;
    std::uint64_t generation{},request_serial{};
    std::map<std::string,std::string> statuses;
    std::atomic<bool> quit{};
    std::thread worker;
#ifdef _WIN32
    struct Port {
        Impl* owner{};MidiInputPort info;HMIDIIN handle{};
        struct Message {std::uint32_t raw{};std::uint64_t timestamp{};};
        std::uint64_t start_ns{};std::mutex mutex;audio::SpscQueue<Message,1024> queue;
        ~Port(){if(handle){midiInStop(handle);midiInReset(handle);midiInClose(handle);}}
        static void CALLBACK receive(HMIDIIN,UINT message,DWORD_PTR user,DWORD_PTR data,DWORD_PTR stamp){
            auto& port=*reinterpret_cast<Port*>(user);
            if(message==MIM_DATA){std::lock_guard lock(port.mutex);if(!port.queue.push({static_cast<std::uint32_t>(data),port.start_ns+static_cast<std::uint64_t>(static_cast<DWORD>(stamp))*1000000ULL}))port.owner->engine->midi_overflow();}
            else if(message==MIM_ERROR)port.owner->engine->midi_input_lost();
        }
    };
#endif
    explicit Impl(std::shared_ptr<audio::AudioEngine> e):engine(std::move(e)){worker=std::thread([this]{run();});}
    ~Impl(){quit=true;if(worker.joinable())worker.join();engine->midi_panic();}
    void run() noexcept {
        try{
            std::vector<MidiInputRoute> active;std::uint64_t active_generation=~std::uint64_t{},applied_request=~std::uint64_t{};
            std::vector<MidiInputPort> inventory;
            auto check=std::chrono::steady_clock::time_point{};
#ifdef _WIN32
            std::vector<std::unique_ptr<Port>> opened;
#endif
            while(!quit){
                bool changed=false;
                {std::lock_guard lock(requests);if(applied_request!=request_serial){changed=active!=requested;active=requested;active_generation=generation;applied_request=request_serial;}}
                const auto now=std::chrono::steady_clock::now();
                if(now>=check){auto current=ports();changed|=current!=inventory;inventory=std::move(current);check=now+std::chrono::seconds(1);}
                if(changed){
#ifdef _WIN32
                    if(!opened.empty())engine->midi_input_lost();opened.clear();std::map<std::string,std::string> state;
                    for(const auto& route:active){if(state.contains(route.port))continue;auto at=std::find_if(inventory.begin(),inventory.end(),[&](const auto& p){return p.id==route.port;});
                        if(at==inventory.end()){state[route.port]="MIDI: missing";continue;}
                        auto port=std::make_unique<Port>();port->owner=this;port->info=*at;
                        if(midiInOpen(&port->handle,at->index,reinterpret_cast<DWORD_PTR>(&Port::receive),reinterpret_cast<DWORD_PTR>(port.get()),CALLBACK_FUNCTION)!=MMSYSERR_NOERROR){state[route.port]="MIDI: unavailable";continue;}
                        port->start_ns=audio::midi_clock_ns();
                        if(midiInStart(port->handle)!=MMSYSERR_NOERROR){state[route.port]="MIDI: unavailable";continue;}
                        state[route.port]="MIDI: connected";opened.push_back(std::move(port));
                    }
                    {std::lock_guard lock(requests);statuses=std::move(state);}
#endif
                }
#ifdef _WIN32
                for(auto& port:opened)for(int n=0;n<1023;++n){Port::Message message{};{std::lock_guard lock(port->mutex);if(!port->queue.pop(message))break;}
                    processing::MidiEvent event;if(!decode_midi_short(message.raw,event))continue;
                    for(const auto& route:active)if(route.port==port->info.id&&(route.channel<0||route.channel==event.channel))engine->enqueue_live_midi({active_generation,route.track,event,message.timestamp});
                }
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }catch(...){engine->midi_input_lost();std::lock_guard lock(requests);for(const auto& route:requested)statuses[route.port]="MIDI: bridge failed";}
    }
};
MidiInputs::MidiInputs(std::shared_ptr<audio::AudioEngine> engine):impl_(std::make_unique<Impl>(std::move(engine))){}
MidiInputs::~MidiInputs()=default;
void MidiInputs::routes(std::vector<MidiInputRoute> routes,std::uint64_t generation){std::lock_guard lock(impl_->requests);if(impl_->requested!=routes||impl_->generation!=generation){impl_->requested=std::move(routes);impl_->generation=generation;++impl_->request_serial;}}
std::string MidiInputs::status(const std::string& port)const{if(port.empty())return "MIDI: off";std::lock_guard lock(impl_->requests);const auto at=impl_->statuses.find(port);return at==impl_->statuses.end()?"MIDI: opening":at->second;}
}
