#pragma once
#include <mrs/ahead_renderer.hpp>
#include <mrs/mixed_renderer.hpp>
#include <chrono>
#include <thread>
#include <stdexcept>
namespace mrs::audio {
// Control-side pacing for manually driven PCM tests. Never used by a device
// callback: producer starvation/recovery has separate deliberately unpaced tests.
struct AheadRendererTestAccess {
    static void wait_full(const AheadRenderer& renderer){
        const auto full=[&]{
            if(renderer.mixed_&&renderer.mixed_->active()){
                const auto& mixed=*renderer.mixed_;
                const auto write=mixed.write_.load(std::memory_order_acquire);
                return (write+1)%mixed.packets_.size()==mixed.read_.load(std::memory_order_acquire);
            }
            const auto write=renderer.write_.load(std::memory_order_acquire);
            return !renderer.packets_.empty()&&(write+1)%renderer.packets_.size()==renderer.read_.load(std::memory_order_acquire);
        };
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!full()){
            if(std::chrono::steady_clock::now()>=until)throw std::runtime_error("PCM producer did not refill test queue");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};
}
