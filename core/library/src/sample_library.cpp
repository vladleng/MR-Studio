#include <mrs/sample_library.hpp>
#include <mrs/sample_tag_rules.hpp>
#include <mrs/sample_preview.hpp>
#include <mrs/audio.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <algorithm>
#include <cmath>
#include <chrono>
#include <array>
#include <cctype>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <fstream>
#include <optional>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>

namespace mrs::library {
namespace {
namespace fs = std::filesystem;

std::string utf8(const fs::path& p) {
    const auto u = p.generic_u8string();
    return {u.begin(), u.end()};
}
fs::path from_utf8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}
fs::path normalized(const fs::path& p) {
    if (p.empty()) throw std::invalid_argument("Choose a library folder");
    std::error_code ec;
    const auto full = fs::absolute(p, ec).lexically_normal();
    if (ec || full.empty()) throw std::runtime_error("Cannot resolve library folder");
    return full;
}
void fail(sqlite3* db, const std::string& context) {
    throw std::runtime_error(context + ": " + (db ? sqlite3_errmsg(db) : "SQLite unavailable"));
}
std::string fold_search(const std::string& value) {
    std::string folded;
    folded.reserve(value.size());
    for(std::size_t i=0;i<value.size();) {
        const auto a=static_cast<unsigned char>(value[i]);
        if(a<128){
            folded.push_back(static_cast<char>(std::tolower(a)));++i;continue;
        }
        if(i+1<value.size()&&a==0xd0){
            const auto b=static_cast<unsigned char>(value[i+1]);
            if(b>=0x90&&b<=0x9f){folded.push_back(static_cast<char>(0xd0));
                folded.push_back(static_cast<char>(b+0x20));i+=2;continue;}
            if(b>=0xa0&&b<=0xaf){folded.push_back(static_cast<char>(0xd1));
                folded.push_back(static_cast<char>(b-0x20));i+=2;continue;}
            if(b==0x81){folded.push_back(static_cast<char>(0xd1));
                folded.push_back(static_cast<char>(0x91));i+=2;continue;}
        }
        folded.push_back(value[i++]);
    }
    return folded;
}
void sqlite_fold(sqlite3_context* ctx,int argc,sqlite3_value** argv) {
    if(argc!=1||sqlite3_value_type(argv[0])==SQLITE_NULL){
        sqlite3_result_null(ctx);return;
    }
    const auto* raw=sqlite3_value_text(argv[0]);
    if(!raw){sqlite3_result_null(ctx);return;}
    const auto folded=fold_search(reinterpret_cast<const char*>(raw));
    sqlite3_result_text(ctx,folded.c_str(),static_cast<int>(folded.size()),SQLITE_TRANSIENT);
}
void sqlite_leaf(sqlite3_context* ctx,int argc,sqlite3_value** argv){
    if(argc!=1||sqlite3_value_type(argv[0])==SQLITE_NULL){sqlite3_result_null(ctx);return;}
    const auto* value=sqlite3_value_text(argv[0]);if(!value){sqlite3_result_null(ctx);return;}
    const std::string path(reinterpret_cast<const char*>(value));const auto slash=path.find_last_of('/');
    const auto leaf=path.substr(slash==std::string::npos?0:slash+1);sqlite3_result_text(ctx,leaf.c_str(),static_cast<int>(leaf.size()),SQLITE_TRANSIENT);
}
void sqlite_key(sqlite3_context* ctx,int argc,sqlite3_value** argv){
    if(argc!=1||sqlite3_value_type(argv[0])==SQLITE_NULL){sqlite3_result_null(ctx);return;}
    const auto* raw=sqlite3_value_text(argv[0]);if(!raw){sqlite3_result_null(ctx);return;}
    std::string value(reinterpret_cast<const char*>(raw));
    for(const auto& [flat,sharp]:std::array<std::pair<const char*,const char*>,9>{{{"Db","C#"},{"Eb","D#"},{"Fb","E"},{"Gb","F#"},{"Ab","G#"},{"Bb","A#"},{"Cb","B"},{"E#","F"},{"B#","C"}}})
        if(value.starts_with(flat)){value.replace(0,2,sharp);break;}
    sqlite3_result_text(ctx,value.c_str(),static_cast<int>(value.size()),SQLITE_TRANSIENT);
}
void sqlite_similarity(sqlite3_context* ctx,int argc,sqlite3_value** argv){
    if(argc!=6){sqlite3_result_double(ctx,1.e9);return;}
    const auto& ref=*static_cast<AudioFeatures*>(sqlite3_user_data(ctx));
    const auto relative=[](double x,double y,double floor){return std::abs(std::log((x+floor)/(y+floor)));};
    double distance=.25*relative(sqlite3_value_double(argv[0]),ref.duration,.05)+.2*relative(sqlite3_value_double(argv[1]),ref.onset_density,.2)+.3*relative(sqlite3_value_double(argv[2]),ref.centroid,100.)+.1*std::abs(sqlite3_value_double(argv[3])-ref.flatness)+.05*relative(sqlite3_value_double(argv[4]),ref.rms,.001);
    const auto bpm=sqlite3_value_double(argv[5]);
    if(bpm>0&&ref.bpm>0){const auto octaves=std::log2(bpm/ref.bpm);distance+=.1*std::abs(octaves-std::round(octaves));}
    sqlite3_result_double(ctx,distance);
}
struct Database {
    sqlite3* db{};
    fs::path filename;
    AudioFeatures reference;
    explicit Database(const fs::path& file):filename(file) {
        if (file.empty()) throw std::invalid_argument("MR Library database path is empty");
        fs::create_directories(file.parent_path());
        const auto name = utf8(file);
        if (sqlite3_open_v2(name.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
            fail(db, "Open MR Library");
        sqlite3_busy_timeout(db, 1500);
        if(sqlite3_create_function_v2(db,"mrs_fold",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,
            nullptr,&sqlite_fold,nullptr,nullptr,nullptr)!=SQLITE_OK)fail(db,"Register Unicode search");
        if(sqlite3_create_function_v2(db,"mrs_leaf",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,
            nullptr,&sqlite_leaf,nullptr,nullptr,nullptr)!=SQLITE_OK)fail(db,"Register filename search");
        if(sqlite3_create_function_v2(db,"mrs_key",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,
            nullptr,&sqlite_key,nullptr,nullptr,nullptr)!=SQLITE_OK)fail(db,"Register key aliases");
        if(sqlite3_create_function_v2(db,"mrs_similarity",6,SQLITE_UTF8,&reference,&sqlite_similarity,nullptr,nullptr,nullptr)!=SQLITE_OK)fail(db,"Register feature similarity");
        exec("PRAGMA journal_mode=WAL");
        exec("PRAGMA foreign_keys=ON");
    }
    ~Database() { if (db) sqlite3_close(db); }
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    void exec(const char* sql) {
        if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) fail(db, "MR Library SQL");
    }
};
struct Statement {
    sqlite3* db{};
    sqlite3_stmt* stmt{};
    Statement(Database& d, const char* sql) : db(d.db) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) fail(db, "Prepare MR Library query");
    }
    ~Statement() { if (stmt) sqlite3_finalize(stmt); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void reset() { sqlite3_reset(stmt); sqlite3_clear_bindings(stmt); }
    void number(int column, std::int64_t value) {
        if (sqlite3_bind_int64(stmt, column, value) != SQLITE_OK) fail(db, "Bind integer");
    }
    void str(int column, const std::string& value) {
        if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::length_error("MR Library path too long");
        if (sqlite3_bind_text(stmt, column, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK)
            fail(db, "Bind text");
    }
    void real(int column,double value){if(sqlite3_bind_double(stmt,column,value)!=SQLITE_OK)fail(db,"Bind real");}
    bool next() {
        const auto result = sqlite3_step(stmt);
        if (result == SQLITE_ROW) return true;
        if (result != SQLITE_DONE) fail(db, "Run MR Library query");
        return false;
    }
    void done() { if (next()) throw std::runtime_error("Unexpected MR Library result"); }
};
void create_schema(Database& d) {
    Statement version(d, "PRAGMA user_version");
    const auto v = version.next() ? sqlite3_column_int(version.stmt, 0) : -1;
    if (v < 0 || v > 6) throw std::runtime_error("Unsupported MR Library database schema");
    if (v == 6) return;
    if(v>0){
        auto path=d.filename;path+=".pre-v6-"+new_id().value+".sqlite";
        sqlite3* destination{};sqlite3_backup* backup{};
        try{
            const auto name=utf8(path);
            if(sqlite3_open_v2(name.c_str(),&destination,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr)!=SQLITE_OK)
                throw std::runtime_error("Cannot create library migration backup");
            backup=sqlite3_backup_init(destination,"main",d.db,"main");
            if(!backup)throw std::runtime_error("Cannot initialize library migration backup");
            const auto copied=sqlite3_backup_step(backup,-1);const auto finished=sqlite3_backup_finish(backup);backup=nullptr;
            if(copied!=SQLITE_DONE||finished!=SQLITE_OK)throw std::runtime_error("Cannot finish library backup; close other library instances and retry");
            sqlite3_close(destination);destination=nullptr;
        }catch(...){if(backup)sqlite3_backup_finish(backup);if(destination)sqlite3_close(destination);
            std::error_code ec;fs::remove(path,ec);throw;}
    }
    d.exec("BEGIN IMMEDIATE");
    try {
        if(v<3){
        if (v < 2) {
        if (v == 0) {
        d.exec("CREATE TABLE IF NOT EXISTS sources (id INTEGER PRIMARY KEY, path TEXT NOT NULL UNIQUE, enabled INTEGER NOT NULL DEFAULT 1)");
        d.exec("CREATE TABLE IF NOT EXISTS assets ("
               "id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,"
               "relative_path TEXT NOT NULL, bytes INTEGER NOT NULL, modified INTEGER NOT NULL,"
               "rate INTEGER, channels INTEGER, bits INTEGER, frames INTEGER,"
               "status TEXT NOT NULL, error TEXT NOT NULL DEFAULT '', seen INTEGER NOT NULL DEFAULT 0,"
               "UNIQUE(source_id,relative_path))");
        d.exec("CREATE INDEX IF NOT EXISTS assets_status_idx ON assets(source_id,status)");
        }
        d.exec("ALTER TABLE assets ADD COLUMN bpm_auto INTEGER NOT NULL DEFAULT 0");
        d.exec("ALTER TABLE assets ADD COLUMN bpm_manual INTEGER NOT NULL DEFAULT 0");
        d.exec("ALTER TABLE assets ADD COLUMN key_auto TEXT NOT NULL DEFAULT ''");
        d.exec("ALTER TABLE assets ADD COLUMN key_manual TEXT NOT NULL DEFAULT ''");
        d.exec("CREATE TABLE IF NOT EXISTS tags (id INTEGER PRIMARY KEY, group_name TEXT NOT NULL, label TEXT NOT NULL, UNIQUE(group_name,label))");
        d.exec("CREATE TABLE IF NOT EXISTS asset_tags (asset_id INTEGER NOT NULL REFERENCES assets(id) ON DELETE CASCADE,"
               " tag_id INTEGER NOT NULL REFERENCES tags(id), origin TEXT NOT NULL, status TEXT NOT NULL,"
               " confidence REAL NOT NULL, PRIMARY KEY(asset_id,tag_id))");
        d.exec("CREATE INDEX IF NOT EXISTS asset_tags_tag_idx ON asset_tags(tag_id,status)");
        d.exec("CREATE TABLE IF NOT EXISTS classification_rules (id INTEGER PRIMARY KEY,"
               " group_name TEXT NOT NULL, phrase TEXT NOT NULL, label TEXT NOT NULL,"
               " in_folder INTEGER NOT NULL DEFAULT 0, UNIQUE(group_name,phrase,in_folder))");
        d.exec("CREATE TABLE IF NOT EXISTS edit_batches (id INTEGER PRIMARY KEY)");
        d.exec("CREATE TABLE IF NOT EXISTS tag_edit_history (batch_id INTEGER NOT NULL REFERENCES edit_batches(id) ON DELETE CASCADE,"
               " asset_id INTEGER NOT NULL,tag_id INTEGER NOT NULL,existed INTEGER NOT NULL,"
               " prior_origin TEXT,prior_status TEXT,prior_confidence REAL)");
        d.exec("CREATE TABLE IF NOT EXISTS metadata_edit_history (batch_id INTEGER NOT NULL REFERENCES edit_batches(id) ON DELETE CASCADE,"
               " asset_id INTEGER NOT NULL,prior_bpm INTEGER,prior_key TEXT)");
        }
        // V3 persists only metadata; source WAVs and artwork are never rewritten.
        d.exec("CREATE TABLE IF NOT EXISTS folders (source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,"
               " path TEXT NOT NULL, seen INTEGER NOT NULL DEFAULT 1, PRIMARY KEY(source_id,path))");
        d.exec("CREATE TABLE IF NOT EXISTS folder_artwork (source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,"
               " folder TEXT NOT NULL, auto_image TEXT NOT NULL DEFAULT '',manual_image TEXT NOT NULL DEFAULT '',"
               " priority INTEGER NOT NULL DEFAULT 0, seen INTEGER NOT NULL DEFAULT 1,PRIMARY KEY(source_id,folder))");
        d.exec("CREATE TABLE IF NOT EXISTS collections (id INTEGER PRIMARY KEY,name TEXT NOT NULL UNIQUE COLLATE NOCASE)");
        d.exec("CREATE TABLE IF NOT EXISTS collection_items (collection_id INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE,"
               " asset_id INTEGER NOT NULL REFERENCES assets(id) ON DELETE CASCADE,"
               " PRIMARY KEY(collection_id,asset_id))");
        d.exec("CREATE INDEX IF NOT EXISTS collection_items_asset_idx ON collection_items(asset_id)");
        d.exec("CREATE TABLE IF NOT EXISTS favorites (asset_id INTEGER PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE)");
        d.exec("CREATE INDEX IF NOT EXISTS folders_source_path_idx ON folders(source_id,path)");
        // v1/v2 have indexed WAV paths but no persistent folder rows.
        // Reconstruct the directory tree from DB paths without rescanning WAVs.
        if(v==1||v==2){
            d.exec("INSERT OR IGNORE INTO folders(source_id,path,seen) SELECT id,'',1 FROM sources");
            Statement indexed(d,"SELECT source_id,relative_path FROM assets");
            Statement insertFolder(d,"INSERT OR IGNORE INTO folders(source_id,path,seen) VALUES(?,?,1)");
            while(indexed.next()){
                const auto id=sqlite3_column_int64(indexed.stmt,0);
                const auto* raw=sqlite3_column_text(indexed.stmt,1);
                const std::string filename=raw?reinterpret_cast<const char*>(raw):"";
                for(auto pos=filename.find('/');pos!=std::string::npos;pos=filename.find('/',pos+1)){
                    insertFolder.reset();insertFolder.number(1,id);
                    insertFolder.str(2,filename.substr(0,pos));insertFolder.done();
                }
            }
        }
        }
        d.exec("CREATE TABLE IF NOT EXISTS managed_covers (kind TEXT NOT NULL,owner INTEGER NOT NULL,key TEXT NOT NULL,image TEXT NOT NULL,PRIMARY KEY(kind,owner,key))");
        d.exec("CREATE TABLE IF NOT EXISTS audio_type_analysis (asset_id INTEGER PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,"
               " bytes INTEGER NOT NULL,modified INTEGER NOT NULL,root TEXT NOT NULL,version INTEGER NOT NULL,"
               " label TEXT NOT NULL,confidence REAL NOT NULL,reason TEXT NOT NULL)");
        d.exec("CREATE TABLE IF NOT EXISTS audio_features (asset_id INTEGER PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,bytes INTEGER NOT NULL,modified INTEGER NOT NULL,root TEXT NOT NULL,version INTEGER NOT NULL,state TEXT NOT NULL,reason TEXT NOT NULL,duration REAL NOT NULL,rms REAL NOT NULL,peak REAL NOT NULL,onsets REAL NOT NULL,centroid REAL NOT NULL,flatness REAL NOT NULL,bpm REAL NOT NULL,bpm_confidence REAL NOT NULL,key_label TEXT NOT NULL,key_confidence REAL NOT NULL)");
        d.exec("CREATE TABLE IF NOT EXISTS saved_searches (id INTEGER PRIMARY KEY,name TEXT NOT NULL UNIQUE,source INTEGER,collection INTEGER,folder TEXT,search TEXT,sort TEXT,key_label TEXT,bpm_min INTEGER,bpm_max INTEGER,favorites INTEGER,similar INTEGER,seed INTEGER)");
        d.exec("CREATE TABLE IF NOT EXISTS saved_search_tags (search_id INTEGER REFERENCES saved_searches(id) ON DELETE CASCADE,excluded INTEGER,group_name TEXT,label TEXT)");
        d.exec("PRAGMA user_version=6");
        d.exec("COMMIT");
    } catch (...) { d.exec("ROLLBACK"); throw; }
}
// Taxonomy correction is idempotent data normalization, not a schema change.
// Retain tag IDs/history when possible; merge a duplicate into its existing Instrument tag.
void normalize_types(Database& d){
    Statement list(d,"SELECT id,label FROM tags WHERE group_name='Type' AND label NOT IN ('Loop','One-shot')");
    std::vector<std::pair<std::int64_t,std::string>> old;
    while(list.next())old.emplace_back(sqlite3_column_int64(list.stmt,0),reinterpret_cast<const char*>(sqlite3_column_text(list.stmt,1)));
    for(const auto& [id,label]:old){
        Statement find(d,"SELECT id FROM tags WHERE group_name='Instrument' AND label=?");find.str(1,label);
        if(find.next()){
            const auto target=sqlite3_column_int64(find.stmt,0);find.reset();
            Statement copy(d,"INSERT INTO asset_tags SELECT asset_id,?,origin,status,confidence FROM asset_tags WHERE tag_id=? ON CONFLICT(asset_id,tag_id) DO UPDATE SET origin=excluded.origin,status=excluded.status,confidence=excluded.confidence WHERE asset_tags.status='proposed' AND excluded.status!='proposed'");copy.number(1,target);copy.number(2,id);copy.done();
            Statement history(d,"UPDATE tag_edit_history SET tag_id=? WHERE tag_id=?");history.number(1,target);history.number(2,id);history.done();
            Statement clear(d,"DELETE FROM asset_tags WHERE tag_id=?");clear.number(1,id);clear.done();
            Statement erase(d,"DELETE FROM tags WHERE id=?");erase.number(1,id);erase.done();
        }else{
            find.reset();Statement move(d,"UPDATE tags SET group_name='Instrument' WHERE id=?");move.number(1,id);move.done();
        }
        Statement cover(d,"UPDATE OR IGNORE managed_covers SET key=? WHERE kind='tag' AND key=?");
        cover.str(1,"Instrument|"+label);cover.str(2,"Type|"+label);cover.done();
    }
    d.exec("UPDATE OR IGNORE classification_rules SET group_name='Instrument' WHERE group_name='Type' AND label NOT IN ('Loop','One-shot')");
}
std::vector<TagRule> read_rules(Database& d) {
    std::vector<TagRule> rules;
    Statement stmt(d,"SELECT group_name,phrase,label,in_folder FROM classification_rules ORDER BY id");
    while(stmt.next()){
        const auto str=[&](int n){const auto* v=sqlite3_column_text(stmt.stmt,n);return v?std::string(reinterpret_cast<const char*>(v)):std::string{};};
        rules.push_back({str(0),str(1),str(2),sqlite3_column_int(stmt.stmt,3)!=0});
    }
    return rules;
}
struct Cancelled : std::runtime_error { Cancelled() : std::runtime_error("Scan cancelled") {} };
void classify_assets(Database& d,const std::optional<std::int64_t> source = {},const std::function<bool()>& cancelled = {},std::int64_t onlyAsset=0) {
    const auto rules=read_rules(d);
    struct Asset {std::int64_t id,sourceId,bytes,modified;std::string path,relative,root;bool enabled;};
    std::vector<Asset> items;
    const std::string select="SELECT a.id,a.relative_path,s.path,a.source_id,a.bytes,a.modified,s.enabled FROM assets a JOIN sources s ON s.id=a.source_id WHERE a.status='ok'";
    const auto sql=select+(onlyAsset>0?" AND a.id=?":" AND (?=0 OR a.source_id=?) ORDER BY a.id");
    Statement list(d,sql.c_str());
    if(onlyAsset>0)list.number(1,onlyAsset);else{list.number(1,source.value_or(0));list.number(2,source.value_or(0));}
    while(list.next()){
        const auto* name=sqlite3_column_text(list.stmt,1);
        const auto* root=sqlite3_column_text(list.stmt,2);
        const auto prefix=root?source_folder_name(from_utf8(reinterpret_cast<const char*>(root))):std::string{};
        const std::string relative=name?reinterpret_cast<const char*>(name):"",rootPath=root?reinterpret_cast<const char*>(root):"";
        items.push_back({sqlite3_column_int64(list.stmt,0),sqlite3_column_int64(list.stmt,3),sqlite3_column_int64(list.stmt,4),sqlite3_column_int64(list.stmt,5),prefix+"/"+relative,relative,rootPath,sqlite3_column_int(list.stmt,6)!=0});
    }
    Statement clear(d,"DELETE FROM asset_tags WHERE asset_id=? AND status='proposed'");
    Statement make(d,"INSERT OR IGNORE INTO tags(group_name,label) VALUES(?,?)");
    Statement find(d,"SELECT id FROM tags WHERE group_name=? AND label=?");
    Statement put(d,"INSERT OR IGNORE INTO asset_tags(asset_id,tag_id,origin,status,confidence) VALUES(?,?,?,'proposed',?)");
    Statement typed(d,"UPDATE assets SET bpm_auto=?,key_auto=? WHERE id=?");
    Statement manualType(d,"SELECT 1 FROM asset_tags at JOIN tags t ON t.id=at.tag_id WHERE at.asset_id=? AND t.group_name='Type' AND at.status='confirmed' LIMIT 1");
    Statement manualPack(d,"SELECT 1 FROM asset_tags at JOIN tags t ON t.id=at.tag_id WHERE at.asset_id=? AND t.group_name='Pack' AND at.status='confirmed' LIMIT 1");
    Statement cachedAudio(d,"SELECT label,confidence,reason FROM audio_type_analysis WHERE asset_id=? AND bytes=? AND modified=? AND root=? AND version=1");
    for(const auto& item:items){
        if(cancelled&&cancelled())throw Cancelled();
        auto suggestion=classify_sample_path(item.path,rules);
        clear.reset();clear.number(1,item.id);clear.done();
        manualType.reset();manualType.number(1,item.id);const bool hasManualType=manualType.next();manualType.reset();
        manualPack.reset();manualPack.number(1,item.id);const bool hasManualPack=manualPack.next();manualPack.reset();
        const bool explicitType=std::any_of(suggestion.tags.begin(),suggestion.tags.end(),[](const auto& tag){return tag.group=="Type"&&tag.confidence>=.85;});
        // Signal analysis supplies Type for arbitrary filenames. Explicit metadata
        // and manual decisions remain authoritative; weak word guesses do not.
        if(!hasManualType&&!explicitType&&!suggestion.type_conflict){
            AudioTypeAnalysis audio;
            cachedAudio.reset();cachedAudio.number(1,item.id);cachedAudio.number(2,item.bytes);cachedAudio.number(3,item.modified);cachedAudio.str(4,item.root);
            const bool cached=cachedAudio.next();
            if(cached){
                const auto* label=sqlite3_column_text(cachedAudio.stmt,0);const auto* reason=sqlite3_column_text(cachedAudio.stmt,2);
                audio={label?reinterpret_cast<const char*>(label):"",reason?reinterpret_cast<const char*>(reason):"",sqlite3_column_double(cachedAudio.stmt,1)};
            }
            cachedAudio.reset();
            if(!audio.label.empty()){
                std::erase_if(suggestion.tags,[](const auto& tag){return tag.group=="Type";});
                suggestion.tags.push_back({"Type",audio.label,"audio",audio.confidence});
            }
        }
        for(const auto& tag:suggestion.tags){
            if(tag.group=="Type"&&hasManualType)continue;
            if(tag.group=="Pack"&&hasManualPack)continue;
            make.reset();make.str(1,tag.group);make.str(2,tag.label);make.done();
            find.reset();find.str(1,tag.group);find.str(2,tag.label);
            if(!find.next())throw std::runtime_error("MR Library tag unavailable");
            const auto tag_id=sqlite3_column_int64(find.stmt,0);find.reset();
            put.reset();put.number(1,item.id);put.number(2,tag_id);put.str(3,tag.origin);
            if(sqlite3_bind_double(put.stmt,4,tag.confidence)!=SQLITE_OK)fail(d.db,"Tag confidence");
            put.done();
        }
        Statement musical(d,"SELECT bpm,key_label FROM audio_features WHERE asset_id=? AND bytes=? AND modified=? AND root=? AND version=2 AND state='complete'");
        musical.number(1,item.id);musical.number(2,item.bytes);musical.number(3,item.modified);musical.str(4,item.root);
        if(musical.next()){
            if(!suggestion.metadata.bpm){const auto value=sqlite3_column_double(musical.stmt,0);if(value>0)suggestion.metadata.bpm=static_cast<int>(std::lround(value));}
            if(!suggestion.metadata.key){const auto* key=sqlite3_column_text(musical.stmt,1);if(key&&*key)suggestion.metadata.key=reinterpret_cast<const char*>(key);}
        }
        typed.reset();typed.number(1,suggestion.metadata.bpm.value_or(0));
        typed.str(2,suggestion.metadata.key.value_or(""));typed.number(3,item.id);typed.done();
    }
}
bool allowed_group(const std::string& value) {
    for(const auto* name:{"Instrument","Genre","Pack","Type","Technique","Rhythm","Character","Custom"})
        if(value==name)return true;
    return false;
}
void valid_tag(const std::string& group,const std::string& label){
    if(!allowed_group(group)||label.empty()||label.size()>(group=="Pack"?512u:96u)||(group=="Type"&&label!="Loop"&&label!="One-shot"))throw std::invalid_argument("Invalid tag name/group");
}
bool is_reparse(const fs::path& file) {
#ifdef _WIN32
    const auto attrs = GetFileAttributesW(file.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code ec;
    return fs::is_symlink(file, ec);
#endif
}
bool is_wav(const fs::path& path) {
    auto extension = utf8(path.extension());
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".wav";
}
} // namespace

struct SampleLibrary::Impl {
    enum class Kind { add, relink, remove, enable, rescan, view, tag, tag_status, bpm, key, rule, undo, browse, create_collection, delete_collection, collection_items, favorites, artwork, cover, analyse, analysis_result, analysis_control, save_search, delete_search };
    struct AnalysisJob {
        std::uint64_t generation{};
        std::int64_t id{},source{},bytes{},modified{};
        std::string root,relative;
        AudioFeatures result;
    };
    struct Command {
        Kind kind;std::int64_t id{};fs::path root;bool enabled{true};
        std::vector<std::int64_t> ids;
        std::string group,label,status,phrase;
        int number{};bool folder{};
        LibraryQuery query;
        std::shared_ptr<AnalysisJob> analysis;
        Command():kind(Kind::view){}
        Command(Kind k,std::int64_t n=0,fs::path p={},bool e=true)
            :kind(k),id(n),root(std::move(p)),enabled(e){}
    };
    fs::path database;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<Command> commands;
    std::shared_ptr<const Snapshot> state{std::make_shared<Snapshot>()};
    std::uint64_t revision{};
    LibraryQuery browserQuery;
    std::atomic<bool> cancel{false}, scanning{false};
    std::mutex analysisMutex;
    std::condition_variable_any analysisWake;
    std::deque<std::shared_ptr<AnalysisJob>> analysisQueue;
    std::atomic<std::uint64_t> analysisGeneration{0};
    std::atomic<bool> analysisPaused{false};
    AnalysisProgress analysisProgress;
    std::uint64_t activeAnalysisGeneration{};
    std::int64_t analysisCursor{};
    bool analysisForce{};
    std::chrono::steady_clock::time_point lastAnalysisPublish{};
    std::jthread analysisWorker,worker;

    explicit Impl(fs::path path) : database(std::move(path)) {
        analysisWorker=std::jthread([this](std::stop_token st){run_analysis(st);});
        worker=std::jthread([this](std::stop_token st){run(st);});
    }
    ~Impl() {
        cancel = true;
        analysisWorker.request_stop();analysisWake.notify_all();
        worker.request_stop();
        wake.notify_all();
        analysisWorker.join();
        worker.join();
    }
    void run_analysis(std::stop_token stop){
        auto nextYield=std::chrono::steady_clock::now()+std::chrono::milliseconds(8);
        while(!stop.stop_requested()){
            std::shared_ptr<AnalysisJob> job;
            {std::unique_lock lock(analysisMutex);analysisWake.wait(lock,stop,[&]{return !analysisQueue.empty()&&!analysisPaused.load();});
                if(stop.stop_requested())return;
                job=std::move(analysisQueue.front());analysisQueue.pop_front();}
            const auto interrupted=[&]{
                if(analysisPaused.load()){std::unique_lock lock(analysisMutex);analysisWake.wait(lock,stop,[&]{return !analysisPaused.load()||job->generation!=analysisGeneration.load();});}
                // Cancellation is polled in both decoding and DSP inner loops.
                // Sleeping on every poll multiplies Windows timer rounding by
                // hundreds of autocorrelation lags. Throttle by elapsed work.
                if(!stop.stop_requested()&&job->generation==analysisGeneration.load()&&std::chrono::steady_clock::now()>=nextYield){
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    nextYield=std::chrono::steady_clock::now()+std::chrono::milliseconds(8);
                }
                return stop.stop_requested()||job->generation!=analysisGeneration.load();
            };
            if(job->generation!=analysisGeneration.load())continue;
            try{
                Snapshot catalogue;catalogue.sources.push_back({job->source,from_utf8(job->root),true,0,0,0});AudioAsset a;a.id=job->id;a.source_id=job->source;a.relative_path=job->relative;a.status="ok";catalogue.assets.push_back(std::move(a));
                const auto path=asset_path(catalogue,job->id);
                if(!path)throw std::runtime_error("Indexed WAV unavailable or unsafe");
                const auto verify=[&]{
                    std::error_code ec;const auto size=fs::file_size(*path,ec);
                    if(ec||size!=static_cast<std::uintmax_t>(job->bytes))throw std::runtime_error("WAV changed during analysis; rescan required");
                    const auto stamp=fs::last_write_time(*path,ec);
                    if(ec||stamp.time_since_epoch().count()!=job->modified)throw std::runtime_error("WAV changed during analysis; rescan required");
                };
                verify();
                job->result=analyse_wav_features(*path,interrupted);
                verify();
            }catch(const std::exception& e){job->result={};job->result.state="error";job->result.reason=e.what();}
            if(stop.stop_requested())return;
            if(job->generation==analysisGeneration.load()){Command command(Kind::analysis_result);command.analysis=std::move(job);enqueue(std::move(command));}
        }
    }
    void refill_analysis(Database& d){
        if(activeAnalysisGeneration!=analysisGeneration.load())return;
        std::size_t capacity{};{std::lock_guard lock(analysisMutex);capacity=64-analysisQueue.size();}
        if(capacity<32)return;
        std::deque<std::shared_ptr<AnalysisJob>> pending;
        Statement list(d,"SELECT a.id,a.source_id,a.bytes,a.modified,s.path,a.relative_path FROM assets a JOIN sources s ON s.id=a.source_id LEFT JOIN audio_features f ON f.asset_id=a.id WHERE a.status='ok' AND s.enabled=1 AND (?=1 OR f.asset_id IS NULL OR f.bytes<>a.bytes OR f.modified<>a.modified OR f.root<>s.path OR f.version<>2) AND a.id>? ORDER BY a.id LIMIT ?");
        list.number(1,analysisForce?1:0);list.number(2,analysisCursor);list.number(3,static_cast<std::int64_t>(capacity));
        while(list.next()){
            auto job=std::make_shared<AnalysisJob>();job->generation=activeAnalysisGeneration;job->id=sqlite3_column_int64(list.stmt,0);job->source=sqlite3_column_int64(list.stmt,1);job->bytes=sqlite3_column_int64(list.stmt,2);job->modified=sqlite3_column_int64(list.stmt,3);analysisCursor=job->id;
            job->root=reinterpret_cast<const char*>(sqlite3_column_text(list.stmt,4));job->relative=reinterpret_cast<const char*>(sqlite3_column_text(list.stmt,5));pending.push_back(std::move(job));
        }
        if(activeAnalysisGeneration!=analysisGeneration.load())return;
        {std::lock_guard lock(analysisMutex);for(auto& job:pending)analysisQueue.push_back(std::move(job));}analysisWake.notify_all();
    }
    void queue_analysis(Database& d,bool force=false){
        activeAnalysisGeneration=analysisGeneration.fetch_add(1)+1;analysisCursor=0;analysisForce=force;analysisPaused=false;
        {std::lock_guard lock(analysisMutex);analysisQueue.clear();}
        Statement count(d,"SELECT COUNT(*) FROM assets a JOIN sources s ON s.id=a.source_id LEFT JOIN audio_features f ON f.asset_id=a.id WHERE a.status='ok' AND s.enabled=1 AND (?=1 OR f.asset_id IS NULL OR f.bytes<>a.bytes OR f.modified<>a.modified OR f.root<>s.path OR f.version<>2)");count.number(1,force?1:0);
        const auto total=count.next()?static_cast<std::uint64_t>(sqlite3_column_int64(count.stmt,0)):0;
        analysisProgress={0,total,total>0,false};refill_analysis(d);
    }
    void enqueue(Command c) {
        const auto kind=c.kind;
        {
            std::lock_guard lock(mutex);
            if(kind==Kind::view||kind==Kind::browse){
                // Typing quickly must not queue hundreds of stale queries.
                std::erase_if(commands,[](const Command& queued){
                    return queued.kind==Kind::view||queued.kind==Kind::browse;
                });
            }
            commands.push_back(std::move(c));
        }
        // Filtering never cancels a user's explicit source scan.
        if(scanning.load()&&(kind==Kind::add||kind==Kind::relink||
            kind==Kind::remove||kind==Kind::enable||kind==Kind::rescan))cancel=true;
        wake.notify_one();
    }
    std::shared_ptr<const Snapshot> snapshot() const {
        std::lock_guard lock(mutex);
        return state;
    }
    void publish(Database& d, Phase phase, std::string message = {}, std::uint64_t processed = 0, std::uint64_t changed = 0) {
        auto next = std::make_shared<Snapshot>();
        next->analysis=analysisProgress;next->analysis.paused=analysisPaused.load();
        next->phase = phase;
        next->message = std::move(message);
        next->processed = processed;
        next->changed = changed;
        Statement entries(d,
            "SELECT s.id,s.path,s.enabled,"
            "COALESCE(SUM(CASE WHEN a.status='ok' THEN 1 ELSE 0 END),0),"
            "COALESCE(SUM(CASE WHEN a.status='missing' THEN 1 ELSE 0 END),0),"
            "COALESCE(SUM(CASE WHEN a.status='invalid' THEN 1 ELSE 0 END),0)"
            " FROM sources s LEFT JOIN assets a ON a.source_id=s.id"
            " GROUP BY s.id ORDER BY s.id");
        while (entries.next()) {
            Source source;
            source.id = sqlite3_column_int64(entries.stmt, 0);
            const auto* path = sqlite3_column_text(entries.stmt, 1);
            source.root = from_utf8(path ? reinterpret_cast<const char*>(path) : "");
            source.enabled = sqlite3_column_int(entries.stmt, 2) != 0;
            source.available = static_cast<std::uint64_t>(sqlite3_column_int64(entries.stmt, 3));
            source.missing = static_cast<std::uint64_t>(sqlite3_column_int64(entries.stmt, 4));
            source.invalid = static_cast<std::uint64_t>(sqlite3_column_int64(entries.stmt, 5));
            next->sources.push_back(std::move(source));
        }
        // All data lookup, pagination and filter evaluation run on this worker.
        // A bounded page is copied to JUCE as an immutable snapshot.
        next->artwork_root=fs::absolute(database).parent_path()/"artwork";
        Statement managed(d,"SELECT kind,owner,key,image FROM managed_covers");
        while(managed.next()){
            const auto str=[&](int i){const auto* x=sqlite3_column_text(managed.stmt,i);return x?std::string(reinterpret_cast<const char*>(x)):std::string{};};
            next->covers.push_back({str(0),sqlite3_column_int64(managed.stmt,1),str(2),str(3)});
        }
        const auto& q=browserQuery;
        next->query=q;
        const std::string featureJoin=q.similar_asset_id>0?" JOIN audio_features sf ON sf.asset_id=a.id AND sf.bytes=a.bytes AND sf.modified=a.modified AND sf.root=(SELECT path FROM sources WHERE id=a.source_id) AND sf.version=2 AND sf.state='complete'":"";
        bool referenceReady=false;
        if(q.similar_asset_id>0){
            Statement ref(d,"SELECT f.duration,f.onsets,f.centroid,f.flatness,f.rms,CASE WHEN a.bpm_manual=-1 THEN 0 WHEN a.bpm_manual>0 THEN a.bpm_manual ELSE a.bpm_auto END FROM audio_features f JOIN assets a ON a.id=f.asset_id JOIN sources s ON s.id=a.source_id WHERE a.id=? AND f.bytes=a.bytes AND f.modified=a.modified AND f.root=s.path AND f.version=2 AND f.state='complete'");ref.number(1,q.similar_asset_id);
            if(ref.next()){referenceReady=true;d.reference.duration=sqlite3_column_double(ref.stmt,0);d.reference.onset_density=sqlite3_column_double(ref.stmt,1);d.reference.centroid=sqlite3_column_double(ref.stmt,2);d.reference.flatness=sqlite3_column_double(ref.stmt,3);d.reference.rms=sqlite3_column_double(ref.stmt,4);d.reference.bpm=sqlite3_column_double(ref.stmt,5);next->similar_reference=d.reference;next->similar_reference.state="complete";}
        }
        Statement saved(d,"SELECT id,name,source,collection,folder,search,sort,key_label,bpm_min,bpm_max,favorites,similar,seed FROM saved_searches ORDER BY name");
        Statement savedTags(d,"SELECT excluded,group_name,label FROM saved_search_tags WHERE search_id=? ORDER BY rowid");
        while(saved.next()){
            const auto value=[&](int i){const auto* raw=sqlite3_column_text(saved.stmt,i);return raw?std::string(reinterpret_cast<const char*>(raw)):std::string{};};
            SavedSearch search;search.id=sqlite3_column_int64(saved.stmt,0);search.name=value(1);auto& filter=search.query;
            filter.source_id=sqlite3_column_int64(saved.stmt,2);filter.collection_id=sqlite3_column_int64(saved.stmt,3);filter.folder=value(4);filter.search=value(5);filter.sort=value(6);filter.key=value(7);filter.bpm_min=sqlite3_column_int(saved.stmt,8);filter.bpm_max=sqlite3_column_int(saved.stmt,9);filter.favorites_only=sqlite3_column_int(saved.stmt,10)!=0;filter.similar_asset_id=sqlite3_column_int64(saved.stmt,11);filter.shuffle_seed=static_cast<std::uint64_t>(sqlite3_column_int64(saved.stmt,12));
            savedTags.reset();savedTags.number(1,search.id);
            while(savedTags.next()){auto& tags=sqlite3_column_int(savedTags.stmt,0)?filter.excluded_tags:filter.tags;tags.push_back({reinterpret_cast<const char*>(sqlite3_column_text(savedTags.stmt,1)),reinterpret_cast<const char*>(sqlite3_column_text(savedTags.stmt,2))});}
            next->saved_searches.push_back(std::move(search));
        }
        Statement available(d,"SELECT t.group_name,t.label,COUNT(*) FROM asset_tags ft JOIN tags t ON t.id=ft.tag_id"
            " JOIN assets a ON a.id=ft.asset_id WHERE ft.status<>'rejected' AND (?=0 OR a.source_id=?)"
            " GROUP BY t.group_name,t.label ORDER BY t.group_name,t.label");
        available.number(1,q.source_id);available.number(2,q.source_id);
        while(available.next()){
            const auto str=[&](int i){const auto* x=sqlite3_column_text(available.stmt,i);return x?std::string(reinterpret_cast<const char*>(x)):std::string{};};
            next->all_facets.push_back({str(0),str(1),static_cast<std::uint64_t>(sqlite3_column_int64(available.stmt,2))});
        }
        Statement collections(d,"SELECT c.id,c.name,COUNT(ci.asset_id) FROM collections c"
            " LEFT JOIN collection_items ci ON ci.collection_id=c.id GROUP BY c.id ORDER BY c.name");
        while(collections.next()){
            const auto* name=sqlite3_column_text(collections.stmt,1);
            next->collections.push_back({sqlite3_column_int64(collections.stmt,0),
                name?reinterpret_cast<const char*>(name):"",
                static_cast<std::uint64_t>(sqlite3_column_int64(collections.stmt,2))});
        }
        std::map<std::pair<std::int64_t,std::string>,std::string> covers;
        Statement art(d,"SELECT source_id,folder,auto_image,manual_image FROM folder_artwork");
        while(art.next()){
            const auto str=[&](int i){const auto* x=sqlite3_column_text(art.stmt,i);
                return x?std::string(reinterpret_cast<const char*>(x)):std::string{};};
            const auto manual=str(3),autoImage=str(2);
            covers[{sqlite3_column_int64(art.stmt,0),str(1)}]=
                manual.empty()?autoImage:manual;
        }
        const auto artwork=[&](std::int64_t source,std::string folder){
            for(;;){
                auto found=covers.find({source,folder});
                if(found!=covers.end()&&!found->second.empty())return found->second;
                if(folder.empty())break;
                const auto slash=folder.find_last_of('/');
                folder=slash==std::string::npos?"":folder.substr(0,slash);
            }
            return std::string{};
        };
        Statement folders(d,"SELECT source_id,path FROM folders WHERE (?=0 OR source_id=?)"
            " ORDER BY source_id,path");
        folders.number(1,q.source_id);folders.number(2,q.source_id);
        while(folders.next()){
            const auto* path=sqlite3_column_text(folders.stmt,1);
            const auto id=sqlite3_column_int64(folders.stmt,0);
            const std::string relative=path?reinterpret_cast<const char*>(path):"";
            next->folders.push_back({id,relative,artwork(id,relative)});
        }
        // Bind parameters in the same order to COUNT and PAGE queries.
        std::string where=" WHERE (?=0 OR a.source_id=?)";
        std::vector<std::string> params;
        if(q.similar_asset_id>0){where+=" AND a.id<>? AND a.status='ok' AND EXISTS(SELECT 1 FROM sources enabled_source WHERE enabled_source.id=a.source_id AND enabled_source.enabled=1)";params.push_back(std::to_string(q.similar_asset_id));if(!referenceReady)where+=" AND 0";}
        if(!q.folder.empty()){
            where+=" AND (substr(a.relative_path,1,length(?))=?)";
            const auto prefix=q.folder+"/";
            params.push_back(prefix);params.push_back(prefix);
        }
        if(q.favorites_only)where+=" AND EXISTS (SELECT 1 FROM favorites fv WHERE fv.asset_id=a.id)";
        if(q.collection_id>0){where+=" AND EXISTS (SELECT 1 FROM collection_items ci"
            " WHERE ci.asset_id=a.id AND ci.collection_id=?)";
            params.push_back(std::to_string(q.collection_id));}
        std::vector<std::string> terms;
        std::istringstream words(q.search.substr(0,512));std::string term;
        while(words>>term&&terms.size()<16)terms.push_back(term.substr(0,96));
        for(const auto& word:terms){
            where+=" AND (instr(mrs_fold(a.relative_path),mrs_fold(?))>0 OR EXISTS (SELECT 1 FROM asset_tags st"
                " JOIN tags t ON t.id=st.tag_id WHERE st.asset_id=a.id AND st.status<>'rejected'"
                " AND instr(mrs_fold(t.label),mrs_fold(?))>0)"
                " OR EXISTS (SELECT 1 FROM collection_items ci JOIN collections c ON c.id=ci.collection_id"
                " WHERE ci.asset_id=a.id AND instr(mrs_fold(c.name),mrs_fold(?))>0))";
            for(int i=0;i<3;++i)params.push_back(word);
        }
        const auto bpm="(CASE WHEN a.bpm_manual=-1 THEN 0"
            " WHEN a.bpm_manual>0 THEN a.bpm_manual ELSE a.bpm_auto END)";
        const auto key="(CASE WHEN a.key_manual='-' THEN ''"
            " WHEN a.key_manual<>'' THEN a.key_manual ELSE a.key_auto END)";
        if(q.bpm_min>0){where+=std::string(" AND ")+bpm+">=CAST(? AS REAL)";params.push_back(std::to_string(q.bpm_min));}
        if(q.bpm_max>0){where+=std::string(" AND ")+bpm+"<=CAST(? AS REAL)";params.push_back(std::to_string(q.bpm_max));}
        if(!q.key.empty()){
            where+=q.key=="?"?std::string(" AND ")+key+"=''":std::string(" AND mrs_key(")+key+")=mrs_key(?)";
            if(q.key!="?")params.push_back(q.key);
        }
        std::map<std::string,std::set<std::string>> groups;
        for(const auto& filter:q.tags)
            if(allowed_group(filter.group)&&!filter.label.empty()&&filter.label.size()<=(filter.group=="Pack"?512u:96u))
                groups[filter.group].insert(filter.label);
        for(const auto& [group,values]:groups){
            where+=" AND EXISTS (SELECT 1 FROM asset_tags at JOIN tags t ON t.id=at.tag_id"
                " WHERE at.asset_id=a.id AND at.status<>'rejected' AND t.group_name=? AND t.label IN (";
            params.push_back(group);
            bool first=true;
            for(const auto& value:values){
                if(!first)where+=",";
                first=false;where+="?";params.push_back(value);
            }
            where+="))";
        }
        for(const auto& filter:q.excluded_tags){
            if(!allowed_group(filter.group)||filter.label.empty()||filter.label.size()>(filter.group=="Pack"?512u:96u))continue;
            where+=" AND NOT EXISTS (SELECT 1 FROM asset_tags xt JOIN tags t ON t.id=xt.tag_id WHERE xt.asset_id=a.id"
                " AND xt.status<>'rejected' AND t.group_name=? AND t.label=?)";
            params.push_back(filter.group);params.push_back(filter.label);
        }
        const auto bind=[&](Statement& st){
            st.number(1,q.source_id);st.number(2,q.source_id);
            int n=3;for(const auto& value:params)st.str(n++,value);
            return n;
        };
        // Facets describe all matching assets, not just the visible page.
        Statement facets(d,("SELECT t.group_name,t.label,COUNT(*) FROM asset_tags ft JOIN tags t ON t.id=ft.tag_id"
            " JOIN assets a ON a.id=ft.asset_id"+featureJoin+where+" AND ft.status<>'rejected'"
            " GROUP BY t.group_name,t.label ORDER BY t.group_name,t.label").c_str());
        bind(facets);
        while(facets.next()){
            const auto str=[&](int i){const auto* x=sqlite3_column_text(facets.stmt,i);return x?std::string(reinterpret_cast<const char*>(x)):std::string{};};
            next->facets.push_back({str(0),str(1),static_cast<std::uint64_t>(sqlite3_column_int64(facets.stmt,2))});
        }
        Statement count(d,("SELECT COUNT(*) FROM assets a"+featureJoin+where).c_str());
        bind(count);
        if(count.next())next->visible_assets=static_cast<std::uint64_t>(sqlite3_column_int64(count.stmt,0));
        std::string sort="mrs_fold(mrs_leaf(a.relative_path)),a.relative_path,a.id";
        if(q.sort=="bpm")sort=std::string(bpm)+" DESC,a.relative_path";
        else if(q.sort=="duration")sort="(1.0*a.frames/NULLIF(a.rate,0)) DESC,a.relative_path";
        else if(q.sort=="updated")sort="a.modified DESC,a.relative_path";
        else if(q.sort=="added")sort="a.id DESC";
        else if(q.sort=="random")sort="((a.id*1103515245+"+std::to_string(q.shuffle_seed%2147483647)+")%2147483647),a.id";
        std::vector<std::string> rank;
        if(q.sort=="relevant"&&!terms.empty()){
            sort="(";
            for(const auto& word:terms){
                if(!rank.empty())sort+="+";
                sort+="CASE WHEN instr(mrs_fold(a.relative_path),mrs_fold(?))>0 THEN 3 ELSE 0 END"
                    "+CASE WHEN EXISTS (SELECT 1 FROM asset_tags rt JOIN tags t ON t.id=rt.tag_id WHERE rt.asset_id=a.id"
                    " AND rt.status<>'rejected' AND mrs_fold(t.label)=mrs_fold(?)) THEN 5 ELSE 0 END";
                rank.push_back(word);rank.push_back(word);
            }
            sort+=") DESC,a.relative_path,a.id";
        }
        if(q.similar_asset_id>0){sort=std::string("mrs_similarity(sf.duration,sf.onsets,sf.centroid,sf.flatness,sf.rms,")+bpm+"),a.id";rank.clear();}
        const std::string query="SELECT a.id,a.source_id,a.relative_path,a.status,"
            "a.bpm_auto,a.bpm_manual,a.key_auto,a.key_manual,a.rate,a.frames,a.modified,"
            "EXISTS(SELECT 1 FROM favorites f WHERE f.asset_id=a.id),a.error"
            " FROM assets a"+featureJoin+where+" ORDER BY "+sort+" LIMIT ? OFFSET ?";
        Statement assets(d,query.c_str());
        int n=bind(assets);
        for(const auto& word:rank)assets.str(n++,word);
        assets.number(n++,static_cast<std::int64_t>(std::clamp(q.limit,std::uint64_t{1},std::uint64_t{300})));
        assets.number(n,static_cast<std::int64_t>(std::min(q.offset,std::uint64_t{1000000000})));
        Statement tags(d,"SELECT t.group_name,t.label,at.origin,at.status,at.confidence"
            " FROM asset_tags at JOIN tags t ON t.id=at.tag_id WHERE at.asset_id=?"
            " ORDER BY t.group_name,t.label");
        Statement feature(d,"SELECT f.state,f.reason,f.duration,f.rms,f.peak,f.onsets,f.centroid,f.flatness,f.bpm,f.bpm_confidence,f.key_label,f.key_confidence FROM audio_features f JOIN assets a ON a.id=f.asset_id JOIN sources s ON s.id=a.source_id WHERE a.id=? AND f.bytes=a.bytes AND f.modified=a.modified AND f.root=s.path AND f.version=2");
        while(assets.next()){
            AudioAsset item;
            item.id=sqlite3_column_int64(assets.stmt,0);
            item.source_id=sqlite3_column_int64(assets.stmt,1);
            const auto text_column=[&](int index){const auto* p=sqlite3_column_text(assets.stmt,index);
                return p?std::string(reinterpret_cast<const char*>(p)):std::string{};};
            item.relative_path=text_column(2);item.status=text_column(3);
            item.detected_bpm=sqlite3_column_int(assets.stmt,4);
            item.manual_bpm=sqlite3_column_int(assets.stmt,5);
            item.detected_key=text_column(6);item.manual_key=text_column(7);
            item.sample_rate=sqlite3_column_int64(assets.stmt,8);
            item.frames=sqlite3_column_int64(assets.stmt,9);
            item.modified=sqlite3_column_int64(assets.stmt,10);
            item.favorite=sqlite3_column_int(assets.stmt,11)!=0;
            feature.reset();feature.number(1,item.id);
            if(feature.next()){
                const auto field=[&](int i){const auto* raw=sqlite3_column_text(feature.stmt,i);return raw?std::string(reinterpret_cast<const char*>(raw)):std::string{};};
                auto& f=item.analysis;f.state=field(0);f.reason=field(1);f.duration=sqlite3_column_double(feature.stmt,2);f.rms=sqlite3_column_double(feature.stmt,3);f.peak=sqlite3_column_double(feature.stmt,4);f.onset_density=sqlite3_column_double(feature.stmt,5);f.centroid=sqlite3_column_double(feature.stmt,6);f.flatness=sqlite3_column_double(feature.stmt,7);f.bpm=sqlite3_column_double(feature.stmt,8);f.bpm_confidence=sqlite3_column_double(feature.stmt,9);f.key=field(10);f.key_confidence=sqlite3_column_double(feature.stmt,11);
                if(f.bpm>0)for(double multiplier:{.5,1.,2.})if(f.bpm*multiplier>=40.&&f.bpm*multiplier<=300.)f.tempo_candidates.push_back(f.bpm*multiplier);
            }else item.analysis.state=analysisProgress.running?"pending":"unknown";
            if(item.status!="ok"){item.analysis.state="error";item.analysis.reason=text_column(12);}
            const auto named=classify_sample_path(item.relative_path).metadata;
            item.bpm_source=item.manual_bpm?"manual":named.bpm?"filename":item.analysis.bpm>0?"audio":"unknown";
            item.key_source=!item.manual_key.empty()?"manual":named.key?"filename":!item.analysis.key.empty()?"audio":"unknown";
            const auto pos=item.relative_path.find_last_of('/');
            const auto dir=pos==std::string::npos?"":item.relative_path.substr(0,pos);
            item.artwork_path=artwork(item.source_id,dir);
            tags.reset();tags.number(1,item.id);
            while(tags.next()){
                const auto field=[&](int index){const auto* p=sqlite3_column_text(tags.stmt,index);
                    return p?std::string(reinterpret_cast<const char*>(p)):std::string{};};
                item.tags.push_back({field(0),field(1),field(2),field(3),sqlite3_column_double(tags.stmt,4)});
            }
            next->assets.push_back(std::move(item));
        }
        { std::lock_guard lock(mutex); next->revision = ++revision; state = std::move(next); }
    }
    void scan_one(Database& d, std::int64_t source_id, const fs::path& root, std::stop_token stop) {
        if (stop.stop_requested() || cancel.load()) throw Cancelled();
        std::error_code ec;
        if (!fs::is_directory(root, ec) || is_reparse(root))
            throw std::runtime_error("Library folder unavailable; relink to rescan (index preserved)");
        scanning = true;
        struct Flag { std::atomic<bool>& active; ~Flag() { active = false; } } reset{scanning};
        publish(d, Phase::scanning, utf8(root));
        d.exec("BEGIN IMMEDIATE");
        std::uint64_t processed{}, changed{};
        try {
            Statement zero(d, "UPDATE assets SET seen=0 WHERE source_id=?");
            zero.number(1,source_id); zero.done();
            Statement existing(d, "SELECT bytes,modified,status FROM assets WHERE source_id=? AND relative_path=?");
            Statement update(d,
                "INSERT INTO assets(source_id,relative_path,bytes,modified,rate,channels,bits,frames,status,error,seen)"
                " VALUES(?,?,?,?,?,?,?,?,?,?,1) ON CONFLICT(source_id,relative_path) DO UPDATE SET"
                " bytes=excluded.bytes,modified=excluded.modified,rate=excluded.rate,"
                " channels=excluded.channels,bits=excluded.bits,frames=excluded.frames,"
                " status=excluded.status,error=excluded.error,seen=1");
            Statement mark(d, "UPDATE assets SET seen=1 WHERE source_id=? AND relative_path=?");
            Statement zeroFolders(d,"UPDATE folders SET seen=0 WHERE source_id=?");
            zeroFolders.number(1,source_id);zeroFolders.done();
            Statement zeroCovers(d,"UPDATE folder_artwork SET seen=0,auto_image='',priority=0 WHERE source_id=?");
            zeroCovers.number(1,source_id);zeroCovers.done();
            Statement directory(d,"INSERT INTO folders(source_id,path,seen) VALUES(?,?,1)"
                " ON CONFLICT(source_id,path) DO UPDATE SET seen=1");
            const auto putDirectory=[&](const std::string& path){
                directory.reset();directory.number(1,source_id);directory.str(2,path);directory.done();
            };
            putDirectory("");
            Statement cover(d,"INSERT INTO folder_artwork(source_id,folder,auto_image,priority,seen)"
                " VALUES(?,?,?,?,1) ON CONFLICT(source_id,folder) DO UPDATE SET"
                " auto_image=CASE WHEN excluded.priority>priority THEN excluded.auto_image ELSE auto_image END,"
                " priority=MAX(priority,excluded.priority),seen=1");
            fs::recursive_directory_iterator it(root, fs::directory_options::none, ec), end;
            if (ec) throw std::runtime_error("Cannot enumerate library folder");
            for (; it != end;) {
                if (stop.stop_requested() || cancel.load()) throw Cancelled();
                auto current = it->path();
                if (is_reparse(current)) { it.disable_recursion_pending(); }
                else {
                    std::error_code type_error;
                    if(it->is_directory(type_error)){
                        putDirectory(utf8(current.lexically_relative(root)));
                    }
                    else if (it->is_regular_file(type_error) && is_wav(current)) {
                        auto relative = current.lexically_relative(root);
                        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
                            throw std::runtime_error("Unsafe library relative path");
                        std::error_code size_error, time_error;
                        const auto size = fs::file_size(current, size_error);
                        const auto modified = fs::last_write_time(current, time_error);
                        if (size_error || time_error || size > static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max()))
                            throw std::runtime_error("WAV attributes unavailable; scan rolled back");
                        const auto rel = utf8(relative);
                        const auto bytes = static_cast<std::int64_t>(size);
                        const auto stamp = static_cast<std::int64_t>(modified.time_since_epoch().count());
                        existing.reset(); existing.number(1,source_id); existing.str(2,rel);
                        bool unchanged = false;
                        if (existing.next()) {
                            const auto* raw=sqlite3_column_text(existing.stmt,2);
                            const std::string status=raw?reinterpret_cast<const char*>(raw):"";
                            unchanged = sqlite3_column_int64(existing.stmt,0)==bytes &&
                                sqlite3_column_int64(existing.stmt,1)==stamp &&
                                (status=="ok"||status=="invalid");
                        }
                        existing.reset(); // finalize SELECT before updating the same assets table
                        if (unchanged) {
                            mark.reset(); mark.number(1,source_id); mark.str(2,rel); mark.done();
                        } else {
                            std::int64_t rate{}, channels{}, bits{}, frames{};
                            std::string status="ok", reason;
                            try {
                                const auto info = mrs::audio::inspect_wav(current);
                                rate=info.sample_rate; channels=info.channels; bits=info.bits; frames=info.frame_count;
                            } catch (const std::exception& e) { status="invalid"; reason=std::string(e.what()).substr(0,400); }
                            update.reset();
                            update.number(1,source_id); update.str(2,rel); update.number(3,bytes);
                            update.number(4,stamp); update.number(5,rate); update.number(6,channels);
                            update.number(7,bits); update.number(8,frames);
                            update.str(9,status); update.str(10,reason); update.done();
                            ++changed;
                        }
                        ++processed;
                        if (processed % 512 == 0) publish(d,Phase::scanning,utf8(root),processed,changed);
                    } else if(!type_error&&it->is_regular_file(type_error)){
                        auto name=utf8(current.filename());
                        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                        int priority=0;
                        if(name=="cover.jpg"||name=="cover.png"||name=="cover.jpeg")priority=4;
                        else if(name=="folder.jpg"||name=="folder.png")priority=3;
                        else if(name=="artwork.jpg"||name=="artwork.png"||name=="artwork.jpeg")priority=2;
                        if(priority){
                            std::error_code imageError;
                            const auto bytes=fs::file_size(current,imageError);
                            if(!imageError&&bytes>0&&bytes<=20u*1024u*1024u){
                                auto folder=current.parent_path().lexically_relative(root);
                                const auto folderName=folder=="."?std::string{}:utf8(folder);
                                cover.reset();cover.number(1,source_id);cover.str(2,folderName);
                                cover.str(3,utf8(current.lexically_relative(root)));cover.number(4,priority);
                                cover.done();
                            }
                        }
                    }
                    if(type_error)throw std::runtime_error("Cannot inspect library item");
                }
                it.increment(ec);
                if (ec) throw std::runtime_error("Cannot traverse library folder; scan rolled back");
            }
            if (stop.stop_requested() || cancel.load()) throw Cancelled();
            Statement missing(d, "UPDATE assets SET status='missing',error='Missing from latest scan' WHERE source_id=? AND seen=0");
            missing.number(1,source_id); missing.done();
            Statement deleteFolders(d,"DELETE FROM folders WHERE source_id=? AND seen=0");
            deleteFolders.number(1,source_id);deleteFolders.done();
            // Preserve manual artwork associations across removed/moved folders.
            Statement deleteCovers(d,"DELETE FROM folder_artwork WHERE source_id=? AND seen=0 AND manual_image=''");
            deleteCovers.number(1,source_id);deleteCovers.done();
            classify_assets(d,source_id,[&]{return stop.stop_requested()||cancel.load();}); // proposals refresh, manual decisions preserved
            d.exec("COMMIT");
        } catch (...) {
            d.exec("ROLLBACK"); // rollback keeps the previous complete catalogue
            throw;
        }
        queue_analysis(d);publish(d,Phase::idle,"Scan complete",processed,changed);
    }
    void process(Database& d, const Command& c, std::stop_token stop) {
        if(c.kind==Kind::save_search){
            if(c.label.empty()||c.label.size()>96||c.query.tags.size()+c.query.excluded_tags.size()>64)throw std::invalid_argument("Saved search name/filters exceed budget");
            d.exec("BEGIN IMMEDIATE");
            try{
                Statement make(d,"INSERT INTO saved_searches(name,source,collection,folder,search,sort,key_label,bpm_min,bpm_max,favorites,similar,seed) VALUES(?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(name) DO UPDATE SET source=excluded.source,collection=excluded.collection,folder=excluded.folder,search=excluded.search,sort=excluded.sort,key_label=excluded.key_label,bpm_min=excluded.bpm_min,bpm_max=excluded.bpm_max,favorites=excluded.favorites,similar=excluded.similar,seed=excluded.seed");
                make.str(1,c.label);make.number(2,c.query.source_id);make.number(3,c.query.collection_id);make.str(4,c.query.folder);make.str(5,c.query.search);make.str(6,c.query.sort);make.str(7,c.query.key);make.number(8,c.query.bpm_min);make.number(9,c.query.bpm_max);make.number(10,c.query.favorites_only?1:0);make.number(11,c.query.similar_asset_id);make.number(12,static_cast<std::int64_t>(c.query.shuffle_seed%2147483647));make.done();
                Statement id(d,"SELECT id FROM saved_searches WHERE name=?");id.str(1,c.label);if(!id.next())throw std::runtime_error("Saved search missing");const auto searchId=sqlite3_column_int64(id.stmt,0);id.reset();
                Statement clear(d,"DELETE FROM saved_search_tags WHERE search_id=?");clear.number(1,searchId);clear.done();Statement insert(d,"INSERT INTO saved_search_tags VALUES(?,?,?,?)");
                for(bool excluded:{false,true})for(const auto& tag:excluded?c.query.excluded_tags:c.query.tags){valid_tag(tag.group,tag.label);insert.reset();insert.number(1,searchId);insert.number(2,excluded?1:0);insert.str(3,tag.group);insert.str(4,tag.label);insert.done();}
                d.exec("COMMIT");
            }catch(...){d.exec("ROLLBACK");throw;}
            publish(d,Phase::idle,"Search saved");return;
        }
        if(c.kind==Kind::delete_search){Statement erase(d,"DELETE FROM saved_searches WHERE id=?");erase.number(1,c.id);erase.done();publish(d,Phase::idle,"Saved search removed");return;}
        if(c.kind==Kind::analyse){queue_analysis(d,c.enabled);publish(d,Phase::idle,"Audio analysis queued");return;}
        if(c.kind==Kind::analysis_control){
            if(c.number==0){analysisGeneration.fetch_add(1);{std::lock_guard lock(analysisMutex);analysisQueue.clear();}analysisProgress.running=false;analysisPaused=false;}
            else analysisPaused=c.number==1;
            analysisWake.notify_all();publish(d,Phase::idle,c.number==0?"Audio analysis cancelled":c.number==1?"Audio analysis paused":"Audio analysis resumed");return;
        }
        if(c.kind==Kind::analysis_result){
            const auto& job=*c.analysis;if(job.generation!=analysisGeneration.load())return;
            Statement current(d,"SELECT 1 FROM assets a JOIN sources s ON s.id=a.source_id WHERE a.id=? AND a.bytes=? AND a.modified=? AND s.path=? AND s.enabled=1 AND a.status='ok'");
            current.number(1,job.id);current.number(2,job.bytes);current.number(3,job.modified);current.str(4,job.root);
            const bool valid=current.next();current.reset();
            if(valid){
                d.exec("BEGIN IMMEDIATE");
                try{
                    const auto& f=job.result;
                    Statement feature(d,"INSERT OR REPLACE INTO audio_features VALUES(?,?,?,?,2,?,?,?,?,?,?,?,?,?,?,?,?)");
                    feature.number(1,job.id);feature.number(2,job.bytes);feature.number(3,job.modified);feature.str(4,job.root);feature.str(5,f.state);feature.str(6,f.reason);
                    feature.real(7,f.duration);feature.real(8,f.rms);feature.real(9,f.peak);feature.real(10,f.onset_density);feature.real(11,f.centroid);feature.real(12,f.flatness);feature.real(13,f.bpm);feature.real(14,f.bpm_confidence);feature.str(15,f.key);feature.real(16,f.key_confidence);feature.done();
                    Statement type(d,"INSERT OR REPLACE INTO audio_type_analysis VALUES(?,?,?,?,1,?,?,?)");type.number(1,job.id);type.number(2,job.bytes);type.number(3,job.modified);type.str(4,job.root);type.str(5,f.type.label);type.real(6,f.type.confidence);type.str(7,f.type.reason);type.done();
                    classify_assets(d,{}, {},job.id);d.exec("COMMIT");
                }catch(...){d.exec("ROLLBACK");throw;}
            }
            ++analysisProgress.completed;analysisProgress.running=analysisProgress.completed<analysisProgress.total;
            if(analysisProgress.running)refill_analysis(d);
            const auto now=std::chrono::steady_clock::now();
            if(!analysisProgress.running||now-lastAnalysisPublish>std::chrono::milliseconds(100)){
                lastAnalysisPublish=now;const auto prior=snapshot();publish(d,prior->phase,prior->message,prior->processed,prior->changed);
            }
            return;
        }
        if (c.kind == Kind::add) {
            const auto root = normalized(c.root);
            if (is_reparse(root)) throw std::runtime_error("Linked/reparse library roots are unsupported");
            Statement add(d,"INSERT OR IGNORE INTO sources(path) VALUES(?)");
            add.str(1,utf8(root)); add.done();
            Statement find(d,"SELECT id FROM sources WHERE path=?");
            find.str(1,utf8(root));
            if (!find.next()) throw std::runtime_error("Library source not saved");
            const auto id=sqlite3_column_int64(find.stmt,0);
            cancel=false; scan_one(d,id,root,stop);
        } else if (c.kind == Kind::relink) {
            const auto root=normalized(c.root);
            Statement update(d,"UPDATE sources SET path=? WHERE id=?");
            update.str(1,utf8(root));update.number(2,c.id);update.done();
            if (!sqlite3_changes(d.db)) throw std::runtime_error("Unknown source or unchanged path");
            cancel=false;scan_one(d,c.id,root,stop);
        } else if (c.kind == Kind::remove) {
            Statement del(d,"DELETE FROM sources WHERE id=?");
            del.number(1,c.id);del.done();
            Statement covers(d,"DELETE FROM managed_covers WHERE kind='folder' AND owner=?");covers.number(1,c.id);covers.done();
            queue_analysis(d);publish(d,Phase::idle,"Source removed from index; files untouched");
        } else if (c.kind == Kind::enable) {
            Statement set(d,"UPDATE sources SET enabled=? WHERE id=?");
            set.number(1,c.enabled?1:0);set.number(2,c.id);set.done();
            queue_analysis(d);publish(d,Phase::idle,"Source updated");
        } else if (c.kind == Kind::view) {
            browserQuery=LibraryQuery{};
            browserQuery.source_id=c.id;browserQuery.search=c.label.substr(0,200);
            publish(d,Phase::idle);
        } else if (c.kind == Kind::browse) {
            browserQuery=c.query;
            browserQuery.limit=std::clamp(browserQuery.limit,std::uint64_t{1},std::uint64_t{300});
            browserQuery.search=browserQuery.search.substr(0,512);
            if(browserQuery.folder.find("..")!=std::string::npos||browserQuery.folder.size()>1024)
                throw std::invalid_argument("Invalid library folder filter");
            while(!browserQuery.folder.empty()&&browserQuery.folder.back()=='/')
                browserQuery.folder.pop_back();
            publish(d,Phase::idle);
        } else if (c.kind == Kind::create_collection) {
            if(c.label.empty()||c.label.size()>100)throw std::invalid_argument("Collection name must be 1..100 bytes");
            Statement create(d,"INSERT OR IGNORE INTO collections(name) VALUES(?)");
            create.str(1,c.label);create.done();
            publish(d,Phase::idle,"Collection available");
        } else if (c.kind == Kind::delete_collection) {
            if(c.id<=0)throw std::invalid_argument("Select collection to remove");
            Statement del(d,"DELETE FROM collections WHERE id=?");
            del.number(1,c.id);del.done();
            Statement covers(d,"DELETE FROM managed_covers WHERE kind='collection' AND owner=?");covers.number(1,c.id);covers.done();
            if(browserQuery.collection_id==c.id)browserQuery.collection_id=0;
            publish(d,Phase::idle,"Collection removed; WAVs untouched");
        } else if (c.kind == Kind::collection_items || c.kind == Kind::favorites) {
            if(c.ids.empty()||c.ids.size()>5000)throw std::invalid_argument("Choose WAV files");
            if(c.kind==Kind::collection_items&&c.id<=0)throw std::invalid_argument("Choose collection");
            d.exec("BEGIN IMMEDIATE");
            try {
                Statement memberAdd(d,"INSERT OR IGNORE INTO collection_items(collection_id,asset_id) VALUES(?,?)");
                Statement memberRemove(d,"DELETE FROM collection_items WHERE collection_id=? AND asset_id=?");
                Statement favAdd(d,"INSERT OR IGNORE INTO favorites(asset_id) VALUES(?)");
                Statement favRemove(d,"DELETE FROM favorites WHERE asset_id=?");
                for(const auto id:c.ids){
                    if(c.kind==Kind::favorites){
                        auto& stmt=c.enabled?favAdd:favRemove;
                        stmt.reset();stmt.number(1,id);stmt.done();
                    }else{
                        auto& stmt=c.enabled?memberAdd:memberRemove;
                        stmt.reset();stmt.number(1,c.id);stmt.number(2,id);stmt.done();
                    }
                }
                d.exec("COMMIT");
            }catch(...){d.exec("ROLLBACK");throw;}
            publish(d,Phase::idle,"Collection updated");
        } else if(c.kind==Kind::cover){
            if(c.group!="folder"&&c.group!="tag"&&c.group!="group"&&c.group!="collection")throw std::invalid_argument("Invalid cover target");
            if(c.label.size()>4096||c.id<0)throw std::invalid_argument("Invalid cover identity");
            if(c.group=="folder"||c.group=="collection"){
                Statement target(d,c.group=="folder"?"SELECT 1 FROM sources WHERE id=?":"SELECT 1 FROM collections WHERE id=?");
                target.number(1,c.id);if(!target.next())throw std::invalid_argument("Cover target no longer exists");
            }
            const auto folder=fs::absolute(database).parent_path()/"artwork";
            if(fs::is_symlink(folder)||is_reparse(folder))throw std::invalid_argument("Artwork storage must not be a link");
            std::string previous;
            {Statement old(d,"SELECT image FROM managed_covers WHERE kind=? AND owner=? AND key=?");old.str(1,c.group);old.number(2,c.id);old.str(3,c.label);
                if(old.next()){const auto* value=sqlite3_column_text(old.stmt,0);if(value)previous=reinterpret_cast<const char*>(value);}}
            fs::path copied;
            try{
                std::string name;
                if(!c.root.empty()){
                    const auto input=fs::canonical(c.root);auto ext=utf8(input.extension());
                    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char x){return static_cast<char>(std::tolower(x));});
                    if((ext!=".png"&&ext!=".jpg"&&ext!=".jpeg")||!artwork_image_supported(input))
                        throw std::invalid_argument("Choose a readable JPG/PNG up to 20 MiB and 16 megapixels");
                    if(fs::is_symlink(folder)||is_reparse(folder))throw std::invalid_argument("Artwork storage must not be a link");
                    fs::create_directories(folder);name=new_id().value+ext;copied=folder/name;
                    fs::copy_file(input,copied,fs::copy_options::none);
                }
                Statement update(d,"INSERT INTO managed_covers(kind,owner,key,image) VALUES(?,?,?,?)"
                    " ON CONFLICT(kind,owner,key) DO UPDATE SET image=excluded.image");
                update.str(1,c.group);update.number(2,c.id);update.str(3,c.label);update.str(4,name);update.done();
            }catch(...){if(!copied.empty()){std::error_code ec;fs::remove(copied,ec);}throw;}
            if(!previous.empty()&&from_utf8(previous).filename()==from_utf8(previous)){
                std::error_code ec;fs::remove(folder/from_utf8(previous),ec);
            }
            publish(d,Phase::idle,"Cover saved");
        } else if (c.kind == Kind::artwork) {
            Statement source(d,"SELECT path FROM sources WHERE id=?");
            source.number(1,c.id);
            if(!source.next())throw std::invalid_argument("Unknown library source");
            const auto* path=sqlite3_column_text(source.stmt,0);
            const fs::path root=from_utf8(path?reinterpret_cast<const char*>(path):"");
            source.reset();
            if(c.label.find("..")!=std::string::npos)throw std::invalid_argument("Invalid artwork folder");
            std::string relative;
            if(!c.root.empty()){
                const auto image=normalized(c.root);
                const auto rel=image.lexically_relative(root);
                if(rel.empty()||rel.is_absolute()||*rel.begin()==".."||is_reparse(image))
                    throw std::invalid_argument("Artwork must be inside the selected library");
                auto ext=utf8(image.extension());
                std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char x){return static_cast<char>(std::tolower(x));});
                if(ext!=".png"&&ext!=".jpg"&&ext!=".jpeg")throw std::invalid_argument("Artwork must be JPG or PNG");
                std::error_code ec;const auto size=fs::file_size(image,ec);
                if(ec||size==0||size>20u*1024u*1024u)
                    throw std::invalid_argument("Artwork missing or larger than 20 MiB");
                relative=utf8(rel);
            }
            Statement override(d,"INSERT INTO folder_artwork(source_id,folder,manual_image) VALUES(?,?,?)"
                " ON CONFLICT(source_id,folder) DO UPDATE SET manual_image=excluded.manual_image");
            override.number(1,c.id);override.str(2,c.label);override.str(3,relative);override.done();
            publish(d,Phase::idle,"Artwork override saved");
        } else if (c.kind == Kind::tag || c.kind == Kind::tag_status ||
                   c.kind == Kind::bpm || c.kind == Kind::key) {
            if(c.ids.empty()||c.ids.size()>5000)throw std::invalid_argument("Choose 1..5000 WAV assets");
            if(c.kind==Kind::tag || c.kind==Kind::tag_status)valid_tag(c.group,c.label);
            if(c.kind==Kind::tag_status&&c.status!="confirmed"&&c.status!="rejected")
                throw std::invalid_argument("Unknown tag decision");
            if(c.kind==Kind::bpm&&c.number!=-1&&c.number!=0&&(c.number<45||c.number>250))
                throw std::invalid_argument("Manual BPM must be 45..250 or 0 (unknown)");
            std::string key=c.label;
            if(c.kind==Kind::key&&!key.empty()&&key!="-"){
                const auto parsed=classify_sample_path("sample_"+key+".wav").metadata.key;
                if(!parsed)throw std::invalid_argument("Manual key format: Am, Gm, F#maj etc.");
                key=*parsed;
            }
            d.exec("BEGIN IMMEDIATE");
            try {
                Statement batch(d,"INSERT INTO edit_batches DEFAULT VALUES");batch.done();
                const auto batch_id=sqlite3_last_insert_rowid(d.db);
                Statement exists(d,"SELECT id FROM assets WHERE id=?");
                Statement make(d,"INSERT OR IGNORE INTO tags(group_name,label) VALUES(?,?)");
                Statement find(d,"SELECT id FROM tags WHERE group_name=? AND label=?");
                Statement prior(d,"SELECT origin,status,confidence FROM asset_tags WHERE asset_id=? AND tag_id=?");
                Statement logTag(d,"INSERT INTO tag_edit_history(batch_id,asset_id,tag_id,existed,prior_origin,prior_status,prior_confidence)"
                    " VALUES(?,?,?,?,?,?,?)");
                Statement upsert(d,"INSERT INTO asset_tags(asset_id,tag_id,origin,status,confidence)"
                    " VALUES(?,?,?, ?,?) ON CONFLICT(asset_id,tag_id) DO UPDATE SET"
                    " origin=excluded.origin,status=excluded.status,confidence=excluded.confidence");
                Statement logOtherType(d,"INSERT INTO tag_edit_history(batch_id,asset_id,tag_id,existed,prior_origin,prior_status,prior_confidence) SELECT ?,at.asset_id,at.tag_id,1,at.origin,at.status,at.confidence FROM asset_tags at JOIN tags t ON t.id=at.tag_id WHERE at.asset_id=? AND t.group_name=? AND at.tag_id<>? AND at.status<>'rejected'");
                Statement removeOtherType(d,"DELETE FROM asset_tags WHERE asset_id=? AND tag_id<>? AND status<>'rejected' AND tag_id IN (SELECT id FROM tags WHERE group_name=?)");
                Statement oldMeta(d,"SELECT bpm_manual,key_manual FROM assets WHERE id=?");
                Statement logMeta(d,"INSERT INTO metadata_edit_history(batch_id,asset_id,prior_bpm,prior_key) VALUES(?,?,?,?)");
                Statement putBpm(d,"UPDATE assets SET bpm_manual=? WHERE id=?");
                Statement putKey(d,"UPDATE assets SET key_manual=? WHERE id=?");
                std::vector<std::int64_t> unique=c.ids;
                std::sort(unique.begin(),unique.end());unique.erase(std::unique(unique.begin(),unique.end()),unique.end());
                std::int64_t tag_id{};
                if(c.kind==Kind::tag||c.kind==Kind::tag_status){
                    make.str(1,c.group);make.str(2,c.label);make.done();
                    find.str(1,c.group);find.str(2,c.label);
                    if(!find.next())throw std::runtime_error("Tag creation failed");
                    tag_id=sqlite3_column_int64(find.stmt,0);find.reset();
                }
                for(const auto id:unique){
                    exists.reset();exists.number(1,id);
                    if(!exists.next())throw std::invalid_argument("Selected WAV was removed; refresh catalogue");
                    exists.reset();
                    if(c.kind==Kind::tag||c.kind==Kind::tag_status){
                        if((c.group=="Type"||c.group=="Pack")&&(c.kind==Kind::tag||c.status=="confirmed")){
                            logOtherType.reset();logOtherType.number(1,batch_id);logOtherType.number(2,id);logOtherType.str(3,c.group);logOtherType.number(4,tag_id);logOtherType.done();
                            removeOtherType.reset();removeOtherType.number(1,id);removeOtherType.number(2,tag_id);removeOtherType.str(3,c.group);removeOtherType.done();
                        }
                        prior.reset();prior.number(1,id);prior.number(2,tag_id);
                        const bool existed=prior.next();
                        std::string oldOrigin,oldStatus;double oldConfidence=0.;
                        if(existed){
                            const auto* origin=sqlite3_column_text(prior.stmt,0);
                            const auto* status=sqlite3_column_text(prior.stmt,1);
                            oldOrigin=origin?reinterpret_cast<const char*>(origin):"";
                            oldStatus=status?reinterpret_cast<const char*>(status):"";
                            oldConfidence=sqlite3_column_double(prior.stmt,2);
                        }
                        prior.reset();
                        logTag.reset();logTag.number(1,batch_id);logTag.number(2,id);logTag.number(3,tag_id);
                        logTag.number(4,existed?1:0);logTag.str(5,oldOrigin);logTag.str(6,oldStatus);
                        if(sqlite3_bind_double(logTag.stmt,7,oldConfidence)!=SQLITE_OK)fail(d.db,"Tag history");
                        logTag.done();
                        upsert.reset();upsert.number(1,id);upsert.number(2,tag_id);
                        upsert.str(3,c.kind==Kind::tag?"manual":existed?oldOrigin:"manual");
                        upsert.str(4,c.kind==Kind::tag?"confirmed":c.status);
                        if(sqlite3_bind_double(upsert.stmt,5,c.kind==Kind::tag?1.:existed?oldConfidence:1.)!=SQLITE_OK)
                            fail(d.db,"Manual confidence");
                        upsert.done();
                    } else {
                        oldMeta.reset();oldMeta.number(1,id);
                        if(!oldMeta.next())throw std::runtime_error("Missing WAV metadata");
                        const auto beforeBpm=sqlite3_column_int(oldMeta.stmt,0);
                        const auto* raw=sqlite3_column_text(oldMeta.stmt,1);
                        const std::string beforeKey=raw?reinterpret_cast<const char*>(raw):"";
                        oldMeta.reset();
                        logMeta.reset();logMeta.number(1,batch_id);logMeta.number(2,id);
                        logMeta.number(3,beforeBpm);logMeta.str(4,beforeKey);logMeta.done();
                        if(c.kind==Kind::bpm){putBpm.reset();putBpm.number(1,c.number==0?-1:c.number==-1?0:c.number);putBpm.number(2,id);putBpm.done();}
                        else {putKey.reset();putKey.str(1,key);putKey.number(2,id);putKey.done();}
                    }
                }
                d.exec("COMMIT");
            }catch(...){d.exec("ROLLBACK");throw;}
            publish(d,Phase::idle,"Library metadata updated");
        } else if (c.kind == Kind::undo) {
            Statement last(d,"SELECT MAX(id) FROM edit_batches");
            if(!last.next()||sqlite3_column_type(last.stmt,0)==SQLITE_NULL){publish(d,Phase::idle,"No library edits to undo");return;}
            const auto batch_id=sqlite3_column_int64(last.stmt,0);last.reset();
            d.exec("BEGIN IMMEDIATE");
            try {
                Statement tags(d,"SELECT asset_id,tag_id,existed,prior_origin,prior_status,prior_confidence"
                    " FROM tag_edit_history WHERE batch_id=?");
                tags.number(1,batch_id);
                Statement remove(d,"DELETE FROM asset_tags WHERE asset_id=? AND tag_id=?");
                Statement restore(d,"INSERT INTO asset_tags(asset_id,tag_id,origin,status,confidence)"
                    " VALUES(?,?,?,?,?) ON CONFLICT(asset_id,tag_id) DO UPDATE SET"
                    " origin=excluded.origin,status=excluded.status,confidence=excluded.confidence");
                while(tags.next()){
                    const auto aid=sqlite3_column_int64(tags.stmt,0),tid=sqlite3_column_int64(tags.stmt,1);
                    if(sqlite3_column_int(tags.stmt,2)){
                        const auto* origin=sqlite3_column_text(tags.stmt,3);
                        const auto* status=sqlite3_column_text(tags.stmt,4);
                        restore.reset();restore.number(1,aid);restore.number(2,tid);
                        restore.str(3,origin?reinterpret_cast<const char*>(origin):"");
                        restore.str(4,status?reinterpret_cast<const char*>(status):"");
                        if(sqlite3_bind_double(restore.stmt,5,sqlite3_column_double(tags.stmt,5))!=SQLITE_OK)
                            fail(d.db,"Restore tag");
                        restore.done();
                    }else{remove.reset();remove.number(1,aid);remove.number(2,tid);remove.done();}
                }
                Statement meta(d,"SELECT asset_id,prior_bpm,prior_key FROM metadata_edit_history WHERE batch_id=?");
                meta.number(1,batch_id);
                Statement update(d,"UPDATE assets SET bpm_manual=?,key_manual=? WHERE id=?");
                while(meta.next()){
                    const auto* key=sqlite3_column_text(meta.stmt,2);
                    update.reset();update.number(1,sqlite3_column_int(meta.stmt,1));
                    update.str(2,key?reinterpret_cast<const char*>(key):"");
                    update.number(3,sqlite3_column_int64(meta.stmt,0));update.done();
                }
                Statement del(d,"DELETE FROM edit_batches WHERE id=?");del.number(1,batch_id);del.done();
                d.exec("COMMIT");
            }catch(...){d.exec("ROLLBACK");throw;}
            publish(d,Phase::idle,"Last library edit undone");
        } else if (c.kind == Kind::rule) {
            valid_tag(c.group,c.label);
            if(c.phrase.empty()||c.phrase.size()>128)throw std::invalid_argument("Invalid rule phrase");
            d.exec("BEGIN IMMEDIATE");
            try {
                Statement rule(d,"INSERT INTO classification_rules(group_name,phrase,label,in_folder)"
                    " VALUES(?,?,?,?) ON CONFLICT(group_name,phrase,in_folder)"
                    " DO UPDATE SET label=excluded.label");
                rule.str(1,c.group);rule.str(2,c.phrase);rule.str(3,c.label);
                rule.number(4,c.folder?1:0);rule.done();
                classify_assets(d,{},[&]{return stop.stop_requested();});
                d.exec("COMMIT");
            }catch(...){d.exec("ROLLBACK");throw;}
            publish(d,Phase::idle,"Rules applied; manual decisions preserved");
        } else if (c.kind == Kind::rescan) {
            Statement list(d,"SELECT id,path FROM sources WHERE enabled=1 ORDER BY id");
            std::vector<std::pair<std::int64_t,fs::path>> sources;
            while(list.next()){
                const auto* p=sqlite3_column_text(list.stmt,1);
                sources.emplace_back(sqlite3_column_int64(list.stmt,0),from_utf8(p?reinterpret_cast<const char*>(p):""));
            }
            cancel=false;
            for(const auto& [id,root]:sources) {
                if (cancel.load()||stop.stop_requested()) throw Cancelled();
                scan_one(d,id,root,stop);
            }
            if (sources.empty()) publish(d,Phase::idle,"No enabled sources to scan");
        }
    }
    void run(std::stop_token stop) {
        try {
            Database db(database);
            create_schema(db);
            scanning=true;publish(db,Phase::scanning,"Analysing sample types (cached results reused)");
            db.exec("BEGIN IMMEDIATE");
            bool startupCancelled=false;
            try { normalize_types(db);classify_assets(db,{},[&]{return stop.stop_requested()||cancel.load();});db.exec("COMMIT"); }
            catch (const Cancelled&){db.exec("ROLLBACK");startupCancelled=true;publish(db,Phase::cancelled,"Type analysis cancelled; previous index preserved");}
            catch (...){db.exec("ROLLBACK");throw;}
            scanning=false;
            if(!startupCancelled){queue_analysis(db);publish(db,Phase::idle);}
            while (!stop.stop_requested()) {
                Command c;
                { std::unique_lock lock(mutex);
                    wake.wait(lock,[&]{return !commands.empty()||stop.stop_requested();});
                    if (stop.stop_requested()) return;
                    c=std::move(commands.front());commands.pop_front();
                }
                try { process(db,c,stop); }
                catch (const Cancelled&) { publish(db,Phase::cancelled,"Scan cancelled; previous index preserved"); }
                catch (const std::exception& e) { publish(db,Phase::error,e.what()); }
            }
        } catch (const std::exception& e) {
            auto next=std::make_shared<Snapshot>();next->phase=Phase::error;next->message=e.what();
            std::lock_guard lock(mutex);next->revision=++revision;state=std::move(next);
        }
    }
};
SampleLibrary::SampleLibrary(fs::path database) : impl_(std::make_unique<Impl>(std::move(database))) {}
std::string source_folder_name(const fs::path& root){
    auto folder=root.lexically_normal();
    if(folder.filename().empty()&&folder!=folder.root_path())folder=folder.parent_path();
    return utf8(!folder.filename().empty()?folder.filename():!folder.root_name().empty()?folder.root_name():folder.root_path());
}
SampleLibrary::~SampleLibrary() = default;
void SampleLibrary::add_source(fs::path root) { impl_->enqueue({Impl::Kind::add,0,std::move(root)}); }
void SampleLibrary::relink_source(std::int64_t id, fs::path root) { impl_->enqueue({Impl::Kind::relink,id,std::move(root)}); }
void SampleLibrary::remove_source(std::int64_t id) { impl_->enqueue({Impl::Kind::remove,id,{}}); }
void SampleLibrary::enable_source(std::int64_t id,bool enabled) { impl_->enqueue({Impl::Kind::enable,id,{},enabled}); }
void SampleLibrary::set_view(std::int64_t source,std::string filter){
    Impl::Command c(Impl::Kind::view,source);c.label=std::move(filter);impl_->enqueue(std::move(c));
}
void SampleLibrary::add_tag(const std::vector<std::int64_t>& ids,std::string group,std::string label){
    Impl::Command c(Impl::Kind::tag);c.ids=ids;c.group=std::move(group);c.label=std::move(label);
    impl_->enqueue(std::move(c));
}
void SampleLibrary::set_tag_status(const std::vector<std::int64_t>& ids,std::string group,std::string label,std::string status){
    Impl::Command c(Impl::Kind::tag_status);c.ids=ids;c.group=std::move(group);c.label=std::move(label);
    c.status=std::move(status);impl_->enqueue(std::move(c));
}
void SampleLibrary::set_bpm(const std::vector<std::int64_t>& ids,int bpm){
    Impl::Command c(Impl::Kind::bpm);c.ids=ids;c.number=bpm;impl_->enqueue(std::move(c));
}
void SampleLibrary::set_key(const std::vector<std::int64_t>& ids,std::string key){
    Impl::Command c(Impl::Kind::key);c.ids=ids;c.label=std::move(key);impl_->enqueue(std::move(c));
}
void SampleLibrary::add_rule(std::string group,std::string phrase,std::string label,bool folder){
    Impl::Command c(Impl::Kind::rule);c.group=std::move(group);c.phrase=std::move(phrase);
    c.label=std::move(label);c.folder=folder;impl_->enqueue(std::move(c));
}
void SampleLibrary::browse(LibraryQuery query){
    Impl::Command c(Impl::Kind::browse);c.query=std::move(query);impl_->enqueue(std::move(c));
}
void SampleLibrary::create_collection(std::string name){
    Impl::Command c(Impl::Kind::create_collection);c.label=std::move(name);impl_->enqueue(std::move(c));
}
void SampleLibrary::delete_collection(std::int64_t id){
    impl_->enqueue(Impl::Command(Impl::Kind::delete_collection,id));
}
void SampleLibrary::set_collection_items(std::int64_t id,std::vector<std::int64_t> ids,bool add){
    Impl::Command c(Impl::Kind::collection_items,id);c.ids=std::move(ids);c.enabled=add;impl_->enqueue(std::move(c));
}
void SampleLibrary::set_favorites(std::vector<std::int64_t> ids,bool enabled){
    Impl::Command c(Impl::Kind::favorites);c.ids=std::move(ids);c.enabled=enabled;impl_->enqueue(std::move(c));
}
void SampleLibrary::set_artwork(std::int64_t id,std::string folder,fs::path image){
    Impl::Command c(Impl::Kind::artwork,id,std::move(image));c.label=std::move(folder);impl_->enqueue(std::move(c));
}
void SampleLibrary::undo_tag_edit(){impl_->enqueue(Impl::Command(Impl::Kind::undo));}
void SampleLibrary::rescan() { impl_->enqueue(Impl::Command(Impl::Kind::rescan)); }
void SampleLibrary::cancel_scan() { impl_->cancel=true; }
void SampleLibrary::analyse_audio(bool force){Impl::Command c(Impl::Kind::analyse);c.enabled=force;impl_->enqueue(std::move(c));}
void SampleLibrary::pause_analysis(bool paused){impl_->analysisPaused=paused;impl_->analysisWake.notify_all();Impl::Command c(Impl::Kind::analysis_control);c.number=paused?1:2;impl_->enqueue(std::move(c));}
void SampleLibrary::cancel_analysis(){impl_->analysisGeneration.fetch_add(1);impl_->analysisPaused=false;impl_->analysisWake.notify_all();Impl::Command c(Impl::Kind::analysis_control);c.number=0;impl_->enqueue(std::move(c));}
void SampleLibrary::save_search(std::string name,LibraryQuery query){Impl::Command c(Impl::Kind::save_search);c.label=std::move(name);c.query=std::move(query);impl_->enqueue(std::move(c));}
void SampleLibrary::delete_search(std::int64_t id){impl_->enqueue({Impl::Kind::delete_search,id});}
std::shared_ptr<const Snapshot> SampleLibrary::snapshot() const { return impl_->snapshot(); }
} // namespace mrs::library

