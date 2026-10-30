#pragma once
#include "../apps/studio-juce/src/LiveShowContextImport.h"

namespace ui::live::test {
inline void showContextSelection() {
    auto state = empty();
    const auto id = addItem(state, "Synthetic source target");
    insertEntry(state, id);
    // Two native items with the same display name must remain distinct.
    // The explicit destination ID, never the title, determines which is edited.
    const auto untouchedId = addItem(state, "Synthetic source target");
    insertEntry(state, untouchedId);
    const auto initial = state.clone();
    auto row = object();
    put(row, "id", "synthetic-section-id");
    put(row, "name", "Verse");
    put(row, "start", 0.);
    put(row, "length", 8.);
    put(row, "color", "blue");
    auto chord = object();
    put(chord, "id", "synthetic-chord-id");
    put(chord, "symbol", "Gmaj7");
    put(chord, "start", 0.);
    put(chord, "length", 4.);
    put(chord, "color", "blue");

    ShowContextSong source;
    source.sourceItemOrdinal = 0;
    source.sourceItemTitle = "Synthetic source (duplicate titles are legal)";
    source.targetItemId = id;
    source.bpm = 92.;
    source.lengthBars = 8.;
    source.sections = juce::Array<juce::var>{row};
    source.chords = juce::Array<juce::var>{chord};

    const auto fail = [](const char* message) { throw std::runtime_error(message); };
    applyShowContext(state, source, {.tempo = true});
    if (static_cast<double>(find(state, "items", id)["tempo"]) != 92. ||
        !sameShowValue(musicalRows(state, id, MusicalLane::sections),
                       musicalRows(initial, id, MusicalLane::sections)) ||
        !sameShowValue(musicalRows(state, id, MusicalLane::chords),
                       musicalRows(initial, id, MusicalLane::chords)) ||
        !state["performance"]["arrangement"][juce::Identifier(id)]["length"].isVoid())
        fail("Tempo-only import modified unselected context fields");

    const auto afterTempo = state.clone();
    source.sourceItemOrdinal = -1;
    try {
        applyShowContext(state, source, {.tempo = true});
        fail("Import without selected source item must fail closed");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("Select a source")) throw;
    }
    if (!sameShowValue(state, afterTempo))
        fail("Unselected source item changed MR Live state");
    source.sourceItemOrdinal = 0;

    source.hasTempoChanges = true;
    try {
        applyShowContext(state, source, {.tempo = true});
        fail("Variable tempo must fail closed");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("Variable source tempo")) throw;
    }
    if (!sameShowValue(state, afterTempo))
        fail("Rejected variable tempo changed the show");
    source.hasTempoChanges = false;

    applyShowContext(state, source, {.sections = true, .chords = true, .length = true});
    if (musicalRows(state, id, MusicalLane::sections).size() != 1 ||
        musicalRows(state, id, MusicalLane::chords).size() != 1 ||
        static_cast<double>(find(state, "items", id)["bars"]) != 8. ||
        static_cast<double>(state["performance"]["arrangement"][juce::Identifier(id)]["length"]) != 8.)
        fail("Selected sections/chords/length were not projected to MR Live model");
    if (static_cast<double>(find(state, "items", id)["tempo"]) != 92.)
        fail("Unselected tempo was changed");
    if (!sameShowValue(find(state, "items", untouchedId),
                       find(initial, "items", untouchedId)))
        fail("Import affected another item with an identical title");

    const auto afterApply = state.clone();
    source.lengthBars = 2.;
    try {
        applyShowContext(state, source, {.length = true});
        fail("Truncating existing musical spans must fail closed");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("would hide")) throw;
    }
    if (!sameShowValue(state, afterApply))
        fail("Rejected length silently modified the show");

    source.lengthBars = 8.;
    auto overlapping = row.clone();
    put(overlapping, "id", "overlap");
    put(overlapping, "start", 1.);
    source.sections = juce::Array<juce::var>{row, overlapping};
    try {
        applyShowContext(state, source, {.sections = true});
        fail("Overlapping imported sections must be rejected");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("overlap")) throw;
    }
    if (!sameShowValue(state, afterApply))
        fail("Invalid sections changed existing state");
}
} // namespace ui::live::test
