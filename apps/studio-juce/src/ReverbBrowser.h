#pragma once
#include "Controls.h"
#include "Settings.h"
#include <mrs/reverb_library.hpp>

namespace ui {
// Browser portion of the native editor. Host/menu/commands/state integration #131.
// Library is app-wide, loader is per-instance; preferences belong to the shell.
class ReverbBrowser final : public juce::Component,private juce::ListBoxModel,private juce::Timer {
public:
    ReverbBrowser(std::shared_ptr<mrs::audio::ReverbLibrary>,std::shared_ptr<mrs::audio::ReverbLoader>,ViewSettings&,std::function<void()> savePreferences,juce::String insertId={});
    ~ReverbBrowser() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void lookAndFeelChanged() override;
    void chooseRoot(const juce::File&); // folder selection also cancels obsolete load
    void activate(int row); // single-click/Return, no file dialog for normal IR selection
    void refresh();
    int visibleRows() const noexcept {return static_cast<int>(filtered.size());}
    void searchFor(const juce::String& text){search.setText(text,juce::sendNotification);}
    std::function<std::shared_ptr<mrs::audio::ReverbLoader>()> loaderProvider;
    std::function<mrs::ReverbState()> rememberedStateProvider;
private:
    int getNumRows() override {return visibleRows();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void listBoxItemClicked(int row,const juce::MouseEvent&) override {activate(row);}
    void returnKeyPressed(int row) override {activate(row);}
    void timerCallback() override {refresh();if(viewDirty&&juce::Time::getMillisecondCounter()-viewChangedAt>=300)flushView();}
    void filter();
    void rememberView();void flushView();
    juce::String viewId;bool viewDirty{};juce::uint32 viewChangedAt{};
    std::shared_ptr<mrs::audio::ReverbLibrary> library;
    std::shared_ptr<mrs::audio::ReverbLoader> loader;
    ViewSettings& preferences;std::function<void()> save;
    std::shared_ptr<const mrs::audio::ReverbLibraryIndex> index;
    std::vector<std::size_t> filtered;
    juce::TextButton browse{"Browse Folder..."};juce::TextEditor search;
    juce::ComboBox folders;juce::ToggleButton quad{"Quad map: LL / LR / RL / RR"};
    juce::TreeView tree;std::unique_ptr<juce::TreeViewItem> treeRoot;juce::String selectedFolder;
    juce::ListBox list{"IR library",this};juce::Label rootLabel,info,status;
    std::unique_ptr<juce::FileChooser> chooser;juce::String localError;
};
void reverbBrowserSmoke();
void reverbEditorSmoke();
void cabEditorSmoke();
void nativeEditorSmoke();
}
