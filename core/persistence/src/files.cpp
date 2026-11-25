#include <mrs/persistence.hpp>
#include <mrs/drum_maps.hpp>
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace mrs::persistence {
namespace {
namespace fs = std::filesystem;
fs::path sibling(const fs::path& path, const char* suffix) { auto result = path; result += suffix; return result; }
std::string read(const fs::path& path, std::size_t limit = max_archive_bytes) {
    const auto n = fs::file_size(path);
    if (n > limit) throw std::runtime_error("file exceeds archive limit");
    std::ifstream stream(path,std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open archive");
    std::string bytes(static_cast<std::size_t>(n),'\0');
    if (n != 0) stream.read(bytes.data(),static_cast<std::streamsize>(n));
    if (!stream || stream.peek() != std::char_traits<char>::eof()) throw std::runtime_error("archive changed during read");
    return bytes;
}
[[noreturn]] void io_error(const char* operation) {
#ifdef _WIN32
    throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),operation);
#else
    throw std::system_error(errno,std::generic_category(),operation);
#endif
}
void synced_file(const fs::path& path, std::string_view bytes, bool& created) {
#ifdef _WIN32
    HANDLE handle = CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (handle == INVALID_HANDLE_VALUE) io_error("create temporary archive");
    created = true;
    try {
        std::size_t done{};
        while (done < bytes.size()) {
            DWORD written{};
            const auto count = static_cast<DWORD>(bytes.size() - done);
            if (!WriteFile(handle,bytes.data()+done,count,&written,nullptr)) io_error("write archive");
            if (written == 0) throw std::runtime_error("short archive write");
            done += written;
        }
        if (!FlushFileBuffers(handle)) io_error("flush archive");
    } catch (...) { CloseHandle(handle); throw; }
    if (!CloseHandle(handle)) io_error("close archive");
#else
    const int handle = ::open(path.c_str(),O_WRONLY | O_CREAT | O_EXCL,0600);
    if (handle < 0) io_error("create temporary archive");
    created = true;
    try {
        std::size_t done{};
        while (done < bytes.size()) {
            auto written = ::write(handle,bytes.data()+done,bytes.size()-done);
            if (written < 0) { if (errno == EINTR) continue; io_error("write archive"); }
            if (written == 0) throw std::runtime_error("short archive write");
            done += static_cast<std::size_t>(written);
        }
        if (::fsync(handle) != 0) io_error("flush archive");
    } catch (...) { ::close(handle); throw; }
    if (::close(handle) != 0) io_error("close archive");
#endif
}
void replace(const fs::path& temp, const fs::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) io_error("replace archive");
#else
    if (::rename(temp.c_str(),destination.c_str()) != 0) io_error("replace archive");
    auto parent = destination.parent_path(); if (parent.empty()) parent = ".";
    const int directory = ::open(parent.c_str(),O_RDONLY | O_DIRECTORY);
    if (directory < 0) io_error("open archive directory");
    const int result = ::fsync(directory); const int saved = errno; ::close(directory);
    if (result != 0) { errno = saved; io_error("flush archive directory"); }
#endif
}
struct Temporary {
    fs::path path;
    bool created{};
    explicit Temporary(const fs::path& destination) : path(destination.parent_path() / ("mrs-write-" + new_id().value + ".tmp")) {}
    ~Temporary() { if (created) { std::error_code error; fs::remove(path,error); } }
};
void atomic(const fs::path& destination, std::string_view bytes) {
    Temporary temp(destination); synced_file(temp.path,bytes,temp.created); replace(temp.path,destination);
}
void atomic_create(const fs::path& destination, std::string_view bytes) {
    Temporary temp(destination); synced_file(temp.path, bytes, temp.created);
#ifdef _WIN32
    if (!MoveFileExW(temp.path.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) io_error("create new drum map (existing files are not overwritten)");
#else
    if (::link(temp.path.c_str(), destination.c_str()) != 0) io_error("create new drum map (existing files are not overwritten)");
#endif
}
template<class Validate> void save(const fs::path& path, std::string_view bytes, Validate validate, const SaveHook& hook) {
    if (path.filename().empty()) throw std::invalid_argument("archive path requires a filename");
    // Refuse to overwrite a damaged primary or silently destroy a valid backup.
    std::optional<std::string> previous;
    if (fs::exists(path)) { previous = read(path); validate(*previous); }
    Temporary temp(path); synced_file(temp.path,bytes,temp.created);
    if (hook) hook(SavePoint::temporary_synced);
    if (previous) atomic(sibling(path,".bak"),*previous);
    if (hook) hook(SavePoint::backup_synced);
    if (hook) hook(SavePoint::before_replace);
    replace(temp.path,path);
}
fs::path drum_map_path(const fs::path& library, const Id& id) {
    if (library.empty() || id.value.empty() || id.value.size() > 64 || id.value.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid drum map library or ID");
    // Encode IDs reversibly; a profile ID is never a path and cannot alias on case-insensitive filesystems.
    constexpr std::string_view hex = "0123456789abcdef";
    std::string filename;
    for (const auto ch : id.value) {
        const auto byte = static_cast<unsigned char>(ch);
        filename += hex[byte >> 4];
        filename += hex[byte & 15];
    }
    return library / (filename + ".mrsdrummap");
}
DrumMapProfile read_drum_map(const Id& id, std::string_view bytes) {
    auto profile = deserialize_drum_map(bytes);
    if (profile.id != id) throw std::invalid_argument("drum map library identity mismatch");
    return profile;
}
} // namespace
void save_drum_map(const fs::path& library, const DrumMapProfile& profile, bool replace_existing) {
    const auto bytes = serialize_drum_map(profile);
    const auto path = drum_map_path(library, profile.id);
    fs::create_directories(library);
    if (!replace_existing) { atomic_create(path, bytes); return; }
    save(path, bytes, [&](std::string_view previous) { (void)read_drum_map(profile.id, previous); }, {});
}
std::optional<DrumMapProfile> load_drum_map(const fs::path& library, const Id& id) {
    const auto path = drum_map_path(library, id);
    if (!fs::exists(path)) return std::nullopt;
    return read_drum_map(id, read(path, max_drum_map_bytes));
}
DrumMapCatalog list_drum_maps(const fs::path& library) {
    DrumMapCatalog result;
    if (library.empty()) throw std::invalid_argument("invalid drum map library");
    if (!fs::exists(library)) return result;
    for (const auto& entry : fs::directory_iterator(library)) {
        if (entry.path().extension() != ".mrsdrummap") continue;
        try {
            auto profile = deserialize_drum_map(read(entry.path(), max_drum_map_bytes));
            if (drum_map_path(library, profile.id) != entry.path()) throw std::invalid_argument("library ID mismatch");
            result.entries.push_back({std::move(profile.id), std::move(profile.name)});
        } catch (const std::invalid_argument& e) {
            result.warnings.emplace_back(e.what());
        } catch (const std::runtime_error& e) {
            result.warnings.emplace_back(e.what());
        }
    }
    std::sort(result.entries.begin(), result.entries.end(), [](const auto& a, const auto& b) {
        return a.name != b.name ? a.name < b.name : a.id.value < b.id.value;
    });
    return result;
}
void delete_drum_map(const fs::path& library, const Id& id) {
    if (!load_drum_map(library, id)) throw std::invalid_argument("drum map no longer exists");
    const auto path = drum_map_path(library, id);
    fs::remove(sibling(path, ".bak"));
    if (!fs::remove(path)) throw std::runtime_error("drum map no longer exists");
}
DrumMapProfile import_drum_map_file(const fs::path& file) {
    auto extension = file.extension().string();
    for (auto& ch : extension) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    if (extension == ".mrsdrummap") return deserialize_drum_map(read(file, max_drum_map_bytes));
    if (extension == ".csv" || extension == ".tsv") {
        const auto name = file.stem().u8string();
        return import_drum_map_table(read(file, max_drum_map_bytes), extension == ".csv" ? ',' : '\t',
                                     std::string(name.begin(), name.end()));
    }
    throw std::invalid_argument("Choose an MR Drum Map, CSV or TSV. Use the PDF as a manual reference.");
}
void export_drum_map_file(const fs::path& file, const DrumMapProfile& profile) {
    if (file.extension() != ".mrsdrummap" || fs::exists(file))
        throw std::invalid_argument("Choose a new .mrsdrummap filename; existing files are not overwritten.");
    atomic_create(file, serialize_drum_map(profile));
}
ResolvedDrumMap resolve_drum_map(const fs::path& library, const DrumMapBinding& binding) {
    binding.validate();
    try {
        if (auto profile = load_drum_map(library, binding.snapshot.id))
            return {std::move(*profile), DrumMapSource::library};
    } catch (const std::invalid_argument&) {
        return {binding.snapshot, DrumMapSource::snapshot_unreadable};
    } catch (const std::runtime_error&) {
        return {binding.snapshot, DrumMapSource::snapshot_unreadable};
    }
    return {binding.snapshot, DrumMapSource::snapshot_missing};
}
void save_project(const fs::path& path, const ProjectDocument& d, SaveHook hook) {
    auto bytes = encode(d); save(path,bytes,[&](std::string_view v) {
        const auto old = decode_project(v);
        if (old.project.id != d.project.id || old.generation > d.generation) throw std::invalid_argument("project identity/generation mismatch");
    },hook);
}
void save_show(const fs::path& path, const ShowDocument& d, SaveHook hook) {
    auto bytes = encode(d); save(path,bytes,[&](std::string_view v) {
        const auto old = decode_show(v);
        if (old.id != d.id || old.generation > d.generation) throw std::invalid_argument("show identity/generation mismatch");
    },hook);
}
ProjectDocument load_project(const fs::path& path) { return decode_project(read(path)); }
ShowDocument load_show(const fs::path& path) { return decode_show(read(path)); }
void save_autosave(const fs::path& project_path, const ProjectDocument& d) {
    auto bytes = encode(d); const auto target = sibling(project_path,".autosave");
    for (const auto& candidate : {project_path,target}) {
        if (!fs::exists(candidate)) continue;
        std::optional<ProjectDocument> old;
        try { old = decode_project(read(candidate)); } catch (const std::invalid_argument&) { /* corrupt autosave can be repaired */ }
        if (old && (old->project.id != d.project.id || old->generation > d.generation)) throw std::invalid_argument("autosave identity/generation mismatch");
    }
    atomic(target,bytes);
}
Recovery recover_project(const fs::path& path) {
    Recovery result; bool found{};
    for (const auto& candidate : {path,sibling(path,".autosave"),sibling(path,".bak")}) {
        if (!fs::exists(candidate)) continue;
        try {
            auto document = load_project(candidate);
            if (found && document.project.id != result.document.project.id) throw std::invalid_argument("recovery candidate belongs to another project");
            if (!found || document.generation > result.document.generation) {
                result.document = std::move(document); result.source = candidate; found = true;
            }
        } catch (const std::exception& e) { result.warnings.push_back(std::string(e.what())); }
    }
    if (!found) throw std::runtime_error("no valid project, autosave or backup available");
    return result;
}
AutosaveWorker::AutosaveWorker(fs::path path) : path_(std::move(path)), thread_([this] { run(); }) {}
AutosaveWorker::~AutosaveWorker() {
    { std::lock_guard lock(mutex_); stopping_ = true; }
    cv_.notify_all(); thread_.join();
}
std::uint64_t AutosaveWorker::submit(std::shared_ptr<const ProjectDocument> document) {
    if (!document) throw std::invalid_argument("null autosave snapshot");
    std::lock_guard lock(mutex_);
    if (stopping_ || submitted_ == std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("autosave worker stopped/exhausted");
    if (submitted_ != 0 && document->generation < last_generation_) throw std::invalid_argument("autosave generation moved backwards");
    last_generation_ = document->generation;
    pending_ = std::move(document); const auto ticket = ++submitted_; cv_.notify_all(); return ticket;
}
std::string AutosaveWorker::wait(std::uint64_t ticket) {
    std::unique_lock lock(mutex_);
    if (ticket == 0 || ticket > submitted_) throw std::invalid_argument("unknown autosave ticket");
    cv_.wait(lock,[&] { return completed_ >= ticket; }); return error_;
}
void AutosaveWorker::run() {
    for (;;) {
        std::shared_ptr<const ProjectDocument> document; std::uint64_t ticket{};
        {
            std::unique_lock lock(mutex_); cv_.wait(lock,[&] { return stopping_ || pending_; });
            if (!pending_) return;
            document = std::move(pending_); ticket = submitted_;
        }
        std::string error;
        try { save_autosave(path_,*document); }
        catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "unknown autosave failure"; }
        { std::lock_guard lock(mutex_); completed_ = ticket; error_ = std::move(error); }
        cv_.notify_all();
    }
}
} // namespace mrs::persistence
