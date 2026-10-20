#include <mrs/sample_library.hpp>
#include <mrs/sample_tag_rules.hpp>
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
#include <cctype>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <set>
#include <sstream>
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
struct Database {
    sqlite3* db{};
    explicit Database(const fs::path& file) {
        if (file.empty()) throw std::invalid_argument("MR Library database path is empty");
        fs::create_directories(file.parent_path());
        const auto name = utf8(file);
        if (sqlite3_open_v2(name.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
            fail(db, "Open MR Library");
        sqlite3_busy_timeout(db, 1500);
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
    if (v < 0 || v > 3) throw std::runtime_error("Unsupported MR Library database schema");
    if (v == 3) return;
    d.exec("BEGIN IMMEDIATE");
    try {
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
        d.exec("PRAGMA user_version=3");
        d.exec("COMMIT");
    } catch (...) { d.exec("ROLLBACK"); throw; }
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
void classify_assets(Database& d,const std::optional<std::int64_t> source = {}) {
    const auto rules=read_rules(d);
    struct Asset {std::int64_t id;std::string path;};
    std::vector<Asset> items;
    Statement list(d,"SELECT id,relative_path FROM assets WHERE status='ok' AND (?=0 OR source_id=?) ORDER BY id");
    list.number(1,source.value_or(0));list.number(2,source.value_or(0));
    while(list.next()){
        const auto* name=sqlite3_column_text(list.stmt,1);
        items.push_back({sqlite3_column_int64(list.stmt,0),name?reinterpret_cast<const char*>(name):""});
    }
    Statement clear(d,"DELETE FROM asset_tags WHERE asset_id=? AND status='proposed'");
    Statement make(d,"INSERT OR IGNORE INTO tags(group_name,label) VALUES(?,?)");
    Statement find(d,"SELECT id FROM tags WHERE group_name=? AND label=?");
    Statement put(d,"INSERT OR IGNORE INTO asset_tags(asset_id,tag_id,origin,status,confidence) VALUES(?,?,?,'proposed',?)");
    Statement typed(d,"UPDATE assets SET bpm_auto=?,key_auto=? WHERE id=?");
    for(const auto& item:items){
        const auto suggestion=classify_sample_path(item.path,rules);
        clear.reset();clear.number(1,item.id);clear.done();
        for(const auto& tag:suggestion.tags){
            make.reset();make.str(1,tag.group);make.str(2,tag.label);make.done();
            find.reset();find.str(1,tag.group);find.str(2,tag.label);
            if(!find.next())throw std::runtime_error("MR Library tag unavailable");
            const auto tag_id=sqlite3_column_int64(find.stmt,0);find.reset();
            put.reset();put.number(1,item.id);put.number(2,tag_id);put.str(3,tag.origin);
            if(sqlite3_bind_double(put.stmt,4,tag.confidence)!=SQLITE_OK)fail(d.db,"Tag confidence");
            put.done();
        }
        typed.reset();typed.number(1,suggestion.metadata.bpm.value_or(0));
        typed.str(2,suggestion.metadata.key.value_or(""));typed.number(3,item.id);typed.done();
    }
}
bool allowed_group(const std::string& value) {
    for(const auto* name:{"Instrument","Genre","Type","Technique","Rhythm","Character","Custom"})
        if(value==name)return true;
    return false;
}
void valid_tag(const std::string& group,const std::string& label){
    if(!allowed_group(group)||label.empty()||label.size()>96)throw std::invalid_argument("Invalid tag name/group");
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
struct Cancelled : std::runtime_error { Cancelled() : std::runtime_error("Scan cancelled") {} };
} // namespace

struct SampleLibrary::Impl {
    enum class Kind { add, relink, remove, enable, rescan, view, tag, tag_status, bpm, key, rule, undo };
    struct Command {
        Kind kind;std::int64_t id{};fs::path root;bool enabled{true};
        std::vector<std::int64_t> ids;
        std::string group,label,status,phrase;
        int number{};bool folder{};
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
    std::int64_t viewSource{};
    std::string viewFilter;
    std::atomic<bool> cancel{false}, scanning{false};
    std::jthread worker;

    explicit Impl(fs::path path) : database(std::move(path)), worker([this](std::stop_token st) { run(st); }) {}
    ~Impl() {
        cancel = true;
        worker.request_stop();
        wake.notify_all();
        worker.join();
    }
    void enqueue(Command c) {
        { std::lock_guard lock(mutex); commands.push_back(std::move(c)); }
        if (scanning.load()) cancel = true; // newer commands supersede a long scan
        wake.notify_one();
    }
    std::shared_ptr<const Snapshot> snapshot() const {
        std::lock_guard lock(mutex);
        return state;
    }
    void publish(Database& d, Phase phase, std::string message = {}, std::uint64_t processed = 0, std::uint64_t changed = 0) {
        auto next = std::make_shared<Snapshot>();
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
        // All catalogue queries happen on the worker; the JUCE thread only reads snapshots.
        Statement assets(d,"SELECT id,source_id,relative_path,status,bpm_auto,bpm_manual,key_auto,key_manual"
            " FROM assets WHERE (?=0 OR source_id=?) AND instr(lower(relative_path),lower(?))>0"
            " ORDER BY relative_path,id LIMIT 300");
        assets.number(1,viewSource);assets.number(2,viewSource);assets.str(3,viewFilter);
        Statement tags(d,"SELECT t.group_name,t.label,at.origin,at.status,at.confidence"
            " FROM asset_tags at JOIN tags t ON t.id=at.tag_id WHERE at.asset_id=?"
            " ORDER BY t.group_name,t.label");
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
            tags.reset();tags.number(1,item.id);
            while(tags.next()){
                const auto field=[&](int index){const auto* p=sqlite3_column_text(tags.stmt,index);
                    return p?std::string(reinterpret_cast<const char*>(p)):std::string{};};
                item.tags.push_back({field(0),field(1),field(2),field(3),sqlite3_column_double(tags.stmt,4)});
            }
            next->assets.push_back(std::move(item));
        }
        Statement count(d,"SELECT COUNT(*) FROM assets WHERE (?=0 OR source_id=?)"
            " AND instr(lower(relative_path),lower(?))>0");
        count.number(1,viewSource);count.number(2,viewSource);count.str(3,viewFilter);
        if(count.next())next->visible_assets=static_cast<std::uint64_t>(sqlite3_column_int64(count.stmt,0));
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
            fs::recursive_directory_iterator it(root, fs::directory_options::none, ec), end;
            if (ec) throw std::runtime_error("Cannot enumerate library folder");
            for (; it != end;) {
                if (stop.stop_requested() || cancel.load()) throw Cancelled();
                auto current = it->path();
                if (is_reparse(current)) { it.disable_recursion_pending(); }
                else {
                    std::error_code type_error;
                    if (it->is_regular_file(type_error) && is_wav(current)) {
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
                    } else if (type_error) throw std::runtime_error("Cannot inspect library item");
                }
                it.increment(ec);
                if (ec) throw std::runtime_error("Cannot traverse library folder; scan rolled back");
            }
            if (stop.stop_requested() || cancel.load()) throw Cancelled();
            Statement missing(d, "UPDATE assets SET status='missing',error='Missing from latest scan' WHERE source_id=? AND seen=0");
            missing.number(1,source_id); missing.done();
            classify_assets(d,source_id); // proposals refresh, manual confirmed/rejected preserved
            d.exec("COMMIT");
        } catch (...) {
            d.exec("ROLLBACK"); // rollback keeps the previous complete catalogue
            throw;
        }
        publish(d,Phase::idle,"Scan complete",processed,changed);
    }
    void process(Database& d, const Command& c, std::stop_token stop) {
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
            publish(d,Phase::idle,"Source removed from index; files untouched");
        } else if (c.kind == Kind::enable) {
            Statement set(d,"UPDATE sources SET enabled=? WHERE id=?");
            set.number(1,c.enabled?1:0);set.number(2,c.id);set.done();
            publish(d,Phase::idle,"Source updated");
        } else if (c.kind == Kind::view) {
            viewSource=c.id;viewFilter=c.label.substr(0,200);
            publish(d,Phase::idle);
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
                classify_assets(d);
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
            db.exec("BEGIN IMMEDIATE");
            try { classify_assets(db);db.exec("COMMIT"); }
            catch (...){db.exec("ROLLBACK");throw;}
            publish(db,Phase::idle);
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
void SampleLibrary::undo_tag_edit(){impl_->enqueue(Impl::Command(Impl::Kind::undo));}
void SampleLibrary::rescan() { impl_->enqueue(Impl::Command(Impl::Kind::rescan)); }
void SampleLibrary::cancel_scan() { impl_->cancel=true; }
std::shared_ptr<const Snapshot> SampleLibrary::snapshot() const { return impl_->snapshot(); }
} // namespace mrs::library