namespace mrs::library {
void SampleLibrary::set_cover(std::string kind,std::int64_t owner,std::string key,std::filesystem::path image){
    Impl::Command c(Impl::Kind::cover,owner,std::move(image));c.group=std::move(kind);c.label=std::move(key);impl_->enqueue(std::move(c));
}
}
namespace mrs::library {
bool artwork_image_supported(const std::filesystem::path& path){
    try{
        std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
        if(ec||size<24||size>20u*1024u*1024u||!std::filesystem::is_regular_file(path))return false;
        std::ifstream input(path,std::ios::binary);std::vector<unsigned char> header(static_cast<std::size_t>(std::min<std::uintmax_t>(size,65536)));
        input.read(reinterpret_cast<char*>(header.data()),static_cast<std::streamsize>(header.size()));if(!input)return false;
        const auto valid=[](std::uint32_t w,std::uint32_t h){return w&&h&&w<=8192&&h<=8192&&static_cast<std::uint64_t>(w)*h<=16u*1024u*1024u;};
        if(header[0]==137&&header[1]==80&&header[2]==78&&header[3]==71&&header[12]==73&&header[13]==72&&header[14]==68&&header[15]==82){
            const auto word=[&](std::size_t i){return (static_cast<std::uint32_t>(header[i])<<24)|(static_cast<std::uint32_t>(header[i+1])<<16)|(static_cast<std::uint32_t>(header[i+2])<<8)|header[i+3];};
            return valid(word(16),word(20));
        }
        if(header[0]!=255||header[1]!=216)return false;
        std::size_t at=2;
        while(at+8<header.size()){
            if(header[at++]!=255)return false;
            while(at<header.size()&&header[at]==255)++at;
            if(at+2>=header.size())return false;
            const auto marker=header[at++];
            if(marker==0xda||marker==0xd9)return false;
            const auto length=static_cast<std::size_t>(header[at])*256+header[at+1];if(length<2||at+length>header.size())return false;
            if((marker>=0xc0&&marker<=0xc3)||(marker>=0xc5&&marker<=0xc7)||(marker>=0xc9&&marker<=0xcb)||(marker>=0xcd&&marker<=0xcf))
                return length>=8&&valid(static_cast<std::uint32_t>(header[at+5])*256+header[at+6],static_cast<std::uint32_t>(header[at+3])*256+header[at+4]);
            at+=length;
        }
    }catch(const std::exception&){}
    return false;
}
}
