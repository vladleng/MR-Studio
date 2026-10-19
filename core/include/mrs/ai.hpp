#pragma once
#include <mrs/core.hpp>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

// NON-RT only. No engine/plugin pointers or device access in this API.
namespace mrs::ai {
inline constexpr std::uint32_t contract_version = 1;
enum class Error { none, invalid_input, unsupported, stale_context, busy, cancelled,
                   timeout, response_too_large, backend_failure, shutdown, not_found, permission_denied };
struct ContextKey {
    Id project, session; // session changes even when reopening the same saved project
    std::uint64_t project_revision{}, graph_revision{};
    bool operator==(const ContextKey&) const = default;
};
enum class EntityKind { track, folder, clip, chord, section, marker };
struct EntityRef {
    EntityKind kind{};
    Id id;
    bool operator==(const EntityRef&) const = default;
};
struct Selection {
    std::vector<EntityRef> entities;
    std::optional<LoopRange> sample_range; // [start,end), never inferred from UI
    bool operator==(const Selection&) const = default;
};
enum class Capability { summary, timing, selection, entity_queries, tools, analysis, live_performance, snapshots };
struct Context {
    std::uint32_t version{contract_version}; // independent from project archive/schema
    ContextKey key;
    std::string title, artist;
    std::uint32_t sample_rate{}, ticks_per_quarter{static_cast<std::uint32_t>(ppq)};
    TimeMap time;
    TransportState observed_transport; // observation, not part of revision identity
    Selection selection; // transient; not persisted or added to history
    std::size_t tracks{}, folders{}, clips{}, chords{}, sections{}, markers{};
    bool supports(Capability) const noexcept;
    double seconds(Sample) const;
};
struct ContextResult {
    Error error{Error::none};
    std::optional<Context> value;
};
// Caller owns the control thread. Copy values; do not capture live services in a worker.
ContextResult make_context(const ProjectState&, const ContextKey&, TransportState, Selection = {});

struct Limits {
    std::size_t outstanding{8}, input_bytes{1024*1024}, output_bytes{1024*1024};
};
struct JobControl {
    std::stop_token stop;
    std::chrono::steady_clock::time_point deadline;
    std::size_t output_bytes{}; // backend must stop receiving at this byte budget
    bool cancelled() const noexcept { return stop.stop_requested(); }
    bool timed_out() const noexcept { return std::chrono::steady_clock::now() >= deadline; }
    bool should_stop() const noexcept { return cancelled() || timed_out(); }
};
class Snapshot;
struct Request {
    Context context;
    std::string input;
    std::chrono::milliseconds timeout{30000};
    std::shared_ptr<const Snapshot> snapshot{};
    std::uint64_t policy_revision{}; // host-only, never in exported snapshot/provider JSON
};
struct Result {
    std::uint64_t request_id{};
    ContextKey key;
    Error error{Error::none};
    std::string output;
    std::uint64_t policy_revision{};
};
struct Submitted { Error error{Error::none}; std::uint64_t request_id{}; };
// Exactly one backend invocation. Backend must honor JobControl and bound its reads.
// Network adapter must supply finite interruptible I/O; arbitrary uncooperative
// C++ calls cannot be safely terminated. No detach, retries, or UI callbacks.
using Task = std::function<std::string(const Request&, const JobControl&)>;
class Worker {
public:
    explicit Worker(Limits = {});
    ~Worker();
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    Submitted submit(Request, Task);
    bool cancel(std::uint64_t);
    void cancel_all();
    std::optional<Result> take(); // control/UI thread drains; no backend callbacks
    void shutdown(); // owner thread only; joins outside audio thread, never inside Task
private:
    struct Job {
        std::uint64_t id{};
        Request request;
        Task task;
        std::stop_source cancel;
        std::chrono::steady_clock::time_point deadline;
    };
    Limits limits_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> pending_;
    std::deque<Result> results_;
    std::map<std::uint64_t,std::stop_source> outstanding_;
    bool stopping_{};
    std::uint64_t next_{1};
    std::thread thread_;
    void run();
};
} // namespace mrs::ai
