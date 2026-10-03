#include <mrs/core.hpp>
#include <stdexcept>
#include <utility>

namespace mrs {
MockTransport::MockTransport(Timeline timeline) : timeline_(std::move(timeline)) {}
void MockTransport::rebind_timeline(Timeline timeline) {
    (void)timeline.to_ticks(state_.sample);
    timeline_ = std::move(timeline);
    commit(state_);
}
TransportState MockTransport::state() const { return state_; }
void MockTransport::commit(TransportState next) {
    next.musical = timeline_.musical_position(timeline_.to_ticks(next.sample));
    if (next == state_) return;
    state_ = next;
    changes_.publish(state_);
}
void MockTransport::play() {
    auto next = state_;
    next.playback = PlaybackState::playing;
    commit(next);
}
void MockTransport::pause() {
    if (state_.playback != PlaybackState::playing) return;
    auto next = state_;
    next.playback = PlaybackState::paused;
    commit(next);
}
void MockTransport::stop() {
    auto next = state_;
    next.playback = PlaybackState::stopped;
    next.sample = 0;
    commit(next);
}
void MockTransport::seek(Sample sample) {
    if (sample < 0 || sample > max_sample) throw std::invalid_argument("invalid seek");
    auto next = state_;
    next.sample = sample;
    commit(next);
}
void MockTransport::set_loop(std::optional<LoopRange> loop) {
    if (loop && (loop->start < 0 || loop->start >= loop->end || loop->end > max_sample))
        throw std::invalid_argument("invalid loop range");
    auto next = state_;
    next.loop = loop;
    commit(next);
}
Connection MockTransport::subscribe(std::function<void(const TransportState&)> callback) {
    return changes_.subscribe(std::move(callback));
}
void MockTransport::advance(Sample frames) {
    if (frames < 0 || frames > max_sample) throw std::invalid_argument("invalid advance");
    if (state_.playback != PlaybackState::playing || frames == 0) return;
    auto next = state_;
    // Looping retains overshoot even when one advance spans multiple cycles.
    if (next.loop && next.sample >= next.loop->end) {
        next.sample = next.loop->start + (next.sample - next.loop->start) % (next.loop->end - next.loop->start);
    }
    if (next.loop && frames >= next.loop->end - next.sample) {
        const auto remaining = frames - (next.loop->end - next.sample);
        next.sample = next.loop->start + remaining % (next.loop->end - next.loop->start);
    } else {
        if (frames > max_sample - next.sample) throw std::out_of_range("transport overflow");
        next.sample += frames;
    }
    commit(next);
}
} // namespace mrs
