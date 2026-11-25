#include "DrumMapEditor.h"
#include "DrumMapPresentation.h"
#include "Controls.h"
#include <mrs/drum_maps.hpp>
#include <mrs/persistence.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <stdexcept>

namespace ui {
namespace {
namespace dm = mrs::persistence;
using namespace juce;
String text(const std::string& s) { return String::fromUTF8(s.c_str()); }
std::filesystem::path path(const File& file) { return std::filesystem::path(file.getFullPathName().toWideCharPointer()); }

class Editor final : public Component, private ListBoxModel, private Timer {
public:
    Editor(std::filesystem::path library, std::function<bool(int, int)> audition, std::optional<mrs::DrumMapProfile> initial = {}, std::function<int()> feedback = {})
        : library_(std::move(library)), audition_(std::move(audition)), feedback_(std::move(feedback)) {
        setLookAndFeel(&theme_);
        if (feedback_) { feedback_(); startTimerHz(30); }
        setComponentID("drum-map-editor");
        setSize(880, 640);
        for (auto* button : {&new_, &import_, &duplicate_, &save_, &discard_, &export_, &delete_, &addGroup_, &addInstrument_,
                             &addArticulation_, &removeRow_, &up_, &down_, &apply_, &listen_}) addAndMakeVisible(*button);
        for (auto* component : std::array<Component*, 7>{&profile_, &list_, &rowName_, &note_, &channel_, &aliases_, &parent_})
            addAndMakeVisible(*component);
        addAndMakeVisible(status_);
        addAndMakeVisible(fullHierarchy_);
        fullHierarchy_.setComponentID("drum-map-full-hierarchy");
        fullHierarchy_.onClick = [this] {
            const bool previous = showFullHierarchy_;
            run([&] { applyRow(); const auto id = currentId(); showFullHierarchy_ = fullHierarchy_.getToggleState(); rebuild(id); });
            fullHierarchy_.setToggleState(showFullHierarchy_, dontSendNotification);
            if (showFullHierarchy_ != previous) updateStatus(showFullHierarchy_ ? "All groups and instrument folders shown." : "Redundant instrument folders hidden; the map is unchanged.");
        };
        list_.setTitle("Groups, instruments and articulations"); list_.setRowHeight(26); list_.setModel(this);
        profile_.setTitle("Saved profiles"); parent_.setTitle("Parent group or instrument");
        profile_.setTextWhenNothingSelected("Select a saved profile");
        const auto field = [](TextEditor& editor, const String& caption) {
            editor.setTitle(caption); editor.setTextToShowWhenEmpty(caption, Colour(0xffaab6c5));
        };
        for (std::size_t j = 0; j < fields_.size(); ++j) {
            addAndMakeVisible(fields_[j]); field(fields_[j], captions_[j]);
        }
        for (auto& editor : fields_) editor.setMultiLine(true, false);
        rowName_.setMultiLine(true, false); aliases_.setMultiLine(true, false);
        field(rowName_, "Row name"); field(note_, "MIDI notes (first = default)");
        field(channel_, "Channel 1-16 (blank: any)"); field(aliases_, "Aliases separated by | (optional)");
        status_.setJustificationType(Justification::topLeft);
        profile_.onChange = [this] { switchProfile(); };
        new_.onClick = [this] { run([&] { requireClean(); mrs::DrumMapProfile p; p.id = mrs::new_id(); p.name = "New drum map"; open(std::move(p), false); }); };
        import_.onClick = [this] { run([&] { requireClean(); choose(false); }); };
        duplicate_.onClick = [this] { run([&] { requireClean(); open(mrs::duplicate_drum_map(*draft_, draft_->name + " Copy"), false); }); };
        save_.onClick = [this] { run([&] {
            applyRow();
            auto next = withFields(); next.validate();
            if (saved_ && dm::load_drum_map(library_, saved_->id) != saved_)
                throw std::runtime_error("Profile changed on disk. Discard and reopen before editing.");
            if (!saved_ && dm::load_drum_map(library_, next.id)) next.id = mrs::new_id();
            dm::save_drum_map(library_, next, saved_.has_value());
            const auto id = currentId();
            open(std::move(next), true); reload(); rebuild(id); updateStatus("Saved.");
        }); };
        discard_.onClick = [this] { run([&] { confirmDiscard(); }); };
        export_.onClick = [this] { run([&] { requireClean(); choose(true); }); };
        delete_.onClick = [this] { run([&] { requireClean(); confirmDelete(); }); };
        for (auto* button : {&up_, &down_}) button->onClick = [this, button] { run([&] { move(button == &up_ ? -1 : 1); }); };
        addGroup_.onClick = [this] { run([&] { add(0); }); };
        addInstrument_.onClick = [this] { run([&] { add(1); }); };
        addArticulation_.onClick = [this] { run([&] { add(2); }); };
        removeRow_.onClick = [this] { run([&] { confirmRemove(); }); };
        apply_.onClick = [this] { run([&] { applyRow(); updateStatus("Row applied to draft. Use Save / Rename to store it."); }); };
        listen_.onClick = [this] { run([&] { const auto n = defaultNote(); const auto c = assignment(channel_, 1, 16);
            if (!n || !audition_ || !audition_(*n, c.value_or(0))) throw std::runtime_error("Audition needs an available instrument track and a MIDI note.");
        }); };
        run([&] {
            reload();
            if (initial) { const auto stored = dm::load_drum_map(library_, initial->id); const bool same = stored == initial; open(std::move(initial), same); }
            else if (!catalog_.entries.empty()) open(dm::load_drum_map(library_, catalog_.entries.front().id), true);
            else open({}, false);
        });
    }
    ~Editor() override { stopTimer(); list_.setModel(nullptr); setLookAndFeel(nullptr); }
    void requestClose(std::function<void()> close) {
        if (draft_ && (!saved_ || withFields() != *saved_ || rowDirty())) {
            confirm("Discard unsaved Drum Map?", "Use Cancel to return to the editor and save your changes.",
                [close = std::move(close)](Editor&) { close(); });
        } else close();
    }
    void smoke(const mrs::DrumMapProfile& base) {
        const auto check = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
        auto compact = base;
        compact.categories.front().name = "Kick";
        auto& groups = compact.categories;
        groups.push_back({{"hh-group"}, "Hi-Hat", {{{"hh-instrument"}, "HH", {{{"hh-hit"}, "CloseT", 30, {}, {}}}}}});
        groups.push_back({{"toms-group"}, "Toms", {{{"tom-instrument"}, "Tom Hi", {{{"tom-hit"}, "Hit", 48, {}, {}}}}}});
        open(compact, false);
        check(rows_.size() == 8 && rows_[1].compact && rows_[4].compact && !rows_.back().compact, "Hide only redundant folders");
        list_.selectRow(1);
        check(parent_.getSelectedId() > 0, "Hidden instrument remains editable as an articulation parent");
        fullHierarchy_.setToggleState(true, dontSendNotification); fullHierarchy_.onClick();
        check(rows_.size() == 10 && currentId() == compact.categories.front().instruments.front().articulations.front().id && draft_ == compact,
              "Full hierarchy restores folders without touching the draft");
        fullHierarchy_.setToggleState(false, dontSendNotification); fullHierarchy_.onClick();
        open(base, false); save_.onClick();
        check(saved_ == base && duplicate_.isEnabled(), "Save imported draft");
        duplicate_.onClick(); fields_[0].setText("Sticks"); list_.selectRow(2);
        note_.setText("38, 40"); list_.selectRow(1);
        check(list_.getSelectedRow() == 2 && note_.getText() == "38, 40", "Keep pending row on selection change");
        apply_.onClick(); listen_.onClick(); save_.onClick();
        check(saved_ && saved_->name == "Sticks" && saved_->id != base.id && art(*selectedRow()).note == 38 && art(*selectedRow()).alternate_notes == std::vector<int>{40}, "Save independent Sticks with alternate note");
        const auto sticks = *saved_;
        open(base, true); duplicate_.onClick(); fields_[0].setText("Brushes"); list_.selectRow(2); note_.clear(); save_.onClick();
        check(saved_ && saved_->name == "Brushes" && saved_->id != base.id && !art(*selectedRow()).note, "Save independent Brushes");
        check(dm::load_drum_map(library_, base.id) == base && dm::load_drum_map(library_, sticks.id) == sticks, "Base and Sticks stay unchanged");
        const auto image = createComponentSnapshot(getLocalBounds(), true, 1.0f, SoftwareImageType{});
        const auto file = File::getCurrentWorkingDirectory().getChildFile("drum-map-editor-smoke.png");
        auto output = file.createOutputStream();
        check(output && output->setPosition(0) && output->truncate().wasOk() && PNGImageFormat().writeImageToStream(image, *output), "Write editor snapshot");
    }
    void paint(Graphics& g) override { g.fillAll(Colour(0xff212630)); }
    void resized() override {
        auto area = getLocalBounds().reduced(14);
        auto header = area.removeFromTop(30); profile_.setBounds(header.removeFromLeft(250));
        for (auto* b : {&new_, &import_, &duplicate_, &save_, &discard_, &export_, &delete_}) {
            header.removeFromLeft(5); b->setBounds(header.removeFromLeft(b == &save_ ? 100 : 72));
        }
        area.removeFromTop(8);
        auto meta1 = area.removeFromTop(28), meta2 = area.removeFromTop(28);
        fields_[0].setBounds(meta1.removeFromLeft(282).reduced(1)); fields_[1].setBounds(meta1.reduced(1));
        for (std::size_t j = 2; j < fields_.size(); ++j)
            fields_[j].setBounds((j == fields_.size() - 1 ? meta2 : meta2.removeFromLeft((getWidth() - 28) / 5)).reduced(1));
        area.removeFromTop(8); status_.setBounds(area.removeFromBottom(58));
        auto left = area.removeFromLeft(area.getWidth() * 3 / 5);
        auto controls = left.removeFromBottom(30);
        for (auto* b : {&addGroup_, &addInstrument_, &addArticulation_, &up_, &down_, &removeRow_})
            b->setBounds(controls.removeFromLeft(b == &addArticulation_ ? 110 : b == &addInstrument_ ? 99 : 69).reduced(1));
        list_.setBounds(left.reduced(0, 2));
        area.removeFromLeft(12);
        for (auto* c : std::array<Component*, 5>{&rowName_, &parent_, &note_, &channel_, &aliases_})
            c->setBounds(area.removeFromTop(33).reduced(0, 3));
        auto actions = area.removeFromTop(34); apply_.setBounds(actions.removeFromLeft(95).reduced(2));
        listen_.setBounds(actions.removeFromLeft(95).reduced(2));
        fullHierarchy_.setBounds(area.removeFromTop(28));
    }
private:
    struct Row { mrs::Id id; int g{}, i{-1}, a{-1}; bool compact{}; };
    Theme theme_;
    std::filesystem::path library_;
    std::function<bool(int, int)> audition_;
    dm::DrumMapCatalog catalog_;
    std::optional<mrs::DrumMapProfile> draft_, saved_;
    std::vector<Row> rows_, headers_, parents_;
    ToggleButton fullHierarchy_{"Show full hierarchy"};
    bool showFullHierarchy_{};
    std::vector<mrs::Id> conflicts_;
    std::unique_ptr<FileChooser> chooser_;
    ComboBox profile_, parent_;
    int shownRow_{-1};
    bool rebuilding_{};
    ListBox list_;
    Label status_;
    std::array<TextEditor, 7> fields_;
    const std::array<String, 7> captions_{"Name", "Description", "Vendor", "Plug-in", "Library", "Kit", "Technique"};
    TextEditor rowName_, note_, channel_, aliases_;
    TextButton new_{"New"}, import_{"Import..."}, duplicate_{"Duplicate"}, save_{"Save / Rename"}, discard_{"Discard"}, export_{"Export..."}, delete_{"Delete"};
    TextButton addGroup_{"+ Group"}, addInstrument_{"+ Instrument"}, addArticulation_{"+ Articulation"}, up_{"Up"}, down_{"Down"}, removeRow_{"Remove"}, apply_{"Apply row"}, listen_{"Audition"};

