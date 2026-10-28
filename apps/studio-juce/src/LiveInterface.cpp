#include "Desktop.h"
#include "LiveLayout.h"
#include "LiveMusicalState.h"
#include "LiveClipProjection.h"

namespace ui {
namespace {
mrs::desktop::Application::PlaybackLayout performanceLayout(const juce::var& state,const juce::String& id,std::uint32_t rate,std::uint64_t token){
    const auto projection=live::musicalProjection(live::musicalVersion(state,id));
    mrs::desktop::Application::PlaybackLayout layout;
    layout.time=projection.time;layout.chords=projection.chords;layout.sections=projection.sections;
    layout.stems=live::playbackStemLayouts(state,id,rate);layout.click=live::liveClick(state);
    layout.extend_to_audio=state["performance"]["arrangement"][juce::Identifier(id)]["length"].isVoid();
    // Preserve all authored sections until Core knows the actual audio extent.
    double preparingAudio=0.;
    if(layout.extend_to_audio&&!live::playbackRows(state,id).empty()){
        preparingAudio=live::performanceEndBars(state,id,0.);
        const auto& meter=projection.time.meters.front();
        const double ticksPerBar=4.*mrs::ppq*meter.numerator/meter.denominator;
        for(const auto& section:projection.sections)preparingAudio=(std::max)(preparingAudio,static_cast<double>(section.end)/ticksPerBar);
    }
    layout.performance=live::preparePerformancePlan(state,id,projection.time,projection.sections,rate,preparingAudio);
    layout.token=token;return layout;
}
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
    if(owner.liveAutoToken){
        const auto target=owner.liveAutoItem;
        const auto graphKey=[&](const juce::var& value){return live::playbackMediaKey(value,target)+live::playbackMixKey(value,target)+
            live::playbackRegionsKey(value,target)+live::canonical(live::musicalVersion(value,target))+live::performanceKey(value,target)+
            live::canonical(live::liveAudio(value));};
        const auto nextEntry=live::nextPerformanceEntry(copy,owner.liveCurrentEntry)["id"].toString();
        if(graphKey(copy)!=graphKey(owner.liveDocument->interfaceState)||nextEntry!=owner.liveAutoEntry){
            // Withdraw before committing show edits, so an already prepared
            // successor can never become audible with superseded bindings.
            if(!owner.app.cancel_staged_playback()||owner.app.playback_token()==owner.liveAutoToken)
                throw std::runtime_error("Item transition in progress; retry the edit after it completes");
            owner.liveAutoKey.clear();owner.liveAutoToken=0;
        }
    }
    const auto current=owner.liveDocument->interfaceState["currentItem"].toString();
    if(live::performanceKey(copy,current)!=live::performanceKey(owner.liveDocument->interfaceState,current)&&
       (owner.app.recording()||owner.app.engine()->state().playback==mrs::PlaybackState::playing))
        throw std::runtime_error("Pause or stop before changing active performance modes");
    const auto oldClick = live::liveClick(owner.liveDocument->interfaceState), newClick = live::liveClick(copy);
    if (newClick != oldClick) {
        if (owner.app.recording())
            throw std::runtime_error("Stop recording before changing Click");
        if (newClick.output_left != oldClick.output_left || newClick.output_right != oldClick.output_right) {
            if (owner.app.engine()->state().playback == mrs::PlaybackState::playing)
                throw std::runtime_error("Pause or stop before routing Click");
            const auto enabled = owner.app.active_outputs();
            for (int output : {newClick.output_left, newClick.output_right})
                if (output >= 0 && std::find(enabled.begin(), enabled.end(), output) == enabled.end())
                    throw std::runtime_error("Enable the Click output in Audio settings first");
        }
    }
    if (!owner.livePlaybackReady())
        return;
    const auto id = owner.liveDocument->interfaceState["currentItem"].toString();
    const auto before = live::playbackRows(owner.liveDocument->interfaceState, id);
    const auto after = live::playbackRows(copy, id);
    if (live::playbackMediaKey(owner.liveDocument->interfaceState, id) != live::playbackMediaKey(copy, id))
        return;
    const auto project = owner.project();
    const auto active = owner.app.active_outputs();
    for (std::size_t i = 0; i < after.size(); ++i) {
        const bool trimChanged =
            before[i]["trimStart"] != after[i]["trimStart"] || before[i]["trimEnd"] != after[i]["trimEnd"];
        const bool routeChanged = live::playbackOutputs(before[i]) != live::playbackOutputs(after[i]);
        const bool regionsChanged=live::canonical(live::playbackRegions(owner.liveDocument->interfaceState,id,before[i])) !=
            live::canonical(live::playbackRegions(copy,id,after[i])) ||
            live::playbackSecondsPerBar(owner.liveDocument->interfaceState,id)!=live::playbackSecondsPerBar(copy,id);
        if (!trimChanged && !routeChanged && !regionsChanged)
            continue;
        if (owner.app.recording() || owner.app.engine()->state().playback == mrs::PlaybackState::playing)
            throw std::runtime_error("Pause or stop Playback before trimming or routing audio");
        const auto frames = owner.app.source_frames(owner.app.playback_sources()[i].id);
        const auto start =
            static_cast<mrs::Sample>(std::llround(static_cast<double>(after[i]["trimStart"]) * project->sample_rate));
        const auto end = after[i]["trimEnd"].isVoid()
                             ? frames
                             : static_cast<mrs::Sample>(
                                   std::llround(static_cast<double>(after[i]["trimEnd"]) * project->sample_rate));
        if (start < 0 || end <= start || end > frames)
            throw std::runtime_error("Trim must stay inside the original WAV source");
        const auto settings=live::playbackStemLayout(copy,id,after[i],project->sample_rate);
        if(settings.regions)for(const auto& region:*settings.regions)
            if(region.offset<0 || region.length<=0 || region.offset>frames || region.length>frames-region.offset)
                throw std::runtime_error("Region must stay inside the original WAV source");
        for (const auto channel : live::playbackOutputs(after[i]))
            if (std::find(active.begin(), active.end(), channel) == active.end())
                throw std::runtime_error("Enable the physical output in Audio settings first");
    }
    if(live::playbackRegionsKey(owner.liveDocument->interfaceState,id)!=live::playbackRegionsKey(copy,id))
        owner.app.validate_playback_regions(live::playbackStemLayouts(copy,id,project->sample_rate));
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
    juce::UndoableAction *createCoalescedAction(juce::UndoableAction *next) override {
        const auto *edit = dynamic_cast<LiveInterfaceEdit *>(next);
        if (!edit || owner != edit->owner || live::canonical(after) != live::canonical(edit->before)) return nullptr;
        return owner ? new LiveInterfaceEdit(*owner, before, edit->after) : nullptr;
    }
    int getSizeInUnits() override {
        return static_cast<int>(juce::JSON::toString(before).getNumBytesAsUTF8() +
                                juce::JSON::toString(after).getNumBytesAsUTF8());
    }

