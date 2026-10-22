#include <mrs/stretch.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace mrs::stretch {
RenderResult render(ITimeStretchEngine& engine, const Config& config, const Parameters& parameters,
                    std::int64_t source_frames, const Read& read, const Write& write, std::stop_token stop) {
    if (!read || !write || source_frames < 0) return {Status::invalid_parameter};
    if (stop.stop_requested()) return {Status::cancelled};
    const auto prepared = engine.prepare(config, parameters);
    if (prepared != Status::ok) return {prepared};
    std::int64_t length{};
    if (output_frames(source_frames, parameters.enabled ? parameters.duration_ratio : 1, length) != Status::ok)
        return {Status::invalid_parameter};
    if (length == 0) return {};
    // This helper currently supports the adapter's mono/stereo contract.
    if (config.channels > 2) return {Status::unsupported};
    const auto latency = engine.latency();
    if (length > std::numeric_limits<std::int64_t>::max() - latency.output_frames -
        config.max_output_frames - config.max_input_frames - latency.input_frames)
        return {Status::invalid_parameter};
    const double ratio = parameters.enabled ? parameters.duration_ratio : 1;
    const long double last_input = latency.input_frames +
        (static_cast<long double>(length) + latency.output_frames)/ratio + config.max_input_frames;
    if (last_input >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
        return {Status::invalid_parameter};
    const auto input_capacity = std::max(config.max_input_frames,latency.input_frames);
    const auto output_capacity = std::max(config.max_output_frames,latency.output_frames);
    std::vector<float> input(static_cast<std::size_t>(input_capacity)*config.channels);
    std::vector<float> output(static_cast<std::size_t>(output_capacity)*config.channels);
    std::vector<float> interleaved(static_cast<std::size_t>(std::max(input_capacity,output_capacity))*config.channels);
    std::array<const float*,2> in{};
    std::array<float*,2> out{};
    for (std::uint32_t c=0;c<config.channels;++c) {
        in[c] = input.data() + static_cast<std::size_t>(c)*input_capacity;
        out[c] = output.data() + static_cast<std::size_t>(c)*output_capacity;
    }
    const auto load = [&](std::int64_t first,std::uint32_t count) {
        const auto valid = first < source_frames ? static_cast<std::uint32_t>(
            std::min<std::int64_t>(count,source_frames-first)) : 0u;
        if (valid && !read(first,std::span{interleaved}.first(static_cast<std::size_t>(valid)*config.channels))) return false;
        for (std::uint32_t c=0;c<config.channels;++c) {
            auto* channel = input.data() + static_cast<std::size_t>(c)*input_capacity;
            for (std::uint32_t i=0;i<valid;++i) channel[i] = interleaved[static_cast<std::size_t>(i)*config.channels+c];
            std::fill(channel+valid,channel+count,0.f);
        }
        return true;
    };
    // Bit-identical worker fast path; entering/leaving it in playback requires
    // the stopped latency transition from #59, not a callback branch.
    if (neutral(parameters)) {
        for (std::int64_t first=0;first<length;) {
            if (stop.stop_requested()) return {Status::cancelled,first};
            const auto n = static_cast<std::uint32_t>(std::min<std::int64_t>(config.max_output_frames,length-first));
            auto block = std::span{interleaved}.first(static_cast<std::size_t>(n)*config.channels);
            if (!read(first,block)) return {Status::io_error,first};
            if (std::any_of(block.begin(),block.end(),[](float x){return !std::isfinite(x);}))
                return {Status::invalid_buffer,first};
            if (!write(first,block)) return {Status::io_error,first};
            first += n;
        }
        return {Status::ok,length};
    }
    if (!load(0,latency.input_frames)) return {Status::io_error};
    const auto primed = engine.seek(std::span{in}.first(config.channels),latency.input_frames,1);
    if (primed != Status::ok) return {primed};
    std::int64_t source = latency.input_frames, processed{}, written{};
    const auto total = length + latency.output_frames;
    while (processed < total) {
        if (stop.stop_requested()) return {Status::cancelled,written};
        const auto n = static_cast<std::uint32_t>(std::min<std::int64_t>(config.max_output_frames,total-processed));
        const auto required = engine.input_required(n);
        if (required > config.max_input_frames) return {Status::invalid_parameter,written};
        if (!load(source,required)) return {Status::io_error,written};
        const auto result = engine.process(std::span{in}.first(config.channels),required,
                                            std::span{out}.first(config.channels),n);
        if (result.status != Status::ok) return {result.status,written};
        source += result.consumed;
        const auto begin = static_cast<std::uint32_t>(std::clamp<std::int64_t>(latency.output_frames-processed,0,n));
        const auto count = n-begin;
        for (std::uint32_t i=0;i<count;++i)
            for (std::uint32_t c=0;c<config.channels;++c)
                interleaved[static_cast<std::size_t>(i)*config.channels+c] = out[c][begin+i];
        if (count && !write(written,std::span{interleaved}.first(static_cast<std::size_t>(count)*config.channels)))
            return {Status::io_error,written};
        written += count;
        processed += n;
    }
    if (stop.stop_requested()) return {Status::cancelled,written};
    // All logical output is already drained with zero padding. Finish remaining
    // synthesis history once, discard the tail outside the logical clip extent.
    const auto drained = engine.flush(std::span{out}.first(config.channels),output_capacity);
    if (drained.status != Status::drained) return {drained.status,written};
    return {Status::ok,written};
}
} // namespace mrs::stretch
