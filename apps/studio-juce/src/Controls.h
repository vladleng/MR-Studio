#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include <cmath>
namespace ui {
constexpr auto background = 0xff303438;
constexpr auto panel = 0xff393e43;
constexpr auto surface = 0xff25282b;
constexpr auto accent = 0xff006dcc;

class Theme final : public juce::LookAndFeel_V4 {
public:
    Theme() : face(juce::Typeface::createSystemTypefaceFor(BinaryData::NotoSans_ttf,
                                                         BinaryData::NotoSans_ttfSize)) {
        setColour(juce::TextButton::buttonColourId, juce::Colour(panel));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(accent));
        setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(background));setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff51565c));
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(surface));setColour(juce::TextEditor::outlineColourId,juce::Colour(0xff51565c));
        setColour(juce::TreeView::linesColourId,juce::Colour(0xff858d94));setColour(juce::ScrollBar::thumbColourId,juce::Colour(0xff62686e));
    }
    void drawMenuBarBackground(juce::Graphics& g,int,int,bool,juce::MenuBarComponent&) override {g.fillAll(juce::Colour(surface));}
    void drawTreeviewPlusMinusBox(juce::Graphics& g,const juce::Rectangle<float>& area,juce::Colour,bool open,bool hover) override {
        g.setColour(hover?juce::Colours::white:juce::Colour(0xffb8bdc2));
        const auto centre=area.getCentre();const float size=juce::jmin(5.f,area.getWidth()*.3f);juce::Path arrow;
        if(open){arrow.startNewSubPath(centre.x-size,centre.y-size*.5f);arrow.lineTo(centre.x,centre.y+size*.5f);arrow.lineTo(centre.x+size,centre.y-size*.5f);}
        else {arrow.startNewSubPath(centre.x-size*.5f,centre.y-size);arrow.lineTo(centre.x+size*.5f,centre.y);arrow.lineTo(centre.x-size*.5f,centre.y+size);}
        g.strokePath(arrow,juce::PathStrokeType(1.5f));
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool hover,bool down) override {
        const auto text=button.getButtonText();auto area=button.getLocalBounds().toFloat().reduced(8);auto centre=area.getCentre();
        if(text=="Play"){g.setColour(juce::Colour(0xff31b7dd));juce::Path shape;shape.addTriangle(centre.x-5,centre.y-7,centre.x-5,centre.y+7,centre.x+7,centre.y);g.fillPath(shape);}
        else if(text=="Stop"){g.setColour(juce::Colour(0xffc6d3df));g.fillRect(centre.x-6,centre.y-6,12.f,12.f);}
        else if(text=="Pause"){g.setColour(juce::Colour(0xffc6d3df));g.fillRect(centre.x-6,centre.y-6,4.f,12.f);g.fillRect(centre.x+2,centre.y-6,4.f,12.f);}
        else if(text=="Record (R)"){g.setColour(juce::Colour(0xffe64b54));g.fillEllipse(centre.x-6,centre.y-6,12,12);}
        else juce::LookAndFeel_V4::drawButtonText(g,button,hover,down);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(12); }
    juce::Font font(float size) const { return juce::Font(juce::FontOptions(face).withPointHeight(size)); }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& c,
                              bool hover, bool down) override {
        g.setColour(c.brighter(down ? .18f : hover ? .09f : 0.f));
        g.fillRoundedRectangle(b.getLocalBounds().reduced(1).toFloat(),3.f);
        g.setColour(b.hasKeyboardFocus(true) ? juce::Colours::skyblue : juce::Colour(0xff525a62));
        g.drawRoundedRectangle(b.getLocalBounds().reduced(1).toFloat(),3.f,1.f);
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
    void notifyValue() {if(auto* handler=getAccessibilityHandler())handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);}
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override {
        struct Value final : juce::AccessibilityRangedNumericValueInterface {
            explicit Value(Handle& c):control(c){}
            bool isReadOnly() const override {return !control.isEnabled()||control.dragging();}
            double getCurrentValue() const override {return control.value;}
            void setValue(double v) override {
                if(isReadOnly()||!std::isfinite(v))return;
                const auto next=juce::jlimit(0.,1.,v);
                if(next==control.value)return;control.value=next;
                if(control.commit)control.commit(next);control.notifyValue();control.repaint();
            }
            AccessibleValueRange getRange() const override {return {{0.,1.},.001};}
            Handle& control;
        };
        return std::make_unique<juce::AccessibilityHandler>(*this,juce::AccessibilityRole::slider,
            juce::AccessibilityActions{},juce::AccessibilityHandler::Interfaces(std::make_unique<Value>(*this)));
    }
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
        if(value!=origin && commit) commit(value); else if(cancel) cancel(); notifyValue();repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::escapeKey && active) { abort(); return true; }
        if(active) return false;
        const int k=key.getKeyCode();
        if(k!=juce::KeyPress::upKey && k!=juce::KeyPress::rightKey &&
           k!=juce::KeyPress::downKey && k!=juce::KeyPress::leftKey) return false;
        const auto step=key.getModifiers().isCtrlDown() ? .001 : .01;
        value=juce::jlimit(0.,1.,value+((k==juce::KeyPress::upKey || k==juce::KeyPress::rightKey) ? step : -step));
        if(commit) commit(value); notifyValue();repaint(); return true;
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
