#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include <cmath>
#include <map>
#include <array>
#include <optional>
#include <functional>
#include "DesignTokens.h"
#include "ConceptSkin.h"
#include "ObjectPalette.h"
namespace ui {
constexpr auto background = 0xff212830;
constexpr auto panel = 0xff212830;
constexpr auto surface = 0xff212830;
constexpr auto arrangementCanvas = 0xff262e37;
constexpr auto outline = 0xff3d444d;
constexpr auto separatorColour = 0xff343c45;
constexpr auto contentSurface = 0xff2d353e;
// Neutral button RGB components are 5% lighter than the upd9 fill.
constexpr auto raised = 0xff2f3841;
constexpr auto primaryText = 0xffdce3ea;
constexpr auto secondaryText = 0xff9daabc;
constexpr float cornerRadius = 6.f;
constexpr float borderWidth = 1.f;
inline void drawCard(juce::Graphics& g,juce::Rectangle<float> bounds,juce::Colour fill=juce::Colour(panel)){
    const auto r=bounds.reduced(.5f);g.setColour(fill);g.fillRoundedRectangle(r,cornerRadius);
    g.setColour(juce::Colour(outline));g.drawRoundedRectangle(r,cornerRadius,borderWidth);
}
constexpr float clipFillAlpha = .72f;
constexpr auto accent = 0xff006dcc;
constexpr auto menuBorder = outline;
constexpr float popupFillAlpha = .85f; // 15% background transparency; text stays opaque.
inline void configurePopupColours(juce::LookAndFeel& theme){
    theme.setColour(juce::PopupMenu::backgroundColourId,juce::Colour(surface).withAlpha(popupFillAlpha));
    theme.setColour(juce::PopupMenu::textColourId,juce::Colours::whitesmoke);
    theme.setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(accent));
    theme.setColour(juce::PopupMenu::highlightedTextColourId,juce::Colours::white);
}
inline void drawPopupSurface(juce::Graphics& g,int width,int height,float alpha=popupFillAlpha,juce::Colour fill=juce::Colour(surface),juce::Colour border=juce::Colour(menuBorder)){
    g.setColour(fill.withAlpha(alpha));g.fillRoundedRectangle(0,0,static_cast<float>(width),static_cast<float>(height),4.f);g.setColour(border);g.drawRoundedRectangle(.5f,.5f,width-1.f,height-1.f,4.f,1.f);
}

// Snapshot only our own UI, once when a menu opens. Never capture the desktop,
// vendor windows or audio data. A menu outside an MR window uses plain alpha.
class PopupBackdrop final:public juce::Component {
    juce::Image image;juce::Colour fill;float opacity{popupFillAlpha};
    static juce::Component* findRoot(juce::Component& c){if(static_cast<bool>(c.getProperties()["mrPopupRoot"]))return &c;for(auto* child:c.getChildren())if(auto* root=findRoot(*child))return root;return nullptr;}
public:
    juce::Component::SafePointer<juce::Component> owner;
    PopupBackdrop(juce::Component& menu,juce::Colour colour,float alpha=popupFillAlpha):fill(colour),opacity(alpha),owner(&menu){
        setInterceptsMouseClicks(false,false);setAccessible(false);setBounds(menu.getLocalBounds());
        auto& desktop=juce::Desktop::getInstance();const auto area=menu.getScreenBounds();
        for(int i=desktop.getNumComponents();--i>=0;){auto* root=findRoot(*desktop.getComponent(i));if(!root||!root->isVisible())continue;
            const auto intersection=area.getIntersection(root->getScreenBounds());if(intersection.isEmpty())continue;
            const auto local=root->getLocalArea(nullptr,intersection);auto snapshot=root->createComponentSnapshot(local,true,1.f,juce::SoftwareImageType{});
            if(snapshot.isValid()){juce::ImageConvolutionKernel kernel(7);kernel.createGaussianBlur(3.f);auto blurred=snapshot.createCopy();kernel.applyToImage(blurred,snapshot,snapshot.getBounds());image=juce::Image(juce::Image::ARGB,getWidth(),getHeight(),true);juce::Graphics graphics(image);graphics.drawImage(blurred,intersection.translated(-area.getX(),-area.getY()).toFloat());}break;
        }
    }
    void paint(juce::Graphics& g)override{juce::Graphics::ScopedSaveState saved(g);juce::Path shape;shape.addRoundedRectangle(getLocalBounds().toFloat(),4.f);g.reduceClipRegion(shape);if(image.isValid())g.drawImageAt(image,0,0);g.setColour(fill.withAlpha(opacity));g.fillAll();g.setColour(juce::Colour(menuBorder));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),4.f,1.f);}
};

