#include "LibraryFoundationPanel.h"
#include "LibraryBrowserView.h"
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
struct LibraryFoundationPanel::AssetRows final : juce::ListBoxModel {
    explicit AssetRows(LibraryFoundationPanel& p):owner(p){}
    int getNumRows() override {return owner.view?static_cast<int>(owner.view->assets.size()):0;}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int /*height*/,bool selected) override {
        if(!owner.view||row<0||row>=static_cast<int>(owner.view->assets.size()))return;
        const auto& a=owner.view->assets[static_cast<std::size_t>(row)];
        if(selected)g.fillAll(juce::Colour(0xff375a80));
        g.setColour(juce::Colours::whitesmoke);g.setFont(Theme::detailFont());
        g.drawFittedText(text(a.relative_path),6,2,width-12,17,juce::Justification::centredLeft,1);
        int x=6;
        for(const auto& tag:a.tags){
            if(tag.status=="rejected")continue;
            const auto label=text(tag.label);
            const int w=juce::jmin(108,label.length()*6+14);
            if(x+w>width-6)break;
            g.setColour(tag.status=="confirmed"?juce::Colour(0xff4a7e71):juce::Colour(0xff414e65));
            g.fillRoundedRectangle(static_cast<float>(x),24.f,static_cast<float>(w),18.f,5.f);
            g.setColour(juce::Colours::whitesmoke);g.drawFittedText(label,x+4,25,w-8,15,juce::Justification::centred,1);
            x+=w+4;
        }
        if(a.tags.empty()) {g.setColour(juce::Colours::grey);g.drawText("No tags",6,24,width-12,18,juce::Justification::centredLeft);}
    }
    void selectedRowsChanged(int) override {owner.fillSuggestedTags();}
    LibraryFoundationPanel& owner;
};
LibraryFoundationPanel::LibraryFoundationPanel() : library(databasePath()) {
    for (auto* c : std::array<juce::Component*,25>{&addFolder,&rescan,&cancel,&relink,&toggle,&remove,&sources,&title,&status,
        &tagInfo,&assets,&modeButton,&sourcesMode,&addTag,&confirmTag,&rejectTag,&undoTag,&setBpm,&setKey,
        &addRule,&assetSearch,&tagLabel,&bpmValue,&keyValue,&rulePhrase})
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
    for(auto* c:{static_cast<juce::Component*>(&tagGroup),static_cast<juce::Component*>(&suggestedTags),
                  static_cast<juce::Component*>(&ruleFolder)})addAndMakeVisible(c);
    assetRows=std::make_unique<AssetRows>(*this);
    assets.setModel(assetRows.get());assets.setRowHeight(46);
    assets.setMultipleSelectionEnabled(true);
    assets.setColour(juce::ListBox::backgroundColourId,juce::Colour(surface));
    assets.setComponentID("mr-library-tagged-assets");
    assetSearch.setTextToShowWhenEmpty("Search indexed WAV filenames...",juce::Colours::grey);
    tagLabel.setTextToShowWhenEmpty("Tag value (Jazz, Brushes...)",juce::Colours::grey);
    rulePhrase.setTextToShowWhenEmpty("Match word/phrase (e.g. drumloop)",juce::Colours::grey);
    bpmValue.setTextToShowWhenEmpty("BPM (95 / 0 / blank)",juce::Colours::grey);
    keyValue.setTextToShowWhenEmpty("Key (Am / - / blank)",juce::Colours::grey);
    suggestedTags.setTextWhenNothingSelected("Select detected tag...");
    static constexpr const char* groups[]={"Instrument","Genre","Type","Technique","Rhythm","Character","Custom"};
    for(int i=0;i<7;++i)tagGroup.addItem(groups[i],i+1);
    tagGroup.setSelectedId(1,juce::dontSendNotification);
    tagInfo.setText("Proposed / confirmed / rejected tags; select WAVs for bulk edits",
                    juce::dontSendNotification);
    tagInfo.setJustificationType(juce::Justification::centredLeft);
    modeButton.onClick=[this]{
        browsing=editingTags;editingTags=!editingTags;updateMode();resized();
    };
    sourcesMode.onClick=[this]{
        browsing=!browsing;
        if(browsing)editingTags=false;
        updateMode();resized();
    };
    assetSearch.onTextChange=[this]{
        library.set_view(sources.getSelectedRow()>=0&&view&&
            sources.getSelectedRow()<static_cast<int>(view->sources.size())
            ?view->sources[static_cast<std::size_t>(sources.getSelectedRow())].id:0,
            assetSearch.getText().toStdString());
    };
    suggestedTags.onChange=[this]{
        const int row=assets.getSelectedRow(),tag=suggestedTags.getSelectedId()-1;
        if(!view||row<0||row>=static_cast<int>(view->assets.size()))return;
        const auto& items=view->assets[static_cast<std::size_t>(row)].tags;
        if(tag<0||tag>=static_cast<int>(items.size()))return;
        tagLabel.setText(text(items[static_cast<std::size_t>(tag)].label),juce::dontSendNotification);
        const auto& group=items[static_cast<std::size_t>(tag)].group;
        for(int i=0;i<7;++i)if(group==groups[i])tagGroup.setSelectedId(i+1,juce::dontSendNotification);
    };
    addTag.onClick=[this]{
        const auto ids=selectedAssetIds();if(!ids.empty())library.add_tag(ids,
            tagGroup.getText().toStdString(),tagLabel.getText().trim().toStdString());
    };
    confirmTag.onClick=[this]{applyTag("confirmed");};
    rejectTag.onClick=[this]{applyTag("rejected");};
    undoTag.onClick=[this]{library.undo_tag_edit();};
    setBpm.onClick=[this]{
        const auto ids=selectedAssetIds();if(ids.empty())return;
        const auto value=bpmValue.getText().trim();
        if(value.isNotEmpty()&&!value.containsOnly("0123456789"))return;
        library.set_bpm(ids,value.isEmpty()?-1:value.getIntValue());
    };
    setKey.onClick=[this]{
        const auto ids=selectedAssetIds();if(!ids.empty())
            library.set_key(ids,keyValue.getText().trim().toStdString());
    };
    addRule.onClick=[this]{
        library.add_rule(tagGroup.getText().toStdString(),rulePhrase.getText().trim().toStdString(),
            tagLabel.getText().trim().toStdString(),ruleFolder.getToggleState());
    };
    browserView=std::make_unique<LibraryBrowserView>(library);
    addAndMakeVisible(*browserView);
    title.setText("MR Library",juce::dontSendNotification);
    updateMode();
    refresh();
    startTimerHz(6);
}
LibraryFoundationPanel::~LibraryFoundationPanel() {
    stopTimer(); chooser.reset();browserView.reset();
    sources.setModel(nullptr);assets.setModel(nullptr);assetRows.reset();
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
    auto header=bounds.removeFromTop(25);
    sourcesMode.setBounds(header.removeFromRight(66));
    header.removeFromRight(4);
    modeButton.setBounds(header.removeFromRight(74));
    title.setBounds(header);
    bounds.removeFromTop(4);
    if(browsing){
        status.setBounds(bounds.removeFromBottom(29));
        bounds.removeFromBottom(3);
        if(browserView)browserView->setBounds(bounds);
        return;
    }
    auto first=bounds.removeFromTop(27);
    addFolder.setBounds(first.removeFromLeft(first.getWidth()/2-3));first.removeFromLeft(6);
    rescan.setBounds(first);
    bounds.removeFromTop(5);
    auto second=bounds.removeFromTop(26);
    relink.setBounds(second.removeFromLeft(second.getWidth()/3-3));second.removeFromLeft(5);
    toggle.setBounds(second.removeFromLeft(second.getWidth()/2-3));second.removeFromLeft(5);
    remove.setBounds(second);
    bounds.removeFromTop(6);
    auto bottom=bounds.removeFromBottom(46);
    cancel.setBounds(bottom.removeFromRight(78).removeFromTop(25));
    status.setBounds(bottom);
    bounds.removeFromBottom(6);
    if(!editingTags){sources.setBounds(bounds);return;}
    sources.setBounds(bounds.removeFromTop(juce::jmin(105,bounds.getHeight()/5)));
    bounds.removeFromTop(5);
    tagInfo.setBounds(bounds.removeFromTop(22));
    auto searchRow=bounds.removeFromTop(26);
    assetSearch.setBounds(searchRow.removeFromLeft(searchRow.getWidth()*2/3-4));
    searchRow.removeFromLeft(6);suggestedTags.setBounds(searchRow);
    bounds.removeFromTop(4);
    auto controls=bounds.removeFromBottom(115);
    assets.setBounds(bounds);
    auto labels=controls.removeFromTop(26);
    tagGroup.setBounds(labels.removeFromLeft(juce::jmax(92,labels.getWidth()/4)));
    labels.removeFromLeft(5);tagLabel.setBounds(labels);
    controls.removeFromTop(4);
    auto buttons=controls.removeFromTop(25);
    const int each=juce::jmax(65,buttons.getWidth()/4);
    addTag.setBounds(buttons.removeFromLeft(each-3));buttons.removeFromLeft(4);
    confirmTag.setBounds(buttons.removeFromLeft(each-3));buttons.removeFromLeft(4);
    rejectTag.setBounds(buttons.removeFromLeft(each-3));buttons.removeFromLeft(4);
    undoTag.setBounds(buttons);
    controls.removeFromTop(4);
    auto metadata=controls.removeFromTop(25);
    bpmValue.setBounds(metadata.removeFromLeft(juce::jmax(72,metadata.getWidth()/5)));
    metadata.removeFromLeft(3);setBpm.setBounds(metadata.removeFromLeft(juce::jmax(70,metadata.getWidth()/4)));
    metadata.removeFromLeft(8);
    keyValue.setBounds(metadata.removeFromLeft(juce::jmax(70,metadata.getWidth()/2)));
    metadata.removeFromLeft(3);setKey.setBounds(metadata);
    controls.removeFromTop(4);
    auto rules=controls.removeFromTop(25);
    rulePhrase.setBounds(rules.removeFromLeft(juce::jmax(82,rules.getWidth()*2/3)));
    rules.removeFromLeft(4);ruleFolder.setBounds(rules.removeFromLeft(66));
    addRule.setBounds(rules);
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
    if(view&&row>=0&&row<static_cast<int>(view->sources.size()))
        library.set_view(view->sources[static_cast<std::size_t>(row)].id,assetSearch.getText().toStdString());
}

