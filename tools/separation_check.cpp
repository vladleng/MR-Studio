#include "SeparationRunner.h"
#include "SeparationModels.h"
#include <juce_cryptography/juce_cryptography.h>
#include <mrs/audio.hpp>
#include <mrs/recording.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mr::separation;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
// Fake device only: proves service isolation, not physical ASIO latency/xruns.
class PlaybackProbe {
public:
    explicit PlaybackProbe(const juce::File& destination)
        : recorder_(std::filesystem::path(destination.getFullPathName().toWideCharPointer()), 48000, 0) {
        auto asset = std::make_shared<mrs::audio::AudioData>(mrs::audio::sine_fixture(48000, 2, 48000, 220));
        mrs::audio::RenderGraph graph;
        graph.voices = {{asset, 0, 0, 48000, {{0,0,1},{1,1,1}}}};
        engine_.prepare({48000, 1, 2, 128}, std::move(graph));
        check(engine_.enqueue({mrs::audio::ControlKind::play}), "fake-device play");
        thread_ = std::thread([this] {
            std::array<float,128> input{}; input.fill(0.25f);
            std::array<float,256> output{};
            mrs::Sample position{};
            while (!stop_.load()) {
                engine_.process(input.data(), output.data(), 128);
                recorder_.capture(input.data(), 1, 128, position);
                position += 128;
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }
        });
    }
    ~PlaybackProbe() { stop(); }
    void verify() {
        stop();
        const auto saved = recorder_.finish();
        check(saved.frames > 0 && saved.status.fault == mrs::audio::RecordFault::none, "raw recording during ML");
        check(engine_.metrics().callbacks > 0 && engine_.state().playback == mrs::PlaybackState::playing,
              "playback continues during ML");
        const auto recorded = mrs::audio::load_wav(saved.path);
        check(std::all_of(recorded.samples.begin(), recorded.samples.end(), [](float v) { return v == 0.25f; }),
              "raw input unchanged by separation");
    }
private:
    void stop() { stop_.store(true); if (thread_.joinable()) thread_.join(); }
    mrs::audio::AudioEngine engine_;
    mrs::audio::Recorder recorder_;
    std::atomic<bool> stop_{};
    std::thread thread_;
};
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
    juce::String phase;
    while (runner.status().state == State::running && std::chrono::steady_clock::now() < until) {
        const auto current = runner.status();
        if (current.phase != phase) {
            phase = current.phase;
            std::cout << "separation phase: " << phase.toStdString() << ' ' << current.effectiveDevice.toStdString() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
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
    check(chooseCpuThreads()>=1&&chooseCpuThreads()<=4,"Auto preserves the accepted four-thread maximum");
    for(const auto choice:{1U,8U,16U,32U}){
        Runner selected({python,worker,root});req.sourceRevision="ok";req.cpuThreads=choice;
        check(selected.start(req),"CPU selection submit");const auto configured=wait(selected);auto result=selected.take();
        check(configured.state==State::ready&&result&&configured.cpuThreads==chooseCpuThreads(choice),"Manual threads capped to affinity");
        check(static_cast<int>(result->manifest["configured_threads"])==static_cast<int>(configured.cpuThreads),"Worker received the selected CPU count");
        check(configured.elapsedSeconds>0&&static_cast<double>(result->manifest["host_wall_seconds"])>0,"Completed processing times");
        result->directory.getParentDirectory().deleteRecursively();
    }
    {Runner invalid({python,worker,root});req.cpuThreads=33;check(invalid.start(req),"Invalid thread fixture submit");const auto state=wait(invalid);check(state.state==State::failed&&state.error=="invalid_input","Unbounded CPU selection rejected");}
    req.cpuThreads=1;
    for (const auto* mode : {"ok", "progress", "bad_progress", "stderr_flood", "crash", "flood", "wrong_job", "missing_model", "bad_hash", "nan", "traversal", "duplicate", "stale", "hang", "cancel"}) {
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
            if (std::string(mode) == "ok" || std::string(mode) == "progress" || std::string(mode) == "stderr_flood") {
                check(status.state == State::ready && result.has_value(), "valid IPC/geometry/float result");
                if(std::string(mode)=="progress")check(status.cpuCapability=="AVX2 (fixture)"&&status.completed==1&&status.total==2&&status.currentPass==1&&status.passes==1,"structured chunk progress survives IPC");
                check(result->manifest["timeline_start"].toString() == "12345", "original placement");
                if (std::string(mode) == "stderr_flood")
                    check(runner.diagnostics().getNumBytesAsUTF8() <= 256 * 1024 && runner.diagnostics().isNotEmpty(), "bounded stderr tail");
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
    req.timelineStart = 123456; req.projectRate = 48000; req.timeout = std::chrono::minutes(3);
    check(runner.start(req), "real submit");
    PlaybackProbe playback(root.getChildFile("synthetic-raw-recording.wav"));
    const auto status = wait(runner);
    playback.verify();
    auto result = runner.take();
    if (!result) {
        std::cerr << runner.diagnostics().toStdString() << '\n';
        throw std::runtime_error("real separation failed: " + status.error.toStdString());
    }
    check(status.effectiveDevice == "cpu", "CPU smoke effective device");
    std::cout << juce::JSON::toString(result->manifest).toStdString() << '\n';
    std::cout << "PASS real separation: 2 FLOAT stereo WAV, 48000 exact frames, original offset/start, host-verified hashes and finite samples\n";
}
ManagerStatus waitManager(ModelManager& manager) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(30);juce::String phase;
    while(manager.status().busy && std::chrono::steady_clock::now()<deadline){
        const auto s=manager.status();if(s.phase!=phase){phase=s.phase;std::cout<<"manager phase: "<<phase.toStdString()<<std::endl;}
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    const auto state=manager.status();check(!state.busy,"manager deadline");return state;
}
void managerTests(const juce::File& requirements) {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mr-models-test-"+juce::Uuid().toString());
    check(root.createDirectory().wasOk(),"manager fixture root");
    struct Cleanup{juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    const juce::String content="owned model fixture";
    const auto bytes=content.toUTF8();const auto hash=juce::SHA256(bytes.getAddress(),content.getNumBytesAsUTF8()).toHexString();
    ModelRecord record{"fixture-model","v1","Fixture","fixture","Vocals / Instrumental","Vocals / Other","CPU","fixture","fixture","fixture",{},{{"weights.bin","https://github.com/fixture",hash,static_cast<juce::int64>(content.getNumBytesAsUTF8())}}};
    bool corrupt{},cancel{};ModelManager* active{};
    auto fetch=[&](const DownloadArtifact& artifact,const juce::File& target,const std::atomic<bool>&,ModelManager::Progress progress){
        check(target.replaceWithText(corrupt?"bad":content),"fixture write");progress(artifact.bytes,artifact.bytes);if(cancel)active->cancel();
    };
    ModelManager manager(root,{},requirements,fetch,{record});active=&manager;
    check(manager.refreshIfNeeded(),"initial offline refresh");check(waitManager(manager).models.front().state==InstallationState::downloadable,"missing cache state");
    check(!manager.refreshIfNeeded()&&!manager.status().busy,"reuse initial scan when opening another panel");
    corrupt=true;check(manager.installModel(record.id),"corrupt download");check(waitManager(manager).error=="integrity_failure","download integrity rejection");
    check(!root.getChildFile("models/fixture-model").exists(),"partial install never published");
    check(manager.refreshIfNeeded(),"failed operation allows a fresh panel scan");check(waitManager(manager).models.front().state==InstallationState::downloadable,"failed install scan remains downloadable");
    corrupt=false;cancel=true;check(manager.installModel(record.id),"cancel install");check(waitManager(manager).error=="cancelled","cancel state");
    check(root.getChildFile("models").findChildFiles(juce::File::findDirectories,false,".part-*").isEmpty(),"cancel staging cleanup");
    cancel=false;check(manager.installModel(record.id),"retry install");check(waitManager(manager).models.front().state==InstallationState::installed,"installed state");
    const auto directory=root.getChildFile("models/fixture-model");const auto foreign=directory.getChildFile("user-file.txt");check(foreign.replaceWithText("do not delete"),"foreign fixture");
    check(manager.removeModel(record.id),"remove guarded cache");check(waitManager(manager).error=="unmanaged_files"&&foreign.existsAsFile(),"unmanaged files preserved");check(foreign.deleteFile(),"fixture remove own file");
    check(directory.getChildFile("weights.bin").replaceWithText("broken"),"corrupt local cache");
    check(manager.refresh(),"verify corrupt cache");check(waitManager(manager).models.front().state==InstallationState::corrupt,"corrupt cache state");
    check(manager.removeModel(record.id),"remove managed corruption");check(waitManager(manager).error.isEmpty()&&!directory.exists(),"remove only managed files");
    check(manager.installModel(record.id),"reinstall");check(waitManager(manager).error.isEmpty(),"reinstall after safe removal");
    check(directory.getChildFile("weights.bin").deleteFile(),"fixture missing file");
    check(!manager.refreshIfNeeded()&&!manager.status().busy&&manager.status().models.front().state==InstallationState::installed,"cached panel status must not rescan disk");
    check(manager.refresh(),"explicit refresh detects missing local file");check(waitManager(manager).models.front().state==InstallationState::corrupt,"missing file marks corrupt");
    check(manager.removeModel(record.id),"remove incomplete cache");check(waitManager(manager).error.isEmpty(),"incomplete cache recoverable");
    auto oversized=record;oversized.id="too-large";oversized.files.front().bytes=root.getBytesFreeOnVolume()+64*1024*1024;
    ModelManager large(root,{},requirements,fetch,{oversized});check(large.installModel(oversized.id),"large install");check(waitManager(large).error=="disk_space","disk preflight");
    const auto before=modelCatalogue().size();check(before>=6&&modelProfile("htdemucs-6s").stems.size()==6&&modelProfile("official-htdemucs-4-v1").stems.size()==4,"model-specific capabilities");
    bool rejected{};try{modelProfile("unknown-model");}catch(...){rejected=true;}check(rejected,"unknown capabilities excluded");
    std::cout<<"PASS model manager: integrity, atomic install, cancel/retry, corruption/missing file, safe removal, disk quota, unsupported catalogue\n";
}
void managed(const juce::File& cache,const juce::File& worker,const juce::File& requirements,const juce::File& evidence) {
    ModelManager manager(cache,worker,requirements);
    auto requireManager=[&]{const auto state=waitManager(manager);if(state.error.isNotEmpty()){std::cerr<<manager.diagnostics().toStdString()<<'\n';throw std::runtime_error("manager failed: "+state.error.toStdString());}};
    check(manager.installRuntime(),"explicit managed runtime install");requireManager();check(manager.status().runtime==InstallationState::installed,"runtime installed");
    check(manager.installModel(modelCatalogue().front().id),"explicit managed model install");requireManager();
    for(const auto& profile:modelCatalogue())if(profile.unsupportedReason.isEmpty()&&profile.id!=modelCatalogue().front().id){check(manager.installModel(profile.id),"explicit extended model install");requireManager();}
    check(manager.verifyRuntime(),"offline full runtime verification");requireManager();
    ModelManager reopened(cache,worker,requirements,[](const auto&,const auto&,const auto&,auto){throw std::runtime_error("offline refresh must not fetch");});
    check(reopened.refresh(),"reopen cache");check(waitManager(reopened).runtime==InstallationState::installed,"offline reopened runtime");
    auto setup=reopened.acquire(modelCatalogue().front().id,Device::automatic);
    check(manager.removeModel(modelCatalogue().front().id),"remove leased model");check(waitManager(manager).error=="in_use","model lease protects running jobs");
    check(manager.removeRuntime(),"remove leased runtime");check(waitManager(manager).error=="in_use","runtime lease protects running jobs");
    real(setup.runner.python,setup.runner.workerScript,setup.modelDirectory,evidence);
    check(manager.verifyRuntime(),"runtime remains immutable after inference");requireManager();
    std::cout<<"PASS managed runtime/model: embedded Python bootstrap, offline reopen, full verification, process leases, real inference\n";
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--self-test") selfTest(juce::File(juce::String::fromUTF8(argv[2])), juce::File(juce::String::fromUTF8(argv[3])));
        else if (argc == 6 && std::string(argv[1]) == "--real") real(juce::File(juce::String::fromUTF8(argv[2])), juce::File(juce::String::fromUTF8(argv[3])), juce::File(juce::String::fromUTF8(argv[4])), juce::File(juce::String::fromUTF8(argv[5])));
        else if(argc==3&&std::string(argv[1])=="--manager-self-test")managerTests(juce::File(juce::String::fromUTF8(argv[2])));
        else if(argc==6&&std::string(argv[1])=="--managed")managed(juce::File(juce::String::fromUTF8(argv[2])),juce::File(juce::String::fromUTF8(argv[3])),juce::File(juce::String::fromUTF8(argv[4])),juce::File(juce::String::fromUTF8(argv[5])));
        else throw std::runtime_error("Usage: --self-test PYTHON FAKE_WORKER | --real PYTHON WORKER MODELS EVIDENCE | --manager-self-test REQUIREMENTS | --managed CACHE WORKER REQUIREMENTS EVIDENCE");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
