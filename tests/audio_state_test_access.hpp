#pragma once
#include <mrs/audio.hpp>
namespace mrs::audio {
// Tests only: device MUST be stopped/manually driven. No production injection API.
struct AudioEngineTestAccess {
    static void processing_fault(AudioEngine& engine) {engine.processing_fault_.store(true);}
    static void worker_timeout(AudioEngine& engine) {
        engine.worker_timeouts_.fetch_add(1);
        engine.processing_fault_.store(true);
    }
    struct Busy {
        AudioEngine& engine;
        explicit Busy(AudioEngine& e):engine(e){engine.sequence_.fetch_add(1);}
        ~Busy(){engine.sequence_.fetch_add(1);}
        Busy(const Busy&)=delete;
        Busy& operator=(const Busy&)=delete;
    };
};
}
