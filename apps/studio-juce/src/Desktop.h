#pragma once
#include "Controls.h"
#include "TrackHeaderLayout.h"
#include "MRControls.h"
#include "SendLevel.h"
#include "Settings.h"
#include "Playhead.h"
#include "LiveDocument.h"
#include <mrs/desktop.hpp>
#include <mrs/midi_import.hpp>
#include <mrs/diagnostics.hpp>
#include <mrs/offline_device.hpp>
#include <mrs/vst3.hpp>
#include <future>
#include <set>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace ui {
namespace kit { class MRNativeParameterHost; }
class Desktop;
class Arrangement;
class ChordTrack;
class Strip;
class TrackFolderHeader;
class Browser;
class LibraryFoundationPanel;
class FxPanel;
class ProjectHome;
class LiveWorkspace;
class EditorWindow final : public juce::DocumentWindow {
public:
    EditorWindow(juce::String,juce::Component*,bool show=true);
    void closeButtonPressed() override;
    void fitNativeEditor(int,int);
    void setOwner(juce::Component&);
    std::function<void()> onClose;
};
class Desktop final : public juce::Component, private juce::Timer,
                      public juce::MenuBarModel, public juce::DragAndDropContainer, public juce::DragAndDropTarget {
public:
    explicit Desktop(bool testing=false,bool startAtHome=false);
    ~Desktop() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool workspaceShortcut(const juce::KeyPress&);
    bool keyStateChanged(bool) override;
    void spaceKey(bool);
    void focusLost(FocusChangeType) override;
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int,const juce::String&) override;
    void menuItemSelected(int,int) override;
    void action(int);
    void navigationMenu(bool sections);
    juce::ComboBox navigationMode;
    juce::PopupMenu channelMenu(std::optional<mrs::Id>);
    void showChannelMenu(std::optional<mrs::Id>,juce::Point<int>);
    void showFolderMenu(mrs::Id,juce::Point<int>);
    void selectTrack(mrs::Id,juce::ModifierKeys);
    std::vector<mrs::Id> folderSelection(std::optional<mrs::Id> fallback={}) const;
    void run(std::function<void()>);
    void refresh(bool force=false);
    void choose(int,std::function<void(const juce::File&)>,juce::String pattern="*");
    void textDialog(juce::String,juce::String,std::function<void(juce::String)>);
    void renameTrack(mrs::Id);
    void editClipStretch(mrs::Id);
    void renameClips(std::vector<mrs::Id>);
    void confirmDiscard(std::function<void()>,bool songOnly=false);
    void openFile(const juce::File&);
    void saveFile(const juce::File&);
    void createLive(const juce::File&);
    void selectLiveProject(int);
    void addLiveProject(const juce::File&);
    void importLiveShow(const juce::File&);
    void selectLiveItem(const juce::String&);
    void bindLiveItem(const juce::String&,const juce::File&);
    void prepareLiveInterface(bool saved);
    void updateLiveDirty();
    void editLiveInterface(const juce::String&,std::function<void(juce::var&)>);
    void applyLiveInterface(const juce::var&);
    void undoLiveInterface(bool redo=false);
    juce::UndoManager liveUndo{32*1024*1024,1};
    juce::String liveSavedContents;
    std::uint64_t liveUiRevision{};
    bool liveShowEditing{};
    juce::File documentFile() const;
    bool songDirty() const;
    bool documentDirty() const{return liveDirty||songDirty();}
    std::optional<std::pair<std::uint64_t,std::uint64_t>> liveEmptyRevision;
    std::optional<LiveDocument> liveDocument;
    juce::File liveFile;
    bool liveDirty{},liveLayoutActive{};
    std::unique_ptr<LiveWorkspace> liveWorkspace;
    void showProjectHome(bool);
    bool projectHomeVisible() const{return homeVisible;}
    bool canReturnToProject() const{return projectSessionStarted;}
    double visualSample();
    mrs::audio::RealtimeState displayState() const noexcept;
    double visualTick();
    void importFiles(const juce::StringArray&);
    void importMidiFiles(const juce::StringArray&,std::optional<mrs::Id>,mrs::Tick);
    void resetDevice();
    void reconnectDevice(bool session=false);
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void dropTrack(const juce::String&,std::size_t boundary);
    void saveSettings();
    bool shortcutAllowed() const;
    bool shortcutAllowedFor(juce::Component*) const;
    ViewSettings view;
    bool knifeMode{}; // transient editing tool, never persisted
    void setKnifeMode(bool enabled);
    void closeEditors(bool keepMidi=false);
    void refreshInsertEditors();
    void closeUnpinnedEditors();
    void audioSettings();
    void aiPermissions();
    void aiAssistant();
    void setAiPermissions(mrs::ai::PermissionPolicy);
    void showProfiling();
    void openMidiClip(mrs::Id);
    void openClipEditor(mrs::Id);
    void openSelectedEditor();
    void toggleEditor();
    bool clipEditorOpen() const;
    void activateClip(mrs::Id);
    void attachClipEditor(juce::Component*,mrs::Id,bool,bool persist=true);
    std::unique_ptr<juce::Component> dockedEditor;
    std::optional<mrs::Id> dockedClip;
    std::vector<mrs::Clip> recordingPreview;
    std::vector<mrs::desktop::AudioRecordingPreview> audioRecordingPreview;
    void updateRecordingPreview();
    void insertMenu(std::optional<mrs::Id>);
    void insertContextMenu(std::optional<mrs::Id>,mrs::Id,juce::Point<int>);
    void applyInsertChoice(std::optional<mrs::Id>,mrs::Id,int);
    void openInsert(std::optional<mrs::Id>,mrs::Id);
