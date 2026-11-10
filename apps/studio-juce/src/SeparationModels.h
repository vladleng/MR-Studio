#pragma once
#include "SeparationRunner.h"
#include <functional>
#include <memory>
#include <vector>

namespace mr::separation {
struct DownloadArtifact {
    juce::String name, url, sha256;
    juce::int64 bytes{};
};
// Generic installation primitives; callers keep their own roots/identities.
void fetchRuntimeArtifact(const DownloadArtifact&,const juce::File&,const std::atomic<bool>&);
void publishRuntimeInventory(const juce::File&,const juce::String&,const juce::var&,const std::atomic<bool>&);
void verifyRuntimeInventory(const juce::File&,const juce::String&,const std::atomic<bool>&);
struct ModelRecord {
    juce::String id, version, title, backend, outputs, nativeOutputs, hardware;
    juce::String codeLicense, weightsLicense, provenance, unsupportedReason;
    std::vector<DownloadArtifact> files;
    juce::String modelFile, architecture;
    juce::StringArray stems, nativeStems, chunks, defaultStems;
};
const std::vector<ModelRecord>& modelCatalogue();
const ModelRecord& modelProfile(const juce::String& id);
juce::String profileCatalogueJson();
juce::StringArray selectedOutputs(const ModelRecord&,const juce::StringArray&);
inline constexpr const char* runtimeProfile = "cpu-py3119-audiosep048-v1";
enum class InstallationState { downloadable, installed, corrupt, unsupported };
struct ModelState { ModelRecord record; InstallationState state{InstallationState::downloadable}; juce::String reason; };
struct ManagerStatus {
    bool busy{}, cancellable{true};
    juce::String operation, phase, error, detail;
    juce::int64 completedBytes{}, totalBytes{};
    InstallationState runtime{InstallationState::downloadable};
    std::vector<ModelState> models;
};
struct ManagedInstallation {
    Configuration runner;
    juce::File modelDirectory;
    std::shared_ptr<void> runtimeLease, modelLease;
};
class ModelManager {
public:
    // Fetch is an injection point for offline fault tests; UI uses the fixed HTTPS catalogue.
    using Progress = std::function<void(juce::int64, juce::int64)>;
    using Fetch = std::function<void(const DownloadArtifact&, const juce::File&, const std::atomic<bool>&, Progress)>;
    explicit ModelManager(juce::File root, juce::File workerScript,
                          juce::File requirements, Fetch = {}, std::vector<ModelRecord> = modelCatalogue());
    ~ModelManager(); // shutdown/service thread only; normal panel close retains the service
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    static juce::File defaultRoot();
    bool refresh(); // explicit full model hash refresh; offline; no Python/network
    bool refreshIfNeeded(); // metadata-only model inventory; shared until invalidated
    bool installModel(const juce::String&);
    bool verifyModel(const juce::String&); // explicit full hash of this model only
    bool removeModel(const juce::String&);
    bool installRuntime(); // explicit network action, managed embedded Python, no system Python
    bool verifyRuntime(); // offline probe
    bool removeRuntime();
    void cancel() noexcept;
    ManagerStatus status() const;
    juce::String diagnostics() const;
    // NON-RT: keep both leases until the Runner is terminal. Removal then returns busy.
    ManagedInstallation acquire(const juce::String& modelId, Device = Device::cpu) const;
    juce::File root() const { return root_; }
private:
    bool start(juce::String operation, std::function<void()>, bool onlyIfUnscanned = false);
    void scan(bool fullHashes = false, juce::String verifiedModel = {});
    void install(const ModelRecord&);
    void uninstall(const juce::File&, const juce::String& identity);
    void setupRuntime();
    void probeRuntime(const juce::File&);
    void phase(juce::String, juce::String = {});
    void ensureRoot();
    juce::File modelsPath() const;
    juce::File runtimePath() const;
    const ModelRecord& record(const juce::String&) const;
    std::shared_ptr<void> lease(const juce::String&, bool exclusive) const;
    juce::File root_, worker_, requirements_;
    Fetch fetch_;
    std::vector<ModelRecord> catalogue_;
    mutable std::mutex mutex_;
    ManagerStatus status_;
    bool scanned_{}; // protected by mutex_; explicit refresh still verifies files
    juce::String log_;
    std::atomic<bool> cancelled_{};
    std::thread thread_;
};
} // namespace mr::separation
