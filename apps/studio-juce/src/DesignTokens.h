#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <stdexcept>
namespace ui {
enum class ColourRole : std::size_t { canvas, window, panel, raised, deep, text, muted, border, focus, on, error, warning, count };
struct DesignTokens {
    static constexpr std::array<const char*,12> colourNames{"canvas","window","panel","raised","deep","text","muted","border","focus","on","error","warning"};
    static constexpr std::array<const char*,10> metricNames{"controlFont","compactFont","mixerFont","menuFont","fontWidth","density","radius","stroke","iconSize","gap"};
    static constexpr std::array<float,10> minimum{10,10,10,10,.8f,.8f,0,.5f,12,4},maximum{20,18,16,20,1.2f,1.4f,16,2,32,24};
    bool uds{};
    std::array<juce::uint32,12> colours{0xff242629,0xff242629,0xff303237,0xff35383c,0xff242629,0xffdce3ea,0xff9daabc,0xff45484d,0xff006dcc,0xff3098ee,0xffd94747,0xffd5b63e};
    std::array<float,10> metrics{14,13,12,14,1.04f,1,6,1,20,12};
    static DesignTokens target(){DesignTokens t;t.uds=true;t.colours={0xff1d1f22,0xff242629,0xff292b2e,0xff35383c,0xff202225,0xffdae5f4,0xff7188ab,0xff4b5f7b,0xff91bbee,0xff7ab69e,0xffe68f88,0xffe2c070};return t;}
    juce::Colour colour(ColourRole r)const{return juce::Colour(colours[static_cast<std::size_t>(r)]);}
    juce::String name()const{return uds?"MR UDS Preview":"Accepted 0.4f";}
    DesignTokens defaults()const{return uds?target():DesignTokens{};}
    void validate()const{
        for(auto c:colours)if((c>>24)!=255)throw std::invalid_argument("Colours must be opaque RGB");
        for(std::size_t i=0;i<metrics.size();++i)if(!std::isfinite(metrics[i])||metrics[i]<minimum[i]||metrics[i]>maximum[i])throw std::invalid_argument("Token metric outside allowed bounds");
    }
    bool operator==(const DesignTokens&)const=default;
    juce::String diff()const{auto base=defaults();juce::StringArray changes;for(std::size_t i=0;i<colours.size();++i)if(colours[i]!=base.colours[i])changes.add(colourNames[i]);for(std::size_t i=0;i<metrics.size();++i)if(metrics[i]!=base.metrics[i])changes.add(metricNames[i]);return changes.isEmpty()?"Profile defaults":changes.joinIntoString(", ");}
    juce::String encode()const{
        validate();auto* root=new juce::DynamicObject;juce::var value(root);root->setProperty("version",1);root->setProperty("base","accepted-0.4f");root->setProperty("profile",uds?"mr-uds":"accepted");root->setProperty("fontFamily","Segoe UI");
        auto* c=new juce::DynamicObject;root->setProperty("colours",juce::var(c));for(std::size_t i=0;i<colours.size();++i)c->setProperty(colourNames[i],"#"+juce::String::toHexString(static_cast<int>(colours[i]&0xffffff)).paddedLeft('0',6).toUpperCase());
        auto* m=new juce::DynamicObject;root->setProperty("metrics",juce::var(m));for(std::size_t i=0;i<metrics.size();++i)m->setProperty(metricNames[i],metrics[i]);return juce::JSON::toString(value,true);
    }
    static DesignTokens decode(const juce::String& text){
        if(text.length()>16384)throw std::invalid_argument("Draft exceeds 16K characters");juce::var value;if(juce::JSON::parse(text,value).failed()||!value.isObject())throw std::invalid_argument("Invalid token JSON");
        const auto* root=value.getDynamicObject();const juce::StringArray keys{"version","base","profile","fontFamily","colours","metrics"};if(root->getProperties().size()!=keys.size())throw std::invalid_argument("Unexpected draft fields");for(const auto& k:keys)if(!root->hasProperty(k))throw std::invalid_argument("Missing draft field");
        if(!value["version"].isInt()||static_cast<int>(value["version"])!=1||value["base"].toString()!="accepted-0.4f"||value["fontFamily"].toString()!="Segoe UI")throw std::invalid_argument("Unsupported draft schema/base/font");
        const auto profile=value["profile"].toString();if(profile!="accepted"&&profile!="mr-uds")throw std::invalid_argument("Unsupported profile");DesignTokens t;t.uds=profile=="mr-uds";
        const auto* c=value["colours"].getDynamicObject();const auto* m=value["metrics"].getDynamicObject();if(!c||!m||c->getProperties().size()!=12||m->getProperties().size()!=10)throw std::invalid_argument("Invalid token roles");
        for(std::size_t i=0;i<t.colours.size();++i){const auto v=c->getProperty(colourNames[i]);const auto s=v.toString();if(!v.isString()||s.length()!=7||!s.startsWithChar('#')||!s.substring(1).containsOnly("0123456789abcdefABCDEF"))throw std::invalid_argument("Expected #RRGGBB colour");t.colours[i]=0xff000000|s.substring(1).getHexValue32();}
        for(std::size_t i=0;i<t.metrics.size();++i){const auto v=m->getProperty(metricNames[i]);if(!v.isInt()&&!v.isDouble()&&!v.isInt64())throw std::invalid_argument("Expected numeric metric");t.metrics[i]=static_cast<float>(static_cast<double>(v));}t.validate();return t;
    }
};
}
