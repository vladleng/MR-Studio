#pragma once
#include "Desktop.h"
#include "LiveTheme.h"
#include "LivePlaybackState.h"
#include "LiveChannelControls.h"
#include <cmath>

namespace ui {
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
            inputs(owner,id,LiveChannelControls::Surface::mixer), gain(kit::ParameterSpec{"gain", "Level", kit::Unit::db, -60, 12, 0, .1, kit::Mapping::linear, 1}, true) {
            for (auto *c : std::array<juce::Component *, 8>{&name, &inputs, &monitor, &gain, &pan, &mute, &solo, &instrumentSelector}) addAndMakeVisible(c);
            instrumentSelector.setComponentID("live-mixer-instrument-"+id);
            instrumentSelector.onClick=[this]{inputs.showInstrumentPicker(instrumentSelector);};
            monitor.setComponentID("live-channel-monitor-"+id);
            monitor.onClick=[this]{owner.run([&]{owner.setLiveInputMonitor(id,!monitor.getToggleState());});};
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
            inputs.setVisible(kind!="Playback");inputs.sync();monitor.setVisible(kind!="Playback");
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
            bool monitoring=false;
            if(target)for(const auto& track:owner.project()->tracks)if(track.id==*target)monitoring=track.kind==mrs::TrackKind::instrument?track.midi_monitor:track.input_monitor;
            monitor.setToggleState(monitoring,juce::dontSendNotification);
            monitor.setEnabled(channel["liveInput"].isObject()&&!owner.app.recording()&&owner.displayState().playback!=mrs::PlaybackState::playing);
            monitor.setTooltip(monitoring?"Monitor ON":"Monitor OFF");
            if (!gain.gesturing() && !gain.hasKeyboardFocus(true)) gain.setValue(static_cast<double>(channel["gain"]));
            if (!pan.isMouseButtonDown() && !pan.hasKeyboardFocus(true)) pan.setValue(live::channelPan(channel), juce::dontSendNotification);
            mute.setToggleState(static_cast<bool>(channel["mute"]), juce::dontSendNotification);
            solo.setToggleState(static_cast<bool>(channel["solo"]), juce::dontSendNotification);
            meterLeft = meterRight = 0.f;
            const auto item = state["currentItem"].toString();
            const auto rows = live::playbackRows(state, item);
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (owner.livePlaybackReady() && i < owner.peaks.size() &&
                    (live::playbackChannelId(state, rows[i]) == id ||
                     (id == live::playbackContainer(state) && !static_cast<bool>(rows[i]["cue"]))))
                    {meterLeft=juce::jmax(meterLeft,owner.peaks[i].left);
                     meterRight=juce::jmax(meterRight,owner.peaks[i].right);}
            const bool liveInput=activeInput;
            if(liveInput) {
                const auto p=owner.project();
                for(std::size_t i=0;i<p->tracks.size();++i)
                    if(p->tracks[i].id==*target && i<owner.peaks.size()) {
                        meterLeft=owner.peaks[i].left;
                        meterRight=owner.peaks[i].right;
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
            inputs.setBounds(area.removeFromTop(44));
            auto buttons = area.removeFromTop(28);
            mute.setBounds(buttons.removeFromLeft(30).reduced(1));solo.setBounds(buttons.removeFromLeft(30).reduced(1));monitor.setBounds(buttons.reduced(1));
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
            const auto background=tokens.colour(ColourRole::panel);
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
                g.setColour(background.darker(.2f));g.fillRect(x,baseY,4,height);
                g.setColour(active);g.fillRect(x,baseY+height-lit,4,lit);
            }
        }
      private:
        void commit() { owner.run([&] { owner.setLiveChannelMix(id, gain.value(), pan.getValue(), mute.getToggleState(), solo.getToggleState()); }); }
        Desktop &owner;
        juce::String id;
        juce::Label name;
        LiveChannelControls inputs;
        juce::TextButton monitor{"I"},instrumentSelector{"Choose VSTi"};
        kit::MRFader gain;
        juce::Slider pan;
        juce::TextButton mute{"M"}, solo{"S"};
        float meterLeft{},meterRight{};
    };
    Desktop &owner;
    juce::Viewport viewport;
    juce::Component body;
    juce::String signature;
    std::vector<std::unique_ptr<Strip>> strips;
};
} // namespace ui
