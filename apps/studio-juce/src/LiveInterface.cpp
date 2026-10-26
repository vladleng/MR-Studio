#include "Desktop.h"
#include "LiveLayout.h"
#include "LiveMusicalState.h"

namespace ui {
namespace {
void validateMusicalEdit(Desktop &owner, const juce::var &copy) {
    if (owner.liveDocument->projects.empty()) {
        const auto prior = owner.liveDocument->interfaceState["currentItem"].toString();
        const auto id = live::find(copy, "items", prior).isObject() ? prior : juce::String();
        const auto music = live::musicalVersion(copy, id);
        if (live::canonical(music) !=
            live::canonical(live::musicalVersion(owner.liveDocument->interfaceState,
                                                 owner.liveDocument->interfaceState["currentItem"].toString()))) {
            if (owner.app.recording() || owner.app.engine()->state().playback != mrs::PlaybackState::stopped ||
                owner.app.engine()->state().loop)
                throw std::runtime_error("Stop Playback and turn off section loop before changing musical data");
            const auto projection = live::musicalProjection(music);
            auto candidate = *owner.project();
            mrs::SetMusicalData{projection.time, projection.chords, projection.sections, {}}.apply(candidate);
            candidate.validate();
        }
    }
}
// Preflight before committing show history, including Undo/Redo. Core owns the source extent.
void validatePlaybackEdit(Desktop &owner, const juce::var &copy) {
    live::validatePlayback(copy);
    if (!owner.livePlaybackReady())
        return;
    const auto id = owner.liveDocument->interfaceState["currentItem"].toString();
    const auto before = live::playback(owner.liveDocument->interfaceState, id);
    const auto after = live::playback(copy, id);
    if (before["file"] != after["file"] ||
        (before["trimStart"] == after["trimStart"] && before["trimEnd"] == after["trimEnd"]))
        return;
    if (owner.app.recording() || owner.app.engine()->state().playback == mrs::PlaybackState::playing)
        throw std::runtime_error("Pause or stop Playback before trimming audio");
    const auto project = owner.project();
    const auto frames = owner.app.source_frames(project->clips.front().id);
    const auto start =
        static_cast<mrs::Sample>(std::llround(static_cast<double>(after["trimStart"]) * project->sample_rate));
    const auto end =
        after["trimEnd"].isVoid()
            ? frames
            : static_cast<mrs::Sample>(std::llround(static_cast<double>(after["trimEnd"]) * project->sample_rate));
    if (start < 0 || end <= start || end > frames)
        throw std::runtime_error("Trim must stay inside the original WAV source");
}
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
void Desktop::launchLive(const juce::File &file, bool create) {
    if (liveLaunchOverride) {
        liveLaunchOverride(file, create);
        return;
    }
    if (standaloneLive)
        throw std::runtime_error("MR Live is already running");
    if (app.recording())
        throw std::runtime_error("Stop recording before launching MR Live");
    const auto executable =
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("MR Live.exe");
    if (!executable.existsAsFile())
        throw std::runtime_error("MR Live.exe is missing from the application package");
    const bool session = app.audio_name() != "Offline clock (no sound)" && app.audio_name() != "Disconnected";
    app.pause();
    app.disconnect();
    if (!executable.startAsProcess((create ? "--new-show " : "--open-show ") + file.getFullPathName().quoted())) {
        reconnectDevice(session);
        throw std::runtime_error("Could not launch MR Live");
    }
    refresh();
}
void Desktop::prepareLiveInterface(bool saved) {
    livePlaybackItem.clear();
    livePlaybackFile.clear();
    livePlaybackError.clear();
    livePlaybackMixKey.clear();
    liveMusicalKey.clear();
    livePlaybackLoading = false;
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
    auto bindings = copy["performance"]["nativePlayback"];
    if (bindings.isObject()) {
        juce::StringArray removed;
        for (const auto &pair : bindings.getDynamicObject()->getProperties())
            if (!live::find(copy, "items", pair.name.toString()).isObject())
                removed.add(pair.name.toString());
        for (const auto &key : removed)
            bindings.getDynamicObject()->removeProperty(key);
    }
    live::validatePlayback(copy);
    validateMusicalEdit(*this, copy);
    validatePlaybackEdit(*this, copy);
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
    validateMusicalEdit(*this, draft);
    validatePlaybackEdit(*this, draft);
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
    if (liveDocument->projects.empty() && liveDocument->interfaceState["currentItem"].toString() == id) {
        ++liveTimelineFocusRevision;
        syncLivePlayback();
        refresh();
        return;
    }
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
    ++liveTimelineFocusRevision;
    livePlaybackItem.clear();
    syncLivePlayback();
    liveShowEditing = true;
    updateLiveDirty();
    ++liveUiRevision;
    refresh();
}
void Desktop::bindLiveItem(const juce::String &id, const juce::File &file) {
    if (standaloneLive)
        throw std::runtime_error("Studio project loading is not a MR Live workflow");
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

namespace ui {
void Desktop::attachLivePlayback(const juce::String &id, const juce::File &file) {
    if (!liveDocument || !live::find(liveDocument->interfaceState, "items", id).isObject())
        throw std::runtime_error("Select a Live item first");
    if (app.recording())
        throw std::runtime_error("Stop recording before changing Playback");
    if (!file.existsAsFile() || !file.hasFileExtension("wav"))
        throw std::runtime_error("Select an existing WAV file");
    editLiveInterface("Attach Playback WAV", [&](auto &state) {
        auto row = live::playback(state, id).clone();
        if (!row.isObject()) {
            row = live::object();
            live::put(row, "gain", 0.);
            live::put(row, "mute", false);
            live::put(row, "solo", false);
        }
        // A replacement source starts with its complete extent, retaining the item mix.
        row.getDynamicObject()->removeProperty("trimStart");
        row.getDynamicObject()->removeProperty("trimEnd");
        live::put(row, "file", file.getFullPathName());
        live::put(live::map(live::performance(state), "nativePlayback"), id, row);
    });
    if (liveDocument->interfaceState["currentItem"].toString() == id)
        livePlaybackItem.clear();
    syncLivePlayback();
}
void Desktop::setLivePlaybackMix(double gain, bool mute, bool solo) {
    if (!liveDocument || !livePlaybackReady())
        throw std::runtime_error("Playback is not ready");
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    editLiveInterface("Playback mix", [&](auto &state) {
        auto &row = live::map(live::map(live::performance(state), "nativePlayback"), id);
        live::put(row, "gain", gain);
        live::put(row, "mute", mute);
        live::put(row, "solo", solo);
    });
    syncLivePlayback();
}
void Desktop::setLivePlaybackTrim(double start, double end) {
    if (!livePlaybackReady())
        throw std::runtime_error("Playback is not ready");
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0. || end <= start)
        throw std::runtime_error("Invalid Playback trim range");
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    editLiveInterface("Trim Playback", [&](auto &state) {
        auto &row = live::map(live::map(live::performance(state), "nativePlayback"), id);
        live::put(row, "trimStart", start);
        live::put(row, "trimEnd", end);
    });
    syncLivePlayback();
}
bool Desktop::livePlaybackReady() const {
    if (!liveDocument || livePlaybackLoading || livePlaybackError.isNotEmpty() || app.wav_import_status().busy)
        return false;
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    return id == livePlaybackItem && livePlaybackFile.isNotEmpty() &&
           live::playback(liveDocument->interfaceState, id)["file"].toString() == livePlaybackFile &&
           project()->tracks.size() == 1 && !project()->clips.empty();
}
void Desktop::syncLivePlayback() {
    if (!liveDocument || !liveDocument->projects.empty())
        return; // Legacy compatibility stays separate.
    bool changed = false;
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    const auto row = live::playback(liveDocument->interfaceState, id);
    const auto file = row["file"].toString();
    const auto music = live::musicalVersion(liveDocument->interfaceState, id);
    const auto musicKey = live::canonical(music);
    const bool mediaChanged = livePlaybackItem != id || livePlaybackFile != file;
    const bool musicChanged = musicKey != liveMusicalKey;
    if (mediaChanged || musicChanged) {
        if (app.recording())
            throw std::runtime_error("Stop recording before changing Playback");
        const bool reload = mediaChanged && (file.isNotEmpty() || livePlaybackFile.isNotEmpty());
        const bool restartImport = livePlaybackLoading;
        if (restartImport)
            app.cancel_wav_import();
        livePlaybackLoading = false;
        livePlaybackError.clear();
        livePlaybackItem = id;
        livePlaybackFile = file;
        livePlaybackMixKey.clear();
        changed = true;
        try {
            if (reload) {
                const bool session =
                    app.audio_name() != "Offline clock (no sound)" && app.audio_name() != "Disconnected";
                closeEditors();
                cancelPreview();
                app.new_project(session ? prefs.rate : 48000);
                selectedTrack.reset();
                selectedClip.reset();
                const auto projection = live::musicalProjection(music);
                app.set_musical_data(projection.time, projection.chords, projection.sections);
                resetDevice();
                reconnectDevice(session);
            } else if (musicChanged) {
                const auto projection = live::musicalProjection(music);
                app.set_musical_data(projection.time, projection.chords, projection.sections);
            }
            if (file.isNotEmpty() && (mediaChanged || restartImport || project()->clips.empty())) {
                if (!juce::File(file).existsAsFile())
                    livePlaybackError = "Playback file is missing: " + file;
                else {
                    app.queue_wav_import({std::filesystem::path(file.toWideCharPointer())});
                    livePlaybackLoading = true;
                }
            }
        } catch (const std::exception &error) {
            livePlaybackError = juce::String::fromUTF8(error.what());
        }
        liveMusicalKey = musicKey;
    }
    if (livePlaybackLoading && !app.wav_import_status().busy) {
        changed = true;
        livePlaybackLoading = false;
        if (project()->tracks.size() != 1 || project()->clips.empty())
            livePlaybackError = juce::String::fromUTF8(app.wav_import_status().message.c_str());
    }
    if (livePlaybackReady()) {
        const auto key = live::canonical(row);
        if (key != livePlaybackMixKey) {
            const auto snapshot = project();
            const auto clip = snapshot->clips.front();
            const auto frames = app.source_frames(clip.id);
            const auto start =
                static_cast<mrs::Sample>(std::llround(static_cast<double>(row["trimStart"]) * snapshot->sample_rate));
            const auto end = row["trimEnd"].isVoid()
                                 ? frames
                                 : static_cast<mrs::Sample>(
                                       std::llround(static_cast<double>(row["trimEnd"]) * snapshot->sample_rate));
            if (start < 0 || end <= start || end > frames) {
                livePlaybackError = "Saved trim exceeds the original WAV source; attach the correct WAV again";
                app.stop();
                app.disconnect(); // Never expose an untrimmed replacement as ready audio.
                liveEmptyRevision =
                    std::pair{app.services().projects->state().revision, app.graphs()->state().revision};
                return;
            }
            if (clip.start != start || clip.length != end - start)
                app.trim_clip(clip.id, start, end);
            auto mix = project()->tracks.front().mix;
            mix.gain = static_cast<float>(std::pow(10., static_cast<double>(row["gain"]) / 20.));
            mix.mute = static_cast<bool>(row["mute"]);
            mix.solo = static_cast<bool>(row["solo"]);
            app.set_track_mix(project()->tracks.front().id, mix);
            livePlaybackMixKey = key;
            changed = true;
        }
    }
    if (changed)
        liveEmptyRevision = std::pair{app.services().projects->state().revision, app.graphs()->state().revision};
}
} // namespace ui
