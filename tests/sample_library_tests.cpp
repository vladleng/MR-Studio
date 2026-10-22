#include <mrs/sample_library.hpp>
#include <mrs/sample_tag_rules.hpp>
#include <mrs/sample_preview.hpp>
#include <mrs/audio.hpp>
#include <mrs/library_preview.hpp>
#include <mrs/ahead_renderer.hpp>
#include <cmath>
#ifdef _WIN32
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
using namespace mrs::library;
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Fixture {
    fs::path root;
    Fixture() {
        root=fs::temp_directory_path()/("mrs-sample-index-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    }
    ~Fixture() { std::error_code ec;fs::remove_all(root,ec); }
};
void wav(const fs::path& file,int frames=400) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file,std::ios::binary|std::ios::trunc);
    const auto put=[&](std::uint32_t value,int width){for(int i=0;i<width;++i)out.put(static_cast<char>((value>>(8*i))&255));};
    out.write("RIFF",4);put(36+frames*2,4);out.write("WAVEfmt ",8);
    put(16,4);put(1,2);put(1,2);put(48000,4);put(96000,4);put(2,2);put(16,2);
    out.write("data",4);put(frames*2,4);
    for(int i=0;i<frames;++i)put(0,2);
    check(static_cast<bool>(out),"Cannot write WAV fixture");
}
template<class Pred> std::shared_ptr<const Snapshot> wait_for(SampleLibrary& library,Pred pred) {
    for(int i=0;i<1200;++i) {
        auto result=library.snapshot();
        if(pred(*result)&&(result->phase!=Phase::idle||!result->analysis.running))return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("Sample Library async worker timeout: "+library.snapshot()->message);
}
const Source& only(const Snapshot& s) {
    check(s.sources.size()==1,"Expected exactly one source");
    return s.sources.front();
}
void test() {
    Fixture f;
    const auto libraryRoot=f.root/"samples";
    wav(libraryRoot/"Jazz"/"Brush_120BPM.wav");
    wav(libraryRoot/fs::path(u8"Кириллица")/fs::path(u8"Барабан.wav"),800);
    wav(libraryRoot/"Ambient"/"Brush_120BPM.wav"); // identical basename in a different folder
    std::ofstream(libraryRoot/"bad.wav")<<"broken";
    std::ofstream(libraryRoot/"not-midi.mid")<<"Not indexed";
    const auto database=f.root/"db"/"samples.sqlite";
    std::int64_t stableId{};
    {
        SampleLibrary lib(database);
        wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle;});
        lib.add_source(libraryRoot);
        auto s=wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle&&s.sources.size()==1&&s.sources.front().available==3;});
        check(only(*s).invalid==1 && only(*s).missing==0,"WAV count/invalid failed");
        stableId=only(*s).id;
        const auto version=s->revision;
        lib.rescan();
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>version;});
        check(s->changed==0,"Unchanged rescans must avoid re-inspecting files");
        lib.add_source(libraryRoot); // duplicate source must be idempotent
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>s->revision;});
        check(s->sources.size()==1,"Duplicate source was created");
        fs::remove(libraryRoot/"Ambient"/"Brush_120BPM.wav");
        wav(libraryRoot/"Jazz"/"Brush_120BPM.wav",801);
        const auto revision=s->revision;
        lib.rescan();
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>revision&&x.sources.front().missing==1;});
        check(only(*s).available==2&&only(*s).invalid==1&&only(*s).missing==1,"Missing/update state incorrect");
        const auto revision2=s->revision;
        lib.enable_source(stableId,false);
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>revision2;});
        check(!only(*s).enabled,"Source enable switch not persisted");
    }
    {
        SampleLibrary lib(database);
        auto s=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
        check(only(*s).id==stableId&&!only(*s).enabled&&only(*s).missing==1,"Restart/persistence failed");
        const auto moved=f.root/"relocated";
        fs::rename(libraryRoot,moved);
        lib.relink_source(stableId,moved);
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.sources.front().root==moved;});
        check(only(*s).id==stableId,"Relink lost stable ID");
        lib.enable_source(stableId,true);
        s=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&x.sources.front().enabled;});
        const auto revision=s->revision;
        lib.remove_source(stableId);
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>revision&&x.sources.empty();});
        check(fs::exists(moved/"Jazz"/"Brush_120BPM.wav"),"Source removal altered user files");
    }
    // A missing root is reported, but cannot wipe a previously committed index.
    {
        SampleLibrary lib(database);
        wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
        lib.add_source(f.root/"not-there");
        auto s=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::error;});
        check(only(*s).available==0,"Missing root should not create phantom samples");
    }
    std::cout<<"MR Library #252: SQLite, unicode, duplicate names, incremental scan, relink, restart PASS\n";
}
}
void tagging() {
    Fixture f;
    const auto root=f.root/"samples";
    wav(root/"Jazz"/"Jazz_Brush_Swing_120BPM_Loop.wav");
    wav(root/"Bass"/"UprightBass_Am_95BPM_Loop.wav");
    wav(root/"Misc"/"Guitar_A_Swell.wav");
    wav(root/"Misc"/"Kick_Deep_01.wav");
    const auto db=f.root/"db"/"existing.sqlite";
    auto has=[](const AudioAsset& a,const std::string& group,const std::string& label,const std::string& status){
        return std::any_of(a.tags.begin(),a.tags.end(),[&](const auto& t){
            return t.group==group&&t.label==label&&t.status==status;
        });
    };
    auto named=[](const Snapshot& s,const std::string& suffix)->const AudioAsset&{
        for(const auto& a:s.assets)if(a.relative_path.find(suffix)!=std::string::npos)return a;
        throw std::runtime_error("Expected WAV missing from tag page");
    };
    std::int64_t kick_id{};
    {
        SampleLibrary lib(db);
        wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle;});
        lib.add_source(root);
        auto snap=wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle&&s.assets.size()==4;});
        const auto& jazz=named(*snap,"Jazz_Brush");
        check(has(jazz,"Genre","Jazz","proposed")&&has(jazz,"Technique","Brushes","proposed")&&
            has(jazz,"Rhythm","Swing","proposed")&&jazz.detected_bpm==120,"Jazz auto tagging");
        const auto& bass=named(*snap,"UprightBass");
        check(has(bass,"Instrument","Upright Bass","proposed")&&bass.detected_bpm==95&&
            bass.detected_key=="Am","Upright/key auto tagging");
        check(named(*snap,"Guitar_A").detected_key.empty(),"Single isolated A is NOT a key");
        check(named(*snap,"Kick_Deep").detected_bpm==0,"Take numbers are not BPM");
        kick_id=named(*snap,"Kick_Deep").id;
        lib.set_tag_status({kick_id},"Instrument","Kick","rejected");
        auto previous=snap->revision;
        snap=wait_for(lib,[&](const Snapshot& s){return s.revision>previous&&s.phase==Phase::idle;});
        check(has(named(*snap,"Kick_Deep"),"Instrument","Kick","rejected"),"Reject proposal");
        previous=snap->revision;lib.rescan();
        snap=wait_for(lib,[&](const Snapshot& s){return s.revision>previous&&s.phase==Phase::idle&&s.message=="Scan complete";});
        check(has(named(*snap,"Kick_Deep"),"Instrument","Kick","rejected"),"Rescan overwrote rejection");
        lib.add_tag({kick_id},"Custom","My Sample");
        previous=snap->revision;
        snap=wait_for(lib,[&](const Snapshot& s){return s.revision>previous&&s.phase==Phase::idle&&
            has(named(s,"Kick_Deep"),"Custom","My Sample","confirmed");});
        lib.set_bpm({kick_id},0);
        previous=snap->revision;
        snap=wait_for(lib,[&](const Snapshot& s){return s.revision>previous&&s.phase==Phase::idle;});
        check(named(*snap,"Kick_Deep").manual_bpm==-1,"Explicit unknown BPM");
        lib.undo_tag_edit();
        previous=snap->revision;
        snap=wait_for(lib,[&](const Snapshot& s){return s.revision>previous&&s.phase==Phase::idle;});
        check(named(*snap,"Kick_Deep").manual_bpm==0,"Undo metadata edit");
        lib.add_rule("Genre","misc","My Style",true);
        previous=snap->revision;
        snap=wait_for(lib,[&](const Snapshot& s){return s.revision>previous&&s.phase==Phase::idle;});
        check(has(named(*snap,"Kick_Deep"),"Genre","My Style","proposed"),"Custom folder rule");
        check(has(named(*snap,"Kick_Deep"),"Custom","My Sample","confirmed"),"Rule wiped manual tag");
        check(has(named(*snap,"Kick_Deep"),"Instrument","Kick","rejected"),"Rule wiped rejection");
    }
    {
        SampleLibrary lib(db);
        auto snap=wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle&&s.assets.size()==4;});
        check(has(named(*snap,"Kick_Deep"),"Custom","My Sample","confirmed"),"Restart wiped manual tag");
        check(has(named(*snap,"Kick_Deep"),"Instrument","Kick","rejected"),"Restart wiped tag veto");
        check(has(named(*snap,"Kick_Deep"),"Genre","My Style","proposed"),"Restart wiped custom rule");
        lib.undo_tag_edit(); // undo persisted history batch: last was add manual tag (BPM was undone)
        const auto previous=snap->revision;
        snap=wait_for(lib,[&](const Snapshot& s){return s.revision>previous&&s.phase==Phase::idle;});
        check(!has(named(*snap,"Kick_Deep"),"Custom","My Sample","confirmed"),"Undo manual tagging");
    }
    std::cout<<"MR Library #253: filename proposals, provenance, veto, manual/undo, rules, persistence PASS\\n";
}
void migration_v1() {
    Fixture f;
    const auto libraryRoot=f.root/"sounds";
    wav(libraryRoot/"Jazz"/"Jazz_Brush_120BPM_Loop.wav");
    const auto file=f.root/"db"/"legacy.sqlite";
    fs::create_directories(file.parent_path());
    sqlite3* db{};
    const auto u=file.u8string();
    const std::string filename(u.begin(),u.end());
    check(sqlite3_open(filename.c_str(),&db)==SQLITE_OK,"Create legacy database");
    const auto exec=[&](const char* sql){
        char* error=nullptr;
        if(sqlite3_exec(db,sql,nullptr,nullptr,&error)!=SQLITE_OK){
            const std::string why=error?error:"SQLite test setup";
            sqlite3_free(error);throw std::runtime_error(why);
        }
    };
    exec("CREATE TABLE sources (id INTEGER PRIMARY KEY, path TEXT NOT NULL UNIQUE, enabled INTEGER NOT NULL DEFAULT 1)");
    exec("CREATE TABLE assets (id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,"
         " relative_path TEXT NOT NULL,bytes INTEGER NOT NULL,modified INTEGER NOT NULL,rate INTEGER,"
         " channels INTEGER,bits INTEGER,frames INTEGER,status TEXT NOT NULL,error TEXT NOT NULL DEFAULT '',"
         " seen INTEGER NOT NULL DEFAULT 0,UNIQUE(source_id,relative_path))");
    exec("CREATE INDEX assets_status_idx ON assets(source_id,status)");
    exec("PRAGMA user_version=1");
    sqlite3_stmt* stmt{};
    check(sqlite3_prepare_v2(db,"INSERT INTO sources(id,path) VALUES(7,?)",-1,&stmt,nullptr)==SQLITE_OK,
          "Prepare legacy source insert");
    const auto root8=libraryRoot.u8string();
    const std::string root(root8.begin(),root8.end());
    sqlite3_bind_text(stmt,1,root.c_str(),-1,SQLITE_TRANSIENT);
    check(sqlite3_step(stmt)==SQLITE_DONE,"Insert legacy source");
    sqlite3_finalize(stmt);
    exec("INSERT INTO assets(id,source_id,relative_path,bytes,modified,rate,channels,bits,frames,status)"
         " VALUES(81,7,'Jazz/Jazz_Brush_120BPM_Loop.wav',0,0,48000,1,16,400,'ok')");
    sqlite3_close(db);
    {
        SampleLibrary lib(file);
        auto snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
        check(snap->sources.size()==1&&snap->sources[0].id==7,"Migration lost source ID");
        check(snap->assets.size()==1&&snap->assets[0].id==81,"Migration lost asset ID");
        check(std::any_of(snap->folders.begin(),snap->folders.end(),[](const LibraryFolder& x){
            return x.relative_path=="Jazz"&&x.source_id==7;
        }),"Existing v1/v2 catalogue must restore folder hierarchy without scanning");
        check(snap->assets[0].detected_bpm==120,"Migration failed to backfill proposals");
        check(std::any_of(snap->assets[0].tags.begin(),snap->assets[0].tags.end(),
              [](const auto& tag){return tag.group=="Genre"&&tag.label=="Jazz"&&tag.status=="proposed";}),
              "Migration dropped auto tags");
    }
    std::cout<<"MR Library #253: schema v1 to v2 migration keeps legacy IDs and fills tags PASS\\n";
}
void browser_v3(){
    Fixture f;
    const auto root=f.root/"samples";
    wav(root/"Jazz"/"Acoustic"/"Jazz_Brush_Swing_120BPM_Loop.wav");
    wav(root/"Jazz"/"Acoustic"/"Kick_80BPM_Loop.wav");
    wav(root/"Jazz"/"Electronic"/"Jazz_Snare_Loop.wav");
    wav(root/"Ambient"/"Pad_Loop.wav");
    fs::create_directories(root/"Jazz");
    {
        std::ofstream image(root/"Jazz"/"cover.jpg",std::ios::binary);
        image<<"JPEG cover fixture";
    }
    {
        std::ofstream image(root/"Ambient"/"folder.png",std::ios::binary);
        image<<"PNG cover fixture";
    }
    const auto db=f.root/"db"/"library.sqlite";
    std::int64_t item{},collection{};
    {
        SampleLibrary lib(db);
        wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
        lib.add_source(root);
        auto snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&x.visible_assets==4
            &&x.folders.size()>=5;});
        check(snap->assets.size()==4,"Browser snapshot missing WAVs");
        check(std::any_of(snap->folders.begin(),snap->folders.end(),[](const LibraryFolder& x){
            return x.relative_path=="Jazz/Acoustic"&&x.artwork_path=="Jazz/cover.jpg";
        }),"Package folder must inherit closest parent cover");
        check(std::any_of(snap->assets.begin(),snap->assets.end(),[](const AudioAsset& x){
            return x.relative_path.find("Jazz/Acoustic/")!=std::string::npos&&x.artwork_path=="Jazz/cover.jpg";
        }),"Asset must inherit corresponding cover");
        item=snap->assets.front().id;
        lib.create_collection("Studio Favorites");
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&!x.collections.empty();});
        check(snap->collections.front().name=="Studio Favorites","Collection creation");
        collection=snap->collections.front().id;
        lib.set_collection_items(collection,{item},true);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&
            x.collections.front().count==1;});
        const auto saved=snap->revision;
        lib.set_favorites({item},true);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>saved;});
        LibraryQuery q;q.favorites_only=true;
        lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query.favorites_only;});
        check(snap->visible_assets==1&&snap->assets.size()==1&&snap->assets[0].id==item,
            "Favorite browse filter");
        q=LibraryQuery{};q.collection_id=collection;
        lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query.collection_id==collection;});
        check(snap->visible_assets==1&&snap->assets[0].id==item,"Collection membership browse");
        q=LibraryQuery{};q.folder="Jazz/Acoustic";q.limit=1;
        lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query.folder=="Jazz/Acoustic";});
        check(snap->visible_assets==2&&snap->assets.size()==1,"Folder prefix and limit");
        q.offset=1;lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query.offset==1;});
        check(snap->visible_assets==2&&snap->assets.size()==1,"Paged offset");
        q=LibraryQuery{};q.tags.push_back({"Genre","Jazz"});q.tags.push_back({"Instrument","Kick"});
        lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&!x.query.tags.empty();});
        check(snap->visible_assets==1,"AND across tag groups");
        q.tags.push_back({"Instrument","Snare"});lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query.tags.size()==3;});
        check(snap->visible_assets==2,"OR within tag group");
        lib.set_artwork(snap->assets.front().source_id,"Jazz/Acoustic",root/"Ambient"/"folder.png");
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&
            std::any_of(x.folders.begin(),x.folders.end(),[](const LibraryFolder& f){
                return f.relative_path=="Jazz/Acoustic"&&f.artwork_path=="Ambient/folder.png";
            });});
        const auto rev=snap->revision;lib.rescan();
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>rev
            &&x.message=="Scan complete";});
        check(std::any_of(snap->folders.begin(),snap->folders.end(),[](const LibraryFolder& f){
            return f.relative_path=="Jazz/Acoustic"&&f.artwork_path=="Ambient/folder.png";
        }),"Manual artwork override lost on rescan");
    }
    {
        SampleLibrary lib(db);
        const auto snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&
            !x.collections.empty()&&x.visible_assets==4;});
        check(snap->collections[0].count==1&&snap->collections[0].id==collection,
            "Collections lost after restart");
        check(std::any_of(snap->assets.begin(),snap->assets.end(),[&](const AudioAsset& a){
            return a.id==item&&a.favorite;
        }),"Favorites lost on restart");
        check(std::any_of(snap->folders.begin(),snap->folders.end(),[](const LibraryFolder& f){
            return f.relative_path=="Jazz/Acoustic"&&f.artwork_path=="Ambient/folder.png";
        }),"Manual cover lost on restart");
        lib.delete_collection(collection);
        const auto after=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&x.collections.empty();});
        check(after->visible_assets==4,"Deleting collection removed audio assets");
    }
    check(fs::exists(root/"Jazz"/"Acoustic"/"Jazz_Brush_Swing_120BPM_Loop.wav"),
        "Browser metadata operations touched original WAV");
    std::cout<<"MR Library #254: folders, artworks, favorites, collections and multi-tag pages PASS\\n";
}
void unicode_browser_search(){
    Fixture f;
    const auto root=f.root/"sounds";
    wav(root/fs::path(u8"Русские")/fs::path(u8"БАРАБАН_80BPM_Loop.wav"));
    wav(root/"Jazz"/"Piano_Loop.wav");
    SampleLibrary lib(f.root/"db"/"unicode.sqlite");
    wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
    lib.add_source(root);
    auto snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&x.visible_assets==2;});
    LibraryQuery q;
    const std::u8string lower=u8"барабан";
    q.search=std::string(lower.begin(),lower.end());
    lib.browse(q);
    snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query.search==q.search;});
    check(snap->visible_assets==1&&snap->assets.size()==1&&
        snap->assets[0].relative_path.find("BPM")!=std::string::npos,
        "Cyrillic Unicode case-insensitive search failed");
    std::cout<<"MR Library #254: Cyrillic casefold browser search PASS\\n";
}
void safe_preview(){
    Fixture f;
    const auto root=f.root/"samples";
    wav(root/"Jazz"/"Kick.wav",8192);
    const auto file=root/"Jazz"/"Kick.wav";
    std::ofstream(root/"Bad.wav")<<"invalid";
    SampleLibrary lib(f.root/"db"/"preview.sqlite");
    wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle;});
    lib.add_source(root);
    const auto snap=wait_for(lib,[](const Snapshot& s){
        return s.phase==Phase::idle&&s.sources.size()==1&&s.sources[0].available==1;
    });
    const auto at=std::find_if(snap->assets.begin(),snap->assets.end(),[](const AudioAsset& a){
        return a.status=="ok";
    });
    check(at!=snap->assets.end(),"Preview fixture missing");
    const auto path=asset_path(*snap,at->id);
    check(path&&*path==file,"Indexed WAV path resolution failed");
    const auto peaks=build_wav_preview(*path,std::make_shared<std::atomic<bool>>(false));
    check(peaks.rate==48000&&peaks.channels==1&&peaks.bits==16&&peaks.frames==8192,
        "Preview did not retain WAV metadata");
    check(std::all_of(peaks.peaks.begin(),peaks.peaks.end(),[](float v){return v==0.f;}),
        "Expected silent waveform fixture");
    auto cancelled=std::make_shared<std::atomic<bool>>(true);
    bool refused=false;
    try{(void)build_wav_preview(file,cancelled);}catch(const std::runtime_error&){refused=true;}
    check(refused,"Waveform cancellation ignored");
    auto tampered=*snap;
    tampered.assets.front().relative_path="../outside.wav";
    check(!asset_path(tampered,tampered.assets.front().id),"Traversal must be refused");
    tampered=*snap;tampered.assets.front().status="missing";
    check(!asset_path(tampered,tampered.assets.front().id),"Missing file must be refused");
    fs::rename(file,root/"Jazz"/"Kick.WaV");
    tampered=*snap;
    auto mixedCase=std::find_if(tampered.assets.begin(),tampered.assets.end(),
        [&](const auto& asset){return asset.id==at->id;});
    mixedCase->relative_path="Jazz/Kick.WaV";
    check(asset_path(tampered,at->id).has_value(),"Mixed-case WAV extension rejected");
    fs::remove(root/"Jazz"/"Kick.WaV");
    check(!asset_path(*snap,at->id),"Deleted WAV must be rejected before rescan");
    fs::create_directory(file);
    check(!asset_path(*snap,at->id),"Directory named WAV must not be draggable");
    std::cout<<"MR Library #255: safe WAV resolver, waveform, cancellation PASS\\n";
}
void audition_mixes_single_device(){
    Fixture f;
    const auto wave=f.root/"audition.wav";
    wav(wave,48000);
    {
        // Replace silent PCM samples with a constant non-clipping reference.
        std::fstream out(wave,std::ios::binary|std::ios::in|std::ios::out);
        out.seekp(44);
        for(int i=0;i<48000;++i){
            const unsigned value=10000;
            out.put(static_cast<char>(value&255));
            out.put(static_cast<char>((value>>8)&255));
        }
        check(static_cast<bool>(out),"Create sample audition fixture");
    }
    mrs::audio::AudioEngine engine;
    engine.prepare({48000,0,2,128},{});
    engine.play_library_preview(wave);
    std::vector<float> output(256);
    bool sound=false;
    for(int attempt=0;attempt<200&&!sound;++attempt){
        std::fill(output.begin(),output.end(),0.f);
        engine.process(nullptr,output.data(),128);
        sound=std::any_of(output.begin(),output.end(),[](float value){return value>.05f;});
        if(!sound)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    check(sound,"Library audio was not mixed into the existing master callback");
    check(std::abs(output[0]-output[1])<.001f,"Mono audition should be centered");
    check(engine.library_preview_state().playing,"Audio preview should run without project transport");
    engine.stop_library_preview();
    std::fill(output.begin(),output.end(),0.f);
    engine.process(nullptr,output.data(),128);
    check(std::all_of(output.begin(),output.end(),[](float value){return value==0.f;}),
          "Library stop must silence preview immediately");
    engine.play_library_preview(wave,.5);
    bool seekSound=false;
    for(int attempt=0;attempt<200&&!seekSound;++attempt){
        std::fill(output.begin(),output.end(),0.f);
        engine.process(nullptr,output.data(),128);
        seekSound=std::any_of(output.begin(),output.end(),[](float value){return value>.05f;});
        if(!seekSound)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    check(seekSound,"Click-to-seek preview should restart audio");
    check(engine.library_preview_state().progress>=.49f,"Preview seek position not observed");
    std::cout<<"MR Library #255: RT-safe engine audition, seek, stop, mono route PASS\\n";
}
void audition_device_modes(){
    Fixture f;
    const auto wave=f.root/"routed.wav";
    wav(wave,48000);
    {
        std::fstream out(wave,std::ios::binary|std::ios::in|std::ios::out);
        out.seekp(44);
        for(int i=0;i<48000;++i){out.put('\x10');out.put('\x27');} // PCM16 10000
    }
    for(const bool anticipative:{false,true}){
        auto engine=std::make_shared<mrs::audio::AudioEngine>();
        mrs::audio::RenderGraph graph;
        graph.master_gain=.5f;graph.master_outputs={2,3};
        engine->prepare({44100,0,4,128},graph);
        mrs::audio::AheadRenderer renderer(engine,128,anticipative?512:0);
        renderer.start();
        if(anticipative)check(renderer.active(),"Preview anticipation fixture must use the producer");
        engine->play_library_preview(wave,.25);
        std::array<float,512> output{};
        bool sound=false;
        for(int attempt=0;attempt<250&&!sound;++attempt){
            renderer.process(nullptr,output.data(),128);
            sound=output[2]>.05f;
            if(!sound)std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        check(sound,"Preview must remain audible with Process Buffer enabled or disabled");
        const auto expected=(10000.f/32768.f)*.65f*.5f;
        check(std::abs(output[2]-expected)<.001f&&std::abs(output[3]-expected)<.001f,
              "Preview must follow Master gain and selected ASIO output pair exactly once");
        check(output[0]==0.f&&output[1]==0.f,"Preview leaked to unselected output channels");
        check(engine->state().playback!=mrs::PlaybackState::playing,
              "Audition must not start project transport");
        engine->stop_library_preview();
        renderer.process(nullptr,output.data(),128);
        check(std::all_of(output.begin(),output.end(),[](float value){return value==0.f;}),
              "Stop must silence audition without waiting for Process Buffer packets");
        renderer.stop();
        engine->play_library_preview(wave);
        engine->prepare({48000,0,4,128},graph);
        check(!engine->library_preview_state().playing,"Device prepare must cancel the old sample-rate stream");
    }
    mrs::audio::LibraryPreviewStream stream;
    std::array<float,256> output{};
    stream.play(wave,48000);
    stream.mix(output.data(),128,2,false);
    check(!stream.status().playing,"Recording gate must cancel audition");
    check(std::all_of(output.begin(),output.end(),[](float value){return value==0.f;}),
          "Preview must not mix during recording");
    stream.play(f.root/"missing.wav",48000);
    for(int attempt=0;attempt<250&&stream.status().playing;++attempt){
        stream.mix(output.data(),128,2,true);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    check(stream.status().failed&&!stream.status().playing,"Missing preview must report failure and stop");
    std::cout<<"MR Library #255: Process Buffer, output/gain, device-rate reset, recording and errors PASS\n";
}
void search_v4(){
    Fixture fixture;const auto root=fixture.root/"samples",database=fixture.root/"index.sqlite";
    wav(root/"Jazz"/"Brush_120BPM_Loop.wav");wav(root/"Ambient"/"Bass_Am_95BPM_Loop.wav");wav(root/"Jazz"/"Kick_oneshot.wav");
    const auto cover=fixture.root/"external.png";
    // Real 1x1 PNG; belongs only to this fixture, outside the sample source.
    const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,11,73,68,65,84,120,156,99,96,0,2,0,0,5,0,1,165,246,69,64,0,0,0,0,73,69,78,68,174,66,96,130};
    {std::ofstream out(cover,std::ios::binary);out.write(reinterpret_cast<const char*>(png),sizeof(png));}
    check(artwork_image_supported(cover),"Valid external PNG rejected");
    {std::ofstream out(fixture.root/"bad.png");out<<"invalid";}
    check(!artwork_image_supported(fixture.root/"bad.png"),"Invalid image accepted");
    std::string stored;std::int64_t source{};
    {
        SampleLibrary lib(database);lib.add_source(root);
        auto snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&x.visible_assets==3;});source=snap->sources.front().id;
        LibraryQuery q;q.search="jazz brushes";q.sort="relevant";lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query==q;});
        check(snap->visible_assets==1&&snap->assets.front().relative_path=="Jazz/Brush_120BPM_Loop.wav","Search terms must match independent filename/tag fields");
        check(std::none_of(snap->facets.begin(),snap->facets.end(),[](const auto& x){return x.label=="Ambient";}),"Facet counts must reflect query");
        check(std::any_of(snap->all_facets.begin(),snap->all_facets.end(),[](const auto& x){return x.label=="Ambient";}),"Other choices disappear after filtering");
        q={};q.tags={{"Type","Loop"}};q.excluded_tags={{"Genre","Jazz"}};lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query==q;});
        check(snap->visible_assets==1&&snap->assets.front().relative_path.find("Bass")!=std::string::npos,"Exclude tags / include combination failed");
        const auto bass=snap->assets.front().id;lib.set_key({bass},"Bbmaj");
        snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&!x.assets.empty()&&x.assets.front().manual_key=="Bb major";});
        q={};q.key="A# major";lib.browse(q);snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query==q;});
        check(snap->visible_assets==1&&snap->assets.front().id==bass,"Enharmonic key aliases must agree");
        q={};q.sort="random";q.shuffle_seed=123;q.limit=1;lib.browse(q);
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query==q;});const auto first=snap->assets.front().id;
        q.offset=1;lib.browse(q);snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.query==q;});
        check(snap->assets.front().id!=first,"Random pagination repeats an item");
        lib.set_cover("folder",source,"Jazz",cover);
        snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&!x.covers.empty();});
        stored=snap->covers.front().image;check(fs::is_regular_file(snap->artwork_root/stored),"Managed cover was not copied");
        check(!fs::exists(root/"external.png"),"Cover modified source folder");fs::remove(cover);
        lib.rescan();snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&x.message=="Scan complete";});
        check(snap->covers.front().image==stored,"Rescan removed managed cover");
        lib.set_cover("group",0,"Instrument",snap->artwork_root/stored);
        snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&x.covers.size()==2;});
    }
    {
        SampleLibrary lib(database);auto snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
        check(snap->covers.size()==2&&fs::is_regular_file(snap->artwork_root/stored),"Managed covers lost on restart");
        lib.set_cover("folder",source,"Jazz",{});
        snap=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&std::any_of(x.covers.begin(),x.covers.end(),[&](const auto& c){return c.kind=="folder"&&c.image.empty();});});
        check(!fs::exists(snap->artwork_root/stored),"Reset did not clean replaced managed image");
    }
    // Existing v3 schema has the same indexed metadata and no managed-cover table.
    sqlite3* legacy{};const auto dbText=database.generic_u8string();
    check(sqlite3_open(reinterpret_cast<const char*>(dbText.c_str()),&legacy)==SQLITE_OK,"Open migration fixture");
    check(sqlite3_exec(legacy,"DROP TABLE managed_covers; PRAGMA user_version=3",nullptr,nullptr,nullptr)==SQLITE_OK,"Prepare v3 migration fixture");sqlite3_close(legacy);
    {SampleLibrary lib(database);const auto snap=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
        check(snap->visible_assets==3&&snap->sources.front().id==source&&snap->covers.empty(),"V3 migration lost catalogue identities");}
    bool backedUp=false;
    for(const auto& entry:fs::directory_iterator(fixture.root))if(entry.path().filename().string().starts_with("index.sqlite.pre-v6-"))backedUp=true;
    check(backedUp,"V3 migration needs a consistent rollback backup");
    std::cout<<"MR Library #261: multiword/filter/facets/stable shuffle/managed covers PASS\n";
}