#ifdef MRS_HAS_ARA
    struct AraCandidate { std::string module; mrs::ara::Capability capability; };
    std::vector<AraCandidate> araCatalog;
    std::filesystem::path araCatalogFile;
    bool araCatalogScanned{},araScanSingle{};
    void loadAraCatalog();
    void publishAraTools();
    void saveAraCatalog();
    std::future<std::vector<AraCandidate>> araScanner;
    std::shared_ptr<std::atomic<bool>> araScanCancel{std::make_shared<std::atomic<bool>>(false)};
    std::optional<mrs::Id> araScanClip;
    std::shared_ptr<mrs::IProjectStore> araScanStore;
    std::shared_ptr<mrs::IProjectStore> araPreparationStore;
    std::uint64_t araPreparationModel{},araPreparationContent{};
    double araEditTime{};
    bool araPreparationAttempted{};
    void prepareAraWhenIdle(double now,bool interacting);
    struct AraPluginDrag {
        std::shared_ptr<mrs::IProjectStore> store;
        std::uint64_t revision{};
        mrs::processing::VstPlugin plugin;
        juce::String description;
        std::string generation;
        bool araOnly{};
    };
    struct AraDrop { AraPluginDrag drag; mrs::Clip clip; };
    std::optional<AraPluginDrag> araPluginDrag;
    std::optional<AraDrop> araDropPending;
    juce::var beginAraPluginDrag(mrs::processing::VstPlugin,bool araOnly,std::size_t index);
    bool isAraPluginDrag(const juce::var&) const;
    bool araDropCurrent(const AraDrop&) const;
    void dropAraPlugin(mrs::Id,const juce::var&);
    void applyAraDrop(AraDrop);
    juce::PopupMenu araMenu(mrs::Id) const;
    juce::PopupMenu araPluginsMenu(mrs::Id) const;
    void addAraMenus(juce::PopupMenu&,mrs::Id) const;
    void araChoice(mrs::Id,int);
    void scanAra(mrs::Id,std::optional<juce::File> file = {});
    void pollAra();
    void openAra(mrs::Id,bool activate=true);
    void followAra(mrs::Id);
    void attachAra(juce::Component*,bool);
    bool araEditorAttached{};
    void closeAra(const std::string&);
