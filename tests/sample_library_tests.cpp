#include <mrs/sample_library.hpp>
#include <mrs/sample_preview.hpp>
#include <mrs/audio.hpp>
#include <mrs/library_preview.hpp>
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
        if(pred(*result))return result;
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
int main() { try {test();tagging();migration_v1();browser_v3();unicode_browser_search();safe_preview();audition_mixes_single_device();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;} }