void type_taxonomy(){
    Fixture f;const auto db=f.root/"types.sqlite";wav(f.root/"audio"/"FX_loop.wav");
    {SampleLibrary lib(db);lib.add_source(f.root/"audio");wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle&&s.visible_assets==1;});}
    sqlite3* raw{};const auto text=db.generic_u8string();
    check(sqlite3_open(reinterpret_cast<const char*>(text.c_str()),&raw)==SQLITE_OK,"Open Type taxonomy fixture");
    check(sqlite3_exec(raw,"INSERT INTO tags(group_name,label) VALUES('Type','FX'); INSERT INTO asset_tags SELECT a.id,t.id,'manual','confirmed',1 FROM assets a,tags t WHERE t.group_name='Type' AND t.label='FX'",nullptr,nullptr,nullptr)==SQLITE_OK,"Seed legacy confirmed Type");sqlite3_close(raw);
    {SampleLibrary lib(db);auto snap=wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle;});
        check(snap->assets.size()==1,"Taxonomy preserves indexed asset");
        bool fx=false,loop=false;for(const auto& t:snap->assets.front().tags){
            check(t.group!="Type"||t.label=="Loop"||t.label=="One-shot","Type must contain only Loop and One-shot");
            if(t.group=="Instrument"&&t.label=="FX"&&t.status=="confirmed")fx=true;
            if(t.group=="Type"&&t.label=="Loop")loop=true;
        }check(fx&&loop,"Legacy Type moves to Instrument preserving confirmation");
    }
    const auto result=classify_sample_path("FX_Fill_SFX_Transition_oneshot.wav",{});
    int instruments=0;bool one=false;for(const auto& t:result.tags){if(t.group=="Instrument")++instruments;if(t.group=="Type"&&t.label=="One-shot")one=true;}
    check(instruments==4&&one,"FX/Fill/SFX/Transition are Instruments; One-shot is Type");
}

