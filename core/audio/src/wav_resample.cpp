#include <mrs/wav_resample.hpp>
#include <mrs/audio.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace mrs::audio {
void resample_wav_file(const std::filesystem::path& source,
    const std::filesystem::path& destination,std::uint32_t rate,
    std::stop_token stop,std::function<void(float)> progress){
    const auto cancel=[&]{if(stop.stop_requested())throw std::runtime_error("WAV import cancelled");};
    cancel();
    if(rate<8000||rate>768000)throw std::invalid_argument("Invalid import sample rate");
    const auto wav=inspect_wav(source);
    const auto frames=(static_cast<std::uint64_t>(wav.frame_count)*rate+wav.sample_rate-1)/wav.sample_rate;
    const auto bytes=frames*wav.channels*4;
    if(!frames||frames>static_cast<std::uint64_t>(max_sample)||bytes>0xffffffffULL-36)
        throw std::invalid_argument("Converted WAV exceeds the RIFF 4 GiB limit");
    std::error_code ec;
    const auto status=std::filesystem::symlink_status(destination,ec);
    if(status.type()!=std::filesystem::file_type::not_found&&
       status.type()!=std::filesystem::file_type::none)
        throw std::invalid_argument("Converted WAV destination already exists");
    if(std::filesystem::absolute(source).lexically_normal()==std::filesystem::absolute(destination).lexically_normal())
        throw std::invalid_argument("Never overwrite the source WAV");
    const double ratio=static_cast<double>(wav.sample_rate)/rate;
    const double bandwidth=std::min(1.,1./ratio);
    const double cutoff=bandwidth*(ratio>1.? .96:1.);
    const auto radius=static_cast<std::size_t>(std::ceil(64./bandwidth));
    const auto taps=radius*2;
    constexpr std::size_t phases=1024,block=512;
    // At the supported 96:1 rate ratio the coefficient table stays below 49 MiB.
    std::vector<float> kernel((phases+1)*taps);
    for(std::size_t phase=0;phase<=phases;++phase){
        cancel();double sum{};
        for(std::size_t tap=0;tap<taps;++tap){
            const double distance=static_cast<double>(tap)-static_cast<double>(radius)+1.-static_cast<double>(phase)/phases;
            const double angle=std::numbers::pi*distance/static_cast<double>(radius);
            const double window=std::abs(distance)>=static_cast<double>(radius)?0.:
                .35875+.48829*std::cos(angle)+.14128*std::cos(2.*angle)+.01168*std::cos(3.*angle);
            const double x=std::numbers::pi*distance*cutoff;
            const double value=(std::abs(x)<1e-14?1.:std::sin(x)/x)*cutoff*window;
            kernel[phase*taps+tap]=static_cast<float>(value);sum+=value;
        }
        for(std::size_t tap=0;tap<taps;++tap)kernel[phase*taps+tap]=static_cast<float>(kernel[phase*taps+tap]/sum);
    }
    bool created=false;
    try{
        std::ofstream out(destination,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("Cannot create converted WAV");
        created=true;
        const auto put=[&](std::uint32_t value,int width){for(int i=0;i<width;++i)out.put(static_cast<char>((value>>(8*i))&255));};
        out.write("RIFF",4);put(static_cast<std::uint32_t>(bytes+36),4);out.write("WAVEfmt ",8);
        put(16,4);put(3,2);put(wav.channels,2);put(rate,4);put(rate*wav.channels*4,4);
        put(wav.channels*4,2);put(32,2);out.write("data",4);put(static_cast<std::uint32_t>(bytes),4);
        std::vector<float> input;
        std::vector<unsigned char> encoded(block*wav.channels*4);
        for(std::uint64_t first=0;first<frames;first+=block){
            cancel();
            const auto count=std::min<std::uint64_t>(block,frames-first);
            const auto begin=std::max<std::int64_t>(0,static_cast<std::int64_t>(first*wav.sample_rate/rate)-static_cast<std::int64_t>(radius)+1);
            const auto end=std::min<std::uint64_t>(static_cast<std::uint64_t>(wav.frame_count),
                (first+count-1)*wav.sample_rate/rate+radius+1);
            input.resize(static_cast<std::size_t>(end-static_cast<std::uint64_t>(begin))*wav.channels);
            wav.read(begin,input);
            for(std::uint64_t frame=0;frame<count;++frame){
                cancel();
                const auto numerator=(first+frame)*wav.sample_rate;
                const auto center=static_cast<std::int64_t>(numerator/rate);
                const double phase=static_cast<double>(numerator%rate)*phases/rate;
                const auto index=std::min(phases-1,static_cast<std::size_t>(phase));
                const float blend=static_cast<float>(phase-static_cast<double>(index));
                for(std::uint32_t channel=0;channel<wav.channels;++channel){
                    double value{};
                    for(std::size_t tap=0;tap<taps;++tap){
                        const auto at=std::clamp(center+static_cast<std::int64_t>(tap)-static_cast<std::int64_t>(radius)+1,
                            std::int64_t{0},wav.frame_count-1);
                        const auto a=kernel[index*taps+tap],b=kernel[(index+1)*taps+tap];
                        value+=input[static_cast<std::size_t>(at-begin)*wav.channels+channel]*(a+(b-a)*blend);
                    }
                    const auto sample=static_cast<float>(value);
                    if(!std::isfinite(sample))throw std::invalid_argument("Non-finite converted WAV");
                    const auto word=std::bit_cast<std::uint32_t>(sample);
                    const auto offset=(static_cast<std::size_t>(frame)*wav.channels+channel)*4;
                    for(unsigned byte=0;byte<4;++byte)encoded[offset+byte]=static_cast<unsigned char>((word>>(8*byte))&255);
                }
            }
            out.write(reinterpret_cast<const char*>(encoded.data()),static_cast<std::streamsize>(count*wav.channels*4));
            if(!out)throw std::runtime_error("Cannot write converted WAV (disk full?)");
            if(progress)progress(static_cast<float>(static_cast<double>(first+count)/static_cast<double>(frames)));
        }
        cancel();out.close();if(!out)throw std::runtime_error("Cannot finish converted WAV");
    }catch(...){if(created){std::error_code ignored;std::filesystem::remove(destination,ignored);}throw;}
}
}
