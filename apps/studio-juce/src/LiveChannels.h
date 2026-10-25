#pragma once
#include "Desktop.h"

namespace ui {
class LiveChannels final : public juce::Component {
  public:
    explicit LiveChannels(Desktop &desktop) : owner(desktop) {
        setComponentID("live-channels");
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(true, false);
    }
    ~LiveChannels() override { viewport.setViewedComponent(nullptr, false); }
    void resized() override {
        viewport.setBounds(getLocalBounds().withTrimmedTop(28));
        layoutRows();
    }
    void paint(juce::Graphics &g) override {
        const auto tokens = DesignTokens::target();
        g.fillAll(tokens.colour(ColourRole::panel));
        g.setColour(tokens.colour(ColourRole::muted));
        g.setFont(owner.theme.font(12));
        g.drawText("Channels", 6, 2, getWidth() - 12, 24, juce::Justification::left);
        if (rows.empty())
            g.drawText("No channels", getLocalBounds().reduced(8).withTrimmedTop(32),
                       juce::Justification::topLeft, true);
    }
    void refresh() {
        const auto project = owner.project();
        juce::String ids;
        for (const auto &track : project->tracks)
            ids += juce::String::fromUTF8(track.id.value.c_str()) + "\n";
        if (ids != signature) {
            signature = ids;
            rows.clear();
            for (std::size_t i = 0; i < project->tracks.size(); ++i) {
                auto row = std::make_unique<Row>(owner, project->tracks[i].id, i);
                body.addAndMakeVisible(*row);
                rows.push_back(std::move(row));
            }
            layoutRows();
        }
        for (auto &row : rows)
            row->sync();
        repaint();
    }

  private:
    class Row final : public juce::Component {
      public:
        Row(Desktop &desktop, mrs::Id id, std::size_t index)
            : owner(desktop), target(std::move(id)), meterIndex(index) {
            for (auto *button : {&mute, &solo, &arm, &monitor})
                addAndMakeVisible(button);
            mute.setComponentID("live-mute-" + juce::String::fromUTF8(target.value.c_str()));
            solo.setComponentID("live-solo-" + juce::String::fromUTF8(target.value.c_str()));
            const auto tokens = DesignTokens::target();
            mute.setColour(juce::TextButton::buttonOnColourId, tokens.colour(ColourRole::error));
            solo.setColour(juce::TextButton::buttonOnColourId, tokens.colour(ColourRole::warning));
            solo.setColour(juce::TextButton::textColourOnId, tokens.colour(ColourRole::window));
            mute.onClick = [this] { change(false); };
            solo.onClick = [this] { change(true); };
            arm.onClick = [this] {
                owner.liveShowEditing = false;
                owner.run([&] { owner.app.set_track_armed(target, !owner.app.track_armed(target)); });
            };
            monitor.onClick = [this] {
                owner.liveShowEditing = false;
                owner.run([&] {
                    const auto project = owner.project();
                    for (const auto &track : project->tracks)
                        if (track.id == target) {
                            if (track.kind == mrs::TrackKind::instrument)
                                owner.app.set_midi_input(target, track.midi_input, track.midi_channel,
                                                         !track.midi_monitor);
                            else
                                owner.app.set_track_monitoring(target, !track.input_monitor);
                            break;
                        }
                });
            };
        }
        void sync() {
            const auto project = owner.project();
            for (const auto &track : project->tracks)
                if (track.id == target) {
                    name = juce::String::fromUTF8(track.name.c_str());
                    mute.setTitle("Mute " + name);
                    solo.setTitle("Solo " + name);
                    arm.setTitle("Arm " + name);
                    monitor.setTitle("Input monitor " + name);
                    mute.setToggleState(track.mix.mute, juce::dontSendNotification);
                    solo.setToggleState(track.mix.solo, juce::dontSendNotification);
                    const bool recordable =
                        track.kind == mrs::TrackKind::audio || track.kind == mrs::TrackKind::instrument;
                    arm.setEnabled(recordable);
                    monitor.setEnabled(recordable);
                    arm.setToggleState(recordable && owner.app.track_armed(target), juce::dontSendNotification);
                    monitor.setToggleState(track.kind == mrs::TrackKind::instrument ? track.midi_monitor
                                                                                    : track.input_monitor,
                                           juce::dontSendNotification);
                    break;
                }
            repaint();
        }
        void resized() override {
            auto area = getLocalBounds().reduced(4);
            area.removeFromTop(24);
            auto controls = area.removeFromTop(26);
            const int width = controls.getWidth() / 4;
            for (auto *button : {&mute, &solo, &arm, &monitor})
                button->setBounds(controls.removeFromLeft(width).reduced(1, 0));
        }
        void paint(juce::Graphics &g) override {
            const auto tokens = DesignTokens::target();
            g.setColour(tokens.colour(ColourRole::text));
            g.setFont(owner.theme.font(12));
            g.drawText(name, 5, 2, getWidth() - 10, 24, juce::Justification::left, true);
            if (meterIndex < owner.peaks.size()) {
                const auto peak = owner.peaks[meterIndex];
                const float level = juce::jlimit(0.f, 1.f, juce::jmax(peak.left, peak.right));
                g.setColour(tokens.colour(ColourRole::border));
                g.fillRect(5, getHeight() - 10, getWidth() - 10, 3);
                g.setColour(tokens.colour(ColourRole::on));
                g.fillRect(5, getHeight() - 10, juce::roundToInt((getWidth() - 10) * level), 3);
            }
            g.setColour(tokens.colour(ColourRole::border));
            g.drawHorizontalLine(getHeight() - 1, 0.f, static_cast<float>(getWidth()));
        }

      private:
        void change(bool soloAction) {
            owner.liveShowEditing = false;
            owner.run([&] {
                const auto project = owner.project();
                for (const auto &track : project->tracks)
                    if (track.id == target) {
                        auto mix = track.mix;
                        if (soloAction)
                            mix.solo = !mix.solo;
                        else
                            mix.mute = !mix.mute;
                        owner.app.set_track_mix(target, mix);
                        break;
                    }
            });
        }
        Desktop &owner;
        mrs::Id target;
        std::size_t meterIndex;
        juce::String name;
        juce::TextButton mute{"M"}, solo{"S"}, arm{"R"}, monitor{"I"};
    };
    void layoutRows() {
        const int width = juce::jmax(1, viewport.getWidth() - viewport.getScrollBarThickness());
        int y = 0;
        for (auto &row : rows) {
            row->setBounds(0, y, width, 72);
            y += 72;
        }
        body.setSize(width, juce::jmax(y, viewport.getHeight()));
    }
    Desktop &owner;
    juce::Viewport viewport;
    juce::Component body;
    juce::String signature;
    std::vector<std::unique_ptr<Row>> rows;
};
} // namespace ui
