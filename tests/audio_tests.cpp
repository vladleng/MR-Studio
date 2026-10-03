#include <mrs/audio.hpp>
#include <mrs/device.hpp>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>

namespace allocation_check {
thread_local bool enabled = false;
std::atomic<std::uint64_t> count{};
}
void* operator new(std::size_t bytes) {
    if (allocation_check::enabled) allocation_check::count.fetch_add(1);
    if (auto p = std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
namespace {
using namespace mrs;
using namespace mrs::audio;
int assertions = 0;
void check(bool value, const char* text, int line) {
    ++assertions;
    if (!value) throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + text);
}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __LINE__)
template<class F> void rejects(F action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    CHECK(failed);
}
std::shared_ptr<const AudioData> data() {
    return std::make_shared<const AudioData>(AudioData{48000, 2, {0.1F,0.2F, 0.3F,0.4F, 0.5F,0.6F, 0.7F,0.8F}});
}
RenderGraph basic() {
    return {{{data(), 0, 0, 4, {{0,0,1}, {1,1,1}}}}, {{0,0,0.5F}, {0,1,0.5F}}};
}
void render() {
    AudioEngine engine;
    engine.prepare({48000, 1, 2, 8}, basic());
    std::array<float, 8> out{};
    std::array<float, 4> in{0.1F,0.1F,0.1F,0.1F};
    engine.process(in.data(), out.data(), 4);
    CHECK(std::abs(out[0] - 0.05F) < 1e-6F); // monitoring while stopped
    CHECK(engine.state().sample == 0);
    CHECK(engine.enqueue({ControlKind::play}));
    const auto before_allocations = allocation_check::count.load();
    allocation_check::enabled = true;
    engine.process(in.data(), out.data(), 4);
    engine.observe(1000, 4, 0);
    allocation_check::enabled = false;
    CHECK(allocation_check::count.load() == before_allocations);
    CHECK(std::abs(out[0] - 0.15F) < 1e-6F && std::abs(out[7] - 0.85F) < 1e-6F);
    CHECK(engine.state().sample == 4);
    engine.process(in.data(), out.data(), 4); // past end -> silence + monitoring
    CHECK(std::abs(out[0] - 0.05F) < 1e-6F);
    auto graph = basic();
    graph.voices[0].start = 2; graph.voices[0].source_offset = 1; graph.voices[0].length = 2;
    graph.monitor.clear();
    engine.prepare({48000,0,2,8}, graph);
    CHECK(engine.enqueue({ControlKind::play}));
    engine.process(nullptr, out.data(), 4);
    CHECK(out[0] == 0 && out[3] == 0 && out[4] == 0.3F && out[7] == 0.6F);
    graph.voices[0].routes = {{0,0,16}, {1,1,16}};
    engine.prepare({48000,0,2,8}, graph);
    CHECK(engine.enqueue({ControlKind::play}));
    engine.process(nullptr, out.data(), 4);
    CHECK(out[4] == 1 && engine.metrics().clipped_samples > 0);
    graph.voices[0].routes[0].output_channel = 2;
    rejects([&] { engine.prepare({48000,0,2,8}, graph); });
    graph = basic();
    rejects([&] { engine.prepare({44100,1,2,8}, graph); });
    graph.voices[0].length = 5;
    rejects([&] { engine.prepare({48000,1,2,8}, graph); });
    graph = basic(); graph.monitor[0].input_channel = 1;
    rejects([&] { engine.prepare({48000,1,2,8}, graph); });
    graph = basic(); graph.voices[0].asset.reset();
    rejects([&] { engine.prepare({48000,1,2,8}, graph); });
    engine.prepare({48000,1,2,8}, basic());
    engine.process(nullptr, out.data(), 4);
    CHECK(engine.metrics().missing_inputs == 1);
    std::array<float, 18> oversized;
    oversized.fill(1);
    engine.process(nullptr, oversized.data(), 9);
    CHECK(engine.metrics().invalid_blocks == 1 && oversized.back() == 0);
    engine.process(nullptr, nullptr, 1);
    CHECK(engine.metrics().invalid_blocks == 2);
    AudioData nonfinite{48000,1,{std::numeric_limits<float>::quiet_NaN()}};
    rejects([&] { nonfinite.validate(); });
}
void transport() {
    auto engine = std::make_shared<AudioEngine>();
    engine->prepare({48000,1,2,8}, basic());
    auto service = std::make_shared<EngineTransport>(engine, Timeline{TimeMap{}, 48000});
    std::shared_ptr<ITransport> arrange = service, live = service;
    std::vector<TransportState> events;
    auto connection = live->subscribe([&](const auto& s) { events.push_back(s); });
    std::array<float, 8> output{};
    live->set_loop(LoopRange{0,4}); arrange->play();
    CHECK(live->state().playback == PlaybackState::stopped); // applied at callback boundary
    engine->process(nullptr, output.data(), 4); service->poll();
    CHECK(live->state().sample == 0 && events.size() == 1);
    CHECK(output[0] == 0.1F && output[7] == 0.8F);
    live->seek(3); engine->process(nullptr, output.data(), 4); service->poll();
    CHECK(arrange->state().sample == 3);
    CHECK(output[0] == 0.7F && output[2] == 0.1F);
    live->pause(); engine->process(nullptr, output.data(), 4); service->poll();
    CHECK(arrange->state().playback == PlaybackState::paused && arrange->state().sample == 3);
    CHECK(output[0] == 0);
    live->stop(); engine->process(nullptr, output.data(), 4); service->poll();
    CHECK(arrange->state().sample == 0 && arrange->state().loop.has_value());
    live->set_loop(std::nullopt); engine->process(nullptr, output.data(), 4); service->poll();
    CHECK(!arrange->state().loop);
    rejects([&] { live->seek(-1); });
    rejects([&] { live->set_loop(LoopRange{4,4}); });
    for (int i = 0; i < 63; ++i) live->play();
    rejects([&] { live->play(); }); // bounded backpressure
    engine->process(nullptr, output.data(), 4); service->poll();
    live->pause(); engine->process(nullptr, output.data(), 4);
    CHECK(live->state().playback == PlaybackState::paused);
}
void queue() {
    SpscQueue<std::uint64_t, 8> queue;
    for (std::uint64_t n = 0; n < 7; ++n) CHECK(queue.push(n));
    CHECK(!queue.push(8));
    std::uint64_t value = 0;
    for (std::uint64_t n = 0; n < 7; ++n) CHECK(queue.pop(value) && value == n);
    CHECK(!queue.pop(value));
    constexpr std::uint64_t total = 100000;
    std::atomic<bool> correct{true};
    std::thread consumer([&] {
        for (std::uint64_t n = 0; n < total; ++n) {
            std::uint64_t received = 0;
            while (!queue.pop(received)) std::this_thread::yield();
            if (received != n) correct = false;
        }
    });
    for (std::uint64_t n = 0; n < total; ++n)
        while (!queue.push(n)) std::this_thread::yield();
    consumer.join();
    CHECK(correct.load());
}
void put16(std::string& bytes, std::uint16_t word) {
    bytes.push_back(static_cast<char>(word & 255U)); bytes.push_back(static_cast<char>(word >> 8));
}
void put32(std::string& bytes, std::uint32_t word) {
    for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<char>((word >> shift) & 255U));
}
std::string wav_bytes(std::uint16_t format, std::uint16_t bits, const std::string& payload) {
    std::string bytes = "RIFF"; put32(bytes, static_cast<std::uint32_t>(36 + payload.size()));
    bytes += "WAVEfmt "; put32(bytes,16); put16(bytes,format); put16(bytes,1); put32(bytes,48000);
    put32(bytes,48000 * (bits / 8)); put16(bytes,static_cast<std::uint16_t>(bits / 8)); put16(bytes,bits);
    bytes += "data"; put32(bytes,static_cast<std::uint32_t>(payload.size())); bytes += payload;
    return bytes;
}
struct TempFile {
    std::filesystem::path path = std::filesystem::temp_directory_path() / (new_id().value + ".wav");
    void write(const std::string& bytes) { std::ofstream out(path,std::ios::binary); out.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); }
    ~TempFile() { std::error_code error; std::filesystem::remove(path,error); }
};
void wav() {
    TempFile file;
    std::string payload;
    put16(payload,0); put16(payload,32767); put16(payload,32768); put16(payload,16384);
    const auto original = wav_bytes(1,16,payload);
    file.write(original);
    const auto asset = load_wav(file.path);
    CHECK(asset.channels == 1 && asset.frames() == 4 && asset.sample_rate == 48000);
    CHECK(asset.samples[0] == 0 && asset.samples[2] == -1 && asset.samples[3] == 0.5F);
    rejects([&] { (void)load_wav(file.path,4); });
    payload.clear(); put32(payload, std::bit_cast<std::uint32_t>(0.25F)); put32(payload, std::bit_cast<std::uint32_t>(-0.5F));
    file.write(wav_bytes(3,32,payload));
    CHECK(load_wav(file.path).samples[1] == -0.5F);
    payload.clear(); put32(payload,0x80000000); put32(payload,0x40000000);
    file.write(wav_bytes(1,32,payload));
    CHECK(load_wav(file.path).samples[0] == -1 && load_wav(file.path).samples[1] == 0.5F);
    payload = std::string("\0\0\x80\0\0\x40",6);
    file.write(wav_bytes(1,24,payload));
    CHECK(load_wav(file.path).samples[0] == -1 && load_wav(file.path).samples[1] == 0.5F);
    file.write(original.substr(0,original.size()-1));
    rejects([&] { (void)load_wav(file.path); });
    auto bad = original; bad[0] = 'X'; file.write(bad);
    rejects([&] { (void)load_wav(file.path); });
    bad = original; bad[20] = 7; file.write(bad); // unknown format
    rejects([&] { (void)load_wav(file.path); });
    bad = original; bad[32] = 0; file.write(bad); // invalid block alignment
    rejects([&] { (void)load_wav(file.path); });
    payload.clear(); put32(payload,std::bit_cast<std::uint32_t>(std::numeric_limits<float>::infinity()));
    file.write(wav_bytes(3,32,payload));
    rejects([&] { (void)load_wav(file.path); });
    rejects([&] { (void)load_wav(file.path.string()+".missing"); });
}
void device() {
    DeviceInfo d{5,"Vendor",{"Input 1","Input 2"},{"Out 1","Out 2"},32,1024,128,-1};
    CHECK(supports_buffer(d,32) && supports_buffer(d,64) && supports_buffer(d,128));
    CHECK(!supports_buffer(d,96) && !supports_buffer(d,0) && !supports_buffer(d,2048));
    DeviceConfig c; c.device = 5; c.inputs = {0};
    validate_device_config(d,c);
    c.inputs = {0,0}; rejects([&] { validate_device_config(d,c); });
    c.inputs = {2}; rejects([&] { validate_device_config(d,c); });
    c.inputs.clear(); c.outputs = {2}; rejects([&] { validate_device_config(d,c); });
    c.outputs.clear(); rejects([&] { validate_device_config(d,c); });
    c.outputs = {0,1}; c.buffer_frames = 96; rejects([&] { validate_device_config(d,c); });
    d.granularity = 32; CHECK(supports_buffer(d,96) && !supports_buffer(d,100));
    d.granularity = 0; CHECK(supports_buffer(d,128) && !supports_buffer(d,64));
    c.buffer_frames = 128; c.device = 6; rejects([&] { validate_device_config(d,c); });
}
void metrics() {
    AudioEngine engine;
    std::array<float,256> output{};
    for (int i=0; i<3; ++i) engine.process(nullptr,output.data(),128);
    engine.observe(100000,128,0);
    engine.observe(200000,128,2|4);
    engine.observe(3000000,128,0);
    const auto m = engine.metrics();
    CHECK(m.callbacks == 3 && m.measured_callbacks == 3 && m.max_callback_ns == 3000000);
    CHECK(m.input_overflows == 1 && m.output_underflows == 1 && m.deadline_misses == 1);
    CHECK(m.min_frames == 128 && m.max_frames == 128);
    CHECK(m.p50_load_percent == 8 && m.p99_load_percent == 100);
}
void independence() {
    auto engine = std::make_shared<AudioEngine>();
    engine->prepare({48000,1,2,128}, basic());
    EngineTransport control{engine, Timeline{TimeMap{},48000}};
    control.set_loop(LoopRange{0,4}); control.play();
    std::atomic<bool> run{true};
    std::thread audio([&] {
        std::array<float,256> output{};
        const auto before = allocation_check::count.load();
        allocation_check::enabled = true;
        while (run.load()) engine->process(nullptr,output.data(),128);
        allocation_check::enabled = false;
        if (allocation_check::count.load() != before) run = false;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto before = engine->metrics().callbacks;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // frozen UI/control pump
    const auto after = engine->metrics().callbacks;
    run = false; audio.join();
    CHECK(after > before);
    CHECK(allocation_check::count.load() == 0);
    control.poll();
    CHECK(control.state().playback == PlaybackState::playing);
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected suite");
        const std::string suite = argv[1];
        if (suite=="render") render(); else if (suite=="transport") transport();
        else if (suite=="queue") queue(); else if (suite=="wav") wav();
        else if (suite=="device") device(); else if (suite=="metrics") metrics();
        else if (suite=="independence") independence(); else throw std::invalid_argument("unknown suite");
        std::cout << "PASS " << suite << ": " << assertions << " checks\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
