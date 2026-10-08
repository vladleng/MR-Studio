#include <mrs/mixed_renderer.hpp>
#include <mrs/processing.hpp>
#include <mrs/channel_workers.hpp>
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
class MixedSignal {
#ifdef _WIN32
    HANDLE handle_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
public:
    MixedSignal(){if(!handle_)throw std::runtime_error("mixed process event creation failed");}
    ~MixedSignal(){CloseHandle(handle_);}
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
struct MixedRenderer::Signals {MixedSignal wake,ready;};
MixedRenderer::MixedRenderer(std::shared_ptr<AudioEngine> engine,std::uint32_t frames,std::uint32_t window)
    :device_(std::move(engine)),signals_(std::make_unique<Signals>()),frames_(frames),window_(window) {
    const auto& g=device_->graph_;
    bool playback=false;
    for(const auto& v:g.voices)if(v.mixer_track!=no_mixer_track&&device_->domains_.channels[v.mixer_track].owner==ProcessingDomains::Owner::ahead)playback=true;
    if(!window||!playback||device_->config_.processing_workers<2)return;
    for(std::size_t t=0;t<g.mixer.size();++t)if(device_->domains_.channels[t].owner==ProcessingDomains::Owner::ahead){
        ahead_channels_[t]=true;device_->domain_edges_[t]=edges_;edges_+=1+g.sends[t].size();}
    const auto slots=(window+frames-1)/frames+1;
    const auto channels=device_->config_.output_channels;
    const auto bytes=device_->ahead_memory_bytes_.load()+sizeof(*this)+sizeof(Signals)+static_cast<std::uint64_t>(slots)*
        (sizeof(Packet)+edges_*frames*channels*sizeof(float))+edges_*device_->config_.max_block*channels*sizeof(float)+frames*channels*sizeof(float);
    if(bytes>16*1024*1024)throw std::invalid_argument("mixed process PCM exceeds 16 MiB");
    device_->ahead_memory_bytes_=bytes;
    input_.resize(edges_*device_->config_.max_block*channels);
    output_.resize(static_cast<std::size_t>(frames)*channels);
    packets_.resize(slots);for(auto& p:packets_)p.pcm.resize(edges_*frames*channels);
    auto graph=g;graph.monitor.clear();graph.recording.reset();graph.recordings.clear();graph.midi_recordings.clear();
    graph.external_midi.reset();graph.external_midi_tracks.clear();graph.external_midi_channels.clear(); // only heard/device scheduler emits hardware events
    graph.click.playback=graph.click.recording=false;graph.count_frames=0;
    // The producer clone owns only prerecorded channels. Retaining live MIDI
    // flags here falsely fails anticipation_safe() and silences the device.
    graph.live_midi.assign(graph.mixer.size(),false);
    for(std::size_t t=0;t<graph.midi_events.size();++t)if(!ahead_channels_[t])graph.midi_events[t].clear();
    for(std::size_t t=0;t<graph.midi_notes.size();++t)if(!ahead_channels_[t])graph.midi_notes[t].clear();
    graph.voices.clear();std::vector<std::size_t> voices;
    for(std::size_t i=0;i<g.voices.size();++i)if(g.voices[i].mixer_track!=no_mixer_track&&ahead_channels_[g.voices[i].mixer_track]){graph.voices.push_back(g.voices[i]);voices.push_back(i);}
    ahead_=std::make_unique<AudioEngine>();auto config=device_->config_;
    // One producer participant, remaining configured participants on device.
    config.processing_workers=1;
    auto initial=device_->rt_;initial.playback=PlaybackState::paused;
    ahead_->prepare(config,std::move(graph),initial);
    ahead_->profile_=device_->profile_;ahead_->set_profiling(device_->profile_->enabled.load(std::memory_order_relaxed));
    ahead_->graph_.processors.reset();ahead_->graph_.master_inserts.reset();
    for(std::size_t t=0;t<g.mixer.size();++t)if(!ahead_channels_[t]&&!ahead_->graph_.inserts.empty())ahead_->graph_.inserts[t].reset();
    // One ReadAhead cursor per voice; callback will no longer begin/read/end
    // these cursors. Control-thread primes still reach the original handle.
    for(std::size_t i=0;i<voices.size();++i)ahead_->graph_.voices[i].stream=g.voices[voices[i]].stream;
    ahead_->domain_owned_=ahead_channels_;ahead_->domain_edges_=device_->domain_edges_;
    ahead_->domain_stride_=frames; ahead_->speculative_=true;
}
MixedRenderer::~MixedRenderer(){stop();}
bool MixedRenderer::start() {
    if(!ahead_)return false;
    if(producer_.joinable())throw std::logic_error("mixed producer already running");
    while(signals_->ready.wait_for(0)){}
    for(std::size_t t=0;t<device_->graph_.mixer.size();++t)
        if(ahead_channels_[t]&&!device_->graph_.inserts.empty()&&device_->graph_.inserts[t]&&!device_->graph_.inserts[t]->anticipation_safe())return false;
    device_->consume_controls();MixerUpdate mix;for(int n=0;n<7&&device_->mixer_controls_.pop(mix);++n)device_->set_mix(mix,true);
    sequence_=0;read_=0;write_=0;latest_=0;quit_=false;
    contexts_[0].context={device_->rt_,device_->mix_head(),device_->control_revision(),0};
    std::unique_ptr<ChannelWorkers> callback_workers;
    if(device_->workers_&&device_->workers_->count()>1)
        callback_workers=std::make_unique<ChannelWorkers>(device_->workers_->count()-1,device_->config_.worker_mmcss);
    saved_workers_=std::move(device_->workers_);device_->workers_=std::move(callback_workers);
    for(std::size_t t=0;t<device_->graph_.mixer.size();++t)device_->domain_owned_[t]=!ahead_channels_[t];
    device_->domain_stride_=device_->config_.max_block;device_->domain_input_=input_.data();device_->mixed_owner_=this;
    active_=true;device_->anticipation_active_=true;device_->mixed_anticipation_=true;
    try{producer_=std::thread([this]{run();});}
    catch(...){stop();throw;}
    if(!signals_->ready.wait_for(1000)){stop();throw std::runtime_error("mixed process priming timed out");}
    return true;
}
void MixedRenderer::stop() noexcept {
    quit_=true;signals_->wake.post();if(producer_.joinable())producer_.join();
    if(active_){
        ahead_->quiesce();device_->quiesce();
        ahead_->rebase_head(device_->rt_);ahead_->restore_mix(device_->mix_head());
        device_->mixed_owner_=nullptr;device_->domain_input_=nullptr;device_->domain_owned_.fill(true);
        device_->reset_compensation();device_->anticipation_active_=false;device_->ahead_buffered_frames_=0;
        device_->mixed_anticipation_=false;
        device_->workers_.reset();device_->workers_=std::move(saved_workers_);
    }
    active_=false;
}
bool MixedRenderer::read_context(Context& result) noexcept {
    for(unsigned attempt=0;attempt<3;++attempt){const auto i=latest_.load(std::memory_order_acquire);int free=0;
        if(!contexts_[i].owner.compare_exchange_strong(free,1,std::memory_order_acquire))continue;
        if(i==latest_.load(std::memory_order_acquire)){result=contexts_[i].context;contexts_[i].owner.store(0,std::memory_order_release);return true;}
        contexts_[i].owner.store(0,std::memory_order_release);
    }return false;
}
void MixedRenderer::publish_context(const Context& context) noexcept {
    const auto latest=latest_.load(std::memory_order_relaxed);
    for(unsigned i=0;i<3;++i)if(i!=latest){int free=0;
        if(!contexts_[i].owner.compare_exchange_strong(free,-1,std::memory_order_acquire))continue;
        contexts_[i].context=context;contexts_[i].owner.store(0,std::memory_order_release);latest_.store(i,std::memory_order_release);return;}
}
void MixedRenderer::run() noexcept {
#ifdef _WIN32
    DWORD task{};HANDLE audio=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
#endif
    Context head;std::uint64_t revision=~std::uint64_t{},sequence=0,reported_disk=0;bool primed=false;
    while(!quit_.load(std::memory_order_acquire)) {
        if(ahead_->processing_fault_.load(std::memory_order_acquire)||!ahead_->anticipation_safe()){
            device_->processing_fault_=true;signals_->wake.wait();continue;}
        if(!read_context(head)){signals_->wake.wait();continue;}
        // Enqueued commands are not necessarily applied. Only a callback
        // snapshot may associate their generation with transport/mixer state.
        if(head.revision!=device_->control_revision()){signals_->wake.wait();continue;}
        if(head.revision!=revision||head.sequence>sequence){
            ahead_->rebase_head(head.head);ahead_->restore_mix(head.mix);sequence=head.sequence;
            if(primed)++device_->ahead_invalidations_;revision=head.revision;}
        const auto w=write_.load(std::memory_order_relaxed),next=(w+1)%static_cast<unsigned>(packets_.size());
        if(next==read_.load(std::memory_order_acquire)){signals_->wake.wait();continue;}
        auto& packet=packets_[w];const auto started=std::chrono::steady_clock::now();
        ahead_->domain_output_=packet.pcm.data();ahead_->process(nullptr,output_.data(),frames_);
        const auto disk=ahead_->disk_underruns_.load(std::memory_order_relaxed);
        device_->disk_underruns_.fetch_add(disk-reported_disk,std::memory_order_relaxed);reported_disk=disk;
        const auto ns=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count());
        device_->ahead_max_process_ns_.store(std::max(ns,device_->ahead_max_process_ns_.load(std::memory_order_relaxed)),std::memory_order_relaxed);
        if(device_->control_revision()!=revision){signals_->wake.wait();continue;}
        packet.sequence=sequence;packet.revision=revision;packet.meters=ahead_->last_render_meters_;
        sequence+=frames_;write_.store(next,std::memory_order_release);
        if(!primed){signals_->ready.post();primed=true;}
    }
#ifdef _WIN32
    if(audio)AvRevertMmThreadCharacteristics(audio);
#endif
}
void MixedRenderer::begin(std::uint32_t frames) noexcept {
    const auto revision=device_->control_revision();
    revision_=revision;
    publish_context({device_->rt_,device_->mix_head(),revision,sequence_});
    const auto channels=device_->config_.output_channels;
    const auto stride=static_cast<std::size_t>(device_->domain_stride_)*channels;
    for(std::size_t edge=0;edge<edges_;++edge)std::fill_n(input_.data()+edge*stride,static_cast<std::size_t>(frames)*channels,0.f);
    device_->domain_meters_={};std::uint32_t copied=0;
    const auto bound=packets_.size()+frames/frames_+2;
    for(std::size_t attempt=0;copied<frames&&attempt<bound;++attempt){
        const auto r=read_.load(std::memory_order_relaxed);if(r==write_.load(std::memory_order_acquire))break;
        const auto& packet=packets_[r];const auto wanted=sequence_+copied;
        if(packet.revision!=revision||packet.sequence+frames_<=wanted){read_.store((r+1)%static_cast<unsigned>(packets_.size()),std::memory_order_release);continue;}
        if(packet.sequence>wanted)break;
        const auto offset=static_cast<std::uint32_t>(wanted-packet.sequence),count=std::min(frames-copied,frames_-offset);
        for(std::size_t edge=0;edge<edges_;++edge)std::copy_n(packet.pcm.data()+edge*frames_*channels+static_cast<std::size_t>(offset)*channels,
            static_cast<std::size_t>(count)*channels,input_.data()+edge*stride+static_cast<std::size_t>(copied)*channels);
        for(std::size_t t=0;t<device_->graph_.mixer.size();++t)if(ahead_channels_[t]){
            auto& m=device_->domain_meters_[t];m.left=std::max(m.left,packet.meters.tracks[t].left);m.right=std::max(m.right,packet.meters.tracks[t].right);}
        copied+=count;if(offset+count==frames_)read_.store((r+1)%static_cast<unsigned>(packets_.size()),std::memory_order_release);
    }
    if(copied<frames)++device_->ahead_underruns_;
    sequence_+=frames;signals_->wake.post();
}
void MixedRenderer::end() noexcept {
    publish_context({device_->rt_,device_->mix_head(),revision_,sequence_});
    const auto w=write_.load(std::memory_order_acquire),r=read_.load(std::memory_order_relaxed);
    const auto queued=w>=r?w-r:w+static_cast<unsigned>(packets_.size())-r;
    auto buffered=queued*frames_;
    if(queued){const auto& packet=packets_[r];if(sequence_>=packet.sequence)buffered-=static_cast<unsigned>(std::min<std::uint64_t>(frames_,sequence_-packet.sequence));}
    device_->ahead_buffered_frames_=buffered;signals_->wake.post();
}
}
