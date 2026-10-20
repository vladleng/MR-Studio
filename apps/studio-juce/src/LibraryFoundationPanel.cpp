#include "LibraryFoundationPanel.h"
#include <array>

namespace ui {
namespace {
std::filesystem::path filePath(const juce::File& f) {
    return std::filesystem::path(f.getFullPathName().toWideCharPointer());
}
std::filesystem::path databasePath() {
    auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MoonRiverStudio").getChildFile("MRLibrary").getChildFile("samples.sqlite");
    return filePath(root);
}
juce::String text(const std::string& utf8) { return juce::String::fromUTF8(utf8.c_str()); }
juce::String pathText(const std::filesystem::path& path) { return juce::String(path.wstring().c_str()); }
}
LibraryFoundationPanel::LibraryFoundationPanel() : library(databasePath()) {
    for (auto* c : std::array<juce::Component*,9>{&addFolder,&rescan,&cancel,&relink,&toggle,&remove,&sources,&title,&status})
        addAndMakeVisible(c);
    setComponentID("mr-library-foundation");
    sources.setComponentID("mr-library-sources");
    sources.setRowHeight(52);
    sources.setColour(juce::ListBox::backgroundColourId,juce::Colour(surface));
    title.setText("MR Library - WAV sources",juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    status.setJustificationType(juce::Justification::topLeft);
    status.setTitle("Library index status");
    addFolder.setTooltip("Index an existing WAV folder without moving files");
    rescan.setTooltip("Incrementally rescan enabled folders");
    remove.setTooltip("Remove selected source from catalogue; keep original WAVs");
    addFolder.onClick=[this]{
        chooseFolder([this](const juce::File& f){library.add_source(filePath(f));});
    };
    rescan.onClick=[this]{library.rescan();};
    cancel.onClick=[this]{library.cancel_scan();};
    relink.onClick=[this]{
        const auto row=sources.getSelectedRow();
        if (!view||row<0||row>=static_cast<int>(view->sources.size()))return;
        const auto id=view->sources[static_cast<std::size_t>(row)].id;
        chooseFolder([this,id](const juce::File& f){library.relink_source(id,filePath(f));});
    };
    toggle.onClick=[this]{
        const auto row=sources.getSelectedRow();
        if (!view||row<0||row>=static_cast<int>(view->sources.size()))return;
        const auto& s=view->sources[static_cast<std::size_t>(row)];
        library.enable_source(s.id,!s.enabled);
    };
    remove.onClick=[this]{
        const auto row=sources.getSelectedRow();
        if (!view||row<0||row>=static_cast<int>(view->sources.size()))return;
        const auto id=view->sources[static_cast<std::size_t>(row)].id;
        juce::Component::SafePointer<LibraryFoundationPanel> safe(this);
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
            "Remove indexed folder?",
            "Remove the selected folder from MR Library's index? Original audio files will not be changed.",
            "Remove Index","Cancel",this,
            juce::ModalCallbackFunction::create([safe,id](int ok){if(safe&&ok)safe->library.remove_source(id);}));
    };
    refresh();
    startTimerHz(6);
}
LibraryFoundationPanel::~LibraryFoundationPanel() {
    stopTimer(); chooser.reset(); sources.setModel(nullptr);
}
void LibraryFoundationPanel::chooseFolder(std::function<void(const juce::File&)> callback) {
    chooser=std::make_unique<juce::FileChooser>("Choose existing WAV library folder");
    juce::Component::SafePointer<LibraryFoundationPanel> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,
        [safe, callback=std::move(callback)](const auto& result){
            if(safe&&result.getResult().isDirectory())callback(result.getResult());
        });
}
void LibraryFoundationPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(surface));
}
void LibraryFoundationPanel::resized() {
    auto bounds=getLocalBounds().reduced(7);
    title.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(4);
    auto first=bounds.removeFromTop(27);
    addFolder.setBounds(first.removeFromLeft(first.getWidth()/2-3));first.removeFromLeft(6);
    rescan.setBounds(first);
    bounds.removeFromTop(5);
    auto second=bounds.removeFromTop(26);
    relink.setBounds(second.removeFromLeft(second.getWidth()/3-3));second.removeFromLeft(5);
    toggle.setBounds(second.removeFromLeft(second.getWidth()/2-3));second.removeFromLeft(5);
    remove.setBounds(second);
    bounds.removeFromTop(6);
    status.setBounds(bounds.removeFromBottom(68));
    bounds.removeFromBottom(5);
    sources.setBounds(bounds);
    cancel.setBounds(getWidth()-84,getHeight()-30,76,23);
}
int LibraryFoundationPanel::getNumRows() {
    return view?static_cast<int>(view->sources.size()):0;
}
void LibraryFoundationPanel::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) {
    if(!view||row<0||row>=static_cast<int>(view->sources.size()))return;
    const auto& item=view->sources[static_cast<std::size_t>(row)];
    if(selected)g.fillAll(juce::Colour(0xff375a80));
    g.setColour(juce::Colours::whitesmoke);
    g.setFont(Theme::detailFont());
    g.drawFittedText(pathText(item.root.filename()),5,1,width-10,19,juce::Justification::centredLeft,1);
    g.setColour(juce::Colours::lightgrey);
    const auto summary=juce::String(item.enabled?"":"Disabled | ")+
        juce::String(static_cast<juce::int64>(item.available))+" WAV, "+
        juce::String(static_cast<juce::int64>(item.invalid))+" invalid, "+
        juce::String(static_cast<juce::int64>(item.missing))+" missing";
    g.drawFittedText(summary,5,23,width-10,17,juce::Justification::centredLeft,1);
    g.setColour(juce::Colour(0xff445163));
    g.drawHorizontalLine(height-1,0.f,static_cast<float>(width));
}
void LibraryFoundationPanel::listBoxItemClicked(int row,const juce::MouseEvent&) {
    sources.selectRow(row);
}
void LibraryFoundationPanel::timerCallback() { refresh(); }
void LibraryFoundationPanel::refresh() {
    auto next=library.snapshot();
    if(next->revision==lastRevision&&view)return;
    view=std::move(next);lastRevision=view->revision;
    sources.updateContent();sources.repaint();
    const auto phase=view->phase;
    juce::String message=phase==mrs::library::Phase::starting?"Opening database..."
        :phase==mrs::library::Phase::scanning?"Scanning WAV folders..."
        :phase==mrs::library::Phase::error?"Library error"
        :phase==mrs::library::Phase::cancelled?"Scan cancelled":"Ready";
    if(!view->message.empty())message+="\n"+text(view->message);
    if(phase==mrs::library::Phase::scanning)
        message+="\nProcessed: "+juce::String(static_cast<juce::int64>(view->processed));
    status.setText(message,juce::dontSendNotification);
    cancel.setEnabled(phase==mrs::library::Phase::scanning);
}
} // namespace ui