#endif
    void savePreset(std::optional<mrs::Id>,mrs::Id);
    void loadPreset(std::optional<mrs::Id>,mrs::Id);
    void refreshPresetLists();
    void resizeBrowser(int);
    void resizeMixer(int);
    void resizeSendArea(int);
    int sendAreaHeight{-1}; // Transient shared mixer divider; not project state.
    int mixerInsertHeight() const;
    void updatePerformance(const mrs::audio::DeviceStatus&,bool hardware);
    juce::String projectCaption() const;
    void loadPresetFile(std::optional<mrs::Id>,mrs::Id,const juce::File&);
    juce::PopupMenu pluginMenu(int base=10000) const;
    void addPlugin(std::optional<mrs::Id>,std::size_t,std::optional<std::size_t> position={});
    void addInstrument(std::size_t);
    void dropPlugin(std::optional<mrs::Id>,std::size_t,std::optional<std::size_t> position={});
    void replaceInstrument(mrs::Id,std::size_t);
    void routeMenu(mrs::Id);
    void sendsMenu(mrs::Id,std::optional<mrs::Id> bus={},juce::Component* anchor=nullptr);
    juce::PopupMenu sendMenu(mrs::Id,std::optional<mrs::Id>,std::vector<mrs::Id>&) const;
    void applySendChoice(mrs::Id,std::optional<mrs::Id>,int,const std::vector<mrs::Id>&);
    std::vector<mrs::NativeInsert> chain(std::optional<mrs::Id>) const;
    void applyChain(std::optional<mrs::Id>,std::vector<mrs::NativeInsert>);
    void toggleInsertPower(std::optional<mrs::Id>);
    bool insertDropAllowed(std::optional<mrs::Id>) const;
    struct InsertDrag {std::shared_ptr<mrs::IProjectStore> store;std::uint64_t revision{};std::optional<mrs::Id> source;mrs::Id slot;};
    std::optional<InsertDrag> insertDrag;
    std::map<std::string,std::map<std::string,bool>> insertPowerRecall;
    std::shared_ptr<mrs::IProjectStore> insertPowerProject;
    void scan(const juce::File&);
    void setWorkspace(mrs::desktop::Workspace);
    void setShellDesign(bool);
    void setAssetDesign(bool);
    const ConceptSkin* assetSkin()const{return assetTheme?assetTheme->conceptSkin():nullptr;}
    const TrackHeaderLayout* trackLayout()const noexcept { return trackHeaderLayout ? &*trackHeaderLayout : nullptr; }
    bool enableTrackHeaderLayout(const juce::File&, juce::String& error);
    void clearTrackHeaderLayout();
    void restoreBundledTrackHeaderLayout();
    void pollTrackHeaderLayout();
    bool isTrackHeaderReloadEnabled() const noexcept { return trackHeaderHotReload; }
    Theme* assetLookAndFeel()const{return assetTheme.get();}
    juce::Colour selectionColour()const{return assetTheme?assetTheme->role(ColourRole::focus,ConceptSkin::selection):juce::Colour(accent);}
    bool shellDesignEnabled()const{return shellTheme!=nullptr;}
    bool busyGesture() const;
    std::shared_ptr<const mrs::Project> project() const {return app.services().projects->state().project;}
    void setPreview(std::optional<mrs::Id>,mrs::Track::Mix,float,std::function<bool()>);
    void setSendPreview(mrs::Id,mrs::Id,float,std::function<bool()>);
    void cancelPreview();
    void finishPreview();
    mrs::desktop::Application app;
    Theme theme;
    std::optional<TrackHeaderLayout> trackHeaderLayout; // UI-only, never project state.
    juce::File trackHeaderLayoutSource;
    juce::Time trackHeaderObservedStamp, trackHeaderPendingStamp;
    juce::int64 trackHeaderObservedBytes{-1}, trackHeaderPendingBytes{-1};
    int trackHeaderPollTick{};
    bool trackHeaderHotReload{}; // Session-only; OFF in every release/startup by default.
    bool reverbKitPilot{}; // Opt-in process/session state, never persisted.
    std::unique_ptr<kit::MRNativeParameterHost> controlHost;
    kit::MRNativeParameterHost& parameterHost();
    std::vector<mrs::MidiNote> noteClipboard;
    std::optional<mrs::Id> selectedTrack,selectedClip;
    std::set<std::string> selectedClips,selectedTracks;
    std::vector<mrs::processing::VstPlugin> catalog;
    std::array<mrs::audio::StereoPeak,mrs::audio::max_mixer_tracks> peaks{};
    mrs::audio::StereoPeak masterPeak{};
    std::unique_ptr<Arrangement> arrangement;
    juce::Rectangle<int> arrangeArea,mixArea;
    juce::Component browserDivider,mixerDivider;
    juce::Label projectTitle;
    mrs::MusicalContext displayedMusical; // one control snapshot for labels and all lane highlights
    std::array<juce::Label,3> contextLabels;
    void refreshMusicalContext();
    double audioCpu{};
    double displayedCpu{},cpuUpdateMs{};bool cpuMeasured{};
    juce::ProgressBar audioCpuBar{audioCpu};
    juce::Label cpuReadout,latencyReadout;
    juce::String footerStatus();
    static constexpr int faderHeight=145;
    std::vector<std::unique_ptr<Strip>> mixer;
    std::vector<std::unique_ptr<Strip>> mixerFolders; // unlinked presentation-only channels
    std::vector<int> mixerColumns,mixerFolderColumns;
    int mixerColumnCount{};
    void rebuildMixerFolders();
    std::unique_ptr<Strip> master;
    std::unique_ptr<Browser> browser;
    std::unique_ptr<ProjectHome> home;
    std::vector<std::unique_ptr<EditorWindow>> windows;
    mrs::desktop::Preferences prefs;
    std::filesystem::path prefsFile,cacheFile,viewFile;
    bool sidebar{true},testing{},snap{};
    int browserWidth{260};
    juce::String message;
