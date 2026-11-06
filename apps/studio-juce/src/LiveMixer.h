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
    destination.left=juce::jmax(destination.left,sample.left);
    destination.right=juce::jmax(destination.right,sample.right);
}
}
// Persistent show-channel controls; processing uses the shared Core mix API.
class LiveMixer final : public juce::Component {
    // One shared splitter, drawn across every channel. Studio-style resize:
    // the bottom bank changes height; inserts occupy the remaining space above.
    class FaderDivider final : public juce::Component {
    public:
        explicit FaderDivider(LiveMixer& mixer) : owner(mixer) {
            setComponentID("live-mixer-fader-divider");
            setTitle("Drag to resize all channel faders");
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        }
        void mouseDown(const juce::MouseEvent& e) override {
            dragging=e.mods.isLeftButtonDown();
            if(dragging) {startY=e.getScreenY();startHeight=owner.currentFaderHeight();}
        }
        void mouseDrag(const juce::MouseEvent& e) override {
            if(dragging)owner.setFaderAreaHeight(startHeight+startY-e.getScreenY());
        }
        void mouseUp(const juce::MouseEvent&) override {
            if(!dragging)return;
            dragging=false;
            owner.owner.saveSettings();
        }
        void paint(juce::Graphics& g) override {
            const auto tokens=liveReferenceTokens();
            g.fillAll(tokens.colour(ColourRole::panel));
            g.setColour(tokens.colour(ColourRole::border));
            g.fillRect(0,getHeight()/2,getWidth(),1);
        }
    private:
        LiveMixer& owner;
        int startY{},startHeight{};
        bool dragging{};
    };
  public:
    explicit LiveMixer(Desktop &desktop) : owner(desktop), divider(*this) {
        setComponentID("live-mixer");
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(true, true);
        body.addAndMakeVisible(divider);
    }
    ~LiveMixer() override { viewport.setViewedComponent(nullptr, false); }
    static constexpr int minimumTopHeight=132;
    static constexpr int minimumFaderHeight=184;
    int stripHeight() const noexcept {return juce::jmax(360,getHeight()-18);}
    int currentFaderHeight() const noexcept {
        return juce::jlimit(minimumFaderHeight,
            juce::jmax(minimumFaderHeight,stripHeight()-minimumTopHeight),
            owner.view.liveMixerFaderHeight);
    }
    void setFaderAreaHeight(int height) {
        const int next=juce::jlimit(minimumFaderHeight,
            juce::jmax(minimumFaderHeight,stripHeight()-minimumTopHeight),height);
        if(next==owner.view.liveMixerFaderHeight)return;
        owner.view.liveMixerFaderHeight=next;
        resized();
    }
    void resized() override {
        viewport.setBounds(getLocalBounds());
        const int height=stripHeight(),split=height-currentFaderHeight();
        int x=0;
        for(auto& strip:strips) {
            strip->setFaderSplitY(split);
            strip->setBounds(x,0,118,height);
            x+=118;
        }
        body.setSize(juce::jmax(x,getWidth()),height);
        divider.setBounds(0,split-4,body.getWidth(),8);
        divider.toFront(false);
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
            observedRevision != owner.liveUiRevision || !lanesInitialized) {
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
            observedRevision=owner.liveUiRevision;lanesInitialized=true;
        }
        // Previously each strip rebuilt playbackRows and searched the full
        // Core track list on *every* 15 Hz UI tick (N strips * N stems).
        // Snapshot playback peaks once, then let each strip read its own pair.
        std::map<juce::String,mrs::audio::StereoPeak> meters;
        if (owner.livePlaybackReady()) {
            const auto item=state["currentItem"].toString();
            const auto rows=live::playbackRows(state,item);
            for(std::size_t i=0;i<rows.size() && i<owner.peaks.size();++i) {
                const auto channel=live::playbackChannelId(state,rows[i]);
                const auto group=live::playbackGroupForRow(state,item,rows[i]);
                live::includeLiveMeterPeak(meters[channel],owner.peaks[i]);
                if(!static_cast<bool>(rows[i]["cue"]) && group!=channel)
                    live::includeLiveMeterPeak(meters[group],owner.peaks[i]);
            }
        }
        std::map<std::string,juce::String> coreChannels;
        for (const auto &strip:strips)
            if(const auto id=owner.liveInputTrack(strip->channelId()))
                coreChannels.emplace(id->value,strip->channelId());
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
            for (auto *c : std::array<juce::Component *, 10>{&name, &inputs, &gain, &pan, &mute, &solo,
                    &instrumentSelector, &insertTitle, &addInsert, &insertViewport}) addAndMakeVisible(c);
            insertTitle.setText("Inserts",juce::dontSendNotification);
            insertTitle.setComponentID("live-mixer-insert-title-"+id);
            insertViewport.setComponentID("live-mixer-inserts-"+id);
            insertViewport.setViewedComponent(&insertBody,false);
            insertViewport.setScrollBarsShown(true,false);
            addInsert.setComponentID("live-mixer-add-insert-"+id);
            addInsert.setTooltip("Add audio FX to this Live channel (stopped)");
            addInsert.onClick=[this]{inputs.showInsertAddMenu(addInsert);};
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
        ~Strip() override {
            gain.onEvent={};owner.app.cancel_mix_preview();
            insertViewport.setViewedComponent(nullptr,false);
        }
        const juce::String& channelId() const noexcept { return id; }
        void setFaderSplitY(int y) {
            if(faderSplitY==y)return;
            faderSplitY=y;
            resized();
        }
        void sync(const std::map<juce::String,mrs::audio::StereoPeak>& meters) {
            const auto state = owner.liveDocument->interfaceState;
            const auto channel = live::find(state, "channels", id);
            name.setText(channel["name"].toString(), juce::dontSendNotification);
            const auto kind=channel["type"].toString();
            inputs.setVisible(kind!="Playback");inputs.sync();
            const auto saved=channel["liveInput"];
            const bool instrument=kind=="Instrument" && (!saved.isObject() || static_cast<int>(saved["version"])==2);
            const bool relayout=instrumentSelector.isVisible()!=instrument ||
                insertViewport.isVisible()!=(kind!="Playback");
            instrumentSelector.setVisible(instrument);
            syncInserts(saved,kind,instrument);
            instrumentSelector.setEnabled(instrument && saved.isObject() &&
                !owner.app.recording() && owner.displayState().playback!=mrs::PlaybackState::playing);
            const auto pluginName=saved.isObject() && saved["inserts"].size()>0 ?
                saved["inserts"][0]["name"].toString() : juce::String();
            instrumentSelector.setButtonText(pluginName.isEmpty()?"Choose VSTi":pluginName);
            instrumentSelector.setTooltip(pluginName.isEmpty()?
                "Choose a VST3 instrument from the scanned catalog":"Replace VST3 instrument: "+pluginName);
            if(relayout)resized();
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
            const int split=juce::jlimit(0,getHeight(),faderSplitY);
            // Reserve identical header/selector rows for every type (including
            // Playback), so FX tops and all bottom controls stay aligned.
            auto top=getLocalBounds().reduced(6);
            top.setBottom(juce::jmax(top.getY(),split-5));
            name.setBounds(top.removeFromTop(24));
            inputs.setBounds(top.removeFromTop(24));
            instrumentSelector.setBounds(top.removeFromTop(24).reduced(1,0));
            auto title=top.removeFromTop(22);
            addInsert.setBounds(title.removeFromRight(24).reduced(1));
            insertTitle.setBounds(title);
            insertViewport.setBounds(top);
            const int contentHeight=static_cast<int>(insertButtons.size())*20;
            const int width=juce::jmax(1,insertViewport.getWidth()-
                (contentHeight>insertViewport.getHeight()?insertViewport.getScrollBarThickness():0));
            insertBody.setSize(width,contentHeight);
            for(std::size_t i=0;i<insertButtons.size();++i)
                insertButtons[i]->setBounds(0,static_cast<int>(i)*20,width,18);

            // The lower zone starts at the same split for every strip. It owns
            // M/S, pan and the entire gain/meter rail; Instruments no longer
            // steal fader height for their VSTi selector.
            auto lower=getLocalBounds().reduced(6);
            lower.setTop(juce::jmin(lower.getBottom(),split+5));
            auto buttons=lower.removeFromTop(28);
            mute.setBounds(buttons.removeFromLeft(30).reduced(1));
            solo.setBounds(buttons.removeFromLeft(30).reduced(1));
            pan.setBounds(lower.removeFromTop(40));
            gain.setBounds(lower.withTrimmedRight(10));
        }
        void paint(juce::Graphics &g) override {
            const auto tokens = liveReferenceTokens();
            g.fillAll(tokens.colour(ColourRole::panel));
            g.setColour(tokens.colour(ColourRole::border)); g.drawRect(getLocalBounds());
            // MR Studio mixer uses independent L/R peak bars aligned with
            // the long fader. Keep the MR Live palette, no new meter colours.
            const int baseY=gain.getY()+5,baseX=getWidth()-17;
            const int height=juce::jmax(0,gain.getHeight()-10);
            const auto meterBackground=tokens.colour(ColourRole::panel);
            const auto active=tokens.colour(ColourRole::on);
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
        void syncInserts(const juce::var& saved,const juce::String& kind,bool instrument) {
            const bool visible=kind!="Playback";
            insertViewport.setVisible(visible);
            insertTitle.setVisible(visible);
            addInsert.setVisible(visible);
            const auto entries=saved["inserts"];
            const bool stopped=!owner.app.recording() &&
                owner.displayState().playback!=mrs::PlaybackState::playing;
            const bool editable=visible && saved.isObject() &&
                !(kind=="Instrument" && static_cast<int>(saved["version"])!=2) && stopped;
            addInsert.setEnabled(editable && entries.size()<8 &&
                (!instrument || entries.size()>0));

            // Rebuild only when an insert's stable ID, name or power changes;
            // never recreate plugin row controls on the 15 Hz meter tick.
            juce::String next=visible?"live:":"playback:";
            for(int i=instrument?1:0;i<entries.size();++i) {
                const auto entry=entries[i];
                next+=entry["id"].toString()+"|"+entry["name"].toString()+"|"+
                    entry["kind"].toString()+"|"+entry["bypass"].toString()+";";
            }
            if(next!=insertSignature) {
                insertSignature=next;
                insertButtons.clear();
                juce::Component::SafePointer<Strip> safe(this);
                for(int i=instrument?1:0;i<entries.size();++i) {
                    const auto entry=entries[i];
                    const auto slotText=entry["id"].toString();
                    if(slotText.isEmpty())continue;
                    const mrs::Id slot{slotText.toStdString()};
                    const auto label=entry["name"].toString().isNotEmpty()?
                        entry["name"].toString():juce::String("Gain");
                    auto control=std::make_unique<MixerInsert>(
                        label,static_cast<bool>(entry["bypass"]));
                    control->setComponentID("live-mixer-insert:"+slotText);
                    juce::Component::SafePointer<MixerInsert> row(control.get());
                    control->editor.onClick=[safe,slot] {
                        if(safe)safe->owner.run([&]{
                            safe->owner.openLiveInsertEditor(safe->id,slot);
                        });
                    };
                    control->menu=[safe,row,slot](juce::Point<int>) {
                        if(safe && row)safe->inputs.showInsertSlotMenu(*row,slot);
                    };
                    const auto generation=owner.liveDocumentGeneration;
                    control->bypass.onClick=[safe,slot,generation] {
                        // A Live FX chain edit replaces its saved UI projection.
                        // Defer until JUCE finishes the source button callback.
                        juce::MessageManager::callAsync([safe,slot,generation] {
                            if(safe && safe->owner.liveDocumentGeneration==generation)
                                safe->inputs.toggleInsertBypass(slot);
                        });
                    };
                    insertBody.addAndMakeVisible(*control);
                    insertButtons.push_back(std::move(control));
                }
                resized();
            }
            for(auto& button:insertButtons) {
                button->bypass.setEnabled(editable);
                button->editor.setEnabled(!owner.app.recording());
            }
        }
        void commit() { owner.run([&] { owner.setLiveChannelMix(id, gain.value(), pan.getValue(), mute.getToggleState(), solo.getToggleState()); }); }
        Desktop &owner;
        juce::String id;
        juce::Label name;
        LiveChannelControls inputs;
        juce::TextButton instrumentSelector{"Choose VSTi"};
        juce::Label insertTitle;
        juce::TextButton addInsert{"+"};
        juce::Viewport insertViewport;
        juce::Component insertBody;
        std::vector<std::unique_ptr<MixerInsert>> insertButtons;
        juce::String insertSignature;
        int faderSplitY{150};
        kit::MRFader gain;
        juce::Slider pan;
        juce::TextButton mute{"M"}, solo{"S"};
        float meterLeft{},meterRight{};
    };
    Desktop &owner;
    juce::Viewport viewport;
    juce::Component body;
    FaderDivider divider;
    juce::String signature;
    std::uint64_t observedGeneration{~std::uint64_t{0}},observedRevision{~std::uint64_t{0}};
    bool lanesInitialized{};
    std::vector<std::unique_ptr<Strip>> strips;
};
} // namespace ui
