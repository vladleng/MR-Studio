#pragma once
#include "MRControlBinding.h"
#include "GalleryProfile.h"
#include <MRReverbArt.h>

namespace ui {
// Composition only: the very same MRKnob/MRButton/Theme and binding as Gallery/#194.
class ReverbKitControls final : public juce::Component {
    Theme theme;
    DesignTokens tokens{DesignTokens::target()};
    std::vector<std::unique_ptr<kit::MRKnob>> knobs;
    std::vector<std::unique_ptr<kit::MRControlBinding>> bindings;
    kit::MRButton importProfile{"Import Gallery..."}, resetProfile{"Reset Kit"};
    juce::Label status;
    std::unique_ptr<juce::Drawable> illustration;
    std::unique_ptr<juce::FileChooser> chooser;
    bool optionsVisible{};
public:
    ReverbKitControls(kit::MRParameterHost& host, std::optional<mrs::Id> track, mrs::Id insert) {
        illustration=juce::Drawable::createFromImageData(MRReverbArt::plate_unit_svg,MRReverbArt::plate_unit_svgSize);
        if(!illustration)throw std::runtime_error("Invalid canonical Reverb illustration");
        theme.applyPreview(tokens);
        setLookAndFeel(&theme);
        setComponentID("reverb-kit");
        for (const auto parameter : {0u, 1u, 2u, 4u}) {
            kit::MRHostParameterKey key{track, insert, {}, parameter};
            auto control = std::make_unique<kit::MRKnob>(host.describe(key).control);
            control->setComponentID("kit-reverb-" + juce::String(static_cast<int>(parameter)));
            control->slider.setTooltip("Real native host parameter. Ctrl fine; double-click default; Enter value; Escape cancel.");
            if(parameter==2)control->slider.setDisplayMidpoint(25.);
            addAndMakeVisible(*control);
            bindings.push_back(std::make_unique<kit::MRControlBinding>(host, key, *control));
            knobs.push_back(std::move(control));
        }
        for (auto* item : std::array<juce::Component*, 3>{&importProfile, &resetProfile, &status}) addAndMakeVisible(*item);
        importProfile.setComponentID("kit-import-gallery");
        resetProfile.setComponentID("kit-reset-profile");
        status.setTitle("Pilot profile status");
        status.setText("MR UDS / approved Kit defaults", juce::dontSendNotification);
        importProfile.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Import Gallery profile into this pilot only", juce::File{}, "*.json");
            juce::Component::SafePointer<ReverbKitControls> safe(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe](const juce::FileChooser& dialog) {
                    if (!safe || !safe->isShowing()) return;
                    const auto file = dialog.getResult();
                    if (file == juce::File{}) return;
                    try {
                        if (file.getSize() > 32768) throw std::invalid_argument("Gallery draft exceeds 32KB");
                        safe->applyProfile(file.loadFileAsString());
                    } catch (const std::exception& e) {
                        safe->status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
                        safe->status.setColour(juce::Label::textColourId, safe->tokens.colour(ColourRole::error));
                    }
                });
        };
        resetProfile.onClick = [this] { applyTokens(DesignTokens::target()); };
    }
    ~ReverbKitControls() override {
        chooser.reset();
        bindings.clear();
        knobs.clear();
        setLookAndFeel(nullptr);
    }
    void cancel() { for (auto& knob : knobs) knob->cancelGesture(); }
    void sync(bool options, bool gate) {
        optionsVisible = options;
        knobs.back()->setVisible(options);
        if (knobs.back()->isEnabled() != (options && gate)) knobs.back()->setEnabled(options && gate);
        for (auto& binding : bindings) binding->sync();
        resized();
    }
    void applyProfile(const juce::String& json) { applyTokens(GalleryDraft::decode(json).tokens); }
    const DesignTokens& profile() const { return tokens; }
    void resized() override {
        auto area = getLocalBounds().reduced(8, 0);
        auto bar = area.removeFromBottom(32);
        importProfile.setBounds(bar.removeFromLeft(154));
        bar.removeFromLeft(8);
        resetProfile.setBounds(bar.removeFromLeft(108));
        bar.removeFromLeft(12);
        status.setBounds(bar);
        area.removeFromLeft(juce::jlimit(140,getWidth()/5,220));
        const auto count = optionsVisible ? 4 : 3;
        const auto width = area.getWidth() / count;
        for (int i = 0; i < count; ++i) knobs[static_cast<std::size_t>(i)]->setBounds(area.removeFromLeft(width).reduced(8, 4));
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(tokens.colour(ColourRole::window));
        auto art=juce::Rectangle<float>(8,8,static_cast<float>(juce::jlimit(140,getWidth()/5,220)-16),static_cast<float>(getHeight()-48));
        g.setColour(tokens.colour(ColourRole::text));g.setFont(Theme::stripFont());g.drawText("MR REVERB",art.removeFromTop(22),juce::Justification::centred);
        auto caption=art.removeFromBottom(20);
        illustration->drawWithin(g,art,juce::RectanglePlacement::centred,.9f);
        g.setColour(tokens.colour(ColourRole::muted));g.drawText("IR CONVOLUTION",caption,juce::Justification::centred);
    }
private:
    void applyTokens(const DesignTokens& next) {
        next.validate();
        cancel();
        theme.applyPreview(next);
        tokens = next;
        status.setColour(juce::Label::textColourId, tokens.colour(ColourRole::text));
        status.setText(tokens.name() + " / " + tokens.diff(), juce::dontSendNotification);
        repaint();
    }
};

void reverbKitSmoke();
void setupReverbKitPreview(class Desktop&);
}