private:
    friend void assetDesignSmoke();
    std::unique_ptr<Theme> assetTheme;
    friend void shellDesignSmoke();
    friend void interface212Smoke();
    friend void channel213Smoke();
    friend void pianoRoll214Smoke();
    void layoutShell();
    void refreshShell();
    void paintShell(juce::Graphics&);
    std::unique_ptr<Theme> shellTheme;
    juce::Label shellStatus,shellTime;
    friend void stateBusySmoke();
    friend void clipCopySmoke();
    friend void clipStretchSmoke();
    mutable mrs::audio::RealtimeState lastDisplayState;
    void timerCallback() override;
    void remember();
    bool homeVisible{};
    bool projectSessionStarted{};
    VisualTransport visualTransport;
    const mrs::audio::AudioEngine* visualEngine{};
    double lastVisualSample{};
    bool spaceHeld{};unsigned previewTick{};
    std::uint64_t wavImportRevision{},stretchRevision{};
    bool resizingBrowser{},resizingMixer{};
    int mixerDragHeight{},mixerDragY{};
    void flushAraDockResize();
    std::optional<int> pendingAraDockHeight;
    juce::Component::SafePointer<juce::Component> resizingAraDock;
    juce::VBlankAttachment araDockResizeFrame;
    friend void araEditorSmoke(const juce::File&);
    std::shared_ptr<mrs::IProjectStore> displayedStore;
    std::uint64_t revision{~0ULL},editorGeneration{};
    std::vector<mrs::Id> ids;
    std::future<std::vector<mrs::processing::VstPlugin>> scanner;
    std::shared_ptr<std::atomic<bool>> scanCancel{std::make_shared<std::atomic<bool>>(false)};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::MenuBarComponent menu{this};
    juce::Viewport mixerViewport;
    juce::Component mixerBody;
    kit::MRButton play{"Play"},pause{"Pause"},stop{"Stop"},record{"Record (R)"},
        previous{"< Section"},next{"Section >"},loop{"Loop section"},
        editButton{"Edit"},mixButton{"Mix"},brows{"BROWS"},
        addTrack{"+ Track"},addBus{"+ Bus"},undo{"Undo"},redo{"Redo"},split{"Split (S)"},
        duplicate{"Duplicate"},remove{"Del clip"},zoomIn{"Zoom +"},zoomOut{"Zoom -"},fit{"Fit"},snapButton{"Snap off"},clickButton{"Click"},countButton{"Count off"},pointerTool{"Pointer (1)"},knifeTool{"Knife (3)"};
    int lastCountBars{1}; // Toolbar off/on restores the last enabled count in this session.
    unsigned diagnosticTick{};
    struct MixPreview {std::optional<mrs::Id> target;mrs::Track::Mix mix;float master;std::function<bool()> active;std::optional<mrs::Id> sendBus{};float sendGain{};};
    std::optional<MixPreview> preview;
};