std::vector<std::int64_t> LibraryFoundationPanel::selectedAssetIds() const {
    std::vector<std::int64_t> ids;
    if(!view)return ids;
    const auto rows=assets.getSelectedRows();
    for(int i=0;i<rows.size();++i){
        const int row=rows[i];
        if(row>=0&&row<static_cast<int>(view->assets.size()))
            ids.push_back(view->assets[static_cast<std::size_t>(row)].id);
    }
    return ids;
}
void LibraryFoundationPanel::fillSuggestedTags(){
    suggestedTags.clear(juce::dontSendNotification);
    const auto row=assets.getSelectedRow();
    if(!view||row<0||row>=static_cast<int>(view->assets.size()))return;
    const auto& item=view->assets[static_cast<std::size_t>(row)];
    for(std::size_t i=0;i<item.tags.size();++i){
        const auto& tag=item.tags[i];
        suggestedTags.addItem(text(tag.group)+": "+text(tag.label)+" ["+text(tag.status)+"]",static_cast<int>(i)+1);
    }
    const int bpm=item.manual_bpm==0?item.detected_bpm:item.manual_bpm;
    const auto key=item.manual_key.empty()?item.detected_key:item.manual_key;
    tagInfo.setText("Selected: "+juce::String(static_cast<juce::int64>(item.id))+
        " | BPM "+(bpm>0?juce::String(bpm):"?")+
        " | Key "+(key.empty()||key=="-"?juce::String("?"):text(key))+
        " | "+juce::String(static_cast<int>(item.tags.size()))+" tags",juce::dontSendNotification);
}
void LibraryFoundationPanel::applyTag(const std::string& decision){
    const auto ids=selectedAssetIds();
    if(ids.empty())return;
    library.set_tag_status(ids,tagGroup.getText().toStdString(),
                           tagLabel.getText().trim().toStdString(),decision);
}
void LibraryFoundationPanel::updateMode(){
    modeButton.setButtonText(editingTags?"Browse":"Edit Tags");
    sourcesMode.setButtonText(browsing?"Sources":"Browse");
    if(browserView)browserView->setVisible(browsing);
    for(auto* c:std::array<juce::Component*,7>{&addFolder,&rescan,&cancel,&relink,&toggle,&remove,&sources})
        c->setVisible(!browsing);
    for(auto* c:std::array<juce::Component*,17>{&assets,&tagInfo,&addTag,&confirmTag,
        &rejectTag,&undoTag,&setBpm,&setKey,&addRule,&assetSearch,&tagLabel,&bpmValue,&keyValue,
        &rulePhrase,&tagGroup,&suggestedTags,&ruleFolder})c->setVisible(editingTags);
}

juce::StringArray LibraryFoundationPanel::selectedLibrarySamples() const {
    return browserView?browserView->selectedWavFiles():juce::StringArray{};
}
void LibraryFoundationPanel::timerCallback() { refresh(); }
void LibraryFoundationPanel::refresh() {
    auto next=library.snapshot();
    if(next->revision==lastRevision&&view)return;
    view=std::move(next);lastRevision=view->revision;
    if(browserView)browserView->setSnapshot(view);
    sources.updateContent();sources.repaint();
    assets.updateContent();assets.repaint();
    fillSuggestedTags();
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
