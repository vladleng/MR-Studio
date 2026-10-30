#pragma once
#include "../apps/studio-juce/src/LiveStudioOneImportPlan.h"
#include "../apps/studio-juce/src/Settings.h"

namespace ui::live::test {
inline void studioOneImportPlan() {
    const auto fail = [](const char* reason) { throw std::runtime_error(reason); };
    // .show selection is a USER preference, not project/setlist state.
    ViewSettings preferences;
    preferences.lastLiveShow = "previous.mrlive";
    preferences.lastStudioOneShow = "C:/Shows/Selected.show";
    const auto persisted = ViewSettings::decode(preferences.encode());
    if (persisted.lastStudioOneShow != preferences.lastStudioOneShow ||
        persisted.lastLiveShow != "previous.mrlive")
        fail("Last .show user preference must round-trip independently of .mrlive");
    const auto legacy = juce::JSON::parse(preferences.encode());
    auto oldSettings = legacy.clone();
    oldSettings.getDynamicObject()->removeProperty("lastStudioOneShow");
    if (ViewSettings::decode(juce::JSON::toString(oldSettings)).lastStudioOneShow.isNotEmpty())
        fail("Older settings files must default to no remembered .show");
    StudioOneShowScan scan;
    for (int i = 0; i < 2; ++i) {
        ShowContextSong song;
        song.sourceItemOrdinal = i;
        song.sourceItemTitle = "Same source title";
        song.sourceMeter = "4/4";
        song.bpm = (i == 0 ? 120. : 88.);
        song.lengthBars = (i == 0 ? 8. : 12.);
        scan.items.push_back(std::move(song));
    }
    auto show = empty();
    const auto target1 = addItem(show,"One");
    const auto target2 = addItem(show,"Two");
    insertEntry(show,target1);
    insertEntry(show,target2);

    const auto old = show.clone();
    const std::vector<StudioOneImportItem> invalid{
        {0,target1,false,{.tempo=true}},
        {1,target1,false,{.tempo=true}}
    };
    try {
        applyStudioOneImportPlan(show,scan,invalid);
        fail("Destination collision in batch must fail");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("destination")) throw;
    }
    if (!sameShowValue(show,old)) fail("Failed batch import mutated original show");

    const std::vector<StudioOneImportItem> selected{
        {0,target1,false,{.tempo=true}},
        {1,target2,false,{.tempo=true}}
    };
    applyStudioOneImportPlan(show,scan,selected);
    if (static_cast<double>(find(show,"items",target1)["tempo"]) != 120. ||
        static_cast<double>(find(show,"items",target2)["tempo"]) != 88.)
        fail("Mapped source ordinal must determine destination tempo");
    const auto updated = show.clone();
    try {
        applyStudioOneImportPlan(show,scan,{{0,"missing",false,{.tempo=true}}});
        fail("Nonexistent destination should fail closed");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("removed")) throw;
    }
    if (!sameShowValue(show,updated)) fail("Invalid target changed imported state");

    const auto previousSetlist = activeEntries(show).clone();
    applyStudioOneImportPlan(show,scan,{{1,{},true,{.tempo=true,.length=true}}});
    if (show["items"].size() != updated["items"].size() + 1)
        fail("Explicit create-new must add precisely one Browser item");
    if (!sameShowValue(activeEntries(show), previousSetlist))
        fail("Studio One import unexpectedly added a new item to Setlist");
    const auto* all = show["items"].getArray();
    if (!all || all->getLast()["name"].toString() != "Same source title" ||
        all->getLast()["meter"].toString() != "4/4" ||
        static_cast<double>(all->getLast()["tempo"]) != 88.)
        fail("Explicit new item must preserve selected source metadata");
    const auto createdId = all->getLast()["id"].toString();
    const auto savedBrowserItem = libraryItem(show, createdId);
    if (!savedBrowserItem.isObject() ||
        savedBrowserItem["name"].toString() != "Same source title" ||
        static_cast<double>(savedBrowserItem["tempo"]) != 88. ||
        static_cast<double>(savedBrowserItem["bars"]) != 12.)
        fail("Browser must display the imported snapshot, not default item values");
    if (activeEntries(show).size() != previousSetlist.size())
        fail("New Browser item must remain outside the Setlist");
    // An existing mapped item may already be on Setlist; preserve it, too.
    const auto previousEntries = activeEntries(show).clone();
    applyStudioOneImportPlan(show,scan,{{0,target1,false,{.tempo=true}}});
    if (!sameShowValue(activeEntries(show), previousEntries))
        fail("Import into existing item must preserve Setlist membership");
    const auto beforeDuplicate = show.clone();
    try {
        applyStudioOneImportPlan(show,scan,{
            {0,target1,false,{.tempo=true}},
            {0,target2,false,{.tempo=true}}
        });
        fail("Duplicate source ordinal must fail closed");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("source")) throw;
    }
    if (!sameShowValue(show,beforeDuplicate))
        fail("Repeated source item import must remain atomic");
}
} // namespace ui::live::test