class Strip final : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient {
    friend void assetDesignSmoke();
public:
    Strip(Desktop&,std::optional<mrs::Id>,bool mini=false,std::optional<mrs::Id> folder={});
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    juce::Rectangle<int> nameBounds() const;
    void mouseUp(const juce::MouseEvent&) override;
    void beginNameEdit();
    void resized() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void sync();
    void applyAssetDesign();
    void refreshArmState();
    void refreshMidiStatus();
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragEnter(const SourceDetails& s) override {itemDragMove(s);}
    void itemDragExit(const SourceDetails&) override;
    bool isEditingTrackName() const noexcept { return editingName; }
    bool active() const {if(resizingSends||gain.dragging()||pan.dragging())return true;for(const auto& b:sendButtons)if(b->dragging())return true;return false;}
    std::size_t pluginInsertionAt(juce::Point<int>) const;
    juce::Rectangle<int> pluginDropArea() const {return insertViewport.getBounds();}
    Handle gain,pan{false};
    std::optional<mrs::Id> target,folder;
private:
    mrs::Track track() const;
    mrs::Track::Mix adjusted(bool,double) const;
    void preview(bool,double);void commit(bool,double);
    void inputMenu();void insertList();void colorMenu();void openInstrument();
    void syncFolderPresentation();
    juce::TextButton folderToggle,folderMenuButton{"..."};
    std::unique_ptr<juce::Drawable> folderIcon;
    Desktop& owner;
    bool mini{};
    bool dragTitle{},dropBefore{},trackHover{};
    std::optional<std::size_t> pluginHover;
    bool insertDropValid{true};juce::String insertDropMode;
    bool editingName{};
    juce::TextEditor nameEditor;
    void finishNameEdit(bool);
    juce::TextButton mute{"M"},solo{"S"},arm{"R"},monitor{"I"},input{"Input"},
        output{"Out: Master"},sends{"+ Send"},add{"+"},instrument{"Instrument"},piano{""};
    class ColorField final:public juce::TextButton{
    public:std::uint32_t rgb{ObjectPalette::defaultClip};
        void paintButton(juce::Graphics& g,bool hover,bool down)override{
            g.setColour(juce::Colour(0xff000000|rgb).withAlpha(isEnabled()?1.f:.35f).withMultipliedBrightness(down?.8f:hover?1.15f:1.f));g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1),2);
            if(hasKeyboardFocus(true)){g.setColour(juce::Colours::white);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),2,1);}
        }
    } colorField;
    InsertPowerButton allPower;
    std::vector<std::unique_ptr<MixerInsert>> insertButtons;
    juce::Viewport insertViewport;
    juce::Component insertBody;
    std::vector<std::unique_ptr<SendLevel>> sendButtons;
    std::vector<mrs::Id> sendDestinations;
    juce::Viewport sendViewport;
    juce::Component sendBody;
    juce::Component sendDivider;
    bool resizingSends{};
    int sendDragY{},sendDragHeight{};
    void sendList();
};

