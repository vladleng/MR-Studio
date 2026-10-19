#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>
#include <stop_token>
#include <stdexcept>
namespace mrs::processing::detail {
// Cab IR's existing offline windowed-sinc calculation, including rate scaling.
// No trim/normalisation. Callers validate rates, channels and allocation budgets.
inline std::vector<float> resample_ir(std::span<const float> source, std::uint32_t channels,
    std::uint32_t source_rate, std::uint32_t rate,std::stop_token stop = {}) {
    const auto source_frames=source.size()/channels;
    const auto frames=static_cast<std::size_t>((static_cast<std::uint64_t>(source_frames)*rate+source_rate-1)/source_rate);
    std::vector<float> result(frames*channels);
    const double ratio=static_cast<double>(source_rate)/rate,cutoff=std::min(1.,1./ratio);
    for(std::size_t f=0;f<frames;++f)for(std::size_t ch=0;ch<channels;++ch){
        if((f&255)==0&&stop.stop_requested())throw std::runtime_error("IR resampling cancelled");
        if(source_rate==rate){result[f*channels+ch]=source[f*channels+ch];continue;}
        const double position=f*ratio;const auto center=static_cast<std::int64_t>(position);double value{};
        for(std::int64_t k=center-32;k<=center+32;++k){
            if(k<0||static_cast<std::uint64_t>(k)>=source_frames)continue;
            const double d=position-k;if(std::abs(d)>=32)continue;
            const double x=std::numbers::pi*d*cutoff,sinc=std::abs(x)<1e-12?1.:std::sin(x)/x;
            const double window=0.5+0.5*std::cos(std::numbers::pi*d/32);
            value+=source[static_cast<std::size_t>(k)*channels+ch]*sinc*window*cutoff*ratio;
        }
        result[f*channels+ch]=static_cast<float>(value);
    }
    return result;
}
}
