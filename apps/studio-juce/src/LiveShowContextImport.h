#pragma once

#include "LiveTimelineEditState.h"
#include <optional>

// Schema-independent destination half of #305. A future, *verified* Studio
// One reader fills this DTO; do not interpret arbitrary .show XML directly
// as Live model rows. Apply inside Desktop::editLiveInterface for one Undo step.
namespace ui::live {
struct ShowContextSelection {
    bool sections{};
    bool tempo{};
    bool chords{};
    bool length{};
};

struct ShowContextSong {
    // Original SetList ordinal is not replaceable by the title: source items
    // may have duplicate names. Set ONLY after the user selects a source item.
    int sourceItemOrdinal{-1};
    juce::String sourceItemTitle;
    juce::String sourceMeter;  // source bar/beat basis; never change target meter silently
    juce::String targetItemId;
    std::optional<double> bpm;
    std::optional<double> lengthBars;
    juce::var sections; // void: unavailable; array: source explicitly contains this lane
    juce::var chords; // void until verified symbol decoder produces valid chords
    juce::var rawChords; // encoded Studio One source data; never write directly to .mrlive
    int sourceSectionCount{};
    int sourceChordCount{};
    juce::StringArray warnings;
    bool hasTempoChanges{}; // current Live projection has only a constant item BPM
};

inline void applyShowContext(juce::var& show, const ShowContextSong& source,
                             ShowContextSelection selection) {
    if (!selection.sections && !selection.tempo && !selection.chords && !selection.length)
        throw std::runtime_error("Select at least one musical context field");
    if (source.sourceItemOrdinal < 0 || source.sourceItemTitle.trim().isEmpty())
        throw std::runtime_error("Select a source Studio One Show item before importing");
    if (!find(show, "items", source.targetItemId).isObject())
        throw std::runtime_error("Target MR Live item was removed");
    if ((selection.sections || selection.chords || selection.length) &&
        source.sourceMeter.isNotEmpty() &&
        find(show, "items", source.targetItemId)["meter"].toString() != source.sourceMeter)
        throw std::runtime_error("Source and target meter differ; meter import is not in this scope");
    // All validations happen on a detached clone, not on a show with live
    // channels and VST3 binary states. Failure is a zero-mutation operation.
    auto candidate = show.clone();

    if (selection.tempo) {
        if (!source.bpm || !std::isfinite(*source.bpm) || *source.bpm < 20. || *source.bpm > 400.)
            throw std::runtime_error("Source tempo is missing or outside MR Live BPM limits");
        if (source.hasTempoChanges)
            throw std::runtime_error("Variable source tempo map is not yet supported by MR Live");
        put(require(candidate, "items", source.targetItemId), "tempo", *source.bpm);
    }
    if (selection.sections) {
        if (!source.sections.isArray())
            throw std::runtime_error("Source sections are unavailable");
        setMusicalRows(candidate, source.targetItemId, MusicalLane::sections, source.sections);
    }
    if (selection.chords) {
        if (!source.chords.isArray())
            throw std::runtime_error("Source chords are unavailable");
        setMusicalRows(candidate, source.targetItemId, MusicalLane::chords, source.chords);
    }
    if (selection.length) {
        if (!source.lengthBars || !std::isfinite(*source.lengthBars) ||
            *source.lengthBars < 1. || *source.lengthBars > 4096.)
            throw std::runtime_error("Source item length is missing or outside MR Live limits");

        // A shorter end never silently hides existing or newly imported spans.
        for (const auto lane : {MusicalLane::sections, MusicalLane::chords}) {
            const auto rows = musicalRows(candidate, source.targetItemId, lane);
            if (const auto* entries = rows.getArray())
                for (const auto& row : *entries)
                    if (static_cast<double>(row["start"]) +
                            static_cast<double>(row["length"]) > *source.lengthBars + 1.e-7)
                        throw std::runtime_error("Imported length would hide a section or chord");
        }
        put(require(candidate, "items", source.targetItemId), "bars", *source.lengthBars);
        setItemView(candidate, source.targetItemId, "length", *source.lengthBars);
    }
    validate(candidate);
    show = std::move(candidate);
}
} // namespace ui::live
