#include <mrs/ara.hpp>
#include <windows.h>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace mrs::ara {
namespace {
struct Child {
    PROCESS_INFORMATION process{};
    ~Child() {
        if (process.hProcess) {
            if (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(process.hProcess, 124);
                WaitForSingleObject(process.hProcess, 1000);
            }
            CloseHandle(process.hProcess);
        }
        if (process.hThread) CloseHandle(process.hThread);
    }
};
struct Temp {
    std::filesystem::path path;
    Temp() {
        wchar_t root[MAX_PATH + 1]{}, file[MAX_PATH + 1]{};
        const auto length = GetTempPathW(MAX_PATH + 1, root);
        if (!length || length > MAX_PATH || !GetTempFileNameW(root, L"ara", 0, file))
            throw std::runtime_error("Cannot create ARA probe output");
        path = file;
    }
    ~Temp() { std::error_code ec; std::filesystem::remove(path, ec); }
};
std::wstring quote(const std::wstring& value) {
    if (value.find(L'"') != std::wstring::npos) throw std::runtime_error("Invalid ARA probe path");
    // Windows command-line escaping, including a trailing backslash.
    std::wstring result = L"\"";
    std::size_t slashes{};
    for (const auto ch : value) {
        if (ch == L'\\') ++slashes;
        else { result.append(slashes, L'\\'); slashes = 0; result += ch; }
    }
    result.append(slashes * 2, L'\\'); result += L'"'; return result;
}
}
std::vector<Capability> probe_isolated(const std::string& module_path,
    const std::filesystem::path& helper, unsigned timeout_ms, std::shared_ptr<std::atomic<bool>> cancel) {
    if (!timeout_ms || timeout_ms > 60000) throw std::runtime_error("Invalid ARA probe timeout");
    if (cancel && cancel->load()) throw std::runtime_error("ARA probe cancelled");
    Temp output;
    const auto executable = std::filesystem::absolute(helper);
    const auto module = std::filesystem::absolute(std::filesystem::path(std::u8string(module_path.begin(), module_path.end())));
    auto command = quote(executable.wstring()) + L" " + quote(module.wstring()) + L" " + quote(output.path.wstring());
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;
    Child child;
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, executable.parent_path().c_str(), &startup, &child.process))
        throw std::runtime_error("ARA scanner helper cannot start");
    const auto start = GetTickCount64();
    for (;;) {
        const auto wait = WaitForSingleObject(child.process.hProcess, 25);
        if (cancel && cancel->load()) throw std::runtime_error("ARA probe cancelled");
        std::error_code output_error;
        const auto output_size = std::filesystem::file_size(output.path, output_error);
        if (!output_error && output_size > 1024 * 1024)
            throw std::runtime_error("ARA scanner output too large");
        if (wait == WAIT_OBJECT_0) break;
        if (wait != WAIT_TIMEOUT) throw std::runtime_error("ARA scanner wait failed");
        if (GetTickCount64() - start >= timeout_ms) throw std::runtime_error("ARA scanner timed out");
    }
    DWORD exit_code{};
    if (!GetExitCodeProcess(child.process.hProcess, &exit_code) || exit_code != 0)
        throw std::runtime_error("ARA scanner failed or crashed");
    if (std::filesystem::file_size(output.path) > 1024 * 1024)
        throw std::runtime_error("ARA scanner output too large");
    std::ifstream in(output.path);
    std::string magic; unsigned version{}; std::size_t count{};
    if (!(in >> magic >> version >> count) || magic != "MRS_ARA_CAPABILITIES" || version != 1 || count > 4096)
        throw std::runtime_error("Invalid ARA scanner header");
    std::vector<Capability> result;
    for (std::size_t i = 0; i < count; ++i) {
        Capability c; int status{};
        if (!(in >> std::quoted(c.class_id) >> status >> std::quoted(c.name) >> std::quoted(c.factory_id)
            >> std::quoted(c.version) >> std::quoted(c.archive_id) >> c.api_generation >> std::quoted(c.diagnostic)) ||
            status < 0 || status > 2 || c.class_id.size() != 32 ||
            c.class_id.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos ||
            c.name.size() > 4096 || c.factory_id.size() > 4096 || c.version.size() > 4096 ||
            c.archive_id.size() > 4096 || c.diagnostic.size() > 4096 ||
            (status == static_cast<int>(CapabilityStatus::available) &&
                (c.factory_id.empty() || c.archive_id.empty() || c.api_generation <= 0)))
            throw std::runtime_error("Invalid ARA scanner capability");
        c.status = static_cast<CapabilityStatus>(status); result.push_back(std::move(c));
    }
    in >> std::ws;
    if (!in.eof()) throw std::runtime_error("Unexpected trailing ARA scanner data");
    return result;
}
}
