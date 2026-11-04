#pragma once
// NON-RT separation adapter: scipy.signal.resample_poly default Kaiser policy.
// Kept separate from the DAW's accepted WAV-import resampler, whose filter differs.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>

namespace mrs::separation::native {
inline std::uint64_t separationFrameCount(std::uint64_t frames,unsigned sourceRate,unsigned targetRate){
    if(!sourceRate||!targetRate||frames>96000ULL*600)throw std::invalid_argument("Invalid separation timebase");
    return (frames*targetRate+sourceRate/2)/sourceRate;
}
inline std::vector<float> separationResample(std::span<const float> input,unsigned channels,
    unsigned sourceRate,unsigned targetRate,std::size_t outputFrames,
    const std::function<void()>& check={}){
    if(check)check();
    if((channels!=1&&channels!=2)||input.empty()||input.size()%channels||sourceRate<8000||sourceRate>96000||
       targetRate<8000||targetRate>96000||!outputFrames||outputFrames>96000ULL*600)
        throw std::invalid_argument("Invalid separation SRC geometry");
    const std::size_t frames=input.size()/channels;
    if(frames>96000ULL*600||!std::all_of(input.begin(),input.end(),[](float value){return std::isfinite(value);}))
        throw std::invalid_argument("Invalid separation SRC input");
    const auto divisor=std::gcd(sourceRate,targetRate);
    const std::uint64_t up=targetRate/divisor,down=sourceRate/divisor;
    const auto available=(frames*up+down-1)/down;
    if(outputFrames>available)throw std::invalid_argument("Separation SRC output extent exceeds reference");
    if(up==1&&down==1){
        if(outputFrames>frames)throw std::invalid_argument("Identity SRC extent exceeds input");
        std::vector<float> output(input.begin(),input.end());output.resize(outputFrames*channels);return output;
    }
    const std::size_t half=10*static_cast<std::size_t>(std::max(up,down));
    const auto length=half*2+1;
    std::vector<double> design(length);
    const double cutoff=1.0/static_cast<double>(std::max(up,down));
    const double windowDenominator=std::cyl_bessel_i(0.0,5.0);
    double sum{};
    for(std::size_t i=0;i<length;++i){
        if(check&&i%4096==0)check();
        const double distance=static_cast<double>(i)-static_cast<double>(half);
        const double x=cutoff*distance;
        const double sinc=x==0.0?1.0:std::sin(std::numbers::pi*x)/(std::numbers::pi*x);
        const double coordinate=distance/static_cast<double>(half);
        const double window=std::cyl_bessel_i(0.0,5.0*std::sqrt(std::max(0.0,1.0-coordinate*coordinate)))/windowDenominator;
        design[i]=cutoff*sinc*window;sum+=design[i];
    }
    if(!std::isfinite(sum)||sum<=0)throw std::runtime_error("Invalid separation SRC filter");
    std::vector<float> filter(length);
    for(std::size_t i=0;i<length;++i){
        // Reference firwin is float64, casts to input float32, then multiplies
        // by up in float32. Preserve that order rather than one double product.
        filter[i]=static_cast<float>(design[i]/sum);
        filter[i]*=static_cast<float>(up);
    }
    std::vector<float> output(outputFrames*channels);
    for(std::size_t frame=0;frame<outputFrames;++frame){
        if(check&&frame%4096==0)check();
        // scipy's zero prepad / n_pre_remove cancel to this centered index.
        // Constant-zero exterior, symmetric Kaiser window, no added latency.
        const std::uint64_t center=frame*down+half;
        const std::uint64_t first=center>2*half?(center-2*half+up-1)/up:0;
        const auto last=std::min<std::uint64_t>(frames-1,center/up);
        for(unsigned channel=0;channel<channels;++channel){
            float value{};
            for(auto source=first;source<=last;++source){
                const auto tap=static_cast<std::size_t>(center-source*up);
                value+=input[static_cast<std::size_t>(source)*channels+channel]*filter[tap];
            }
            if(!std::isfinite(value))throw std::runtime_error("Non-finite separation SRC output");
            output[frame*channels+channel]=value;
        }
    }
    if(check)check();
    return output;
}
}
