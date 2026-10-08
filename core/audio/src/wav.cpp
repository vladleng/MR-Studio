#include <mrs/audio.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numbers>
#include <stdexcept>

namespace mrs::audio {
namespace {
std::uint16_t u16(const unsigned char* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}
std::uint32_t u32(const unsigned char* p) {
    return p[0] | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
void read_exact(std::istream& in, unsigned char* out, std::size_t bytes) {
    if (!in.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(bytes)))
        throw std::invalid_argument("truncated WAV");
}
}
WavFile inspect_wav(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open WAV asset");
    in.seekg(0, std::ios::end);
    const auto file_size = in.tellg();
    if (file_size < 12) throw std::invalid_argument("short WAV header");
    in.seekg(0);
    std::array<unsigned char, 12> header{};
    read_exact(in, header.data(), header.size());
    if (std::memcmp(header.data(), "RIFF", 4) || std::memcmp(header.data() + 8, "WAVE", 4))
        throw std::invalid_argument("only RIFF/WAVE supported (no RF64)");
    const auto declared_end = static_cast<std::uint64_t>(u32(header.data() + 4)) + 8;
    if (declared_end < 12 || declared_end > static_cast<std::uint64_t>(file_size))
        throw std::invalid_argument("invalid RIFF size");
    std::uint16_t format{}, channels{}, bits{}, block_align{};
    std::uint32_t rate{};
    std::uint64_t offset = 12, data_offset = 0;
    std::uint32_t data_size = 0;
    bool have_format = false, have_data = false;
    while (offset + 8 <= declared_end) {
        in.seekg(static_cast<std::streamoff>(offset));
        std::array<unsigned char, 8> chunk{};
        read_exact(in, chunk.data(), chunk.size());
        const auto size = u32(chunk.data() + 4);
        const auto payload = offset + 8;
        if (payload + size > declared_end) throw std::invalid_argument("invalid WAV chunk bounds");
        if (!std::memcmp(chunk.data(), "fmt ", 4)) {
            if (have_format || size < 16) throw std::invalid_argument("invalid/duplicate WAV format");
            std::array<unsigned char, 40> fmt{};
            const auto read = static_cast<std::size_t>(std::min(size, 40U));
            read_exact(in, fmt.data(), read);
            format = u16(fmt.data()); channels = u16(fmt.data() + 2); rate = u32(fmt.data() + 4);
            block_align = u16(fmt.data() + 12); bits = u16(fmt.data() + 14);
            if (format == 0xfffe) {
                if (size < 40 || u16(fmt.data() + 16) < 22 || u16(fmt.data() + 18) != bits)
                    throw std::invalid_argument("unsupported WAV extensible valid-bit layout");
                static constexpr unsigned char guid_tail[12] = {0, 0, 0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71};
                if (std::memcmp(fmt.data() + 28, guid_tail, 12) || u16(fmt.data() + 26) != 0)
                    throw std::invalid_argument("unsupported WAV extensible subformat");
                format = u16(fmt.data() + 24);
            }
            if ((format != 1 && format != 3) || channels == 0 || channels > max_channels ||
                (format == 1 && bits != 16 && bits != 24 && bits != 32) ||
                (format == 3 && bits != 32) || block_align != channels * (bits / 8) ||
                rate < 8000 || rate > 768000)
                throw std::invalid_argument("unsupported WAV format; use PCM16/24/32 or float32");
            have_format = true;
        } else if (!std::memcmp(chunk.data(), "data", 4)) {
            if (have_data) throw std::invalid_argument("multiple WAV data chunks unsupported");
            data_offset = payload; data_size = size; have_data = true;
        }
        offset = payload + size + (size & 1U);
        if (offset > declared_end) throw std::invalid_argument("missing WAV chunk padding");
    }
    if (!have_format || !have_data || data_size == 0 || data_size % block_align != 0)
        throw std::invalid_argument("missing or misaligned WAV data");
    return {path,rate,channels,bits,format,data_offset,static_cast<Sample>(data_size/block_align)};
}
void WavFile::read(Sample first, std::span<float> output) const {
    if (first < 0 || first > frame_count || output.size()%channels ||
        output.size()/channels > static_cast<std::size_t>(frame_count-first))
        throw std::invalid_argument("invalid WAV read range");
    std::ifstream in(path,std::ios::binary);
    if (!in) throw std::runtime_error("cannot open WAV asset");
    in.seekg(static_cast<std::streamoff>(data_offset+static_cast<std::uint64_t>(first)*channels*(bits/8)));
    std::vector<unsigned char> bytes(output.size()*(bits/8));
    read_exact(in,bytes.data(),bytes.size());
    std::size_t cursor{};
    for (auto& sample : output) {
        const auto* word = bytes.data()+cursor; cursor += bits/8;
        if (format == 3) sample = std::bit_cast<float>(u32(word));
        else if (bits == 16) sample = static_cast<float>(std::bit_cast<std::int16_t>(u16(word))) / 32768.0F;
        else if (bits == 24) {
            std::int32_t value = static_cast<std::int32_t>(word[0] | (static_cast<std::uint32_t>(word[1]) << 8) |
                                                        (static_cast<std::uint32_t>(word[2]) << 16));
            if (value & 0x800000) value -= 0x1000000;
            sample = static_cast<float>(value) / 8388608.0F;
        } else sample = static_cast<float>(std::bit_cast<std::int32_t>(u32(word))) / 2147483648.0F;
        if (!std::isfinite(sample)) throw std::invalid_argument("non-finite WAV sample");
    }
}
AudioData load_wav(const std::filesystem::path& path, std::size_t max_decoded_bytes) {
    const auto file = inspect_wav(path);
    const auto samples = static_cast<std::uint64_t>(file.frame_count)*file.channels;
    if (samples > max_decoded_bytes/sizeof(float)) throw std::invalid_argument("WAV preload memory budget exceeded");
    AudioData result{file.sample_rate,file.channels,{}};
    result.samples.resize(static_cast<std::size_t>(samples));
    // Bounded temporary decode storage even for a full preload.
    for (Sample first = 0; first < file.frame_count; first += 8192) {
        const auto count = std::min(Sample{8192},file.frame_count-first);
        file.read(first,std::span<float>(result.samples).subspan(static_cast<std::size_t>(first)*file.channels,static_cast<std::size_t>(count)*file.channels));
    }
    result.validate(); return result;
}
CabIr load_cab_ir(const std::filesystem::path& path){
    const auto file=inspect_wav(path);if(file.channels>2||file.sample_rate>192000||file.frame_count>file.sample_rate)throw std::invalid_argument("Cab IR requires mono/stereo WAV, 8–192 kHz, at most one second");
    const auto decoded=load_wav(path,192000*2*sizeof(float));CabIr result;const auto name=path.filename().u8string();result.name.assign(name.begin(),name.end());result.sample_rate=decoded.sample_rate;result.channels=decoded.channels;result.samples=decoded.samples;result.validate();return result;
}
AudioData open_wav(const std::filesystem::path& path, std::size_t preload_bytes) {
    auto file = std::make_shared<const WavFile>(inspect_wav(path));
    if (static_cast<std::uint64_t>(file->frame_count)*file->channels <= preload_bytes/sizeof(float))
        return load_wav(path,preload_bytes);
    AudioData result{file->sample_rate,file->channels,{},file};
    result.validate(); return result;
}
AudioData sine_fixture(std::uint32_t rate, std::uint32_t channels, Sample frames, double frequency) {
    if (rate < 8000 || rate > 768000 || channels == 0 || channels > max_channels ||
        frames <= 0 || frames > 10'000'000 || !std::isfinite(frequency) || frequency <= 0 || frequency >= rate / 2.0)
        throw std::invalid_argument("invalid sine fixture");
    AudioData result;
    result.sample_rate = rate; result.channels = channels;
    result.samples.resize(static_cast<std::size_t>(frames) * channels);
    for (Sample frame = 0; frame < frames; ++frame) {
        const auto sample = static_cast<float>(0.1 * std::sin(2 * std::numbers::pi * frequency * static_cast<double>(frame) / rate));
        for (std::uint32_t channel = 0; channel < channels; ++channel)
            result.samples[static_cast<std::size_t>(frame) * channels + channel] = sample;
    }
    return result;
}
} // namespace mrs::audio
