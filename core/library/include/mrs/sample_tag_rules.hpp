#pragma once
#include <optional>
#include <string>
#include <vector>

namespace mrs::library {
struct TagRule {
    std::string group;       // Instrument / Genre / Pack / Type / Technique / Rhythm / Character / Custom
    std::string phrase;      // word or phrase to match as a complete token sequence
    std::string label;       // canonical tag
    bool match_folder{};     // false: filename, true: enclosing directories
};
struct SuggestedTag {
    std::string group, label, origin; // filename / folder / rule
    double confidence{};
};
struct SuggestedMetadata {
    std::optional<int> bpm;
    std::optional<std::string> key;
};
struct Classification {
    std::vector<SuggestedTag> tags;
    SuggestedMetadata metadata;
};

// Pure, deterministic and local. No filesystem, database, ML or audio-thread work.
// Only explicit filename conventions are allowed to infer BPM/key.
Classification classify_sample_path(const std::string& utf8_relative_path,
                                    const std::vector<TagRule>& user_rules = {});
} // namespace mrs::library
