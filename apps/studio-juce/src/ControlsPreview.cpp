#include "MRControls.h"
#include <iostream>
namespace ui::kit {
namespace {
void ensure(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
ParameterSpec gain(){return {"gain","Gain / mini",Unit::db,-60,12,-12,.1,Mapping::linear,1};}
ParameterSpec frequency(){return {"frequency","Frequency",Unit::hz,20,20000,1000,1,Mapping::logarithmic,0};}
ParameterSpec mix(){return {"mix","Mix / large",Unit::percent,0,100,50,.1,Mapping::linear,1};}
ParameterSpec pan(){return {"pan","Pan / bipolar",Unit::none,-1,1,0,.01,Mapping::bipolar,2};}
class Catalog final : public juce::Component {
public:
    MRKnob mini{gain(),KnobSize::mini},standard{frequency()},large{mix(),KnobSize::large},bipolar{pan()},disabledKnob{{"disabled","Disabled",Unit::percent,0,100,50,1,Mapping::linear,0}};
    MRFader horizontal{{"level","Level / horizontal",Unit::db,-60,12,-6,.1,Mapping::linear,1}},vertical{{"output","Output / vertical",Unit::db,-60,12,0,.1,Mapping::linear,1},true};
    MRFader logarithmic{{"time","Time / logarithmic",Unit::ms,1,2000,125,1,Mapping::logarithmic,0}};
    std::array<MRParameterControl*,8> parameters{&mini,&standard,&large,&bipolar,&horizontal,&vertical,&logarithmic,&disabledKnob};
    std::array<juce::Label,6> labels;
    std::vector<std::unique_ptr<MRValueField>> values;
    MRButton action{"Action"},selected{"Selected"},disabled{"Disabled"},focus{"Focus me"};
    MRIconButton power{Glyph::power,"Module power (demo)"},add{Glyph::plus,"Add module (demo)"};
    MRToggle toggle{"Module enabled (demo)"},toggleDisabled{"Disabled switch"};
    MRSegmented segmented{"Placement (demo)",{"PRE","POST","A/B"}};
    MRDropdown dropdown{"Output mode (demo)",{text("Стерео / Stereo"),text("Моно / Mono"),text("Средний / Mid"),text("Боковой / Side")}};
    juce::Label actionStatus;std::function<void()> onAction;
    Catalog(){
        for(auto* c:parameters)addAndMakeVisible(*c);disabledKnob.setEnabled(false);
        const std::array<ParameterSpec,6> specs{{{"db","dB",Unit::db,-60,12,-6,.1,Mapping::linear,1},{"hz","Hz",Unit::hz,20,20000,1000,1,Mapping::logarithmic,0},{"ms","ms",Unit::ms,0,2000,125,.1,Mapping::linear,1},{"percent","%",Unit::percent,0,100,50,.1,Mapping::linear,1},{"ratio","Ratio",Unit::ratio,1,20,4,.1,Mapping::linear,1},{"error","Error example",Unit::percent,0,100,50,1,Mapping::linear,0}}};
        for(std::size_t i=0;i<specs.size();++i){auto field=std::make_unique<MRValueField>(specs[i]);addAndMakeVisible(*field);field->onCommit=[this](double){if(onAction)onAction();};values.push_back(std::move(field));labels[i].setText(specs[i].name,juce::dontSendNotification);addAndMakeVisible(labels[i]);}
        values.back()->setText("150 %",false);values.back()->commitText();
        for(auto* c:std::array<juce::Component*,11>{&action,&selected,&disabled,&focus,&power,&add,&toggle,&toggleDisabled,&segmented,&dropdown,&actionStatus})addAndMakeVisible(*c);
        selected.setClickingTogglesState(true);selected.setToggleState(true,juce::dontSendNotification);disabled.setEnabled(false);toggleDisabled.setEnabled(false);toggleDisabled.setToggleState(true,juce::dontSendNotification);toggle.setToggleState(true,juce::dontSendNotification);
        for(auto* b:std::array<juce::Button*,5>{&action,&selected,&focus,&power,&add})b->onClick=[this]{actionStatus.setText("Action received (demo)",juce::dontSendNotification);if(onAction)onAction();};
        dropdown.onChange=[this]{if(onAction)onAction();};segmented.onSelection=[this](int){if(onAction)onAction();};toggle.onClick=[this]{if(onAction)onAction();};
        setSize(860,1060);
    }
    void resized()override{
        mini.setTopLeftPosition(20,96);standard.setTopLeftPosition(130,60);large.setTopLeftPosition(290,20);bipolar.setTopLeftPosition(490,60);disabledKnob.setTopLeftPosition(666,60);
        horizontal.setBounds(24,292,430,84);logarithmic.setBounds(24,400,430,84);vertical.setBounds(536,300,100,208);
        for(std::size_t i=0;i<values.size();++i){const int x=24+static_cast<int>(i)*134;labels[i].setBounds(x,582,126,24);values[i]->setBounds(x,610,126,32);}
        action.setBounds(24,748,112,36);selected.setBounds(148,748,112,36);disabled.setBounds(272,748,112,36);focus.setBounds(396,748,112,36);power.setBounds(528,748,40,36);add.setBounds(580,748,40,36);actionStatus.setBounds(640,748,196,36);
        toggle.setBounds(24,800,240,32);toggleDisabled.setBounds(280,800,220,32);segmented.setBounds(24,920,288,36);dropdown.setBounds(344,920,290,36);
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(colour(*this,ColourRole::window));
        const auto heading=[&](int y,const char* title,const char* help){g.setFont(Theme::browserFont());g.setColour(colour(*this,ColourRole::text));g.drawText(title,24,y,800,24,juce::Justification::left);g.setFont(Theme::detailFont());g.setColour(colour(*this,ColourRole::muted));g.drawText(help,24,y+28,800,22,juce::Justification::left);};
        // Knob header sits below the varying visual sizes to preserve their centre alignment.
        g.setFont(Theme::detailFont());g.setColour(colour(*this,ColourRole::muted));g.drawText("01  KNOBS     24 / 56 / 96 visual px; 40 / 72 / 112 hit px",24,212,800,24,juce::Justification::left);
        heading(242,"02  FADERS","Relative drag / Ctrl fine / double-click reset / arrows / Home / End");
        heading(528,"03  VALUE ENTRY","Type a number with its unit; Enter commits, Escape restores. Red ! means invalid input.");
        heading(686,"04  BUTTONS & SWITCHES","Hover / press / selected / disabled / Tab focus. Icons: canonical MR UDS SVG.");
        heading(858,"05  SELECTORS","Segment arrows / Home / End; dropdown native keyboard, Unicode labels and popup.");
        heading(990,"06  ADAPTER EXTENSIONS","MeterSource / GraphSource are GUI snapshot contracts. No DSP measurements are displayed.");
        g.setColour(colour(*this,ColourRole::border));for(auto y:{238,524,682,854,986})g.drawHorizontalLine(y,24,836);
    }
};
class PreviewRoot final : public juce::Component {
public:
    Theme theme;DesignTokens tokens{DesignTokens::target()};int begins{},previews{},commits{},cancels{},actions{};
    Catalog catalog;MRPanel panel{"CORE MR CONTROLS / reusable native JUCE"};MRSelector profile{"Preview palette",{"MR UDS","Accepted reference"}},zoom{"Logical zoom",{"100%","125%","150%","200%"}};
    MRButton reset{"Reset demo"};juce::Label info;float factor{1};
    PreviewRoot(){setLookAndFeel(&theme);theme.applyPreview(tokens);for(auto* c:std::array<juce::Component*,5>{&panel,&profile,&zoom,&reset,&info})addAndMakeVisible(*c);panel.content.addAndMakeVisible(catalog);info.setJustificationType(juce::Justification::topLeft);info.setInterceptsMouseClicks(false,false);
        for(auto* c:catalog.parameters)c->onEvent=[this](const ControlEvent& e){switch(e.phase){case Phase::begin:++begins;break;case Phase::preview:++previews;break;case Phase::commit:++commits;break;case Phase::cancel:++cancels;break;}update(e.id+" = "+juce::String(e.value,2));};
        catalog.onAction=[this]{++actions;update("Native action");};
        profile.onChange=[this]{tokens=profile.selected()==0?DesignTokens::target():DesignTokens{};theme.applyPreview(tokens);sendLookAndFeelChange();repaint();};
        zoom.onChange=[this]{const std::array<float,4> scales{1,1.25f,1.5f,2};scale(scales[static_cast<std::size_t>(zoom.selected())]);};
        reset.onClick=[this]{for(auto* c:catalog.parameters){c->cancelGesture();c->setValue(c->spec().initial);}for(auto& c:catalog.values)c->setValue(c->spec().initial);begins=previews=commits=cancels=actions=0;update("Defaults restored");};
        update("Ready");setSize(1220,900);scale(1);
    }
    ~PreviewRoot()override{for(auto* c:catalog.parameters)c->onEvent=nullptr;catalog.onAction=nullptr;panel.content.removeChildComponent(&catalog);setLookAndFeel(nullptr);}
    void update(const juce::String& last){info.setText(text("DEMO / NO DSP\nЗначения витрины, не проекта.\n\nGESTURE EVENTS\n")+"Begin: "+juce::String(begins)+"\nPreview: "+juce::String(previews)+"\nCommit: "+juce::String(commits)+"\nCancel: "+juce::String(cancels)+"\nActions: "+juce::String(actions)+"\n\n"+last+text("\n\nКЛАВИАТУРА\nTab / Shift+Tab — фокус\nСтрелки — 10 шагов\nCtrl + стрелка — 1 шаг\nHome / End — границы\nEnter — принять\nEscape — отменить\n\nМЫШЬ\nCtrl + drag — точно\nДвойной щелчок — reset\nWheel — отдельный жест\n\nПотеря фокуса / disable\nотменяют drag.\n\nZoom — программный.\nФизический DPI не проверен."),juce::dontSendNotification);}
    void scale(float value){factor=value;catalog.setTransform(juce::AffineTransform::scale(value));panel.setContentSize(juce::roundToInt(860*value),juce::roundToInt(1060*value));}
    void resized()override{panel.setBounds(16,92,getWidth()-288,getHeight()-132);profile.setBounds(getWidth()-248,104,228,32);zoom.setBounds(getWidth()-248,148,228,32);reset.setBounds(getWidth()-248,192,228,32);info.setBounds(getWidth()-248,246,228,getHeight()-280);scale(factor);}
    void paint(juce::Graphics& g)override{g.fillAll(tokens.colour(ColourRole::canvas));g.setColour(tokens.colour(ColourRole::text));g.setFont(Theme::browserFont().withHeight(23));g.drawText("MR / CORE CONTROLS",24,14,getWidth()-48,32,juce::Justification::left);g.setColour(tokens.colour(ColourRole::muted));g.setFont(Theme::detailFont());g.drawText(text("#192 · реальные JUCE-компоненты · sandbox · без аудиоустройства"),24,50,getWidth()-48,24,juce::Justification::left);g.drawText("MR UI Controls 0.2 / separate preview / scroll to inspect all components",24,getHeight()-32,getWidth()-48,24,juce::Justification::left);}
};
void snapshot(juce::Component& c,const juce::String& name,float scale=1){auto image=c.createComponentSnapshot(c.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(name).createOutputStream();ensure(stream!=nullptr,"Screenshot output");stream->setPosition(0);stream->truncate();ensure(juce::PNGImageFormat().writeImageToStream(image,*stream),"Screenshot write");}
juce::MouseEvent mouse(juce::Component& c,float x,float y,bool ctrl=false,bool button=true){const auto now=juce::Time::getCurrentTime();return {juce::Desktop::getInstance().getMainMouseSource(),{x,y},juce::ModifierKeys((button?juce::ModifierKeys::leftButtonModifier:0)|(ctrl?juce::ModifierKeys::ctrlModifier:0)),1,0,0,0,0,&c,&c,now,{0,0},now,1,true};}
void smoke(){
    PreviewRoot root;auto& knob=root.catalog.mini;auto& s=knob.slider;
    for(auto* c:root.catalog.parameters){c->spec().validate();ensure(c->field.getHeight()>=28&&c->slider.getWidth()>=40,"Hit and value bounds");}
    auto linear=gain();ensure(linear.parse("-6,5 dB")==-6.5&&linear.parse("+12 dB")==12,"Decimal comma and unit parse");for(auto bad:{"13 dB","-61","NaN","Inf","5 Hz","6 garbage","+",""})ensure(!linear.parse(bad),"Invalid numeric entry accepted");
    const auto rejects=[](auto fn){bool failed=false;try{fn();}catch(const std::invalid_argument&){failed=true;}ensure(failed,"Invalid spec accepted");};
    auto invalid=frequency();invalid.minimum=0;rejects([&]{invalid.validate();});invalid=gain();invalid.initial=20;rejects([&]{invalid.validate();});invalid=gain();invalid.step=0;rejects([&]{invalid.validate();});
    auto& log=root.catalog.standard.slider;ensure(std::abs(log.proportionOfLengthToValue(.25)-20*std::pow(1000.,.25))<1e-8,"True logarithmic quarter mapping");for(auto p:{0.,.25,.5,.75,1.})ensure(std::abs(log.valueToProportionOfLength(log.proportionOfLengthToValue(p))-p)<1e-10,"Log inverse mapping");
    ensure(root.catalog.bipolar.slider.proportionOfLengthToValue(.5)==0,"Bipolar centre");
    s.mouseDown(mouse(s,0,100));s.mouseDrag(mouse(s,0,80));s.mouseUp(mouse(s,0,80));const auto normal=knob.value()-knob.spec().initial;ensure(root.begins==1&&root.commits==1&&normal>0,"Pointer one gesture commit");knob.setValue(knob.spec().initial);
    s.mouseDown(mouse(s,0,100,true));s.mouseDrag(mouse(s,0,80,true));s.mouseUp(mouse(s,0,80,true));const auto fine=knob.value()-knob.spec().initial;ensure(std::abs(normal/fine-10)<.5,"Ctrl fine drag");
    auto original=knob.value();auto committed=root.commits;auto cancelled=root.cancels;s.mouseDown(mouse(s,0,100));s.mouseDrag(mouse(s,0,60));s.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));s.mouseDrag(mouse(s,0,20));s.mouseUp(mouse(s,0,20));ensure(knob.value()==original&&root.commits==committed&&root.cancels==cancelled+1,"Escape cancels once, no post-cancel drag");
    s.mouseDown(mouse(s,0,100));s.mouseDrag(mouse(s,0,50));s.focusLost(juce::Component::focusChangedDirectly);s.mouseUp(mouse(s,0,50));ensure(knob.value()==original&&root.commits==committed,"Focus loss cancels");
    s.mouseDown(mouse(s,0,100));s.mouseDrag(mouse(s,0,50));knob.setEnabled(false);s.mouseUp(mouse(s,0,50));s.keyPressed(juce::KeyPress(juce::KeyPress::endKey));ensure(knob.value()==original&&!knob.gesturing(),"Disable cancels and rejects input");knob.setEnabled(true);
    s.keyPressed(juce::KeyPress(juce::KeyPress::endKey));ensure(knob.value()==12,"End maximum");s.mouseDoubleClick(mouse(s,0,0));ensure(knob.value()==-12,"Double click default");
    knob.field.setText("-6 dB",false);ensure(knob.field.commitText()&&knob.value()==-6,"Editable value feeds gesture API");knob.field.setText("99 dB",false);committed=root.commits;ensure(!knob.field.commitText()&&knob.value()==-6&&knob.field.hasError(),"Error preserves last good value");knob.field.onEscapeKey();ensure(knob.field.getText()=="-6.0 dB"&&!knob.field.hasError()&&root.commits==committed,"Escape restores value text");
    knob.field.setText("-8 dB",false);knob.field.onFocusLost();ensure(knob.value()==-8,"Valid focus loss commits value field");
    juce::MouseWheelDetails wheel;wheel.deltaY=.1f;committed=root.commits;s.mouseWheelMove(mouse(s,0,0,false,false),wheel);ensure(!knob.gesturing()&&root.commits==committed+1,"Wheel changes value and terminates one gesture");
    int lifetimeCancel{};{MRKnob temporary(gain());temporary.onEvent=[&](const ControlEvent& e){if(e.phase==Phase::cancel)++lifetimeCancel;};temporary.beginGesture();temporary.change(-5);}ensure(lifetimeCancel==1,"Destructor ends active gesture with cancellation");
    auto& segments=root.catalog.segmented;static_cast<juce::Component&>(segments.segment(0)).keyPressed(juce::KeyPress(juce::KeyPress::rightKey));ensure(segments.selected()==1&&segments.segment(1).getToggleState()&&!segments.segment(0).getToggleState(),"Segment keyboard selection single state");root.catalog.dropdown.select(1);ensure(root.catalog.dropdown.selected()==1&&root.catalog.dropdown.getText().contains(text("Моно")),"Dropdown selection UTF8");
    // Native window peer exercises focus transfer/Tab traversal without touching the DAW.
    root.addToDesktop(juce::ComponentPeer::windowIsTemporary);root.setVisible(true);s.grabKeyboardFocus();ensure(s.hasKeyboardFocus(false),"Slider keyboard focus");s.moveKeyboardFocusToSibling(true);ensure(knob.field.hasKeyboardFocus(false),"Tab traversal slider to editable field");root.catalog.focus.grabKeyboardFocus();ensure(root.catalog.focus.hasKeyboardFocus(false),"Button focus independent of selected state");snapshot(root,"controls192-focus.png");
    root.catalog.action.setState(juce::Button::buttonOver);root.catalog.focus.setState(juce::Button::buttonDown);snapshot(root.catalog,"controls192-hover-pressed.png");root.catalog.action.setState(juce::Button::buttonNormal);root.catalog.focus.setState(juce::Button::buttonNormal);
    for(auto scale:{1.f,1.25f,1.5f,2.f}){root.scale(scale);ensure(root.panel.content.getHeight()==juce::roundToInt(1060*scale),"Scroll extent scales");snapshot(root.catalog,"controls192-catalog-"+juce::String(static_cast<int>(scale*100))+".png",scale);}root.scale(1);snapshot(root,"controls192-window.png");root.setSize(1060,760);root.panel.viewport.setViewPosition(0,700);ensure(root.panel.viewport.getViewPositionY()>0,"Panel actual scroll");snapshot(root,"controls192-minimum.png");root.setSize(1600,1100);snapshot(root,"controls192-large.png");root.removeFromDesktop();
    std::cout<<"MR controls: numeric/units/log/bipolar/drag/fine/cancel/disable/reset/entry/wheel/lifetime/keyboard/Tab/scroll/Unicode/scales PASS\n";
}
class Window final : public juce::DocumentWindow {
public:
    Window():DocumentWindow(text("MR UI Controls 0.2 · #192"),DesignTokens::target().colour(ColourRole::canvas),allButtons){setUsingNativeTitleBar(true);setContentOwned(new PreviewRoot,true);setResizable(true,false);setResizeLimits(1060,760,2400,1600);centreWithSize(1220,900);setVisible(true);}
    void closeButtonPressed()override{juce::JUCEApplication::getInstance()->quit();}
};
class Application final : public juce::JUCEApplication {
    std::unique_ptr<Window> window;
public:
    const juce::String getApplicationName()override{return "MR UI Controls";}const juce::String getApplicationVersion()override{return "0.2.0";}bool moreThanOneInstanceAllowed()override{return true;}
    void initialise(const juce::String& args)override{if(args.contains("--smoke")){try{smoke();setApplicationReturnValue(0);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';juce::File::getCurrentWorkingDirectory().getChildFile("controls192-failure.txt").replaceWithText(e.what());setApplicationReturnValue(1);}quit();}else window=std::make_unique<Window>();}
    void shutdown()override{window.reset();}
};
}
}
START_JUCE_APPLICATION(ui::kit::Application)
