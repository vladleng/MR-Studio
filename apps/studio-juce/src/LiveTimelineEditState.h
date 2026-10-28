#pragma once
#include "LiveShowState.h"
#include <algorithm>
#include <vector>

namespace ui::live {
enum class MusicalLane { sections, chords };
enum class SpanGesture { move, left, right, joined };
inline double barSnap(double value) { return std::round(value * 4.) / 4.; }
inline juce::var musicalRows(const juce::var &state, const juce::String &item, MusicalLane lane) {
    return lane == MusicalLane::sections ? find(state, "items", item)["sections"]
                                        : state["performance"]["chords"][juce::Identifier(item)];
}
inline juce::var spanById(const juce::var &rows, const juce::String &id) {
    if (const auto *values = rows.getArray()) for (const auto &row : *values)
        if (row["id"].toString() == id) return row;
    return {};
}
inline void validateMusicalRows(const juce::var &rows, MusicalLane lane) {
    if (!rows.isArray() || rows.size() > (lane == MusicalLane::sections ? 128 : 256))
        throw std::runtime_error("Musical lane limit exceeded");
    std::set<juce::String> ids;
    std::vector<juce::var> sorted;
    for (const auto &row : *rows.getArray()) {
        const auto title = row[lane == MusicalLane::sections ? "name" : "symbol"];
        const auto color = row["color"].toString();
        if (!named(row["id"]) || !ids.insert(row["id"].toString()).second || !named(title, 120) ||
            (!row["color"].isVoid() && !juce::StringArray{"graphite","blue","teal","green","amber","red","violet"}.contains(color)) ||
            !number(row["start"]) || !number(row["length"]))
            throw std::runtime_error("Invalid musical event");
        const double start = row["start"], length = row["length"];
        if (!std::isfinite(start) || !std::isfinite(length) || start < 0 || length < .25 || start + length > 4096. + 1.e-7)
            throw std::runtime_error("Musical event must stay within 0..4096 bars (minimum 1/4 bar)");
        sorted.push_back(row);
    }
    {
        std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
            return static_cast<double>(a["start"]) < static_cast<double>(b["start"]);
        });
        for (std::size_t i = 1; i < sorted.size(); ++i)
            if (static_cast<double>(sorted[i]["start"]) + 1.e-7 <
                static_cast<double>(sorted[i-1]["start"]) + static_cast<double>(sorted[i-1]["length"]))
                throw std::runtime_error("Sections and chords cannot overlap within their lane");
    }
}
inline void setMusicalRows(juce::var &state, const juce::String &item, MusicalLane lane, const juce::var &rows) {
    if (!find(state,"items",item).isObject()) throw std::runtime_error("Show item no longer exists");
    validateMusicalRows(rows, lane);
    if (lane == MusicalLane::sections) put(require(state, "items", item), "sections", rows.clone());
    else put(map(performance(state), "chords"), item, rows.clone());
}
inline juce::var editMusicalSpan(const juce::var &rows, const juce::String &id, double delta,
                                  SpanGesture mode, MusicalLane lane, bool copy = false) {
    auto result = rows.clone();
    const auto original = spanById(rows, id);
    if (!original.isObject()) throw std::runtime_error("Musical event no longer exists");
    const double start = original["start"], length = original["length"], end = start + length;
    double left = 0, right = 4096;
    if (!copy) for (const auto &row : *rows.getArray()) {
        if (row["id"].toString() == id) continue;
        const double a = row["start"], b = a + static_cast<double>(row["length"]);
        if (b <= start + 1.e-7) left = (std::max)(left, b);
        if (a >= end - 1.e-7) right = (std::min)(right, a);
    }
    if (mode == SpanGesture::joined) {
        juce::String neighbour;
        double limit = 4096.;
        for (const auto &row : *rows.getArray())
            if (row["id"].toString() != id && std::abs(static_cast<double>(row["start"]) - end) < 1.e-7) {
                neighbour = row["id"].toString();
                limit = static_cast<double>(row["start"]) + static_cast<double>(row["length"]);
                break;
            }
        if (neighbour.isEmpty()) throw std::runtime_error("Joined boundary no longer exists");
        const double boundary = juce::jlimit(start + .25, limit - .25, barSnap(end + delta));
        for (auto &row : *result.getArray()) {
            if (row["id"].toString() == id) put(row, "length", boundary - start);
            if (row["id"].toString() == neighbour) { put(row, "start", boundary); put(row, "length", limit - boundary); }
        }
        validateMusicalRows(result, lane);
        return result;
    }
    auto next = original.clone();
    if (mode == SpanGesture::move) put(next, "start", juce::jlimit(left, right - length, barSnap(start + delta)));
    if (mode == SpanGesture::left) {
        const double a = juce::jlimit(left, end - .25, barSnap(start + delta));
        put(next, "start", a); put(next, "length", end - a);
    }
    if (mode == SpanGesture::right) put(next, "length", juce::jlimit(.25, right - start, barSnap(length + delta)));
    if (copy) { put(next, "id", freshId()); result.getArray()->add(next); }
    else for (auto &row : *result.getArray()) if (row["id"].toString() == id) row = next;
    validateMusicalRows(result, lane);
    return result;
}
struct MusicalHit {
    juce::String id;
    SpanGesture mode{SpanGesture::move};
};
inline MusicalHit musicalHit(const juce::var &rows, double at, double tolerance) {
    MusicalHit edge;
    double distance = tolerance + 1.;
    if (const auto *values = rows.getArray()) for (const auto &row : *values) {
        const double start = row["start"], end = start + static_cast<double>(row["length"]);
        for (bool right : {false, true}) {
            const double d = std::abs(at - (right ? end : start));
            if (d > tolerance || d >= distance) continue;
            edge = {row["id"].toString(), right ? SpanGesture::right : SpanGesture::left};
            distance = d;
        }
    }
    if (edge.id.isNotEmpty()) {
        const auto row = spanById(rows, edge.id);
        const double boundary = static_cast<double>(row["start"]) +
            (edge.mode == SpanGesture::right ? static_cast<double>(row["length"]) : 0.);
        if (const auto *values = rows.getArray()) for (const auto &other : *values) {
            if (other["id"].toString() == edge.id) continue;
            const double otherStart = other["start"], otherEnd = otherStart + static_cast<double>(other["length"]);
            if ((edge.mode == SpanGesture::right && std::abs(otherStart - boundary) < 1.e-7) ||
                (edge.mode == SpanGesture::left && std::abs(otherEnd - boundary) < 1.e-7))
                return {edge.mode == SpanGesture::right ? edge.id : other["id"].toString(), SpanGesture::joined};
        }
        return edge;
    }
    if (const auto *values = rows.getArray()) for (const auto &row : *values)
        if (at >= static_cast<double>(row["start"]) && at < static_cast<double>(row["start"]) + static_cast<double>(row["length"]))
            return {row["id"].toString(), SpanGesture::move};
    return {};
}
inline juce::var newMusicalSpan(const juce::var &state, const juce::String &item, MusicalLane lane,
                                 double at, double end, const juce::String &name = "Verse") {
    auto rows = musicalRows(state, item, lane).clone();
    if (!rows.isArray()) rows = juce::Array<juce::var>();
    double start = juce::jlimit(0., 4095.75, barSnap(at));
    std::vector<juce::var> ordered;
    for (const auto &row : *rows.getArray()) ordered.push_back(row);
    std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
        return static_cast<double>(a["start"]) < static_cast<double>(b["start"]);
    });
    for (const auto &row : ordered) {
        const double a = row["start"], b = a + static_cast<double>(row["length"]);
        if (start >= a && start < b) start = b;
    }
    for (const auto &row : ordered) if (static_cast<double>(row["start"]) >= start)
        end = (std::min)(end, static_cast<double>(row["start"]));
    const auto meter = find(state, "items", item)["meter"].toString();
    const double beats = meter.upToFirstOccurrenceOf("/", false, false).getDoubleValue() * 4. /
                         meter.fromFirstOccurrenceOf("/", false, false).getDoubleValue();
    const double length = (std::min)(lane == MusicalLane::sections ? 8. : 4. / beats, end - start);
    if (length < .25) throw std::runtime_error("No free space for a musical event");
    auto row = object(); put(row, "id", freshId()); put(row, "start", start); put(row, "length", length);
    put(row, lane == MusicalLane::sections ? "name" : "symbol", lane == MusicalLane::sections ? name : "Am");
    put(row, "color", name == "Chorus" ? "violet" : "blue"); rows.getArray()->add(row);
    validateMusicalRows(rows, lane); return rows;
}
inline void setItemView(juce::var &state, const juce::String &item, const juce::Identifier &key, double value) {
    const bool scale = key == juce::Identifier("scale"), lead = key == juce::Identifier("lead");
    if ((!scale && !lead && key != juce::Identifier("length")) || !std::isfinite(value) ||
        value < (scale ? 4. : lead ? 0. : 1.) || value > (scale ? 128. : 4096.))
        throw std::runtime_error("Invalid item view range");
    if (!find(state,"items",item).isObject()) throw std::runtime_error("Show item no longer exists");
    auto &view = map(map(performance(state), "arrangement"), item);
    if (view["scale"].isVoid()) put(view,"scale",16.);
    if (view["lead"].isVoid()) put(view,"lead",0.);
    put(view, key, value);
}
} // namespace ui::live
