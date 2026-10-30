#pragma once
#include <core/model/ChordModel.h>
#include <juce_core/juce_core.h>
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

// #305: Adapt Studio One's archived ARA-style chord fields to the accepted
// Smart Improviser host-neutral model; harmonic naming is NOT reimplemented.
// Smart Improviser snapshot provenance: core/harmony/si_snapshot/.
namespace ui::live {
inline int studioOneChordInteger(std::string_view value, int radix) {
    int output = 0;
    const auto [cursor, error] = std::from_chars(value.data(), value.data() + value.size(), output, radix);
    if (error != std::errc{} || cursor != value.data() + value.size())
        throw std::runtime_error("Malformed Studio One chord integer");
    return output;
}

inline juce::String decodeStudioOneChord(const juce::XmlElement& encoded) {
    if (!encoded.hasAttribute("root") || !encoded.hasAttribute("intervals"))
        throw std::runtime_error("Source chord root or intervals missing");
    const auto root = studioOneChordInteger(encoded.getStringAttribute("root").trim().toStdString(), 10);
    if (root < -12 || root > 12) throw std::runtime_error("Unsupported chord root spelling");
    const auto bass = encoded.hasAttribute("bass")
        ? studioOneChordInteger(encoded.getStringAttribute("bass").trim().toStdString(), 10)
        : root;
    if (bass < -12 || bass > 12) throw std::runtime_error("Unsupported chord bass spelling");
    const auto intervals = juce::StringArray::fromTokens(encoded.getStringAttribute("intervals"), false);
    if (intervals.size() != smartimproviser::harmony::kPitchClassCount)
        throw std::runtime_error("Expected 12 semitone-relative chord degree values");

    smartimproviser::harmony::ChordContext chord{};
    chord.available = true;
    chord.defined = true;
    chord.root = root;
    chord.bass = bass;
    for (int i = 0; i < intervals.size(); ++i) {
        const auto token = intervals[i].trim().toStdString();
        const auto value = studioOneChordInteger(token, 16); // A/B/D are hex degree labels
        if (value < 0 || value > 13) throw std::runtime_error("Invalid source chord degree");
        chord.intervals.values[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value);
    }
    if (chord.intervals.values[0] == 0)
        throw std::runtime_error("Source chord tonic is missing");

    using smartimproviser::harmony::ChordQuality;
    const auto normalized = smartimproviser::harmony::normalizeChord(chord);
    if (!normalized.valid || normalized.quality == ChordQuality::unknown ||
        normalized.quality == ChordQuality::undefined ||
        normalized.quality == ChordQuality::noThird)
        throw std::runtime_error("Chord quality is not recognized by Smart Improviser");
    const auto text = smartimproviser::harmony::normalizedChordSymbol(normalized);
    if (text.empty() || text == "(no chord)" || text.find('?') != std::string::npos)
        throw std::runtime_error("Chord symbol is ambiguous");
    return juce::String::fromUTF8(text.c_str());
}
} // namespace ui::live
