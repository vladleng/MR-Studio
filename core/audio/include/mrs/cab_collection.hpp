#pragma once
#include <mrs/reverb_library.hpp>
#include <map>
#include <string_view>

namespace mrs::audio {
// All catalog APIs are NON-RT. Metadata only; no PCM or project schema changes.
struct CabMapping {
    std::string cabinet, mic, position, mix, voicing, room, channel_mode;
    std::string source{"unclassified"}, confidence{"unknown"};
    std::map<std::string,std::string> tags; // optional provider-independent facets
    bool premixed{}, classified{};
    bool operator==(const CabMapping&) const = default;
};
struct CabAsset {
    std::string id, variant_id, relative, digest, error;
    CabMapping mapping;
    std::uint64_t bytes{}, frames{};
    std::int64_t modified{};
    std::uint32_t rate{}, channels{}, bits{};
    bool present{true};
    double duration_ms() const { return rate?1000.*static_cast<double>(frames)/rate:0.; }
};
struct CabManifest {
    std::string collection_id;
    std::filesystem::path root; // local only, not persisted
    std::vector<CabAsset> assets;
    std::map<std::string,CabMapping> overrides; // relative path -> explicit mapping
    std::string error;
    std::uint64_t revision{};
};
CabMapping parse_celestion(std::string_view filename, std::uint32_t channels);
std::string cab_ir_role(const CabMapping&);
std::map<std::string,std::string> cab_asset_metadata(const CabAsset&);
// Explicit user mapping, metadata only. Suffix stripping is opt-in for rate pairs.
void map_cab_assets(CabManifest&, const std::vector<std::string>& relative_paths,
    const std::map<std::string,std::string>& metadata, std::string_view rate_suffix={});
CabManifest scan_cab_collection(const std::filesystem::path&, std::string collection_id,
    const CabManifest* previous=nullptr, std::stop_token={});
std::string serialize_cab_manifest(const CabManifest&);
// Worker-only integrity check before applying an asset; changed/missing never substitutes.
bool verify_cab_asset(const CabManifest&, const CabAsset&, std::stop_token={});
CabManifest deserialize_cab_manifest(std::string_view);
// Never changes semantic variant or length. Non-exact choice requires worker resampling in P3.
const CabAsset* select_cab_asset(const CabManifest&, std::string_view variant,
    double duration_ms, std::uint32_t project_rate);
class CabCollection {
public:
    CabCollection();
    ~CabCollection();
    void scan(std::filesystem::path, std::string collection_id, CabManifest previous={});
    void publish_mapping(CabManifest); // NON-RT metadata transaction; scanned revision must still be current.
    std::shared_ptr<const CabManifest> snapshot() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
