#pragma once
#include "Controls.h"
#include "Settings.h"
#include <mrs/desktop.hpp>
#include <mrs/offline_device.hpp>
#include <mrs/vst3.hpp>
#include <future>

namespace ui {
class Desktop;
class Arrangement;
class Strip;
class Browser;
class FxPanel;
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
    explicit Desktop(bool testing=false);
    ~Desktop() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;
    void spaceKey(bool);
    void focusLost(FocusChangeType) override;
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int,const juce::String&) override;
    void menuItemSelected(int,int) override;
    void action(int);
    void run(std::function<void()>);
    void refresh(bool force=false);
    void choose(int,std::function<void(const juce::File&)>,juce::String pattern="*");
    void textDialog(juce::String,juce::String,std::function<void(juce::String)>);
    void confirmDiscard(std::function<void()>);
    void openFile(const juce::File&);
    void saveFile(const juce::File&);
    void importFiles(const juce::StringArray&);
    void resetDevice();
    void reconnectDevice(bool session=false);
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void dropTrack(const juce::String&,std::size_t boundary);
    void saveSettings();
    bool shortcutAllowed() const;
    bool shortcutAllowedFor(juce::Component*) const;
    ViewSettings view;
    void closeEditors(bool keepMidi=false);
    void closeUnpinnedEditors();
    void audioSettings();
    void showProfiling();
    void openMidiClip(mrs::Id);
    void insertMenu(std::optional<mrs::Id>);
    void openInsert(std::optional<mrs::Id>,mrs::Id);
    void savePreset(std::optional<mrs::Id>,mrs::Id);
    void loadPreset(std::optional<mrs::Id>,mrs::Id);
    void refreshPresetLists();
    void resizeBrowser(int);
    void resizeMixer(int);
    void updatePerformance(const mrs::audio::DeviceStatus&,bool hardware);
    juce::String projectCaption() const;
    void loadPresetFile(std::optional<mrs::Id>,mrs::Id,const juce::File&);
    juce::PopupMenu pluginMenu(int base=10000) const;
    void addPlugin(std::optional<mrs::Id>,std::size_t);
    void routeMenu(mrs::Id);
    void sendsMenu(mrs::Id);
    std::vector<mrs::NativeInsert> chain(std::optional<mrs::Id>) const;
    void applyChain(std::optional<mrs::Id>,std::vector<mrs::NativeInsert>);
    void scan(const juce::File&);
    void setWorkspace(mrs::desktop::Workspace);
    bool busyGesture() const;
    std::shared_ptr<const mrs::Project> project() const {return app.services().projects->state().project;}
    void setPreview(std::optional<mrs::Id>,mrs::Track::Mix,float,std::function<bool()>);
    void cancelPreview();
    void finishPreview();
    mrs::desktop::Application app;
    Theme theme;
    std::vector<mrs::MidiNote> noteClipboard;
    std::optional<mrs::Id> selectedTrack,selectedClip;
    std::vector<mrs::processing::VstPlugin> catalog;
    std::array<mrs::audio::StereoPeak,mrs::audio::max_mixer_tracks> peaks{};
    mrs::audio::StereoPeak masterPeak{};
    std::unique_ptr<Arrangement> arrangement;
    juce::Rectangle<int> arrangeArea,mixArea;
    juce::Component browserDivider,mixerDivider;
    juce::Label projectTitle;
    double audioCpu{};
    juce::ProgressBar audioCpuBar{audioCpu};
    juce::Label cpuReadout,latencyReadout;
    int faderHeight{160};
    std::vector<std::unique_ptr<Strip>> mixer;
    std::unique_ptr<Strip> master;
    std::unique_ptr<Browser> browser;
    std::vector<std::unique_ptr<EditorWindow>> windows;
    mrs::desktop::Preferences prefs;
    std::filesystem::path prefsFile,cacheFile,viewFile;
    bool sidebar{true},testing{},snap{};
    int browserWidth{260};
    juce::String message;
