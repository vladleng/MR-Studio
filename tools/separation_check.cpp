#include "SeparationRunner.h"
#include <mrs/audio.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mr::separation;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void fixture(const juce::File& file, int rate, int frames) {
    auto out = file.createOutputStream();
    check(out && out->openedOk(), "fixture create");
    out->write("RIFF", 4); out->writeInt(36 + frames * 8); out->write("WAVEfmt ", 8);
    out->writeInt(16); out->writeShort(3); out->writeShort(2); out->writeInt(rate);
    out->writeInt(rate * 8); out->writeShort(8); out->writeShort(32);
    out->write("data", 4); out->writeInt(frames * 8);
    for (int i = 0; i < frames; ++i) {
        const auto sample = static_cast<float>(0.25 * std::sin(i * 6.283185307179586 * 440 / rate));
        out->writeFloat(sample); out->writeFloat(sample * 0.7f);
    }
    out->flush();
}
Status wait(Runner& runner) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::minutes(20);
    while (runner.status().state == State::running && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    check(runner.status().state != State::running, "runner exceeded test deadline");
    return runner.status();
}
void selfTest(const juce::File& python, const juce::File& worker) {
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mr-separation-test-" + juce::Uuid().toString());
    check(root.createDirectory().wasOk(), "test directory");
    struct Cleanup { juce::File root; ~Cleanup() { root.deleteRecursively(); } } cleanup{root};
    auto source = root.getChildFile(juce::String::fromUTF8("клип test.wav"));
    fixture(source, 48000, 200);
    Request req;
    req.source = source; req.modelDirectory = root; req.sourceFrames = 150;
    req.sourceStart = 25; req.timelineStart = 12345; req.sessionId = "session";
    for (const auto* mode : {"ok", "crash", "flood", "wrong_job", "missing_model", "bad_hash", "nan", "traversal", "duplicate", "stale", "hang", "cancel"}) {
        {
            Runner runner({python, worker, root});
            req.sourceRevision = mode;
            req.timeout = std::string(mode) == "hang" ? std::chrono::milliseconds(300) : std::chrono::seconds(15);
            check(runner.start(req), "submit");
            check(!runner.start(req), "one running job only");
            if (std::string(mode) == "cancel") {
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); runner.cancel();
            }
            const auto status = wait(runner);
            auto result = runner.take();
            if (std::string(mode) == "ok") {
                check(status.state == State::ready && result.has_value(), "valid IPC/geometry/float result");
                check(result->manifest["timeline_start"].toString() == "12345", "original placement");
                result->directory.getParentDirectory().deleteRecursively();
            } else {
                check(!result && status.state != State::ready, "fault must not publish");
                if (std::string(mode) == "cancel") check(status.state == State::cancelled, "cancel terminal");
                if (std::string(mode) == "hang") check(status.error == "timeout", "deadline terminal");
            }
        }
        check(root.findChildFiles(juce::File::findDirectories, false, "mrs-separation-*").isEmpty(), "failed job cleanup");
        check(source.existsAsFile(), "source preserved");
    }
    Runner runner({root.getChildFile("absent-python.exe"), worker, root});
    req.sourceRevision = "missing-python";
    check(runner.start(req), "missing runtime submit");
    check(wait(runner).error == "python_missing", "missing Python structured error");
    check(runner.start(req), "restart after terminal failure");
    check(wait(runner).state == State::failed, "restart isolated failure");
    std::cout << "PASS separation host: IPC, float WAV, Unicode paths, bounds, cancel/timeout/crash, stale/hostile outputs, restart\n";
}
void real(const juce::File& python, const juce::File& worker, const juce::File& models, const juce::File& root) {
    check(root.createDirectory().wasOk(), "evidence directory");
    auto source = root.getChildFile("synthetic-source.wav");
    check(!source.exists(), "do not overwrite existing evidence");
    fixture(source, 44100, 88200);
    Runner runner({python, worker, root});
    Request req;
    req.source = source; req.modelDirectory = models; req.sessionId = "windows-cpu-smoke";
    req.sourceRevision = "synthetic-v1"; req.sourceStart = 11025; req.sourceFrames = 44100;
    req.timelineStart = 123456; req.projectRate = 48000; req.timeout = std::chrono::minutes(15);
    check(runner.start(req), "real submit");
    const auto status = wait(runner);
    auto result = runner.take();
    if (!result) throw std::runtime_error("real separation failed: " + status.error.toStdString());
    check(status.effectiveDevice == "cpu", "CPU smoke effective device");
    std::cout << juce::JSON::toString(result->manifest).toStdString() << '\n';
    std::cout << "PASS real separation: 2 FLOAT stereo WAV, 48000 exact frames, original offset/start, host-verified hashes and finite samples\n";
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--self-test") selfTest(juce::File(juce::String::fromUTF8(argv[2])), juce::File(juce::String::fromUTF8(argv[3])));
        else if (argc == 6 && std::string(argv[1]) == "--real") real(juce::File(juce::String::fromUTF8(argv[2])), juce::File(juce::String::fromUTF8(argv[3])), juce::File(juce::String::fromUTF8(argv[4])), juce::File(juce::String::fromUTF8(argv[5])));
        else throw std::runtime_error("Usage: --self-test PYTHON FAKE_WORKER | --real PYTHON WORKER MODELS NEW_EVIDENCE_DIRECTORY");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
