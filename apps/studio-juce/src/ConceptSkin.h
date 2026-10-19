#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <MRConceptAssets.h>
#include <map>
#include <array>
#include <cmath>

namespace ui {
// UI-owned SVG cache. Construction/parsing happens at activation, never in paint
// or in the audio callback. Live controls use host values, not state-sample SVGs.
class ConceptSkin final {
    std::map<juce::String,std::unique_ptr<juce::Drawable>> art;
    std::map<juce::String,std::unique_ptr<juce::Drawable>> selectedArt;
public:
    static constexpr juce::uint32 channel=0xff2f3841, channelButton=0xff39434e, selection=0xff91bbee;
    static constexpr juce::uint32 canvas=0xff111720, panel=0xff192333,
        border=0xff4b5f7b, text=0xffdae5f4, cyan=0xff00b8ef,
        muted=0xff7188ab, red=0xfff04459, amber=0xffe2b44a, green=0xff24d67c;
    ConceptSkin(){
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        for(const auto* name:{"track_synth","track_master","bus","wave","plus",
                "duplicate","folder","folder_open","plugin_fx","list","mixer_params",
                "value_capsule_default","value_capsule_focus",
                "button_mute_off","button_mute_on","button_solo_off","button_solo_on",
                "button_record_off","button_record_on"}){
            int size{};const auto* data=MRConceptAssets::getNamedResource((juce::String(name)+"_svg").toRawUTF8(),size);
            if(!data)throw std::runtime_error("Missing concept SVG: "+std::string(name));
            auto svg=juce::parseXML(juce::String::fromUTF8(data,size));
            // Channel actions use the catalog glyphs without their sample plates.
            // Keep the viewBox, semantic paths and on-state marks unchanged.
            if(svg&&juce::String(name).startsWith("button_"))
                for(int i=svg->getNumChildElements();--i>=0;)
                    if(auto* child=svg->getChildElement(i);child->hasTagName("rect"))svg->removeChildElement(child,true);
            auto image=svg?juce::Drawable::createFromSVGString(svg->toString()):nullptr;
            if(!image)throw std::runtime_error("Invalid concept SVG: "+std::string(name));
            auto selected=image->createCopy();
            selected->replaceColour(juce::Colour(0xffa9bfdc),juce::Colour(canvas));
            selectedArt.emplace(name,std::move(selected));
            art.emplace(name,std::move(image));
        }
    }
    void draw(juce::Graphics& g,const juce::String& name,juce::Rectangle<float> r,float alpha=1.f,bool stretch=false,bool selected=false)const{
        const auto& cache=selected?selectedArt:art;
        const auto i=cache.find(name);if(i==cache.end())return;
        // Fit channel glyphs to the catalog viewBox, not the remaining path's
        // tight bounds after removing its sample plate.
        if(name.startsWith("button_")){
            i->second->draw(g,alpha,juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit({0,0,40,36},r));
            return;
        }
        i->second->drawWithin(g,r,stretch?juce::RectanglePlacement::stretchToFit:juce::RectanglePlacement::centred,alpha);
    }
    static void focus(juce::Graphics& g,juce::Rectangle<float> r){g.setColour(juce::Colour(0xff91bbee));g.drawRoundedRectangle(r.reduced(1),4,1.5f);}
    // Bounded UI-thread passes around the glyph, without a button plate.
    template<class Draw> static void glow(Draw&& draw){
        for(const auto p:std::array<juce::Point<float>,8>{{{-1,0},{1,0},{0,-1},{0,1},{-2,0},{2,0},{0,-2},{0,2}}})draw(p,.09f);
    }
    // Shared painted Micro/Slide geometry. k is the actual interactive thumb;
    // paint follows its centre so old hit testing and relative gestures match.
    static void fader(juce::Graphics& g,juce::Rectangle<float> area,juce::Rectangle<float> k,
                      bool vertical,bool enabled,bool hover,bool active,bool focused,double unity,bool bipolar=false){
        const auto alpha=enabled?1.f:.4f;
        const auto c=juce::Colour(enabled?cyan:muted).withAlpha(alpha);
        auto rail=vertical?juce::Rectangle<float>(area.getCentreX()-3,21,6,juce::jmax(1.f,area.getHeight()-42))
                          :juce::Rectangle<float>(17,area.getCentreY()-3,juce::jmax(1.f,area.getWidth()-34),6);
        g.setColour(juce::Colour(canvas));g.fillRoundedRectangle(rail,3);
        g.setColour(juce::Colour(muted).withAlpha(alpha));g.drawRoundedRectangle(rail,3,.65f);
        for(int i=0;i<=8;++i){const auto p=static_cast<float>(i)/8;
            g.setColour(juce::Colour(border));
            if(vertical){const auto y=rail.getY()+rail.getHeight()*p;g.drawLine(rail.getX()-7,y,rail.getX()-3,y,1);}
            else{const auto x=rail.getX()+rail.getWidth()*p;g.drawLine(x,rail.getBottom()+3,x,rail.getBottom()+6,1);}}
        g.setColour(c);
        if(enabled){if(vertical)g.drawLine(rail.getCentreX(),rail.getBottom(),rail.getCentreX(),k.getCentreY(),2.5f);
            else if(bipolar){g.setColour(juce::Colours::white);g.drawLine(rail.getCentreX(),rail.getCentreY(),k.getCentreX(),rail.getCentreY(),2.5f);}
            else g.drawLine(rail.getX(),rail.getCentreY(),k.getCentreX(),rail.getCentreY(),2.5f);}
        // Existing gain mapping is -60..+12 dB, therefore unity=60/72.
        g.setColour(juce::Colour(text).withAlpha(alpha));
        if(vertical){const auto y=rail.getBottom()-static_cast<float>(unity)*rail.getHeight();g.drawLine(rail.getRight()+3,y,rail.getRight()+8,y,1.5f);}
        else{const auto x=rail.getX()+static_cast<float>(unity)*rail.getWidth();g.drawLine(x,rail.getY()-5,x,rail.getY()-2,1.5f);}
        g.setColour(juce::Colour(active?0xff34465e:0xff2d394e).withAlpha(alpha));g.fillRoundedRectangle(k,3);
        g.setColour(juce::Colour(hover||active?text:0xffa9bfdc).withAlpha(alpha));g.drawRoundedRectangle(k.reduced(.5f),3,.7f);
        g.setColour(c);g.drawLine(k.getX()+4,k.getCentreY(),k.getRight()-4,k.getCentreY(),1.5f);
        if(focused)focus(g,area);
    }
    static void pan(juce::Graphics& g,juce::Rectangle<float> r,double value,bool enabled,bool focused){
        const auto a=enabled?1.f:.4f;
        g.setColour(juce::Colour(channel));g.fillEllipse(r);
        g.setColour(juce::Colour(0xffa9bfdc).withAlpha(a));g.drawEllipse(r,.75f);
        const auto angle=static_cast<float>((value-.5)*4.7);const auto p=r.getCentre();
        if(std::abs(value-.5)>.00001){juce::Path arc;const auto rad=r.getWidth()*.5f-2;
            arc.addCentredArc(p.x,p.y,rad,rad,0,juce::jmin(0.f,angle),juce::jmax(0.f,angle),true);
            g.setColour(juce::Colours::white.withAlpha(a));g.strokePath(arc,juce::PathStrokeType(2));}
        g.setColour(juce::Colour(cyan).withAlpha(a));g.drawLine(p.x+std::sin(angle)*r.getWidth()*.14f,p.y-std::cos(angle)*r.getHeight()*.14f,p.x+std::sin(angle)*r.getWidth()*.40f,p.y-std::cos(angle)*r.getHeight()*.40f,2);
        if(focused)focus(g,r.expanded(3));
    }
    static void meter(juce::Graphics& g,juce::Rectangle<int> r,float level,bool horizontal,bool enabled){
        constexpr int count=24;
        const auto lit=juce::jlimit(0,count,static_cast<int>(std::ceil(level*count)));
        for(int i=0;i<count;++i){const auto step=static_cast<float>(horizontal?r.getWidth():r.getHeight())/count;
            const auto segment=horizontal?juce::Rectangle<float>(r.getX()+i*step,static_cast<float>(r.getY()),juce::jmax(.5f,step-1),static_cast<float>(r.getHeight()))
                :juce::Rectangle<float>(static_cast<float>(r.getX()),r.getBottom()-(i+1)*step,static_cast<float>(r.getWidth()),juce::jmax(.5f,step-1));
            g.setColour(juce::Colour(i>=lit?panel:i>=22?red:i>=17?amber:i>=9?green:cyan).withAlpha(enabled?1.f:.4f));g.fillRect(segment);
        }
    }
};
}
