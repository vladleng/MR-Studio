#pragma once
#include "Desktop.h"
#include "LiveTheme.h"
#include "LivePlaybackState.h"
#include "LiveInputPilotPanel.h"

namespace ui {
// Persistent show-channel controls; processing uses the shared Core mix API.
class LiveMixer final : public juce::Component {
  public:
    explicit LiveMixer(Desktop &desktop) : owner(desktop) {
        setComponentID("live-mixer");
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(false, true);
    }
    ~LiveMixer() override { viewport.setViewedComponent(nullptr, false); }
    void resized() override {
        viewport.setBounds(getLocalBounds());
        int x = 0;
        for (auto &strip : strips) { strip->setBounds(x, 0, 118, juce::jmax(180, getHeight() - 18)); x += 118; }
        body.setSize(juce::jmax(x, getWidth()), juce::jmax(180, getHeight() - 18));
    }
    void paint(juce::Graphics &g) override {
        const auto tokens = liveReferenceTokens();
        g.fillAll(tokens.colour(ColourRole::panel));
        g.setColour(tokens.colour(ColourRole::border)); g.drawHorizontalLine(0, 0.f, static_cast<float>(getWidth()));
    }
    void sync() {
        if (!owner.liveDocument) return;
        const auto state = owner.liveDocument->interfaceState;
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
        for (auto &strip : strips) strip->sync();
    }
  private:
    class Strip final : public juce::Component, public juce::SettableTooltipClient {
      public:
        Strip(Desktop &desktop, juce::String channel) : owner(desktop), id(std::move(channel)),
            gain(kit::ParameterSpec{"gain", "Level", kit::Unit::db, -60, 12, 0, .1, kit::Mapping::linear, 1}, true) {
            for (auto *c : std::array<juce::Component *, 6>{&name, &inputFx, &gain, &pan, &mute, &solo}) addAndMakeVisible(c);
            inputFx.setComponentID("live-mixer-input-" + id);
            inputFx.setTooltip("Select a physical ASIO channel input, monitoring, FX and hardware outputs");
            inputFx.onClick = [this] { openLiveInputPilot(owner,id); };
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
        void sync() {
            const auto state = owner.liveDocument->interfaceState;
            const auto channel = live::find(state, "channels", id);
            name.setText(channel["name"].toString(), juce::dontSendNotification);
            const auto kind=channel["type"].toString();
            inputFx.setEnabled(kind!="Playback");
            const bool activeInput=owner.liveInputPilotTrack && owner.liveInputPilotChannel==id;
            bool monitoring=false;
            if(activeInput)
                for(const auto& track:owner.project()->tracks)
                    if(track.id==*owner.liveInputPilotTrack) { monitoring=track.input_monitor;break; }
            const auto profile=channel["liveInput"];
            inputFx.setButtonText(activeInput ? (monitoring ? "Monitor ON / FX" : "Monitor OFF / FX")
                : profile.isObject()
                    ? "In " + juce::String(static_cast<int>(profile["input"])+1) +
                      (static_cast<bool>(profile["stereo"]) ? " ST / FX" : " / FX")
                    : "Input / FX");
            inputFx.setTooltip(profile.isObject()
                ? "ASIO " + profile["deviceName"].toString() + " / " + profile["inputName"].toString()
                : "Assign ASIO input, monitoring, VST3 FX and hardware output to this show channel");
            if (!gain.gesturing() && !gain.hasKeyboardFocus(true)) gain.setValue(static_cast<double>(channel["gain"]));
            if (!pan.isMouseButtonDown() && !pan.hasKeyboardFocus(true)) pan.setValue(live::channelPan(channel), juce::dontSendNotification);
            mute.setToggleState(static_cast<bool>(channel["mute"]), juce::dontSendNotification);
            solo.setToggleState(static_cast<bool>(channel["solo"]), juce::dontSendNotification);
            meter = 0.f;
            const auto item = state["currentItem"].toString();
            const auto rows = live::playbackRows(state, item);
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (owner.livePlaybackReady() && i < owner.peaks.size() &&
                    (live::playbackChannelId(state, rows[i]) == id ||
                     (id == live::playbackContainer(state) && !static_cast<bool>(rows[i]["cue"]))))
                    meter = juce::jmax(meter, juce::jmax(owner.peaks[i].left, owner.peaks[i].right));
            const bool liveInput=owner.liveInputPilotTrack && id==owner.liveInputPilotChannel;
            if(liveInput) {
                const auto p=owner.project();
                for(std::size_t i=0;i<p->tracks.size();++i)
                    if(p->tracks[i].id==*owner.liveInputPilotTrack && i<owner.peaks.size()) {
                        meter=juce::jmax(owner.peaks[i].left,owner.peaks[i].right);
                        break;
                    }
            }
            setTooltip(liveInput ? "Live Input via shared Core / VST3" :
                       channel["type"].toString() == "Playback" ? "Show-wide channel mix" :
                       "No input or processor assigned");
            repaint();
        }
        void resized() override {
            auto area = getLocalBounds().reduced(6);
            name.setBounds(area.removeFromTop(24));
            inputFx.setBounds(area.removeFromTop(24).reduced(0,1));
            auto buttons = area.removeFromTop(28);
            mute.setBounds(buttons.removeFromLeft(48).reduced(1)); solo.setBounds(buttons.reduced(1));
            pan.setBounds(area.removeFromTop(40));
            gain.setBounds(area.withTrimmedRight(10));
        }
        void paint(juce::Graphics &g) override {
            const auto tokens = liveReferenceTokens();
            g.fillAll(tokens.colour(ColourRole::panel));
            g.setColour(tokens.colour(ColourRole::border)); g.drawRect(getLocalBounds());
            const auto height = juce::jmax(0, getHeight() - 110);
            g.fillRect(getWidth() - 12, 104, 4, height);
            g.setColour(tokens.colour(ColourRole::on));
            const int level = juce::roundToInt(height * juce::jlimit(0.f, 1.f, meter));
            g.fillRect(getWidth() - 12, 104 + height - level, 4, level);
        }
      private:
        void commit() { owner.run([&] { owner.setLiveChannelMix(id, gain.value(), pan.getValue(), mute.getToggleState(), solo.getToggleState()); }); }
        Desktop &owner;
        juce::String id;
        juce::Label name;
        juce::TextButton inputFx{"Input / FX"};
        kit::MRFader gain;
        juce::Slider pan;
        juce::TextButton mute{"M"}, solo{"S"};
        float meter{};
    };
    Desktop &owner;
    juce::Viewport viewport;
    juce::Component body;
    juce::String signature;
    std::vector<std::unique_ptr<Strip>> strips;
};
} // namespace ui
