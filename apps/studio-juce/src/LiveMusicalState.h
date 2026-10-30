#pragma once
#include "LiveShowState.h"
#include "LiveTheme.h"
#include <cmath>
#include <mrs/musical.hpp>
namespace ui::live {
inline juce::var musicalVersion(const juce::var &state, const juce::String &id) {
    const auto item = find(state, "items", id);
    if (!item.isObject())
        return object();
    auto result = object();
    for (const auto *key : {"tempo", "meter", "sections"})
        put(result, key, item[key].clone());
    put(result, "chords", state["performance"]["chords"][juce::Identifier(id)].clone());
    return result;
}
struct MusicalProjection {
    mrs::TimeMap time;
    std::vector<mrs::ArrangerSection> sections;
    std::vector<mrs::Chord> chords;
};
inline MusicalProjection musicalProjection(const juce::var &source) {
    MusicalProjection result;
    if (source.getDynamicObject()->getProperties().isEmpty())
        return result;
    const auto meter = source["meter"].toString();
    if (!meter.containsChar('/'))
        throw std::runtime_error("Invalid Live meter");
    result.time.tempos = {{0, static_cast<double>(source["tempo"])}};
    result.time.meters = {{1, meter.upToFirstOccurrenceOf("/", false, false).getIntValue(),
                           meter.fromFirstOccurrenceOf("/", false, false).getIntValue()}};
    result.time.validate();
    const auto &m = result.time.meters.front();
    const auto ticksPerBar = 4 * mrs::ppq * m.numerator / m.denominator;
    const auto extent = [&](const juce::var &row) {
        const double start = static_cast<double>(row["start"]), length = static_cast<double>(row["length"]);
        if (!number(row["start"]) || !number(row["length"]) || !std::isfinite(start) || !std::isfinite(length) ||
            start < 0 || length <= 0 || start + length > 8192)
            throw std::runtime_error("Invalid Live musical span");
        return std::pair<mrs::Tick, mrs::Tick>{
            static_cast<mrs::Tick>(std::llround(start * static_cast<double>(ticksPerBar))),
            static_cast<mrs::Tick>(std::llround((start + length) * static_cast<double>(ticksPerBar)))};
    };
    if (const auto *rows = source["sections"].getArray())
        for (const auto &row : *rows) {
            const auto [start, end] = extent(row);
            result.sections.push_back({{row["id"].toString().toStdString()},
                                       row["name"].toString().toStdString(),
                                       start,
                                       end,
                                       liveBlockColour(row["color"].toString()).getARGB() & 0xffffffu});
        }
    if (const auto *rows = source["chords"].getArray())
        for (const auto &row : *rows) {
            const auto [start, end] = extent(row);
            result.chords.push_back(
                {{row["id"].toString().toStdString()}, row["symbol"].toString().toStdString(), start, end, {}});
        }
    // Browser authoring may overlap adjacent spans; Core's musical lanes are single-valued.
    // Later starts win in the runtime projection. Authored show spans stay untouched.
    const auto lane = [](auto &rows) {
        std::stable_sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.start < b.start; });
        for (std::size_t i = 0; i + 1 < rows.size(); ++i)
            rows[i].end = (std::min)(rows[i].end, rows[i + 1].start);
        std::erase_if(rows, [](const auto &row) { return row.end <= row.start; });
    };
    lane(result.sections);
    lane(result.chords);
    return result;
}
} // namespace ui::live
