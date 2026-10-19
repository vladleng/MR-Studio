#include <mrs/ara.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <windows.h>
namespace {
std::string utf8(const wchar_t* text) {
    const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw std::runtime_error("Invalid ARA scanner argument");
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result.data(), size, nullptr, nullptr);
    result.pop_back(); return result;
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    try {
        const auto capabilities = mrs::ara::probe(utf8(argv[1]));
        std::ofstream out(std::filesystem::path(argv[2]), std::ios::trunc);
        out << "MRS_ARA_CAPABILITIES 1 " << capabilities.size() << '\n';
        for (const auto& c : capabilities)
            out << std::quoted(c.class_id) << ' ' << static_cast<int>(c.status) << ' '
                << std::quoted(c.name) << ' ' << std::quoted(c.factory_id) << ' '
                << std::quoted(c.version) << ' ' << std::quoted(c.archive_id) << ' '
                << c.api_generation << ' ' << std::quoted(c.diagnostic) << '\n';
        out.flush(); return out ? 0 : 3;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