class Theme final : public juce::LookAndFeel_V4 {
public:
    Theme() : face(juce::Typeface::createSystemTypefaceFor(BinaryData::NotoSans_ttf,
                                                         BinaryData::NotoSans_ttfSize)) {
        const auto add=[&](const char* label,const char* data,int size){auto icon=juce::Drawable::createFromImageData(data,static_cast<std::size_t>(size));if(!icon)throw std::runtime_error("Invalid transport SVG");transport[label]=std::move(icon);transportColours[label]=juce::Colours::white;};
        add("Play",BinaryData::play_svg,BinaryData::play_svgSize);add("Pause",BinaryData::pause_svg,BinaryData::pause_svgSize);add("Stop",BinaryData::stop_svg,BinaryData::stop_svgSize);add("Record (R)",BinaryData::record_svg,BinaryData::record_svgSize);
        add("< Section",BinaryData::previous_svg,BinaryData::previous_svgSize);add("Section >",BinaryData::next_svg,BinaryData::next_svgSize);add("Loop section",BinaryData::loop_svg,BinaryData::loop_svgSize);
        add("host-power",MRConceptAssets::power_svg,MRConceptAssets::power_svgSize);add("host-pin",MRConceptAssets::pin_svg,MRConceptAssets::pin_svgSize);transportColours["host-power"]=transportColours["host-pin"]=juce::Colour(0xffdae5f4);
        for(const auto* name:{"editor_attach","editor_detach","editor_fullscreen","editor_restore"}){int size{};const auto* data=MRConceptAssets::getNamedResource((juce::String(name)+"_svg").toRawUTF8(),size);if(!data)throw std::runtime_error("Missing editor SVG");const auto key=std::string("host-")+name;add(key.c_str(),data,size);transportColours[key]=juce::Colour(0xffdae5f4);}
        add("host-tool_pointer",MRConceptAssets::tool_pointer_svg,MRConceptAssets::tool_pointer_svgSize);add("host-tool_knife",MRConceptAssets::tool_knife_svg,MRConceptAssets::tool_knife_svgSize);add("host-tool_split",MRConceptAssets::tool_split_svg,MRConceptAssets::tool_split_svgSize);for(const auto* key:{"host-tool_pointer","host-tool_knife","host-tool_split"})transportColours[key]=juce::Colour(0xffdae5f4);
        for(const auto* name:{"action_undo","action_redo","action_zoom_in","action_zoom_out","action_fit","action_snap","count_off","assistant","metronome"}){int size{};const auto* data=MRConceptAssets::getNamedResource((juce::String(name)+"_svg").toRawUTF8(),size);if(!data)throw std::runtime_error("Missing action SVG");const auto key=std::string("host-")+name;add(key.c_str(),data,size);transportColours[key]=juce::Colour(0xffdae5f4);}
        add("host-action_duplicate",MRConceptAssets::duplicate_svg,MRConceptAssets::duplicate_svgSize);add("host-browser_fx",MRConceptAssets::plugin_fx_svg,MRConceptAssets::plugin_fx_svgSize);add("host-browser_folder",MRConceptAssets::folder_svg,MRConceptAssets::folder_svgSize);for(const auto* key:{"host-action_duplicate","host-browser_fx","host-browser_folder"})transportColours[key]=juce::Colour(0xffa9bfdc);transportColours["host-metronome"]=juce::Colours::white;
        setColour(juce::TextButton::buttonColourId, juce::Colour(raised));
        configurePopupColours(*this);
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(accent));
        setColour(juce::TextButton::textColourOffId, role(ColourRole::text,primaryText));
        setColour(juce::Label::textColourId,role(ColourRole::text,primaryText));
        setColour(juce::ComboBox::textColourId,role(ColourRole::text,primaryText));
        setColour(juce::TextEditor::textColourId,role(ColourRole::text,primaryText));
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(background));setColour(juce::ComboBox::outlineColourId,role(ColourRole::border,outline));
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(surface));setColour(juce::TextEditor::outlineColourId,role(ColourRole::border,outline));
        setColour(juce::TreeView::linesColourId,juce::Colour(0xff858d94));setColour(juce::ScrollBar::thumbColourId,juce::Colour(0xff62686e));
    }
    // Explicit instance-only preview. Default construction retains accepted DAW behaviour.
    void applyPreview(const DesignTokens& tokens){tokens.validate();preview_=tokens;
        const auto c=[&](ColourRole role){return tokens.colour(role);};
        setColour(juce::TextButton::buttonColourId,c(ColourRole::raised));setColour(juce::TextButton::buttonOnColourId,c(ColourRole::focus));
        setColour(juce::TextButton::textColourOffId,c(ColourRole::text));setColour(juce::TextButton::textColourOnId,c(tokens.uds?ColourRole::window:ColourRole::text));
        setColour(juce::Label::textColourId,c(ColourRole::text));setColour(juce::ComboBox::textColourId,c(ColourRole::text));
        setColour(juce::ComboBox::backgroundColourId,c(ColourRole::deep));setColour(juce::ComboBox::outlineColourId,c(ColourRole::border));
        setColour(juce::TextEditor::textColourId,c(ColourRole::text));setColour(juce::TextEditor::backgroundColourId,c(ColourRole::deep));setColour(juce::TextEditor::outlineColourId,c(ColourRole::border));
        setColour(juce::PopupMenu::backgroundColourId,c(ColourRole::window).withAlpha(popupFillAlpha));setColour(juce::PopupMenu::textColourId,c(ColourRole::text));setColour(juce::PopupMenu::highlightedBackgroundColourId,c(ColourRole::focus));setColour(juce::PopupMenu::highlightedTextColourId,c(ColourRole::window));
        setColour(juce::Slider::trackColourId,c(ColourRole::focus));setColour(juce::Slider::thumbColourId,c(ColourRole::focus));setColour(juce::Slider::backgroundColourId,c(ColourRole::deep));
        setColour(juce::Slider::textBoxTextColourId,c(ColourRole::text));setColour(juce::Slider::textBoxBackgroundColourId,c(ColourRole::deep));setColour(juce::Slider::textBoxOutlineColourId,c(ColourRole::border));
    }
    void useLiveContextStyle(bool enabled){liveContext_=enabled;}
    bool liveContextStyle()const noexcept{return liveContext_;}
    void setPopupOpacity(float alpha){popupOpacity_=juce::jlimit(.7f,1.f,alpha);setColour(juce::PopupMenu::backgroundColourId,role(ColourRole::window,surface).withAlpha(popupOpacity_));}
    float popupOpacity()const noexcept{return popupOpacity_;}
    bool isPreview()const noexcept{return preview_.has_value();}
    void useConceptAssets(bool enabled){concept_=enabled?std::make_unique<ConceptSkin>():nullptr;}
    const ConceptSkin* conceptSkin()const noexcept{return concept_.get();}
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float min,float max,juce::Slider::SliderStyle style,juce::Slider& slider)override{
        if(!concept_||!static_cast<bool>(slider.getProperties()["mrConceptFader"])){
            juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,pos,min,max,style,slider);return;}
        const bool vertical=slider.isVertical();const auto area=slider.getLocalBounds().toFloat();
        const auto proportion=static_cast<float>(slider.valueToProportionOfLength(slider.getValue()));
        const auto k=vertical?juce::Rectangle<float>(area.getCentreX()-8,12+(1-proportion)*juce::jmax(1.f,area.getHeight()-42),16,18)
            :juce::Rectangle<float>(8+proportion*juce::jmax(1.f,area.getWidth()-34),8,18,juce::jmax(8.f,area.getHeight()-16));
        ConceptSkin::fader(g,area,k,vertical,slider.isEnabled(),slider.isMouseOver(true),slider.isMouseButtonDown(),slider.hasKeyboardFocus(true),slider.valueToProportionOfLength(slider.getDoubleClickReturnValue()));
    }
    juce::Colour role(ColourRole r,juce::uint32 fallback)const{return preview_?preview_->colour(r):juce::Colour(fallback);}
    float radius()const{return preview_?preview_->metrics[6]:cornerRadius;}
    float stroke()const{return preview_?preview_->metrics[7]:borderWidth;}
    juce::Font roleFont(std::size_t index)const{return preview_?detailFont().withHeight(preview_->metrics[index]).withHorizontalScale(index==2?1.f:preview_->metrics[4]):index==2?detailFont():browserFont();}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float start,float end,juce::Slider& slider)override{
        if(!preview_){juce::LookAndFeel_V4::drawRotarySlider(g,x,y,width,height,pos,start,end,slider);return;}
        auto r=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(width),static_cast<float>(height)).reduced(8);const auto side=juce::jmin(r.getWidth(),r.getHeight());r=r.withSizeKeepingCentre(side,side);const auto centre=r.getCentre();const float rad=side*.5f,angle=start+pos*(end-start);
        g.setColour(preview_->colour(ColourRole::deep));g.fillEllipse(r.reduced(5));juce::Path arc;arc.addCentredArc(centre.x,centre.y,rad,rad,0,start,end,true);g.setColour(preview_->colour(ColourRole::border));g.strokePath(arc,juce::PathStrokeType(stroke()));juce::Path value;value.addCentredArc(centre.x,centre.y,rad,rad,0,start,angle,true);g.setColour(preview_->colour(ColourRole::focus).withAlpha(slider.isEnabled()?1.f:.3f));g.strokePath(value,juce::PathStrokeType(stroke()+1));g.drawLine(centre.x,centre.y,centre.x+std::sin(angle)*rad*.65f,centre.y-std::cos(angle)*rad*.65f,stroke()+.5f);if(slider.hasKeyboardFocus(true))g.drawEllipse(r.expanded(4),stroke());
    }
    void drawMenuBarBackground(juce::Graphics& g,int,int,bool,juce::MenuBarComponent&) override {g.fillAll(role(ColourRole::window,surface));}
    void drawTreeviewPlusMinusBox(juce::Graphics& g,const juce::Rectangle<float>& area,juce::Colour,bool open,bool hover) override {
        g.setColour(hover?juce::Colours::white:juce::Colour(0xffb8bdc2));
        const auto centre=area.getCentre();const float size=juce::jmin(5.f,area.getWidth()*.3f);juce::Path arrow;
        if(open){arrow.startNewSubPath(centre.x-size,centre.y-size*.5f);arrow.lineTo(centre.x,centre.y+size*.5f);arrow.lineTo(centre.x+size,centre.y-size*.5f);}
        else {arrow.startNewSubPath(centre.x-size*.5f,centre.y-size);arrow.lineTo(centre.x+size*.5f,centre.y);arrow.lineTo(centre.x-size*.5f,centre.y+size);}
        g.strokePath(arrow,juce::PathStrokeType(1.5f));
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool hover,bool down) override {
        const bool smallToolbar=static_cast<bool>(button.getProperties()["mrSmallToolbarIcon"]);const float iconSize=button.getProperties().contains("mrIconSize")?static_cast<float>(button.getProperties()["mrIconSize"]):smallToolbar?14.f:20.f;
        const auto hostIcon=button.getProperties()["mrHostIcon"].toString();
        if(hostIcon.isNotEmpty()){
            const auto key=("host-"+hostIcon).toStdString();const auto at=transport.find(key);if(at==transport.end())return;
            const bool active=hostIcon=="power"?!button.getToggleState():button.getToggleState();
            auto colour=juce::Colour(active?ConceptSkin::selection:ConceptSkin::muted);if(hover||down)colour=colour.brighter(.15f);
            at->second->replaceColour(transportColours[key],colour);transportColours[key]=colour;
            const float hostSize=hostIcon=="count_off"?8.f:smallToolbar?14.f:18.f;const auto area=button.getLocalBounds().toFloat().withSizeKeepingCentre(hostSize,hostSize);
            if(active&&button.isEnabled())ConceptSkin::glow([&](juce::Point<float> p,float alpha){at->second->drawWithin(g,area.translated(p.x,p.y),juce::RectanglePlacement::centred,alpha);});
            at->second->drawWithin(g,area,juce::RectanglePlacement::centred,button.isEnabled()?1.f:.35f);
            if(static_cast<bool>(button.getProperties()["mrMixedPower"])){g.setColour(juce::Colour(ConceptSkin::amber));g.fillRect(area.getRight()-3,area.getY(),3.f,2.f);}
            if(button.hasKeyboardFocus(true)){g.setColour(colour);g.fillRect(area.getX(),area.getBottom()+1,area.getWidth(),1.f);}return;
        }
        if(concept_){const auto name=button.getProperties()["mrConceptIcon"].toString();
            if(name.isNotEmpty()){
                const bool channel=static_cast<bool>(button.getProperties()["mrConceptChannel"]);
                const auto area=channel?button.getLocalBounds().toFloat():button.getLocalBounds().toFloat().reduced(5).withSizeKeepingCentre(iconSize,iconSize);
                const auto glyph=channel?name+(button.getToggleState()?"_on":"_off"):name;
                if(button.isEnabled()&&(button.getToggleState()||down||button.hasKeyboardFocus(true)))
                    ConceptSkin::glow([&](juce::Point<float> p,float a){concept_->draw(g,glyph,area.translated(p.x,p.y),a,channel);});
                concept_->draw(g,glyph,area,!button.isEnabled()?.4f:hover||down||button.getToggleState()?1.f:.85f,channel);
                if(button.hasKeyboardFocus(true)){g.setColour(role(ColourRole::focus,primaryText));g.fillEllipse(area.getCentreX()-1,channel?area.getBottom()-2:area.getBottom()+2,2,2);}
                return;}}
        const auto text=button.getButtonText();auto area=button.getLocalBounds().toFloat().reduced(8);auto centre=area.getCentre();
        juce::ignoreUnused(centre);
        if(const auto at=transport.find(text.toStdString());at!=transport.end()){
            auto colour=text=="Record (R)"?juce::Colour(0xffe64b54):button.getToggleState()?juce::Colour(0xff31b7dd):juce::Colour(0xffc6d3df);
            if(text=="Record (R)"&&button.getToggleState())colour=colour.brighter(.35f);
            if(preview_&&static_cast<bool>(button.getProperties()["mrShellTransport"]))colour=preview_->colour(button.getToggleState()&&text!="Stop"?ColourRole::window:text=="Record (R)"?ColourRole::error:ColourRole::text);
            if(concept_)colour=role(text=="Record (R)"?ColourRole::error:button.getToggleState()?(text=="Play"?ColourRole::on:text=="Pause"?ColourRole::warning:ColourRole::focus):ColourRole::text,primaryText);
            if(button.hasKeyboardFocus(true)&&text!="Record (R)"&&!(preview_&&static_cast<bool>(button.getProperties()["mrShellTransport"])&&button.getToggleState()))colour=role(ColourRole::focus,0xff87ceeb);
            if(hover||down)colour=colour.brighter(down?.4f:.2f);if(!button.isEnabled())colour=colour.withAlpha(.35f);
            at->second->replaceColour(transportColours[text.toStdString()],colour);transportColours[text.toStdString()]=colour;
            if(concept_&&button.isEnabled()&&(button.getToggleState()||down||button.hasKeyboardFocus(true)))
                ConceptSkin::glow([&](juce::Point<float> p,float a){at->second->drawWithin(g,area.withSizeKeepingCentre(iconSize,iconSize).translated(p.x,p.y),juce::RectanglePlacement::centred,a);});
            if(concept_&&button.hasKeyboardFocus(true)){g.setColour(role(ColourRole::focus,primaryText));g.fillEllipse(area.getCentreX()-1,area.getCentreY()+12,2,2);}
            at->second->drawWithin(g,area.withSizeKeepingCentre(iconSize,iconSize),juce::RectanglePlacement::centred,1.f);if(text=="Record (R)"&&button.hasKeyboardFocus(true)&&!concept_){g.setColour(role(ColourRole::focus,0xff87ceeb));g.drawRect(button.getLocalBounds().reduced(5),1);}
        }else {g.setFont(getTextButtonFont(button,button.getHeight()));g.setColour(button.findColour(button.getToggleState()?juce::TextButton::textColourOnId:juce::TextButton::textColourOffId).withMultipliedAlpha(button.isEnabled()?1.f:.5f));g.drawText(text,button.getLocalBounds().reduced(3,1),juce::Justification::centred,true);}
    }
    juce::Font getTextButtonFont(juce::TextButton& button, int height) override {
        if(static_cast<bool>(button.getProperties()["flatMixerButton"])||static_cast<bool>(button.getProperties()["stripFont"]))return preview_?roleFont(2):detailFont();
        if(preview_)return roleFont(height<=22?1:0);
        return height<=22?browserFont().withHeight(13.f):browserFont();
    }
    // Reference density: Windows UI face, 12 logical JUCE units, with Cyrillic fallback.
    static juce::Font detailFont() { return juce::Font(juce::FontOptions("Segoe UI",12.f,juce::Font::plain)); }
    static juce::Font browserFont() { return detailFont().withHeight(14.f).withHorizontalScale(1.04f); }
    static juce::Font stripFont() { return detailFont(); }
    static juce::Font channelFont() { return detailFont(); }
    juce::Font getLabelFont(juce::Label& label) override {return preview_?roleFont(dynamic_cast<juce::ComboBox*>(label.getParentComponent())?3:2):dynamic_cast<juce::ComboBox*>(label.getParentComponent())?browserFont():detailFont();}
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& button,bool hover,bool down) override {
        drawTickBox(g,button,2.f,(button.getHeight()-16.f)*.5f,16.f,16.f,button.getToggleState(),button.isEnabled(),hover,down);
        g.setFont(detailFont());g.setColour(role(ColourRole::text,primaryText).withMultipliedAlpha(button.isEnabled()?1.f:.5f));
        g.drawText(button.getButtonText(),button.getLocalBounds().withTrimmedLeft(24).withTrimmedRight(2),juce::Justification::centredLeft,true);
    }
    void fillTextEditorBackground(juce::Graphics& g,int width,int height,juce::TextEditor& editor) override {
        g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));g.fillRoundedRectangle(juce::Rectangle<float>(0,0,static_cast<float>(width),static_cast<float>(height)).reduced(.5f),radius());
    }
    void drawTextEditorOutline(juce::Graphics& g,int width,int height,juce::TextEditor& editor) override {
        if(liveContext_ && static_cast<bool>(editor.getProperties()["liveTableCell"])) { if(editor.hasKeyboardFocus(true)){g.setColour(role(ColourRole::focus,accent));g.drawHorizontalLine(height-1,0.f,static_cast<float>(width));} return; }
        g.setColour(editor.hasKeyboardFocus(true)?role(ColourRole::focus,0xff87ceeb):role(ColourRole::border,outline));g.drawRoundedRectangle(juce::Rectangle<float>(0,0,static_cast<float>(width),static_cast<float>(height)).reduced(.5f),radius(),stroke());
    }
    juce::Font getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) override { return font(13); }
    juce::Font getPopupMenuFont() override { return preview_?roleFont(3):browserFont(); }
    void getIdealPopupMenuItemSize(const juce::String& text,bool separator,int height,int& width,int& resultHeight) override {
        // V4 reduces the glyph height after removing 2px from its item bounds.
        juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text,separator,separator?height:juce::jmax(liveContext_?30:22,height),width,resultHeight);
    }
    void drawPopupMenuItem(juce::Graphics& g,const juce::Rectangle<int>& area,bool separator,bool active,
                           bool highlighted,bool ticked,bool sub,const juce::String& text,
                           const juce::String& shortcut,const juce::Drawable* icon,const juce::Colour* colour)override{
        if(!liveContext_){juce::LookAndFeel_V4::drawPopupMenuItem(g,area,separator,active,highlighted,ticked,sub,text,shortcut,icon,colour);return;}
        if(separator){g.setColour(role(ColourRole::border,outline));g.fillRect(area.reduced(8).withHeight(1));return;}
        const auto bounds=area.toFloat().reduced(3.f,1.f);
        if(highlighted&&active){g.setColour(role(ColourRole::raised,raised));g.fillRoundedRectangle(bounds,2.f);g.setColour(role(ColourRole::focus,accent));g.drawRoundedRectangle(bounds.reduced(.5f),2.f,1.f);}
        g.setColour(role(active?ColourRole::text:ColourRole::muted,primaryText).withAlpha(active?1.f:.5f));
        g.setFont(getPopupMenuFont().boldened());
        g.drawText((ticked?juce::String::fromUTF8("✓ "):juce::String())+text,area.reduced(12,0).withTrimmedRight(sub?14:0),juce::Justification::centredLeft,true);
        if(sub)g.drawText(">",area.withWidth(18).withX(area.getRight()-18),juce::Justification::centred);
    }
    int getCallOutBoxBorderSize(const juce::CallOutBox& box)override{return liveContext_?4:juce::LookAndFeel_V4::getCallOutBoxBorderSize(box);}
    void drawCallOutBoxBackground(juce::CallOutBox& box,juce::Graphics& g,const juce::Path& path,juce::Image& cache)override{
        if(!liveContext_){juce::LookAndFeel_V4::drawCallOutBoxBackground(box,g,path,cache);return;}
        drawPopupSurface(g,box.getWidth(),box.getHeight(),popupOpacity_,role(ColourRole::window,surface),role(ColourRole::border,menuBorder));
    }
    void drawPopupMenuBackground(juce::Graphics& g,int width,int height) override {drawPopupSurface(g,width,height,popupOpacity_,role(ColourRole::window,surface),role(ColourRole::border,menuBorder));}
    void drawPopupMenuBackgroundWithOptions(juce::Graphics& g,int width,int height,const juce::PopupMenu::Options& options)override{PopupBackdrop* best{};double distance=1.e20;const auto target=options.getTargetScreenArea().getCentre();for(auto& candidate:popupBackdrops)if(candidate->owner&&candidate->getWidth()==width&&candidate->getHeight()==height){const auto bounds=candidate->owner->getScreenBounds();const auto centre=bounds.getCentre();const auto dx=static_cast<double>(centre.x)-target.x,dy=static_cast<double>(centre.y)-target.y;const auto score=dx*dx+dy*dy+(bounds.contains(target)?1.e12:0.);if(score<=distance){distance=score;best=candidate.get();}}if(best)best->paint(g);else drawPopupMenuBackground(g,width,height);}
    void preparePopupMenuWindow(juce::Component& window)override{juce::LookAndFeel_V4::preparePopupMenuWindow(window);std::erase_if(popupBackdrops,[](const auto& c){return !c->owner;});popupBackdrops.push_back(std::make_unique<PopupBackdrop>(window,role(ColourRole::window,surface),popupOpacity_));}
    juce::Font getComboBoxFont(juce::ComboBox&) override {return preview_?roleFont(3):browserFont();}
    void drawComboBox(juce::Graphics& g,int width,int height,bool,int,int,int,int,juce::ComboBox& box) override {
        if(preview_){auto r=box.getLocalBounds().toFloat().reduced(.5f);g.setColour(box.findColour(juce::ComboBox::backgroundColourId));g.fillRoundedRectangle(r,radius());g.setColour(preview_->colour(ColourRole::border));g.drawRoundedRectangle(r,radius(),stroke());}else drawCard(g,{0.f,0.f,static_cast<float>(width),static_cast<float>(height)},box.findColour(juce::ComboBox::backgroundColourId));
        g.setColour(role(ColourRole::muted,secondaryText));juce::Path arrow;const float x=width-13.f,y=height*.5f;
        arrow.startNewSubPath(x-3,y-2);arrow.lineTo(x,y+1);arrow.lineTo(x+3,y-2);g.strokePath(arrow,juce::PathStrokeType(1.f));
    }
    void drawFileBrowserRow(juce::Graphics& g,int width,int height,const juce::File&,const juce::String& filename,juce::Image* icon,const juce::String& sizeText,const juce::String& dateText,bool directory,bool selected,int,juce::DirectoryContentsDisplayComponent&) override {
        if(selected){g.setColour(juce::Colour(accent));g.fillRoundedRectangle(juce::Rectangle<float>(0,0,static_cast<float>(width),static_cast<float>(height)).reduced(1),radius());}
        if(icon&&icon->isValid())g.drawImageWithin(*icon,2,2,16,juce::jmax(1,height-4),juce::RectanglePlacement::centred);
        else {g.setColour(role(ColourRole::muted,secondaryText));g.drawRoundedRectangle(3.f,height*.5f-5,12.f,10.f,2.f,1.f);if(directory)g.drawHorizontalLine(juce::roundToInt(height*.5f-7),3,9);}
        g.setFont(browserFont());g.setColour(role(ColourRole::text,primaryText));
        if(width>450&&!directory){const int sizeX=juce::roundToInt(width*.7f),dateX=juce::roundToInt(width*.8f);
            g.drawText(filename,22,0,sizeX-24,height,juce::Justification::centredLeft,true);g.setColour(role(ColourRole::muted,secondaryText));
            g.drawText(sizeText,sizeX,0,dateX-sizeX-8,height,juce::Justification::centredRight,true);g.drawText(dateText,dateX,0,width-dateX-8,height,juce::Justification::centredRight,true);
        }else g.drawText(filename,22,0,juce::jmax(0,width-26),height,juce::Justification::centredLeft,true);
    }
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label) override{
        juce::LookAndFeel_V4::positionComboBoxText(box,label);
        label.setFont(browserFont());label.setMinimumHorizontalScale(1.f);
    }
    juce::Font font(float size) const { return juce::Font(juce::FontOptions(face).withPointHeight(size)); }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& c,
                              bool hover, bool down) override {
        if(static_cast<bool>(b.getProperties()["mrLiveTransport"])&&transport.contains(b.getButtonText().toStdString()))return;
        if(b.getProperties()["mrHostIcon"].toString().isNotEmpty())return;
        if(concept_){
            if(static_cast<bool>(b.getProperties()["mrChannelPlate"])){
                g.setColour(juce::Colour(ConceptSkin::channelButton).brighter(down?.12f:hover?.06f:0.f));g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(1),4);
                g.setColour(role(ColourRole::border,outline));g.drawRoundedRectangle(b.getLocalBounds().toFloat().reduced(1),4,1);
                if(b.hasKeyboardFocus(true))ConceptSkin::focus(g,b.getLocalBounds().toFloat());return;
            }
            if(static_cast<bool>(b.getProperties()["mrShellTransport"])||b.getProperties()["mrConceptIcon"].toString().isNotEmpty())return;
            const bool channel=static_cast<bool>(b.getProperties()["mrConceptChannel"]);
            if(!channel){const auto fill=static_cast<bool>(b.getProperties()["stripFont"])?juce::Colour(ConceptSkin::channelButton):role(ColourRole::raised,raised);g.setColour(fill.brighter(down?.12f:hover?.06f:0.f));g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(1),4);
                g.setColour(role(ColourRole::border,outline));g.drawRoundedRectangle(b.getLocalBounds().toFloat().reduced(1),4,1);
                if(b.getToggleState()){g.setColour(c);g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(3),3);}}
            if(b.hasKeyboardFocus(true))ConceptSkin::focus(g,b.getLocalBounds().toFloat());return;}
        if(static_cast<bool>(b.getProperties()["flatMixerButton"])){
            auto fill=c;if(down)fill=fill.darker(.08f);else if(hover)fill=fill.brighter(.08f);
            drawCard(g,b.getLocalBounds().reduced(1).toFloat(),fill);
            if(b.hasKeyboardFocus(true)){g.setColour(role(ColourRole::focus,0xff87ceeb));g.drawRoundedRectangle(b.getLocalBounds().reduced(1).toFloat(),radius(),stroke());}
            return;
        }
        if(transport.contains(b.getButtonText().toStdString())&&!(preview_&&static_cast<bool>(b.getProperties()["mrShellTransport"])))return;
        g.setColour(c.brighter(down ? .18f : hover ? .09f : 0.f));
        g.fillRoundedRectangle(b.getLocalBounds().reduced(1).toFloat(),radius());
        g.setColour(b.hasKeyboardFocus(true) ? role(ColourRole::focus,0xff87ceeb) : role(ColourRole::border,outline));
        g.drawRoundedRectangle(b.getLocalBounds().reduced(1).toFloat(),radius(),stroke());
    }
