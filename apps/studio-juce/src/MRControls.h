#pragma once
#include "Controls.h"
#include <charconv>
#include <span>
#include <MRControlAssets.h>
namespace ui::kit {
inline juce::String text(const char* s){return juce::String::fromUTF8(s);}
inline juce::Colour colour(const juce::Component& c,ColourRole role){const auto* theme=dynamic_cast<const Theme*>(&c.getLookAndFeel());const auto fallback=DesignTokens{}.colours[static_cast<std::size_t>(role)];return theme?theme->role(role,fallback):juce::Colour(fallback);}
inline float radius(const juce::Component& c){if(const auto* theme=dynamic_cast<const Theme*>(&c.getLookAndFeel()))return theme->radius();return cornerRadius;}
enum class Unit { none, db, hz, ms, percent, ratio };
enum class Mapping { linear, logarithmic, bipolar };
struct ParameterSpec {
    juce::String id,name;Unit unit{Unit::none};double minimum{},maximum{1},initial{},step{.01};Mapping mapping{Mapping::linear};int decimals{2};bool exactHostValues{};
    juce::String suffix()const{switch(unit){case Unit::db:return "dB";case Unit::hz:return "Hz";case Unit::ms:return "ms";case Unit::percent:return "%";case Unit::ratio:return ":1";default:return {};}}
    void validate()const{if(static_cast<int>(unit)<0||static_cast<int>(unit)>static_cast<int>(Unit::ratio)||static_cast<int>(mapping)<0||static_cast<int>(mapping)>static_cast<int>(Mapping::bipolar)||id.isEmpty()||name.isEmpty()||!std::isfinite(minimum)||!std::isfinite(maximum)||!std::isfinite(initial)||!std::isfinite(step)||minimum>=maximum||initial<minimum||initial>maximum||step<=0||step>maximum-minimum||decimals<0||decimals>6||!std::isfinite((maximum-minimum)/step)||(!exactHostValues&&std::abs((maximum-minimum)/step-std::round((maximum-minimum)/step))>1e-6)||(mapping==Mapping::logarithmic&&minimum<=0)||(mapping==Mapping::bipolar&&!(minimum<0&&maximum>0)))throw std::invalid_argument("Invalid control parameter specification");}
    double constrain(double value)const{if(!std::isfinite(value))throw std::invalid_argument("Nonfinite control value");value=juce::jlimit(minimum,maximum,value);if(exactHostValues&&(value==initial||value==minimum||value==maximum))return value;const auto snapped=minimum+std::round((value-minimum)/step)*step;return juce::jlimit(minimum,maximum,snapped);}
    juce::String format(double value)const{return juce::String(value,decimals)+(suffix().isEmpty()?juce::String{}:" "+suffix());}
    std::optional<double> parse(juce::String input)const{
        input=input.trim();const auto unitText=suffix();if(unitText.isNotEmpty()&&input.endsWithIgnoreCase(unitText))input=input.dropLastCharacters(unitText.length()).trimEnd();input=input.replaceCharacter(',','.');const auto bytes=input.toStdString();if(bytes.empty())return {};
        auto first=bytes.data();if(*first=='+')++first;double value{};const auto result=std::from_chars(first,bytes.data()+bytes.size(),value);if(result.ec!=std::errc{}||result.ptr!=bytes.data()+bytes.size()||!std::isfinite(value)||value<minimum||value>maximum)return {};return constrain(value);
    }
};
enum class Phase { begin, preview, commit, cancel };
struct ControlEvent {Phase phase;juce::String id;double value;};
using EventSink=std::function<void(const ControlEvent&)>;
class MRValueField final : public juce::TextEditor {
    ParameterSpec spec_;double value_{};bool error_{};
public:
    std::function<void(double)> onCommit;std::function<void()> onCancel;
    explicit MRValueField(ParameterSpec spec):spec_(std::move(spec)),value_(spec_.initial){spec_.validate();value_=spec_.constrain(value_);setMultiLine(false);setSelectAllWhenFocused(true);setJustification(juce::Justification::centredRight);setTitle(spec_.name+" "+spec_.suffix());setDescription("Type a value; Enter commits, Escape cancels");setText(spec_.format(value_),false);onReturnKey=[this]{commitText();};onEscapeKey=[this]{revert();if(onCancel)onCancel();};onFocusLost=[this]{if(getText()!=spec_.format(value_))commitText();};}
    const ParameterSpec& spec()const{return spec_;}double value()const{return value_;}bool hasError()const{return error_;}
    void setValue(double v){if(!std::isfinite(v)||v<spec_.minimum||v>spec_.maximum)throw std::invalid_argument("Value field outside range");value_=spec_.constrain(v);error_=false;setTooltip({});setText(spec_.format(value_),false);setDescription("Value "+spec_.format(value_));repaint();}
    void setAuthoritativeValue(double v){if(!spec_.exactHostValues||!std::isfinite(v)||v<spec_.minimum||v>spec_.maximum)throw std::invalid_argument("Invalid authoritative field value");value_=v;error_=false;setTooltip({});setText(spec_.format(v),false);setDescription("Value "+spec_.format(v));repaint();}
    void revert(){error_=false;setTooltip({});setText(spec_.format(value_),false);setDescription("Value "+spec_.format(value_));repaint();}
    bool commitText(){if(!isEnabled())return false;if(spec_.exactHostValues&&getText().trim()==spec_.format(value_).trim()){revert();return true;}const auto v=spec_.parse(getText());if(!v){error_=true;setDescription("Invalid value. Allowed "+spec_.format(spec_.minimum)+" to "+spec_.format(spec_.maximum));setTooltip(getDescription());repaint();return false;}const auto changed=*v!=value_;setValue(*v);if(changed&&onCommit)onCommit(*v);return true;}
    void paintOverChildren(juce::Graphics& g)override{if(error_){g.setColour(colour(*this,ColourRole::error));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),radius(*this),1.5f);g.setFont(Theme::detailFont());g.drawText("!",3,0,12,getHeight(),juce::Justification::centred);}}
};
class MRParameterControl;
class GestureSlider final : public juce::Slider {
    bool pointer_{},cancelled_{};juce::Point<float> previous_;bool vertical_{};Mapping mapping_{Mapping::linear};bool authoritative_{};double displayPower_{1};
public:
    std::function<void()> begin,end,cancel;std::function<bool()> active;
    explicit GestureSlider(bool vertical):vertical_(vertical){setWantsKeyboardFocus(true);setScrollWheelEnabled(true);}
    void configureHostPrecision(const ParameterSpec& spec){juce::NormalisableRange<double> range(spec.minimum,spec.maximum,[](double a,double b,double p){return a+p*(b-a);},[](double a,double b,double v){return (v-a)/(b-a);},[this,spec](double,double,double v){return authoritative_?v:spec.constrain(v);});range.interval=spec.step;setNormalisableRange(range);}
    void setAuthoritativeValue(double value){const juce::ScopedValueSetter<bool> guard(authoritative_,true);setValue(value,juce::dontSendNotification);}
    void setMapping(Mapping mapping){mapping_=mapping;}
    // Optional view-only travel curve. Host values/ranges/defaults remain physical.
    void setDisplayMidpoint(double midpoint){if(mapping_!=Mapping::linear||!std::isfinite(midpoint)||midpoint<=getMinimum()||midpoint>=getMaximum())throw std::invalid_argument("Invalid display midpoint");displayPower_=std::log((midpoint-getMinimum())/(getMaximum()-getMinimum()))/std::log(.5);}

