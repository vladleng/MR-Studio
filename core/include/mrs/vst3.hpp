#pragma once
#include <mrs/processing.hpp>
#include <filesystem>
#include <atomic>
namespace mrs::processing {
struct VstPlugin {std::string path, class_id, name, vendor, version; bool operator==(const VstPlugin&) const=default;};
std::vector<VstPlugin> probe_vst3(const std::string& path);
std::unique_ptr<IProcessor> vst3_factory(const NodeState&);
std::unique_ptr<IProcessor> hosted_factory(const NodeState&);
// Control-thread only: each candidate is probed in a hidden child with a timeout.
std::vector<VstPlugin> scan_vst3(const std::filesystem::path& root,const std::filesystem::path& helper,const std::filesystem::path& cache, std::shared_ptr<std::atomic<bool>> cancel={});
std::vector<VstPlugin> load_vst3_cache(const std::filesystem::path&);
void save_vst3_probe(const std::filesystem::path&, const std::vector<VstPlugin>&);
}
