#pragma once
#include "Desktop.h"
#include "LiveContext.h"
#include "LiveStudioOneImportPlan.h"
#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <vector>

// #305 / MR Live 0.8.11 — read-only source selector; a single explicit
// Import operation creates a single show Undo transaction.
// No Studio One file content is persisted or uploaded.
namespace ui {
class LiveStudioOneImportDialog final : public juce::Component {
    struct Draft {
        bool selected{}, createNew{};
        juce::String targetItemId;
        live::ShowContextSelection fields;
    };
    struct SourceRow final : juce::Component {
        juce::ToggleButton selected;
        juce::TextButton configure{"Options"};
        juce::Label summary;
        SourceRow() {
            addAndMakeVisible(selected);
            addAndMakeVisible(configure);
            addAndMakeVisible(summary);
        }
        void resized() override {
            auto area = getLocalBounds().reduced(4, 2);
            selected.setBounds(area.removeFromLeft(245));
            configure.setBounds(area.removeFromRight(84).reduced(2));
            summary.setBounds(area);
        }
        void paint(juce::Graphics& g) override {
            g.setColour(juce::Colour(0xff47515e));
            g.drawHorizontalLine(getHeight() - 1, 0.f, static_cast<float>(getWidth()));
        }
    };
public:
    LiveStudioOneImportDialog(Desktop& desktop, live::StudioOneShowScan imported,
                               juce::String filename)
        : owner(&desktop), scan(std::move(imported)), sourceFileName(std::move(filename)),
          originalState(desktop.liveDocument->interfaceState.clone()),
          documentGeneration(desktop.liveDocumentGeneration),
          drafts(scan.items.size()) {
        configureLiveContext(theme, static_cast<float>(desktop.view.liveMenuOpacity));
        setLookAndFeel(&theme);
        setComponentID("live-studio-one-show-import");
        setName("Studio One Show context import");
        setSize(900, 600);
        for (auto* child : std::array<juce::Component*, 17>{
                 &title, &sourceName, &chooseShow, &search, &sources, &configureTitle, &target,
                 &sections, &tempo, &chords, &length, &previewTitle, &previewText,
                 &status, &previewButton, &importButton, &cancelButton})
            addAndMakeVisible(child);
        sources.setViewedComponent(&sourceBody, false);
        sources.setScrollBarsShown(true, false);
        sourceBody.setInterceptsMouseClicks(true, true);
        title.setText("Import musical context from Studio One Show", juce::dontSendNotification);
        sourceName.setText("No .show loaded", juce::dontSendNotification);
        chooseShow.setButtonText("Choose .show...");
        chooseShow.setTitle("Select a Studio One Show file without closing the import window");
        chooseShow.setComponentID("live-show-import-choose-file");
        chooseShow.onClick = [this] { chooseSourceFile(); };
        search.setTextToShowWhenEmpty("Filter source item titles", juce::Colours::grey);
        search.setTitle("Filter only: source positions and selections are preserved");
        search.setComponentID("live-show-import-search");
        search.onTextChange = [this] { rebuildSourceList(); };
        configureTitle.setText("Select a source item, then configure its import", juce::dontSendNotification);
        target.setTextWhenNothingSelected("Choose destination item...");
        target.addItem("Create new Browser item (not in Setlist)", 2);
        if (const auto* all = originalState["items"].getArray())
            for (const auto& item : *all) {
                const auto id = item["id"].toString();
                const auto label = item["name"].toString();
                destinationIds.push_back(id);
                target.addItem(juce::String(static_cast<int>(destinationIds.size())) + ". " + label,
                               static_cast<int>(destinationIds.size()) + 2);
            }
        target.setComponentID("live-show-import-target");
        target.onChange = [this] {
            if (syncing || focused < 0) return;
            auto& draft = drafts[static_cast<std::size_t>(focused)];
            draft.createNew = target.getSelectedId() == 2;
            const int ix = target.getSelectedId() - 3;
            draft.targetItemId = ix >= 0 && ix < static_cast<int>(destinationIds.size())
                               ? destinationIds[static_cast<std::size_t>(ix)] : juce::String{};
            invalidatePreview();
        };
        for (auto* field : {&sections, &tempo, &chords, &length}) {
            field->onClick = [this] {
                if (syncing || focused < 0) return;
                auto& d = drafts[static_cast<std::size_t>(focused)].fields;
                d.sections = sections.getToggleState();
                d.tempo = tempo.getToggleState();
                d.chords = chords.getToggleState();
                d.length = length.getToggleState();
                invalidatePreview();
            };
        }
        sections.setButtonText("Sections");
        tempo.setButtonText("Tempo");
        chords.setButtonText("Chords");
        length.setButtonText("Length (bars)");
        previewTitle.setText("Source availability, warnings and destination preview",
                             juce::dontSendNotification);
        previewText.setMultiLine(true);
        previewText.setReadOnly(true);
        previewText.setScrollbarsShown(true);
        previewText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff202831));
        previewText.setColour(juce::TextEditor::textColourId, juce::Colour(0xffcedbe8));
        previewText.setComponentID("live-show-import-preview-text");
        previewButton.setButtonText("Preview");
        previewButton.setComponentID("live-show-import-preview");
        importButton.setButtonText("Import");
        importButton.setComponentID("live-show-import-apply");
        importButton.setEnabled(false);
        cancelButton.setButtonText("Cancel");
        previewButton.onClick = [this] { validatePreview(); };
        importButton.onClick = [this] { commitImport(); };
        cancelButton.onClick = [this] { if (close) close(); };
        status.setText("Select item(s), configure each destination and field, then Preview.",
                       juce::dontSendNotification);
        status.setColour(juce::Label::textColourId, juce::Colour(0xffd2b178));
        focused = -1;
        syncConfiguration();
        updateSelectionCount();
        rebuildSourceList();
    }
    ~LiveStudioOneImportDialog() override {
        sources.setViewedComponent(nullptr, false);
        rows.clear();
        setLookAndFeel(nullptr);
    }
    std::function<void()> close;

    // Opening the dialog reuses the last successfully read source. The user
    // may switch sources inside the dialog; no musical import happens until
    // Import is explicitly confirmed.
    void restoreLastSource() {
        if (!owner || owner->view.lastStudioOneShow.isEmpty()) {
            status.setText("Choose a Studio One .show file to start importing.",
                           juce::dontSendNotification);
            return;
        }
        const juce::File previous(owner->view.lastStudioOneShow);
        if (!previous.existsAsFile()) {
            status.setText("Previous .show is missing. Use Choose .show... to locate it.",
                           juce::dontSendNotification);
            return;
        }
        try { loadSourceFile(previous); }
        catch (const std::exception& error) {
            status.setText("Cannot reopen last .show: " + juce::String::fromUTF8(error.what()),
                           juce::dontSendNotification);
        }
    }

    void resized() override {
        auto area = getLocalBounds().reduced(12);
        title.setBounds(area.removeFromTop(26));
        auto fileRow = area.removeFromTop(30);
        chooseShow.setBounds(fileRow.removeFromRight(150).reduced(2));
        sourceName.setBounds(fileRow.reduced(2));
        area.removeFromTop(6);
        auto bottom = area.removeFromBottom(42);
        cancelButton.setBounds(bottom.removeFromRight(112).reduced(3));
        importButton.setBounds(bottom.removeFromRight(118).reduced(3));
        previewButton.setBounds(bottom.removeFromRight(118).reduced(3));
        status.setBounds(bottom);
        area.removeFromBottom(6);
        auto left = area.removeFromLeft(440);
        search.setBounds(left.removeFromTop(30).reduced(2));
        left.removeFromTop(6);
        sources.setBounds(left);
        area.removeFromLeft(12);
        configureTitle.setBounds(area.removeFromTop(35));
        target.setBounds(area.removeFromTop(30).reduced(2));
        area.removeFromTop(10);
        auto checkA = area.removeFromTop(30);
        sections.setBounds(checkA.removeFromLeft(checkA.getWidth()/2));
        tempo.setBounds(checkA);
        auto checkB = area.removeFromTop(30);
        chords.setBounds(checkB.removeFromLeft(checkB.getWidth()/2));
        length.setBounds(checkB);
        area.removeFromTop(10);
        previewTitle.setBounds(area.removeFromTop(28));
        previewText.setBounds(area.reduced(2));
        layoutSourceRows();
    }