private:
    bool liveContext_{};
    float popupOpacity_{popupFillAlpha};
    std::vector<std::unique_ptr<PopupBackdrop>> popupBackdrops;
    std::unique_ptr<ConceptSkin> concept_;
    std::optional<DesignTokens> preview_;
    juce::Typeface::Ptr face;
    std::map<std::string,std::unique_ptr<juce::Drawable>> transport;
    std::map<std::string,juce::Colour> transportColours;
};

// Independent buttons prevent a bypass click from reaching the editor action.
// Toggle state is a projection of NativeInsert.bypass, never a second state owner.
class InsertPowerButton final : public juce::TextButton {
public:
    void mouseDown(const juce::MouseEvent& e) override {if(!e.mods.isPopupMenu())juce::TextButton::mouseDown(e);}
    void mouseUp(const juce::MouseEvent& e) override {if(!e.mods.isPopupMenu())juce::TextButton::mouseUp(e);}
    InsertPowerButton(){
        glyph.addCentredArc(6.f,6.f,4.5f,4.5f,0.f,.65f,juce::MathConstants<float>::twoPi-.65f,true);
        glyph.startNewSubPath(6.f,.5f);glyph.lineTo(6.f,6.f);
    }
    juce::Colour iconColour() const {return juce::Colour(getToggleState()?0xffbdc6cf:0xff3098ee);}
    void paint(juce::Graphics& g) override {
        if(getProperties()["mrHostIcon"].toString().isNotEmpty()){juce::TextButton::paint(g);return;}
        if(const auto* theme=dynamic_cast<const Theme*>(&getLookAndFeel());theme&&theme->conceptSkin()){
            auto colour=juce::Colour(getToggleState()?ConceptSkin::muted:ConceptSkin::selection);
            if(isMouseOver()||isDown())colour=colour.brighter(.15f);
            const auto transform=juce::AffineTransform::translation((getWidth()-12.f)*.5f,(getHeight()-12.f)*.5f);
            if(isEnabled()&&(!getToggleState()||isDown()||hasKeyboardFocus(true)))
                ConceptSkin::glow([&](juce::Point<float> p,float a){g.setColour(colour.withAlpha(a));g.strokePath(glyph,juce::PathStrokeType(1.3f),transform.translated(p.x,p.y));});
            g.setColour(colour.withAlpha(isEnabled()?1.f:.35f));g.strokePath(glyph,juce::PathStrokeType(1.3f),transform);
            if(hasKeyboardFocus(true)){g.setColour(juce::Colour(ConceptSkin::selection));g.fillEllipse(getWidth()*.5f-1,getHeight()-2.f,2,2);}
            if(static_cast<bool>(getProperties()["mrMixedPower"])){g.setColour(juce::Colour(ConceptSkin::amber));g.fillRect(getWidth()-3.f,0.f,3.f,2.f);}
            return;
        }
        auto colour=iconColour();if(isMouseOver()||isDown())colour=colour.brighter(isDown()?.35f:.2f);
        if(!isEnabled())colour=colour.withAlpha(.35f);g.setColour(colour);
        g.strokePath(glyph,juce::PathStrokeType(1.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded),
                     juce::AffineTransform::translation((getWidth()-12.f)*.5f,(getHeight()-12.f)*.5f));
        if(hasKeyboardFocus(true)){g.setColour(juce::Colours::skyblue);g.drawRect(getLocalBounds().toFloat().reduced(.5f),.75f);}
        if(static_cast<bool>(getProperties()["mrMixedPower"])){g.setColour(juce::Colour(ConceptSkin::amber));g.fillRect(getWidth()-3.f,0.f,3.f,2.f);}
    }
private:
    juce::Path glyph;
};