void type_search_coverage(){
    const auto typeOf=[](const std::string& path){
        const auto classified=classify_sample_path(path,{});std::string type;
        for(const auto& tag:classified.tags)if(tag.group=="Type"){check(type.empty(),"Automatic file type must be exclusive");type=tag.label;}
        return type;
    };
    for(const auto* folder:{"One Shots","One-Shots","one_shots","Oneshots","OneShots","ONESHOTS","1-Shots"})
        check(typeOf(std::string(folder)+"/Drums/Kick.wav")=="One-shot","Plural/camel/number One-shot folder aliases");
    check(typeOf("Loops/Drums/Ghosthack_Kick_Snare_115bpm.wav")=="Loop","Loops folder supplies type when filename omits it");
    check(typeOf("Loops/One Shots/Kick.wav")=="One-shot"&&typeOf("One Shots/Loops/Groove.wav")=="Loop","Nearest labelled folder wins");
    check(typeOf("Loops/Clap_one_shot.wav")=="One-shot"&&typeOf("One Shots/Bass_loop.wav")=="Loop","Filename wins over inherited folder type");
    check(typeOf("Kick_oneshot01.wav")=="One-shot"&&typeOf("Drums_Loop02.wav")=="Loop"&&typeOf("Loopback01.wav").empty(),"Numeric take suffixes preserve explicit type without prefix false positives");
    check(typeOf("Loose/Texture.wav").empty()&&typeOf("Loop_OneShot.wav").empty(),"Unknown/conflicting types must not be silently assigned");
    for(const auto* path:{"Drums/Snaps/KSHMR Snap - Compact.wav","Drums/Snaps/KSHMR Snap - Roomy.wav","Drums/Snaps/Compact.wav","Cymatics - Gold Modern Crash 20.wav"})
        check(typeOf(path)=="One-shot","Single percussion filenames and categories supply missing One-shot Type");
    for(const auto* path:{"Drums/Kicks/Kick01.wav","Percussion/Rimshot_07.wav","FX/Impact09.wav","Synths/Stab04_Am.wav","Single Hits/Tone.wav","OneHits/Tone.wav","Ваншоты/Тон.wav"})
        check(typeOf(path)=="One-shot","General single-hit categories and numbered aliases cover different sample families");
    check(typeOf("Drums/Fill_128BPM.wav")=="Loop"&&typeOf("Synths/Arp_120_Am.wav")=="Loop","Tempo-bearing rhythmic phrases supply Loop without an explicit loop word");
    check(typeOf("One Shots/Arp_120BPM.wav")=="One-shot"&&typeOf("Loops/Stab01.wav")=="Loop","Explicit Type outranks all fallback families");
    for(const auto* path:{"KSHMR 808 - Clean - Earthquake (C).wav","KSHMR 808 - Clean - Pressure (E).wav","KSHMR Closed Hat - Acoustic - Clean.wav","Closed Hi-Hats/Acoustic.wav","KSHMR Hip Hop Snare - Classic - Candy.wav","KSHMR Hip Hop Snare - Classic - Player.wav"})
        check(typeOf(path)=="One-shot","User examples: pitched 808, closed hats and hip-hop snares are general single-hit families");
    check(typeOf("Loops/KSHMR 808 - Groove.wav")=="Loop"&&typeOf("KSHMR 808 - Groove_120BPM.wav")=="Loop","808 bass family must not override explicit/tempo phrase loops");
    for(const auto* path:{"KSHMR Snap - Loop.wav","Loops/Snaps/Compact.wav"})
        check(typeOf(path)=="Loop","Explicit Loop outranks single percussion inference");
    for(const auto* path:{"Snaps/Fill.wav","Kick_Snare_115bpm.wav","Kick_Groove.wav","Snap_OneShot_Loop.wav"})
        check(typeOf(path).empty(),"Tempo, phrases and conflicts cannot be mistaken for a single hit");
    Fixture fixture;const auto db=fixture.root/"types.sqlite",root=fixture.root/"audio";
    for(const auto* path:{"One Shots/Drums/Kick.wav","One-Shots/Drums/Snare.wav","Oneshots/Perc/Hat.wav","one_shots/FX/Boom.wav",
        "Loops/Drums/Kick_115BPM.wav","loops/Groove.wav","Loops/One Shots/Piano.wav","One Shots/Loops/Brush.wav",
        "Loops/one_shot_Clap.wav","One Shots/Bass_loop.wav","Loose/Texture.wav"})wav(root/path);
    const auto queryType=[](SampleLibrary& lib,const std::string& label,std::uint64_t expected){
        LibraryQuery q;q.tags={{"Type",label}};lib.browse(q);
        const auto snap=wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.query==q;});
        check(snap->visible_assets==expected,"Type search must include every indexed alias/inherited match");
        return snap;
    };
    {
        SampleLibrary lib(db);lib.add_source(root);auto snap=wait_for(lib,[](const Snapshot& v){return v.phase==Phase::idle&&v.visible_assets==11;});
        const auto target=std::find_if(snap->assets.begin(),snap->assets.end(),[](const auto& asset){return asset.relative_path=="Loops/Drums/Kick_115BPM.wav";});
        check(target!=snap->assets.end(),"Find manual Type fixture");const auto id=target->id;
        queryType(lib,"One-shot",6);queryType(lib,"Loop",4);
        auto revision=lib.snapshot()->revision;lib.add_tag({id},"Type","One-shot");
        wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision;});
        queryType(lib,"One-shot",7);queryType(lib,"Loop",3);
        revision=lib.snapshot()->revision;lib.undo_tag_edit();wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision;});
        queryType(lib,"One-shot",6);queryType(lib,"Loop",4);
        revision=lib.snapshot()->revision;lib.add_tag({id},"Type","One-shot");wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision;});
        revision=lib.snapshot()->revision;lib.rescan();wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision&&v.message=="Scan complete";});
        queryType(lib,"One-shot",7);queryType(lib,"Loop",3);
    }
    {
        SampleLibrary lib(db);wait_for(lib,[](const Snapshot& v){return v.phase==Phase::idle;});
        queryType(lib,"One-shot",7);queryType(lib,"Loop",3);
        const auto sourceRoot=fixture.root/"One Shots";wav(sourceRoot/"RootKick.wav");
        LibraryQuery q;lib.browse(q);wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.query==q;});
        lib.add_source(sourceRoot);wait_for(lib,[](const Snapshot& v){return v.phase==Phase::idle&&v.visible_assets==12;});
        queryType(lib,"One-shot",8);queryType(lib,"Loop",3);
    }
}

