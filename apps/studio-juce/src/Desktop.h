#pragma once
#include "Controls.h"
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
    EditorWindow(juce::String,juce::Component*);
    void closeButtonPressed() override;
    std::function<void()> onClose;
};
class Desktop final : public juce::Component, private juce::Timer,
                      public juce::MenuBarModel, public juce::DragAndDropContainer {
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
    void closeEditors();
    void audioSettings();
    void insertMenu(std::optional<mrs::Id>);
    void openInsert(std::optional<mrs::Id>,mrs::Id);
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
    std::optional<mrs::Id> selectedTrack,selectedClip;
    std::vector<mrs::processing::VstPlugin> catalog;
    std::array<mrs::audio::StereoPeak,mrs::audio::max_mixer_tracks> peaks{};
    mrs::audio::StereoPeak masterPeak{};
    std::unique_ptr<Arrangement> arrangement;
    juce::Rectangle<int> arrangeArea,mixArea;
    std::vector<std::unique_ptr<Strip>> mixer;
    std::unique_ptr<Strip> master;
    std::unique_ptr<Browser> browser;
    std::vector<std::unique_ptr<EditorWindow>> windows;
    mrs::desktop::Preferences prefs;
    std::filesystem::path prefsFile,cacheFile;
    bool sidebar{true},testing{},snap{};
    int browserWidth{260};
    juce::String message;
private:
    void timerCallback() override;
    void remember();
    bool spaceHeld{};
    bool resizingBrowser{};
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
    void resized() override;
    void paint(juce::Graphics&) override;
    void sync();
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
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
    juce::TextButton mute{"M"},solo{"S"},arm{"R"},monitor{"I"},input{"Input"},
        inserts{"Inserts"},output{"Out: Master"},sends{"Sends"},add{"+"};
    std::vector<std::unique_ptr<juce::TextButton>> insertButtons;
};

class Arrangement final : public juce::Component, public juce::FileDragAndDropTarget {
public:
    explicit Arrangement(Desktop&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void rebuild();
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
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

class Browser final : public juce::Component {
public:
    explicit Browser(Desktop&);
    ~Browser() override {tree.setRootItem(nullptr);}
    void paint(juce::Graphics&) override;
    void resized() override;
    void rebuild();
    juce::TreeView tree;
private:
    Desktop& owner;
    juce::TextButton scanButton{"Scan VST3..."};
    juce::TextEditor search;
    std::unique_ptr<juce::TreeViewItem> root;
};
void j2Smoke(Desktop&,const juce::File& fixture={});
void j2PluginSmoke(Desktop&,const juce::File&);
}
