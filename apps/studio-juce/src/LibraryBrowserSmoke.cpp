#include "LibraryBrowserView.h"
#include <mrs/recording.hpp>
#include <chrono>
#include <thread>
namespace ui {
void libraryBrowserSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    const auto root=std::filesystem::temp_directory_path()/("mrs-library261-"+mrs::new_id().value);
    std::filesystem::create_directories(root/"samples"/"Jazz");
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}}cleanup{root};
    for(const auto* name:{"Brush_120BPM_Loop.wav","Kick_oneshot.wav"}){
        mrs::audio::Recorder recorder(root/"samples"/"Jazz"/name,48000,0,{0});std::array<float,512> pcm{};pcm.fill(.25f);recorder.capture(pcm.data(),1,512,0);recorder.finish();
    }
    mrs::library::SampleLibrary library(root/"catalog.sqlite");
    bool playing{};int previews{};
    LibraryBrowserView browser(library,[&](const auto&,double){++previews;playing=true;return true;},[&]{playing=false;},[&]{return std::tuple{playing,.25f,false};});
    const auto pump=[&](const auto& ready){
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(std::chrono::steady_clock::now()<until){auto snap=library.snapshot();browser.setSnapshot(snap);if(ready(*snap))return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        throw std::runtime_error("Library UI worker timed out");
    };
    library.add_source(root/"samples");pump([](const auto& snap){return snap.phase==mrs::library::Phase::idle&&snap.visible_assets==2;});
    browser.setSize(260,640);check(browser.rootMode&&browser.cards.size()==1,"Folders root should show source cover cards");
    check(browser.assetIndex(0,0)<0,"Folder card must not masquerade as a WAV drag");
    const auto snapshot=[&](const juce::String& name){for(float scale:{1.f,1.5f}){
        const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("juce-library261-"+name+"-"+juce::String(static_cast<int>(scale*100))+".png");
        auto output=file.createOutputStream();check(output!=nullptr,"Create Library preview");
        check(juce::PNGImageFormat().writeImageToStream(browser.createComponentSnapshot(browser.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*output),"Write Library preview");
    }};
    snapshot("folders-260");
    browser.openCard(browser.cards.front());pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.cards.size()==1&&browser.cards.front().key=="Jazz","Folder navigation must list direct children");
    browser.openCard(browser.cards.front());pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.cards.empty()&&browser.assetIndex(0,0)==0,"Folder navigation must preserve WAV indices");
    browser.results.selectRow(0);browser.results.selectRow(1,true,false);
    check(browser.selectedAssets().size()==2,"List mode must retain multiple WAV selection");
    browser.audition(0,0.,false);check(previews==1&&playing,"Audition must use injected existing preview path");
    browser.query.search="jazz brushes";browser.rootMode=false;browser.sendQuery();
    pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.view->visible_assets==1,"Library text query must combine filename and tags");
    browser.setSize(440,640);browser.listGrid.onClick();
    check(browser.gridView,"Grid mode uses a single WAV card");
    snapshot("samples-440");
    for(const auto* component:std::array<const juce::Component*,7>{&browser.search,&browser.instruments,&browser.genres,&browser.typeButton,&browser.keyButton,&browser.bpmButton,&browser.results})
        check(browser.getLocalBounds().contains(component->getBounds()),"Library controls escape narrow/wide browser bounds");
    browser.chooseSection(LibraryBrowserView::Section::tags);pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.rootMode&&!browser.cards.empty()&&browser.cards.front().kind=="group","Tags root needs category cover cards");
    browser.openCard(browser.cards.front());check(!browser.cards.empty()&&browser.cards.front().kind=="tag","Category opens tag cards");
    library.create_collection("Session");pump([](const auto& snap){return snap.phase==mrs::library::Phase::idle&&!snap.collections.empty();});
    browser.chooseSection(LibraryBrowserView::Section::collections);pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.cards.size()==1&&browser.cards.front().kind=="collection","Collections root needs cover cards");
    browser.openCard(browser.cards.front());pump([&](const auto& snap){return snap.query==browser.query&&snap.phase==mrs::library::Phase::idle;});
    check(browser.view->visible_assets==0&&!browser.rootMode,"Empty collection should stay in collection scope");
}
}
