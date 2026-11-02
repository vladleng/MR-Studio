#include "SeparationRunner.h"
#include <juce_cryptography/juce_cryptography.h>
#include <mrs/audio.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mr::separation {
namespace {
using Clock = std::chrono::steady_clock;
struct Failure { juce::String code; };
void require(bool condition, const char* error) { if (!condition) throw Failure{error}; }
juce::var object() { return juce::var(new juce::DynamicObject); }
void put(juce::var& v, const char* key, juce::var value) { v.getDynamicObject()->setProperty(key, std::move(value)); }
juce::String hash(const juce::File& file) {
    auto stream = file.createInputStream();
    require(stream && stream->openedOk(), "io_error");
    return juce::SHA256(*stream).toHexString();
}
bool plainFile(const juce::File& file) {
    const auto attributes = GetFileAttributesW(file.getFullPathName().toWideCharPointer());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}
struct Handle {
    HANDLE value{};
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    void close() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); value = nullptr; }
};
struct Child {
    Handle job, process, input, output, errorOutput;
    std::string diagnostics;
    std::uint64_t logRevision{};
    ~Child() {
        if (job.value) TerminateJobObject(job.value, 1);
        if (process.value) WaitForSingleObject(process.value, 5000); // release file handles before cleanup
    }
    void launch(const Configuration& config, const juce::File& work, juce::uint64 memory) {
        launch(config.python, {"-I", "-B", "-u", config.workerScript.getFullPathName()}, work, memory, 1);
    }
    void launch(const juce::File& executable, const juce::StringArray& arguments,
                const juce::File& work, juce::uint64 memory, unsigned processLimit) {
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        Handle childIn, childOut, childError;
        require(CreatePipe(&childIn.value, &input.value, &security, 65536) != FALSE, "worker_start_failed");
        require(CreatePipe(&output.value, &childOut.value, &security, 65536) != FALSE, "worker_start_failed");
        require(CreatePipe(&errorOutput.value, &childError.value, &security, 65536) != FALSE, "worker_start_failed");
        require(SetHandleInformation(input.value, HANDLE_FLAG_INHERIT, 0) != FALSE &&
                SetHandleInformation(output.value, HANDLE_FLAG_INHERIT, 0) != FALSE &&
                SetHandleInformation(errorOutput.value, HANDLE_FLAG_INHERIT, 0) != FALSE, "worker_start_failed");
        job.value = CreateJobObjectW(nullptr, nullptr);
        require(job.value != nullptr, "worker_start_failed");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
            JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_JOB_MEMORY;
        limits.BasicLimitInformation.ActiveProcessLimit = processLimit;
        limits.ProcessMemoryLimit = static_cast<SIZE_T>(memory);
        limits.JobMemoryLimit = static_cast<SIZE_T>(memory);
        require(SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE,
                "worker_start_failed");
        SIZE_T bytes{};
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<unsigned char> storage(bytes);
        auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        require(InitializeProcThreadAttributeList(attributes, 1, 0, &bytes) != FALSE, "worker_start_failed");
        struct Cleanup { LPPROC_THREAD_ATTRIBUTE_LIST p; ~Cleanup() { DeleteProcThreadAttributeList(p); } } cleanup{attributes};
        HANDLE inherited[]{childIn.value, childOut.value, childError.value};
        require(UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                         inherited, sizeof(inherited), nullptr, nullptr) != FALSE, "worker_start_failed");
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = childIn.value;
        startup.StartupInfo.hStdOutput = childOut.value;
        startup.StartupInfo.hStdError = childError.value;
        startup.lpAttributeList = attributes;
        // File paths cannot contain quotes on Windows. No shell / command interpolation.
        auto command = executable.getFullPathName().quoted();
        for (const auto& argument : arguments) {
            require(!argument.containsChar('"'), "invalid_input");
            command += " " + argument.quoted();
        }
        std::wstring mutableCommand(command.toWideCharPointer());
        PROCESS_INFORMATION info{};
        require(CreateProcessW(executable.getFullPathName().toWideCharPointer(), mutableCommand.data(), nullptr, nullptr,
                               TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
                               nullptr, work.getFullPathName().toWideCharPointer(), &startup.StartupInfo, &info) != FALSE,
                "python_missing");
        process.value = info.hProcess;
        Handle thread{info.hThread};
        if (!AssignProcessToJobObject(job.value, process.value) || ResumeThread(thread.value) == static_cast<DWORD>(-1)) {
            TerminateProcess(process.value, 1);
            throw Failure{"worker_start_failed"};
        }
    }
    void send(const juce::var& message) {
        const auto json = juce::JSON::toString(message, true);
        const auto size = json.getNumBytesAsUTF8();
        require(size > 0 && size <= 65536, "protocol_error");
        std::vector<unsigned char> buffer(4 + size);
        for (unsigned i = 0; i < 4; ++i) buffer[i] = static_cast<unsigned char>((size >> (8 * i)) & 255);
        std::copy_n(reinterpret_cast<const unsigned char*>(json.toRawUTF8()), size, buffer.begin() + 4);
        DWORD written{};
        require(WriteFile(input.value, buffer.data(), static_cast<DWORD>(buffer.size()), &written, nullptr) != FALSE &&
                written == buffer.size(), "protocol_error");
    }
    bool read(std::vector<unsigned char>& buffer) {
        DWORD errorBytes{};
        if (PeekNamedPipe(errorOutput.value, nullptr, 0, nullptr, &errorBytes, nullptr) && errorBytes) {
            std::array<char, 65536> chunk{};
            DWORD count{};
            if (ReadFile(errorOutput.value, chunk.data(), std::min<DWORD>(errorBytes, static_cast<DWORD>(chunk.size())), &count, nullptr)) {
                diagnostics.append(chunk.data(), count);
                if (diagnostics.size() > 256 * 1024) diagnostics.erase(0, diagnostics.size() - 256 * 1024);
                ++logRevision;
            }
        }
        DWORD available{};
        if (!PeekNamedPipe(output.value, nullptr, 0, nullptr, &available, nullptr)) return false;
        if (available) {
            // Bound aggregate traffic even if a worker floods without frame boundaries.
            require(buffer.size() + available <= 131072, "protocol_error");
            const auto old = buffer.size();
            buffer.resize(old + available);
            DWORD count{};
            require(ReadFile(output.value, buffer.data() + old, available, &count, nullptr) != FALSE, "protocol_error");
            buffer.resize(old + count);
        }
        return available || WaitForSingleObject(process.value, 0) == WAIT_TIMEOUT;
    }
};
juce::int64 number(const juce::var& value) {
    const auto s = value.toString();
    require(value.isString() && s.isNotEmpty() && s.length() <= 18 && s.containsOnly("0123456789") &&
            (s == "0" || !s.startsWithChar('0')), "invalid_output");
    return s.getLargeIntValue();
}
void verify(const juce::var& manifest, const Request& request, const juce::String& jobId,
            const juce::String& sourceHash, const juce::File& result, const std::atomic<bool>& stop) {
    require(manifest.isObject() && static_cast<int>(manifest["version"]) == 1 &&
            manifest["job_id"].toString() == jobId && manifest["session_id"].toString() == request.sessionId &&
            manifest["source_revision"].toString() == request.sourceRevision &&
            manifest["source_sha256"].toString() == sourceHash &&
            manifest["backend_version"].toString() == "0.48.0" &&
            manifest["model"].toString() == "official-htdemucs-derived-2-v1", "invalid_output");
    require(number(manifest["timeline_start"]) == request.timelineStart &&
            number(manifest["source_start"]) == request.sourceStart &&
            number(manifest["source_frames"]) == request.sourceFrames &&
            number(manifest["project_rate"]) == request.projectRate, "invalid_output");
    const auto sourceRate = number(manifest["source_rate"]);
    const auto sourceInfo = mrs::audio::inspect_wav(std::filesystem::path(request.source.getFullPathName().toWideCharPointer()));
    require(sourceRate == sourceInfo.sample_rate && sourceInfo.channels == 2 &&
            request.sourceStart <= sourceInfo.frame_count && request.sourceFrames <= sourceInfo.frame_count - request.sourceStart,
            "invalid_output");
    const auto expected = (request.sourceFrames * request.projectRate + sourceRate / 2) / sourceRate;
    const auto* outputs = manifest["outputs"].getArray();
    require(outputs && outputs->size() == 2 && result.isDirectory() && plainFile(result), "invalid_output");
    int seen{};
    for (const auto& output : *outputs) {
        const auto stem = output["stem"].toString();
        const int bit = stem == "vocals" ? 1 : stem == "instrumental" ? 2 : 0;
        require(bit && !(seen & bit), "invalid_output"); seen |= bit;
        require(output["path"].toString() == stem + ".wav" && output["format"].toString() == "WAV_FLOAT32" &&
                number(output["frames"]) == expected && number(output["rate"]) == request.projectRate &&
                number(output["origin"]) == 0 && static_cast<int>(output["channels"]) == 2, "invalid_output");
        const auto file = result.getChildFile(stem + ".wav");
        require(file.existsAsFile() && plainFile(file) && file.getSize() <= 512LL * 1024 * 1024 &&
                file.getSize() == number(output["bytes"]) && hash(file) == output["sha256"].toString(), "integrity_failure");
        const auto wav = mrs::audio::inspect_wav(std::filesystem::path(file.getFullPathName().toWideCharPointer()));
        require(wav.format == 3 && wav.bits == 32 && wav.channels == 2 && wav.frame_count == expected &&
                wav.sample_rate == static_cast<unsigned>(request.projectRate), "invalid_output");
        std::array<float, 8192> data{};
        for (mrs::Sample at = 0; at < wav.frame_count;) {
            require(!stop.load(), "cancelled");
            const auto count = static_cast<std::size_t>(std::min<mrs::Sample>(4096, wav.frame_count - at));
            wav.read(at, std::span<float>(data.data(), count * 2));
            require(std::all_of(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(count * 2),
                                [](float sample) { return std::isfinite(sample); }), "invalid_output");
            at += static_cast<mrs::Sample>(count);
        }
    }
}
} // namespace

