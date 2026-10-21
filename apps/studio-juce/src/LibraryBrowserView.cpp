#include "LibraryBrowserView.h"
#include <MRConceptAssets.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>

namespace ui {
namespace {
juce::String uiText(const std::string& value){return juce::String::fromUTF8(value.c_str());}
juce::String pathText(const std::filesystem::path& value){return juce::String(value.wstring().c_str());}
juce::Colour muted(){return juce::Colour(0xff98a8bd);}

}
LibraryBrowserView::LibraryBrowserView(
    mrs::library::SampleLibrary& lib,
    std::function<bool(const std::filesystem::path&,double)> play,
    std::function<void()> stop,
    std::function<std::tuple<bool,float,bool>()> position)
    :library(lib),previewPlay(std::move(play)),previewStop(std::move(stop)),
      previewState(std::move(position)){
    for(auto* control:std::array<juce::Component*,25>{&foldersTab,&tagsTab,&collectionsTab,&favorites,&clearFilters,&listGrid,
        &samples,&back,&options,&instruments,&genres,&keyButton,&bpmButton,&typeButton,&activeFilters,&prevPage,&nextPage,
        &stopPreview,&sorts,&search,&counter,&breadcrumb,&previousSound,&playPause,&nextSound})addAndMakeVisible(control);
    addAndMakeVisible(results);results.setRowHeight(108);results.setMultipleSelectionEnabled(true);
    results.setComponentID("mr-library-browser-results");results.setColour(juce::ListBox::backgroundColourId,juce::Colour(surface));
    search.setTextToShowWhenEmpty("Search filenames, tags, folders...",muted());search.setInputRestrictions(128);
    foldersTab.onClick=[this]{chooseSection(Section::folders);};tagsTab.onClick=[this]{chooseSection(Section::tags);};
    collectionsTab.onClick=[this]{chooseSection(Section::collections);};
    instruments.onClick=[this]{filterMenu("Instrument",&instruments);};genres.onClick=[this]{filterMenu("Genre",&genres);};
    typeButton.onClick=[this]{filterMenu("Type",&typeButton);};
    keyButton.onClick=[this]{musicalMenu(false,&keyButton);};bpmButton.onClick=[this]{musicalMenu(true,&bpmButton);};
    options.onClick=[this]{optionsMenu();};
    activeFilters.onClick=[this]{
        juce::PopupMenu menu;std::vector<std::pair<bool,std::size_t>> entries;
        for(bool excluded:{false,true}){const auto& tags=excluded?query.excluded_tags:query.tags;
            for(std::size_t i=0;i<tags.size();++i){entries.emplace_back(excluded,i);menu.addItem(static_cast<int>(entries.size()),
                "Remove "+juce::String(excluded?"exclude ":"")+uiText(tags[i].group+": "+tags[i].label));}}
        if(!query.key.empty())menu.addItem(3000,"Remove key filter");
        if(query.bpm_min||query.bpm_max)menu.addItem(3001,"Remove BPM filter");
        juce::Component::SafePointer<LibraryBrowserView> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&activeFilters),[safe,entries](int choice){
            if(!safe||choice<=0)return;
            if(choice==3000||choice==3001){if(choice==3000)safe->query.key.clear();else{safe->query.bpm_min=0;safe->query.bpm_max=0;}safe->query.offset=0;safe->sendQuery();return;}
            if(choice>static_cast<int>(entries.size()))return;
            const auto [excluded,index]=entries[static_cast<std::size_t>(choice-1)];
            auto& tags=excluded?safe->query.excluded_tags:safe->query.tags;
            if(index<tags.size())tags.erase(tags.begin()+static_cast<std::ptrdiff_t>(index));
            safe->query.offset=0;safe->sendQuery();});
    };
    favorites.onClick=[this]{rootMode=false;query.favorites_only=!query.favorites_only;query.offset=0;sendQuery();};
    clearFilters.onClick=[this]{const auto source=query.source_id,collection=query.collection_id;const auto folder=query.folder;
        query={};query.source_id=source;query.collection_id=collection;query.folder=folder;
        search.setText({},false);query.offset=0;sendQuery();};
    samples.onClick=[this]{rootMode=false;query.offset=0;sendQuery();};
    back.onClick=[this]{
        if(section==Section::folders&&query.source_id>0&&!query.folder.empty()){
            const auto at=query.folder.find_last_of('/');query.folder=at==std::string::npos?"":query.folder.substr(0,at);query.offset=0;sendQuery();
        }else if(section==Section::tags&&rootMode&&!tagPath.empty()){tagPath.clear();rebuildCards();results.updateContent();results.repaint();}
        else chooseSection(section);
    };
    listGrid.onClick=[this]{gridView=!gridView;listGrid.setButtonText(gridView?"List":"Grid");
        results.setMultipleSelectionEnabled(!gridView);gridSelected=-1;results.deselectAllRows();
        results.setRowHeight(gridView?164:108);results.updateContent();results.repaint();};
    search.onTextChange=[this]{query.search=search.getText().toStdString();rootMode=false;query.offset=0;sendQuery();};
    for(const auto& name:std::array<const char*,7>{"Name","Relevant","BPM","Duration","Modified","Recently indexed","Random"})sorts.addItem(name,sorts.getNumItems()+1);
    sorts.setSelectedId(1,juce::dontSendNotification);
    sorts.onChange=[this]{static constexpr std::array<const char*,7> modes={"name","relevant","bpm","duration","updated","added","random"};
        const auto index=sorts.getSelectedId()-1;if(index>=0&&index<7)query.sort=modes[static_cast<std::size_t>(index)];query.shuffle_seed=static_cast<std::uint64_t>(juce::Time::getMillisecondCounter());query.offset=0;sendQuery();};
    prevPage.onClick=[this]{query.offset=query.offset>query.limit?query.offset-query.limit:0;sendQuery();};
    nextPage.onClick=[this]{if(view&&query.offset+query.limit<view->visible_assets){query.offset+=query.limit;sendQuery();}};
    prevPage.setTooltip("Previous results page");nextPage.setTooltip("Next results page");
    previousSound.setTooltip("Previous sample");nextSound.setTooltip("Next sample");playPause.setTooltip("Play / pause sample preview");
    for(auto* button:{&previousSound,&nextSound,&playPause,&stopPreview})button->getProperties().set("mrSmallToolbarIcon",true);
    previousSound.onClick=[this]{stepPreview(-1);};nextSound.onClick=[this]{stepPreview(1);};playPause.onClick=[this]{stepPreview(0);};
    stopPreview.onClick=[this]{if(previewStop)previewStop();activePreviewId=0;results.repaint();};
    for(const auto* name:{"track_drums_compact","track_bass_compact","track_piano_compact","track_guitar_compact","track_synth","wave","folder"}){
        int size{};const auto* bytes=MRConceptAssets::getNamedResource((juce::String(name)+"_svg").toRawUTF8(),size);
        if(bytes)instrumentArt[name]=juce::Drawable::createFromImageData(bytes,size);
    }
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
            if(mrs::library::artwork_image_supported(std::filesystem::path(work.second.getFullPathName().toWideCharPointer()))){
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
    waveWorker=std::jthread([this](std::stop_token stop){
        while(!stop.stop_requested()){
            WaveTask item;
            {
                std::unique_lock lock(waveMutex);
                waveWake.wait(lock,stop,[this]{return !waveQueue.empty();});
                if(stop.stop_requested())break;
                item=std::move(waveQueue.front());
                waveQueue.pop_front();
            }
            const auto cancel=item.cancel;
            std::stop_callback stopRequested(stop,[cancel]{cancel->store(true);});
            RowWave decoded;
            try{
                const auto path=mrs::library::asset_path(*item.catalogue,item.assetId);
                if(path)decoded.wav=mrs::library::build_wav_preview(*path,cancel);
                else decoded.error="File unavailable";
            }catch(const std::exception& e){decoded.error=e.what();}
            {
                std::lock_guard lock(waveMutex);
                if(cancel->load())continue;
                if(waveCache.size()>=96){
                    waveRequested.erase(waveCache.begin()->first);
                    waveCache.erase(waveCache.begin());
                }
                waveCache[item.key]=std::move(decoded);
            }
            wavesReady.fetch_add(1,std::memory_order_release);
        }
    });
    updateVisibility();startTimerHz(6);
}
LibraryBrowserView::~LibraryBrowserView(){
    stopTimer();imageChooser.reset();results.setModel(nullptr);
    if(previewStop)previewStop();
    waveWorker.request_stop();waveWake.notify_all();
    if(waveWorker.joinable())waveWorker.join();
    imageWorker.request_stop();imageWake.notify_all();
    if(imageWorker.joinable())imageWorker.join();
}
void LibraryBrowserView::chooseSection(Section next){
    section=next;rootMode=true;tagPath.clear();query={};search.setText({},false);sendQuery();
}
void LibraryBrowserView::invalidatePreview(){
    if(previewStop)previewStop();
    activePreviewId=0;
    dragPayload.clear();
    {
        std::lock_guard lock(waveMutex);
        waveCancel->store(true);
        waveCancel=std::make_shared<std::atomic<bool>>(false);
        waveQueue.clear();waveRequested.clear();waveCache.clear();
    }
    results.repaint();
}
void LibraryBrowserView::sendQuery(){
    query.limit=std::clamp(query.limit,std::uint64_t{1},std::uint64_t{300});query.search=query.search.substr(0,512);
    pendingQuery=true;counter.setText("Searching...",juce::dontSendNotification);results.setEnabled(false);dragPayload.clear();library.browse(query);
}
void LibraryBrowserView::setSnapshot(std::shared_ptr<const mrs::library::Snapshot> next){
    if(!next||revision==next->revision)return;
    if(pendingQuery&&next->query!=query)return;
    const bool resetScroll=pendingQuery;pendingQuery=false;results.setEnabled(true);
    view=std::move(next);revision=view->revision;
    if(activePreviewId&&!std::any_of(view->assets.begin(),view->assets.end(),
        [this](const auto& asset){return asset.id==activePreviewId&&asset.status=="ok"&&
            waveKey(asset)==activePreviewKey;})){
        if(previewStop)previewStop();
        activePreviewId=0;
    }
    query=view->query;
    refreshControls();
    gridSelected=-1;
    results.updateContent();if(resetScroll)results.scrollToEnsureRowIsOnscreen(0);results.repaint();
}
void LibraryBrowserView::refreshControls(){
    if(!view)return;
    counter.setText(juce::String(static_cast<juce::int64>(view->visible_assets))+" results",juce::dontSendNotification);
    prevPage.setEnabled(!rootMode&&query.offset>0);nextPage.setEnabled(!rootMode&&query.offset+query.limit<view->visible_assets);
    favorites.setToggleState(query.favorites_only,juce::dontSendNotification);
    activeFilters.setButtonText("Filters ("+juce::String(static_cast<int>(query.tags.size()+query.excluded_tags.size())+(!query.key.empty()?1:0)+(query.bpm_min||query.bpm_max?1:0))+")");
    keyButton.setButtonText(query.key.empty()?"Key":uiText(query.key));
    bpmButton.setButtonText(query.bpm_min||query.bpm_max?juce::String(query.bpm_min)+"-"+juce::String(query.bpm_max)+" BPM":"BPM");
    chips.clear();
    const bool active=!query.tags.empty()||!query.excluded_tags.empty();
    const auto addChip=[&](const std::string& group,const std::string& label,bool exclude){
        if(chips.size()>=3)return;auto chip=std::make_unique<juce::TextButton>(uiText((exclude?"-":"")+label)+(active?" x":""));
        chip->setTooltip(uiText(group+": "+label));chip->onClick=[this,group,label,exclude]{toggleTag(group,label,exclude);};addAndMakeVisible(*chip);chips.push_back(std::move(chip));
    };
    if(active){for(const auto& tag:query.tags)addChip(tag.group,tag.label,false);for(const auto& tag:query.excluded_tags)addChip(tag.group,tag.label,true);}
    else for(const auto& facet:view->facets)addChip(facet.group,facet.label,false);
    static constexpr std::array<const char*,7> modes={"name","relevant","bpm","duration","updated","added","random"};
    for(std::size_t i=0;i<modes.size();++i)if(query.sort==modes[i])sorts.setSelectedId(static_cast<int>(i)+1,juce::dontSendNotification);
    rebuildCards();
    if(rootMode)counter.setText(juce::String(static_cast<int>(cards.size()))+" items",juce::dontSendNotification);
    for(auto* button:{&previousSound,&playPause,&nextSound})button->setEnabled(!rootMode&&!view->assets.empty());
    updateVisibility();resized();
}
void LibraryBrowserView::updateVisibility(){
    foldersTab.setToggleState(section==Section::folders,juce::dontSendNotification);
    tagsTab.setToggleState(section==Section::tags,juce::dontSendNotification);
    collectionsTab.setToggleState(section==Section::collections,juce::dontSendNotification);
}
void LibraryBrowserView::resized(){
    auto area=getLocalBounds().reduced(6);
    auto line=area.removeFromTop(26);options.setBounds(line.removeFromRight(30));search.setBounds(line);area.removeFromTop(5);
    line=area.removeFromTop(25);const int tab=std::max(1,(line.getWidth()-48)/3);
    foldersTab.setBounds(line.removeFromLeft(tab));tagsTab.setBounds(line.removeFromLeft(tab));collectionsTab.setBounds(line.removeFromLeft(tab));favorites.setBounds(line);
    area.removeFromTop(5);line=area.removeFromTop(24);back.setBounds(line.removeFromLeft(44));samples.setBounds(line.removeFromRight(64));breadcrumb.setBounds(line);
    const bool narrow=getWidth()<440;
    area.removeFromTop(5);line=area.removeFromTop(25);const int filter=std::max(1,line.getWidth()/(narrow?3:5));
    instruments.setBounds(line.removeFromLeft(filter));genres.setBounds(line.removeFromLeft(filter));typeButton.setBounds(line.removeFromLeft(filter));
    if(narrow){area.removeFromTop(4);line=area.removeFromTop(25);keyButton.setBounds(line.removeFromLeft(line.getWidth()/2));bpmButton.setBounds(line);}
    else {keyButton.setBounds(line.removeFromLeft(filter));bpmButton.setBounds(line);}
    area.removeFromTop(4);line=area.removeFromTop(24);clearFilters.setBounds(line.removeFromRight(44));activeFilters.setBounds(line.removeFromLeft(70));
    for(auto& chip:chips){const int width=std::min(100,line.getWidth());chip->setVisible(width>=44);chip->setBounds(line.removeFromLeft(width));}
    area.removeFromTop(5);line=area.removeFromTop(24);listGrid.setBounds(line.removeFromRight(44));sorts.setBounds(line.removeFromRight(std::min(122,line.getWidth()/2)));counter.setBounds(line);
    area.removeFromTop(4);auto footer=area.removeFromBottom(27);prevPage.setBounds(footer.removeFromLeft(22));nextPage.setBounds(footer.removeFromLeft(22));previousSound.setBounds(footer.removeFromLeft(32));playPause.setBounds(footer.removeFromLeft(40));nextSound.setBounds(footer.removeFromLeft(32));stopPreview.setBounds(footer);
    area.removeFromBottom(4);results.setBounds(area);results.updateContent();
}
void LibraryBrowserView::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(surface));
    if(view&&getNumRows()==0){g.setColour(muted());g.drawFittedText(rootMode?"No folders or collections yet":"No matching WAV files",results.getBounds(),juce::Justification::centred,2);}
}
void LibraryBrowserView::visibilityChanged(){
    if(isShowing()){if(view)sendQuery();return;}
    if(!isShowing()){
        pendingQuery=true; // Edit Tags uses the same catalogue; retain this browser query until shown again.
        if(previewStop)previewStop();
        activePreviewId=0;
        results.repaint();
    }
}
std::vector<std::int64_t> LibraryBrowserView::selectedAssets() const {
    std::vector<std::int64_t> ids;
    if(!view)return ids;
    if(rootMode)return ids;
    if(gridView){
        if(gridSelected>=0&&gridSelected<static_cast<int>(view->assets.size()))
            ids.push_back(view->assets[static_cast<std::size_t>(gridSelected)].id);
        return ids;
    }
    const auto rows=results.getSelectedRows();
    for(int i=0;i<rows.size();++i)
        if(const int index=assetIndex(rows[i],0);index>=0&&index<static_cast<int>(view->assets.size()))
            ids.push_back(view->assets[static_cast<std::size_t>(index)].id);
    return ids;
}
int LibraryBrowserView::columns() const{return results.getWidth()>=380?2:1;}
int LibraryBrowserView::cardRows() const{return static_cast<int>((cards.size()+static_cast<std::size_t>(columns())-1)/static_cast<std::size_t>(columns()));}
int LibraryBrowserView::assetIndex(int row,int x) const {
    if(rootMode||row<cardRows())return -1;
    return (row-cardRows())*(gridView?columns():1)+(gridView&&columns()==2&&x>=results.getWidth()/2?1:0);
}
int LibraryBrowserView::getNumRows(){
    if(!view)return 0;const auto cols=gridView?columns():1;
    return cardRows()+(rootMode?0:static_cast<int>((view->assets.size()+static_cast<std::size_t>(cols)-1)/static_cast<std::size_t>(cols)));
}
juce::Image LibraryBrowserView::cachedImage(const juce::File& file){
    const auto name=file.getFullPathName().toStdString();
    std::lock_guard lock(imageMutex);
    if(const auto cached=imageCache.find(name);cached!=imageCache.end())return cached->second;
    if(pendingImages.size()<64&&requestedImages.insert(name).second){pendingImages.emplace_back(name,file);imageWake.notify_one();}
    return {};
}
juce::Image LibraryBrowserView::thumbnail(const mrs::library::AudioAsset& asset){
    if(!view)return {};
    auto parent=asset.relative_path;const auto slash=parent.find_last_of('/');parent=slash==std::string::npos?"":parent.substr(0,slash);
    auto cover=asset.artwork_path;
    for(;;){const auto managed=coverFor({"folder","",parent,"","",asset.source_id,0});
        if(!managed.empty()){cover=managed;break;}if(parent.empty())break;
        const auto at=parent.find_last_of('/');parent=at==std::string::npos?"":parent.substr(0,at);}
    if(cover.starts_with("managed:"))return cachedImage(juce::File(pathText(view->artwork_root)).getChildFile(uiText(cover.substr(8))));
    const auto source=std::find_if(view->sources.begin(),view->sources.end(),[&](const auto& x){return x.id==asset.source_id;});
    return source==view->sources.end()||cover.empty()?juce::Image{}:cachedImage(juce::File(pathText(source->root)).getChildFile(uiText(cover)));
}
void LibraryBrowserView::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected){
    if(!view)return;
    if(row<cardRows()){
        const int cols=columns(),cell=width/cols;
        for(int col=0;col<cols;++col){const auto index=row*cols+col;if(index>=static_cast<int>(cards.size()))break;
            const auto& card=cards[static_cast<std::size_t>(index)];const int x=col*cell;
            g.setColour(juce::Colour(0xff222b38));g.fillRoundedRectangle(static_cast<float>(x+2),2.f,static_cast<float>(cell-5),static_cast<float>(height-4),4.f);
            mrs::library::AudioAsset art;art.source_id=card.owner;art.relative_path=card.key+"/item.wav";art.artwork_path=card.cover;
            auto cover=coverFor(card);juce::Image bitmap;
            if(cover.starts_with("managed:"))bitmap=cachedImage(juce::File(pathText(view->artwork_root)).getChildFile(uiText(cover.substr(8))));
            else if(card.kind=="folder")bitmap=thumbnail(art);
            if(bitmap.isValid())g.drawImageWithin(bitmap,x+6,8,50,50,juce::RectanglePlacement::centred,false);
            else if(const auto icon=instrumentArt.find("folder");icon!=instrumentArt.end()&&icon->second)icon->second->drawWithin(g,{static_cast<float>(x+12),14,36,36},juce::RectanglePlacement::centred,1.f);
            g.setColour(juce::Colours::whitesmoke);g.setFont(Theme::detailFont());g.drawFittedText(uiText(card.label),x+62,12,std::max(1,cell-84),38,juce::Justification::centredLeft,2);
            g.setColour(muted());g.setFont(juce::Font(juce::FontOptions(10.f)));
            const auto caption=card.kind=="group"?juce::String("Browse tags"):card.kind=="folder"&&!card.key.empty()?juce::String("Open folder"):juce::String(static_cast<juce::int64>(card.count))+" WAV";
            g.drawFittedText(caption,x+62,55,std::max(1,cell-68),16,juce::Justification::centredLeft,1);
            g.drawText("...",x+40,6,20,20,juce::Justification::centred);
        }return;
    }
    if(rootMode)return;
    row-=cardRows();
    const auto cols=gridView?columns():1;
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
        const auto slash=item.relative_path.find_last_of('/');
        const auto file=uiText(item.relative_path.substr(slash==std::string::npos?0:slash+1));
        g.drawFittedText(file,left,4,std::max(1,right-24),18,juce::Justification::centredLeft,1);
        g.setColour(muted());g.setFont(juce::Font(juce::FontOptions(11.f)));
        const int bpm=item.manual_bpm==0?item.detected_bpm:item.manual_bpm;
        const auto key=item.manual_key.empty()?item.detected_key:item.manual_key;
        const auto duration=item.sample_rate>0?
            static_cast<double>(item.frames)/static_cast<double>(item.sample_rate):0.;
        const auto waveform=rowWave(item);
        juce::String details=(item.favorite?juce::String::fromUTF8("★ "):juce::String{})+
            (bpm>0?juce::String(bpm)+" BPM  ":"")+
            ((key.empty()||key=="-")?juce::String{}:uiText(key)+"  ")+
            juce::String(duration,1)+" s";
        if(item.status!="ok")details+=" | "+uiText(item.status);
        else if(!waveform)details+=" | Loading...";
        else if(!waveform->error.empty())details+=" | "+uiText(waveform->error);
        else details+=" | "+juce::String(waveform->wav.rate)+" Hz / "+
            juce::String(waveform->wav.bits)+" bit / "+
            (waveform->wav.channels==1?juce::String("mono"):
                waveform->wav.channels==2?juce::String("stereo"):
                juce::String(waveform->wav.channels)+" ch");
        g.drawFittedText(details,left,gridView?78:64,right,14,juce::Justification::centredLeft,1);
        {
            juce::String tags;
            for(const auto& t:item.tags)if(t.status!="rejected"){
                if(tags.isNotEmpty())tags+=", ";
                tags+=uiText(t.label);
            }
            g.drawFittedText(tags,left,24,right-22,17,juce::Justification::centredLeft,1);
            const auto path=slash==std::string::npos?juce::String{}:uiText(item.relative_path.substr(0,slash));
            g.drawFittedText(path,left,44,right,16,juce::Justification::centredLeft,1);
            const char* glyph="wave";for(const auto& tag:item.tags)if(tag.group=="Instrument"&&tag.status!="rejected"){
                const auto label=uiText(tag.label).toLowerCase();if(label.contains("drum")||label.contains("percussion")||label.contains("kick")||label.contains("snare")||label.contains("hat")||label.contains("ride")||label.contains("tom"))glyph="track_drums_compact";
                else if(label.contains("bass"))glyph="track_bass_compact";else if(label.contains("guitar"))glyph="track_guitar_compact";
                else if(label.contains("piano")||label.contains("key"))glyph="track_piano_compact";else if(label.contains("synth"))glyph="track_synth";break;}
            if(const auto icon=instrumentArt.find(glyph);icon!=instrumentArt.end()&&icon->second)icon->second->drawWithin(g,{static_cast<float>(x+cellW-28),23,18,18},juce::RectanglePlacement::centred,1.f);
            g.drawText("...",x+cellW-26,2,22,20,juce::Justification::centred);
        }
        const int waveTop=gridView?height-35:height-27;
        const int waveHeight=gridView?26:19;
        const int waveLeft=left+23;
        const int waveWidth=std::max(1,right-31);
        const auto mid=static_cast<float>(waveTop)+waveHeight*.5f;
        const auto state=previewState?previewState():std::tuple{false,0.f,false};
        const bool nowPlaying=item.id==activePreviewId&&std::get<0>(state);
        if(item.id==activePreviewId)paintedPlaying=std::get<0>(state);
        g.setColour(nowPlaying?juce::Colour(0xffc1e8f7):juce::Colour(0xff80b9dc));
        g.drawFittedText(nowPlaying?"II":">",left,waveTop,18,waveHeight,
                         juce::Justification::centred,1);
        g.setColour(juce::Colour(0xff314256));
        g.drawHorizontalLine(static_cast<int>(mid),static_cast<float>(waveLeft),
                             static_cast<float>(waveLeft+waveWidth));
        if(waveform&&waveform->error.empty()){
            g.setColour(juce::Colour(selectedCell?0xffb1ddf3:0xff80b9dc));
            for(int pixel=0;pixel<waveWidth;++pixel){
                const auto bin=std::min(255,pixel*256/waveWidth);
                const auto peak=std::clamp(waveform->wav.peaks[static_cast<std::size_t>(bin)],0.f,1.f);
                const auto size=std::max(0.5f,peak*static_cast<float>(waveHeight-3)*.5f);
                g.drawVerticalLine(waveLeft+pixel,mid-size,mid+size);
            }
        }
        if(item.id==activePreviewId){
            const int playheadX=waveLeft+static_cast<int>(
                std::clamp(std::get<1>(state),0.f,1.f)*static_cast<float>(waveWidth));
            g.setColour(juce::Colour(0xffe7f3fb));
            g.drawVerticalLine(playheadX,static_cast<float>(waveTop),
                               static_cast<float>(waveTop+waveHeight));
        }
    }
}
void LibraryBrowserView::selectedRowsChanged(int){
    if(!rootMode&&cardRows()>0){auto selected=results.getSelectedRows();auto cleaned=selected;
        cleaned.removeRange({0,cardRows()});if(cleaned!=selected)results.setSelectedRows(cleaned);}
    results.repaint();
}
juce::String LibraryBrowserView::getTooltipForRow(int row){
    if(!view)return {};const auto index=assetIndex(row,results.getMouseXYRelative().x);
    if(index<0||index>=static_cast<int>(view->assets.size()))return {};
    const auto& asset=view->assets[static_cast<std::size_t>(index)];juce::String text=uiText(asset.relative_path);
    for(const auto& tag:asset.tags)if(tag.status!="rejected")text+=" | "+uiText(tag.label);
    return text;
}
void LibraryBrowserView::listBoxItemClicked(int row,const juce::MouseEvent& event){
    if(!view)return;
    if(row<cardRows()){
        const int cell=results.getWidth()/columns(),col=std::clamp(event.x/std::max(1,cell),0,columns()-1),index=row*columns()+col;
        if(index>=static_cast<int>(cards.size()))return;const auto card=cards[static_cast<std::size_t>(index)];
        if(event.mods.isPopupMenu()||(event.x%cell>=38&&event.x%cell<=62&&event.y<30))coverMenu(card,&results);else openCard(card);return;
    }
    const int index=assetIndex(row,event.x);if(index<0||index>=static_cast<int>(view->assets.size()))return;
    if(gridView)gridSelected=index;
    const int cols=gridView?columns():1,width=results.getWidth()/cols,col=cols==2&&event.x>=width?1:0;
    if(event.mods.isPopupMenu()||(event.x-col*width>width-30&&event.y<24)){rowMenu(static_cast<std::size_t>(index));return;}
    const int top=gridView?results.getRowHeight()-35:results.getRowHeight()-27;
    const int left=col*width+(gridView?74:32)+12,start=left+23,span=std::max(1,width-(gridView?74:32)-51);
    if(event.y>=top){if(event.x>=left&&event.x<start)audition(static_cast<std::size_t>(index),0.,true);
        else if(event.x>=start&&event.x<=start+span)audition(static_cast<std::size_t>(index),std::clamp(static_cast<double>(event.x-start)/span,0.,.999999),false);}
    results.repaint();
}
void LibraryBrowserView::audition(std::size_t index,double position,bool toggle){
    if(!view||index>=view->assets.size())return;
    const auto& item=view->assets[index];
    const auto current=previewState?previewState():std::tuple{false,0.f,false};
    if(toggle&&item.id==activePreviewId&&std::get<0>(current)){
        // Retain position so the next Play resumes from this point.
        if(previewStop)previewStop();
        return;
    }
    if(toggle&&item.id==activePreviewId&&!std::get<0>(current)&&std::get<1>(current)>.001f&&std::get<1>(current)<.999f)
        position=std::get<1>(current);
    if(!previewPlay)return;
    const auto wav=mrs::library::asset_path(*view,item.id);
    if(!wav){
        counter.setText("WAV unavailable: rescan or relink its source folder",juce::dontSendNotification);
        return;
    }
    try{
        if(previewPlay(*wav,position)){
            activePreviewId=item.id;activePreviewKey=waveKey(item);
        }
        else counter.setText("Preview unavailable: connect audio or stop recording",
                             juce::dontSendNotification);
    }catch(const std::exception& e){
        counter.setText("Preview failed: "+uiText(e.what()),juce::dontSendNotification);
    }
}
void LibraryBrowserView::stepPreview(int delta){
    if(!view||rootMode||view->assets.empty())return;
    auto at=std::find_if(view->assets.begin(),view->assets.end(),[this](const auto& asset){return asset.id==activePreviewId;});
    const auto selected=selectedAssets();
    if(!selected.empty()&&(delta==0||at==view->assets.end()))at=std::find_if(view->assets.begin(),view->assets.end(),[&](const auto& asset){return asset.id==selected.front();});
    std::ptrdiff_t index=at==view->assets.end()?0:at-view->assets.begin();
    if(at!=view->assets.end())index+=delta;
    index=std::clamp(index,std::ptrdiff_t{0},static_cast<std::ptrdiff_t>(view->assets.size()-1));
    audition(static_cast<std::size_t>(index),0.,delta==0);
}
void LibraryBrowserView::timerCallback(){
    const auto ready=readyImages.load(std::memory_order_acquire);
    const auto wave=wavesReady.load(std::memory_order_acquire);
    const auto playback=previewState?previewState():std::tuple{false,0.f,false};
    const auto playing=std::get<0>(playback);
    playPause.setButtonText(playing?"Pause":"Play");
    if(activePreviewId&&std::get<2>(playback))
        counter.setText("Preview failed: WAV missing, unreadable or unsupported",juce::dontSendNotification);
    if(ready!=paintedImages||wave!=wavesPainted||playing||playing!=paintedPlaying){
        paintedImages=ready;wavesPainted=wave;paintedPlaying=playing;results.repaint();
    }
}
std::string LibraryBrowserView::waveKey(const mrs::library::AudioAsset& asset) const {
    if(!view)return {};
    const auto source=std::find_if(view->sources.begin(),view->sources.end(),
        [&](const auto& entry){return entry.id==asset.source_id&&entry.enabled;});
    if(source==view->sources.end())return {};
    const auto root=source->root.generic_u8string();
    return std::string(root.begin(),root.end())+"|"+std::to_string(asset.source_id)+"|"+
        std::to_string(asset.id)+"|"+std::to_string(asset.modified)+"|"+
        std::to_string(asset.frames)+"|"+std::to_string(asset.sample_rate)+"|"+asset.relative_path;
}
std::optional<LibraryBrowserView::RowWave> LibraryBrowserView::rowWave(
    const mrs::library::AudioAsset& asset){
    if(!view||asset.status!="ok")return {};
    const auto cacheKey=waveKey(asset);
    if(cacheKey.empty())return {};
    std::lock_guard lock(waveMutex);
    if(const auto found=waveCache.find(cacheKey);found!=waveCache.end())return found->second;
    if(waveQueue.size()<20&&waveRequested.insert(cacheKey).second){
        waveQueue.push_back({cacheKey,view,asset.id,waveCancel});
        waveWake.notify_one();
    }
    return {};
}
juce::StringArray LibraryBrowserView::selectedWavFiles() const {
    juce::StringArray files;
    if(!view)return files;
    for(const auto& [id,original]:dragPayload){
        const auto path=mrs::library::asset_path(*view,id);
        // Async browse/rescan/relink cannot silently substitute another file
        // for the item the user originally picked up.
        if(!path||*path!=original)return {};
        files.add(pathText(*path));
    }
    return files;
}
juce::var LibraryBrowserView::getDragSourceDescription(const juce::SparseSet<int>& rows){
    // ListBox row components own the drag gesture and preserve multi-selection.
    dragPayload.clear();
    if(!view)return {};
    const int cols=gridView?columns():1;
    const auto down=results.getLocalPoint(nullptr,juce::Desktop::getInstance()
        .getMainMouseSource().getLastMouseDownPosition().roundToInt());
    const auto downRow=results.getRowContainingPosition(down.x,down.y);
    if(downRow>=0&&downRow<cardRows())return {};
    const int col=cols==2&&down.x>=results.getWidth()/2?1:0;
    for(int i=0;i<rows.size();++i){
        const int index=assetIndex(rows[i],col*(results.getWidth()/cols));
        if(index<0||index>=static_cast<int>(view->assets.size()))return {};
        const auto& item=view->assets[static_cast<std::size_t>(index)];
        const auto file=mrs::library::asset_path(*view,item.id);
        if(!file){
            dragPayload.clear();
            counter.setText("Drag unavailable: rescan or relink selected WAVs",juce::dontSendNotification);
            return {};
        }
        dragPayload.emplace_back(item.id,*file);
        if(gridView){gridSelected=index;break;}
    }
    results.repaint();
    return dragPayload.empty()?juce::var{}:juce::var("mrs-library-wav");
}
void LibraryBrowserView::rebuildCards(){
    cards.clear();if(!view)return;
    juce::String location=section==Section::folders?"Libraries":section==Section::tags?uiText(tagPath.empty()?"Tags":tagPath):juce::String("Collections");
    if(section==Section::folders&&query.source_id>0){
        const auto source=std::find_if(view->sources.begin(),view->sources.end(),[this](const auto& entry){return entry.id==query.source_id;});
        if(source!=view->sources.end())location=pathText(source->root.filename());
        if(!query.folder.empty())location+=" / "+uiText(query.folder);
    }
    if(section==Section::collections&&query.collection_id>0){
        const auto collection=std::find_if(view->collections.begin(),view->collections.end(),[this](const auto& entry){return entry.id==query.collection_id;});
        if(collection!=view->collections.end())location=uiText(collection->name);
    }
    breadcrumb.setText(location,juce::dontSendNotification);breadcrumb.setTooltip(location);
    if(section==Section::folders&&(rootMode||query.source_id>0)){
        if(query.source_id==0){for(const auto& source:view->sources)if(source.enabled){
            std::string cover;for(const auto& folder:view->folders)if(folder.source_id==source.id&&folder.relative_path.empty())cover=folder.artwork_path;
            cards.push_back({"folder",pathText(source.root.filename()).toStdString(),"","",cover,source.id,source.available});}}
        else for(const auto& folder:view->folders){
            if(folder.source_id!=query.source_id||folder.relative_path.empty())continue;
            const auto slash=folder.relative_path.find_last_of('/');const auto parent=slash==std::string::npos?"":folder.relative_path.substr(0,slash);
            if(parent==query.folder)cards.push_back({"folder",folder.relative_path.substr(slash==std::string::npos?0:slash+1),folder.relative_path,"",folder.artwork_path,folder.source_id,0});
        }
    }else if(section==Section::collections&&rootMode){for(const auto& collection:view->collections)
        cards.push_back({"collection",collection.name,"","","",collection.id,collection.count});
    }else if(section==Section::tags&&rootMode){
        std::set<std::string> groups;
        for(const auto& facet:view->all_facets){
            if(tagPath.empty()){if(groups.insert(facet.group).second)cards.push_back({"group",facet.group,facet.group,"","",0,0});}
            else if(facet.group==tagPath)cards.push_back({"tag",facet.label,facet.group+"|"+facet.label,facet.group,"",0,facet.count});
        }
    }
    results.deselectAllRows();
}
std::string LibraryBrowserView::coverFor(const Card& card) const {
    if(view)for(const auto& cover:view->covers)if(cover.kind==card.kind&&cover.owner==card.owner&&cover.key==card.key&&!cover.image.empty())return "managed:"+cover.image;
    return card.cover;
}
void LibraryBrowserView::openCard(const Card& card){
    query.offset=0;
    if(card.kind=="folder"){query.source_id=card.owner;query.folder=card.key;rootMode=false;}
    else if(card.kind=="collection"){query.collection_id=card.owner;rootMode=false;}
    else if(card.kind=="group"){tagPath=card.key;rootMode=true;rebuildCards();results.updateContent();results.repaint();return;}
    else if(card.kind=="tag"){toggleTag(card.group,card.label);return;}
    sendQuery();
}
void LibraryBrowserView::toggleTag(std::string group,std::string label,bool exclude){
    auto& entries=exclude?query.excluded_tags:query.tags;auto& opposite=exclude?query.tags:query.excluded_tags;
    const auto matches=[&](const auto& x){return x.group==group&&x.label==label;};
    const auto found=std::find_if(entries.begin(),entries.end(),matches);
    if(found==entries.end())entries.push_back({group,label});else entries.erase(found);
    std::erase_if(opposite,matches);rootMode=false;query.offset=0;sendQuery();
}
void LibraryBrowserView::filterMenu(const std::string& group,juce::Component* anchor){
    if(!view)return;juce::PopupMenu menu,excluded;
    std::vector<std::string> labels;
    // Retain selected values when another condition removes their facet count.
    for(const auto& facet:view->all_facets)if(facet.group==group)labels.push_back(facet.label);
    for(const auto& filters:{query.tags,query.excluded_tags})for(const auto& tag:filters)
        if(tag.group==group&&std::find(labels.begin(),labels.end(),tag.label)==labels.end())labels.push_back(tag.label);
    const auto selected=[&](const auto& tags,const std::string& label){return std::any_of(tags.begin(),tags.end(),[&](const auto& x){return x.group==group&&x.label==label;});};
    for(std::size_t i=0;i<labels.size()&&i<1000;++i){
        const auto& label=labels[i];std::uint64_t count{};for(const auto& facet:view->facets)if(facet.group==group&&facet.label==label)count=facet.count;
        menu.addItem(static_cast<int>(i)+1,uiText(label)+" ("+juce::String(static_cast<juce::int64>(count))+")",true,selected(query.tags,label));
        excluded.addItem(static_cast<int>(i)+1001,uiText(label),true,selected(query.excluded_tags,label));
    }
    menu.addSubMenu("Exclude",excluded);menu.addSeparator();menu.addItem(3000,"Clear "+uiText(group));
    juce::Component::SafePointer<LibraryBrowserView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(anchor),[safe,group,labels](int choice){
        if(!safe||choice==0)return;
        if(choice==3000){std::erase_if(safe->query.tags,[&](const auto& x){return x.group==group;});std::erase_if(safe->query.excluded_tags,[&](const auto& x){return x.group==group;});safe->query.offset=0;safe->sendQuery();}
        else {const bool exclude=choice>=1001;const auto index=static_cast<std::size_t>(choice-(exclude?1001:1));if(index<labels.size())safe->toggleTag(group,labels[index],exclude);}
    });
}
void LibraryBrowserView::prompt(juce::String title,juce::String initial,std::function<void(juce::String)> apply){
    auto* dialog=new juce::AlertWindow(title,{},juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("value",initial);dialog->addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<LibraryBrowserView> safe(this);
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog,apply=std::move(apply)](int result){if(safe&&result)apply(dialog->getTextEditorContents("value").trim());}),true);
}
void LibraryBrowserView::musicalMenu(bool bpm,juce::Component* anchor){
    juce::PopupMenu menu;juce::Component::SafePointer<LibraryBrowserView> safe(this);
    if(bpm){menu.addItem(1,"Exact BPM...");menu.addItem(2,"BPM range...");menu.addItem(3,"Clear BPM");
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(anchor),[safe](int choice){
            if(!safe||!choice)return;if(choice==3){safe->query.bpm_min=0;safe->query.bpm_max=0;safe->query.offset=0;safe->sendQuery();return;}
            safe->prompt(choice==1?"Exact BPM":"BPM range: min-max",{},[safe,choice](juce::String value){
                if(!safe)return;const auto parts=juce::StringArray::fromTokens(value,"-","");
                if(parts.size()!=(choice==1?1:2))return;for(const auto& part:parts)if(!part.trim().containsOnly("0123456789"))return;
                const auto low=parts[0].getIntValue(),high=choice==1?low:parts[1].getIntValue();
                if(low<1||high<low||high>999)return;safe->query.bpm_min=low;safe->query.bpm_max=high;safe->rootMode=false;safe->query.offset=0;safe->sendQuery();});
        });
    }else{
        std::vector<std::string> keys;
        for(const auto* note:{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"})for(const bool minor:{false,true})keys.push_back(std::string(note)+(minor?"m":" major"));
        for(std::size_t i=0;i<keys.size();++i)menu.addItem(static_cast<int>(i)+1,uiText(keys[i]),true,query.key==keys[i]);
        menu.addItem(100,"Unknown",true,query.key=="?");menu.addItem(101,"Any key");
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(anchor),[safe,keys](int choice){
            if(!safe||!choice)return;safe->query.key=choice==100?"?":choice==101?"":keys[static_cast<std::size_t>(choice-1)];safe->rootMode=false;safe->query.offset=0;safe->sendQuery();});
    }
}
void LibraryBrowserView::coverMenu(Card card,juce::Component* anchor){
    juce::PopupMenu menu;menu.addItem(1,"Choose cover...");menu.addItem(2,"Reset cover");
    juce::Component::SafePointer<LibraryBrowserView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(anchor),[safe,card](int choice){if(safe&&choice)safe->chooseArtwork(card,choice==2);});
}
void LibraryBrowserView::chooseArtwork(Card card,bool clear){
    if(clear){library.set_cover(card.kind,card.owner,card.key,{});return;}
    imageChooser=std::make_unique<juce::FileChooser>("Choose JPG or PNG cover",juce::File{},"*.jpg;*.jpeg;*.png");
    juce::Component::SafePointer<LibraryBrowserView> safe(this);
    imageChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,card](const juce::FileChooser& chooser){
        if(safe&&chooser.getResult().existsAsFile())safe->library.set_cover(card.kind,card.owner,card.key,std::filesystem::path(chooser.getResult().getFullPathName().toWideCharPointer()));
    });
}
void LibraryBrowserView::rowMenu(std::size_t index){
    if(!view||index>=view->assets.size())return;const auto item=view->assets[index];
    auto ids=selectedAssets();if(std::find(ids.begin(),ids.end(),item.id)==ids.end())ids={item.id};
    const auto path=mrs::library::asset_path(*view,item.id);
    juce::PopupMenu menu,add;menu.addItem(1,item.favorite?"Remove from Likes":"Add to Likes");
    for(std::size_t i=0;i<view->collections.size();++i)add.addItem(100+static_cast<int>(i),uiText(view->collections[i].name));
    menu.addSubMenu("Add to collection",add);menu.addItem(2,"Create collection...");
    if(query.collection_id)menu.addItem(3,"Remove from this collection");
    juce::PopupMenu tagMenu;
    const std::array<std::string,7> tagGroups={"Instrument","Genre","Type","Technique","Rhythm","Character","Custom"};
    for(std::size_t i=0;i<tagGroups.size();++i)tagMenu.addItem(400+static_cast<int>(i),uiText(tagGroups[i]));
    menu.addSubMenu("Add tag",tagMenu);menu.addItem(5,"Show WAV in Explorer",path.has_value());menu.addItem(6,"Show containing folder",path.has_value());
    juce::Component::SafePointer<LibraryBrowserView> safe(this);const auto collections=view->collections;const auto collection=query.collection_id;
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&results),[safe,item,ids,path,collections,collection,tagGroups](int choice){
        if(!safe||!choice)return;
        if(choice==1)safe->library.set_favorites(ids,!item.favorite);
        else if(choice==2)safe->prompt("New collection",{},[safe](juce::String value){if(safe&&value.isNotEmpty())safe->library.create_collection(value.toStdString());});
        else if(choice==3)safe->library.set_collection_items(collection,ids,false);
        else if(choice>=400&&choice<407){const auto group=tagGroups[static_cast<std::size_t>(choice-400)];
            safe->prompt("New "+uiText(group)+" tag",{},[safe,ids,group](juce::String label){if(safe&&label.isNotEmpty())safe->library.add_tag(ids,group,label.toStdString());});}
        else if(choice==5&&path)juce::File(pathText(*path)).revealToUser();
        else if(choice==6&&path)juce::File(pathText(path->parent_path())).revealToUser();
        else if(choice>=100&&choice<100+static_cast<int>(collections.size()))safe->library.set_collection_items(collections[static_cast<std::size_t>(choice-100)].id,ids,true);
    });
}
void LibraryBrowserView::optionsMenu(){
    juce::PopupMenu menu;menu.addItem(1,"Create collection...");menu.addItem(2,"Rescan libraries");
    if(query.collection_id){menu.addItem(3,"Delete this collection");menu.addItem(4,"Collection cover...");}
    if(query.source_id&&section==Section::folders)menu.addItem(5,"Folder cover...");
    juce::Component::SafePointer<LibraryBrowserView> safe(this);const auto collection=query.collection_id;
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&options),[safe,collection](int choice){
        if(!safe)return;
        if(choice==1)safe->prompt("New collection",{},[safe](juce::String name){if(safe&&name.isNotEmpty())safe->library.create_collection(name.toStdString());});
        else if(choice==2){safe->invalidatePreview();safe->library.rescan();}
        else if(choice==3){
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,"Remove collection?","Original WAV files will stay in place.","Remove","Cancel",safe.getComponent(),
                juce::ModalCallbackFunction::create([safe,collection](int answer){if(safe&&answer){safe->library.delete_collection(collection);safe->chooseSection(Section::collections);}}));}
        else if(choice==4)safe->coverMenu({"collection","","","","",collection,0},&safe->options);
        else if(choice==5)safe->coverMenu({"folder","",safe->query.folder,"","",safe->query.source_id,0},&safe->options);
    });
}
} // namespace ui
