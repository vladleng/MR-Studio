#include <mrs/audio.hpp>
#include <mrs/device.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef MRS_HAS_ASIO
#define NOMINMAX
#include <windows.h>
#endif

namespace {
using namespace mrs;
using namespace mrs::audio;
struct Options {
    bool self_test{}, list{}, panel{}, tone{}, interactive{};
    int device{-1}, monitor{-1};
    std::uint32_t rate{48000}, buffer{128}, seconds{30}, voices{1};
    std::vector<int> outputs{0, 1};
    std::vector<std::filesystem::path> wavs;
    std::filesystem::path report{"mrs-audio-report.json"};
};
int number(std::string_view text) {
    std::size_t end = 0;
    const auto n = std::stoll(std::string(text), &end);
    if (end != text.size() || n < 0 || n > 1000000) throw std::invalid_argument("invalid numeric argument");
    return static_cast<int>(n);
}
std::filesystem::path utf8_path(std::string_view value) {
    std::u8string encoded;
    encoded.reserve(value.size());
    for (const unsigned char c : value) encoded.push_back(static_cast<char8_t>(c));
    return std::filesystem::path{encoded};
}
std::vector<int> channels(std::string_view text) {
    std::vector<int> result;
    std::istringstream in{std::string(text)};
    std::string part;
    while (std::getline(in, part, ',')) {
        const auto n = number(part);
        if (n < 1 || n > 64) throw std::invalid_argument("channel numbers are 1..64");
        result.push_back(n - 1);
    }
    if (result.empty()) throw std::invalid_argument("empty channel list");
    return result;
}
#ifdef MRS_HAS_ASIO
std::string json_string(std::string_view text) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
        else out << static_cast<char>(c);
    }
    out << '"';
    return out.str();
}
void report(const Options& o, std::string_view device, const DeviceStatus& s, const Metrics& m,
            std::string_view result, std::string_view error, std::uint32_t input_count) {
    std::ofstream out(o.report);
    if (!out) throw std::runtime_error("cannot write JSON report");
    out << std::setprecision(10);
    out << "{\n  \"schema\": 1,\n  \"mode\": \"ASIO hardware prototype\",\n"
        << "  \"result\": " << json_string(result) << ",\n  \"error\": " << json_string(error)
        << ",\n  \"device\": " << json_string(device)
        << ",\n  \"requested_sample_rate\": " << o.rate << ",\n  \"actual_sample_rate\": " << s.sample_rate
        << ",\n  \"requested_buffer_frames\": " << o.buffer << ",\n  \"duration_seconds\": " << o.seconds
        << ",\n  \"voices_per_asset\": " << o.voices << ",\n  \"wav_assets\": " << o.wavs.size()
        << ",\n  \"tone\": " << (o.tone ? "true" : "false")
        << ",\n  \"input_channels\": " << input_count << ",\n  \"output_channels\": " << o.outputs.size()
        << ",\n  \"output_selectors_one_based\": [";
    for (std::size_t i = 0; i < o.outputs.size(); ++i) out << (i ? "," : "") << o.outputs[i] + 1;
    out << "],\n  \"monitor_input_one_based\": " << (o.monitor < 0 ? 0 : o.monitor + 1)
        << ",\n  \"reported_input_latency_ms\": " << s.input_latency_ms
        << ",\n  \"reported_output_latency_ms\": " << s.output_latency_ms
        << ",\n  \"round_trip_latency_measured\": false,\n  \"portaudio_cpu_load_at_end\": " << s.cpu_load
        << ",\n  \"callbacks\": " << m.callbacks << ",\n  \"callback_frames_min\": " << m.min_frames
        << ",\n  \"callback_frames_max\": " << m.max_frames << ",\n  \"input_overflows\": " << m.input_overflows
        << ",\n  \"input_underflows\": " << m.input_underflows << ",\n  \"output_underflows\": " << m.output_underflows
        << ",\n  \"output_overflows\": " << m.output_overflows << ",\n  \"callback_deadline_misses\": " << m.deadline_misses
        << ",\n  \"invalid_blocks\": " << m.invalid_blocks << ",\n  \"clipped_samples\": " << m.clipped_samples
        << ",\n  \"missing_input_blocks\": " << m.missing_inputs
        << ",\n  \"max_callback_body_ms\": " << static_cast<double>(m.max_callback_ns) / 1e6
        << ",\n  \"callback_body_load_p50_percent_bucket\": " << m.p50_load_percent
        << ",\n  \"callback_body_load_p95_percent_bucket\": " << m.p95_load_percent
        << ",\n  \"callback_body_load_p99_percent_bucket\": " << m.p99_load_percent
        << ",\n  \"performance_gate\": \"pending Studio Pro comparison and hardware acceptance\"\n}\n";
    if (!out) throw std::runtime_error("failed writing JSON report");
}
#endif
RenderGraph graph(const Options& o, std::uint32_t outputs) {
    RenderGraph result;
    std::vector<std::shared_ptr<const AudioData>> assets;
    if (o.tone) assets.push_back(std::make_shared<const AudioData>(sine_fixture(o.rate, 2, o.rate * 2, 220)));
    std::size_t decoded = 0;
    constexpr std::size_t budget = 512 * 1024 * 1024;
    for (const auto& path : o.wavs) {
        auto data = load_wav(path, budget - decoded);
        if (data.channels > 2) throw std::invalid_argument("prototype WAV playback accepts mono/stereo assets");
        decoded += data.samples.size() * sizeof(float);
        assets.push_back(std::make_shared<const AudioData>(std::move(data)));
    }
    if (assets.size() * o.voices > max_voices) throw std::invalid_argument("max 128 prepared voices");
    for (const auto& asset : assets) {
        for (std::uint32_t copy = 0; copy < o.voices; ++copy) {
            Voice voice{asset, 0, 0, asset->frames(), {}};
            const auto gain = 0.25F / static_cast<float>(o.voices);
            for (std::uint32_t output = 0; output < std::min(outputs, 2U); ++output)
                voice.routes.push_back({asset->channels == 1 ? 0U : output, output, gain});
            result.voices.push_back(std::move(voice));
        }
    }
    if (o.monitor >= 0)
        for (std::uint32_t output = 0; output < std::min(outputs, 2U); ++output)
            result.monitor.push_back({0, output, 0.25F});
    return result;
}
int self_test() {
    auto engine = std::make_shared<AudioEngine>();
    Options o; o.tone = true;
    engine->prepare({48000, 1, 2, 128}, graph(o, 2));
    const auto shared = std::make_shared<EngineTransport>(engine, Timeline{TimeMap{}, 48000});
    const std::shared_ptr<ITransport> arrange = shared, live = shared;
    int events = 0;
    auto subscription = live->subscribe([&](const auto&) { ++events; });
    live->set_loop(LoopRange{0, 96000}); arrange->play();
    std::array<float, 256> out{};
    for (int block = 0; block < 750; ++block) {
        const auto begin = std::chrono::steady_clock::now();
        engine->process(nullptr, out.data(), 128);
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count();
        engine->observe(static_cast<std::uint64_t>(ns), 128, 0);
    }
    shared->poll();
    if (arrange != live || live->state().sample != 0 || events != 1 || engine->metrics().callbacks != 750)
        throw std::runtime_error("shared realtime transport self-test failed");
    live->pause(); engine->process(nullptr, out.data(), 128); shared->poll();
    if (arrange->state().playback != PlaybackState::paused || events != 2)
        throw std::runtime_error("pause self-test failed");
    std::cout << "PASS: one shared render engine and ITransport for Arrange/Live\n"
              << "PASS: fixed command queue, block-boundary play/pause/loop\n"
              << "PASS: preloaded stereo playback and callback metrics\n"
              << "Audio self-test passed. This is an offline check, not an ASIO performance gate.\n";
    return 0;
}
void help() {
    std::cout << "Moon River Studio - SHARED Stage 1 audio prototype\n"
              << "  --self-test                       offline render check\n"
              << "  --list                            list vendor ASIO devices and channels\n"
              << "  --device N --panel                open vendor control panel\n"
              << "  --device N --rate 48000 --buffer 128 --seconds 30\n"
              << "  --tone                            low-level prepared test tone\n"
              << "  --wav PATH                        preload WAV (repeat for stems)\n"
              << "  --voices N                        duplicate each asset for load test\n"
              << "  --outputs 1,2                     physical outputs, one-based\n"
              << "  --monitor-input 1                 physical input, one-based; use headphones\n"
              << "  --report PATH                     JSON report (default mrs-audio-report.json)\n"
              << "  --interactive                     guided hardware check\n"
              << "No --tone/--wav/--monitor-input means silence, useful for the first driver check.\n";
}
Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string {
            if (++i >= argc) throw std::invalid_argument("missing option value");
            return argv[i];
        };
        if (arg == "--self-test") o.self_test = true;
        else if (arg == "--list") o.list = true;
        else if (arg == "--panel") o.panel = true;
        else if (arg == "--tone") o.tone = true;
        else if (arg == "--interactive") o.interactive = true;
        else if (arg == "--device") o.device = number(value());
        else if (arg == "--rate") o.rate = static_cast<std::uint32_t>(number(value()));
        else if (arg == "--buffer") o.buffer = static_cast<std::uint32_t>(number(value()));
        else if (arg == "--seconds") o.seconds = static_cast<std::uint32_t>(number(value()));
        else if (arg == "--voices") o.voices = static_cast<std::uint32_t>(number(value()));
        else if (arg == "--monitor-input") { o.monitor = number(value()) - 1; if (o.monitor < 0) throw std::invalid_argument("input is one-based"); }
        else if (arg == "--outputs") o.outputs = channels(value());
        else if (arg == "--wav") o.wavs.emplace_back(utf8_path(value()));
        else if (arg == "--report") o.report = utf8_path(value());
        else throw std::invalid_argument("unknown option: " + arg);
    }
    if (o.seconds == 0 || o.seconds > 14400 || o.voices == 0 || o.voices > 128)
        throw std::invalid_argument("duration 1..14400 seconds, voices 1..128");
    return o;
}
#ifdef MRS_HAS_ASIO
void list_devices(const std::vector<DeviceInfo>& devices) {
    if (devices.empty()) std::cout << "No 64-bit ASIO device found. Install the native vendor driver and retry.\n";
    for (const auto& d : devices) {
        std::cout << "Device " << d.index << ": " << d.name << '\n'
                  << "  Native buffer min/max/preferred/granularity: " << d.min_buffer << '/'
                  << d.max_buffer << '/' << d.preferred_buffer << '/' << d.granularity << '\n';
        for (std::size_t i = 0; i < d.inputs.size(); ++i) std::cout << "  Input " << i + 1 << ": " << d.inputs[i] << '\n';
        for (std::size_t i = 0; i < d.outputs.size(); ++i) std::cout << "  Output " << i + 1 << ": " << d.outputs[i] << '\n';
    }
}
std::string prompt(const char* label, const char* fallback) {
    std::cout << label << " [" << fallback << "]: " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) throw std::runtime_error("console input closed");
    return line.empty() ? fallback : line;
}
Options interactive(Options o, const std::vector<DeviceInfo>& devices) {
    list_devices(devices);
    if (devices.empty()) throw std::runtime_error("native ASIO driver is required for the hardware check");
    const auto default_device = std::to_string(devices.front().index);
    o.device = number(prompt("Device number", default_device.c_str()));
    o.rate = static_cast<std::uint32_t>(number(prompt("Sample rate", "48000")));
    o.buffer = static_cast<std::uint32_t>(number(prompt("Buffer frames", "128")));
    o.seconds = static_cast<std::uint32_t>(number(prompt("Test seconds", "30")));
    o.outputs = channels(prompt("Physical outputs (one-based)", "1,2"));
    std::cout << "Mode 1=silence, 2=tone, 3=WAV playback, 4=input monitor\n";
    const auto mode = number(prompt("Mode", "1"));
    if (mode == 2) o.tone = true;
    else if (mode == 3) o.wavs.emplace_back(utf8_path(prompt("WAV path without surrounding quotes", "")));
    else if (mode == 4) {
        std::cout << "Use headphones for microphone monitoring.\n";
        o.monitor = number(prompt("Physical input (one-based)", "1")) - 1;
        if (o.monitor < 0) throw std::invalid_argument("input is one-based");
    } else if (mode != 1) throw std::invalid_argument("mode must be 1..4");
    o.voices = static_cast<std::uint32_t>(number(prompt("Voices per asset (load test)", "1")));
    if (o.seconds == 0 || o.seconds > 14400 || o.voices == 0 || o.voices > 128)
        throw std::invalid_argument("invalid duration/load");
    return o;
}
int hardware(Options o) {
    auto device = make_asio_device();
    const auto devices = device->enumerate();
    if (o.list) { list_devices(devices); return devices.empty() ? 2 : 0; }
    if (o.interactive) o = interactive(std::move(o), devices);
    if (o.device < 0) throw std::invalid_argument("select --device N from --list");
    const auto found = std::find_if(devices.begin(), devices.end(), [&](const auto& d) { return d.index == o.device; });
    if (found == devices.end()) throw std::invalid_argument("ASIO device not found; re-run --list");
    if (o.panel) { device->control_panel(o.device); return 0; }
    DeviceConfig config;
    config.device = o.device; config.sample_rate = o.rate; config.buffer_frames = o.buffer; config.outputs = o.outputs;
    if (o.monitor >= 0) config.inputs.push_back(o.monitor);
    validate_device_config(*found, config);
    auto engine = std::make_shared<AudioEngine>();
    auto prepared = graph(o, static_cast<std::uint32_t>(o.outputs.size()));
    Sample loop_end = 0;
    for (const auto& voice : prepared.voices) loop_end = std::max(loop_end, voice.length);
    engine->prepare({o.rate, static_cast<std::uint32_t>(config.inputs.size()),
                     static_cast<std::uint32_t>(config.outputs.size()), 65536}, std::move(prepared));
    EngineTransport transport{engine, Timeline{TimeMap{}, o.rate}};
    DeviceStatus status;
    std::string error;
    std::string result = "PASS";
    try {
        device->open(config, engine);
        status = device->status();
        if (loop_end) transport.set_loop(LoopRange{0, loop_end});
        transport.play(); device->start();
        std::cout << "ASIO: " << found->name << ", " << status.sample_rate << " Hz, requested buffer " << o.buffer << '\n'
                  << "Reported input/output latency: " << status.input_latency_ms << '/' << status.output_latency_ms << " ms\n"
                  << "Running " << o.seconds << " seconds. Control thread sleeps; callback runs independently.\n";
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(o.seconds);
        auto heartbeat = std::chrono::steady_clock::now();
        std::uint64_t previous_callbacks = 0;
        while (std::chrono::steady_clock::now() < end) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            status = device->status();
            if (status.phase == DevicePhase::error) throw std::runtime_error(status.error);
            const auto callbacks = engine->metrics().callbacks;
            if (callbacks != previous_callbacks) { heartbeat = std::chrono::steady_clock::now(); previous_callbacks = callbacks; }
            else if (std::chrono::steady_clock::now() - heartbeat > std::chrono::seconds(2))
                throw std::runtime_error("ASIO callback stalled; stop, reconnect device, then re-enumerate");
        }
        device->stop();
    } catch (const std::exception& e) {
        error = e.what(); result = "ERROR";
        device->close(); // explicit recovery boundary; do not retry from callback
    }
    const auto m = engine->metrics();
    if (result == "PASS" && (m.callbacks == 0 || m.output_underflows || m.output_overflows ||
        m.input_overflows || m.input_underflows || m.deadline_misses || m.invalid_blocks || m.missing_inputs)) result = "REVIEW";
    if (result == "PASS" && (m.min_frames != o.buffer || m.max_frames != o.buffer)) result = "REVIEW";
    report(o, found->name, status, m, result, error, static_cast<std::uint32_t>(config.inputs.size()));
    std::cout << result << ": callbacks=" << m.callbacks << ", frames=" << m.min_frames << ".." << m.max_frames
              << ", input overflow=" << m.input_overflows << ", output underflow=" << m.output_underflows
              << ", deadline misses=" << m.deadline_misses << ", max callback body="
              << static_cast<double>(m.max_callback_ns) / 1e6 << " ms\n"
              << "Report: " << o.report.string() << '\n'
              << "Stage 1 performance gate remains pending hardware/Studio Pro comparison.\n";
    if (!error.empty()) std::cerr << "Error: " << error << '\n';
    device->close();
    return result == "PASS" ? 0 : result == "REVIEW" ? 2 : 1;
}
#endif
}
int run_main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--help") { help(); return 0; }
        auto options = parse(argc, argv);
        if (options.self_test) return self_test();
#ifdef MRS_HAS_ASIO
        if (argc == 1) options.interactive = true;
        return hardware(std::move(options));
#else
        help();
        if (argc == 1) return 0;
        throw std::runtime_error("hardware ASIO support is not compiled in this build");
#endif
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}

#ifdef MRS_HAS_ASIO
int wmain(int argc, wchar_t** argv) {
    SetConsoleCP(CP_UTF8); SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> utf8;
    std::vector<char*> args;
    utf8.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        const auto size = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) { std::cerr << "ERROR: invalid command-line encoding\n"; return 1; }
        std::string value(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, value.data(), size, nullptr, nullptr);
        value.pop_back(); utf8.push_back(std::move(value));
    }
    for (auto& value : utf8) args.push_back(value.data());
    return run_main(argc, args.data());
}
#else
int main(int argc, char** argv) { return run_main(argc, argv); }
#endif