ProcessResult runLocalProcess(const juce::File& executable, const juce::StringArray& arguments,
                             const juce::File& workingDirectory, const std::atomic<bool>& cancel,
                             std::chrono::milliseconds timeout, unsigned processLimit) {
    ProcessResult result;
    try {
        require(processLimit > 0 && processLimit <= 8, "invalid_input");
        Child child;
        child.launch(executable, arguments, workingDirectory, 8ULL * 1024 * 1024 * 1024, processLimit);
        child.input.close(); // pip/probes must never prompt or wait for stdin
        const auto deadline = Clock::now() + timeout;
        std::vector<unsigned char> bytes;
        std::string output;
        for (;;) {
            require(!cancel.load(), "cancelled");
            require(Clock::now() < deadline, "timeout");
            child.read(bytes);
            if (!bytes.empty()) output.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            bytes.clear();
            if (output.size() > 256 * 1024) output.erase(0, output.size() - 256 * 1024);
            result.log = juce::String::fromUTF8(output.data(), static_cast<int>(output.size())) +
                         juce::String::fromUTF8(child.diagnostics.data(), static_cast<int>(child.diagnostics.size()));
            result.log = result.log.substring(std::max(0, result.log.length() - 256 * 1024));
            DWORD code{};
            require(GetExitCodeProcess(child.process.value, &code) != FALSE, "worker_crash");
            if (code != STILL_ACTIVE) {
                result.exitCode = static_cast<juce::int64>(code);
                result.success = code == 0;
                if (!result.success) result.error = "process_failed";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    } catch (const Failure& error) { result.error = error.code; }
      catch (...) { result.error = "process_failed"; }
    return result;
}

Runner::Runner(Configuration config) : config_(std::move(config)) {}
Runner::~Runner() { cancel(); if (thread_.joinable()) thread_.join(); }
bool Runner::start(Request request) {
    { std::lock_guard lock(mutex_); if (status_.state == State::running || result_) return false; }
    if (thread_.joinable()) thread_.join();
    cancelled_.store(false);
    const auto id = juce::Uuid().toString();
    { std::lock_guard lock(mutex_); status_ = {State::running, id, "queued", {}, {}}; diagnostics_.clear(); }
    try {
        thread_ = std::thread([this, request = std::move(request), id]() mutable { run(std::move(request), id); });
    } catch (...) {
        std::lock_guard lock(mutex_); status_.state = State::failed; status_.error = "resource_limit";
        return false;
    }
    return true;
}
void Runner::cancel() noexcept { cancelled_.store(true); }
Status Runner::status() const { std::lock_guard lock(mutex_); return status_; }
juce::String Runner::diagnostics() const { std::lock_guard lock(mutex_); return diagnostics_; }
std::optional<Result> Runner::take() { std::lock_guard lock(mutex_); auto result = std::move(result_); result_.reset(); return result; }
void Runner::run(Request request, juce::String jobId) noexcept {
    const auto work = config_.temporaryRoot.getChildFile("mrs-separation-" + jobId);
    bool preserve{};
    bool ownsWork{};
    std::optional<Result> completed;
    const auto began = Clock::now();
    try {
        require(config_.python.existsAsFile(), "python_missing");
        require(config_.workerScript.existsAsFile(), "worker_missing");
        require(request.source.existsAsFile() && request.source.getSize() <= 512LL * 1024 * 1024 &&
                request.sourceFrames > 0 && request.sourceFrames <= 96000LL * 600 && request.sourceStart >= 0 &&
                request.timelineStart >= 0 && request.projectRate >= 8000 && request.projectRate <= 96000 &&
                request.memoryBytes >= 256ULL * 1024 * 1024 && request.memoryBytes <= 16ULL * 1024 * 1024 * 1024 &&
                request.timeout.count() >= 100 && request.timeout <= std::chrono::hours(24) &&
                request.sessionId.isNotEmpty() && request.sessionId.length() <= 128 &&
                request.sourceRevision.isNotEmpty() && request.sourceRevision.length() <= 128, "invalid_input");
        require(config_.temporaryRoot.isDirectory() &&
                CreateDirectoryW(work.getFullPathName().toWideCharPointer(), nullptr) != FALSE, "io_error");
        ownsWork = true;
        require(plainFile(work), "io_error");
        const auto sourceHash = hash(request.source);
        require(!cancelled_.load(), "cancelled");
        Child child;
        child.launch(config_, work, request.memoryBytes);
        auto message = object();
        put(message, "version", 1); put(message, "type", "submit"); put(message, "job_id", jobId);
        put(message, "session_id", request.sessionId); put(message, "source_revision", request.sourceRevision);
        put(message, "source_path", request.source.getFullPathName()); put(message, "source_sha256", sourceHash);
        put(message, "source_start", juce::String(request.sourceStart)); put(message, "source_frames", juce::String(request.sourceFrames));
        put(message, "timeline_start", juce::String(request.timelineStart)); put(message, "project_rate", juce::String(request.projectRate));
        put(message, "model_dir", request.modelDirectory.getFullPathName());
        put(message, "device", request.device == Device::cpu ? "cpu" : request.device == Device::cuda ? "cuda" : "auto");
        put(message, "memory_bytes", juce::String(request.memoryBytes)); put(message, "timeout_ms", juce::String(request.timeout.count()));
        put(message, "threads", juce::String(std::max(1, std::min(4, static_cast<int>(std::thread::hardware_concurrency()) - 2))));
        child.send(message);
        std::vector<unsigned char> bytes;
        bool hello{}, cancelSent{};
        auto cancelAt = Clock::now();
        auto lastEvent = Clock::now();
        juce::int64 sequence{};
        std::uint64_t logRevision{};
        while (true) {
            const auto now = Clock::now();
            if (now - began > request.timeout) throw Failure{"timeout"};
            if (cancelled_.load() && !cancelSent) {
                auto cancelMessage = object(); put(cancelMessage, "type", "cancel"); put(cancelMessage, "job_id", jobId);
                child.send(cancelMessage); cancelSent = true; cancelAt = now;
            }
            if (cancelSent && now - cancelAt > std::chrono::seconds(5)) throw Failure{"cancelled"};
            require(now - lastEvent < std::chrono::seconds(30), "worker_unresponsive");
            const bool alive = child.read(bytes);
            if (logRevision != child.logRevision) {
                std::lock_guard lock(mutex_);
                diagnostics_ = juce::String::fromUTF8(child.diagnostics.data(), static_cast<int>(child.diagnostics.size()));
                logRevision = child.logRevision;
            }
            while (bytes.size() >= 4) {
                std::uint32_t size{};
                for (unsigned i = 0; i < 4; ++i) size |= static_cast<std::uint32_t>(bytes[i]) << (8 * i);
                require(size > 0 && size <= 65536, "protocol_error");
                if (bytes.size() < 4 + size) break;
                juce::var event;
                require(juce::JSON::parse(juce::String::fromUTF8(reinterpret_cast<const char*>(bytes.data() + 4), static_cast<int>(size)), event).wasOk() && event.isObject(), "protocol_error");
                bytes.erase(bytes.begin(), bytes.begin() + 4 + size);
                require(static_cast<int>(event["version"]) == 1 && number(event["sequence"]) > sequence, "protocol_error");
                sequence = number(event["sequence"]); lastEvent = now;
                const auto type = event["type"].toString();
                if (!hello) { require(type == "hello", "protocol_error"); hello = true; continue; }
                require(event["job_id"].toString() == jobId, "protocol_error");
                if (type == "error") throw Failure{cancelled_.load() ? "cancelled" : event["code"].toString().substring(0, 64)};
                if (type == "result") {
                    require(!cancelled_.load(), "cancelled");
                    const auto resultDirectory = work.getChildFile("result");
                    auto manifest = event["manifest"];
                    verify(manifest, request, jobId, sourceHash, resultDirectory, cancelled_);
                    require(hash(request.source) == sourceHash, "stale_source");
                    require(!cancelled_.load() && Clock::now() - began <= request.timeout, "cancelled");
                    const auto manifestFile = resultDirectory.getChildFile("manifest.json");
                    require(manifestFile.existsAsFile() && manifestFile.getSize() <= 49152 && plainFile(manifestFile) &&
                            juce::JSON::toString(juce::JSON::parse(manifestFile), true) == juce::JSON::toString(manifest, true), "invalid_output");
                    // Only verified outputs survive. All transient input/native files are discarded.
                    work.getChildFile("snapshot.wav").deleteFile();
                    work.getChildFile("native").deleteRecursively();
                    require(!cancelled_.load(), "cancelled");
                    completed = Result{jobId, request.sessionId, request.sourceRevision, resultDirectory, manifest};
                    preserve = true;
                    break;
                }
                require(type == "progress" || type == "heartbeat", "protocol_error");
                if (type == "progress") {
                    const auto phase = event["phase"].toString();
                    require(phase == "validating" || phase == "import_numpy" || phase == "import_soundfile" || phase == "import_scipy" ||
                            phase == "preparing" || phase == "loading" || phase == "inference" || phase == "verifying", "protocol_error");
                    std::lock_guard lock(mutex_); status_.phase = phase;
                    status_.effectiveDevice = event["device"].toString();
                }
            }
            if (preserve) break;
            require(alive, cancelled_.load() ? "cancelled" : "worker_crash");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } catch (const Failure& failure) {
        std::lock_guard lock(mutex_);
        status_.error = failure.code; status_.phase = "terminal";
        status_.state = cancelled_.load() || failure.code == "cancelled" ? State::cancelled : State::failed;
    } catch (...) {
        std::lock_guard lock(mutex_); status_.error = "backend_failure"; status_.state = State::failed;
    }
    // Child has exited and released its cwd/files before exposing cache ownership.
    if (preserve) {
        work.getChildFile("numba-cache").deleteRecursively();
        std::lock_guard lock(mutex_);
        if (cancelled_.load()) {
            preserve = false; status_.state = State::cancelled; status_.error = "cancelled";
        } else {
            result_ = std::move(completed);
            status_.state = State::ready; status_.phase = "ready";
            status_.effectiveDevice = result_->manifest["effective_device"].toString();
        }
    }
    if (ownsWork && !preserve) work.deleteRecursively();
}
} // namespace mr::separation