class Arrangement final : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget, public juce::SettableTooltipClient {
public:
    explicit Arrangement(Desktop&);
    ~Arrangement() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void rebuild();
    void paintGrid(juce::Graphics&, float top, float bottom, bool labels = false) const;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void focusLost(FocusChangeType) override;
    bool keyPressed(const juce::KeyPress&) override;
    void cancelGesture();
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    void fileDragEnter(const juce::StringArray& f,int x,int y) override {fileDragMove(f,x,y);}
    void fileDragMove(const juce::StringArray&,int,int) override;
    void fileDragExit(const juce::StringArray&) override;
    void itemDragEnter(const SourceDetails& s) override {itemDragMove(s);}
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    std::vector<juce::Rectangle<float>> midiDropRects() const;
    std::vector<juce::Rectangle<float>> wavDropRects() const;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void zoom(double);void fit();
    void wheel(float,juce::ModifierKeys,float);
    juce::Rectangle<float> clipRect(const mrs::Clip&) const;
    mrs::Tick displaySpacing() const;
    mrs::Tick snapTick(mrs::Tick) const;
    mrs::Sample snapSample(mrs::Sample) const;
    mrs::Sample sampleAt(float) const;
    mrs::Sample seekSampleAt(float) const;
    int edgeAt(const mrs::Clip&,juce::Point<float>) const;
    std::optional<mrs::Id> clipAt(juce::Point<float>) const;
    void clipMenu(mrs::Id,juce::Point<int>);
    int trackAt(float) const;
    int trackY(int) const;
    void rebuildFolderLayout();
    struct FolderRow {mrs::Folder folder;int y{},depth{};};
    std::vector<FolderRow> folderRows;
    std::vector<std::unique_ptr<TrackFolderHeader>> folderHeaders;
    std::vector<int> trackPositions;
    int contentHeight{},layoutTrackHeight{};
    std::vector<std::unique_ptr<Strip>> rows;
    juce::Component rowsBody;
    double pixelsPerSecond{30},horizontal{};
    float vertical{};
    int trackHeight{128};
    static constexpr int header=165,left=250; // Default all-visible layout (legacy fixtures).
    int headerHeight()const;
    bool laneVisible(int index)const;
    void setLaneVisible(int index,bool visible);
    std::array<std::unique_ptr<juce::Button>,5> laneButtons;
    std::unique_ptr<class MeterTrack> meters;
    std::unique_ptr<class TempoTrack> tempos;
    std::unique_ptr<class SectionTrack> sections;
    std::unique_ptr<ChordTrack> chords;
    std::unique_ptr<class MarkerTrack> markers;
private:
    Desktop& owner;
    // One bounded latest-request worker; no WAV I/O in drag/paint callbacks.
    struct DropWave {std::uint64_t frames{};std::uint32_t rate{};};
    mutable std::mutex dropMutex;
    std::condition_variable_any dropWake;
    std::jthread dropWorker;
    juce::StringArray pendingDropFiles;
    std::vector<DropWave> dropWaves;
    std::uint64_t dropGeneration{};
    bool dropRequested{};
    juce::StringArray dropFiles;
    std::vector<mrs::MidiPart> dropParts;
    std::optional<juce::Point<int>> dropPoint;
    std::optional<mrs::Id> araDropClip;
    std::optional<mrs::Id> midiDropTarget(int y) const;
    mrs::Tick midiDropTick(int x) const;
    std::optional<mrs::Clip> drag;
    std::optional<mrs::Id> dragTrack;
    mrs::Sample dragStart{};
    mrs::Sample trimStart{},trimEnd{},dragSourceFrames{};
    void updateTrim(mrs::Sample delta,bool useSnap);
    bool rulerSeeking{};
    float dragX{};
    int trim{};std::vector<mrs::Clip> dragGroup;int dragTrackDelta{};
    std::vector<mrs::Clip> trimPreviews;
    std::map<std::string,mrs::Sample> trimFrames;
    bool copyDrag{},dragMoved{};
    bool shiftRemove{};
    std::set<std::string> selectionBase;
    std::optional<mrs::Clip> fadeOriginal;
    std::shared_ptr<mrs::IProjectStore> fadeStore;
    mrs::Sample fadeInPreview{},fadeOutPreview{};
    bool fadeRight{};
    bool selecting{};juce::Point<float> selectionOrigin;juce::Rectangle<float> selectionBox;
    PlayheadLine playhead;
};

