#pragma once
#include <mrs/audio_to_midi_worker.hpp>
#include <memory>

namespace mrs::transcription {
struct ModelFile {std::string name, sha256;std::uint64_t bytes{};};
struct ModelDownloadTarget {std::string host,path;bool authorize{};};
// Exact HTTPS origin/CDN allowlist. Authorization is only for huggingface.co.
ModelDownloadTarget model_download_target(std::string_view url);
std::string model_download_redirect(std::string_view current,std::string_view location);
using ModelDownloadProgress=std::function<void(std::uint64_t,std::uint64_t)>;
using ModelFetch=std::function<void(const std::string& url,const std::filesystem::path& target,
                                  std::uint64_t bytes,std::stop_token,const ModelDownloadProgress&)>;
// Windows WinHTTP, TLS verification enabled, manual validated redirects.
// Token stays in this one download operation, never in files/logs/preferences.
ModelFetch authenticated_model_fetch(std::string token);
struct ModelSpec {
    std::string id, revision, license;
    std::vector<ModelFile> files;
};
const ModelSpec& official_medium();
// Held throughout inference, preventing install/remove through this store
// instance (and its copies). External filesystem changes are hash-checked.
struct ModelLease {
    std::filesystem::path directory;
    ModelSpec spec;
    std::shared_ptr<const void> lifetime;
    void configure(Request&) const;
};
// Blocking NON-RT installation/verification/download. No token storage.
// Default catalog is pinned official Medium; alternate specs support fixtures.
class ModelStore {
public:
    explicit ModelStore(std::filesystem::path root,ModelSpec = official_medium());
    void install(const std::filesystem::path& authorized_folder,bool acknowledge_terms,
                 std::stop_token = {}) const;
    void verify(std::stop_token = {}) const;
    // Explicit official-HF download through an injected transport; never stores
    // credentials. Network transport must enforce model_download_target policy.
    void download(const std::filesystem::path& temporary_root,bool acknowledge_terms,
                  const ModelFetch&,std::stop_token = {},const ModelDownloadProgress& = {}) const;
    void remove() const;
    ModelLease acquire() const; // metadata/receipt only; worker verifies hashes
    std::filesystem::path directory() const;
private:
    struct Data;
    std::shared_ptr<Data> data_;
};
}
