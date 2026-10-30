#pragma once
#include "LiveStudioOneShowReader.h"
#include <set>
#include <vector>

// #305: non-UI, single-transaction application of explicitly selected
// Show Page source items. The dialog will use this after preview/confirmation.
// The caller owns exactly one Desktop::editLiveInterface Undo transaction.
namespace ui::live {
struct StudioOneImportItem {
    int sourceOrdinal{-1};
    juce::String targetItemId;
    bool createNewItem{}; // the user must explicitly request creation
    ShowContextSelection fields;
};

inline void applyStudioOneImportPlan(juce::var& show, const StudioOneShowScan& scan,
                                      const std::vector<StudioOneImportItem>& requests) {
    if (requests.empty()) throw std::runtime_error("Select at least one source item");
    if (requests.size() > scan.items.size())
        throw std::runtime_error("Import selection exceeds source item count");
    auto candidate = show.clone();
    std::set<int> usedSource;
    std::set<juce::String> usedTargets;
    for (const auto& request : requests) {
        if (request.sourceOrdinal < 0 ||
            static_cast<std::size_t>(request.sourceOrdinal) >= scan.items.size() ||
            !usedSource.insert(request.sourceOrdinal).second)
            throw std::runtime_error("Invalid or duplicate source item selection");
        const auto& source = scan.items[static_cast<std::size_t>(request.sourceOrdinal)];
        if (source.sourceItemOrdinal != request.sourceOrdinal)
            throw std::runtime_error("Source item ordinals have changed");
        auto mapped = source; // never change the parsed source or its raw chords
        if (request.createNewItem) {
            if (request.targetItemId.isNotEmpty())
                throw std::runtime_error("New item creation conflicts with an existing target ID");
            // Browser owns all items; Setlist contains only explicit live
            // performance entries. Import must NOT place new items on stage.
            mapped.targetItemId = addItem(candidate, mapped.sourceItemTitle);
            // Set the new target meter to the source meter only when explicitly
            // creating an item; never change an already existing target meter.
            if (mapped.sourceMeter.isNotEmpty())
                put(require(candidate, "items", mapped.targetItemId), "meter", mapped.sourceMeter);
        } else {
            if (request.targetItemId.isEmpty())
                throw std::runtime_error("Choose an existing MR Live item explicitly");
            mapped.targetItemId = request.targetItemId;
        }
        if (!usedTargets.insert(mapped.targetItemId).second)
            throw std::runtime_error("Multiple source items cannot overwrite one destination");
        applyShowContext(candidate, mapped, request.fields);
    }
    validate(candidate);
    show = std::move(candidate); // no partial changes on failure
}
} // namespace ui::live
