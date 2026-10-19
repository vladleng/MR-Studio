#include "Controls.h"
#include <iostream>
namespace ui {
namespace {
juce::String utf8(const char* value){return juce::String::fromUTF8(value);}
void ensure(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
class SamplePane final : public juce::Component {
public:
    Theme theme; // Declared before attached children; destroyed after them.
    DesignTokens tokens;bool baseline;
    std::array<juce::TextButton,4> buttons;
    juce::ComboBox selector;juce::Slider knob,slider;juce::ToggleButton power{"Module enabled (demo)"};
    explicit SamplePane(bool accepted):baseline(accepted){
        tokens=accepted?DesignTokens{}:DesignTokens::target();if(!accepted)theme.applyPreview(tokens);setLookAndFeel(&theme);
        const std::array<const char*,4> names{"Normal","Selected","Disabled","Focus me"};
        for(std::size_t i=0;i<buttons.size();++i){auto& b=buttons[i];addAndMakeVisible(b);b.setButtonText(names[i]);b.setTitle(names[i]);b.setExplicitFocusOrder(static_cast<int>(i)+1);}
        buttons[1].setToggleState(true,juce::dontSendNotification);buttons[1].setClickingTogglesState(true);buttons[2].setEnabled(false);buttons[3].onClick=[this]{buttons[3].grabKeyboardFocus();};
        addAndMakeVisible(power);power.setToggleState(true,juce::dontSendNotification);power.setTitle("Demo module power");
        addAndMakeVisible(selector);selector.addItem(utf8("Mono / Моно"),1);selector.addItem(utf8("Stereo / Стерео"),2);selector.setSelectedId(2);selector.setTitle("Sandbox channel selector");
        addAndMakeVisible(knob);knob.setTitle("Demo gain in dB");knob.setRange(-24,12,.1);knob.setValue(-6);knob.setTextValueSuffix(" dB");knob.setDoubleClickReturnValue(true,0);knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);knob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,28);knob.setVelocityModeParameters(.25,1,0,true,juce::ModifierKeys::ctrlModifier);
        addAndMakeVisible(slider);slider.setTitle("Demo frequency in Hz");slider.setRange(20,20000,1);slider.setSkewFactorFromMidPoint(1000);slider.setValue(2500);slider.setTextValueSuffix(" Hz");slider.setDoubleClickReturnValue(true,1000);slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,105,28);
        setSize(520,620);
    }
    ~SamplePane()override{setLookAndFeel(nullptr);}
    void apply(const DesignTokens& t){tokens=t;if(!baseline)theme.applyPreview(t);sendLookAndFeelChange();resized();repaint();}
    void resized()override{
        const auto density=tokens.metrics[5];const int h=juce::roundToInt(30*density),gap=juce::roundToInt(tokens.metrics[9]);
        auto r=getLocalBounds().reduced(24);r.removeFromTop(100);auto row=r.removeFromTop(h);for(auto& b:buttons)b.setBounds(row.removeFromLeft((getWidth()-48)/4).reduced(3,0));r.removeFromTop(gap+12);
        power.setBounds(r.removeFromTop(h));r.removeFromTop(gap);selector.setBounds(r.removeFromTop(h));r.removeFromTop(40);
        knob.setBounds(r.getX(),r.getY(),150,175);slider.setBounds(r.getX()+180,r.getY()+65,r.getWidth()-180,h);
    }
    void paint(juce::Graphics& g)override{
        const auto c=[&](ColourRole r){return tokens.colour(r);};auto bounds=getLocalBounds().toFloat().reduced(.5f);g.setColour(c(ColourRole::window));g.fillRoundedRectangle(bounds,tokens.metrics[6]);g.setColour(c(ColourRole::border));g.drawRoundedRectangle(bounds,tokens.metrics[6],tokens.metrics[7]);
        g.setFont(Theme::browserFont().withHeight(19));g.setColour(c(ColourRole::text));g.drawText(baseline?"Accepted 0.4f":"MR UDS / preview",24,22,getWidth()-48,28,juce::Justification::left);
        g.setFont(Theme::detailFont());g.setColour(c(ColourRole::muted));g.drawText(baseline?utf8("Current Theme · read-only reference"):utf8("Scoped Theme · editable tokens"),24,56,getWidth()-48,22,juce::Justification::left);
        const int y=selector.getBottom()+14;g.setColour(c(ColourRole::text));g.drawText(utf8("Gain / усиление"),24,y,150,24,juce::Justification::left);g.drawText(utf8("Frequency / частота"),204,y,getWidth()-230,24,juce::Justification::left);
        auto tile=juce::Rectangle<float>(24.f,static_cast<float>(getHeight()-160),static_cast<float>(getWidth()-48),68.f);g.setColour(c(ColourRole::panel));g.fillRoundedRectangle(tile,tokens.metrics[6]);g.setColour(c(ColourRole::border));g.drawRoundedRectangle(tile,tokens.metrics[6],tokens.metrics[7]);
        const auto size=tokens.metrics[8];const auto centre=tile.getCentre().withX(tile.getX()+28);juce::Path icon;icon.addCentredArc(centre.x,centre.y,size*.4f,size*.4f,0,.6f,juce::MathConstants<float>::twoPi-.6f,true);icon.startNewSubPath(centre.x,centre.y-size*.5f);icon.lineTo(centre.x,centre.y);g.setColour(c(ColourRole::on));g.strokePath(icon,juce::PathStrokeType(tokens.metrics[7]+.3f));
        g.setFont(Theme::browserFont());g.setColour(c(ColourRole::text));g.drawText(utf8("Module / модуль"),tile.toNearestInt().withTrimmedLeft(55).withTrimmedBottom(30),juce::Justification::centredLeft);g.setFont(Theme::detailFont());g.setColour(c(ColourRole::muted));g.drawText("Semantic power + border + text",tile.toNearestInt().withTrimmedLeft(55).withTrimmedTop(30),juce::Justification::centredLeft);
        const std::array<ColourRole,3> roles{ColourRole::on,ColourRole::warning,ColourRole::error};const std::array<juce::String,3> labels{utf8("ON · demo"),utf8("MISSING · demo"),utf8("ERROR · demo")};for(std::size_t i=0;i<3;++i){const int x=24+static_cast<int>(i)*(getWidth()-48)/3;g.setColour(c(roles[i]));g.fillEllipse(static_cast<float>(x),static_cast<float>(getHeight()-62),5,5);g.setFont(Theme::detailFont());g.drawText(labels[i],x+12,getHeight()-70,(getWidth()-48)/3-14,22,juce::Justification::left);}
    }
};
class Comparison final : public juce::Component {
public:
    SamplePane accepted{true},preview{false};
    Comparison(){addAndMakeVisible(accepted);addAndMakeVisible(preview);setSize(1096,652);}
    int previewHeight{628};
    void fit(){const auto& m=preview.tokens.metrics;previewHeight=628+juce::roundToInt(std::max(0.f,m[5]-1)*90+std::max(0.f,m[9]-12)*2);setSize(1096,juce::jmax(652,previewHeight+24));resized();}
    void resized()override{accepted.setBounds(12,12,530,628);preview.setBounds(554,12,530,previewHeight);}
};
class Stage final : public juce::Component {
public:
    Comparison comparison;float zoom{1};
    Stage(){addAndMakeVisible(comparison);scale(1);}
    void scale(float value){zoom=value;comparison.fit();comparison.setTransform(juce::AffineTransform::scale(value));setSize(juce::roundToInt(comparison.getWidth()*value),juce::roundToInt(comparison.getHeight()*value));}
};
class PreviewRoot final : public juce::Component {
public:
    Theme chrome;
    Stage stage;juce::Viewport viewport;
    DesignTokens current=DesignTokens::target();
    std::array<DesignTokens,2> drafts{DesignTokens{},DesignTokens::target()},saved=drafts;
    juce::ComboBox profile,scale,colour,metric;
    juce::TextEditor rgb;juce::Slider metricValue;
    juce::TextButton applyColour{"Update colour"},reset{"Reset profile"},save{"Save draft..."},load{"Load draft..."};
    juce::Label info,error;
    std::unique_ptr<juce::FileChooser> chooser;
    bool updating{};
    PreviewRoot(){
        chrome.applyPreview(DesignTokens::target());setLookAndFeel(&chrome);setTitle("MR design token comparison");
        addAndMakeVisible(viewport);viewport.setViewedComponent(&stage,false);viewport.setScrollBarsShown(true,true);
        for(auto* b:{&profile,&scale,&colour,&metric})addAndMakeVisible(*b);
        profile.addItem("MR UDS Preview",1);profile.addItem("Accepted 0.4f",2);profile.setSelectedId(1);profile.setTitle("Right preview profile");profile.onChange=[this]{drafts[current.uds?1:0]=current;current=drafts[profile.getSelectedId()==1?1:0];error.setText({},juce::dontSendNotification);refresh();};
        scale.addItem("100%",1);scale.addItem("125%",2);scale.addItem("150%",3);scale.addItem("200%",4);scale.setSelectedId(1);scale.setTitle("Logical preview zoom");scale.onChange=[this]{const std::array<float,4> values{1,1.25f,1.5f,2};stage.scale(values[static_cast<std::size_t>(scale.getSelectedId()-1)]);};
        for(std::size_t i=0;i<DesignTokens::colourNames.size();++i)colour.addItem(DesignTokens::colourNames[i],static_cast<int>(i)+1);colour.setSelectedId(3);colour.setTitle("Semantic colour role");colour.onChange=[this]{syncFields();};
        addAndMakeVisible(rgb);rgb.setTitle("RGB colour, #RRGGBB");rgb.setInputRestrictions(7,"#0123456789abcdefABCDEF");rgb.onReturnKey=[this]{setColourValue();};
        for(auto* b:{&applyColour,&reset,&save,&load})addAndMakeVisible(*b);
        applyColour.onClick=[this]{setColourValue();};reset.onClick=[this]{current=current.defaults();error.setText({},juce::dontSendNotification);refresh();};
        for(std::size_t i=0;i<DesignTokens::metricNames.size();++i)metric.addItem(DesignTokens::metricNames[i],static_cast<int>(i)+1);metric.setSelectedId(1);metric.setTitle("Typography and geometry token");metric.onChange=[this]{syncFields();};
        addAndMakeVisible(metricValue);metricValue.setSliderStyle(juce::Slider::LinearHorizontal);metricValue.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,28);metricValue.setTitle("Token value");metricValue.onValueChange=[this]{if(updating)return;const auto i=static_cast<std::size_t>(metric.getSelectedId()-1);auto next=current;next.metrics[i]=static_cast<float>(metricValue.getValue());set(next);};
        addAndMakeVisible(info);info.setJustificationType(juce::Justification::topLeft);addAndMakeVisible(error);error.setColour(juce::Label::textColourId,DesignTokens::target().colour(ColourRole::warning));error.setJustificationType(juce::Justification::topLeft);
        save.onClick=[this]{choose(true);};load.onClick=[this]{choose(false);};
        refresh();setSize(1440,850);
    }
    ~PreviewRoot()override{chooser.reset();viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);}
    bool dirty()const{for(std::size_t i=0;i<2;++i)if((i==(current.uds?1u:0u)?current:drafts[i])!=saved[i])return true;return false;}
    void set(const DesignTokens& t){try{t.validate();current=t;error.setText({},juce::dontSendNotification);refresh();}catch(const std::exception& e){error.setText(e.what(),juce::dontSendNotification);}}
    void setColourValue(){const auto s=rgb.getText().trim();if(s.length()!=7||!s.startsWithChar('#')||!s.substring(1).containsOnly("0123456789abcdefABCDEF")){error.setText("Use #RRGGBB",juce::dontSendNotification);return;}auto next=current;next.colours[static_cast<std::size_t>(colour.getSelectedId()-1)]=0xff000000|s.substring(1).getHexValue32();set(next);}
    void syncFields(){updating=true;const auto c=static_cast<std::size_t>(colour.getSelectedId()-1),m=static_cast<std::size_t>(metric.getSelectedId()-1);rgb.setText("#"+juce::String::toHexString(static_cast<int>(current.colours[c]&0xffffff)).paddedLeft('0',6).toUpperCase(),false);metricValue.setRange(DesignTokens::minimum[m],DesignTokens::maximum[m],.01);metricValue.setValue(current.metrics[m],juce::dontSendNotification);updating=false;}
    void refresh(){stage.comparison.preview.apply(current);stage.scale(stage.zoom);syncFields();info.setText("Changed roles\n"+current.diff()+"\n\nSegoe UI / system fallback\nMixer font remains separate.\n\n"+(dirty()?juce::String("Unsaved draft"):juce::String("No unsaved changes")),juce::dontSendNotification);info.setTooltip(info.getText());repaint();}
    void writeDraft(const juce::File& f){juce::TemporaryFile temp(f);if(!temp.getFile().replaceWithText(current.encode())||!temp.overwriteTargetFileWithTemporary())throw std::runtime_error("Cannot save draft");saved[current.uds?1:0]=current;refresh();}
    void readDraft(const juce::File& f){if(f.getSize()>16384)throw std::runtime_error("Draft exceeds 16KB");auto t=DesignTokens::decode(f.loadFileAsString());drafts[current.uds?1:0]=current;current=t;drafts[t.uds?1:0]=t;saved[t.uds?1:0]=t;profile.setSelectedId(t.uds?1:2,juce::dontSendNotification);refresh();}
    void choose(bool writing){chooser=std::make_unique<juce::FileChooser>(writing?"Save token draft":"Open token draft",juce::File(),"*.json");juce::Component::SafePointer<PreviewRoot> safe(this);chooser->launchAsync(writing?(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting):(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles),[safe,writing](const juce::FileChooser& c){if(!safe)return;auto f=c.getResult();if(f==juce::File())return;try{if(writing)safe->writeDraft(f);else safe->readDraft(f);}catch(const std::exception& e){safe->error.setText(e.what(),juce::dontSendNotification);}});}
    void paint(juce::Graphics& g)override{auto t=DesignTokens::target();g.fillAll(t.colour(ColourRole::canvas));g.setColour(t.colour(ColourRole::text));g.setFont(Theme::browserFont().withHeight(23));g.drawText("MR / DESIGN PREVIEW",24,14,getWidth()-48,36,juce::Justification::left);g.setFont(Theme::browserFont());g.setColour(t.colour(ColourRole::muted));g.drawText("Current reference + scoped MR UDS     /     live tokens, native JUCE",24,52,getWidth()-48,24,juce::Justification::left);g.setFont(Theme::detailFont());g.drawText(utf8("DESIGN SANDBOX · no project or audio engine · values are demo only · logical zoom is not a physical DPI test"),24,getHeight()-30,getWidth()-48,22,juce::Justification::left);
        const int x=getWidth()-266;g.setFont(Theme::browserFont());g.setColour(t.colour(ColourRole::text));g.drawText("RIGHT PREVIEW",x,100,240,24,juce::Justification::left);g.drawText("Colour role / RGB",x,203,240,24,juce::Justification::left);g.drawText("Typography / geometry",x,345,240,24,juce::Justification::left);
    }
    void resized()override{viewport.setBounds(12,94,getWidth()-300,getHeight()-138);const int x=getWidth()-266;profile.setBounds(x,132,240,30);scale.setBounds(x,168,240,28);colour.setBounds(x,235,240,30);rgb.setBounds(x,274,114,30);applyColour.setBounds(x+120,274,120,30);metric.setBounds(x,376,240,30);metricValue.setBounds(x,416,240,60);reset.setBounds(x,490,240,30);save.setBounds(x,530,116,30);load.setBounds(x+124,530,116,30);info.setBounds(x,580,240,juce::jmax(24,getHeight()-688));error.setBounds(x,getHeight()-108,240,64);}
};
void snapshot(juce::Component& c,const juce::String& name,float scale=1){auto image=c.createComponentSnapshot(c.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(name).createOutputStream();ensure(stream!=nullptr,"Screenshot output");stream->setPosition(0);stream->truncate();ensure(juce::PNGImageFormat().writeImageToStream(image,*stream),"Screenshot write");}
bool same(const juce::Image& a,const juce::Image& b){if(a.getBounds()!=b.getBounds())return false;for(int y=0;y<a.getHeight();++y)for(int x=0;x<a.getWidth();++x)if(a.getPixelAt(x,y)!=b.getPixelAt(x,y))return false;return true;}
void smoke(){
    const auto rejects=[](auto f){bool threw{};try{f();}catch(const std::exception&){threw=true;}ensure(threw,"Invalid token draft accepted");};
    for(auto t:{DesignTokens{},DesignTokens::target()}){ensure(DesignTokens::decode(t.encode())==t,"Exact schema1 roundtrip");t.metrics[0]=std::numeric_limits<float>::quiet_NaN();rejects([&]{t.validate();});t=DesignTokens{};t.colours[0]=0;rejects([&]{t.validate();});}
    auto encoded=DesignTokens::target().encode();rejects([&]{DesignTokens::decode(encoded.replace("\"version\": 1","\"version\": 99"));});rejects([&]{DesignTokens::decode(encoded.replace("\"density\": 1","\"density\": 100"));});rejects([&]{DesignTokens::decode("{\"unexpected\":1}");});rejects([&]{DesignTokens::decode(juce::String::repeatedString("x",16385));});
    auto* global=&juce::LookAndFeel::getDefaultLookAndFeel();PreviewRoot root;root.setSize(1440,850);Theme untouched;juce::TextButton b("baseline");const auto font=untouched.getTextButtonFont(b,30).getHeight();const auto left=root.stage.comparison.accepted.createComponentSnapshot(root.stage.comparison.accepted.getLocalBounds(),true,1,juce::SoftwareImageType{});
    auto t=root.current;t.colours[2]=0xff293849;t.metrics[6]=12;t.metrics[0]=17;root.set(t);ensure(root.dirty(),"Draft dirty state");ensure(!untouched.isPreview()&&untouched.getTextButtonFont(b,30).getHeight()==font&&global==&juce::LookAndFeel::getDefaultLookAndFeel(),"Default Theme and global LAF isolation");ensure(same(left,root.stage.comparison.accepted.createComponentSnapshot(root.stage.comparison.accepted.getLocalBounds(),true,1,juce::SoftwareImageType{})),"Accepted subtree exact render isolation");
    {PreviewRoot other;ensure(other.current==DesignTokens::target(),"Independent simultaneous context");other.set(t);}ensure(root.current==t,"Other context teardown preserves owner");
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("token-smoke-draft.json");root.writeDraft(file);root.set(DesignTokens::target());root.readDraft(file);ensure(root.current==t&&!root.dirty(),"Atomic draft save/load and reset");root.reset.onClick();ensure(root.current==DesignTokens::target(),"Reset button restores profile defaults");root.saved[root.current.uds?1:0]=root.current;root.refresh();ensure(root.current.diff()=="Profile defaults","Default diff/reset");
    auto dense=DesignTokens::target();dense.metrics[5]=1.4f;dense.metrics[9]=24;root.set(dense);auto& pane=root.stage.comparison.preview;ensure(pane.knob.getBottom()<=pane.getHeight()-160,"Maximum density does not overlap module tile");root.set(DesignTokens::target());root.saved[1]=root.current;root.refresh();
    b.getProperties().set("stripFont",true);ensure(untouched.getTextButtonFont(b,30).getHeight()==12&&pane.theme.getTextButtonFont(b,30).getHeight()==12,"Mixer typography exception");ensure(pane.selector.getText().contains(utf8("Стерео")),"UTF8 Cyrillic selector");
    pane.buttons[0].setState(juce::Button::buttonOver);pane.buttons[3].setState(juce::Button::buttonDown);snapshot(root.stage.comparison,"theme191-hover-pressed.png");pane.buttons[0].setState(juce::Button::buttonNormal);pane.buttons[3].setState(juce::Button::buttonNormal);
    for(auto zoom:{1.f,1.25f,1.5f,2.f}){root.stage.scale(zoom);ensure(root.stage.getWidth()==juce::roundToInt(1096*zoom),"DPI extent");snapshot(root.stage.comparison,"theme191-comparison-"+juce::String(static_cast<int>(zoom*100))+".png",zoom);}root.stage.scale(1);snapshot(root,"theme191-window.png");root.setSize(1100,720);ensure(root.info.getBottom()<=root.error.getY()&&root.load.getBottom()<root.info.getY(),"Minimum inspector layout");snapshot(root,"theme191-minimum.png");std::cout<<"Tokens schema/bounds/draft/Theme isolation/render/DPI/lifetime PASS; no audio-linked target\n";
}
class Window final : public juce::DocumentWindow {
public:
    Window():DocumentWindow(utf8("MR UI Preview · #191"),DesignTokens::target().colour(ColourRole::canvas),allButtons){setUsingNativeTitleBar(true);setContentOwned(new PreviewRoot,true);setResizable(true,false);setResizeLimits(1100,720,2400,1600);centreWithSize(1440,850);setVisible(true);}
    void closeButtonPressed()override{auto* root=dynamic_cast<PreviewRoot*>(getContentComponent());if(!root||!root->dirty()){juce::JUCEApplication::getInstance()->quit();return;}juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Unsaved token draft").withMessage("Discard the preview changes? The MR Studio project is unaffected.").withButton("Discard").withButton("Keep editing"),[](int result){if(result==1)juce::JUCEApplication::getInstance()->quit();});}
};
class PreviewApplication final : public juce::JUCEApplication {
    std::unique_ptr<Window> window;
public:
    const juce::String getApplicationName()override{return "MR UI Preview";}
    const juce::String getApplicationVersion()override{return "0.1.0";}
    bool moreThanOneInstanceAllowed()override{return true;}
    void initialise(const juce::String& args)override{if(args.contains("--smoke")){try{smoke();setApplicationReturnValue(0);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';juce::File::getCurrentWorkingDirectory().getChildFile("theme191-failure.txt").replaceWithText(e.what());setApplicationReturnValue(1);}quit();}else window=std::make_unique<Window>();}
    void shutdown()override{window.reset();}
};
}
}
START_JUCE_APPLICATION(ui::PreviewApplication)