class InsertEditorButton final : public juce::TextButton {
public:
    void mouseDown(const juce::MouseEvent& e) override {if(!e.mods.isPopupMenu())juce::TextButton::mouseDown(e);}
    void mouseUp(const juce::MouseEvent& e) override {if(!e.mods.isPopupMenu())juce::TextButton::mouseUp(e);}
    void paint(juce::Graphics& g) override {
        getLookAndFeel().drawButtonBackground(g,*this,findColour(buttonColourId),isMouseOver(true),isDown());
        g.setFont(Theme::stripFont());g.setColour(findColour(textColourOffId).withMultipliedAlpha(isEnabled()?1.f:.5f));
        // One shared surface, with an explicit text inset for the power child.
        g.drawText(getButtonText(),getLocalBounds().withTrimmedLeft(17).withTrimmedRight(3),juce::Justification::centredLeft,true);
    }
};

class MixerInsert final : public juce::Component {
public:
    MixerInsert(const juce::String& name,bool bypassed) {
        bypass.setComponentID("mixer-insert-bypass");editor.setComponentID("mixer-insert-editor");
        bypass.getProperties().set("stripFont",true);editor.getProperties().set("stripFont",true);
        editor.getProperties().set("flatMixerButton",true);
        bypass.setTitle((bypassed?"Enable ":"Disable ")+name);bypass.setDescription("Plugin power: blue is active, grey is bypassed. Toggle without opening the editor");
        bypass.setToggleState(bypassed,juce::dontSendNotification);
        bypass.setTooltip(bypassed?"Bypassed — click to enable; right click for menu":"Active — click to bypass; right click for menu");
        editor.setButtonText(name);editor.setTitle("Open "+name);editor.setTooltip(name+(bypassed?" — bypassed":" — active"));
        addAndMakeVisible(editor);editor.addAndMakeVisible(bypass);addMouseListener(this,true);
    }
    std::function<void(juce::Point<int>)> menu;
    std::function<void()> beginDrag;
    bool dragged{};
    void mouseDown(const juce::MouseEvent& e) override {dragged=false;if(e.mods.isPopupMenu()&&menu)menu(e.getScreenPosition());}
    void mouseDrag(const juce::MouseEvent& e) override {if(!dragged&&e.mods.isLeftButtonDown()&&e.eventComponent!=&bypass&&e.getDistanceFromDragStart()>5&&beginDrag){dragged=true;editor.setState(juce::Button::buttonNormal);beginDrag();}}
    void resized() override {editor.setBounds(getLocalBounds());const auto side=juce::jmin(13,juce::jmin(getWidth(),getHeight()));bypass.setBounds(2,(getHeight()-side)/2,side,side);}
    InsertPowerButton bypass;
    InsertEditorButton editor;
};

