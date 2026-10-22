#pragma once
#include <string>
#include <vector>
namespace mrs::library {
struct AudioTypeAnalysis {std::string label,reason;double confidence{};};
struct AudioFeatures {
    static constexpr int version=2;
    AudioTypeAnalysis type;
    std::string state{"unknown"},reason,key;
    double duration{},rms{},peak{},onset_density{},centroid{},flatness{};
    double bpm{},bpm_confidence{},key_confidence{};
    std::vector<double> tempo_candidates;
};
}