class Browser final : public juce::Component,private juce::FileBrowserListener,private juce::Timer {
public:
    explicit Browser(Desktop&);
    ~Browser() override;
    void showFiles(bool);
    void showAssistant();
    void showLibrary();
    juce::Component* assistantComponent() const{return assistant.get();}
    void navigate(const juce::File&);
    juce::StringArray selectedSamples() const;
    juce::StringArray selectedLibrarySamples() const;
    bool samplePreviewReady() const {return wantedFile!=juce::File()&&preview.file==wantedFile;}
    void paint(juce::Graphics&) override;
    void resized() override;
    void rebuild();
    void applyAssetDesign();
    juce::TreeView tree;
private:
    void selectionChanged() override;
    void fileClicked(const juce::File&,const juce::MouseEvent&) override {}
    void fileDoubleClicked(const juce::File& file) override {if(file.isDirectory())navigate(file);}
    void browserRootChanged(const juce::File&) override {}
    void timerCallback() override;
    struct Preview {juce::File file;juce::String info;std::array<float,256> levels{};};
    std::future<Preview> previewWorker;juce::File wantedFile;Preview preview;
    juce::ComboBox locations;std::vector<juce::File> locationPaths;
    juce::Viewport breadcrumbView;juce::Component breadcrumbBody;
    std::vector<std::unique_ptr<juce::TextButton>> crumbs;
    Desktop& owner;
    juce::TextButton scanButton{"Scan VST3..."};
    juce::TextEditor search;
    juce::TextButton vstTab{"VST3"},filesTab{"Files"},parentFolder{"Up"},chooseFolder{"Folder..."};
    juce::TextButton aiTab{"AI"},libraryTab{"Lib"};
    std::unique_ptr<LibraryFoundationPanel> libraryPanel;
    bool libraryVisible{};
    std::unique_ptr<juce::Component> assistant;
    bool assistantVisible{};
    juce::TextEditor folderPath;
    juce::TimeSliceThread fileThread{"MR Studio file browser"};
    juce::WildcardFileFilter fileFilter{"*.wav;*.mid;*.midi","*","Audio and MIDI files"};
    juce::DirectoryContentsList directory{&fileFilter,fileThread};
    juce::FileTreeComponent fileTree{directory};
    bool filesVisible{};
    std::unique_ptr<juce::TreeViewItem> root;
};
void trackFolderSmoke();
void j2Smoke(Desktop&,const juce::File& fixture={});
void j2PluginSmoke(Desktop&,const juce::File&);
void j3Smoke(Desktop&);
void projectHomeSmoke();
void liveInterfaceSmoke(const juce::File&);
void controllerScrollSmoke();
void noteEditorLayoutSmoke();
void midi4aSmoke(Desktop&,const juce::File&);
void midi4bSmoke(Desktop&,const juce::File&);
void pianoRoll214Smoke();
void midi4cSmoke(Desktop&,const juce::File&);
void j3AudioSmoke(Desktop&);
void profilingSmoke(Desktop&);
void thuDiagnostic(const juce::File&);
void retirePluginWindow(EditorWindow&);
void retireAraWindow(EditorWindow&);
void retireAraComponent(juce::Component&);
void araEditorSmoke(const juce::File&);
void performAraOperation(Desktop&,const std::function<void()>&);
void presetSelectorSmoke(Desktop&,mrs::Id,mrs::Id,const juce::File&);
void reconnectPluginWindow(EditorWindow&);
void aiPermissionsSmoke(Desktop&);
void aiAssistantSmoke(Desktop&);
void clipStretchSmoke();
std::unique_ptr<juce::Component> makeAiAssistant(Desktop&);
}