// Pointer gestures start only on the handle. Relative movement never jumps to a rail click.
// Preview runs through Application's bounded mixer queue; release creates one history entry.
class Handle final : public juce::Component,public juce::SettableTooltipClient {
public:
    explicit Handle(bool vertical) : vertical_(vertical) { setWantsKeyboardFocus(true); }
    std::function<void(double)> preview, commit;
    std::function<void()> cancel;
    double value{.8};
    bool rotary{};
    bool compact{}; // Same gain/gesture binding; fill-only horizontal presentation.
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
        if(compact&&!vertical_)return {12.f+static_cast<float>(value)*travel()-9.f,0,18.f,static_cast<float>(getHeight())};
        if (vertical_) return {static_cast<float>(getWidth()/2-8), 12.f + static_cast<float>(1-value) * travel(),
                               16.f, 18.f};
        return {8.f + static_cast<float>(value)*travel(), 8.f, 18.f, static_cast<float>(getHeight()-16)};
    }
    void paint(juce::Graphics& g) override {
        if(compact&&!vertical_&&!rotary){
            const auto y=getHeight()*.5f;const auto end=12.f+static_cast<float>(value)*travel();const auto alpha=isEnabled()?1.f:.35f;
            g.setColour(juce::Colour(ConceptSkin::muted).withAlpha(.22f*alpha));g.drawLine(12,y,getWidth()-12.f,y,3.f);
            g.setColour(juce::Colour(ConceptSkin::cyan).brighter(active?.2f:isMouseOver(true)?.1f:0.f).withAlpha(alpha));if(end>12.f){juce::Path fill;fill.startNewSubPath(12,y);fill.lineTo(end,y);g.strokePath(fill,juce::PathStrokeType(3.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));}
            const auto unity=12.f+static_cast<float>(60./72.)*travel();g.setColour(juce::Colour(ConceptSkin::muted).withAlpha(alpha));g.drawLine(unity,y-4,unity,y-2,1.f);
            if(hasKeyboardFocus(true)&&!pointerFocus_){g.setColour(juce::Colour(ConceptSkin::selection));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),2.f,1.f);}return;
        }
        if(const auto* theme=dynamic_cast<const Theme*>(&getLookAndFeel());theme&&theme->conceptSkin()){
            const bool gain=static_cast<bool>(getProperties()["mrConceptGain"]);
            const bool focused=hasKeyboardFocus(true)&&!pointerFocus_;
            if(rotary)ConceptSkin::pan(g,knob(),value,isEnabled(),focused);
            else ConceptSkin::fader(g,getLocalBounds().toFloat(),knob(),vertical_,isEnabled(),isMouseOver(true),active,focused,vertical_||gain?60./72.:.5,!vertical_&&!gain);
            return;}
        if(rotary){auto r=knob();g.setColour(juce::Colour(0xff20262b));g.fillEllipse(r);g.setColour(hasKeyboardFocus(true)?juce::Colours::skyblue:juce::Colours::grey);g.drawEllipse(r,1);
            const float angle=static_cast<float>((value-.5)*4.7);auto c=r.getCentre();g.setColour(juce::Colours::skyblue);g.drawLine(c.x,c.y,c.x+std::sin(angle)*r.getWidth()*.38f,c.y-std::cos(angle)*r.getHeight()*.38f,2);return;}
        const auto rail=getLocalBounds().reduced(vertical_ ? 12 : 8, vertical_ ? 10 : 17);
        g.setColour(juce::Colour(0xff15191c));
        g.fillRect(rail);
        g.setColour(juce::Colour(0xff485159));
        if(vertical_) g.drawVerticalLine(rail.getCentreX(),static_cast<float>(rail.getY()),static_cast<float>(rail.getBottom()));
        else g.drawHorizontalLine(rail.getCentreY(),static_cast<float>(rail.getX()),static_cast<float>(rail.getRight()));
        g.setColour(hasKeyboardFocus(true) ? juce::Colours::skyblue : juce::Colour(0xffb7c2cb));
        g.fillRect(knob()); g.setColour(juce::Colour(0xff4e5963));
        auto k=knob(); g.drawLine(k.getX(),k.getCentreY(),k.getRight(),k.getCentreY(),2);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isLeftButtonDown()){pointerFocus_=true;repaint();}
        if (!e.mods.isLeftButtonDown() || !knob().contains(e.position)) return;
        if(rotary && e.position.getDistanceFrom(knob().getCentre())>knob().getWidth()/2)return;
        pointerFocus_=true;grabKeyboardFocus(); active=true; origin=value; last=vertical_ ? e.position.y : e.position.x;
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
    void mouseDoubleClick(const juce::MouseEvent& e)override{if(!compact||vertical_||!e.mods.isLeftButtonDown())return;abort();const auto unity=60./72.;if(value==unity)return;value=unity;if(commit)commit(value);notifyValue();repaint();}
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::escapeKey && active) { abort(); return true; }
        if(active) return false;
        const int k=key.getKeyCode();
        if(k!=juce::KeyPress::upKey && k!=juce::KeyPress::rightKey &&
           k!=juce::KeyPress::downKey && k!=juce::KeyPress::leftKey) return false;
        const auto step=key.getModifiers().isCtrlDown() ? .001 : .01;
        pointerFocus_=false;
        value=juce::jlimit(0.,1.,value+((k==juce::KeyPress::upKey || k==juce::KeyPress::rightKey) ? step : -step));
        if(commit) commit(value); notifyValue();repaint(); return true;
    }
    void focusLost(FocusChangeType) override { abort(); }
    void mouseEnter(const juce::MouseEvent&)override{repaint();}
    void mouseExit(const juce::MouseEvent&)override{repaint();}
    void focusGained(FocusChangeType cause)override{if(cause==focusChangedByTabKey)pointerFocus_=false;else if(cause==focusChangedByMouseClick)pointerFocus_=true;repaint();}
