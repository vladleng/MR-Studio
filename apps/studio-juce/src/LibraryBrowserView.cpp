#include "LibraryBrowserView.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>

namespace ui {
namespace {
juce::String uiText(const std::string& value){return juce::String::fromUTF8(value.c_str());}
juce::String pathText(const std::filesystem::path& value){return juce::String(value.wstring().c_str());}
juce::Colour muted(){return juce::Colour(0xff98a8bd);}
std::int64_t selectId(const juce::ComboBox& box,const std::vector<std::int64_t>& ids){
    const auto n=box.getSelectedId()-1;
    return n>=0&&n<static_cast<int>(ids.size())?ids[static_cast<std::size_t>(n)]:0;
}
std::string selectString(const juce::ComboBox& box,const std::vector<std::string>& items){
    const auto n=box.getSelectedId()-1;
    return n>=0&&n<static_cast<int>(items.size())?items[static_cast<std::size_t>(n)]:std::string{};
}
}
LibraryBrowserView::LibraryBrowserView(mrs::library::SampleLibrary& lib):library(lib){
    for(auto* control:std::array<juce::Component*,29>{
        &foldersTab,&tagsTab,&collectionsTab,&favorites,&clearFilters,&listGrid,
        &addFilter,&removeFilter,&newCollection,&deleteCollection,&addToCollection,
        &removeFromCollection,&favoriteSelected,&chooseCover,&resetCover,&prevPage,
        &nextPage,&sources,&folders,&tagGroups,&tagValues,&selectedFilters,&collections,
        &sorts,&search,&bpmFrom,&bpmTo,&keyField,&collectionName})
        addAndMakeVisible(control);
    addAndMakeVisible(counter);addAndMakeVisible(filterLabel);addAndMakeVisible(results);
    addAndMakeVisible(previewName);addAndMakeVisible(previewMeta);
    previewName.setText("Select WAV to inspect",juce::dontSendNotification);
    previewName.setJustificationType(juce::Justification::centredLeft);
    previewMeta.setJustificationType(juce::Justification::centredLeft);
    results.startLibraryDrag=[this]{startLibraryDrag();};
    results.setRowHeight(49);results.setMultipleSelectionEnabled(true);
    results.setComponentID("mr-library-browser-results");
    results.setColour(juce::ListBox::backgroundColourId,juce::Colour(surface));
    search.setTextToShowWhenEmpty("Search samples / tags...",muted());
    bpmFrom.setTextToShowWhenEmpty("BPM min",muted());
    bpmTo.setTextToShowWhenEmpty("BPM max",muted());
    keyField.setTextToShowWhenEmpty("Key / ?",muted());
    collectionName.setTextToShowWhenEmpty("New collection name",muted());
    favorites.setTooltip("Show only starred samples");
    chooseCover.setTooltip("Choose JPG/PNG within the selected sample library");
    resetCover.setTooltip("Restore automatic cover for this folder");
    for(const auto& [label,id]:std::array<std::pair<const char*,int>,4>{{
        {"Name",1},{"BPM",2},{"Duration",3},{"Modified",4}}})sorts.addItem(label,id);
    sorts.setSelectedId(1,juce::dontSendNotification);
    foldersTab.onClick=[this]{chooseSection(Section::folders);};
    tagsTab.onClick=[this]{chooseSection(Section::tags);};
    collectionsTab.onClick=[this]{chooseSection(Section::collections);};
    sources.onChange=[this]{
        query.source_id=selectId(sources,sourceIds);query.folder.clear();query.offset=0;sendQuery();
    };
    folders.onChange=[this]{query.folder=selectString(folders,folderNames);query.offset=0;sendQuery();};
    collections.onChange=[this]{
        query.collection_id=selectId(collections,collectionIds);query.offset=0;sendQuery();
    };
    tagGroups.onChange=[this]{updateFacets();};
    addFilter.onClick=[this]{
        const auto group=selectString(tagGroups,facetGroups);
        const auto label=selectString(tagValues,facetLabels);
        if(group.empty()||label.empty())return;
        const auto found=std::find_if(query.tags.begin(),query.tags.end(),[&](const auto& x){
            return x.group==group&&x.label==label;
        });
        if(found==query.tags.end())query.tags.push_back({group,label});
        query.offset=0;sendQuery();
    };
    removeFilter.onClick=[this]{
        const auto row=selectedFilters.getSelectedId()-1;
        if(row<0||row>=static_cast<int>(query.tags.size()))return;
        query.tags.erase(query.tags.begin()+row);query.offset=0;sendQuery();
    };
    favorites.onClick=[this]{
        query.favorites_only=!query.favorites_only;
        favorites.setToggleState(query.favorites_only,juce::dontSendNotification);
        query.offset=0;sendQuery();
    };
    clearFilters.onClick=[this]{
        const auto source=query.source_id;
        query=mrs::library::LibraryQuery{};query.source_id=source;
        search.clear();bpmFrom.clear();bpmTo.clear();keyField.clear();
        favorites.setToggleState(false,juce::dontSendNotification);sendQuery();
    };
    listGrid.onClick=[this]{
        gridView=!gridView;listGrid.setButtonText(gridView?"List":"Grid");
        gridSelected=-1;results.deselectAllRows();
        results.setRowHeight(gridView?134:49);results.updateContent();results.repaint();
    };
    search.onTextChange=[this]{query.search=search.getText().toStdString();query.offset=0;sendQuery();};
    sorts.onChange=[this]{
        static constexpr std::array<const char*,4> modes={"name","bpm","duration","updated"};
        const int index=sorts.getSelectedId()-1;
        if(index>=0&&index<4)query.sort=modes[static_cast<std::size_t>(index)];
        query.offset=0;sendQuery();
    };
    const auto updateMusical=[this]{
        const auto low=bpmFrom.getText().trim(),high=bpmTo.getText().trim();
        query.bpm_min=low.isEmpty()?0:juce::jlimit(0,250,low.getIntValue());
        query.bpm_max=high.isEmpty()?0:juce::jlimit(0,250,high.getIntValue());
        query.key=keyField.getText().trim().toStdString();query.offset=0;sendQuery();
    };
    bpmFrom.onReturnKey=updateMusical;bpmTo.onReturnKey=updateMusical;
    keyField.onReturnKey=updateMusical;
    newCollection.onClick=[this]{
        const auto name=collectionName.getText().trim();
        if(name.isNotEmpty())library.create_collection(name.toStdString());
    };
    deleteCollection.onClick=[this]{
        const auto id=selectId(collections,collectionIds);
        if(id<=0)return;
        juce::Component::SafePointer<LibraryBrowserView> safe(this);
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
            "Delete collection?","Remove the virtual collection? Original WAVs will not be touched.",
            "Remove","Cancel",this,
            juce::ModalCallbackFunction::create([safe,id](int result){
                if(safe&&result)safe->library.delete_collection(id);
            }));
    };
    addToCollection.onClick=[this]{
        const auto id=selectId(collections,collectionIds);
        const auto items=selectedAssets();
        if(id>0&&!items.empty())library.set_collection_items(id,items,true);
    };
    removeFromCollection.onClick=[this]{
        const auto id=selectId(collections,collectionIds);
        const auto items=selectedAssets();
        if(id>0&&!items.empty())library.set_collection_items(id,items,false);
    };
    favoriteSelected.onClick=[this]{
        const auto items=selectedAssets();if(items.empty()||!view)return;
        bool all=true;
        for(const auto id:items){
            const auto found=std::find_if(view->assets.begin(),view->assets.end(),
                [id](const auto& x){return x.id==id;});
            if(found==view->assets.end()||!found->favorite){all=false;break;}
        }
        library.set_favorites(items,!all);
    };
    prevPage.onClick=[this]{
        query.offset=query.offset>query.limit?query.offset-query.limit:0;sendQuery();
    };
    nextPage.onClick=[this]{
        if(view&&query.offset+query.limit<view->visible_assets){query.offset+=query.limit;sendQuery();}
    };
    chooseCover.onClick=[this]{chooseArtwork(false);};
    resetCover.onClick=[this]{chooseArtwork(true);};
    filterLabel.setText("Select a tag, then Add filter",juce::dontSendNotification);
    filterLabel.setJustificationType(juce::Justification::centredLeft);
    counter.setJustificationType(juce::Justification::centredLeft);
    imageWorker=std::jthread([this](std::stop_token stop){
        for(;;){
            std::pair<std::string,juce::File> work;
            {
                std::unique_lock lock(imageMutex);
                imageWake.wait(lock,stop,[this]{return !pendingImages.empty();});
                if(stop.stop_requested())break;
                work=std::move(pendingImages.front());pendingImages.pop_front();
            }
            juce::Image image;
            if(work.second.existsAsFile()&&work.second.getSize()<=20*1024*1024){
                image=juce::ImageFileFormat::loadFrom(work.second);
                if(image.isValid())image=image.rescaled(94,94,juce::Graphics::mediumResamplingQuality);
            }
            {
                std::lock_guard lock(imageMutex);
                if(imageCache.size()>95){
                    requestedImages.erase(imageCache.begin()->first);
                    imageCache.erase(imageCache.begin());
                }
                imageCache.emplace(work.first,std::move(image));
            }
            readyImages.fetch_add(1);
        }
    });
    updateVisibility();startTimerHz(6);
}
LibraryBrowserView::~LibraryBrowserView(){
    stopTimer();imageChooser.reset();results.setModel(nullptr);
    if(waveformCancel)waveformCancel->store(true);
    if(waveformWorker.valid())waveformWorker.wait();
    imageWorker.request_stop();imageWake.notify_all();
    if(imageWorker.joinable())imageWorker.join();
}
void LibraryBrowserView::chooseSection(Section next){
    section=next;updateVisibility();resized();
}
void LibraryBrowserView::sendQuery(){
    if(query.limit==0)query.limit=120;
    library.browse(query);
}
void LibraryBrowserView::setSnapshot(std::shared_ptr<const mrs::library::Snapshot> next){
    if(!next||revision==next->revision)return;
    view=std::move(next);revision=view->revision;
    query=view->query;
    refreshControls();
    gridSelected=-1;
    results.updateContent();results.repaint();
    prepareSelected();
}
void LibraryBrowserView::refreshControls(){
    if(!view)return;
    sourceIds.clear();sources.clear(juce::dontSendNotification);
    sourceIds.push_back(0);sources.addItem("All libraries",1);
    for(const auto& item:view->sources){
        sourceIds.push_back(item.id);
        sources.addItem(pathText(item.root.filename()),static_cast<int>(sourceIds.size()));
    }
    const auto src=std::find(sourceIds.begin(),sourceIds.end(),query.source_id);
    sources.setSelectedId(src==sourceIds.end()?1:static_cast<int>(src-sourceIds.begin())+1,juce::dontSendNotification);
    folderNames.clear();folders.clear(juce::dontSendNotification);
    folderNames.push_back("");folders.addItem("All folders",1);
    for(const auto& item:view->folders){
        if(query.source_id!=0&&item.source_id!=query.source_id)continue;
        if(item.relative_path.empty())continue;
        if(std::find(folderNames.begin(),folderNames.end(),item.relative_path)!=folderNames.end())continue;
        folderNames.push_back(item.relative_path);
        folders.addItem(uiText(item.relative_path),static_cast<int>(folderNames.size()));
    }
    const auto folder=std::find(folderNames.begin(),folderNames.end(),query.folder);
    folders.setSelectedId(folder==folderNames.end()?1:static_cast<int>(folder-folderNames.begin())+1,
                          juce::dontSendNotification);
    collectionIds.clear();collections.clear(juce::dontSendNotification);
    collectionIds.push_back(0);collections.addItem("All collections",1);
    for(const auto& item:view->collections){
        collectionIds.push_back(item.id);collections.addItem(uiText(item.name)+" ("+
            juce::String(static_cast<juce::int64>(item.count))+")",static_cast<int>(collectionIds.size()));
    }
    const auto collection=std::find(collectionIds.begin(),collectionIds.end(),query.collection_id);
    collections.setSelectedId(collection==collectionIds.end()?1:static_cast<int>(collection-collectionIds.begin())+1,
                             juce::dontSendNotification);
    facetGroups.clear();tagGroups.clear(juce::dontSendNotification);
    for(const auto& item:view->facets){
        if(std::find(facetGroups.begin(),facetGroups.end(),item.group)==facetGroups.end()){
            facetGroups.push_back(item.group);tagGroups.addItem(uiText(item.group),static_cast<int>(facetGroups.size()));
        }
    }
    if(!facetGroups.empty())tagGroups.setSelectedId(1,juce::dontSendNotification);
    updateFacets();
    selectedFilters.clear(juce::dontSendNotification);
    for(std::size_t i=0;i<query.tags.size();++i){
        selectedFilters.addItem(uiText(query.tags[i].group+": "+query.tags[i].label),static_cast<int>(i)+1);
    }
    if(!query.tags.empty())selectedFilters.setSelectedId(1,juce::dontSendNotification);
    counter.setText(juce::String(static_cast<juce::int64>(view->visible_assets))+" WAV | "+
        juce::String(static_cast<juce::int64>(query.offset+1))+"-"+
        juce::String(static_cast<juce::int64>(std::min(query.offset+query.limit,view->visible_assets)))+
        " | "+(view->assets.empty()?"No matches":"Indexed locally"),juce::dontSendNotification);
    prevPage.setEnabled(query.offset>0);
    nextPage.setEnabled(query.offset+query.limit<view->visible_assets);
}
void LibraryBrowserView::updateFacets(){
    facetLabels.clear();tagValues.clear(juce::dontSendNotification);
    if(!view)return;
    const auto group=selectString(tagGroups,facetGroups);
    for(const auto& item:view->facets)if(item.group==group){
        facetLabels.push_back(item.label);
        tagValues.addItem(uiText(item.label)+" ("+
            juce::String(static_cast<juce::int64>(item.count))+")",static_cast<int>(facetLabels.size()));
    }
    if(!facetLabels.empty())tagValues.setSelectedId(1,juce::dontSendNotification);
}
void LibraryBrowserView::updateVisibility(){
    foldersTab.setToggleState(section==Section::folders,juce::dontSendNotification);
    tagsTab.setToggleState(section==Section::tags,juce::dontSendNotification);
    collectionsTab.setToggleState(section==Section::collections,juce::dontSendNotification);
    for(auto* c:std::array<juce::Component*,3>{&folders,&chooseCover,&resetCover})
        c->setVisible(section==Section::folders);
    for(auto* c:std::array<juce::Component*,9>{&tagGroups,&tagValues,&addFilter,&removeFilter,
        &selectedFilters,&filterLabel,&bpmFrom,&bpmTo,&keyField})
        c->setVisible(section==Section::tags);
    for(auto* c:std::array<juce::Component*,6>{&collections,&newCollection,&deleteCollection,
        &addToCollection,&removeFromCollection,&collectionName})
        c->setVisible(section==Section::collections);
}
void LibraryBrowserView::resized(){
    auto area=getLocalBounds().reduced(6);
    auto tabs=area.removeFromTop(26);
    const auto each=juce::jmax(1,tabs.getWidth()/3);
    foldersTab.setBounds(tabs.removeFromLeft(each-2));tabs.removeFromLeft(3);
    tagsTab.setBounds(tabs.removeFromLeft(each-2));tabs.removeFromLeft(3);
    collectionsTab.setBounds(tabs);
    area.removeFromTop(5);
    auto searchRow=area.removeFromTop(26);
    search.setBounds(searchRow);
    area.removeFromTop(4);
    auto mainRow=area.removeFromTop(25);
    sources.setBounds(mainRow.removeFromLeft(mainRow.getWidth()/2-3));
    mainRow.removeFromLeft(6);sorts.setBounds(mainRow);
    area.removeFromTop(5);
    auto context=area.removeFromTop(26);
    if(section==Section::folders){
        folders.setBounds(context.removeFromLeft(context.getWidth()*3/5-3));
        context.removeFromLeft(5);chooseCover.setBounds(context.removeFromLeft(context.getWidth()/2));
        resetCover.setBounds(context);
    }else if(section==Section::tags){
        tagGroups.setBounds(context.removeFromLeft(context.getWidth()/3-2));
        context.removeFromLeft(4);tagValues.setBounds(context);
    }else{
        collections.setBounds(context.removeFromLeft(context.getWidth()*2/3-2));
        context.removeFromLeft(5);deleteCollection.setBounds(context);
    }
    area.removeFromTop(4);
    auto context2=area.removeFromTop(25);
    if(section==Section::tags){
        addFilter.setBounds(context2.removeFromLeft(context2.getWidth()/3-2));
        context2.removeFromLeft(5);selectedFilters.setBounds(context2.removeFromLeft(context2.getWidth()*2/3-2));
        removeFilter.setBounds(context2);
    }else if(section==Section::collections){
        collectionName.setBounds(context2.removeFromLeft(context2.getWidth()*2/3-3));
        context2.removeFromLeft(5);newCollection.setBounds(context2);
    }else{
        favorites.setBounds(context2.removeFromLeft(context2.getWidth()/3-2));
        context2.removeFromLeft(5);clearFilters.setBounds(context2.removeFromLeft(context2.getWidth()/2-2));
        context2.removeFromLeft(4);listGrid.setBounds(context2);
    }
    area.removeFromTop(4);
    if(section==Section::tags){
        auto musical=area.removeFromTop(24);
        bpmFrom.setBounds(musical.removeFromLeft(musical.getWidth()/3-2));
        musical.removeFromLeft(3);bpmTo.setBounds(musical.removeFromLeft(musical.getWidth()/2-2));
        musical.removeFromLeft(3);keyField.setBounds(musical);
    }else if(section==Section::collections){
        auto ops=area.removeFromTop(24);
        addToCollection.setBounds(ops.removeFromLeft(ops.getWidth()/2-2));
        ops.removeFromLeft(4);removeFromCollection.setBounds(ops);
    }
    area.removeFromTop(4);
    auto actions=area.removeFromTop(24);
    favoriteSelected.setBounds(actions.removeFromLeft(actions.getWidth()/2-2));
    actions.removeFromLeft(4);listGrid.setBounds(actions.removeFromLeft(actions.getWidth()/2));
    actions.removeFromLeft(4);clearFilters.setBounds(actions);
    area.removeFromTop(4);
    auto bottom=area.removeFromBottom(27);
    prevPage.setBounds(bottom.removeFromLeft(58));bottom.removeFromLeft(4);
    nextPage.setBounds(bottom.removeFromLeft(58));bottom.removeFromLeft(6);
    counter.setBounds(bottom);
    area.removeFromBottom(4);
    auto previewArea=area.removeFromBottom(118);
    auto textArea=previewArea.removeFromTop(45);
    previewName.setBounds(textArea.removeFromTop(22));
    previewMeta.setBounds(textArea);
    results.setBounds(area);
}
void LibraryBrowserView::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(surface));
    auto area=getLocalBounds().reduced(6);
    area.removeFromBottom(27);
    auto waveArea=area.removeFromBottom(66).reduced(1,5);
    g.setColour(juce::Colour(0xff243044));g.fillRoundedRectangle(waveArea.toFloat(),5.f);
    g.setColour(juce::Colour(0xff66768a));
    const auto centre=waveArea.getCentreY();
    g.drawHorizontalLine(centre,static_cast<float>(waveArea.getX()+2),
                          static_cast<float>(waveArea.getRight()-2));
    if(waveform){
        g.setColour(juce::Colour(0xff86bedb));
        for(int i=0;i<waveArea.getWidth();++i){
            const int bin=std::min(255,i*256/std::max(1,waveArea.getWidth()));
            const auto amp=std::clamp(waveform->peaks[static_cast<std::size_t>(bin)],0.f,1.f);
            const auto y=amp*static_cast<float>(waveArea.getHeight()-8)*.5f;
            g.drawVerticalLine(waveArea.getX()+i,centre-y,centre+y);
        }
    }
}
std::vector<std::int64_t> LibraryBrowserView::selectedAssets() const {
    std::vector<std::int64_t> ids;
    if(!view)return ids;
    if(gridView){
        if(gridSelected>=0&&gridSelected<static_cast<int>(view->assets.size()))
            ids.push_back(view->assets[static_cast<std::size_t>(gridSelected)].id);
        return ids;
    }
    const auto rows=results.getSelectedRows();
    for(int i=0;i<rows.size();++i)
        if(rows[i]>=0&&rows[i]<static_cast<int>(view->assets.size()))
            ids.push_back(view->assets[static_cast<std::size_t>(rows[i])].id);
    return ids;
}
int LibraryBrowserView::getNumRows(){
    if(!view)return 0;
    const auto cols=gridView&&results.getWidth()>=340?2:1;
    return static_cast<int>((view->assets.size()+static_cast<std::size_t>(cols)-1)/static_cast<std::size_t>(cols));
}
juce::Image LibraryBrowserView::thumbnail(const mrs::library::AudioAsset& asset){
    if(!view||asset.artwork_path.empty())return {};
    const auto source=std::find_if(view->sources.begin(),view->sources.end(),
        [&](const auto& x){return x.id==asset.source_id;});
    if(source==view->sources.end())return {};
    const auto file=juce::File(pathText(source->root)).getChildFile(uiText(asset.artwork_path));
    const auto name=file.getFullPathName().toStdString();
    {
        std::lock_guard lock(imageMutex);
        auto existing=imageCache.find(name);
        if(existing!=imageCache.end())return existing->second;
        if(requestedImages.insert(name).second&&pendingImages.size()<64){
            pendingImages.emplace_back(name,file);imageWake.notify_one();
        }
    }
    return {};
}
void LibraryBrowserView::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected){
    if(!view)return;
    const auto cols=gridView&&width>=340?2:1;
    for(int col=0;col<cols;++col){
        const auto index=row*cols+col;
        if(index>=static_cast<int>(view->assets.size()))continue;
        const auto& item=view->assets[static_cast<std::size_t>(index)];
        const auto cellW=width/cols;
        const int x=col*cellW;
        const auto selectedCell=gridView?(gridSelected==index):selected;
        auto rect=juce::Rectangle<int>(x+2,2,cellW-5,height-4);
        if(selectedCell)g.setColour(juce::Colour(0xff375a80));
        else g.setColour(juce::Colour(gridView?0xff263244:0xff222b38));
        g.fillRoundedRectangle(rect.toFloat(),5.f);
        g.setColour(juce::Colour(0xff526076));g.drawRoundedRectangle(rect.toFloat(),5.f,0.7f);
        const int artWidth=gridView?74:32;
        auto cover=thumbnail(item);
        if(cover.isValid())g.drawImageWithin(cover,x+6,6,artWidth,height-12,
            juce::RectanglePlacement::centred,false);
        else {
            g.setColour(juce::Colour(0xff3a4a61));
            g.fillRoundedRectangle(static_cast<float>(x+6),6.f,static_cast<float>(artWidth),
                static_cast<float>(height-12),4.f);
            g.setColour(muted());g.setFont(juce::Font(juce::FontOptions(11.f)));
            g.drawFittedText("WAV",x+6,6,artWidth,height-12,juce::Justification::centred,1);
        }
        const auto left=x+artWidth+12;
        const auto right=cellW-artWidth-20;
        g.setColour(juce::Colours::whitesmoke);g.setFont(Theme::detailFont());
        const auto file=uiText(item.relative_path);
        g.drawFittedText(file,left,4,right,18,juce::Justification::centredLeft,1);
        g.setColour(muted());g.setFont(juce::Font(juce::FontOptions(11.f)));
        const int bpm=item.manual_bpm==0?item.detected_bpm:item.manual_bpm;
        const auto key=item.manual_key.empty()?item.detected_key:item.manual_key;
        const auto duration=item.sample_rate>0?
            static_cast<double>(item.frames)/static_cast<double>(item.sample_rate):0.;
        juce::String details=(item.favorite?juce::String::fromUTF8("★ "):juce::String{})+
            (bpm>0?juce::String(bpm)+" BPM  ":"")+
            ((key.empty()||key=="-")?juce::String{}:uiText(key)+"  ")+
            juce::String(duration,1)+" s";
        g.drawFittedText(details,left,23,right,17,juce::Justification::centredLeft,1);
        if(gridView){
            juce::String tags;
            for(const auto& t:item.tags)if(t.status!="rejected"){
                if(tags.isNotEmpty())tags+=", ";
                tags+=uiText(t.label);
            }
            g.drawFittedText(tags,left,43,right,height-49,juce::Justification::topLeft,3);
        }
    }
}
void LibraryBrowserView::selectedRowsChanged(int){
    if(!gridView)prepareSelected();
}
void LibraryBrowserView::listBoxItemClicked(int row,const juce::MouseEvent& event){
    if(!gridView||!view)return;
    const auto cols=results.getWidth()>=340?2:1;
    const int col=cols==2&&event.x>=results.getWidth()/2?1:0;
    gridSelected=row*cols+col;
    prepareSelected();
    results.repaint();
}
void LibraryBrowserView::chooseArtwork(bool clear){
    if(!view||query.source_id<=0||section!=Section::folders)return;
    const auto source=query.source_id;
    const auto folder=query.folder;
    if(clear){library.set_artwork(source,folder,{});return;}
    imageChooser=std::make_unique<juce::FileChooser>("Choose cover inside sample library",juce::File{},"*.jpg;*.jpeg;*.png");
    juce::Component::SafePointer<LibraryBrowserView> safe(this);
    imageChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe,source,folder](const juce::FileChooser& chooser){
            if(safe&&chooser.getResult().existsAsFile()){
                const auto path=std::filesystem::path(chooser.getResult().getFullPathName().toWideCharPointer());
                safe->library.set_artwork(source,folder,path);
            }
        });
}
void LibraryBrowserView::timerCallback(){
    if(waveformWorker.valid()&&
       waveformWorker.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
        try{
            const auto result=waveformWorker.get();
            if(juce::File(pathText(result.path))==wantedPreview){
                waveform=std::move(result);
                previewMeta.setText(juce::String(waveform->rate)+" Hz | "+
                    juce::String(waveform->channels)+" ch | "+juce::String(waveform->bits)+" bit | "+
                    juce::String(static_cast<double>(waveform->frames)/waveform->rate,2)+" s",
                    juce::dontSendNotification);
                repaint();
            }
        }catch(const std::exception& e){
            if(pendingPreview==wantedPreview)
                previewMeta.setText("Preview unavailable: "+juce::String::fromUTF8(e.what()),juce::dontSendNotification);
        }
    }
    if(!waveformWorker.valid()&&pendingPreview!=wantedPreview){
        pendingPreview=wantedPreview;
        if(pendingPreview.existsAsFile()){
            waveformCancel=std::make_shared<std::atomic<bool>>(false);
            const auto cancel=waveformCancel;
            const auto file=std::filesystem::path(pendingPreview.getFullPathName().toWideCharPointer());
            waveformWorker=std::async(std::launch::async,[file,cancel]{
                return mrs::library::build_wav_preview(file,cancel);
            });
            previewMeta.setText("Preparing waveform...",juce::dontSendNotification);
        }
    }
    const auto ready=readyImages.load();
    if(ready!=paintedImages){paintedImages=ready;results.repaint();}
}
juce::StringArray LibraryBrowserView::selectedWavFiles() const {
    juce::StringArray files;
    if(!view)return files;
    for(const auto id:selectedAssets()){
        const auto path=mrs::library::asset_path(*view,id);
        if(path)files.add(pathText(*path));
    }
    return files;
}
void LibraryBrowserView::prepareSelected(){
    if(waveformCancel)waveformCancel->store(true);
    waveform.reset();
    wantedPreview={};
    const auto ids=selectedAssets();
    if(ids.size()!=1||!view){
        previewName.setText(ids.empty()?"Select WAV to inspect":"Multiple WAV selected",juce::dontSendNotification);
        previewMeta.setText({},juce::dontSendNotification);repaint();return;
    }
    const auto path=mrs::library::asset_path(*view,ids.front());
    if(!path){
        previewName.setText("WAV unavailable / relink source",juce::dontSendNotification);
        previewMeta.setText({},juce::dontSendNotification);repaint();return;
    }
    wantedPreview=juce::File(pathText(*path));
    previewName.setText(wantedPreview.getFileName(),juce::dontSendNotification);
    previewMeta.setText("Loading waveform...",juce::dontSendNotification);
    repaint();
}
void LibraryBrowserView::startLibraryDrag(){
    const auto files=selectedWavFiles();
    if(files.isEmpty())return;
    if(auto* container=juce::DragAndDropContainer::findParentDragContainerFor(this))
        container->startDragging("mrs-library-wav", &results);
}
} // namespace ui
