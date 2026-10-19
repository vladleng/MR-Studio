#pragma once
#include "Controls.h"

namespace ui {
class SendLevel final : public juce::Component, public juce::SettableTooltipClient {
public:
    SendLevel(){setWantsKeyboardFocus(true);setDescription("Drag the top edge to adjust send; Ctrl for fine adjustment; right click chooses destination; Escape cancels");
        addAndMakeVisible(power);power.setComponentID("mixer-send-bypass");addMouseListener(this,true);
        power.onClick=[this]{abort();if(toggleBypass)toggleBypass();};}
    InsertPowerButton power;
    bool bypassed{};
    juce::String destination;
    float gain{1};
    std::function<void(float)> preview,commit;
    std::function<void()> cancel,menu,toggleBypass;
    bool dragging()const{return active;}
    static double unit(float value){return value<=0?0.:juce::jlimit(0.,1.,(20.*std::log10(value)+60.)/span);}
    static float level(double value){return value<=0?0.f:value>=1?16.f:static_cast<float>(std::pow(10.,(-60.+span*value)/20.));}
    juce::Rectangle<int> rail()const{return {2,1,juce::jmax(0,getWidth()-4),5};}
    juce::String levelText()const{return gain<=0?juce::String("-inf dB"):juce::String(20.*std::log10(gain),1)+" dB";}
    void sync(juce::String name,float value,bool bypass=false){destination=std::move(name);if(!active)gain=value;bypassed=bypass;
        power.setToggleState(bypass,juce::dontSendNotification);power.setTitle((bypass?"Enable send to ":"Disable send to ")+destination);
        power.setTooltip(bypass?"Bypassed — click to enable":"Active — click to bypass");
        setTitle("Send to "+destination);setTooltip(destination+" — "+levelText()+"; drag top edge; right click to choose send");repaint();}
    void resized()override{power.setBounds(2,8,14,14);}
    void paint(juce::Graphics& g)override{
        const auto* theme=dynamic_cast<Theme*>(&getLookAndFeel());
        auto fill=juce::Colour(theme&&theme->conceptSkin()?ConceptSkin::channelButton:raised);if(isMouseOver(true))fill=fill.brighter(.08f);if(!isEnabled())fill=fill.withAlpha(.5f);
        drawCard(g,getLocalBounds().reduced(1).toFloat(),fill);
        const auto r=rail();g.setColour(juce::Colour(0xff15191c));g.fillRect(r);
        const bool conceptStyle=theme&&theme->conceptSkin();
        g.setColour(juce::Colour(bypassed?0xff77838e:conceptStyle?ConceptSkin::selection:0xff3098ee).withMultipliedAlpha(isEnabled()?1.f:.4f));g.fillRect(r.withWidth(juce::roundToInt(r.getWidth()*unit(gain))));
        g.setColour(juce::Colour(0xffbdc6cf));const auto unity=r.getX()+juce::roundToInt(r.getWidth()*unit(1));g.drawVerticalLine(unity,static_cast<float>(r.getY()),static_cast<float>(r.getBottom()));
        g.setFont(Theme::stripFont());g.setColour(juce::Colours::whitesmoke.withMultipliedAlpha(isEnabled()&&!bypassed?1.f:.5f));g.drawText(conceptStyle&&(active||isMouseOver(true)||hasKeyboardFocus(true))?levelText():destination,getLocalBounds().withTrimmedTop(7).withTrimmedLeft(17).withTrimmedRight(3),juce::Justification::centred,true);
        if(hasKeyboardFocus(true)){g.setColour(juce::Colour(conceptStyle?ConceptSkin::selection:0xff87ceeb));g.drawRoundedRectangle(getLocalBounds().reduced(1).toFloat(),cornerRadius,borderWidth);}
    }
    void mouseDown(const juce::MouseEvent& e)override{
        if(!isEnabled())return;
        if(e.mods.isPopupMenu()){abort();if(menu)menu();return;}
        if(e.eventComponent!=this||!e.mods.isLeftButtonDown())return;grabKeyboardFocus();
        if(e.y>=11)return;active=true;origin=gain;last=e.position.x;
        if(!e.mods.isCtrlDown())gain=level((e.position.x-rail().getX())/juce::jmax(1,rail().getWidth()));
        if(preview)preview(gain);update();
    }
    void mouseDrag(const juce::MouseEvent& e)override{
        if(!active)return;const auto x=e.position.x;
        gain=level(e.mods.isCtrlDown()?unit(gain)+(x-last)/juce::jmax(1,rail().getWidth())*.1:(x-rail().getX())/juce::jmax(1,rail().getWidth()));last=x;
        if(preview)preview(gain);update();
    }
    void mouseUp(const juce::MouseEvent&)override{
        if(!active)return;active=false;if(gain!=origin&&commit)commit(gain);else if(cancel)cancel();update();
    }
    void mouseDoubleClick(const juce::MouseEvent& e)override{if(e.eventComponent==this&&isEnabled()&&e.mods.isLeftButtonDown()&&e.y<11){abort();gain=1;if(commit)commit(gain);update();}}
    bool keyPressed(const juce::KeyPress& k)override{
        if(k.getKeyCode()==juce::KeyPress::escapeKey&&active){abort();return true;}if(active||!isEnabled())return false;
        if(k.getKeyCode()==juce::KeyPress::F10Key&&k.getModifiers().isShiftDown()){if(menu)menu();return true;}
        const auto code=k.getKeyCode();double next=unit(gain);
        if(code==juce::KeyPress::homeKey)next=0;else if(code==juce::KeyPress::endKey)next=1;
        else if(code==juce::KeyPress::leftKey||code==juce::KeyPress::downKey)next-=k.getModifiers().isCtrlDown()?.001:.01;
        else if(code==juce::KeyPress::rightKey||code==juce::KeyPress::upKey)next+=k.getModifiers().isCtrlDown()?.001:.01;else return false;
        const auto before=gain;gain=level(juce::jlimit(0.,1.,next));if(gain!=before&&commit)commit(gain);update();return true;
    }
    void focusLost(FocusChangeType)override{abort();}
    void abort(){if(active){active=false;gain=origin;if(cancel)cancel();update();}}
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler()override{
        struct Value final:juce::AccessibilityValueInterface {
            SendLevel& control;explicit Value(SendLevel& c):control(c){}
            bool isReadOnly()const override{return !control.isEnabled()||control.dragging();}
            double getCurrentValue()const override{return control.gain;}
            juce::String getCurrentValueAsString()const override{return control.levelText();}
            void setValue(double v)override{if(isReadOnly()||!std::isfinite(v))return;const auto next=static_cast<float>(juce::jlimit(0.,16.,v));if(next==control.gain)return;control.gain=next;if(control.commit)control.commit(next);control.update();}
            void setValueAsString(const juce::String& value)override{setValue(value=="-inf dB"?0.:std::pow(10.,value.getDoubleValue()/20.));}
            AccessibleValueRange getRange()const override{return {{0.,16.},.001};}
        };
        return std::make_unique<juce::AccessibilityHandler>(*this,juce::AccessibilityRole::slider,juce::AccessibilityActions{},juce::AccessibilityHandler::Interfaces(std::make_unique<Value>(*this)));
    }
private:
    static constexpr double span=84.0823996531185;
    bool active{};float origin{1},last{};
    void update(){setTooltip(destination+" — "+levelText()+"; drag top edge; right click to choose send");if(auto* h=getAccessibilityHandler())h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);repaint();}
};
}
