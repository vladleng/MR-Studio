#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include <cmath>
namespace ui {
constexpr auto background = 0xff292d31;
constexpr auto panel = 0xff383e43;
constexpr auto accent = 0xff3285ec;

class Theme final : public juce::LookAndFeel_V4 {
public:
    Theme() : face(juce::Typeface::createSystemTypefaceFor(BinaryData::NotoSans_ttf,
                                                         BinaryData::NotoSans_ttfSize)) {
        setColour(juce::TextButton::buttonColourId, juce::Colour(panel));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(accent));
        setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(14); }
    juce::Font font(float size) const { return juce::Font(juce::FontOptions(face).withPointHeight(size)); }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& c,
                              bool hover, bool down) override {
        g.setColour(c.brighter(down ? .18f : hover ? .09f : 0.f));
        g.fillRect(b.getLocalBounds().reduced(1));
        g.setColour(b.hasKeyboardFocus(true) ? juce::Colours::skyblue : juce::Colour(0xff525a62));
        g.drawRect(b.getLocalBounds().reduced(1));
    }
private:
    juce::Typeface::Ptr face;
};

// Pointer gestures start only on the handle. Relative movement never jumps to a rail click.
// Preview runs through Application's bounded mixer queue; release creates one history entry.
class Handle final : public juce::Component {
public:
    explicit Handle(bool vertical) : vertical_(vertical) { setWantsKeyboardFocus(true); }
    std::function<void(double)> preview, commit;
    std::function<void()> cancel;
    double value{.8};
    bool rotary{};
    bool dragging() const { return active; }
    juce::Rectangle<float> knob() const {
        if(rotary){const float side=static_cast<float>(juce::jmin(getWidth(),getHeight())-8);return getLocalBounds().toFloat().withSizeKeepingCentre(side,side);}
        if (vertical_) return {7.f, 12.f + static_cast<float>(1-value) * travel(),
                               static_cast<float>(getWidth()-14), 18.f};
        return {8.f + static_cast<float>(value)*travel(), 8.f, 18.f, static_cast<float>(getHeight()-16)};
    }
    void paint(juce::Graphics& g) override {
        if(rotary){auto r=knob();g.setColour(juce::Colour(0xff20262b));g.fillEllipse(r);g.setColour(hasKeyboardFocus(true)?juce::Colours::skyblue:juce::Colours::grey);g.drawEllipse(r,1);
            const float angle=static_cast<float>((value-.5)*4.7);auto c=r.getCentre();g.setColour(juce::Colours::skyblue);g.drawLine(c.x,c.y,c.x+std::sin(angle)*r.getWidth()*.38f,c.y-std::cos(angle)*r.getHeight()*.38f,2);return;}
        g.setColour(juce::Colour(0xff171b1e));
        g.fillRect(getLocalBounds().reduced(vertical_ ? 20 : 8, vertical_ ? 12 : 17));
        g.setColour(hasKeyboardFocus(true) ? juce::Colours::skyblue : juce::Colour(0xffbbc4cd));
        g.fillRect(knob()); g.setColour(juce::Colour(0xff4e5963));
        auto k=knob(); g.drawLine(k.getX(),k.getCentreY(),k.getRight(),k.getCentreY(),2);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if (!e.mods.isLeftButtonDown() || !knob().contains(e.position)) return;
        if(rotary && e.position.getDistanceFrom(knob().getCentre())>knob().getWidth()/2)return;
        grabKeyboardFocus(); active=true; origin=value; last=vertical_ ? e.position.y : e.position.x;
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!active) return;
        const auto position=vertical_ ? e.position.y : e.position.x;
        const auto delta=(position-last)*(vertical_ ? -1.f : 1.f); last=position;
        value=juce::jlimit(0.,1.,value+delta/travel()*(e.mods.isCtrlDown() ? .1 : 1.));
        if(preview) preview(value); repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if(!active) return; active=false;
        if(value!=origin && commit) commit(value); else if(cancel) cancel(); repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::escapeKey && active) { abort(); return true; }
        if(active) return false;
        const int k=key.getKeyCode();
        if(k!=juce::KeyPress::upKey && k!=juce::KeyPress::rightKey &&
           k!=juce::KeyPress::downKey && k!=juce::KeyPress::leftKey) return false;
        const auto step=key.getModifiers().isCtrlDown() ? .001 : .01;
        value=juce::jlimit(0.,1.,value+((k==juce::KeyPress::upKey || k==juce::KeyPress::rightKey) ? step : -step));
        if(commit) commit(value); repaint(); return true;
    }
    void focusLost(FocusChangeType) override { abort(); }
private:
    float travel() const { return rotary ? 240.f : static_cast<float>(juce::jmax(1,(vertical_ ? getHeight()-42 : getWidth()-34))); }
    void abort() { if(active) {active=false; value=origin; if(cancel)cancel(); repaint();} }
    bool vertical_, active{};
    double origin{};
    float last{};
};

}
