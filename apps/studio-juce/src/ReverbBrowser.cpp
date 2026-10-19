#include "ReverbBrowser.h"
#include <mrs/recording.hpp>
#include <windows.h>
#include <thread>
#include <set>
#include <map>

namespace ui {
namespace {
juce::String text(const std::string& s){return juce::String::fromUTF8(s.c_str());}
juce::String folded(const juce::String& value){
    if(value.isEmpty())return {};const std::wstring input(value.toWideCharPointer());
    // JUCE/C-runtime case conversion can depend on the process C locale. Use
    // invariant Unicode mapping so Cyrillic filename searches work on Windows.
    const auto size=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,input.data(),static_cast<int>(input.size()),nullptr,0,nullptr,nullptr,0);
    if(size<=0)return value.toLowerCase();std::wstring output(static_cast<std::size_t>(size),L'\0');
    LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,input.data(),static_cast<int>(input.size()),output.data(),size,nullptr,nullptr,0);return juce::String(output.c_str());
}
std::filesystem::path filePath(const juce::File& f){return {f.getFullPathName().toWideCharPointer()};}
juce::String mode(mrs::audio::ReverbFileKind kind){using K=mrs::audio::ReverbFileKind;
    return kind==K::mono?"Mono":kind==K::stereo?"Stereo":kind==K::quad||kind==K::pair?"True Stereo":"Unsupported";}
