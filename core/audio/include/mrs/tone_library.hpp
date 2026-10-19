#pragma once
#include <mrs/cab_collection.hpp>

namespace mrs::audio {
// NON-RT model bookmarks. No PCM, image bytes or DSP/runtime ownership.
enum class ToneKind { wav_ir, nam };
struct SourceAsset {
    std::string id, root;
    ReverbReference file;
    std::uint32_t rate{}, channels{};
    std::uint64_t frames{};
    bool operator==(const SourceAsset&) const = default;
};
struct ToneModel {
    std::string id, title, maker, variant, role{"unknown"}, provenance{"unclassified"};
    ToneKind kind{ToneKind::wav_ir};
    std::vector<std::string> sources, tags;
    std::string artwork, artwork_rights, notes;
    bool favorite{};
    // A capture bookmark retains its stable identity; the gallery presents one
    // physical model family with these bookmarks as selectable IR variants.
    std::string family, family_title;
    bool preferred{};
    bool operator==(const ToneModel&) const = default;
};
struct ToneLibrary;
std::string tone_family(const ToneModel&);
std::vector<const ToneModel*> tone_variants(const ToneLibrary&, const ToneModel&);
struct ToneLibrary {
    std::vector<SourceAsset> sources;
    std::vector<ToneModel> models;
    void validate() const;
    bool operator==(const ToneLibrary&) const = default;
};
std::string serialize_tone_library(const ToneLibrary&);
ToneLibrary deserialize_tone_library(std::string_view);
// Reuses the accepted scanner result. Existing curation and IDs win on reindex.
void curate_cab_manifest(ToneLibrary&,const CabManifest&);
const SourceAsset* tone_source(const ToneLibrary&,const ToneModel&,std::uint32_t project_rate);
bool tone_source_available(const SourceAsset&,std::stop_token={}); // worker-only full identity check
ReverbState tone_source_state(const SourceAsset&,std::string name);
// Same-content relink only; rejected replacements cannot change the saved bookmark.
void relink_tone_source(SourceAsset&,const CabManifest&,const CabAsset&);
}
