#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
namespace mrs::diagnostics {
// Message/control-thread API only. Never call from an audio/device/DSP callback.
class Session {
public:
    Session(std::filesystem::path folder,std::filesystem::path helper,std::string version) noexcept;
    ~Session();
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    bool available() const noexcept;
    bool crash_capture_available() const noexcept;
    std::filesystem::path folder() const;
    std::filesystem::path base() const;
    std::filesystem::path previous_report() const;
private:
    struct Impl;std::unique_ptr<Impl> impl;static Impl* active;
    friend void event(std::string_view) noexcept;
    friend std::string status();
    friend std::filesystem::path folder();
};
void event(std::string_view text) noexcept;
std::string status();
std::filesystem::path folder();
}
