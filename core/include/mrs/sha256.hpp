#pragma once
#include <filesystem>
#include <cstdint>
#include <stop_token>
#include <string>
#include <string_view>
namespace mrs {
// Non-RT content integrity for portable project assets; no platform crypto SDK.
std::string sha256(std::string_view);
std::string sha256_file_range(const std::filesystem::path&,std::uint64_t offset,std::uint64_t bytes,std::stop_token = {});
std::string sha256_file(const std::filesystem::path&,std::stop_token = {});
}
