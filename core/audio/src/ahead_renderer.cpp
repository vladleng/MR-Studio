#include <mrs/ahead_renderer.hpp>
#include <algorithm>
#include <chrono>
#include <semaphore>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <avrt.h>
#endif
namespace mrs::audio {
namespace {
class Signal {
#ifdef _WIN32
    HANDLE handle_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
public:
    Signal(){if(!handle_)throw std::runtime_error("process buffer event creation failed");}
    ~Signal(){CloseHandle(handle_);}
    void post()noexcept{SetEvent(handle_);}
    void wait()noexcept{WaitForSingleObject(handle_,INFINITE);}
    bool wait_for(unsigned ms)noexcept{return WaitForSingleObject(handle_,ms)==WAIT_OBJECT_0;}
#else
    std::counting_semaphore<2147483647> signal_{0};
public:
    void post()noexcept{signal_.release();}
    void wait()noexcept{signal_.acquire();}
    bool wait_for(unsigned ms)noexcept{return signal_.try_acquire_for(std::chrono::milliseconds(ms));}
#endif
};
}
struct AheadRenderer::Signals {Signal wake,ready;};
AheadRenderer::AheadRenderer(std::shared_ptr<AudioEngine> engine,std::uint32_t device,std::uint32_t process)
    :engine_(std::move(engine)),signals_(std::make_unique<Signals>()),device_frames_(device),process_frames_(process) {
    if(!engine_||!device||device>engine_->config().max_block||process>8192||(process&&process<device))throw std::invalid_argument("invalid Device/Process Buffer");
    const auto slots=process?(process+device-1)/device+1:0;
    const auto bytes=sizeof(*this)+sizeof(Signals)+16*sizeof(MixerUpdate)+static_cast<std::uint64_t>(slots)*(sizeof(Packet)+static_cast<std::uint64_t>(device)*engine_->config().output_channels*sizeof(float));
    if(bytes>16*1024*1024)throw std::invalid_argument("process buffer exceeds 16 MiB");
    packets_.resize(slots);for(auto& packet:packets_)packet.audio.resize(static_cast<std::size_t>(device)*engine_->config().output_channels);
    mixes_.resize(16);
    engine_->process_buffer_frames_=process;engine_->ahead_underruns_=0;engine_->ahead_invalidations_=0;engine_->ahead_max_process_ns_=0;
    engine_->ahead_memory_bytes_=bytes;
}
AheadRenderer::~AheadRenderer(){stop();}
void AheadRenderer::start() {
    if(producer_.joinable())throw std::logic_error("process producer already running");
    active_=process_frames_&&engine_->anticipation_safe();engine_->anticipation_active_=active_;
    if(!active_)return;
    consumer_head_={engine_->state(),engine_->mix_head()};delivered_[0].value=consumer_head_;latest_=0;
    read_=0;write_=0;partial_=0;quit_=false;control_count_=mix_count_=0;control_serial_=mix_serial_=0;engine_->speculative_=true;engine_->ahead_owner_=this;
    try{producer_=std::thread([this]{run();});}
    catch(...){engine_->speculative_=false;engine_->ahead_owner_=nullptr;active_=false;engine_->anticipation_active_=false;throw;}
    if(!signals_->ready.wait_for(1000)){stop();throw std::runtime_error("process buffer priming timed out");}
}
void AheadRenderer::stop() noexcept {
    quit_=true;signals_->wake.post();if(producer_.joinable())producer_.join();
    if(active_){engine_->quiesce();Delivered head;if(read_head(head)){engine_->rebase_head(head.head);engine_->restore_mix(head.mix);replay(head);}engine_->speculative_=false;engine_->ahead_owner_=nullptr;}
    active_=false;engine_->anticipation_active_=false;engine_->ahead_buffered_frames_=0;
}
bool AheadRenderer::read_head(Delivered& result) noexcept {
    for(unsigned attempt=0;attempt<3;++attempt){const auto index=latest_.load(std::memory_order_acquire);int free=0;
        if(!delivered_[index].owner.compare_exchange_strong(free,1,std::memory_order_acquire))continue;
        if(index==latest_.load(std::memory_order_acquire)){result=delivered_[index].value;delivered_[index].owner.store(0,std::memory_order_release);return true;}
        delivered_[index].owner.store(0,std::memory_order_release);
    }return false;
}
void AheadRenderer::publish_head() noexcept {
    const auto latest=latest_.load(std::memory_order_relaxed);
    for(std::uint32_t i=0;i<3;++i)if(i!=latest){int free=0;if(!delivered_[i].owner.compare_exchange_strong(free,-1,std::memory_order_acquire))continue;
        delivered_[i].value=consumer_head_;delivered_[i].owner.store(0,std::memory_order_release);latest_.store(i,std::memory_order_release);return;}
}
void AheadRenderer::advance(Delivered& delivered,std::uint32_t frames) noexcept {
    auto& head=delivered.head;
    if(head.playback==PlaybackState::playing){
        if(head.loop&&head.sample>=head.loop->end)head.sample=head.loop->start+(head.sample-head.loop->start)%(head.loop->end-head.loop->start);
        head.sample=std::min(max_sample,head.sample+static_cast<Sample>(frames));
        if(head.loop&&head.sample>=head.loop->end)head.sample=head.loop->start+(head.sample-head.loop->start)%(head.loop->end-head.loop->start);
    }
    auto& m=delivered.mix;const auto ramp=std::min(m.ramp,frames);
    for(std::uint32_t f=0;f<ramp;++f){for(std::size_t t=0;t<engine_->graph_.mixer.size();++t){for(std::size_t c=0;c<2;++c)m.gain[t][c]+=m.step[t][c];m.gate[t]+=m.gate_step[t];for(std::size_t j=0;j<8;++j)m.send[t][j]+=m.send_step[t][j];}m.master+=m.master_step;}
    m.ramp-=ramp;if(ramp&&!m.ramp){m.gain=m.target;m.gate=m.gate_target;m.send=m.send_target;m.master=m.master_target;}
}
void AheadRenderer::record_control(const Control& c) noexcept {
    Delivered heard;if(read_head(heard)){std::size_t keep{};for(std::size_t i=0;i<control_count_;++i)if(controls_[i].serial>heard.control_serial)controls_[keep++]=controls_[i];control_count_=keep;}
    if(control_count_==controls_.size()){engine_->processing_fault_=true;return;}controls_[control_count_++]=c;control_serial_=c.serial;
}
void AheadRenderer::record_mix(const MixerUpdate& m) noexcept {
    Delivered heard;if(read_head(heard)){std::size_t keep{};for(std::size_t i=0;i<mix_count_;++i)if(mixes_[i].serial>heard.mix_serial)mixes_[keep++]=mixes_[i];mix_count_=keep;}
    if(mix_count_==mixes_.size()){engine_->processing_fault_=true;return;}mixes_[mix_count_++]=m;mix_serial_=m.serial;
}
void AheadRenderer::replay(const Delivered& delivered) noexcept {
    std::size_t keep{};for(std::size_t i=0;i<control_count_;++i)if(controls_[i].serial>delivered.control_serial)controls_[keep++]=controls_[i];control_count_=keep;
    keep=0;for(std::size_t i=0;i<mix_count_;++i)if(mixes_[i].serial>delivered.mix_serial)mixes_[keep++]=mixes_[i];mix_count_=keep;
    control_serial_=delivered.control_serial;mix_serial_=delivered.mix_serial;
    for(std::size_t i=0;i<control_count_;++i){engine_->apply_control(controls_[i]);control_serial_=controls_[i].serial;}
    for(std::size_t i=0;i<mix_count_;++i){engine_->set_mix(mixes_[i],true);mix_serial_=mixes_[i].serial;}
}
void AheadRenderer::run() noexcept {
#ifdef _WIN32
    DWORD task{};HANDLE audio=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
#endif
    auto revision=engine_->control_revision();bool primed=false;
    while(!quit_.load(std::memory_order_acquire)) {
        // A timed-out channel helper may still own DSP/PDC memory. Only stop()
        // joins it; never rebase or retry the graph after the fault is latched.
        if(engine_->processing_fault_.load(std::memory_order_acquire)){signals_->wake.wait();continue;}
        if(!engine_->anticipation_safe()){engine_->processing_fault_=true;signals_->wake.wait();continue;}
        const auto desired=engine_->control_revision();
        if(desired!=revision){Delivered head;if(!read_head(head)){signals_->wake.wait();continue;}
            engine_->rebase_head(head.head);engine_->restore_mix(head.mix);replay(head);revision=desired;++engine_->ahead_invalidations_;}
        const auto write=write_.load(std::memory_order_relaxed),next=(write+1)%static_cast<std::uint32_t>(packets_.size());
        if(next==read_.load(std::memory_order_acquire)||engine_->processing_fault_.load(std::memory_order_relaxed)){signals_->wake.wait();continue;}
        auto& packet=packets_[write];const auto start=std::chrono::steady_clock::now();
        engine_->process(nullptr,packet.audio.data(),device_frames_);
        const auto ns=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
        engine_->ahead_max_process_ns_.store(std::max(ns,engine_->ahead_max_process_ns_.load(std::memory_order_relaxed)),std::memory_order_relaxed);
        if(engine_->control_revision()!=revision)continue;
        packet.first=engine_->last_render_start_;packet.last=engine_->rt_;packet.mix=engine_->last_render_mix_;packet.last_mix=engine_->mix_head();packet.meters=engine_->last_render_meters_;packet.revision=revision;packet.control_serial=control_serial_;packet.mix_serial=mix_serial_;
        write_.store(next,std::memory_order_release);if(!primed){signals_->ready.post();primed=true;}
    }
#ifdef _WIN32
    if(audio)AvRevertMmThreadCharacteristics(audio);
#endif
}
void AheadRenderer::process(const float* input,float* output,std::uint32_t frames,std::uint32_t flags) noexcept {
    if(!active_){engine_->process(input,output,frames,flags);return;}
    ++engine_->callbacks_;const auto channels=engine_->config().output_channels;
    if(!output||!frames||frames>engine_->config().max_block){++engine_->invalid_blocks_;if(output)std::fill_n(output,static_cast<std::size_t>(frames)*channels,0.f);return;}
    auto minimum=engine_->min_frames_.load(std::memory_order_relaxed);if(!minimum||frames<minimum)engine_->min_frames_=frames;if(frames>engine_->max_frames_.load(std::memory_order_relaxed))engine_->max_frames_=frames;
    std::fill_n(output,static_cast<std::size_t>(frames)*channels,0.f);
    if(engine_->processing_fault_.load(std::memory_order_relaxed)){consumer_head_.head.playback=PlaybackState::paused;publish_head();engine_->publish_delivered(consumer_head_.head,{});signals_->wake.post();return;}
    const auto revision=engine_->control_revision();auto result=consumer_head_;MixerMeters meters;std::uint32_t copied{};
    const auto bound=packets_.size()+static_cast<std::size_t>(frames/device_frames_)+2;
    for(std::size_t attempt=0;copied<frames&&attempt<bound;++attempt) {
        const auto read=read_.load(std::memory_order_relaxed);if(read==write_.load(std::memory_order_acquire))break;
        const auto& packet=packets_[read];
        if(packet.revision!=revision){partial_=0;read_.store((read+1)%static_cast<std::uint32_t>(packets_.size()),std::memory_order_release);continue;}
        if(!partial_)result={packet.first,packet.mix,packet.control_serial,packet.mix_serial};
        const auto count=std::min(frames-copied,device_frames_-partial_);
        std::copy_n(packet.audio.data()+static_cast<std::size_t>(partial_)*channels,static_cast<std::size_t>(count)*channels,output+static_cast<std::size_t>(copied)*channels);
        copied+=count;partial_+=count;if(partial_<device_frames_)advance(result,count);
        for(std::size_t t=0;t<max_mixer_tracks;++t){meters.tracks[t].left=std::max(meters.tracks[t].left,packet.meters.tracks[t].left);meters.tracks[t].right=std::max(meters.tracks[t].right,packet.meters.tracks[t].right);}
        meters.master.left=std::max(meters.master.left,packet.meters.master.left);meters.master.right=std::max(meters.master.right,packet.meters.master.right);
        if(partial_==device_frames_){result.head=packet.last;result.mix=packet.last_mix;partial_=0;read_.store((read+1)%static_cast<std::uint32_t>(packets_.size()),std::memory_order_release);}
    }
    if(engine_->control_revision()!=revision||engine_->processing_fault_.load(std::memory_order_relaxed)){std::fill_n(output,static_cast<std::size_t>(frames)*channels,0.f);partial_=0;++engine_->ahead_underruns_;}
    else {if(copied<frames)++engine_->ahead_underruns_;if(copied){consumer_head_=result;publish_head();engine_->publish_delivered(result.head,meters);}}
    const auto write=write_.load(std::memory_order_acquire),read=read_.load(std::memory_order_relaxed);
    const auto queued=write>=read?write-read:write+static_cast<std::uint32_t>(packets_.size())-read;
    engine_->ahead_buffered_frames_=queued*device_frames_-std::min(partial_,queued*device_frames_);signals_->wake.post();
}
}