void pack_type_search(){
    Fixture fixture;const auto root=fixture.root/"Library";
    for(const auto* path:{"KSHMR/Drums/Snaps/KSHMR Snap - Compact.wav","KSHMR/Drums/Snaps/Roomy.wav",
        "KSHMR/Loops/Snap.wav","Cymatics/Drums/Cymatics - Gold Modern Crash 20.wav","Cymatics/Loops/Crash.wav"})wav(root/path);
    SampleLibrary lib(fixture.root/"pack.sqlite");lib.add_source(root);
    auto snap=wait_for(lib,[](const Snapshot& v){return v.phase==Phase::idle&&v.visible_assets==5;});
    const auto query=[&](LibraryQuery q,std::uint64_t count){lib.browse(q);auto v=wait_for(lib,[&](const Snapshot& s){return s.phase==Phase::idle&&s.query==q;});check(v->visible_assets==count,"Pack and Type filters must compose over the full catalogue");return v;};
    LibraryQuery q;q.tags={{"Pack","KSHMR"}};q.limit=1;snap=query(q,3);
    check(snap->assets.size()==1&&std::any_of(snap->facets.begin(),snap->facets.end(),[](const auto& f){return f.group=="Pack"&&f.label=="KSHMR"&&f.count==3;}),"Pack facets must count beyond the visible page");
    q.tags.push_back({"Type","One-shot"});query(q,2);
    q.tags={{"Type","One-shot"}};q.excluded_tags={{"Pack","KSHMR"}};snap=query(q,1);
    check(snap->assets.front().relative_path.find("Cymatics/")==0,"Pack exclusions retain other matching One-shots");
    q.tags={{"Pack","KSHMR"},{"Pack","Cymatics"},{"Type","Loop"}};q.excluded_tags.clear();query(q,2);
    q.tags={{"Pack","KSHMR"},{"Type","One-shot"}};q.limit=120;snap=query(q,2);
    const auto id=snap->assets.front().id;auto revision=snap->revision;lib.add_tag({id},"Pack","Session Pack");
    wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision;});
    q.tags={{"Pack","Session Pack"}};snap=query(q,1);
    check(std::none_of(snap->assets.front().tags.begin(),snap->assets.front().tags.end(),[](const auto& tag){return tag.group=="Pack"&&tag.label=="KSHMR"&&tag.status!="rejected";}),"Manual Pack replaces its automatic alternative immediately");
    revision=snap->revision;lib.rescan();wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision&&v.message=="Scan complete";});
    query(q,1);revision=lib.snapshot()->revision;lib.undo_tag_edit();wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision;});
    q.tags={{"Pack","KSHMR"},{"Type","One-shot"}};query(q,2);
    const auto direct=classify_sample_path("Gold Modern/Drums/Crash.wav");
    check(std::any_of(direct.tags.begin(),direct.tags.end(),[](const auto& tag){return tag.group=="Pack"&&tag.label=="Gold Modern";}),"Source pointed directly at a pack retains its name");
}

