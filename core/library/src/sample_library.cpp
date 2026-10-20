#include <mrs/sample_library.hpp>
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
    if (v != 0 && v != 1) throw std::runtime_error("Unsupported MR Library database schema");
    if (v == 1) return;
    d.exec("BEGIN IMMEDIATE");
    try {
        d.exec("CREATE TABLE IF NOT EXISTS sources (id INTEGER PRIMARY KEY, path TEXT NOT NULL UNIQUE, enabled INTEGER NOT NULL DEFAULT 1)");
        d.exec("CREATE TABLE IF NOT EXISTS assets ("
               "id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,"
               "relative_path TEXT NOT NULL, bytes INTEGER NOT NULL, modified INTEGER NOT NULL,"
               "rate INTEGER, channels INTEGER, bits INTEGER, frames INTEGER,"
               "status TEXT NOT NULL, error TEXT NOT NULL DEFAULT '', seen INTEGER NOT NULL DEFAULT 0,"
               "UNIQUE(source_id,relative_path))");
        d.exec("CREATE INDEX IF NOT EXISTS assets_status_idx ON assets(source_id,status)");
        d.exec("PRAGMA user_version=1");
        d.exec("COMMIT");
    } catch (...) { d.exec("ROLLBACK"); throw; }
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
    enum class Kind { add, relink, remove, enable, rescan };
    struct Command { Kind kind; std::int64_t id{}; fs::path root; bool enabled{true}; };
    fs::path database;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<Command> commands;
    std::shared_ptr<const Snapshot> state{std::make_shared<Snapshot>()};
    std::uint64_t revision{};
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
                        if (existing.next())
                            unchanged = sqlite3_column_int64(existing.stmt,0)==bytes &&
                                sqlite3_column_int64(existing.stmt,1)==stamp &&
                                std::string(reinterpret_cast<const char*>(sqlite3_column_text(existing.stmt,2)))=="ok";
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
void SampleLibrary::remove_source(std::int64_t id) { impl_->enqueue({Impl::Kind::remove,id}); }
void SampleLibrary::enable_source(std::int64_t id,bool enabled) { impl_->enqueue({Impl::Kind::enable,id,{},enabled}); }
void SampleLibrary::rescan() { impl_->enqueue({Impl::Kind::rescan}); }
void SampleLibrary::cancel_scan() { impl_->cancel=true; }
std::shared_ptr<const Snapshot> SampleLibrary::snapshot() const { return impl_->snapshot(); }
} // namespace mrs::library
