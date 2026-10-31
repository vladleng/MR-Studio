#pragma once
#include "Desktop.h"
#include "LiveTheme.h"
#include "LivePlaybackState.h"
#include "LiveChannelControls.h"
#include <cmath>
#include <map>

namespace ui {
namespace live {
// Stereo peaks are composited component-wise, including Cue/Click exclusion
// below. This is UI-only: no new DSP/graph work or audio-callback access.
inline void includeLiveMeterPeak(mrs::audio::StereoPeak& destination,
                                 const mrs::audio::StereoPeak& sample) noexcept {
    destination.left=std::max(destination.left,sample.left);
    destination.right=std::max(destination.right,sample.right);
}
}
// Persistent show-channel controls; processing uses the shared Core mix API.
class LiveMixer final : public juce::Component {
  public:
    explicit LiveMixer(Desktop &desktop) : owner(desktop) {
        setComponentID("live-mixer");
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(true, true);
    }
    ~LiveMixer() override { viewport.setViewedComponent(nullptr, false); }
    void resized() override {
        viewport.setBounds(getLocalBounds());
        int x = 0;
        for (auto &strip : strips) { strip->setBounds(x, 0, 118, juce::jmax(260, getHeight() - 18)); x += 118; }
        body.setSize(juce::jmax(x, getWidth()), juce::jmax(260, getHeight() - 18));
    }
    void paint(juce::Graphics &g) override {
        const auto tokens = liveReferenceTokens();
        g.fillAll(tokens.colour(ColourRole::panel));
        g.setColour(tokens.colour(ColourRole::border)); g.drawHorizontalLine(0, 0.f, static_cast<float>(getWidth()));
    }
    void sync() {
        if (!owner.liveDocument) return;
        const auto state = owner.liveDocument->interfaceState;
        // Structural show lanes change only on show edits/open, never on meter
        // ticks. Keep the card controls stable when transport is running.
        if (observedGeneration != owner.liveDocumentGeneration ||
            observedRevision != owner.liveUiRevision || signature.isEmpty()) {
            std::vector<juce::String> channels;
            for (const auto &lane : live::playbackLanes(state))
                if (!lane.child) channels.push_back(lane.channel);
            juce::String next;
            for (const auto &channel : channels) next += channel + "|";
            if (next != signature) {
                signature = next; strips.clear();
                for (const auto &channel : channels) {
                    auto strip = std::make_unique<Strip>(owner, channel);
                    body.addAndMakeVisible(*strip); strips.push_back(std::move(strip));
                }
                resized();
            }
            observedGeneration=owner.liveDocumentGeneration;
            observedRevision=owner.liveUiRevision;
        }
        // Previously each strip rebuilt playbackRows and searched the full
        // Core track list on *every* 15 Hz UI tick (N strips * N stems).
        // Snapshot playback peaks once, then let each strip read its own pair.
        std::map<juce::String,mrs::audio::StereoPeak> meters;
        if (owner.livePlaybackReady()) {
            const auto item=state["currentItem"].toString();
            const auto rows=live::playbackRows(state,item);
            const auto container=live::playbackContainer(state);
            for(std::size_t i=0;i<rows.size() && i<owner.peaks.size();++i) {
                const auto channel=live::playbackChannelId(state,rows[i]);
                live::includeLiveMeterPeak(meters[channel],owner.peaks[i]);
                if(!static_cast<bool>(rows[i]["cue"]))
                    live::includeLiveMeterPeak(meters[container],owner.peaks[i]);
            }
        }
        std::map<std::string,juce::String> coreChannels;
        for (const auto &strip:strips)
            if(const auto id=owner.liveInputTrack(strip->id))
                coreChannels.emplace(id->value,strip->id);
        const auto p=owner.project();
        for(std::size_t i=0;i<p->tracks.size() && i<owner.peaks.size();++i)
            if(const auto it=coreChannels.find(p->tracks[i].id.value);it!=coreChannels.end())
                meters[it->second]=owner.peaks[i]; // Live input overrides Playback.
        for (auto &strip : strips) strip->sync(meters);
    }
  private:
    class Strip final : public juce::Component, public juce::SettableTooltipClient {
      public:
        Strip(Desktop &desktop, juce::String channel) : owner(desktop), id(std::move(channel)),
            inputs(owner,id,LiveChannelControls::Surface::mixer), gain(kit::ParameterSpec{"gain", "Level", kit::Unit::db, -60, 12, 0, .1, kit::Mapping::linear, 1}, true) {
            for (auto *c : std::array<juce::Component *, 7>{&name, &inputs, &gain, &pan, &mute, &solo, &instrumentSelector}) addAndMakeVisible(c);
            instrumentSelector.setComponentID("live-mixer-instrument-"+id);
            instrumentSelector.onClick=[this]{inputs.showInstrumentPicker(instrumentSelector);};
            gain.setComponentID("live-mixer-gain-" + id);
            pan.setComponentID("live-mixer-pan-" + id);
            mute.setComponentID("live-mixer-mute-" + id);
            solo.setComponentID("live-mixer-solo-" + id);
            gain.onEvent = [this](const kit::ControlEvent &event) {
                if (event.phase == kit::Phase::preview) owner.run([&] {
                    owner.previewLiveChannelMix(id, gain.value(), pan.getValue(), mute.getToggleState(), solo.getToggleState());
                });
                if (event.phase == kit::Phase::cancel) owner.app.cancel_mix_preview();
                if (event.phase == kit::Phase::commit) commit();
            };
            pan.setRange(-1, 1, .01); pan.setSliderStyle(juce::Slider::LinearHorizontal);
            pan.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
            pan.setTitle("Channel pan");
            pan.onDragEnd = [this] { commit(); };
            pan.onValueChange = [this] { if (!pan.isMouseButtonDown()) commit(); };
            for (auto *button : {&mute, &solo}) {
                button->setClickingTogglesState(true); button->onClick = [this] { commit(); };
            }
        }
        ~Strip() override { gain.onEvent = {}; owner.app.cancel_mix_preview(); }
        void sync(const std::map<juce::String,mrs::audio::StereoPeak>& meters) {
            const auto state = owner.liveDocument->interfaceState;
            const auto channel = live::find(state, "channels", id);
            name.setText(channel["name"].toString(), juce::dontSendNotification);
            const auto kind=channel["type"].toString();
            inputs.setVisible(kind!="Playback");inputs.sync();
            const auto saved=channel["liveInput"];
            const bool instrument=kind=="Instrument" && (!saved.isObject() || static_cast<int>(saved["version"])==2);
            instrumentSelector.setVisible(instrument);
            instrumentSelector.setEnabled(instrument && saved.isObject() &&
                !owner.app.recording() && owner.displayState().playback!=mrs::PlaybackState::playing);
            const auto pluginName=saved.isObject() && saved["inserts"].size()>0 ?
                saved["inserts"][0]["name"].toString() : juce::String();
            instrumentSelector.setButtonText(pluginName.isEmpty()?"Choose VSTi":pluginName);
            instrumentSelector.setTooltip(pluginName.isEmpty()?
                "Choose a VST3 instrument from the scanned catalog":"Replace VST3 instrument: "+pluginName);
            const auto target=owner.liveInputTrack(id);const bool activeInput=target.has_value();
            if (!gain.gesturing() && !gain.hasKeyboardFocus(true)) gain.setValue(static_cast<double>(channel["gain"]));
            if (!pan.isMouseButtonDown() && !pan.hasKeyboardFocus(true)) pan.setValue(live::channelPan(channel), juce::dontSendNotification);
            mute.setToggleState(static_cast<bool>(channel["mute"]), juce::dontSendNotification);
            solo.setToggleState(static_cast<bool>(channel["solo"]), juce::dontSendNotification);
            const auto found=meters.find(id);
            const float left=found==meters.end()?0.f:found->second.left;
            const float right=found==meters.end()?0.f:found->second.right;
            if(left!=meterLeft || right!=meterRight) {
                meterLeft=left;meterRight=right;
                repaint(); // Meter-only paint; idle strips do not repaint.
            }
            const bool liveInput=activeInput;
            setTooltip(liveInput ? "Automatic Audio/MIDI monitoring via shared Core / VST3" :
                       channel["type"].toString() == "Playback" ? "Show-wide channel mix" :
                       saved.isObject() && owner.liveAutoMonitorError.isNotEmpty() ?
                           "Auto-monitor unavailable: "+owner.liveAutoMonitorError :
                       "No valid Audio/MIDI input or processor assigned");
        }
        void resized() override {
            auto area = getLocalBounds().reduced(6);
            name.setBounds(area.removeFromTop(24));
            inputs.setBounds(area.removeFromTop(44));
            auto buttons = area.removeFromTop(28);
            mute.setBounds(buttons.removeFromLeft(30).reduced(1));solo.setBounds(buttons.removeFromLeft(30).reduced(1));
            pan.setBounds(area.removeFromTop(40));
            if(instrumentSelector.isVisible())
                instrumentSelector.setBounds(area.removeFromTop(22).reduced(1,0));
            gain.setBounds(area.withTrimmedRight(10));
        }
        void paint(juce::Graphics &g) override {
            const auto tokens = liveReferenceTokens();
            g.fillAll(tokens.colour(ColourRole::panel));
            g.setColour(tokens.colour(ColourRole::border)); g.drawRect(getLocalBounds());
            // MR Studio mixer uses independent L/R peak bars aligned with
            // the long fader. Keep the MR Live palette, no new meter colours.
            const int height = juce::jmax(0, getHeight() - 116);
            const auto meterBackground=tokens.colour(ColourRole::panel);
            const auto active=tokens.colour(ColourRole::on);
            const int baseY=102, baseX=getWidth()-17;
            for (int i=0;i<2;++i) {
                const int x=baseX+i*6;
                const float value=juce::jlimit(0.f,1.f,i==0?meterLeft:meterRight);
                // dB-distributed visual scale comparable to MR Studio,
                // retaining actual shared-Core L/R peak values.
                const float db=value>0.f?20.f*std::log10(value):-60.f;
                const float fraction=juce::jlimit(0.f,1.f,(db+60.f)/60.f);
                const int lit=juce::roundToInt(height*fraction);
                g.setColour(meterBackground.darker(.2f));g.fillRect(x,baseY,4,height);
                g.setColour(active);g.fillRect(x,baseY+height-lit,4,lit);
            }
        }
      private:
        void commit() { owner.run([&] { owner.setLiveChannelMix(id, gain.value(), pan.getValue(), mute.getToggleState(), solo.getToggleState()); }); }
        Desktop &owner;
        juce::String id;
        juce::Label name;
        LiveChannelControls inputs;
        juce::TextButton instrumentSelector{"Choose VSTi"};
        kit::MRFader gain;
        juce::Slider pan;
        juce::TextButton mute{"M"}, solo{"S"};
        float meterLeft{},meterRight{};
    };
    Desktop &owner;
    juce::Viewport viewport;
    juce::Component body;
    juce::String signature;
    std::uint64_t observedGeneration{~std::uint64_t{0}},observedRevision{~std::uint64_t{0}};
    std::vector<std::unique_ptr<Strip>> strips;
};
} // namespace ui
