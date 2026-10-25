#include "Desktop.h"
#include "LiveLayout.h"

namespace ui {
namespace {
class LiveInterfaceEdit final : public juce::UndoableAction {
  public:
    LiveInterfaceEdit(Desktop &desktop, const juce::var &oldState, const juce::var &newState)
        : owner(&desktop), before(oldState.clone()), after(newState.clone()) {}
    bool perform() override {
        if (!owner || !owner->liveDocument)
            return false;
        owner->applyLiveInterface(after);
        return true;
    }
    bool undo() override {
        if (!owner || !owner->liveDocument)
            return false;
        owner->applyLiveInterface(before);
        return true;
    }
    int getSizeInUnits() override {
        return static_cast<int>(juce::JSON::toString(before).getNumBytesAsUTF8() +
                                juce::JSON::toString(after).getNumBytesAsUTF8());
    }

  private:
    juce::Component::SafePointer<Desktop> owner;
    juce::var before, after;
};
} // namespace
void Desktop::prepareLiveInterface(bool saved) {
    liveUndo.clearUndoHistory();
    liveShowEditing = false;
    if (liveDocument) {
        const LiveLayout reference;
        liveDocument->ensureInterface(reference.defaults);
        auto &perf = live::performance(liveDocument->interfaceState);
        if (!perf["itemCardLayout"].isObject())
            live::put(perf, "itemCardLayout", reference.defaults["itemCardLayout"].clone());
        if (!perf["showOrganizer"].isBool())
            live::put(perf, "showOrganizer", reference.defaults["showOrganizer"]);
        liveDocument->validate();
    }
    liveSavedContents = saved && liveDocument ? liveDocument->signature() : juce::String();
    updateLiveDirty();
    ++liveUiRevision;
}
void Desktop::updateLiveDirty() {
    liveDirty = liveDocument && (liveSavedContents.isEmpty() || liveDocument->signature() != liveSavedContents);
}
void Desktop::applyLiveInterface(const juce::var &value) {
    if (!liveDocument)
        throw std::runtime_error("Open a MR Live show first");
    auto copy = value.clone();
    const auto current = liveDocument->interfaceState["currentItem"],
               set = liveDocument->interfaceState["activeSetlist"];
    live::put(copy, "currentItem", live::find(copy, "items", current.toString()).isObject() ? current : juce::var());
    if (live::find(copy, "setlists", set.toString()).isObject())
        live::put(copy, "activeSetlist", set);
    live::validate(copy);
    auto transaction = liveDocument->clone();
    transaction.interfaceState = copy;
    transaction.validate();
    liveDocument->interfaceState = copy;
    updateLiveDirty();
    ++liveUiRevision;
}
void Desktop::editLiveInterface(const juce::String &label, std::function<void(juce::var &)> edit) {
    if (!liveDocument)
        throw std::runtime_error("Create or open a MR Live show first");
    auto draft = liveDocument->interfaceState.clone();
    edit(draft);
    live::validate(draft);
    if (live::canonical(draft) == live::canonical(liveDocument->interfaceState))
        return;
    liveShowEditing = true;
    liveUndo.beginNewTransaction(label);
    liveUndo.perform(new LiveInterfaceEdit(*this, liveDocument->interfaceState, draft));
    liveUndo.beginNewTransaction();
}
void Desktop::undoLiveInterface(bool redoAction) {
    liveShowEditing = true;
    if (redoAction)
        liveUndo.redo();
    else
        liveUndo.undo();
    refresh();
}
void Desktop::importLiveShow(const juce::File &file) {
    if (!file.existsAsFile() || file.getSize() > 1024 * 1024)
        throw std::runtime_error("Missing or oversized editor show JSON");
    LiveDocument imported;
    imported.interfaceState = live::importDemo(file.loadFileAsString());
    imported.title = live::activeSetlist(imported.interfaceState)["name"].toString();
    imported.validate();
    const bool session = app.audio_name() != "Offline clock (no sound)" && app.audio_name() != "Disconnected";
    closeEditors();
    cancelPreview();
    app.new_project(session ? prefs.rate : 48000);
    resetDevice();
    liveDocument = std::move(imported);
    liveFile = juce::File();
    app.workspace(mrs::desktop::Workspace::live);
    prepareLiveInterface(false);
    selectedTrack.reset();
    selectedClip.reset();
    refresh(true);
    showProjectHome(false);
    reconnectDevice(session);
    liveEmptyRevision = std::pair{app.services().projects->state().revision, app.graphs()->state().revision};
}
void Desktop::selectLiveItem(const juce::String &id) {
    if (!liveDocument || !live::find(liveDocument->interfaceState, "items", id).isObject())
        throw std::runtime_error("Missing show item");
    const int index = liveDocument->projectFor(id);
    if (index >= 0) {
        selectLiveProject(index);
        live::put(liveDocument->interfaceState, "currentItem", id);
    } else {
        if (app.recording())
            throw std::runtime_error("Stop recording before changing items");
        const bool session = app.audio_name() != "Offline clock (no sound)" && app.audio_name() != "Disconnected";
        closeEditors();
        cancelPreview();
        app.new_project(session ? prefs.rate : 48000);
        resetDevice();
        liveDocument->active = -1;
        live::put(liveDocument->interfaceState, "currentItem", id);
        selectedTrack.reset();
        selectedClip.reset();
        refresh(true);
        reconnectDevice(session);
        liveEmptyRevision = std::pair{app.services().projects->state().revision, app.graphs()->state().revision};
    }
    liveShowEditing = true;
    updateLiveDirty();
    ++liveUiRevision;
    refresh();
}
void Desktop::bindLiveItem(const juce::String &id, const juce::File &file) {
    if (!liveDocument || !live::find(liveDocument->interfaceState, "items", id).isObject() ||
        !file.hasFileExtension("mrsproject"))
        throw std::runtime_error("Select a Studio project for this item");
    if (liveDocument->projects.size() >= 1024 &&
        std::find(liveDocument->projects.begin(), liveDocument->projects.end(), file) == liveDocument->projects.end())
        throw std::runtime_error("Too many project references");
    const bool session = app.audio_name() != "Offline clock (no sound)" && app.audio_name() != "Disconnected";
    closeEditors();
    cancelPreview();
    app.open_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));
    resetDevice();
    const auto found = std::find(liveDocument->projects.begin(), liveDocument->projects.end(), file);
    int index = static_cast<int>(found - liveDocument->projects.begin());
    if (found == liveDocument->projects.end()) {
        liveDocument->projects.push_back(file);
        index = static_cast<int>(liveDocument->projects.size()) - 1;
    }
    liveDocument->itemProjects[id] = index;
    liveDocument->active = index;
    live::put(liveDocument->interfaceState, "currentItem", id);
    liveUndo.clearUndoHistory();
    liveShowEditing = true;
    updateLiveDirty();
    ++liveUiRevision;
    selectedTrack.reset();
    selectedClip.reset();
    liveEmptyRevision.reset();
    refresh(true);
    reconnectDevice(session);
}
} // namespace ui
