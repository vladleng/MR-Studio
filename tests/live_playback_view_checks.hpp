#pragma once
#include "../apps/studio-juce/src/LivePlaybackState.h"
#include "../apps/studio-juce/src/PlaybackWaveform.h"
#include <array>
#include <stdexcept>

namespace ui::live::test {
inline void playbackViews() {
    const auto check = [](bool pass, const char *message) { if (!pass) throw std::runtime_error(message); };
    const auto close = [](float a, float b) { return std::abs(a - b) < .00001f; };
    mrs::audio::AudioData a; a.sample_rate = 8000; a.channels = 1; a.samples.resize(8000);
    for (std::size_t i = 0; i < a.samples.size(); ++i) a.samples[i] = i % 2 ? .8f : -.8f;
    mrs::audio::AudioData b; b.sample_rate = 16000; b.channels = 2; b.samples.resize(32000);
    for (int i = 0; i < 16000; ++i) { b.samples[i * 2] = i % 2 ? -.4f : .4f; b.samples[i * 2 + 1] = i % 2 ? -.2f : .2f; }
    const mrs::audio::Waveform wa(a), wb(b);
    std::array<PlaybackWaveformSource, 2> sources{{{&wa, 8000, 0, 0, 1}, {&wb, 16000, 0, 0, 1}}};
    auto peak = averagePlaybackEnvelope(sources, .25, .5);
    check(close(peak.minimum, -.55f) && close(peak.maximum, .55f), "Average uses both stems/rates/stereo channels without PCM cancellation");
    sources[1].start = 1.; sources[1].offset = .25; sources[1].length = .5;
    peak = averagePlaybackEnvelope(sources, .25, .5);
    check(close(peak.maximum, .4f), "A delayed stem contributes silence before its timeline start");
    peak = averagePlaybackEnvelope(sources, 1.1, 1.3);
    check(close(peak.maximum, .15f), "A finished stem contributes silence without renormalizing the surviving stem");
    check(averagePlaybackEnvelope(sources, 2., 3.).maximum == 0.f, "Average is silent beyond every source");
    check(averagePlaybackEnvelope({}, 0., 1.).maximum == 0.f, "An empty container has no invented waveform");
    auto offsetData = a;
    for (std::size_t i = 0; i < offsetData.samples.size(); ++i) offsetData.samples[i] = i < 4000 ? .2f : .6f;
    const mrs::audio::Waveform offsetWave(offsetData);
    PlaybackWaveformSource trimmed{&offsetWave, 8000, 1., .5, .5};
    peak = trimmed.display(1.1, 1.2);
    check(close(peak.minimum, .6f) && close(peak.maximum, .6f), "Trim maps timeline time to the original source offset");
    peak = trimmed.display(.9, 1.1);
    check(peak.minimum == 0.f && close(peak.maximum, .6f), "An interval crossing the trim boundary includes silence");

    auto state = empty();
    const auto keys = addChannel(state, "Instrument", "Keys");
    const auto backing = addChannel(state, "Playback", "Backing");
    const auto vocal = addChannel(state, "Player", "Vocal");
    const auto drums = addChannel(state, "Playback", "Drums"), bass = addChannel(state, "Playback", "Bass");
    const auto item = addItem(state, "One");
    auto row = object(); put(row, "file", "one.wav"); put(row, "gain", 0.); put(row, "mute", false); put(row, "solo", false);
    auto drumRow = row.clone(); put(drumRow, "id", drums); auto bassRow = row.clone(); put(bassRow, "id", bass);
    put(row, "stems", juce::Array<juce::var>{drumRow, bassRow}); put(map(performance(state), "nativePlayback"), item, row);
    saveItem(state, item);
    auto lanes = playbackLanes(state);
    check(lanes.size() == 3 && lanes[0].channel == keys && lanes[1].channel == backing && lanes[1].container && lanes[2].channel == vocal,
          "Collapsed Playback replaces its flat stem channels with one parent lane");
    put(performance(state), "stemsExpanded", true);
    const auto before = canonical(state);
    lanes = playbackLanes(state);
    check(lanes.size() == 6 && lanes[1].container && lanes[2].channel == backing && lanes[2].child &&
              lanes[3].channel == drums && lanes[3].child && lanes[4].channel == bass && lanes[4].child && lanes[5].channel == vocal,
          "Expansion inserts every stem, including the primary WAV, directly under Playback");
    check(canonical(state) == before && !itemDirty(state, item), "Display projection/fold does not rewrite item content, IDs or media");
}
} // namespace ui::live::test
