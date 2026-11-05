#pragma once
#include <mrs/audio_to_midi_worker.hpp>
#include <memory>

namespace mrs::transcription {
struct ModelFile {std::string name, sha256;std::uint64_t bytes{};};
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
// Blocking NON-RT local installation/verification. No downloads or token storage.
// Default catalog is pinned official Medium; alternate specs support fixtures.
class ModelStore {
public:
    explicit ModelStore(std::filesystem::path root,ModelSpec = official_medium());
    void install(const std::filesystem::path& authorized_folder,bool acknowledge_terms,
                 std::stop_token = {}) const;
    void verify(std::stop_token = {}) const;
    void remove() const;
    ModelLease acquire() const; // metadata/receipt only; worker verifies hashes
    std::filesystem::path directory() const;
private:
    struct Data;
    std::shared_ptr<Data> data_;
};
}
