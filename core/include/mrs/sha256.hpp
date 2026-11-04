#pragma once
#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>
namespace mrs {
// Non-RT content integrity for portable project assets; no platform crypto SDK.
std::string sha256(std::string_view);
std::string sha256_file(const std::filesystem::path&,std::stop_token = {});
}
