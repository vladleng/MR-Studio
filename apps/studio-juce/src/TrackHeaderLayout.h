#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <vector>

namespace ui {

// UI-thread-only layout contract exported by MR Design Lab (schemaVersion=1).
// Never parsed in paint/resized, and never read from the audio callback.
// A rejected document leaves the accepted legacy Track Header intact.
class TrackHeaderLayout final {
public:
    struct Element {
        juce::String id, kind, style, assetId;
        juce::Rectangle<int> bounds;
        bool visible{};
    };
    struct Profile {
        int referenceHeight{};
        std::vector<Element> elements;
        const Element* find(const juce::String& id) const noexcept {
            for (const auto& element : elements)
                if (element.id == id) return &element;
            return nullptr;
        }
    };

    static std::optional<TrackHeaderLayout> parse(const juce::String& json, juce::String& error) {
        error.clear();
        const auto root = juce::JSON::parse(json);
        if (!root.isObject() || root.getProperty("schemaVersion", {}).toString() != "1"
            || root.getProperty("component", {}).toString() != "arrangement.trackHeader"
            || root.getProperty("assetLibrary", {}).toString() != "assets/ui/mr-uds/manifest.json"
            || root.getProperty("width", {}).toString() != "244") {
            error = "Unsupported MR Design Lab Track Header JSON v1";
            return std::nullopt;
        }
        const auto profiles = root.getProperty("profiles", {});
        if (!profiles.isObject()) { error = "Missing Track Header profiles"; return std::nullopt; }

        TrackHeaderLayout result;
        constexpr std::array<const char*, 3> names{"compact", "medium", "expanded"};
        constexpr std::array<int, 3> low{38, 84, 124}, high{83, 123, 358};
        const auto integer = [](const juce::var& object, const char* key) -> std::optional<int> {
            const auto v = object.getProperty(key, {});
            if (!v.isInt() && !v.isInt64()) return std::nullopt;
            const auto n = static_cast<juce::int64>(v);
            if (n < -10000 || n > 10000) return std::nullopt;
            return static_cast<int>(n);
        };
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto data = profiles.getProperty(names[i], {});
            const auto h = integer(data, "height");
            const auto list = data.getProperty("elements", {});
            const auto* arr = list.getArray();
            if (!h || *h < low[i] || *h > high[i] || arr == nullptr || arr->size() > 100) {
                error = "Invalid profile height/elements: " + juce::String(names[i]);
                return std::nullopt;
            }
            auto& profile = result.profiles_[i];
            profile.referenceHeight = *h;
            std::set<juce::String> ids;
            const juce::String prefix = juce::String(names[i]) + "-";
            for (const auto& node : *arr) {
                if (!node.isObject()) { error = "Invalid layout element"; return std::nullopt; }
                Element el;
                el.id = node.getProperty("id", {}).toString();
                el.kind = node.getProperty("kind", {}).toString();
                el.style = node.getProperty("style", {}).toString();
                el.assetId = node.getProperty("assetId", {}).toString();
                const auto x = integer(node, "x"), y = integer(node, "y");
                const auto w = integer(node, "w"), height = integer(node, "h");
                const auto visible = node.getProperty("visible", {});
                if (el.id.length() > 100 || !el.id.startsWith(prefix) || !ids.insert(el.id).second
                    || ! (el.kind == "button" || el.kind == "label" || el.kind == "fader"
                          || el.kind == "badge" || el.kind == "meter" || el.kind == "asset")
                    || !x || !y || !w || !height || *x < 0 || *y < 0 || *w < 8 || *height < 8
                    || *x + *w > 244 || *y + *height > *h || !visible.isBool()
                    || el.assetId.length() > 150 || (el.kind == "fader" && el.style != "micro"
                        && el.style != "slide" && el.style != "precision")) {
                    error = "Invalid layout element: " + el.id;
                    return std::nullopt;
                }
                el.bounds = {*x, *y, *w, *height};
                el.visible = static_cast<bool>(visible);
                profile.elements.push_back(std::move(el));
            }
            // Binding contract: never silently drop core host controls or repaint
            // a decorative asset as a functional JUCE component.
            const std::array<std::pair<const char*, const char*>, 7> required{{
                {"icon", "asset"}, {"name", "label"}, {"mute", "button"},
                {"solo", "button"}, {"record", "button"}, {"monitor", "button"},
                {"gain", "fader"}
            }};
            for (const auto& [role, kind] : required) {
                const auto* found = profile.find(prefix + role);
                if (!found || found->kind != kind) {
                    error = "Missing semantic control: " + prefix + role;
                    return std::nullopt;
                }
            }
            if (i > 0) {
                for (const auto& role : {"pan", "meter", "input"}) {
                    if (!profile.find(prefix + role)) {
                        error = "Missing Track Header control: " + prefix + role;
                        return std::nullopt;
                    }
                }
            }
            if (i == 2 && !profile.find(prefix + "output")) {
                error = "Missing expanded output routing";
                return std::nullopt;
            }
        }
        return result;
    }

    static std::optional<TrackHeaderLayout> readFile(const juce::File& file, juce::String& error) {
        if (!file.existsAsFile() || file.getSize() <= 0 || file.getSize() > 1024 * 1024) {
            error = "Missing or oversized Track Header JSON: " + file.getFullPathName();
            return std::nullopt;
        }
        return parse(file.loadFileAsString(), error);
    }

    static int profileIndex(int height) noexcept { return height < 84 ? 0 : height < 124 ? 1 : 2; }
    const Profile& profile(int height) const noexcept { return profiles_[static_cast<std::size_t>(profileIndex(height))]; }
    const Element* find(int height, const char* role) const noexcept {
        constexpr std::array<const char*, 3> prefixes{"compact-", "medium-", "expanded-"};
        const auto index = static_cast<std::size_t>(profileIndex(height));
        return profiles_[index].find(juce::String(prefixes[index]) + role);
    }

    juce::Rectangle<int> rectangle(int height, int width, const Element& element, const char* role) const noexcept {
        const auto& reference = profile(height);
        auto x = element.bounds.getX();
        auto w = element.bounds.getWidth();
        auto y = element.bounds.getY();
        auto h = element.bounds.getHeight();
        const int difference = width - 244;
        const juce::String name(role);
        if (name == "mute" || name == "solo" || name == "record"
            || name == "monitor" || name == "pan") x += difference;
        else if (name == "gain" || name == "meter" || name == "input" || name == "output") w += difference;

        // Keep the first row at native hit-target size. Fit the lower controls to
        // the current Strip height without overlapping its header. Exact at the
        // saved 60/110/166 heights, responsive between profile boundaries.
        if (y >= 25 && reference.referenceHeight > 24) {
            const float scale = static_cast<float>(juce::jmax(1, height - 24))
                              / static_cast<float>(reference.referenceHeight - 24);
            y = 24 + juce::roundToInt(static_cast<float>(y - 24) * scale);
            h = juce::jmax(8, juce::roundToInt(static_cast<float>(h) * scale));
        }
        x = juce::jlimit(0, juce::jmax(0, width - 8), x);
        y = juce::jlimit(0, juce::jmax(0, height - 8), y);
        w = juce::jlimit(8, juce::jmax(8, width - x), w);
        h = juce::jlimit(8, juce::jmax(8, height - y), h);
        return {x, y, w, h};
    }

private:
    std::array<Profile, 3> profiles_{};
};
} // namespace ui