    double proportionOfLengthToValue(double p)override{p=juce::jlimit(0.,1.,p);if(mapping_==Mapping::logarithmic)return getMinimum()*std::pow(getMaximum()/getMinimum(),p);if(mapping_==Mapping::bipolar)return p<=.5?getMinimum()*(1-2*p):getMaximum()*(2*p-1);return getMinimum()+(displayPower_==1?p:std::pow(p,displayPower_))*(getMaximum()-getMinimum());}
    double valueToProportionOfLength(double v)override{v=juce::jlimit(getMinimum(),getMaximum(),v);if(mapping_==Mapping::logarithmic)return std::log(v/getMinimum())/std::log(getMaximum()/getMinimum());if(mapping_==Mapping::bipolar)return v<=0?.5*(1-v/getMinimum()):.5+.5*v/getMaximum();const auto proportion=(v-getMinimum())/(getMaximum()-getMinimum());return displayPower_==1?proportion:std::pow(proportion,1/displayPower_);}
    void cancelPointerGesture(){if(pointer_)cancelled_=true;}
    bool pointerActive()const{return pointer_;}
    void paintOverChildren(juce::Graphics& g)override{if(hasKeyboardFocus(false)){g.setColour(colour(*this,ColourRole::focus));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),radius(*this),1);}}

    void mouseDown(const juce::MouseEvent& e)override{if(!isEnabled()||!e.mods.isLeftButtonDown())return;grabKeyboardFocus();pointer_=true;cancelled_=false;previous_=e.position;if(begin)begin();}
    void mouseDrag(const juce::MouseEvent& e)override{if(!isEnabled()||!pointer_||cancelled_)return;const auto delta=vertical_?previous_.y-e.position.y:e.position.x-previous_.x;previous_=e.position;const auto next=juce::jlimit(0.,1.,valueToProportionOfLength(getValue())+delta/200.*(e.mods.isCtrlDown()?.1:1.));setValue(proportionOfLengthToValue(next),juce::sendNotificationSync);}
    void mouseUp(const juce::MouseEvent&)override{if(!pointer_)return;pointer_=false;if(!cancelled_&&end)end();cancelled_=false;}
    void mouseDoubleClick(const juce::MouseEvent&)override{if(!isEnabled())return;if(active&&active()&&cancel)cancel();cancelled_=pointer_;setValue(getDoubleClickReturnValue(),juce::sendNotificationSync);}
    bool keyPressed(const juce::KeyPress& key)override{
        if(!isEnabled())return false;const auto code=key.getKeyCode();if(code==juce::KeyPress::escapeKey&&active&&active()){cancelled_=pointer_;if(cancel)cancel();return true;}if(pointer_)return false;
        if(code==juce::KeyPress::homeKey){setValue(getMinimum(),juce::sendNotificationSync);return true;}if(code==juce::KeyPress::endKey){setValue(getMaximum(),juce::sendNotificationSync);return true;}
        if(code==juce::KeyPress::leftKey||code==juce::KeyPress::downKey||code==juce::KeyPress::rightKey||code==juce::KeyPress::upKey){const auto direction=code==juce::KeyPress::leftKey||code==juce::KeyPress::downKey?-1.:1.;setValue(getValue()+direction*getInterval()*(key.getModifiers().isCtrlDown()?1:10),juce::sendNotificationSync);return true;}return juce::Slider::keyPressed(key);
    }
    void focusLost(FocusChangeType cause)override{if(pointer_){cancelled_=true;if(cancel)cancel();}juce::Slider::focusLost(cause);}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& d)override{if(isEnabled()&&!pointer_)juce::Slider::mouseWheelMove(e,d);}
};
class MRParameterControl : public juce::Component {
    double origin_{},value_{};bool active_{};
    void emit(Phase phase,double v){if(onEvent)onEvent({phase,spec_.id,v});}
protected:
    ParameterSpec spec_;
public:
    EventSink onEvent;juce::Label label;GestureSlider slider;MRValueField field;
    MRParameterControl(ParameterSpec spec,bool vertical):value_(spec.initial),spec_(std::move(spec)),slider(vertical),field(spec_){
        spec_.validate();value_=spec_.constrain(value_);setTitle(spec_.name);addAndMakeVisible(label);label.setText(spec_.name,juce::dontSendNotification);label.setInterceptsMouseClicks(false,false);addAndMakeVisible(slider);addAndMakeVisible(field);
        slider.textFromValueFunction=[this](double v){return spec_.format(v);};slider.valueFromTextFunction=[this](const juce::String& v){return spec_.parse(v).value_or(value_);};slider.setTitle(spec_.name+" "+spec_.suffix());slider.setDescription("Relative drag, Ctrl fine, arrows, Home/End, double-click reset, Escape cancel");slider.setRange(spec_.minimum,spec_.maximum,spec_.step);slider.setMapping(spec_.mapping);if(spec_.exactHostValues)slider.configureHostPrecision(spec_);slider.setValue(value_,juce::dontSendNotification);slider.setDoubleClickReturnValue(true,spec_.initial);slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        slider.begin=[this]{beginGesture();};slider.end=[this]{endGesture();};slider.cancel=[this]{cancelGesture();};slider.active=[this]{return active_;};slider.onDragStart=[this]{beginGesture();};slider.onDragEnd=[this]{endGesture();};slider.onValueChange=[this]{change(slider.getValue());};field.onCommit=[this](double v){change(v);};field.onCancel=[this]{cancelGesture();};
    }
    ~MRParameterControl()override{slider.valueFromTextFunction=nullptr;slider.textFromValueFunction=nullptr;slider.onValueChange=nullptr;slider.onDragStart=nullptr;slider.onDragEnd=nullptr;slider.begin=nullptr;slider.end=nullptr;slider.cancel=nullptr;slider.active=nullptr;field.onCommit=nullptr;field.onCancel=nullptr;cancelGesture();}
    const ParameterSpec& spec()const{return spec_;}double value()const{return value_;}bool gesturing()const{return active_;}
    void setValue(double v){if(!std::isfinite(v)||v<spec_.minimum||v>spec_.maximum)throw std::invalid_argument("Control outside range");value_=spec_.constrain(v);slider.setValue(value_,juce::dontSendNotification);field.setValue(value_);}
    void setAuthoritativeValue(double v){if(!spec_.exactHostValues||!std::isfinite(v)||v<spec_.minimum||v>spec_.maximum)throw std::invalid_argument("Invalid authoritative control value");value_=v;slider.setAuthoritativeValue(v);field.setAuthoritativeValue(v);}
    void beginGesture(){if(active_||!isEnabled())return;origin_=value_;active_=true;emit(Phase::begin,origin_);}
    void change(double v){if(!isEnabled())return;const auto next=spec_.constrain(v);if(next==value_)return;const bool discrete=!active_;juce::Component::SafePointer<MRParameterControl> safe(this);if(discrete)beginGesture();if(!safe)return;setValue(next);emit(Phase::preview,next);if(safe&&discrete)endGesture();}
    void endGesture(){if(!active_)return;active_=false;emit(value_==origin_?Phase::cancel:Phase::commit,value_);}
    void cancelGesture(){slider.cancelPointerGesture();if(!active_)return;active_=false;if(spec_.exactHostValues)setAuthoritativeValue(origin_);else setValue(origin_);emit(Phase::cancel,origin_);}
    void enablementChanged()override{if(!isEnabled())cancelGesture();}
};
enum class KnobSize { mini, standard, large };
class MRKnob final : public MRParameterControl {
    KnobSize size_;
public:
    explicit MRKnob(ParameterSpec spec,KnobSize size=KnobSize::standard):MRParameterControl(std::move(spec),true),size_(size){slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);setSize(preferredSize().x,preferredSize().y);}
    juce::Point<int> preferredSize()const{switch(size_){case KnobSize::mini:return {80,104};case KnobSize::large:return {160,184};default:return {120,144};}}
    void resized()override{auto r=getLocalBounds().reduced(4);label.setBounds(r.removeFromTop(20));field.setBounds(r.removeFromBottom(28));const auto maxSide=size_==KnobSize::mini?40:size_==KnobSize::large?112:72;slider.setBounds(r.withSizeKeepingCentre(juce::jmin(maxSide,r.getWidth()),juce::jmin(maxSide,r.getHeight())));}
};
class MRFader final : public MRParameterControl {
    bool vertical_;
public:
    explicit MRFader(ParameterSpec spec,bool vertical=false):MRParameterControl(std::move(spec),vertical),vertical_(vertical){slider.getProperties().set("mrConceptFader",true);slider.setSliderStyle(vertical?juce::Slider::LinearVertical:juce::Slider::LinearHorizontal);setSize(vertical?100:330,vertical?240:84);}
    void resized()override{auto r=getLocalBounds().reduced(4);label.setBounds(r.removeFromTop(20));if(vertical_){field.setBounds(r.removeFromBottom(28));slider.setBounds(r);}else{r.removeFromTop(6);field.setBounds(r.removeFromRight(104).withSizeKeepingCentre(104,28));r.removeFromRight(8);slider.setBounds(r);}}
};
class MRButton : public juce::TextButton {
public:explicit MRButton(juce::String label):juce::TextButton(label){setTitle(label);setWantsKeyboardFocus(true);setSize(120,32);}
};
enum class Glyph { power, plus };
class MRIconButton final : public juce::Button {
    Glyph glyph_;std::unique_ptr<juce::Drawable> icon_;float iconSize_{24};
public:
    explicit MRIconButton(Glyph glyph,juce::String name):juce::Button(name),glyph_(glyph){setTitle(name);setTooltip(name);setSize(36,36);icon_=juce::Drawable::createFromImageData(glyph==Glyph::power?MRControlAssets::power_svg:MRControlAssets::plus_svg,glyph==Glyph::power?MRControlAssets::power_svgSize:MRControlAssets::plus_svgSize);if(!icon_)throw std::runtime_error("Invalid canonical SVG");if(glyph==Glyph::power)setClickingTogglesState(true);}
    void setIconSize(float value){if(!std::isfinite(value)||value<12||value>32)throw std::invalid_argument("Icon size outside token bounds");iconSize_=value;repaint();}
    void paintButton(juce::Graphics& g,bool hover,bool down)override{auto r=getLocalBounds().toFloat().reduced(.5f);auto fill=colour(*this,ColourRole::raised);g.setColour(fill.brighter(down?.14f:hover?.07f:0));g.fillRoundedRectangle(r,radius(*this));g.setColour(colour(*this,hasKeyboardFocus(true)?ColourRole::focus:ColourRole::border));g.drawRoundedRectangle(r,radius(*this),hasKeyboardFocus(true)?1.5f:1.f);icon_->drawWithin(g,r.reduced(6).withSizeKeepingCentre(juce::jmin(iconSize_,r.getWidth()-12),juce::jmin(iconSize_,r.getHeight()-12)),juce::RectanglePlacement::centred,isEnabled()?1.f:.35f);if(glyph_==Glyph::power&&getToggleState()){g.setColour(colour(*this,ColourRole::on));g.fillEllipse(getWidth()-8.f,3.f,4.f,4.f);}}

};
class MRToggle final : public juce::ToggleButton {
public:explicit MRToggle(juce::String label):juce::ToggleButton(label){setTitle(label);setWantsKeyboardFocus(true);setSize(190,32);}
    void paintOverChildren(juce::Graphics& g)override{if(hasKeyboardFocus(true)){g.setColour(colour(*this,ColourRole::focus));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),radius(*this),1);}}
};
using MRSwitch=MRToggle;
class MRSelector final : public juce::ComboBox {
public:
    MRSelector(juce::String name,const juce::StringArray& choices){if(choices.isEmpty())throw std::invalid_argument("Empty selector");setTitle(name);for(int i=0;i<choices.size();++i)addItem(choices[i],i+1);setSelectedId(1,juce::dontSendNotification);setSize(220,32);}
    void select(int index,juce::NotificationType notify=juce::sendNotificationSync){if(index<0||index>=getNumItems())throw std::invalid_argument("Invalid selector index");setSelectedId(index+1,notify);}
    int selected()const{return getSelectedId()-1;}
    void paintOverChildren(juce::Graphics& g)override{if(hasKeyboardFocus(true)){g.setColour(colour(*this,ColourRole::focus));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),radius(*this),1.5f);}}
};
using MRDropdown=MRSelector;
class MRSegmented final : public juce::Component {
    class Segment final : public juce::TextButton {
        MRSegmented& owner;int index;
    public:Segment(MRSegmented& o,int i,juce::String name):juce::TextButton(name),owner(o),index(i){setTitle(name);setClickingTogglesState(true);setRadioGroupId(1);onClick=[this]{owner.select(index,juce::sendNotificationSync);};}
        bool keyPressed(const juce::KeyPress& key)override{if(!isEnabled())return false;const auto k=key.getKeyCode();if(k==juce::KeyPress::leftKey||k==juce::KeyPress::rightKey||k==juce::KeyPress::homeKey||k==juce::KeyPress::endKey){const auto last=static_cast<int>(owner.buttons_.size())-1;owner.select(k==juce::KeyPress::homeKey?0:k==juce::KeyPress::endKey?last:juce::jlimit(0,last,index+(k==juce::KeyPress::leftKey?-1:1)),juce::sendNotificationSync);owner.buttons_[static_cast<std::size_t>(owner.selected_)]->grabKeyboardFocus();return true;}return juce::TextButton::keyPressed(key);}
    };
    std::vector<std::unique_ptr<Segment>> buttons_;int selected_{};
public:
    std::function<void(int)> onSelection;
    MRSegmented(juce::String name,const juce::StringArray& labels){if(labels.isEmpty()||labels.size()>8)throw std::invalid_argument("Segment count must be1..8");setTitle(name);for(int i=0;i<labels.size();++i){auto b=std::make_unique<Segment>(*this,i,labels[i]);addAndMakeVisible(*b);buttons_.push_back(std::move(b));}buttons_[0]->setToggleState(true,juce::dontSendNotification);setSize(240,32);}
    void select(int index,juce::NotificationType notify=juce::dontSendNotification){if(index<0||index>=static_cast<int>(buttons_.size()))throw std::invalid_argument("Invalid segment index");const bool changed=index!=selected_;selected_=index;buttons_[static_cast<std::size_t>(index)]->setToggleState(true,juce::dontSendNotification);if(changed&&notify!=juce::dontSendNotification&&onSelection)onSelection(index);}
    int selected()const{return selected_;}juce::Button& segment(int index){return *buttons_.at(static_cast<std::size_t>(index));}
    void resized()override{for(std::size_t i=0;i<buttons_.size();++i){const int left=static_cast<int>(i)*getWidth()/static_cast<int>(buttons_.size()),right=static_cast<int>(i+1)*getWidth()/static_cast<int>(buttons_.size());buttons_[i]->setBounds(left,0,right-left-2,getHeight());}}
};
class MRPanel final : public juce::Component {
    juce::String title_;int width_{1},height_{1};
public:
    juce::Component content;juce::Viewport viewport;
    explicit MRPanel(juce::String title):title_(std::move(title)){setTitle(title_);addAndMakeVisible(viewport);viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,true);}
    ~MRPanel()override{viewport.setViewedComponent(nullptr,false);}
    void setContentSize(int w,int h){if(w<1||w>8192||h<1||h>16384)throw std::invalid_argument("Panel extent outside bounds");width_=w;height_=h;resized();}
    void resized()override{viewport.setBounds(getLocalBounds().reduced(12).withTrimmedTop(28));content.setSize(juce::jmax(width_,viewport.getWidth()-16),height_);}
    void paint(juce::Graphics& g)override{auto r=getLocalBounds().toFloat().reduced(.5f);g.setColour(colour(*this,ColourRole::window));g.fillRoundedRectangle(r,radius(*this));g.setColour(colour(*this,ColourRole::border));g.drawRoundedRectangle(r,radius(*this),1);g.setFont(Theme::browserFont());g.setColour(colour(*this,ColourRole::text));g.drawText(title_,16,8,getWidth()-32,24,juce::Justification::left);}
};
// GUI-only extension contracts. The caller owns a safe snapshot and its lifetime;
// these interfaces do not read audio state, claim thread safety or invent DSP data.
struct MeterFrame {float left{},right{};bool demo{true};};
struct MeterSource {virtual ~MeterSource()=default;virtual MeterFrame snapshot()const=0;};
struct GraphSource {virtual ~GraphSource()=default;virtual std::span<const juce::Point<float>> points()const=0;virtual bool demo()const=0;};
}