private:
    juce::Component::SafePointer<Desktop> owner;
    Theme theme;
    live::StudioOneShowScan scan;
    juce::String sourceFileName;
    juce::var originalState;
    std::uint64_t documentGeneration{};
    std::vector<Draft> drafts;
    std::vector<juce::String> destinationIds;
    int focused{-1};
    bool syncing{}, previewValid{};
    std::vector<live::StudioOneImportItem> previewPlan;
    juce::Label title, sourceName, configureTitle, previewTitle, status;
    juce::TextButton chooseShow;
    std::unique_ptr<juce::FileChooser> showChooser;
    juce::TextEditor search, previewText;
    juce::Viewport sources;
    juce::Component sourceBody;
    std::vector<std::unique_ptr<SourceRow>> rows;
    juce::ComboBox target;
    juce::ToggleButton sections, tempo, chords, length;
    juce::TextButton previewButton, importButton, cancelButton;

    void loadSourceFile(const juce::File& file) {
        // Decode completely BEFORE changing the current draft or remembered
        // file. A missing/corrupt replacement leaves the old source usable.
        auto fresh = live::readStudioOneShow(file);
        if (fresh.items.empty())
            throw std::runtime_error("The selected .show has no SetList items");
        scan = std::move(fresh);
        sourceFileName = file.getFileName();
        sourceName.setTooltip(file.getFullPathName());
        drafts.assign(scan.items.size(), Draft{});
        focused = -1;
        search.clear();
        syncConfiguration();
        updateSelectionCount();
        rebuildSourceList();
        invalidatePreview();
        status.setText("Loaded " + sourceFileName +
                           ". Select source items, destinations and fields.",
                       juce::dontSendNotification);
        if (owner && owner->view.lastStudioOneShow != file.getFullPathName()) {
            owner->view.lastStudioOneShow = file.getFullPathName();
            owner->saveSettings(); // user preference only; not .mrlive content
        }
    }
    void chooseSourceFile() {
        if (!owner) return;
        const auto lastPath = owner->view.lastStudioOneShow;
        const auto initial = lastPath.isNotEmpty()
            ? juce::File(lastPath)
            : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        showChooser = std::make_unique<juce::FileChooser>(
            "Choose Studio One Show", initial, "*.show", true);
        juce::Component::SafePointer<LiveStudioOneImportDialog> safe(this);
        showChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& chooser) {
                if (!safe || !safe->owner) return;
                const auto file = chooser.getResult();
                if (file == juce::File()) return; // Cancel retains draft
                try { safe->loadSourceFile(file); }
                catch (const std::exception& error) {
                    safe->status.setText("File not loaded: " +
                        juce::String::fromUTF8(error.what()) +
                        " (previous selection retained)", juce::dontSendNotification);
                }
            });
    }
    bool documentCurrent() const {
        return owner && owner->liveDocument && owner->liveDocumentGeneration == documentGeneration &&
               live::sameShowValue(owner->liveDocument->interfaceState, originalState);
    }
    bool available(const live::ShowContextSong& source, int field) const {
        switch (field) {
            case 0: return source.sections.isArray();
            case 1: return source.bpm.has_value() && !source.hasTempoChanges;
            case 2: return source.chords.isArray();
            case 3: return source.lengthBars.has_value();
        }
        return false;
    }
    void updateSelectionCount() {
        if (sourceFileName.isEmpty()) {
            sourceName.setText("No .show loaded — choose a file", juce::dontSendNotification);
            return;
        }
        const auto selected = static_cast<int>(std::count_if(drafts.begin(), drafts.end(),
                                                       [](const Draft& draft) { return draft.selected; }));
        sourceName.setText(sourceFileName + "  |  " + juce::String(static_cast<int>(scan.items.size())) +
                           " source items  |  " + juce::String(selected) + " selected",
                           juce::dontSendNotification);
    }
    void invalidatePreview() {
        updateSelectionCount();
        previewValid = false;
        previewPlan.clear();
        importButton.setEnabled(false);
        try {
            // Do not automatically choose any destination/field. Once the
            // explicitly configured selections are complete, run the same
            // detached preview as the button, so Import becomes available
            // without a mysterious extra click.
            (void)collectPlan();
            validatePreview();
        } catch (const std::exception& error) {
            const auto reason = juce::String::fromUTF8(error.what());
            status.setText("Not ready: " + reason, juce::dontSendNotification);
            previewText.setText("Import not available yet.\n" + reason +
                                "\n\nChoose Options for each selected source, set a destination "
                                "and at least one available field. Preview or Import "
                                "never changes the source file.", false);
        }
    }
    void rebuildSourceList() {
        rows.clear();
        const auto filter = search.getText().trim();
        for (std::size_t i = 0; i < scan.items.size(); ++i) {
            const auto& s = scan.items[i];
            if (filter.isNotEmpty() && !s.sourceItemTitle.containsIgnoreCase(filter) &&
                !juce::String(static_cast<int>(i + 1)).contains(filter))
                continue;
            auto row = std::make_unique<SourceRow>();
            row->selected.setButtonText(juce::String(static_cast<int>(i + 1)) + ". " + s.sourceItemTitle);
            row->selected.setTitle("Select source position " + juce::String(static_cast<int>(i + 1)));
            row->selected.setTooltip(s.sourceItemTitle + " — source position " +
                                     juce::String(static_cast<int>(i + 1)));
            row->selected.setToggleState(drafts[i].selected, juce::dontSendNotification);
            row->selected.setComponentID("live-show-import-item-" + juce::String(static_cast<int>(i)));
            row->configure.setToggleState(focused == static_cast<int>(i), juce::dontSendNotification);
            row->summary.setText("S" + juce::String(s.sourceSectionCount) + "  C" +
                                 juce::String(s.sourceChordCount), juce::dontSendNotification);
            row->summary.setTooltip(s.warnings.joinIntoString(" | "));
            auto* selectedToggleRow = row.get();
            row->selected.onClick = [this, i, selectedToggleRow] {
                drafts[i].selected = !drafts[i].selected;
                focused = static_cast<int>(i);
                syncConfiguration();
                for (auto& visible : rows)
                    visible->configure.setToggleState(visible.get() == selectedToggleRow,
                                                      juce::dontSendNotification);
                invalidatePreview();
            };
            auto* selectedRow = row.get();
            row->configure.onClick = [this, i, selectedRow] {
                focused = static_cast<int>(i);
                syncConfiguration();
                for (auto& visible : rows)
                    visible->configure.setToggleState(visible.get() == selectedRow,
                                                      juce::dontSendNotification);
            };
            sourceBody.addAndMakeVisible(*row);
            rows.push_back(std::move(row));
        }
        layoutSourceRows();
    }
    void layoutSourceRows() {
        const int width = juce::jmax(360, sources.getMaximumVisibleWidth());
        sourceBody.setSize(width, juce::jmax(sources.getHeight(),
                           static_cast<int>(rows.size()) * 39));
        for (std::size_t i = 0; i < rows.size(); ++i)
            rows[i]->setBounds(0, static_cast<int>(i) * 39, width, 39);
    }
    void syncConfiguration() {
        syncing = true;
        const bool hasSource = focused >= 0 && static_cast<std::size_t>(focused) < scan.items.size();
        if (hasSource) {
            const auto i = static_cast<std::size_t>(focused);
            const auto& source = scan.items[i];
            const auto& d = drafts[i];
            configureTitle.setText(juce::String(static_cast<int>(i + 1)) + ". " + source.sourceItemTitle +
                                   (d.selected ? "  [selected]" : "  [not selected]"), juce::dontSendNotification);
            int option = 0;
            if (d.createNew) option = 2;
            else for (std::size_t k = 0; k < destinationIds.size(); ++k)
                if (destinationIds[k] == d.targetItemId) option = static_cast<int>(k) + 3;
            target.setSelectedId(option, juce::dontSendNotification);
            sections.setToggleState(d.fields.sections, juce::dontSendNotification);
            tempo.setToggleState(d.fields.tempo, juce::dontSendNotification);
            chords.setToggleState(d.fields.chords, juce::dontSendNotification);
            length.setToggleState(d.fields.length, juce::dontSendNotification);
            sections.setEnabled(available(source, 0));
            tempo.setEnabled(available(source, 1));
            chords.setEnabled(available(source, 2));
            length.setEnabled(available(source, 3));
            juce::String detail = "Detected  |  Sections: " + juce::String(source.sourceSectionCount);
            if (source.sections.isArray()) detail += " (usable " + juce::String(source.sections.size()) + ")";
            detail += "  |  Chords: " + juce::String(source.sourceChordCount);
            if (source.chords.isArray()) detail += " (usable " + juce::String(source.chords.size()) + ")";
            detail += "\n";
            if (source.sourceSectionCount && !source.sections.isArray()) detail += "Sections unavailable: see warning below\n";
            if (source.sourceChordCount && !source.chords.isArray()) detail += "Chords unavailable: see warning below\n";
            detail += "Tempo: " + (source.bpm ? juce::String(*source.bpm, 2) + " BPM" : juce::String("unavailable"));
            detail += "  |  Meter: " + source.sourceMeter;
            detail += "  |  Length: " + (source.lengthBars ? juce::String(*source.lengthBars, 3) + " bars" :
                                                   juce::String("unavailable"));
            detail += "\n" + (source.warnings.isEmpty() ? juce::String("No source warnings") :
                                                               source.warnings.joinIntoString("\n"));
            previewText.setText(detail, false);
        } else {
            configureTitle.setText("Select a source item on the left", juce::dontSendNotification);
            target.setSelectedId(0, juce::dontSendNotification);
            for (auto* control : {&sections, &tempo, &chords, &length})
                control->setToggleState(false, juce::dontSendNotification);
            previewText.setText("Choose one or more SetList items. Duplicate titles are identified by position.", false);
        }
        target.setEnabled(hasSource);
        if (!hasSource) for (auto* control : {&sections, &tempo, &chords, &length})
            control->setEnabled(false);
        syncing = false;
    }
    std::vector<live::StudioOneImportItem> collectPlan() const {
        std::vector<live::StudioOneImportItem> result;
        std::set<juce::String> targets;
        for (std::size_t i = 0; i < drafts.size(); ++i) {
            const auto& d = drafts[i];
            if (!d.selected) continue;
            const auto identity = "Source #" + std::to_string(i + 1) + ": ";
            if (!d.fields.sections && !d.fields.tempo && !d.fields.chords && !d.fields.length)
                throw std::runtime_error(identity + "choose at least one available field");
            if (!d.createNew && d.targetItemId.isEmpty())
                throw std::runtime_error(identity + "choose an MR Live destination or Create New");
            if (!d.createNew && !targets.insert(d.targetItemId).second)
                throw std::runtime_error(identity + "destination already assigned to another source");
            result.push_back({static_cast<int>(i), d.targetItemId, d.createNew, d.fields});
        }
        if (result.empty()) throw std::runtime_error("Select at least one source Show item");
        return result;
    }
    void validatePreview() {
        previewValid = false;
        importButton.setEnabled(false);
        previewPlan.clear();
        try {
            if (!documentCurrent()) throw std::runtime_error("MR Live show changed: reopen import");
            if (owner->app.recording()) throw std::runtime_error("Stop recording before importing");
            const auto plan = collectPlan();
            auto candidate = originalState.clone();
            live::applyStudioOneImportPlan(candidate, scan, plan);
            juce::String explanation = "VALIDATED — " + juce::String(static_cast<int>(plan.size())) +
                                       " item(s); no project changes yet.\n\n";
            for (const auto& request : plan) {
                const auto& source = scan.items[static_cast<std::size_t>(request.sourceOrdinal)];
                explanation += juce::String(request.sourceOrdinal + 1) + ". " + source.sourceItemTitle + "\n";
                explanation += "  -> " + (request.createNewItem ? juce::String("[New Browser item — not added to Setlist]") :
                    live::find(originalState,"items",request.targetItemId)["name"].toString()) + "\n";
                explanation += "  Fields: ";
                if (request.fields.sections) explanation += "Sections (" + juce::String(source.sourceSectionCount) + ") ";
                if (request.fields.tempo) explanation += "Tempo (" + juce::String(*source.bpm, 2) + " BPM) ";
                if (request.fields.chords) explanation += "Chords (" + juce::String(source.sourceChordCount) + ") ";
                if (request.fields.length) explanation += "Length (" + juce::String(*source.lengthBars, 3) + " bars)";
                explanation += "\n";
                if (!source.warnings.isEmpty())
                    explanation += "  Other source warnings: " + source.warnings.joinIntoString("; ") + "\n";
                explanation += "\n";
            }
            previewText.setText(explanation, false);
            previewPlan = plan;
            previewValid = true;
            importButton.setEnabled(true);
            status.setText("Preview valid. Import applies all selected mappings in one Undo step.",
                           juce::dontSendNotification);
        } catch (const std::exception& error) {
            previewText.setText("PREVIEW BLOCKED\n" + juce::String::fromUTF8(error.what()), false);
            status.setText("Nothing changed. Correct the selection, then Preview again.",
                           juce::dontSendNotification);
        }
    }
    void commitImport() {
        if (!previewValid || !owner) return;
        bool success = false;
        auto plan = previewPlan;
        owner->run([&] {
            if (!documentCurrent()) throw std::runtime_error("MR Live show changed since preview");
            if (owner->app.recording()) throw std::runtime_error("Stop recording before importing");
            owner->editLiveInterface("Import Studio One Show", [this, &plan](juce::var& state) {
                if (!live::sameShowValue(state, originalState))
                    throw std::runtime_error("Show changed during Studio One import");
                live::applyStudioOneImportPlan(state, scan, plan);
            });
            success = true;
        });
        if (success) {
            if (close) close();
        } else if (owner) {
            previewValid = false;
            importButton.setEnabled(false);
            previewText.setText("IMPORT BLOCKED\n" + owner->message, false);
        }
    }
};

inline void openStudioOneShowImport(Desktop& desktop) {
    if (!desktop.liveDocument || desktop.app.recording()) return;
    // Always show the same persistent dialog. Restore last file if available,
    // or leave the picker visible if it was removed/moved/invalid.
    auto content = std::make_unique<LiveStudioOneImportDialog>(
        desktop, live::StudioOneShowScan{}, juce::String{});
    auto* dialog = content.get();
    dialog->restoreLastSource();
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "MR Live — Import Studio One Show";
    options.dialogBackgroundColour = juce::Colour(0xff202831);
    options.content.setOwned(content.release());
    options.componentToCentreAround = desktop.getTopLevelComponent();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    auto* window = options.launchAsync();
    juce::Component::SafePointer<juce::DialogWindow> safeWindow(window);
    dialog->close = [safeWindow] {
        if (safeWindow) safeWindow->exitModalState(0);
    };
}
} // namespace ui
