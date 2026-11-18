#pragma once
#include "DesignTokens.h"
#include <map>
#include <vector>

namespace ui {
// Approved Home-screen direction "Slate" + type scale D, decided 2026-11-18.
// Scoped to the Project Home screen only: it does NOT repaint Arrange, Mixer,
// Live or native plugin editors. Migrating the shared ui::DesignTokens to this
// palette is a separate, explicitly approved slice; do not copy these values
// into other screens.
//
// Contrast is measured against these values (WCAG 2.1, all above 4.5:1):
// text/canvas 14.9, muted/surface 6.7, muted/raised 5.8, accent/canvas 7.0,
// onAccent/accent 8.0.
struct HomeDesign {
    // Colours come from the shared "Slate" profile: one definition for every screen
    // that adopts the redesign instead of a private copy on this screen.
    static juce::Colour canvas()    { return palette().colour(ColourRole::canvas); }
    static juce::Colour surface()   { return palette().colour(ColourRole::panel); }
    static juce::Colour raised()    { return palette().colour(ColourRole::raised); }
    static juce::Colour hover()     { return DesignTokens::slateHover(); }
    static juce::Colour line()      { return palette().colour(ColourRole::border); }
    static juce::Colour lineStrong(){ return DesignTokens::slateLineStrong(); }
    // content
    static juce::Colour text()      { return palette().colour(ColourRole::text); }
    static juce::Colour muted()     { return palette().colour(ColourRole::muted); }
    static juce::Colour accent()    { return palette().colour(ColourRole::focus); }
    static juce::Colour accentDim() { return DesignTokens::slateAccentDim(); }
    static juce::Colour onAccent()  { return DesignTokens::slateOnAccent(); }
    // status (never colour-only: pair with a label)
    static juce::Colour ok()        { return palette().colour(ColourRole::on); }
    static juce::Colour warn()      { return palette().colour(ColourRole::warning); }
    static juce::Colour error()     { return palette().colour(ColourRole::error); }

    // ---- type scale D -------------------------------------------------------
    // Segoe UI Variable is a single variable font: the Display/Text/Small cuts are
    // optical-size instances of it. JUCE resolves the family but not the optical
    // instance by name, so the axes are set explicitly here. Faces without a
    // variable axis (Cascadia Mono) fall back to plain style flags.
    static juce::Font display(float pointHeight)     { return font(Role::display, pointHeight, true); }
    static juce::Font ui(float pointHeight)          { return font(Role::ui, pointHeight, false); }
    static juce::Font uiSemibold(float pointHeight)  { return font(Role::ui, pointHeight, true); }
    static juce::Font mono(float pointHeight)        { return font(Role::mono, pointHeight, false); }
    static juce::Font monoSmallCaps(float pointHeight){
        return font(Role::mono, pointHeight, true).withExtraKerningFactor(0.13f);
    }
    static juce::Font brandFont(float pointHeight){
        return font(Role::display, pointHeight, true).withExtraKerningFactor(0.10f);
    }

    // Scoped Theme preview values for this screen (ui::Theme::applyPreview).
    // Same palette roles as ui::DesignTokens, mapped to the Slate surfaces.
    static DesignTokens tokens() { return DesignTokens::slate(); }

private:
    static const DesignTokens& palette() {
        static const DesignTokens value = DesignTokens::slate();
        return value;
    }

    enum class Role { display, ui, mono };

    struct Axes {
        bool weight{}, optical{};
        juce::Range<float> weightRange{400.f, 700.f}, opticalRange{8.f, 36.f};
    };

    static const char* familyFor(Role role) {
        switch (role) {
            case Role::display: return "Segoe UI Variable Display";
            case Role::ui:      return "Segoe UI Variable Text";
            default:            return "Cascadia Mono";
        }
    }

    static Axes axesFor(Role role) {
        static std::map<int, Axes> cache;
        const auto key = static_cast<int>(role);
        if (const auto found = cache.find(key); found != cache.end())
            return found->second;
        Axes axes;
        juce::Font probe(juce::FontOptions().withName(familyFor(role)).withPointHeight(14.f));
        if (const auto typeface = probe.getTypefacePtr()) {
            for (const auto& tag : typeface->getSupportedVariables()) {
                const auto name = tag.toString();
                if (name == "wght") {
                    axes.weight = true;
                    if (const auto range = typeface->getRangeForVariable(tag)) axes.weightRange = *range;
                } else if (name == "opsz") {
                    axes.optical = true;
                    if (const auto range = typeface->getRangeForVariable(tag)) axes.opticalRange = *range;
                }
            }
        }
        cache.emplace(key, axes);
        return axes;
    }

    // The configured typeface itself has to be cloned: setting axes on a description
    // that still carries a family name is resolved back to the plain family face and the
    // axes are dropped. Cached per (role, weight, rounded size) because the optical axis
    // follows the point size.
    static juce::Typeface::Ptr typefaceFor(Role role, bool bold, float pointHeight) {
        static std::map<int, juce::Typeface::Ptr> cache;
        const auto key = static_cast<int>(role) * 100000 + (bold ? 10000 : 0) + juce::roundToInt(pointHeight);
        if (const auto found = cache.find(key); found != cache.end())
            return found->second;

        const auto base = juce::Font(juce::FontOptions().withName(familyFor(role))).getTypefacePtr();
        if (base == nullptr)
            return nullptr;
        const auto axes = axesFor(role);
        std::vector<juce::FontVariableSetting> variables;
        if (axes.optical)
            variables.push_back({juce::FontFeatureTag("opsz"), axes.opticalRange.clipValue(pointHeight)});
        if (axes.weight)
            variables.push_back({juce::FontFeatureTag("wght"), axes.weightRange.clipValue(bold ? 600.f : 400.f)});
        auto configured = variables.empty()
                        ? base
                        : base->cloneWithVariableSettings({variables.data(), variables.size()});
        cache.emplace(key, configured);
        return configured;
    }

    static juce::Font font(Role role, float pointHeight, bool bold) {
        if (const auto typeface = typefaceFor(role, bold, pointHeight)) {
            auto options = juce::FontOptions().withTypeface(typeface).withPointHeight(pointHeight);
            return juce::Font(options);
        }
        return juce::Font(juce::FontOptions().withName(familyFor(role))
                              .withStyleFlags(bold ? juce::Font::bold : juce::Font::plain)
                              .withPointHeight(pointHeight));
    }
};
} // namespace ui
