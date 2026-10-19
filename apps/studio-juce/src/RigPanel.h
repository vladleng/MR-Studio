#pragma once
#include "ToneBrowser.h"
namespace ui {
class Desktop;
class RigPanel final:public juce::Component {
public:
    RigPanel(Desktop&,std::optional<mrs::Id>,mrs::Id);
    ~RigPanel() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
    void cancelGesture();
    bool keyPressed(const juce::KeyPress&) override;
private:
    friend void rigShellSmoke();
    struct Impl;std::unique_ptr<Impl> impl;
};
void rigShellSmoke();
}
