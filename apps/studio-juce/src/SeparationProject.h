#pragma once
#include "SeparationModels.h"
#include <mrs/core.hpp>
#include <mrs/audio.hpp>
#include <functional>
#include <filesystem>
#include <stop_token>

namespace mr::separation {
struct ProjectInput {
    mrs::Clip clip;
    std::filesystem::path source,projectFile;
    std::uint32_t rate{};
    juce::String session,revision,model;
    std::shared_ptr<const mrs::audio::AudioData> sourceLifetime;
    std::shared_ptr<void> sourceGuard;
};
struct ProjectMedia {
    juce::File directory;
    bool persistent{};
    std::atomic<bool> accepted{};
    ~ProjectMedia();
};
struct ProjectResult {
    mrs::Clip expected;
    mrs::SeparationResult record;
    std::shared_ptr<ProjectMedia> media;
};
struct ProjectPlacement {
    mrs::SeparationResult expected;
    std::vector<std::shared_ptr<const mrs::audio::AudioData>> audio;
};
struct ProjectOutcome {
    std::shared_ptr<ProjectResult> result;
    std::optional<ProjectPlacement> placement;
};
// One per Studio Desktop. Normal utility-window close only cancels; shutdown
// joins outside audio. Runner and all filesystem/preload work live on its thread.
class ProjectJob {
public:
    using Processor=std::function<Result(const ProjectInput&,std::stop_token)>;
    explicit ProjectJob(std::shared_ptr<ModelManager>,Processor = {});
    ~ProjectJob();
    bool separate(ProjectInput);
    bool preparePlacement(mrs::SeparationResult,std::vector<std::filesystem::path>,std::shared_ptr<void> guard);
    void cancel();
    Status status() const;
    std::optional<ProjectOutcome> take();
private:
    bool begin(std::function<ProjectOutcome(std::stop_token)>);
    Result process(const ProjectInput&,std::stop_token);
    void phase(juce::String);
    std::shared_ptr<ModelManager> manager_;
    Processor processor_;
    mutable std::mutex mutex_;
    Status status_;
    std::optional<ProjectOutcome> outcome_;
    std::stop_source stop_;
    std::thread thread_;
};
} // namespace mr::separation