void signal_type_analysis(){
    Fixture fixture;const auto root=fixture.root/"Arbitrary",database=fixture.root/"signal.sqlite";
    const auto make=[&](const fs::path& file,int shape,int rate=8000,int channels=1,int bits=16){
        fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary|std::ios::trunc);
        const int frames=rate*4,bytes=bits/8;
        const auto put=[&](std::uint32_t value,int width){for(int i=0;i<width;++i)out.put(static_cast<char>((value>>(8*i))&255));};
        out.write("RIFF",4);put(36+frames*channels*bytes,4);out.write("WAVEfmt ",8);put(16,4);put(1,2);put(channels,2);put(rate,4);put(rate*channels*bytes,4);put(channels*bytes,2);put(bits,2);out.write("data",4);put(frames*channels*bytes,4);
        for(int i=0;i<frames;++i){
            const double t=static_cast<double>(i)/rate;
            const auto envelope=shape==0?std::exp(-t*4.):shape==1?std::exp(-std::fmod(t,.5)*25.):shape==2?.4+.3*std::cos(t*4.*3.141592653589793):shape==3?t/4.:0.;
            const auto value=.65*envelope*std::sin(t*2.*3.141592653589793*173.);
            const auto pcm=static_cast<std::int32_t>(value*(bits==24?8388607.:32767.));
            for(int c=0;c<channels;++c)put(static_cast<std::uint32_t>(pcm),bytes);
        }
        check(static_cast<bool>(out),"Write signal Type fixture");
    };
    make(root/"A.wav",0);make(root/"B.wav",1);make(root/"C.wav",2,44100,2,24);make(root/"D.wav",3);make(root/"E.wav",4);
    make(root/"Kick.wav",1);make(root/"Loop_OneShot.wav",1);
    check(analyse_wav_type(root/"A.wav").label=="One-shot","Unnamed isolated decay is a One-shot from signal evidence");
    check(analyse_wav_type(root/"B.wav").label=="Loop","Unnamed repeated attacks are a Loop from signal evidence");
    check(analyse_wav_type(root/"C.wav").label=="Loop","Legato repeating envelope works for 24-bit 44.1k stereo");
    check(analyse_wav_type(root/"D.wav").label.empty()&&analyse_wav_type(root/"E.wav").label.empty(),"Monotonic rise and silence cannot become loops from high raw-envelope correlation");
    bool cancelled=false;try{analyse_wav_type(root/"A.wav",[]{return true;});}catch(const std::exception&){cancelled=true;}
    check(cancelled,"Signal Type analysis is cancellable");
    const auto query=[&](SampleLibrary& lib,const std::string& label,std::uint64_t count){LibraryQuery q;q.tags={{"Type",label}};lib.browse(q);const auto snap=wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.query==q&&!v.analysis.running;});check(snap->visible_assets==count,"Type search indexes signal-derived tags for arbitrary names");return snap;};
    {
        SampleLibrary lib(database);lib.add_source(root);wait_for(lib,[](const Snapshot& v){return v.phase==Phase::idle&&v.visible_assets==7;});
        const auto one=query(lib,"One-shot",1);check(one->assets.front().relative_path=="A.wav"&&std::any_of(one->assets.front().tags.begin(),one->assets.front().tags.end(),[](const auto& t){return t.group=="Type"&&t.origin=="audio"&&t.confidence>.8;}),"Signal Type keeps audio provenance and confidence");
        query(lib,"Loop",3); // audio Loop overrides weak Kick word guess; explicit conflict stays unknown
        auto revision=lib.snapshot()->revision;lib.add_tag({one->assets.front().id},"Type","Loop");wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision;});query(lib,"One-shot",0);query(lib,"Loop",4);
        revision=lib.snapshot()->revision;lib.rescan();wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision&&v.message=="Scan complete";});query(lib,"Loop",4);
        revision=lib.snapshot()->revision;lib.undo_tag_edit();wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision;});query(lib,"One-shot",1);query(lib,"Loop",3);
        revision=lib.snapshot()->revision;make(root/"A.wav",1);lib.rescan();wait_for(lib,[&](const Snapshot& v){return v.phase==Phase::idle&&v.revision>revision&&v.message=="Scan complete";});query(lib,"Loop",4);query(lib,"One-shot",0);
    }
    // Upgrade a populated v4 index; backfill signal tags without a source rescan.
    sqlite3* legacy{};const auto legacyName=database.generic_u8string();check(sqlite3_open(reinterpret_cast<const char*>(legacyName.c_str()),&legacy)==SQLITE_OK,"Open v4 Type migration fixture");
    check(sqlite3_exec(legacy,"DROP TABLE audio_type_analysis; DROP TABLE audio_features; DELETE FROM asset_tags WHERE origin='audio'; PRAGMA user_version=4",nullptr,nullptr,nullptr)==SQLITE_OK,"Prepare existing v4 catalogue");sqlite3_close(legacy);
    {SampleLibrary lib(database);wait_for(lib,[](const Snapshot& v){return v.phase==Phase::idle;});query(lib,"Loop",4);}
    bool backup=false;for(const auto& entry:fs::directory_iterator(fixture.root))if(entry.path().filename().string().starts_with("signal.sqlite.pre-v6-"))backup=true;
    check(backup,"Audio analysis schema upgrade keeps a rollback backup");
    // A cached successful analysis can be used with the source offline, without re-decoding.
    fs::rename(root,fixture.root/"offline");
    {SampleLibrary lib(database);wait_for(lib,[](const Snapshot& v){return v.phase==Phase::idle;});query(lib,"Loop",4);}
    sqlite3* db{};const auto name=database.generic_u8string();check(sqlite3_open(reinterpret_cast<const char*>(name.c_str()),&db)==SQLITE_OK,"Open audio analysis cache");
    sqlite3_stmt* statement{};check(sqlite3_prepare_v2(db,"SELECT count(*) FROM audio_type_analysis WHERE version=1",-1,&statement,nullptr)==SQLITE_OK,"Prepare cache check");
    check(sqlite3_step(statement)==SQLITE_ROW&&sqlite3_column_int(statement,0)==7,"All analysed audio, including uncertain results, is cached by algorithm version");sqlite3_finalize(statement);sqlite3_close(db);
}

void audio_analysis256_tests();
int main() { try {audio_analysis256_tests();signal_type_analysis();pack_type_search();type_search_coverage();type_taxonomy();search_v4();test();tagging();migration_v1();browser_v3();unicode_browser_search();safe_preview();audition_mixes_single_device();audition_device_modes();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;} }
