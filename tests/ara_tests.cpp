#include <mrs/ara.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <fstream>
#include <chrono>
#include <windows.h>
namespace {
void check(bool ok) { if (!ok) throw std::runtime_error("ARA test assertion failed"); }
template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    check(rejected);
}
}
int main(int argc, char** argv) {
    // Child-process fault fixtures for the scanner's containment contract.
    if (argc == 3 && std::filesystem::path(argv[1]).filename().string().starts_with("mrs-probe-")) {
        const auto mode = std::filesystem::path(argv[1]).filename().string();
        if (mode == "mrs-probe-hang") { Sleep(INFINITE); return 0; }
        if (mode == "mrs-probe-crash") return 42;
        std::ofstream out(argv[2]); out << "invalid capability output"; return 0;
    }
    if (argc != 4) return 2;
    try {
        const std::string path = argv[1];
        const bool supported = std::string(argv[2]) == "available";
        auto capabilities = mrs::ara::probe(path);
        check(!capabilities.empty());
        const auto info = capabilities.front();
        if (std::string(argv[2]) == "incompatible")
            check(info.status == mrs::ara::CapabilityStatus::incompatible && !info.diagnostic.empty());
        const auto isolated = mrs::ara::probe_isolated(path, argv[3]);
        check(isolated.size() == capabilities.size() && isolated.front().class_id == info.class_id &&
            isolated.front().status == info.status && isolated.front().factory_id == info.factory_id);
        check((info.status == mrs::ara::CapabilityStatus::available) == supported);
        rejects([&] { mrs::ara::Session::open(path, "00000000000000000000000000000000"); });
        auto cancel = std::make_shared<std::atomic<bool>>(true);
        rejects([&] { mrs::ara::Session::open(path, info.class_id, cancel); });
        rejects([&] { mrs::ara::probe_isolated(path, argv[3], 10000, cancel); });
        rejects([&] { mrs::ara::probe_isolated(path + ".missing", argv[3]); });
        rejects([&] { mrs::ara::probe_isolated(path, path + ".missing"); });
        rejects([&] { mrs::ara::probe_isolated("mrs-probe-hang", argv[0], 100); });
        rejects([&] { mrs::ara::probe_isolated("mrs-probe-crash", argv[0]); });
        rejects([&] { mrs::ara::probe_isolated("mrs-probe-malformed", argv[0]); });
        cancel->store(false);
        std::thread canceller([&] { std::this_thread::sleep_for(std::chrono::milliseconds(100)); cancel->store(true); });
        rejects([&] { mrs::ara::probe_isolated("mrs-probe-hang", argv[0], 10000, cancel); });
        canceller.join();
        if (!supported) {
            rejects([&] { mrs::ara::Session::open(path, info.class_id); });
        } else {
            for (int i = 0; i != 20; ++i) {
                auto first = mrs::ara::Session::open(path, info.class_id);
                auto second = mrs::ara::Session::open(path, info.class_id);
                check(first->roles_bound() && second->roles_bound());
                check(first->capability().factory_id == info.factory_id);
                first->edit([] {});
                rejects([&] { first->edit([] { throw std::runtime_error("cancel edit"); }); });
                first->edit([&] {
                    rejects([&] { first->edit([] {}); });
                    rejects([&] { first->notify(); });
                });
                first->notify();
                bool wrong_thread_rejected = false;
                std::thread thread([&] {
                    try { first->notify(); } catch (const std::exception&) { wrong_thread_rejected = true; }
                    rejects([&] { mrs::ara::Session::open(path, info.class_id); });
                });
                thread.join(); check(wrong_thread_rejected);
                first.reset(); second->edit([] {}); second->notify();
            }
        }
        std::cout << "ARA " << (supported ? "lifecycle" : "unsupported") << " PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
