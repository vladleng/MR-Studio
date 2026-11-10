#pragma once
#include <mrs/transcription_models.hpp>
#include <future>

namespace mrs::transcription {
enum class JobState { idle, running, cancelling, ready, empty, cancelled, timeout, failed, stale, placed };
struct JobStatus {JobState state{JobState::idle};int completed{},total{};std::string error;double elapsed_seconds{};unsigned cpu_threads{};};
// Control-thread owner; immutable request/media/lease only cross into its job.
// poll/cancel never wait for inference. Shutdown joins outside the audio callback.
class Controller {
public:
    Controller();
    ~Controller();
    Controller(const Controller&)=delete;
    Controller& operator=(const Controller&)=delete;
    void start(const std::shared_ptr<IProjectStore>&,std::filesystem::path project_path,
               Id clip,std::filesystem::path source,ModelStore,Request engine_settings,
               std::shared_ptr<const void> media = {});
    void poll(const std::shared_ptr<IProjectStore>&,const std::filesystem::path& project_path);
    void cancel();
    void shutdown();
    JobStatus status() const;
    const Result* preview() const;
    PlaceTranscription prepare(const std::shared_ptr<IProjectStore>&,const std::filesystem::path&,
                               const std::optional<std::vector<std::string>>& roles = {},bool original_timing = false);
    void placed();
private:
    struct Job;
    struct Stamp {std::uintmax_t bytes{};std::filesystem::file_time_type modified{};bool operator==(const Stamp&)const=default;};
    static Stamp stamp(const std::filesystem::path&);
    bool current(const std::shared_ptr<IProjectStore>&,const std::filesystem::path&) const;
    std::unique_ptr<Job> job_;
    std::weak_ptr<IProjectStore> owner_;
    ProjectState captured_;
    std::filesystem::path project_path_,source_;
    Id clip_;
    Stamp stamp_;
    JobStatus status_;
    std::optional<Result> result_;
};
}
