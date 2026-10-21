#include <mrs/library_preview.hpp>
#include <mrs/audio.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace mrs::audio {
LibraryPreviewStream::LibraryPreviewStream()
    :thread_([this](std::stop_token stop){worker(stop);}){}
LibraryPreviewStream::~LibraryPreviewStream(){
    stop();
    thread_.request_stop();
    wake_.notify_all();
    if(thread_.joinable())thread_.join();
}
void LibraryPreviewStream::play(std::filesystem::path path,std::uint32_t rate,double fraction){
    if(path.empty()||rate<8000||rate>768000||!std::isfinite(fraction))
        throw std::invalid_argument("Invalid MR Library preview request");
    fraction=std::clamp(fraction,0.,.999999);
    const auto gen=generation_.fetch_add(1,std::memory_order_acq_rel)+1;
    active_.store(0,std::memory_order_release);
    position_.store(0,std::memory_order_relaxed);
    length_.store(0,std::memory_order_relaxed);
    finished_.store(0,std::memory_order_relaxed);
    failed_.store(0,std::memory_order_relaxed);
    {
        std::lock_guard lock(mutex_);
        request_={gen,std::move(path),rate,fraction};
        active_.store(gen,std::memory_order_release);
    }
    wake_.notify_all();
}
void LibraryPreviewStream::stop() noexcept {
    active_.store(0,std::memory_order_release);
    generation_.fetch_add(1,std::memory_order_acq_rel);
    wake_.notify_all();
}
LibraryPreviewStream::Status LibraryPreviewStream::status() const noexcept {
    const auto total=length_.load(std::memory_order_relaxed);
    const auto now=position_.load(std::memory_order_relaxed);
    return {active_.load(std::memory_order_acquire)!=0,
            total?static_cast<float>(std::min(1.,static_cast<double>(now)/static_cast<double>(total))):0.f,
            failed_.load(std::memory_order_acquire)!=0&&
            failed_.load(std::memory_order_acquire)==generation_.load(std::memory_order_acquire)};
}
void LibraryPreviewStream::mix(float* output,std::uint32_t frames,std::uint32_t channels,bool allowed,
    std::span<const std::size_t> outputs,const float* envelope,float level) noexcept {
    if(!allowed){active_.store(0,std::memory_order_release);return;}
    const auto gen=active_.load(std::memory_order_acquire);
    if(!gen||!output||!channels)return;
    std::uint64_t consumed=read_.load(std::memory_order_relaxed);
    const auto produced=write_.load(std::memory_order_acquire);
    // The single producer publishes generations in order. Skip a full stale
    // queue in logarithmic work instead of scanning 32768 frames on seek.
    auto first=consumed,last=produced;
    while(first<last){
        const auto middle=first+(last-first)/2;
        if(ring_[static_cast<std::size_t>(middle%ringSize)].generation<gen)first=middle+1;
        else last=middle;
    }
    consumed=first;
    std::uint64_t advanced{};
    for(std::uint32_t i=0;i<frames;++i){
        if(consumed==produced)break; // Underflow: preserve project output.
        const auto frame=ring_[static_cast<std::size_t>(consumed%ringSize)];
        if(frame.generation!=gen)break; // Preserve a newer request for the next callback.
        ++consumed;
        const auto dest=static_cast<std::size_t>(i)*channels;
        const float gain=.65f*(envelope?envelope[i]:level);
        const auto add=[&](std::size_t channel,float value){
            if(channel<channels)output[dest+channel]=std::clamp(output[dest+channel]+value,-1.f,1.f);
        };
        if(channels==1||outputs.size()==1)
            add(outputs.empty()?0:outputs[0],(frame.left+frame.right)*(.5f*gain));
        else{
            add(outputs.empty()?0:outputs[0],frame.left*gain);
            add(outputs.empty()?1:outputs[1],frame.right*gain);
        }
        ++advanced;
    }
    read_.store(consumed,std::memory_order_release);
    if(generation_.load(std::memory_order_acquire)!=gen)return;
    position_.fetch_add(advanced,std::memory_order_relaxed);
    if(consumed==write_.load(std::memory_order_acquire)&&
       finished_.load(std::memory_order_acquire)==gen){
        auto expected=gen;
        active_.compare_exchange_strong(expected,0,std::memory_order_acq_rel);
    }
}
void LibraryPreviewStream::worker(std::stop_token stop){
    std::uint64_t processed{};
    while(!stop.stop_requested()){
        Request request;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock,stop,[&]{return request_.generation!=processed;});
            if(stop.stop_requested())break;
            request=request_;
            processed=request.generation;
        }
        try{
            if(generation_.load(std::memory_order_acquire)!=request.generation)continue;
            const auto wav=inspect_wav(request.path);
            if(wav.channels<1||wav.channels>2||wav.frame_count<=0)
                throw std::runtime_error("Audition only supports mono/stereo WAV");
            const auto total=static_cast<std::uint64_t>(
                std::ceil(static_cast<double>(wav.frame_count)*request.output_rate/wav.sample_rate));
            const auto first=static_cast<std::uint64_t>(request.fraction*total);
            if(generation_.load(std::memory_order_acquire)!=request.generation)continue;
            length_.store(total,std::memory_order_release);
            position_.store(first,std::memory_order_release);
            constexpr std::uint32_t blockFrames=4096;
            std::vector<float> cache(static_cast<std::size_t>(blockFrames)*wav.channels);
            std::uint64_t cacheBegin=0,cacheCount=0;
            auto sample=[&](std::uint64_t index,std::uint32_t channel)->float{
                if(index>=static_cast<std::uint64_t>(wav.frame_count))index=static_cast<std::uint64_t>(wav.frame_count-1);
                if(cacheCount==0||index<cacheBegin||index>=cacheBegin+cacheCount){
                    cacheBegin=index;
                    cacheCount=std::min<std::uint64_t>(blockFrames,
                        static_cast<std::uint64_t>(wav.frame_count)-cacheBegin);
                    wav.read(static_cast<Sample>(cacheBegin),
                        std::span<float>(cache).first(static_cast<std::size_t>(cacheCount)*wav.channels));
                }
                return cache[static_cast<std::size_t>(index-cacheBegin)*wav.channels+channel];
            };
            const auto ratio=static_cast<double>(wav.sample_rate)/request.output_rate;
            for(std::uint64_t out=first;out<total&&!stop.stop_requested();){
                if(generation_.load(std::memory_order_acquire)!=request.generation||
                    active_.load(std::memory_order_acquire)!=request.generation)break;
                auto write=write_.load(std::memory_order_relaxed);
                if(write-read_.load(std::memory_order_acquire)>=ringSize){
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                const auto position=static_cast<double>(out)*ratio;
                const auto a=std::min<std::uint64_t>(
                    static_cast<std::uint64_t>(position),static_cast<std::uint64_t>(wav.frame_count-1));
                const auto b=std::min<std::uint64_t>(a+1,static_cast<std::uint64_t>(wav.frame_count-1));
                const auto blend=static_cast<float>(std::clamp(position-a,0.,1.));
                const auto interp=[&](std::uint32_t channel){
                    const float v0=sample(a,channel),v1=sample(b,channel);
                    const auto v=v0+(v1-v0)*blend;
                    return std::isfinite(v)?v:0.f;
                };
                Frame frame{request.generation,interp(0),0.f};
                frame.right=wav.channels==1?frame.left:interp(1);
                ring_[static_cast<std::size_t>(write%ringSize)]=frame;
                write_.store(write+1,std::memory_order_release);
                ++out;
            }
        }catch(const std::exception&){
            if(generation_.load(std::memory_order_acquire)==request.generation)
                failed_.store(request.generation,std::memory_order_release);
        }
        if(generation_.load(std::memory_order_acquire)==request.generation)
            finished_.store(request.generation,std::memory_order_release);
    }
}
} // namespace mrs::audio