class FolderItem final : public juce::TreeViewItem {
public:
    FolderItem(juce::String n,juce::String p,std::function<void(juce::String)> f):name(std::move(n)),folder(std::move(p)),select(std::move(f)){}
    bool mightContainSubItems() override{return getNumSubItems()>0;}
    int getItemHeight() const override{return 24;}
    juce::String getUniqueName() const override{return folder.isEmpty()?"All folders":folder;}
    void paintItem(juce::Graphics& g,int width,int height) override{
        const auto* view=getOwnerView();const auto* theme=view?dynamic_cast<const Theme*>(&view->getLookAndFeel()):nullptr;const bool modern=theme&&theme->conceptSkin();
        if(isSelected())g.fillAll(juce::Colour(modern?ConceptSkin::selection:accent));g.setColour(modern&&isSelected()?juce::Colour(ConceptSkin::canvas):juce::Colours::whitesmoke);g.setFont(Theme::channelFont());g.drawText(name,4,0,width-8,height,juce::Justification::centredLeft,true);
    }
    void itemSelectionChanged(bool isChosen) override{if(isChosen)select(folder);}
    juce::String name,folder;std::function<void(juce::String)> select;
};
}
ReverbBrowser::ReverbBrowser(std::shared_ptr<mrs::audio::ReverbLibrary> l,std::shared_ptr<mrs::audio::ReverbLoader> v,ViewSettings& p,std::function<void()> persist,juce::String insertId)
    :library(std::move(l)),loader(std::move(v)),preferences(p),save(std::move(persist)){
    viewId=std::move(insertId);if(viewId.isNotEmpty()){const auto state=preferences.nativeEditor(viewId);search.setText(state.search,juce::dontSendNotification);selectedFolder=state.folder;}
    for(auto* c:std::array<juce::Component*,8>{&browse,&search,&folders,&quad,&list,&rootLabel,&info,&status})addAndMakeVisible(c);
    browse.setComponentID("reverb-browse-folder");search.setComponentID("reverb-search");list.setComponentID("reverb-ir-list");tree.setComponentID("reverb-folders");
    search.setTextToShowWhenEmpty("Search IR filename...",juce::Colours::grey);search.setTitle("Search IR filenames");
    search.setFont(Theme::detailFont());search.onTextChange=[this]{rememberView();filter();};folders.setVisible(false);addAndMakeVisible(tree);tree.setTitle("IR folders");tree.setRootItemVisible(true);tree.setColour(juce::TreeView::backgroundColourId,juce::Colour(surface));
    quad.setTitle("Confirm quad LL LR RL RR channel map for this folder");quad.setToggleState(preferences.reverbQuadConfirmed,juce::dontSendNotification);
    quad.onClick=[this]{preferences.reverbQuadConfirmed=quad.getToggleState();try{if(save)save();localError.clear();}catch(const std::exception& e){localError=text(e.what());}refresh();};
    browse.onClick=[this]{juce::Component::SafePointer<ReverbBrowser> safe(this);
        chooser=std::make_unique<juce::FileChooser>("Choose your IR library folder",juce::File(preferences.reverbRoot));
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const auto& selected){if(safe&&selected.getResult()!=juce::File())safe->chooseRoot(selected.getResult());});};
    list.setRowHeight(44);list.setColour(juce::ListBox::backgroundColourId,juce::Colour(surface));
    info.setJustificationType(juce::Justification::topLeft);status.setJustificationType(juce::Justification::topLeft);
    rootLabel.setTitle("IR library folder");status.setTitle("IR loading status");
    if(library->snapshot()->phase==mrs::audio::ReverbLibraryPhase::empty&&preferences.reverbRoot.isNotEmpty())library->set_root(filePath(juce::File(preferences.reverbRoot)));
    setSize(900,330);refresh();startTimerHz(20);
}
ReverbBrowser::~ReverbBrowser(){stopTimer();rememberView();flushView();chooser.reset();list.setModel(nullptr);tree.setRootItem(nullptr);}
void ReverbBrowser::rememberView(){if(viewId.isEmpty())return;auto& state=preferences.nativeEditor(viewId);state.search=search.getText().substring(0,1024);state.folder=selectedFolder.substring(0,4096);viewDirty=true;viewChangedAt=juce::Time::getMillisecondCounter();}
void ReverbBrowser::flushView(){if(!viewDirty)return;try{if(save)save();viewDirty=false;}catch(const std::exception& e){localError=text(e.what());}}
void ReverbBrowser::chooseRoot(const juce::File& root){
    if(loaderProvider)loader=loaderProvider();if(!loader)return;
    loader->cancel();preferences.reverbRoot=root.getFullPathName();preferences.reverbQuadConfirmed=false;quad.setToggleState(false,juce::dontSendNotification);selectedFolder.clear();rememberView();
    library->set_root(filePath(root));try{if(save)save();localError.clear();}catch(const std::exception& e){localError=text(e.what());}refresh();
}
void ReverbBrowser::resized(){auto area=getLocalBounds().reduced(12);auto top=area.removeFromTop(28);browse.setBounds(top.removeFromRight(145));rootLabel.setBounds(top);
    area.removeFromTop(8);auto bottom=area.removeFromBottom(42);quad.setBounds(bottom.removeFromLeft(280));status.setBounds(bottom);
    auto filters=area.removeFromLeft(juce::jlimit(150,190,getWidth()/5));search.setBounds(filters.removeFromTop(28));filters.removeFromTop(8);tree.setBounds(filters);
    area.removeFromLeft(10);info.setBounds(area.removeFromRight(juce::jlimit(160,225,getWidth()/4)).reduced(8));area.removeFromRight(10);list.setBounds(area);
}
void ReverbBrowser::lookAndFeelChanged(){
    const auto* theme=dynamic_cast<const Theme*>(&getLookAndFeel());const bool modern=theme&&theme->conceptSkin();
    const auto fill=modern?DesignTokens::target().colour(ColourRole::window):juce::Colour(surface);
    tree.setColour(juce::TreeView::backgroundColourId,fill);list.setColour(juce::ListBox::backgroundColourId,fill);
    if(modern)search.setColour(juce::TextEditor::focusedOutlineColourId,juce::Colour(ConceptSkin::selection));
    else search.removeColour(juce::TextEditor::focusedOutlineColourId);
}
void ReverbBrowser::paint(juce::Graphics& g){const auto* theme=dynamic_cast<const Theme*>(&getLookAndFeel());drawCard(g,getLocalBounds().toFloat(),theme&&theme->conceptSkin()?DesignTokens::target().colour(ColourRole::window):juce::Colour(contentSurface));g.setColour(juce::Colour(outline));g.drawRoundedRectangle(list.getBounds().toFloat().reduced(.5f),cornerRadius,borderWidth);}
void ReverbBrowser::filter(){filtered.clear();if(index){const auto query=folded(search.getText());
    for(std::size_t i=0;i<index->entries.size();++i){const auto& entry=index->entries[i];
        if((selectedFolder.isEmpty()||text(entry.folder)==selectedFolder||text(entry.folder).startsWith(selectedFolder+"/"))&&folded(text(entry.name)).contains(query))filtered.push_back(i);}}
    list.deselectAllRows();list.updateContent();list.repaint();
}
void ReverbBrowser::refresh(){if(loaderProvider)loader=loaderProvider();if(!loader){status.setText("Audio disconnected — reopen editor after connecting",juce::dontSendNotification);return;}const auto current=library->snapshot();if(current!=index){
    // A shared root change invalidates this view's in-flight request, not its active IR.
    index=current; // Loader itself rejects obsolete catalogue generations.
    rootLabel.setText(juce::String(index->root.wstring().c_str()),juce::dontSendNotification);rootLabel.setTooltip(rootLabel.getText());
    tree.setRootItem(nullptr);auto select=[this](juce::String f){selectedFolder=std::move(f);rememberView();filter();};
    treeRoot=std::make_unique<FolderItem>("All folders","",select);std::map<juce::String,juce::TreeViewItem*> items;items[""]=treeRoot.get();
    for(const auto& e:index->entries){auto parts=juce::StringArray::fromTokens(text(e.folder),"/","");juce::String parent;
        for(const auto& part:parts){if(part.isEmpty())continue;auto key=parent.isEmpty()?part:parent+"/"+part;if(!items.contains(key)){auto* item=new FolderItem(part,key,select);items[parent]->addSubItem(item);items[key]=item;}parent=key;}}
    treeRoot->setOpen(true);tree.setRootItem(treeRoot.get());if(items.contains(selectedFolder)){auto* chosen=items[selectedFolder];for(auto* parent=chosen->getParentItem();parent;parent=parent->getParentItem())parent->setOpen(true);chosen->setSelected(true,true,juce::dontSendNotification);}filter();
}
    const auto loaded=loader->snapshot();juce::String details;
    int activeRow=-1;if(loaded->active&&index)for(std::size_t row=0;row<filtered.size();++row){const auto& entry=index->entries[filtered[row]];if(entry.key==loaded->active->entry.key&&(index->root/entry.first).lexically_normal()==loaded->active->first.absolute.lexically_normal()){activeRow=static_cast<int>(row);break;}}
    if(list.getSelectedRow()!=activeRow){if(activeRow>=0)list.selectRow(activeRow);else list.deselectAllRows();}
    if(loaded->active){const auto& e=loaded->active->entry;details=text(e.name)+"\n\n"+mode(e.kind)+"\n"+juce::String(e.sample_rate)+" Hz / "+juce::String(e.duration(),2)+" s\n\nExternal user IR";}
    else if(rememberedStateProvider){const auto saved=rememberedStateProvider();if(!saved.name.empty())details=text(saved.name)+"\n\n"+(saved.channels==4?"True Stereo":saved.channels==2?"Stereo":"Mono")+"\n"+juce::String(saved.sample_rate)+" Hz / "+juce::String(saved.sample_rate?static_cast<double>(saved.frames)/saved.sample_rate:0.,2)+" s\n\n"+text(loaded->message);}
    if(details.isEmpty())details="Select an IR\n\nQuad files need an explicitly confirmed channel map.";
    info.setText(details,juce::dontSendNotification);auto message=text(loaded->message);
    if(index->phase==mrs::audio::ReverbLibraryPhase::scanning)message="Scanning IR folder...";
    else if(index->phase==mrs::audio::ReverbLibraryPhase::missing||index->phase==mrs::audio::ReverbLibraryPhase::error)message=text(index->message);
    else if(message.isEmpty())message=index->entries.empty()?"Choose an IR folder":"Choose an IR";
    if(localError.isNotEmpty())message=localError;status.setText(message,juce::dontSendNotification);status.setTooltip(message);
}
void ReverbBrowser::activate(int row){if(!index||row<0||row>=visibleRows())return;const auto& entry=index->entries[filtered[static_cast<std::size_t>(row)]];
    if(loaderProvider)loader=loaderProvider();if(!loader)return;
    localError.clear();if(entry.kind==mrs::audio::ReverbFileKind::quad&&!preferences.reverbQuadConfirmed){localError="Confirm quad channel map LL / LR / RL / RR before loading";refresh();return;}
    loader->select(entry,index->root,preferences.reverbQuadConfirmed?mrs::processing::ReverbQuadMap::ll_lr_rl_rr:mrs::processing::ReverbQuadMap::unspecified);refresh();
}
void ReverbBrowser::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected){if(!index||row<0||row>=visibleRows())return;
    const auto* theme=dynamic_cast<const Theme*>(&getLookAndFeel());const bool modern=theme&&theme->conceptSkin();
    const auto& e=index->entries[filtered[static_cast<std::size_t>(row)]];if(selected)g.fillAll(juce::Colour(modern?ConceptSkin::selection:accent));
    const auto ink=modern&&selected?juce::Colour(ConceptSkin::canvas):juce::Colours::whitesmoke;
    g.setFont(Theme::channelFont());g.setColour(ink);g.drawText(text(e.name),8,2,width-16,20,juce::Justification::centredLeft,true);
    g.setFont(Theme::detailFont());g.setColour(modern&&selected?ink:juce::Colours::lightgrey);auto meta=mode(e.kind);
    if(e.kind==mrs::audio::ReverbFileKind::quad&&!preferences.reverbQuadConfirmed)meta="Quad — confirm map";
    meta+=" | "+juce::String(e.sample_rate)+" Hz | "+juce::String(e.duration(),2)+" s";
    g.drawText(meta,8,23,width-16,height-23,juce::Justification::centredLeft,true);
}
void reverbBrowserSmoke(){
    auto check=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);const auto folder=temp.getChildFile("mrs-reverb-browser-"+text(mrs::new_id().value));
    check(folder.getParentDirectory()==temp&&!folder.exists(),"browser fixture scope");check(folder.createDirectory().wasOk(),"fixture folder");
    struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}}cleanup{folder};
    std::array<float,256> samples{};samples[0]=.5f;samples[1]=.25f;
    mrs::audio::Recorder writer(filePath(folder.getChildFile(juce::String::fromUTF8("Зал с длинным названием.wav"))),48000,0,{0,1});writer.capture(samples.data(),2,128,0);writer.finish();
    mrs::processing::GraphState state;state.id={"browser-fixture"};state.patch_name="Browser fixture";
    state.nodes={{{"reverb"},"mr_reverb",mrs::processing::ProcessorFormat::native,{},{{2,1}},false}};state.edges={{std::nullopt,{"reverb"},1}};state.outputs={{{"reverb"}}};
    auto graph=std::make_shared<mrs::processing::PreparedGraph>(mrs::processing::GraphSnapshot{std::make_shared<const mrs::processing::GraphState>(state),0,false,false},mrs::processing::ProcessConfig{48000,2,128});
    auto library=std::make_shared<mrs::audio::ReverbLibrary>();auto loader=std::make_shared<mrs::audio::ReverbLoader>(graph,mrs::Id{"reverb"},mrs::processing::ProcessConfig{48000,2,128},library);
    ViewSettings prefs;const auto prefFile=folder.getChildFile("prefs.json");Theme theme;ReverbBrowser browser(library,loader,prefs,[&]{publishSettings(prefFile,prefs.encode());});browser.setLookAndFeel(&theme);
    struct Detach{ReverbBrowser& b;~Detach(){b.setLookAndFeel(nullptr);}}detach{browser};browser.setVisible(true);browser.chooseRoot(folder);
    auto wait=[&](auto predicate,const char* step){for(int i=0;i<3000;++i){MSG message{};int count{};while(count++<100&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}browser.refresh();if(predicate())return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error(std::string("browser timeout: ")+step+" / "+library->snapshot()->message+" / "+loader->snapshot()->message);};
    wait([&]{return browser.visibleRows()==1;},"initial index");check(ViewSettings::decode(prefFile.loadFileAsString()).reverbRoot==folder.getFullPathName(),"folder preference persisted");
    browser.searchFor(juce::String::fromUTF8("не найден"));wait([&]{return browser.visibleRows()==0;},"empty search");browser.searchFor(juce::String::fromUTF8("зал"));wait([&]{return browser.visibleRows()==1;},"Unicode search");
    browser.activate(0);wait([&]{return loader->snapshot()->phase==mrs::audio::ReverbLoadPhase::ready;},"IR loading");check(loader->snapshot()->active.has_value(),"click loaded IR identity");
    for(float scale:{1.f,1.5f}){auto image=browser.createComponentSnapshot(browser.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("reverb-browser-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();
        check(stream!=nullptr,"browser preview");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"browser snapshot");}
    browser.chooseRoot(folder.getChildFile("missing"));wait([&]{return library->snapshot()->phase==mrs::audio::ReverbLibraryPhase::missing;},"missing root");check(loader->snapshot()->active.has_value(),"missing root retains active IR");
}
}