private:
    void timerCallback() override;
    void remember();
    bool spaceHeld{};
    bool resizingBrowser{},resizingMixer{};
    int mixerDragHeight{},mixerDragY{};
    std::shared_ptr<mrs::IProjectStore> displayedStore;
    std::uint64_t revision{~0ULL},editorGeneration{};
    std::vector<mrs::Id> ids;
    std::future<std::vector<mrs::processing::VstPlugin>> scanner;
    std::shared_ptr<std::atomic<bool>> scanCancel{std::make_shared<std::atomic<bool>>(false)};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::MenuBarComponent menu{this};
    juce::Viewport mixerViewport;
    juce::Component mixerBody;
    juce::TextButton play{"Play"},pause{"Pause"},stop{"Stop"},record{"Record (R)"},
        previous{"< Section"},next{"Section >"},loop{"Loop section"},
        arrangeButton{"Arrange"},editButton{"Edit"},mixButton{"Mix"},brows{"BROWS"},
        addTrack{"+ Track"},addBus{"+ Bus"},undo{"Undo"},redo{"Redo"},split{"Split (S)"},
        remove{"Del clip"},zoomIn{"Zoom +"},zoomOut{"Zoom -"},fit{"Fit"},audio{"Audio settings"},snapButton{"Snap off"};
    struct MixPreview {std::optional<mrs::Id> target;mrs::Track::Mix mix;float master;std::function<bool()> active;};
    std::optional<MixPreview> preview;
};

class Strip final : public juce::Component, public juce::DragAndDropTarget {
public:
    Strip(Desktop&,std::optional<mrs::Id>,bool mini=false);
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void sync();
    void refreshMidiStatus();
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    bool active() const {return gain.dragging()||pan.dragging();}
    Handle gain,pan{false};
    std::optional<mrs::Id> target;
private:
    mrs::Track track() const;
    mrs::Track::Mix adjusted(bool,double) const;
    void preview(bool,double);void commit(bool,double);
    void inputMenu();void insertList();
    Desktop& owner;
    bool mini{};
    bool dragTitle{},dropBefore{},trackHover{};
    juce::TextButton mute{"M"},solo{"S"},arm{"R"},monitor{"I"},input{"Input"},
        inserts{"Inserts"},output{"Out: Master"},sends{"Sends"},add{"+"};
    std::vector<std::unique_ptr<juce::TextButton>> insertButtons;
    juce::Viewport insertViewport;
    juce::Component insertBody;
};

class Arrangement final : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget {
public:
    explicit Arrangement(Desktop&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void rebuild();
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void zoom(double);void fit();
    void wheel(float,juce::ModifierKeys,float);
    juce::Rectangle<float> clipRect(const mrs::Clip&) const;
    mrs::Sample sampleAt(float) const;
    int trackAt(float) const;
    std::vector<std::unique_ptr<Strip>> rows;
    juce::Component rowsBody;
    double pixelsPerSecond{30},horizontal{};
    float vertical{};
    int trackHeight{128};
private:
    Desktop& owner;
    std::optional<mrs::Clip> drag;
    std::optional<mrs::Id> dragTrack;
    mrs::Sample dragStart{};
    float dragX{};
    int trim{};
    static constexpr int header=70,left=250;
};

class Browser final : public juce::Component,private juce::FileBrowserListener,private juce::Timer {
public:
    explicit Browser(Desktop&);
    ~Browser() override;
    void showFiles(bool);
    void navigate(const juce::File&);
    juce::StringArray selectedSamples() const;
    bool samplePreviewReady() const {return wantedFile!=juce::File()&&preview.file==wantedFile;}
    void paint(juce::Graphics&) override;
    void resized() override;
    void rebuild();
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
    juce::TextEditor folderPath;
    juce::TimeSliceThread fileThread{"MR Studio file browser"};
    juce::WildcardFileFilter fileFilter{"*.wav","*","WAV samples"};
    juce::DirectoryContentsList directory{&fileFilter,fileThread};
    juce::FileTreeComponent fileTree{directory};
    bool filesVisible{};
    std::unique_ptr<juce::TreeViewItem> root;
};
void j2Smoke(Desktop&,const juce::File& fixture={});
void j2PluginSmoke(Desktop&,const juce::File&);
void j3Smoke(Desktop&);
void midi4aSmoke(Desktop&,const juce::File&);
void midi4bSmoke(Desktop&,const juce::File&);
void midi4cSmoke(Desktop&,const juce::File&);
void j3AudioSmoke(Desktop&);
void profilingSmoke(Desktop&);
void thuDiagnostic(const juce::File&);
void retirePluginWindow(EditorWindow&);
void reconnectPluginWindow(EditorWindow&);
}
