#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <juce_graphics/juce_graphics.h>

namespace ui {
// Same RGB values as the concept catalog's track_tag_* assets. Stored custom
// project colors remain exact RGB; only new choices/default rendering use these.
struct ObjectPalette {
    struct Swatch { const char* name; std::uint32_t rgb; };
    static constexpr std::array<Swatch,8> tags{{
        {"Cyan",0x00b8ef},{"Pale Blue",0x91bbee},{"Purple",0xb45df0},{"Rose",0xf04459},
        {"Amber",0xe2b44a},{"Yellow",0xf0d447},{"Green",0x24d67c},{"Graphite",0x192333}}};
    static constexpr std::array<Swatch,32> clips{{
        tags[0],tags[1],tags[2],tags[3],tags[4],tags[5],tags[6],tags[7],
        {"Azure",0x3797ef},{"Indigo",0x777de8},{"Lavender",0xc58ee9},{"Pink",0xec83b8},
        {"Orange",0xef934d},{"Lemon",0xd7e46a},{"Lime",0x96d75e},{"Mint",0x77d6af},
        {"Teal",0x36b9b7},{"Ice",0x8fd5df},{"Periwinkle",0xa3b0ec},{"Mauve",0xc578b4},
        {"Coral",0xef8274},{"Sand",0xdac290},{"Olive",0x9cb877},{"Slate",0x8295ad},
        {"Deep Blue",0x385aa8},{"Deep Violet",0x6a4694},{"Plum",0x954d7d},{"Brick",0xaf594d},
        {"Ochre",0xaf8745},{"Forest",0x47885f},{"Ocean",0x397f91},{"Silver",0xb8c4d1}}};
    static constexpr std::uint32_t defaultClip=0x3797ef, defaultMarker=0x00b8ef,
        defaultSection=0x91bbee, warning=0xe2b44a;
    static juce::Colour ink(std::uint32_t rgb,float alpha,juce::uint32 background){
        const auto fill=juce::Colour(background).overlaidWith(juce::Colour(0xff000000|rgb).withAlpha(alpha));
        const auto luminance=[](juce::Colour c){const auto linear=[](float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};
            return .2126f*linear(c.getFloatRed())+.7152f*linear(c.getFloatGreen())+.0722f*linear(c.getFloatBlue());};
        const juce::Colour light(0xffdce3ea),dark(0xff111720);const auto l=luminance(fill);
        return (luminance(light)+.05f)/(l+.05f)>(l+.05f)/(luminance(dark)+.05f)?light:dark;
    }
};
}