  private:
    juce::Component::SafePointer<Desktop> owner;
    juce::var before, after;
};
// Notes are plain text. Typing must not clone/validate every media, patch and
// musical binding, or retain full show snapshots for each keystroke.
class LiveNotesEdit final : public juce::UndoableAction {
  public:
    LiveNotesEdit(Desktop &desktop, juce::String item, juce::String oldText, juce::String newText)
        : owner(&desktop), id(std::move(item)), before(std::move(oldText)), after(std::move(newText)) {}
    bool perform() override { return apply(after); }
    bool undo() override { return apply(before); }
    juce::UndoableAction *createCoalescedAction(juce::UndoableAction *next) override {
        const auto *edit = dynamic_cast<LiveNotesEdit *>(next);
        return edit && owner && owner == edit->owner && id == edit->id && after == edit->before
            ? new LiveNotesEdit(*owner, id, before, edit->after) : nullptr;
    }
    int getSizeInUnits() override { return static_cast<int>(before.getNumBytesAsUTF8() + after.getNumBytesAsUTF8()); }
  private:
    bool apply(const juce::String &text) {
        if (!owner || !owner->liveDocument) return false;
        live::put(live::require(owner->liveDocument->interfaceState, "items", id), "notes", text);
        owner->updateLiveDirty();
        ++owner->liveUiRevision;
        return true;
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::String id, before, after;
};
} // namespace
void Desktop::setLiveItemNotes(const juce::String &id, const juce::String &text) {
    if (!liveDocument || text.length() > 4000) throw std::runtime_error("Notes must be at most 4000 characters");
    auto &item = live::require(liveDocument->interfaceState, "items", id);
    const auto before = item["notes"].toString();
    if (before == text) return;
    // Retain the document size bound without running unrelated runtime preflight.
    const auto bytes = juce::JSON::toString(liveDocument->interfaceState).getNumBytesAsUTF8() +
        juce::JSON::toString(juce::var(text)).getNumBytesAsUTF8() - juce::JSON::toString(juce::var(before)).getNumBytesAsUTF8();
    if (bytes > 1024 * 1024)
        throw std::runtime_error("Show interface exceeds 1 MiB");
    const auto now = juce::Time::getMillisecondCounter();
    const auto label = "Item notes " + id;
    if (liveTypingLabel != label || now - liveTypingAt > 800) liveUndo.beginNewTransaction(label);
    liveShowEditing = true;
    liveUndo.perform(new LiveNotesEdit(*this, id, before, text));
    liveTypingLabel = label; liveTypingAt = now;
}
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
void Desktop::newLiveShow() {
    if (!standaloneLive)
        throw std::runtime_error("A blank Live show belongs to the MR Live application");
    if (app.recording())
        throw std::runtime_error("Stop recording before creating a new show");
    const bool session = app.audio_name() != "Offline clock (no sound)" && app.audio_name() != "Disconnected";
    closeEditors();
    cancelPreview();
    app.set_click_settings({});
    liveAudioKey.clear();
    app.new_project(session ? prefs.rate : 48000);
    liveDocument.emplace();
    liveFile = juce::File();
    prepareLiveInterface(true); // Clean untitled document; first edit enables Save/close confirmation.
    selectedTrack.reset();
    selectedClip.reset();
    selectedClips.clear();
    resetDevice();
    app.workspace(mrs::desktop::Workspace::live);
    liveEmptyRevision = std::pair{app.services().projects->state().revision, app.graphs()->state().revision};
    showProjectHome(false);
    refresh(true);
    reconnectDevice(session);
}
void Desktop::prepareLiveInterface(bool saved) {
    ++liveSourceCheckEpoch;++liveSelectionRevision;
    for (const auto &entry : livePreparedItems)
        app.cancel_playback(entry.media);
    livePreparedItems.clear();
    liveCurrentEntry.clear();
    livePendingEntry.clear();
    livePlaybackStore.reset();
    livePendingItem.clear();
    liveFailedPlaybackKey.clear();
    liveNextKey.clear();
    liveNextItem.clear();
    liveNextStatus.clear();
    livePreloadError.clear();
    livePlaybackItem.clear();
    livePlaybackStem.clear();
    livePlaybackMediaKey.clear();
    livePlaybackDeviceKey.clear();
    livePlaybackFile.clear();
    livePlaybackError.clear();
    livePlaybackMixKey.clear();
    liveMusicalKey.clear();livePerformanceKey.clear();liveAutoKey.clear();liveAutoToken=liveRuntimeToken=0;
    app.cancel_staged_playback();
    liveAudioKey.clear();
    app.set_click_settings({});
    livePlaybackLoading = false;
    liveUndo.clearUndoHistory();
    liveTypingLabel.clear();
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
void Desktop::editLiveInterface(const juce::String &label, std::function<void(juce::var &)> edit, bool coalesceTyping) {
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
    const auto now = juce::Time::getMillisecondCounter();
    if (!coalesceTyping || liveTypingLabel != label || now - liveTypingAt > 800) liveUndo.beginNewTransaction(label);
    liveUndo.perform(new LiveInterfaceEdit(*this, liveDocument->interfaceState, draft));
    liveTypingLabel = coalesceTyping ? label : juce::String();
    liveTypingAt = now;
    if (!coalesceTyping) liveUndo.beginNewTransaction();
}
void Desktop::undoLiveInterface(bool redoAction) {
    liveTypingLabel.clear();
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
    app.set_click_settings({});
    liveAudioKey.clear();
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
void Desktop::selectLiveItem(const juce::String &id, const juce::String &entry) {
    ++liveSelectionRevision;
    if (!liveDocument || !live::find(liveDocument->interfaceState, "items", id).isObject())
        throw std::runtime_error("Missing show item");
    if (liveDocument->projects.empty()) {
        if (app.recording())
            throw std::runtime_error("Stop recording before changing items");
        livePreloadError.clear();
        liveFailedPlaybackKey.clear();
        const auto index =
            live::entryIndex(liveDocument->interfaceState, id, entry.isNotEmpty() ? entry : liveCurrentEntry);
        livePendingEntry = index < 0
                               ? juce::String()
                               : live::activeSetlist(liveDocument->interfaceState)["entries"][index]["id"].toString();
        const bool anotherEntry = entry.isNotEmpty() && livePendingEntry != liveCurrentEntry;
        if (liveDocument->interfaceState["currentItem"].toString() != id || livePlaybackItem != id || anotherEntry)
            livePendingItem = id;
        else {
            livePendingItem.clear();
            livePendingEntry.clear();
            livePlaybackLoading = false;
        }
        if (livePendingItem.isEmpty())
            ++liveTimelineFocusRevision; // Reselect an already active entry immediately.
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
        app.set_click_settings({});
        liveAudioKey.clear();
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
namespace {
juce::String addPlaybackChannel(juce::var &state, const juce::String &name) {
    return live::addChannel(state, "Playback", name);
}
void ensurePlaybackChannel(juce::var &state) {
    for (const auto &channel : *state["channels"].getArray())
        if (channel["type"].toString() == "Playback")
            return;
    addPlaybackChannel(state, "Backing");
}
} // namespace
void Desktop::addLivePlaybackStems(const juce::String &id, const juce::Array<juce::File> &files) {
    if (!liveDocument || !live::find(liveDocument->interfaceState, "items", id).isObject())
        throw std::runtime_error("Select a Live item first");
    if (app.recording())
        throw std::runtime_error("Stop recording before changing stems");
    if (files.isEmpty())
        return;
    for (const auto &file : files)
        if (!file.existsAsFile() || !file.hasFileExtension("wav"))
            throw std::runtime_error("Select existing WAV files");
    editLiveInterface("Add Playback stems", [&](auto &state) {
        ensurePlaybackChannel(state);
        auto root = live::playback(state, id);
        if (!root.isObject())
            root = live::object();
        std::set<juce::String> used;
        for (const auto &channel : *state["channels"].getArray())
            if (channel["type"].toString() == "Playback") {
                used.insert(channel["id"].toString());
                break;
            }
        for (const auto &stem : live::playbackRows(state, id))
            if (stem["id"].isString())
                used.insert(stem["id"].toString());
        for (const auto &file : files) {
            auto stem = live::object();
            live::put(stem, "file", file.getFullPathName());
            live::put(stem, "gain", 0.);
            live::put(stem, "mute", false);
            live::put(stem, "solo", false);
            if (!root["file"].isString()) {
                live::put(stem, "gain", live::importedStemGain(state, id, stem));
                live::resetPlaybackRegions(state,id,stem);
                root = stem;
                continue;
            }
            auto values = root["stems"];
            if (!values.isArray()) {
                values = juce::Array<juce::var>();
                live::put(root, "stems", values);
            }
            if (values.size() >= 31)
                throw std::runtime_error("Playback supports up to 32 stems");
            juce::String channelId;
            for (const auto &channel : *state["channels"].getArray())
                if (channel["type"].toString() == "Playback" && !used.contains(channel["id"].toString())) {
                    channelId = channel["id"].toString();
                    break;
                }
            if (channelId.isEmpty())
                channelId = addPlaybackChannel(state, file.getFileNameWithoutExtension());
            used.insert(channelId);
            live::put(stem, "id", channelId);
            live::put(stem, "gain", live::importedStemGain(state, id, stem));
            live::resetPlaybackRegions(state,id,stem);
            values.getArray()->add(stem);
        }
        live::put(live::map(live::performance(state), "nativePlayback"), id, root);
    });
    syncLivePlayback();
}
void Desktop::attachLivePlayback(const juce::String &id, const juce::File &file, const juce::String &stem) {
    if (!liveDocument || !live::find(liveDocument->interfaceState, "items", id).isObject())
        throw std::runtime_error("Select a Live item first");
    if (app.recording())
        throw std::runtime_error("Stop recording before changing Playback");
    if (!file.existsAsFile() || !file.hasFileExtension("wav"))
        throw std::runtime_error("Select an existing WAV file");
    editLiveInterface("Attach Playback WAV", [&](auto &state) {
        ensurePlaybackChannel(state);
        auto row = live::playbackStem(state, id, stem);
        if (stem.isNotEmpty() && !row.isObject())
            throw std::runtime_error("Playback stem was removed");
        if (!row.isObject()) {
            row = live::object();
            live::put(row, "gain", live::importedStemGain(state, id, row));
            live::put(row, "mute", false);
            live::put(row, "solo", false);
        }
        // A replacement source starts with its complete extent, retaining the item mix.
        row.getDynamicObject()->removeProperty("trimStart");
        row.getDynamicObject()->removeProperty("trimEnd");
        live::put(row, "file", file.getFullPathName());
        live::resetPlaybackRegions(state,id,row);
        if (stem.isEmpty())
            live::put(live::map(live::performance(state), "nativePlayback"), id, row);
    });
    invalidateLivePlayback(id);syncLivePlayback();
}
void Desktop::invalidateLivePlayback(const juce::String& id){
    // Choosing a WAV explicitly refreshes even a same-path preparation after an external edit.
    for (auto it = livePreparedItems.begin(); it != livePreparedItems.end();) {
        if (it->id == id) {
            app.cancel_playback(it->media);
            it = livePreparedItems.erase(it);
        } else
            ++it;
    }
    liveFailedPlaybackKey.clear();
    if (liveDocument->interfaceState["currentItem"].toString() == id)
        livePlaybackItem.clear();
    if (liveNextItem == id)
        liveNextKey.clear();
}
void Desktop::commitLiveRelink(const juce::String& item,const juce::String& stem,const juce::File& file){
    requireLiveRepair(item);
    if(!file.existsAsFile())throw std::runtime_error("Replacement WAV is missing");
    editLiveInterface("Relink Playback WAV",[&](auto& state){auto row=live::playbackStem(state,item,stem);
        if(!row.isObject())throw std::runtime_error("Playback source was removed");
        live::put(row,"file",file.getFullPathName());
    });
    invalidateLivePlayback(item);syncLivePlayback();
}

void Desktop::setLivePlaybackMix(double gain, bool mute, bool solo) {
    if (!liveDocument || !livePlaybackReady())
        throw std::runtime_error("Playback is not ready");
    setLivePlaybackStemMix(liveDocument->interfaceState["currentItem"].toString(), livePlaybackStem, gain, mute, solo);
}
void Desktop::setLivePlaybackStemMix(const juce::String &id, const juce::String &stem, double gain, bool mute, bool solo) {
    if (!liveDocument || !live::playbackStem(liveDocument->interfaceState, id, stem).isObject())
        throw std::runtime_error("Playback stem was removed");
    editLiveInterface("Playback stem mix", [&](auto &state) {
        auto row = live::playbackStem(state, id, stem);
        live::put(row, "gain", gain); live::put(row, "mute", mute); live::put(row, "solo", solo);
    });
    syncLivePlayback();
}
void Desktop::setLiveItemPlaybackGain(double gain) {
    if (!liveDocument) return;
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    editLiveInterface("Item Playback level", [&](auto &state) {
        live::put(live::require(state, "items", id), "playbackGroupGain", gain);
    });
    syncLivePlayback();
}
void Desktop::setLiveChannelMix(const juce::String &id, double gain, double pan, bool mute, bool solo) {
    app.cancel_mix_preview();
    editLiveInterface("Channel mix", [&](auto &state) {
        auto &channel = live::require(state, "channels", id);
        live::put(channel, "gain", gain);
        live::put(channel, "pan", pan * 100.);
        live::put(channel, "mute", mute);
        live::put(channel, "solo", solo);
    });
    syncLivePlayback();
}
void Desktop::previewLiveChannelMix(const juce::String &id, double gain, double pan, bool mute, bool solo) {
    if (!liveDocument || !livePlaybackReady()) return;
    auto state = liveDocument->interfaceState.clone();
    auto &channel = live::require(state, "channels", id);
    live::put(channel, "gain", gain); live::put(channel, "pan", pan * 100.);
    live::put(channel, "mute", mute); live::put(channel, "solo", solo);
    const auto item = state["currentItem"].toString();
    const auto rows = live::playbackRows(state, item);
    std::vector<std::pair<mrs::Id, mrs::Track::Mix>> mixes;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto mix = live::playbackMix(state, item, rows[i]); mix.validate();
        mixes.emplace_back(project()->tracks[i].id, mix);
    }
    app.preview_mixes(mixes, project()->master_gain);
}
void Desktop::setLivePlaybackTrim(double start, double end) {
    if (!livePlaybackReady())
        throw std::runtime_error("Playback is not ready");
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0. || end <= start)
        throw std::runtime_error("Invalid Playback trim range");
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    editLiveInterface("Trim Playback", [&](auto &state) {
        auto row = live::playbackStem(state, id, livePlaybackStem);
        live::put(row, "trimStart", start);
        live::put(row, "trimEnd", end);
    });
    syncLivePlayback();
}
std::size_t Desktop::livePlaybackIndex() const {
    if (!liveDocument || livePlaybackStem.isEmpty())
        return 0;
    const auto rows =
        live::playbackRows(liveDocument->interfaceState, liveDocument->interfaceState["currentItem"].toString());
    for (std::size_t i = 1; i < rows.size(); ++i)
        if (rows[i]["id"].toString() == livePlaybackStem)
            return i;
    return 0;
}
void Desktop::requireLiveRepair(const juce::String& item,bool global){
    if(!liveDocument||app.recording())throw std::runtime_error("Open a show and stop recording before repairing sources");
    if(liveAutoToken&&(liveAutoItem==item||global)){
        if(!app.cancel_staged_playback())throw std::runtime_error("Item transition in progress; retry repair");
        if(app.playback_token()==liveAutoToken)syncLivePlayback();
        else{liveAutoKey.clear();liveAutoToken=0;}
    }
    if((global||liveDocument->interfaceState["currentItem"].toString()==item)&&app.engine()->state().playback==mrs::PlaybackState::playing)
        throw std::runtime_error("Pause or stop before repairing active outputs or media");
}
void Desktop::repairLivePlaybackOutputs(const juce::String& item,const juce::String& stem){
    requireLiveRepair(item);const bool active=liveDocument->interfaceState["currentItem"].toString()==item;
    editLiveInterface("Repair Playback output",[&](auto& state){auto row=live::playbackStem(state,item,stem);
        if(!row.isObject())throw std::runtime_error("Playback source was removed");
        live::put(row,"outputs",juce::Array<juce::var>());
    });
    if(active)livePlaybackError.clear();
    liveFailedPlaybackKey.clear();syncLivePlayback();
}
void Desktop::setLivePlaybackOutputs(const std::vector<int> &outputs) {
    if (!liveDocument || app.recording() || app.engine()->state().playback == mrs::PlaybackState::playing)
        throw std::runtime_error("Pause or stop Playback before routing audio");
    const auto active = app.active_outputs();
    for (const auto output : outputs)
        if (std::find(active.begin(), active.end(), output) == active.end())
            throw std::runtime_error("Enable the physical output in Audio settings first");
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    if (!live::playbackStem(liveDocument->interfaceState, id, livePlaybackStem).isObject())
        throw std::runtime_error("Choose a Playback stem first");
    editLiveInterface("Playback output", [&](auto &state) {
        auto row = live::playbackStem(state, id, livePlaybackStem);
        juce::Array<juce::var> channels;
        for (int output : outputs)
            channels.add(output);
        live::put(row, "outputs", channels);
    });
    if (!project()->clips.empty())
        livePlaybackError.clear();
    syncLivePlayback();
}
void Desktop::setLiveCue(bool cue) {
    if (app.recording())
        throw std::runtime_error("Stop recording before changing Cue");
    if (!liveDocument)
        throw std::runtime_error("Open a show first");
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    if (!live::playbackStem(liveDocument->interfaceState, id, livePlaybackStem).isObject())
        throw std::runtime_error("Choose a stem first");
    editLiveInterface("Playback / Cue role", [&](auto &state) {
        auto stem = live::playbackStem(state, id, livePlaybackStem);
        live::put(stem, "cue", cue);
    });
    syncLivePlayback();
}
void Desktop::setLiveAudio(double gain, mrs::audio::ClickSettings click) {
    if (app.recording())
        throw std::runtime_error("Stop recording before changing Live audio");
    click.validate();
    editLiveInterface("Live Playback / Click", [&](auto &state) {
        auto settings = live::map(live::performance(state), "nativeLiveAudio");
        live::put(settings, "gain", gain);
        auto value = live::object();
        live::put(value, "enabled", click.playback);
        live::put(value, "accent", click.accent);
        live::put(value, "level", click.level);
        juce::Array<juce::var> outputs;
        if (click.output_left >= 0)
            outputs.add(click.output_left);
        if (click.output_right >= 0)
            outputs.add(click.output_right);
        live::put(value, "outputs", outputs);
        live::put(settings, "click", value);
    });
    livePlaybackError.clear();
    syncLivePlayback();
}
void Desktop::removeLivePlaybackStem() {
    if (!liveDocument || livePlaybackStem.isEmpty())
        throw std::runtime_error("The primary Playback cannot be removed here");
    const auto id = liveDocument->interfaceState["currentItem"].toString(), stem = livePlaybackStem;
    editLiveInterface("Remove Playback stem", [&](auto &state) {
        auto stems = live::playback(state, id)["stems"];
        for (int i = stems.size(); --i >= 0;)
            if (stems[i]["id"].toString() == stem)
                stems.getArray()->remove(i);
    });
    livePlaybackStem.clear();
    syncLivePlayback();
}
bool Desktop::livePlaybackReady() const {
    if (!liveDocument || livePlaybackLoading || livePlaybackError.isNotEmpty() || app.wav_import_status().busy)
        return false;
    const auto id = liveDocument->interfaceState["currentItem"].toString();
    const auto rows = live::playbackRows(liveDocument->interfaceState, id);
    return id == livePlaybackItem && !rows.empty() &&
           livePlaybackMediaKey == live::playbackMediaKey(liveDocument->interfaceState, id) &&
           project()->tracks.size() == rows.size() && app.playback_sources().size() == rows.size();
}
bool Desktop::liveTransportReady() const {
    if (livePlaybackReady())
        return true;
    if (!liveDocument || livePlaybackLoading || livePlaybackError.isNotEmpty())
        return false;
    const auto &state = liveDocument->interfaceState;
    const auto id = state["currentItem"].toString();
    return live::find(state, "items", id).isObject() && live::playbackRows(state, id).empty() &&
           live::liveClick(state).playback && liveMusicalKey == live::canonical(live::musicalVersion(state, id));
}
std::shared_ptr<mrs::desktop::Application::PlaybackPreparation> Desktop::prepareLivePlayback(const juce::String &id) {
    const auto &state = liveDocument->interfaceState;
    const auto key = live::playbackMediaKey(state, id) + ":" + juce::String(project()->sample_rate);
    for (auto &entry : livePreparedItems)
        if (entry.id == id && entry.key == key)
            return entry.media;
    for (auto it = livePreparedItems.begin(); it != livePreparedItems.end();) {
        if (it->id == id) {
            app.cancel_playback(it->media);
            it = livePreparedItems.erase(it);
        } else
            ++it;
    }
    std::vector<std::filesystem::path> paths;
    for (const auto &row : live::playbackRows(state, id))
        paths.emplace_back(row["file"].toString().toWideCharPointer());
    auto media = app.prepare_playback(paths);
    livePreparedItems.push_back({id, key, media});
    while (livePreparedItems.size() > 3) {
        auto victim = std::find_if(livePreparedItems.begin(), livePreparedItems.end(), [&](const auto &entry) {
            return entry.id != state["currentItem"].toString() && entry.id != livePendingItem && entry.id != id;
        });
        if (victim == livePreparedItems.end())
            break;
        app.cancel_playback(victim->media);
        livePreparedItems.erase(victim);
    }
    return media;
}
void Desktop::syncLivePreload() {
    if (!liveDocument || !liveDocument->projects.empty())
        return;
    const auto &state = liveDocument->interfaceState;
    const auto nextEntry = live::nextPerformanceEntry(state, liveCurrentEntry);
    const auto id = nextEntry["itemId"].toString();
    const auto key = nextEntry["id"].toString() + id + live::playbackMediaKey(state, id) + ":" +
                     juce::String(project()->sample_rate);
    try {
        if (key != liveNextKey) {
            liveNextKey = key;
            liveNextItem = id;
            liveNextStatus.clear();
            if (id.isNotEmpty())
                prepareLivePlayback(id);
        }
        if (id.isEmpty()) { app.cancel_staged_playback();liveAutoKey.clear();liveAutoToken=0;return; }
        const auto found = std::find_if(livePreparedItems.begin(), livePreparedItems.end(),
                                        [&](const auto &entry) { return entry.id == id; });
        if (found == livePreparedItems.end())
            return;
        const auto status = app.playback_preparation(found->media);
        const auto name = live::find(state, "items", id)["name"].toString();
        liveNextStatus = "Next: " + name + " — " +
                         (status.busy ? "preparing " + juce::String(static_cast<int>(status.progress * 100)) + "%"
                          : status.message.empty() ? "media ready"
                                                   : "unavailable: " + juce::String::fromUTF8(status.message.c_str()));
        const auto stageKey=key+live::canonical(live::musicalVersion(state,id))+live::playbackMixKey(state,id)+live::playbackRegionsKey(state,id)+
            live::performanceKey(state,id)+live::performanceKey(state,livePlaybackItem)+live::playbackMixKey(state,livePlaybackItem)+
            live::canonical(live::liveAudio(state));
        if(!status.busy&&status.message.empty()&&liveRuntimeToken&&app.audio_running()&&livePendingItem.isEmpty()&&
           (liveAutoKey!=stageKey||app.staged_playback_token()==0)){
            const auto token=++liveTokenSerial;
            app.stage_playback(found->media,performanceLayout(state,id,project()->sample_rate,token),liveRuntimeToken);
            liveAutoToken=token;liveAutoItem=id;liveAutoEntry=nextEntry["id"].toString();liveAutoKey=stageKey;
        }
        if(app.staged_playback_token())liveNextStatus="Next: "+name+" — audio ready; graph prepared";
        const auto boundary=app.engine()->performance_status();
        if((boundary.action==mrs::PerformanceAction::next||boundary.action==mrs::PerformanceAction::next_stopped)&&
           app.engine()->state().playback==mrs::PlaybackState::stopped)
            liveNextStatus+="; waiting at item end";

    } catch (const std::exception &error) {
        liveNextStatus = "Next: unavailable — " + juce::String::fromUTF8(error.what());
    }
}
void Desktop::cancelLiveSwitch() {
    const auto id = livePendingItem;
    if (id.isEmpty())
        return;
    for (auto it = livePreparedItems.begin(); it != livePreparedItems.end();) {
        if (it->id == id) {
            app.cancel_playback(it->media);
            it = livePreparedItems.erase(it);
        } else
            ++it;
    }
    livePendingItem.clear();
    livePendingEntry.clear();
    livePlaybackLoading = false;
    livePreloadError = "Item switch cancelled";
    if (liveNextItem == id)
        liveNextStatus = "Next: cancelled";
}
void Desktop::stepLiveItem(int direction) {
    if (!liveDocument)
        return;
    const auto entry = live::neighborEntry(liveDocument->interfaceState, direction, liveCurrentEntry);
    const auto id = entry["itemId"].toString();
    if (id.isNotEmpty())
        selectLiveItem(id, entry["id"].toString());
}
void Desktop::syncLivePlayback() {
    if (!liveDocument || !liveDocument->projects.empty())
        return;
    app.poll_playback_transition();
    if(liveAutoToken&&app.playback_token()==liveAutoToken){
        const auto id=liveAutoItem;liveRuntimeToken=liveAutoToken;liveAutoToken=0;liveAutoKey.clear();
        live::put(liveDocument->interfaceState,"currentItem",id);liveCurrentEntry=liveAutoEntry;
        livePlaybackItem=id;livePlaybackStore=app.services().projects;
        livePlaybackMediaKey=live::playbackMediaKey(liveDocument->interfaceState,id);
        livePlaybackFile=live::playback(liveDocument->interfaceState,id)["file"].toString();
        liveMusicalKey=live::canonical(live::musicalVersion(liveDocument->interfaceState,id));
        livePerformanceKey=live::performanceKey(liveDocument->interfaceState,id);
        livePlaybackMixKey=live::playbackMixKey(liveDocument->interfaceState,id);
        livePlaybackRegionsKey=live::playbackRegionsKey(liveDocument->interfaceState,id);
        livePlaybackError.clear();
        liveEmptyRevision=std::pair{app.services().projects->state().revision,app.graphs()->state().revision};
        visualTransport.reset();lastVisualSample=0;lastDisplayState={};
        selectedTrack.reset();selectedClip.reset();++liveUiRevision;++liveTimelineFocusRevision;refresh(true);
    }
    bool changed = false;
    const auto selectedId = liveDocument->interfaceState["currentItem"].toString();
    const auto selectedIndex = live::entryIndex(liveDocument->interfaceState, selectedId, liveCurrentEntry);
    const auto selectedEntry =
        selectedIndex < 0
            ? juce::String()
            : live::activeSetlist(liveDocument->interfaceState)["entries"][selectedIndex]["id"].toString();
    if (selectedEntry != liveCurrentEntry) {
        liveCurrentEntry = selectedEntry;
        ++liveUiRevision;
        ++liveTimelineFocusRevision;
    }
    const auto id =
        livePendingItem.isNotEmpty() ? livePendingItem : liveDocument->interfaceState["currentItem"].toString();
    if (!live::find(liveDocument->interfaceState, "items", id).isObject()) {
        livePendingItem.clear();
        livePlaybackLoading = false;
        if (livePlaybackItem.isNotEmpty()) {
            auto empty = app.prepare_playback({});
            app.activate_playback(empty, {});
            livePlaybackItem.clear();
            livePlaybackFile.clear();
            livePlaybackMediaKey.clear();
            liveMusicalKey.clear();
            liveAudioKey.clear();
            liveEmptyRevision = std::pair{app.services().projects->state().revision, app.graphs()->state().revision};
            refresh(true);
        }
        syncLivePreload();
        return;
    }
    const auto row = live::playback(liveDocument->interfaceState, id);
    const auto rows = live::playbackRows(liveDocument->interfaceState, id);
    const auto file = row["file"].toString();
    const auto mediaKey = live::playbackMediaKey(liveDocument->interfaceState, id);
    juce::String deviceKey;
    for (int output : app.active_outputs())
        deviceKey += juce::String(output) + ":";
    const auto music = live::musicalVersion(liveDocument->interfaceState, id);
    const auto musicKey = live::canonical(music);
    const auto failureKey = id + live::canonical(row) + live::canonical(live::liveAudio(liveDocument->interfaceState)) +
                            musicKey + deviceKey + live::playbackRegionsKey(liveDocument->interfaceState,id)+live::performanceKey(liveDocument->interfaceState,id);
    const bool mediaChanged = (livePendingItem.isNotEmpty() && livePendingEntry != liveCurrentEntry) ||
                              livePlaybackStore != app.services().projects || livePlaybackItem != id ||
                              livePlaybackMediaKey != mediaKey ||
                              (livePlaybackError.isNotEmpty() && livePlaybackDeviceKey != deviceKey);
    if (!mediaChanged)
        livePlaybackLoading = false;
    if (mediaChanged) {
        if (liveFailedPlaybackKey == failureKey)
            return;
        if (app.recording())
            throw std::runtime_error("Stop recording before changing Playback");
        std::shared_ptr<mrs::desktop::Application::PlaybackPreparation> media;
        try {
            media = prepareLivePlayback(id);
            const auto status = app.playback_preparation(media);
            livePlaybackLoading = status.busy;
            if (status.busy)
                return; // Existing Core graph/device continues until the entire batch is ready.
            if (!status.message.empty())
                throw std::runtime_error(status.message);
            auto layout=performanceLayout(liveDocument->interfaceState,id,project()->sample_rate,++liveTokenSerial);
            closeEditors();cancelPreview();app.activate_playback(media,layout);
            liveRuntimeToken=layout.token;livePerformanceKey=live::performanceKey(liveDocument->interfaceState,id);
            visualTransport.reset();lastVisualSample=0;lastDisplayState={};
            liveAutoKey.clear();liveAutoToken=0;
            const bool selected = liveDocument->interfaceState["currentItem"].toString() != id;
            live::put(liveDocument->interfaceState, "currentItem", id);
            const auto entryIndex = live::entryIndex(
                liveDocument->interfaceState, id, livePendingEntry.isNotEmpty() ? livePendingEntry : liveCurrentEntry);
            liveCurrentEntry =
                entryIndex < 0
                    ? juce::String()
                    : live::activeSetlist(liveDocument->interfaceState)["entries"][entryIndex]["id"].toString();
            livePendingEntry.clear();
            livePendingItem.clear();
            livePlaybackLoading = false;
            livePreloadError.clear();
            livePlaybackError.clear();
            liveFailedPlaybackKey.clear();
            livePlaybackItem = id;
            livePlaybackFile = file;
            livePlaybackMediaKey = mediaKey;
            livePlaybackStore = app.services().projects;
            livePlaybackDeviceKey = deviceKey;
            livePlaybackMixKey.clear();
            // activate_playback already installed these exact regions and plan.
            livePlaybackRegionsKey=live::playbackRegionsKey(liveDocument->interfaceState,id);
            liveMusicalKey = musicKey;
            liveAudioKey.clear();
            selectedTrack.reset();
            selectedClip.reset();
            if (livePlaybackIndex() == 0)
                livePlaybackStem.clear();
            if (selected) {
                liveShowEditing = true;
                updateLiveDirty();
            }
            // Completion is the focus boundary, including another occurrence
            // of the same item whose media preparation finished asynchronously.
            ++liveTimelineFocusRevision;
            ++liveUiRevision;
            changed = true;
            refresh(true);
        } catch (const std::exception &error) {
            livePlaybackLoading = false;
            const auto reason = juce::String::fromUTF8(error.what());
            if (livePendingItem.isNotEmpty()) {
                livePreloadError = "Cannot switch: " + reason;
                if (id == liveNextItem)
                    liveNextStatus = livePreloadError;
                livePendingItem.clear();
                livePendingEntry.clear();
            } else {
                livePlaybackError = reason;
                liveFailedPlaybackKey = failureKey;
            }
            for (auto it = livePreparedItems.begin(); it != livePreparedItems.end();) {
                if (it->id == id && (!media || !app.playback_preparation(media).message.empty())) {
                    app.cancel_playback(it->media);
                    it = livePreparedItems.erase(it);
                } else
                    ++it;
            }
            return;
        }
    } else if (musicKey != liveMusicalKey) {
        const auto projection = live::musicalProjection(music);
        app.set_musical_data(projection.time, projection.chords, projection.sections);
        liveMusicalKey = musicKey;livePerformanceKey.clear();
        changed = true;
    }
    const auto audioKey = live::canonical(live::liveAudio(liveDocument->interfaceState)) + deviceKey;
    if (audioKey != liveAudioKey) {
        try {
            app.set_click_settings(live::liveClick(liveDocument->interfaceState));
            liveAudioKey = audioKey;
        } catch (const std::exception &error) {
            app.stop();
            app.set_click_settings({});
            liveAudioKey = audioKey; // Retry only after an explicit settings/device change.
            livePlaybackError = juce::String::fromUTF8(error.what());
            return;
        }
    }
    if (livePlaybackReady()) {
        const auto key = live::playbackMixKey(liveDocument->interfaceState, id);
        if (key != livePlaybackMixKey) {
            const auto snapshot = project();
            const auto active = app.active_outputs();
            // Validate the complete batch before any Core trim/route is published.
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const auto frames = app.source_frames(app.playback_sources()[i].id);
                const auto start = static_cast<mrs::Sample>(
                    std::llround(static_cast<double>(rows[i]["trimStart"]) * snapshot->sample_rate));
                const auto end = rows[i]["trimEnd"].isVoid()
                                     ? frames
                                     : static_cast<mrs::Sample>(std::llround(static_cast<double>(rows[i]["trimEnd"]) *
                                                                             snapshot->sample_rate));
                bool valid = start >= 0 && end > start && end <= frames;
                for (const auto output : live::playbackOutputs(rows[i]))
                    valid = valid && std::find(active.begin(), active.end(), output) != active.end();
                if (!valid) {
                    livePlaybackError =
                        "Playback trim or output is unavailable. Check WAV sources and active Audio outputs.";
                    app.stop();
                    liveEmptyRevision =
                        std::pair{app.services().projects->state().revision, app.graphs()->state().revision};
                    return;
                }
            }
            const auto regionsKey=live::playbackRegionsKey(liveDocument->interfaceState,id);
            if(regionsKey!=livePlaybackRegionsKey){
                app.set_playback_regions(live::playbackStemLayouts(liveDocument->interfaceState,id,snapshot->sample_rate));
                livePlaybackRegionsKey=regionsKey;livePerformanceKey.clear();
            }
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const auto outputs = live::playbackOutputs(rows[i]);
                if (project()->tracks[i].hardware_outputs != outputs)
                    app.set_hardware_output(project()->tracks[i].id, outputs);
                auto mix = live::playbackMix(liveDocument->interfaceState, id, rows[i]);
                app.set_track_mix(project()->tracks[i].id, mix);
            }
            livePlaybackMixKey = key;
            changed = true;
        }
    }
    // Region publication can change the implicit item end. Finish its plan in
    // this synchronization pass, before callers can start playback.
    const auto policyKey=live::performanceKey(liveDocument->interfaceState,id);
    if(policyKey!=livePerformanceKey){
        const auto projection=live::musicalProjection(music);
        const auto snapshot=project();mrs::Sample end{};for(const auto& clip:snapshot->clips)end=(std::max)(end,clip.start+clip.length);
        const mrs::Timeline clock(projection.time,snapshot->sample_rate);const auto& meter=projection.time.meters.front();
        const auto renderedBars=static_cast<double>(clock.to_ticks(end))/(4.*mrs::ppq*meter.numerator/meter.denominator);
        auto policy=live::preparePerformancePlan(liveDocument->interfaceState,id,projection.time,projection.sections,snapshot->sample_rate,renderedBars);
        if(end>0&&liveDocument->interfaceState["performance"]["arrangement"][juce::Identifier(id)]["length"].isVoid()){
            policy.end=(std::min)(end,clock.to_samples(static_cast<mrs::Tick>(4096.*4.*mrs::ppq*meter.numerator/meter.denominator)));
            std::erase_if(policy.sections,[&](const auto& section){return section.start>=policy.end;});
            for(auto& section:policy.sections)if(section.end>policy.end){
                section.end=policy.end;
                if(section.mode==mrs::PerformanceMode::stop||section.mode==mrs::PerformanceMode::loop)section.mode=mrs::PerformanceMode::normal;
            }
        }
        app.set_performance_plan(std::move(policy));livePerformanceKey=policyKey;
    }
    if (changed)
        liveEmptyRevision = std::pair{app.services().projects->state().revision, app.graphs()->state().revision};
    syncLivePreload();
}
} // namespace ui
