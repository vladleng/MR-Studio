#pragma once
#include "Controls.h"
#include "Settings.h"
#include <mrs/desktop.hpp>

namespace ui {
// Shared curated model browser; Rig shell owns project controls and Rig presets.
class ToneBrowser final:public juce::Component {
public:
    ToneBrowser(mrs::desktop::Application&,std::optional<mrs::Id>,mrs::Id,ViewSettings&,std::function<void()>);
    ~ToneBrowser() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
    bool ready()const;
    void compact(bool);
    void rigLibrary(juce::Component*,std::function<void(juce::String)>);
    juce::Image artwork(const std::string&)const;
    std::function<void()> manageFiles;
    void importCatalog(const mrs::audio::CabManifest&); // metadata snapshot only, no media copying
    void selectModel(const std::string&);
    void searchFor(const juce::String&);
    int visibleModels()const;
    const mrs::audio::ToneLibrary& models()const;
    void setTab(int);void setRole(int);void setFavorite(bool);
private:
    friend void toneLibrarySmoke();
    struct Impl;std::unique_ptr<Impl> impl;
};
void toneLibrarySmoke();
juce::String librarySearchKey(const juce::String&);
juce::Image loadToneArtwork(const mrs::audio::ToneModel&); // worker only
}
