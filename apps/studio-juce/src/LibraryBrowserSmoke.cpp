#include "LibraryBrowserView.h"
#include <mrs/recording.hpp>
#include <chrono>
#include <thread>
namespace ui {
void wavDrop263Smoke();
void browserFocus263Smoke(juce::Component&,juce::Component&);
void libraryBrowserSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    const auto root=std::filesystem::temp_directory_path()/("mrs-library261-"+mrs::new_id().value);
    std::filesystem::create_directories(root/"samples"/"Jazz");
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}}cleanup{root};
    for(const auto* name:{"Brush_120BPM_Loop.wav","Kick_oneshot.wav"}){
        mrs::audio::Recorder recorder(root/"samples"/"Jazz"/name,48000,0,{0});std::array<float,512> pcm{};pcm.fill(.25f);recorder.capture(pcm.data(),1,512,0);recorder.finish();
    }
    mrs::library::SampleLibrary library(root/"catalog.sqlite");
    Theme theme;
    bool playing{};int previews{};
    LibraryBrowserView browser(library,[&](const auto&,double){++previews;playing=true;return true;},[&]{playing=false;},[&]{return std::tuple{playing,.25f,false};});
    browser.setLookAndFeel(&theme);
    const auto pump=[&](const auto& ready){
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(std::chrono::steady_clock::now()<until){auto snap=library.snapshot();browser.setSnapshot(snap);if(ready(*snap)&&!browser.pendingQuery)return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        throw std::runtime_error("Library UI worker timed out");
    };
    library.add_source(root/"samples");pump([](const auto& snap){return snap.phase==mrs::library::Phase::idle&&snap.visible_assets==2;});
    browser.setSize(260,640);check(browser.rootMode&&browser.cards.size()==1,"Folders root should show source cover cards");
    check(browser.assetIndex(0,0)<0,"Folder card must not masquerade as a WAV drag");
    const auto snapshot=[&](const juce::String& name){for(float scale:{1.f,1.25f,1.5f,2.f}){
        const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("juce-library261-"+name+"-"+juce::String(static_cast<int>(scale*100))+".png");
        auto output=file.createOutputStream();check(output!=nullptr,"Create Library preview");
        check(juce::PNGImageFormat().writeImageToStream(browser.createComponentSnapshot(browser.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*output),"Write Library preview");
    }};
    snapshot("folders-260");
    browser.openCard(browser.cards.front());pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.cards.size()==1&&browser.cards.front().key=="Jazz","Folder navigation must list direct children");
    browser.openCard(browser.cards.front());pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.cards.empty()&&browser.assetIndex(0,0)==0,"Folder navigation must preserve WAV indices");
    check(browser.results.getRowHeight()==60,"Sample rows stay compact without a separate waveform lane");
    check(browser.rowLayout(260,60).wave.isEmpty(),"Narrow results hide waveforms");
    for(int width:{380,440,500,559}){
        const auto compact=browser.rowLayout(width,60);
        check(compact.wave.getWidth()>=64&&compact.wave.getRight()<=width-30&&compact.title.getWidth()>=std::clamp(browser.pageNameWidth,1,150),"Waveforms remain visible when medium-width rows still have room");
    }
    const auto wide=browser.rowLayout(960,60);
    check(wide.wave.getWidth()<=160&&wide.wave.getX()>wide.title.getRight()&&wide.wave.getBottom()<=60,"Wide waveform stays short, beside filename and inside row");
    check(wide.instrument.getCentreY()==wide.wave.getCentreY()&&wide.instrument.getRight()<wide.wave.getX()&&wide.title.getRight()<wide.instrument.getX(),"Instrument and waveform share a horizontal line after the filename column");
    const auto bounded=browser.rowLayout(560,60);
    check(bounded.wave.getRight()<=560-30&&bounded.title.getWidth()>0&&bounded.title.getRight()<bounded.wave.getX(),"Shared page columns keep separate waveform/title hit regions");
    const auto font=Theme::detailFont().withHeight(14.f).boldened();int widest{};
    for(const auto& item:browser.view->assets){const auto at=item.relative_path.find_last_of('/');widest=std::max(widest,juce::GlyphArrangement::getStringWidthInt(font,juce::String(item.relative_path.substr(at==std::string::npos?0:at+1)))+2);}
    check(browser.pageNameWidth==widest&&wide.title.getWidth()==widest,"Wave and instrument columns align to longest filename on current page");
    browser.invalidatePreview();
    const auto firstWave=browser.waveKey(browser.view->assets.front());
    {
        std::lock_guard lock(browser.waveMutex);
        for(std::size_t i=0;i<browser.waveCapacity-1;++i){LibraryBrowserView::RowWave stale;stale.lastUse=++browser.waveUse;browser.waveCache.emplace("zz-stale-"+std::to_string(i),std::move(stale));}
        LibraryBrowserView::RowWave visible;visible.wav.peaks.fill(.25f);visible.lastUse=++browser.waveUse;browser.waveCache.emplace(firstWave,std::move(visible));
    }
    check(browser.rowWave(browser.view->assets.front()).has_value(),"Painted waveform remains cached");
    const auto untilWave=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!browser.rowWave(browser.view->assets[1])&&std::chrono::steady_clock::now()<untilWave)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    {
        std::lock_guard lock(browser.waveMutex);
        check(browser.waveCache.contains(browser.waveKey(browser.view->assets[1]))&&browser.waveCache.contains(firstWave)&&!browser.waveCache.contains("zz-stale-0"),"Worker completion evicts the oldest unused wave, never the visible lexically first key");
        check(browser.waveCache.size()==browser.waveCapacity&&browser.waveCapacity>=300,"Bounded waveform cache holds a complete maximum results page");
    }
    const auto point=juce::Point<int>{123,234};
    check(browser.popupOptions(&browser.results,point).getTargetScreenArea()==juce::Rectangle<int>{123,234,1,1},"Context menu anchors at exact screen click rather than full list bounds");
    check(browser.back.getY()==browser.counter.getY()&&browser.breadcrumb.getY()==browser.counter.getY(),"Back and location share the results toolbar row");
    check(browser.packButton.getX()>browser.genres.getX()&&browser.packButton.getY()==browser.genres.getY(),"Pack chooser follows Genres");
    check(browser.stopPreview.getWidth()==28&&browser.stopPreview.getHeight()==24,"Player does not reserve an oversized Stop field");
    mrs::library::AudioAsset unknown;unknown.id=-1;check(browser.rowGlyph(unknown,false,false)=="unknown","Unknown type must not masquerade as One-shot");
    check(browser.rowGlyph(browser.view->assets[0],false,false)=="loop"&&browser.rowGlyph(browser.view->assets[1],false,false)=="sample_one_shot","Loop and One-shot have separate glyphs");
    check(browser.rowGlyph(browser.view->assets[0],false,true)=="play"&&browser.rowGlyph(browser.view->assets[1],true,false)=="play","Hover and keyboard selection expose Play");
    juce::SparseSet<int> dragRows;dragRows.addRange({0,1});int dragX{},dragY{};
    const auto drag=browser.results.createSnapshotOfRows(dragRows,dragX,dragY);
    const auto mouse=browser.results.getMouseXYRelative();
    check(drag.getImage().getWidth()==240&&drag.getImage().getHeight()==36&&dragX==mouse.x+18&&dragY==mouse.y+12,"Compact drag label follows cursor leaving insertion preview ahead");
    browserFocus263Smoke(browser,browser.results);
    browser.sendQuery();pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    const auto catalogue=browser.view;auto covers=std::make_shared<mrs::library::Snapshot>(*catalogue);
    const auto sourceId=covers->assets.front().source_id;
    covers->covers.push_back({"folder",sourceId,"","pack.png"});
    covers->covers.push_back({"folder",sourceId,"Jazz","category.png"});
    browser.view=covers;
    check(browser.sampleCover(covers->assets.front())=="managed:pack.png","Sample cover comes from pack rather than nested category");
    browser.view=catalogue;
    browser.results.selectRow(0);browser.results.selectRow(1,true,false);
    check(browser.selectedAssets().size()==2,"List mode must retain multiple WAV selection");
    browser.results.selectRow(1,false,true);browser.playPause.onClick();
    check(previews==1&&playing&&browser.activePreviewId==browser.view->assets[1].id,"Footer Play follows selected WAV using the existing preview path");
    browser.previousSound.onClick();check(previews==2&&browser.activePreviewId==browser.view->assets.front().id,"Footer previous sample follows active audition");
    browser.stopPreview.onClick();check(!playing&&browser.activePreviewId==0,"Footer Stop cancels audition");
    browser.results.selectRow(0,false,true);
    check(browser.results.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey))&&playing,"Space starts browser audition");
    check(browser.rowGlyph(browser.view->assets[0],false,false)=="stop","Playing sample exposes Stop even without hover");
    const auto started=previews;
    check(browser.results.keyPressed(juce::KeyPress(juce::KeyPress::downKey))&&previews==started+1&&browser.activePreviewId==browser.view->assets[1].id,"Down selects and auditions next sample");
    browser.results.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    check(previews==started+1,"Last-row arrow does not restart the same sample");
    browser.results.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
    check(!playing&&!browser.keyboardAudition,"Second Space disables audition");
    browser.results.keyPressed(juce::KeyPress(juce::KeyPress::upKey));
    check(previews==started+1&&browser.selectedAssets().front()==browser.view->assets[0].id,"Arrows select silently after stopping");
    check(browser.details.getText().contains("Brush_120BPM_Loop.wav")&&browser.details.getText().contains("BPM"),"Selected file path and technical details belong in footer");
    browser.setSize(960,640);
    const auto control=browser.rowLayout(browser.results.getWidth(),browser.results.getRowHeight()).control.getCentre().toFloat();
    const auto click=juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),control,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&browser.results,&browser.results,juce::Time::getCurrentTime(),control,juce::Time::getCurrentTime(),1,false);
    browser.listBoxItemClicked(0,click);check(playing&&browser.rowGlyph(browser.view->assets[0],true,false)=="stop","Inline Play starts existing preview");
    snapshot("samples-playing-960");
    browser.listBoxItemClicked(0,click);check(!playing&&!browser.keyboardAudition&&browser.activePreviewId==0,"Inline Stop stops and disarms arrow audition");
    browser.setSize(260,640);snapshot("samples-260");
    check(!browser.browserKey(juce::KeyPress('A')),"Unrelated browser keys remain unhandled");
    browser.query.search="jazz brushes";browser.rootMode=false;browser.sendQuery();
    pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.view->visible_assets==1,"Library text query must combine filename and tags");
    auto foreign=std::make_shared<mrs::library::Snapshot>(*browser.view);foreign->revision+=100;foreign->query.search="Edit Tags search";
    browser.pendingQuery=true;browser.setSnapshot(foreign);
    check(browser.query.search=="jazz brushes","A foreign editor query must not replace a pending browser search");
    browser.sendQuery();pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    browser.setSize(440,640);browser.listGrid.onClick();
    check(browser.gridView,"Grid mode uses a single WAV card");
    snapshot("samples-440");
    for(const auto* component:std::array<const juce::Component*,8>{&browser.search,&browser.instruments,&browser.genres,&browser.packButton,&browser.typeButton,&browser.keyButton,&browser.bpmButton,&browser.results})
        check(browser.getLocalBounds().contains(component->getBounds()),"Library controls escape narrow/wide browser bounds");
    browser.chooseSection(LibraryBrowserView::Section::tags);pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.rootMode&&!browser.cards.empty()&&browser.cards.front().kind=="group","Tags root needs category cover cards");
    browser.openCard(browser.cards.front());check(!browser.cards.empty()&&browser.cards.front().kind=="tag","Category opens tag cards");
    library.create_collection("Session");pump([](const auto& snap){return snap.phase==mrs::library::Phase::idle&&!snap.collections.empty();});
    browser.chooseSection(LibraryBrowserView::Section::collections);pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.cards.size()==1&&browser.cards.front().kind=="collection","Collections root needs cover cards");
    browser.openCard(browser.cards.front());pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.view->visible_assets==0&&!browser.rootMode,"Empty collection should stay in collection scope");
    wavDrop263Smoke();
}
}