    template<class F> void run(F&& f) {
        try { f(); } catch (const std::exception& e) { updateStatus(text(e.what())); }
    }
    static std::optional<int> assignment(const TextEditor& editor, int low, int high) {
        const auto s = editor.getText().trim().toStdString();
        if (s.empty()) return {};
        int result{}; const auto end = s.data() + s.size(); const auto r = std::from_chars(s.data(), end, result);
        if (r.ec != std::errc{} || r.ptr != end || result < low || result > high)
            throw std::invalid_argument("Use a numeric MIDI note 0-127 and channel 1-16, or leave blank.");
        return result - low;
    }
    static auto fieldPointers(mrs::DrumMapProfile& p) {
        return std::array{&p.name, &p.description, &p.vendor, &p.plugin, &p.library, &p.kit, &p.technique};
    }
    mrs::DrumMapProfile withFields() const {
        auto p = *draft_;
        const auto pointers = fieldPointers(p);
        for (std::size_t j = 0; j < fields_.size(); ++j) *pointers[j] = fields_[j].getText().toStdString();
        return p;
    }
    void requireClean() const {
        if (draft_ && (!saved_ || withFields() != *saved_ || rowDirty()))
            throw std::runtime_error("Save or discard the current draft first.");
    }
    void reload() { catalog_ = dm::list_drum_maps(library_); syncProfiles(); }
    void syncProfiles() {
        profile_.clear(dontSendNotification);
        int selected = 0;
        for (std::size_t j = 0; j < catalog_.entries.size(); ++j) {
            const auto& entry = catalog_.entries[j]; const auto index = static_cast<int>(j + 1);
            profile_.addItem(text(entry.name), index);
            if (saved_ && entry.id == saved_->id) selected = index;
        }
        profile_.setSelectedId(selected, dontSendNotification);
    }
    void switchProfile() {
        const auto index = profile_.getSelectedItemIndex();
        run([&] { requireClean(); if (index >= 0) {
            auto profile = dm::load_drum_map(library_, catalog_.entries[static_cast<std::size_t>(index)].id);
            if (!profile) throw std::runtime_error("Profile no longer exists.");
            open(std::move(profile), true);
        } });
        syncProfiles();
    }
    void open(std::optional<mrs::DrumMapProfile> next, bool persisted) {
        draft_ = std::move(next); saved_ = persisted ? draft_ : std::nullopt;
        for (std::size_t j = 0; j < fields_.size(); ++j) {
            fields_[j].setEnabled(draft_.has_value());
            fields_[j].setText(draft_ ? text(*fieldPointers(*draft_)[j]) : String{}, false);
        }
        save_.setEnabled(draft_.has_value()); discard_.setEnabled(draft_.has_value());
        duplicate_.setEnabled(saved_.has_value()); export_.setEnabled(saved_.has_value()); delete_.setEnabled(saved_.has_value());
        addGroup_.setEnabled(draft_.has_value());
        syncProfiles(); rebuild({});
        updateStatus(draft_ && !saved_ ? "New draft. Save / Rename adds it to the library." : "CSV/TSV: midi_note, instrument, articulation, group; optional channel. Blank note = Unmapped.");
    }
    const Row* selectedRow() const {
        const auto n = list_.getSelectedRow(); return n >= 0 && static_cast<std::size_t>(n) < rows_.size() ? &rows_[static_cast<std::size_t>(n)] : nullptr;
    }
    std::optional<mrs::Id> currentId() const { if (const auto* r = selectedRow()) return r->id; return {}; }
    std::string rowLabel(const Row& r) const {
        const auto& g = draft_->categories[static_cast<std::size_t>(r.g)];
        if (r.i < 0) return g.name;
        const auto& i = g.instruments[static_cast<std::size_t>(r.i)];
        return r.a < 0 ? i.name : i.articulations[static_cast<std::size_t>(r.a)].name;
    }
    const mrs::DrumMapArticulation& art(const Row& r) const {
        return draft_->categories[static_cast<std::size_t>(r.g)].instruments[static_cast<std::size_t>(r.i)].articulations[static_cast<std::size_t>(r.a)];
    }
    void rebuild(const std::optional<mrs::Id>& selected) {
        rebuilding_ = true;
        rows_.clear(); headers_.clear(); conflicts_.clear();
        if (draft_) {
            conflicts_ = mrs::drum_map_conflicts(*draft_);
            int g = 0;
            for (const auto& group : draft_->categories) {
                rows_.push_back({group.id, g, -1, -1}); headers_.push_back(rows_.back()); int i = 0;
                const bool compact = !showFullHierarchy_ && compactDrumCategory(group);
                for (const auto& instrument : group.instruments) {
                    headers_.push_back({instrument.id, g, i, -1});
                    if (!compact) rows_.push_back(headers_.back());
                    int a = 0;
                    for (const auto& row : instrument.articulations) rows_.push_back({row.id, g, i, a++, compact});
                    ++i;
                }
                ++g;
            }
        }
        list_.updateContent(); list_.deselectAllRows();
        if (selected) for (std::size_t j = 0; j < rows_.size(); ++j)
            if (rows_[j].id == selected) { list_.selectRow(static_cast<int>(j)); break; }
        rebuilding_ = false; shownRow_ = -1;
        selectedRowsChanged(list_.getSelectedRow()); list_.repaint();
    }
    int getNumRows() override { return static_cast<int>(rows_.size()); }
    void paintListBoxItem(int index, Graphics& g, int w, int h, bool selected) override {
        if (index < 0 || index >= getNumRows()) return;
        const auto& r = rows_[static_cast<std::size_t>(index)];
        g.fillAll(Colour(selected ? 0xff375677 : r.i < 0 ? 0xff333d4b : r.a < 0 ? 0xff2a323f : 0xff242b36));
        if (flashed_ == r.id) { g.setColour(Colour(0xff65dacc)); g.fillRect(0, 0, 4, h); }
        g.setColour(Colours::whitesmoke); g.setFont(14.0f); const int x = r.i < 0 ? 8 : r.a < 0 || r.compact ? 24 : 40;
        auto label = text(rowLabel(r));
        if(r.a<0){const auto rgb=rowColor(r);if(rgb){g.setColour(Colour(0xff000000u|*rgb));g.fillRect(w-16,7,10,12);g.setColour(Colours::whitesmoke);}}
        if (r.a >= 0) {
            const auto& a = art(r);
            label += a.note ? "  |  " + String(*a.note) + (a.channel ? " / Ch " + String(*a.channel + 1) : " / Any") : "  |  Unmapped";
            if (std::find(conflicts_.begin(), conflicts_.end(), r.id) != conflicts_.end()) label += "  [Conflict]";
        }
        g.drawFittedText(label, x, 0, w - x - 6, h, Justification::centredLeft, 1);
    }
    std::function<int()> feedback_;
    std::optional<mrs::Id> flashed_;
    double flashUntil_{};
    void timerCallback() override {
        if (!isShowing()) { flashed_.reset(); return; }
        const int key = feedback_();
        if (key >= 0 && draft_) {
            const int pitch = key % 128, channel = key / 128;
            const mrs::DrumMapArticulation* match = nullptr;
            for (const auto& category : draft_->categories) for (const auto& instrument : category.instruments)
                for (const auto& a : instrument.articulations) {
                    if (a.channel && *a.channel != channel) continue;
                    if (a.note != pitch && std::find(a.alternate_notes.begin(), a.alternate_notes.end(), pitch) == a.alternate_notes.end()) continue;
                    if (!match || (!match->channel && a.channel)) match = &a;
                }
            if (match) {
                flashed_ = match->id;
                flashUntil_ = Time::getMillisecondCounterHiRes() + 600;
                const auto it = std::find_if(rows_.begin(), rows_.end(), [&](const auto& row) { return row.id == *flashed_; });
                if (it != rows_.end()) list_.scrollToEnsureRowIsOnscreen(static_cast<int>(it - rows_.begin()));
                list_.repaint();
            }
        }
        if (flashed_ && Time::getMillisecondCounterHiRes() >= flashUntil_) { flashed_.reset(); list_.repaint(); }
    }
    void listBoxItemClicked(int row, const MouseEvent& event) override {
        if (event.mods.isLeftButtonDown() && row == list_.getSelectedRow() && listen_.isEnabled()) listen_.onClick();
    }
    void selectedRowsChanged(int) override {
        if (rebuilding_) return;
        const auto index = list_.getSelectedRow();
        if (shownRow_ >= 0 && index != shownRow_ && rowDirty(&rows_[static_cast<std::size_t>(shownRow_)])) {
            rebuilding_ = true; list_.selectRow(shownRow_); rebuilding_ = false;
            updateStatus("Apply row or restore its values before changing selection.");
            return;
        }
        shownRow_ = index;
        const auto* r = selectedRow(); parents_.clear(); parent_.clear(dontSendNotification);
        const bool articulation = r && r->a >= 0, hasParent = r && r->i >= 0;
        rowName_.setText(r ? text(rowLabel(*r)) : String{}, false); rowName_.setEnabled(r != nullptr);
        const bool group = r && r->a < 0;
        const String noteCaption = group ? "Color #RRGGBB (blank: inherit)" : "MIDI notes (first = default)";
        note_.setTitle(noteCaption); note_.setTextToShowWhenEmpty(noteCaption, Colour(0xffaab6c5));
        note_.setEnabled(articulation || group); channel_.setEnabled(articulation); aliases_.setEnabled(articulation);
        String note, channel, aliases;
        if (articulation) {
            const auto& a = art(*r); note = noteText(a); if (a.channel) channel = String(*a.channel + 1);
            for (const auto& alias : a.aliases) { if (aliases.isNotEmpty()) aliases += " | "; aliases += text(alias); }
        }
        if (group) note = groupColorText(*r);
        // setText ignores notifications so selection never edits the model.
        note_.setText(note, false); channel_.setText(channel, false); aliases_.setText(aliases, false);
        if (hasParent) for (const auto& p : headers_) {
            if (articulation ? p.i < 0 || p.a >= 0 : p.i >= 0) continue;
            parents_.push_back(p); const auto n = static_cast<int>(parents_.size());
            const auto prefix = articulation ? text(draft_->categories[static_cast<std::size_t>(p.g)].name) + " / " : String{};
            parent_.addItem(prefix + text(rowLabel(p)), n);
            if (p.g == r->g && (!articulation || p.i == r->i)) parent_.setSelectedId(n, dontSendNotification);
        }
        parent_.setEnabled(hasParent);
        addInstrument_.setEnabled(r != nullptr); addArticulation_.setEnabled(hasParent || (r && !showFullHierarchy_ && compactDrumCategory(draft_->categories[static_cast<std::size_t>(r->g)])));
        for (auto* b : {&apply_, &up_, &down_, &removeRow_}) b->setEnabled(r != nullptr);
        listen_.setEnabled(articulation && static_cast<bool>(audition_));
    }
    std::optional<std::uint32_t> rowColor(const Row& r) const {
        const auto& category=draft_->categories[static_cast<std::size_t>(r.g)];
        return r.i<0?category.color:category.instruments[static_cast<std::size_t>(r.i)].color;
    }
    String groupColorText(const Row& r) const {
        const auto rgb=rowColor(r);
        return rgb ? "#"+String::toHexString(static_cast<int>(*rgb)).paddedLeft('0',6).toUpperCase() : String{};
    }
    std::optional<std::uint32_t> groupColor() const {
        auto input=note_.getText().trim().toStdString(); if(input.empty())return {};
        if(input.front()=='#')input.erase(0,1);
        std::uint32_t rgb{}; const auto end=input.data()+input.size(); const auto parsed=std::from_chars(input.data(),end,rgb,16);
        if(input.size()!=6 || parsed.ec!=std::errc{} || parsed.ptr!=end)throw std::invalid_argument("Use a six-digit group color, e.g. #F07468, or leave blank.");
        return rgb;
    }
    static String noteText(const mrs::DrumMapArticulation& a) {
        if (!a.note) return {};
        String result(*a.note);
        for (int alt : a.alternate_notes) result += ", " + String(alt);
        return result;
    }
    std::vector<int> parseNotes() const {
        auto rest = note_.getText().trim();
        if (rest.isEmpty()) return {};
        std::vector<int> result;
        for (;;) {
            const int comma = rest.indexOfChar(',');
            const auto token = (comma < 0 ? rest : rest.substring(0, comma)).trim().toStdString();
            int value{};
            const auto end = token.data() + token.size();
            const auto parsed = std::from_chars(token.data(), end, value);
            if (parsed.ec != std::errc{} || parsed.ptr != end || value < 0 || value > 127 ||
                std::find(result.begin(), result.end(), value) != result.end())
                throw std::invalid_argument("Use unique MIDI notes 0-127 separated by commas. First note is the default.");
            result.push_back(value);
            if (comma < 0) return result;
            rest = rest.substring(comma + 1);
        }
    }
    std::optional<int> defaultNote() const {
        const auto notes = parseNotes();
        if (notes.empty()) return {};
        return notes.front();
    }
    static String aliasText(const mrs::DrumMapArticulation& a) {
        StringArray result;
        for (const auto& alias : a.aliases) result.add(text(alias));
        return result.joinIntoString(" | ");
    }
    bool rowDirty() const { return rowDirty(selectedRow()); }
    bool rowDirty(const Row* r) const {
        if (!r) return false;
        if (rowName_.getText() != text(rowLabel(*r))) return true;
        const auto n = parent_.getSelectedItemIndex();
        if (n >= 0) { const auto& p = parents_[static_cast<std::size_t>(n)]; if (p.g != r->g || (r->a >= 0 && p.i != r->i)) return true; }
        if (r->a < 0) return note_.getText()!=groupColorText(*r);
        const auto& a = art(*r);
        return note_.getText() != noteText(a) ||
            channel_.getText() != (a.channel ? String(*a.channel + 1) : String{}) || aliases_.getText() != aliasText(a);
    }
    void updateStatus(const String& detail) {
        auto message = detail;
        if (draft_) {
            std::size_t unmapped = 0;
            for (const auto& g : draft_->categories) for (const auto& i : g.instruments)
                for (const auto& a : i.articulations) if (!a.note) ++unmapped;
            message += "\n" + String(static_cast<int>(unmapped)) + " unmapped | " + String(static_cast<int>(conflicts_.size())) + " conflicting articulations";
        }
        if (!catalog_.warnings.empty()) message += "\nLibrary skipped " + String(static_cast<int>(catalog_.warnings.size())) + " unreadable file(s).";
        status_.setText(message, dontSendNotification);
    }
    void commitRows(mrs::DrumMapProfile next, const std::optional<mrs::Id>& id) {
        next.validate(); draft_ = std::move(next); rebuild(id); updateStatus("Draft changed. Apply row before selecting another row; Save / Rename stores the profile.");
    }
    void applyRow() {
        if (!rowDirty()) return;
        const auto r = *selectedRow();
        auto next = *draft_;
        auto& group = next.categories[static_cast<std::size_t>(r.g)];
        const auto name = rowName_.getText().trim().toStdString();
        const auto parentIndex = parent_.getSelectedItemIndex();
        const auto destination = parentIndex >= 0 ? parents_[static_cast<std::size_t>(parentIndex)] : r;
        if (r.i < 0) { group.name = name; group.color=groupColor(); }
        else {
            auto& instrument = group.instruments[static_cast<std::size_t>(r.i)];
            if (r.a < 0) {
                instrument.name = name; instrument.color=groupColor();
                if (destination.g != r.g) {
                    auto moved = std::move(instrument); group.instruments.erase(group.instruments.begin() + r.i);
                    next.categories[static_cast<std::size_t>(destination.g)].instruments.push_back(std::move(moved));
                }
            } else {
                auto& a = instrument.articulations[static_cast<std::size_t>(r.a)];
                a.name = name; { auto notes = parseNotes(); a.note = notes.empty() ? std::optional<int>{} : std::optional<int>{notes.front()}; if (!notes.empty()) notes.erase(notes.begin()); a.alternate_notes = std::move(notes); } a.channel = assignment(channel_, 1, 16);
                if (aliases_.getText() != aliasText(a)) {
                    a.aliases.clear(); StringArray aliases; aliases.addTokens(aliases_.getText(), "|", "");
                    for (const auto& alias : aliases) if (alias.trim().isNotEmpty()) a.aliases.push_back(alias.trim().toStdString());
                }
                if (destination.g != r.g || destination.i != r.i) {
                    auto moved = std::move(a); instrument.articulations.erase(instrument.articulations.begin() + r.a);
                    next.categories[static_cast<std::size_t>(destination.g)].instruments[static_cast<std::size_t>(destination.i)].articulations.push_back(std::move(moved));
                }
            }
        }
        commitRows(std::move(next), r.id);
    }
    void add(int kind) {
        applyRow(); auto next = *draft_; const auto id = mrs::new_id();
        if (kind == 0) next.categories.push_back({id, "New group", {}});
        else {
            const auto* r = selectedRow(); if (!r) return;
            auto& group = next.categories[static_cast<std::size_t>(r->g)];
            if (kind == 1) group.instruments.push_back({id, "New instrument", {}});
            else if (r->i >= 0) group.instruments[static_cast<std::size_t>(r->i)].articulations.push_back({id, "New articulation", {}, {}, {}});
            else if (!showFullHierarchy_ && compactDrumCategory(group)) group.instruments.front().articulations.push_back({id, "New articulation", {}, {}, {}});
        }
        commitRows(std::move(next), id); rowName_.grabKeyboardFocus(); rowName_.selectAll();
    }
    void move(int delta) {
        applyRow(); const auto r = *selectedRow(); auto next = *draft_;
        const auto swap = [delta](auto& list, int index) {
            if (index + delta >= 0 && index + delta < static_cast<int>(list.size()))
                std::iter_swap(list.begin() + index, list.begin() + index + delta);
        };
        auto& g = next.categories[static_cast<std::size_t>(r.g)];
        if (r.i < 0) swap(next.categories, r.g);
        else if (r.a < 0) swap(g.instruments, r.i);
        else swap(g.instruments[static_cast<std::size_t>(r.i)].articulations, r.a);
        commitRows(std::move(next), r.id);
    }
    void confirm(const String& title, const String& detail, std::function<void(Editor&)> action) {
        Component::SafePointer<Editor> safe(this);
        AlertWindow::showOkCancelBox(MessageBoxIconType::QuestionIcon, title, detail, "Confirm", "Cancel", this,
            ModalCallbackFunction::create([safe, action = std::move(action)](int ok) { if (safe && ok) safe->run([&] { action(*safe.getComponent()); }); }));
    }
    void confirmRemove() {
        const auto r = *selectedRow();
        confirm("Remove row?", "This removes the selected row and any children from the draft.", [r](Editor& e) {
            auto next = *e.draft_; auto& g = next.categories[static_cast<std::size_t>(r.g)];
            if (r.i < 0) next.categories.erase(next.categories.begin() + r.g);
            else if (r.a < 0) g.instruments.erase(g.instruments.begin() + r.i);
            else { auto& a = g.instruments[static_cast<std::size_t>(r.i)].articulations; a.erase(a.begin() + r.a); }
            e.commitRows(std::move(next), {});
        });
    }
    void confirmDiscard() {
        confirm("Discard draft?", "Changes not saved to the library will be lost.", [](Editor& e) {
            if (e.saved_) {
                auto latest = dm::load_drum_map(e.library_, e.saved_->id); const auto persisted = latest.has_value();
                e.open(std::move(latest), persisted);
            } else e.open({}, false);
            e.reload();
        });
    }
    void confirmDelete() {
        const auto expected = *saved_;
        confirm("Delete saved profile?", "Existing projects retain their embedded map. The user-library copy will be removed.", [expected](Editor& e) {
            if (dm::load_drum_map(e.library_, expected.id) != expected) throw std::runtime_error("Profile changed on disk; reopen it first.");
            dm::delete_drum_map(e.library_, expected.id); e.open({}, false); e.reload(); e.updateStatus("Deleted from user library.");
        });
    }
    void choose(bool exporting) {
        chooser_ = std::make_unique<FileChooser>(exporting ? "Export Drum Map" : "Import one Drum Map", File{},
            exporting ? "*.mrsdrummap" : "*.mrsdrummap;*.csv;*.tsv", true, false, this);
        const auto mode = exporting ? FileBrowserComponent::saveMode : FileBrowserComponent::openMode;
        Component::SafePointer<Editor> safe(this);
        chooser_->launchAsync(mode | FileBrowserComponent::canSelectFiles, [safe, exporting](const FileChooser& chooser) {
            if (!safe || chooser.getResult() == File{}) return;
            safe->run([&] {
                if (exporting) {
                    auto file = chooser.getResult(); if (file.getFileExtension().isEmpty()) file = file.withFileExtension(".mrsdrummap");
                    dm::export_drum_map_file(path(file), *safe->saved_); safe->updateStatus("Map exported.");
                } else {
                    auto draft = dm::import_drum_map_file(path(chooser.getResult()));
                    // Import is always a new profile, never an update to an existing one.
                    draft.id = mrs::new_id(); safe->open(std::move(draft), false);
                }
            });
        });
    }
};
class DrumMapWindow final : public DocumentWindow {
public:
    DrumMapWindow(Component& owner, std::filesystem::path library, std::function<bool(int, int)> audition, std::optional<mrs::DrumMapProfile> initial, std::function<int()> feedback)
        : DocumentWindow("Drum Maps", Colour(0xff212630), DocumentWindow::closeButton), editor_(new Editor(std::move(library), std::move(audition), std::move(initial), std::move(feedback))) {
        setUsingNativeTitleBar(true);
        setAlwaysOnTop(true);
        setResizable(false, false);
        setContentOwned(editor_, true);
        centreAroundComponent(&owner, getWidth(), getHeight());
    }
    void closeButtonPressed() override {
        Component::SafePointer<DrumMapWindow> safe(this);
        editor_->requestClose([safe] { if (safe) { safe->setVisible(false);  } });
    }
private:
    Editor* editor_;
};
} // namespace

std::unique_ptr<juce::DocumentWindow> makeDrumMapEditor(Component& owner, std::filesystem::path library,
        std::function<bool(int, int)> audition, std::optional<mrs::DrumMapProfile> initial, std::function<int()> feedback) {
    return std::make_unique<DrumMapWindow>(owner, std::move(library), std::move(audition), std::move(initial), std::move(feedback));
}
void drumMapEditorSmoke() {
    const auto root = File::getSpecialLocation(File::tempDirectory).getChildFile("mrs-drum-map-smoke-" + text(mrs::new_id().value));
    if (root.exists() || root.createDirectory().failed()) throw std::runtime_error("Cannot create drum map fixture directory");
    struct Cleanup { File root; ~Cleanup() { root.deleteRecursively(); } } cleanup{root};
    const auto csv = root.getChildFile("Base.csv");
    if (!csv.replaceWithText("midi_note,instrument,articulation,group,channel\n36,Kick,Open,Kicks,10\n36,Kick,Hit,Kicks,10\n"))
        throw std::runtime_error("Cannot write drum map fixture");
    int played = -1;
    Editor editor(path(root.getChildFile("Library")), [&](int note, int channel) { played = channel == 9 ? note : -1; return true; });
    editor.smoke(dm::import_drum_map_file(path(csv)));
    if (played != 38) throw std::runtime_error("Unexpected audition note or channel");
}
} // namespace ui