private:
    float travel() const { return rotary ? 240.f : static_cast<float>(juce::jmax(1,(vertical_ ? getHeight()-42 : getWidth()-(compact?24:34)))); }
    void abort() { if(active) {active=false; value=origin; if(cancel)cancel(); repaint();} }
    bool vertical_, active{},pointerFocus_{};
    double origin{};
    float last{};
};

class InlineNameEditor final:public juce::TextEditor {
public:
    using Finish=std::function<void(InlineNameEditor&,std::optional<juce::String>)>;
    InlineNameEditor(juce::String value,Finish callback):finish(std::move(callback)){
        setName("Rename");setFont(Theme::browserFont());setText(value,false);setSelectAllWhenFocused(true);
        onReturnKey=[this]{complete(true);};onEscapeKey=[this]{complete(false);};onFocusLost=[this]{complete(true);};
    }
private:
    void complete(bool apply){if(done)return;done=true;const auto text=apply?std::optional<juce::String>{getText().trim()}:std::nullopt;
        juce::Component::SafePointer<InlineNameEditor> safe(this);auto callback=finish;
        juce::MessageManager::callAsync([safe,callback,text]{if(safe)callback(*safe,text);});}
    Finish finish;bool done{};
};
class MenuColourPalette final:public juce::PopupMenu::CustomComponent {
public:
    MenuColourPalette(Theme& theme,std::uint32_t current,std::shared_ptr<std::uint32_t> result,bool clips=false,bool modern=false):CustomComponent(false),clipPalette(clips),modernPalette(modern){
        setLookAndFeel(&theme);setName(clips?"Clip colors":"Marker color");
        const std::array<std::uint32_t,7> colors{0x4056D6,0x7050A0,0x287B89,0x487D52,0xD5B63E,0xAD5677,0x505B70};
        const std::array<juce::String,7> names{"Blue","Purple","Teal","Green","Amber","Rose","Grey"};
        const std::array<std::uint32_t,14> bright{0x467BFF,0x845CFF,0x23C9CF,0x47CE73,0xF5C542,0xEE659D,0xA6B4CC,0x2CB0FF,0xBF65F1,0x38E0B6,0xA4D94F,0xFF9845,0xF15E62,0xDF8A54};
        count=modern?(clips?ObjectPalette::clips.size():8):(clips?14:7);
        for(std::size_t i=0;i<count;++i){auto& b=buttons[i];addAndMakeVisible(b);b.setName(modern?juce::String(clips?ObjectPalette::clips[i].name:ObjectPalette::tags[i].name):i<names.size()?names[i]:"Color "+juce::String(static_cast<int>(i+1)));b.setTooltip(b.getName());
            const auto rgb=modern?(clips?ObjectPalette::clips[i].rgb:ObjectPalette::tags[i].rgb):clips?bright[i]:colors[i];const auto color=juce::Colour(0xff000000|rgb);b.setColour(juce::TextButton::buttonColourId,color);b.setColour(juce::TextButton::buttonOnColourId,color);
            b.setToggleState(current==rgb,juce::dontSendNotification);b.onClick=[this,result,rgb]{*result=rgb;triggerMenuItem();};}
    }
    ~MenuColourPalette()override{setLookAndFeel(nullptr);}
    void getIdealSize(int& w,int& h)override{w=modernPalette?332:300;h=clipPalette?(modernPalette?156:80):42;}
    void resized()override{const int columns=modernPalette?8:7;for(std::size_t i=0;i<count;++i)buttons[i].setBounds(12+40*(static_cast<int>(i)%columns),8+38*(static_cast<int>(i)/columns),28,26);}
private:bool clipPalette{},modernPalette{};std::size_t count{};std::array<juce::TextButton,32> buttons;
};

}
