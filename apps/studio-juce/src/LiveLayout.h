#pragma once
#include "Controls.h"
#include "LiveShowState.h"
#include <MRLiveAssets.h>

namespace ui {
class LiveLayout final {
  public:
    LiveLayout() {
        const auto source = resource("mr-live-layout-v5.json");
        if (juce::JSON::parse(source, layout).failed() || layout["schema"].toString() != "mr-live-layout" ||
            static_cast<int>(layout["version"]) != 1 || layout["target"].toString() != "mr-live-main" ||
            !layout["blocks"].isArray() || layout["blocks"].size() > 128)
            throw std::runtime_error("Invalid bundled MR Live layout");
        const auto raw = resource("mr-live-interface-defaults-v1.json");
        if (juce::JSON::parse(raw, defaults).failed() ||
            defaults["schema"].toString() != "mr-live-interface-defaults" || static_cast<int>(defaults["version"]) != 1)
            throw std::runtime_error("Invalid bundled MR Live defaults");
        live::validateLayout(defaults["itemCardLayout"]);
        const double vw = layout["viewport"]["width"], vh = layout["viewport"]["height"];
        if (!std::isfinite(vw) || !std::isfinite(vh) || vw < 320 || vw > 4096 || vh < 240 || vh > 4096)
            throw std::runtime_error("Invalid Live viewport");
        for (const auto &block : *layout["blocks"].getArray())
            for (const auto *axis : {"x", "y", "w", "h"})
                if (!live::number(block[axis]) || static_cast<double>(block[axis]) < 0 ||
                    static_cast<double>(block[axis]) > 4096)
                    throw std::runtime_error("Invalid Live block bounds");
    }
    juce::Rectangle<int> bounds(const juce::String &type, int width, int height) const {
        // Legacy transport/input blocks remain hidden; header owns transport.
        if (type == "input.patches" || type == "control.transport")
            return {};
        for (const auto &block : *layout["blocks"].getArray())
            if (block["type"].toString() == type && static_cast<bool>(block["visible"])) {
                const double sx = width / static_cast<double>(layout["viewport"]["width"]),
                             sy = height / static_cast<double>(layout["viewport"]["height"]);
                return {juce::roundToInt(static_cast<double>(block["x"]) * sx),
                        juce::roundToInt(static_cast<double>(block["y"]) * sy),
                        juce::roundToInt(static_cast<double>(block["w"]) * sx),
                        juce::roundToInt(static_cast<double>(block["h"]) * sy)};
            }
        return {};
    }
    juce::var cardLayout(const juce::var &state) const {
        return state["performance"]["itemCardLayout"].isObject() ? state["performance"]["itemCardLayout"]
                                                                 : defaults["itemCardLayout"];
    }
    juce::var defaults;

  private:
    static juce::String resource(const juce::String &filename) {
        for (int i = 0; i < MRLiveAssets::namedResourceListSize; ++i)
            if (filename == MRLiveAssets::originalFilenames[i]) {
                int size = 0;
                const auto *bytes = MRLiveAssets::getNamedResource(MRLiveAssets::namedResourceList[i], size);
                if (bytes)
                    return juce::String::fromUTF8(bytes, size);
            }
        throw std::runtime_error("Missing bundled Live resource");
    }
    juce::var layout;
};
} // namespace ui
