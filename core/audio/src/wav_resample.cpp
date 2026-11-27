#include <mrs/wav_resample.hpp>
#include <mrs/audio.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace mrs::audio {
#ifdef _WIN32
namespace detail {
Mp3ExportResult encode_mp3_windows(const WavFile&,const std::filesystem::path&,
                                  const std::function<void(float)>&);
}
#endif
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
namespace {
namespace fs=std::filesystem;
constexpr std::uint32_t export_rate=48000;
constexpr std::uint64_t riff_payload=0xffffffffULL-36;
struct ExportCancelled {};
struct ExportStage {
    fs::path directory;
    explicit ExportStage(const fs::path& destination){
        directory=destination.parent_path()/(".mrs-export-"+new_id().value);
        if(!fs::create_directory(directory))throw std::runtime_error("Cannot create private export staging directory");
    }
    ~ExportStage(){std::error_code ignored;fs::remove_all(directory,ignored);}
    ExportStage(const ExportStage&)=delete;
    ExportStage& operator=(const ExportStage&)=delete;
};
bool occupied(const fs::path& path){
    std::error_code ec;const auto status=fs::symlink_status(path,ec);
    if(ec&&ec!=std::errc::no_such_file_or_directory)throw fs::filesystem_error("Cannot inspect export destination",path,ec);
    return status.type()!=fs::file_type::not_found&&status.type()!=fs::file_type::none;
}
class ExportWavWriter {
    std::ofstream out_;
    Sample expected_,written_{};
    bool pcm24_;
    WavExportResult stats_{};
    std::vector<unsigned char> bytes_;
public:
    ExportWavWriter(const fs::path& path,std::uint32_t rate,Sample frames,bool pcm24)
        :out_(path,std::ios::binary|std::ios::trunc),expected_(frames),pcm24_(pcm24) {
        if(!out_)throw std::runtime_error("Cannot create staged WAV");
        const auto width=pcm24?3u:4u;
        const auto bytes=static_cast<std::uint64_t>(frames)*2*width;
        if(!frames||bytes>riff_payload)throw std::invalid_argument("WAV exceeds RIFF size limit");
        const auto put=[&](std::uint32_t n,unsigned count){for(unsigned i=0;i<count;++i)out_.put(static_cast<char>((n>>(8*i))&255));};
        out_.write("RIFF",4);put(static_cast<std::uint32_t>(bytes+36),4);out_.write("WAVEfmt ",8);
        put(16,4);put(pcm24?1u:3u,2);put(2,2);put(rate,4);put(rate*2*width,4);
        put(2*width,2);put(width*8,2);out_.write("data",4);put(static_cast<std::uint32_t>(bytes),4);
        if(!out_)throw std::runtime_error("Cannot write staged WAV header");
    }
    void write(std::span<const float> stereo){
        if(stereo.size()%2||stereo.size()/2>static_cast<std::uint64_t>(expected_-written_))
            throw std::runtime_error("Export sink received unexpected frame count");
        const auto width=pcm24_?3u:4u;
        // Bound temporary encoding storage independently of render block size.
        for(std::size_t first=0;first<stereo.size();){
            const auto count=std::min<std::size_t>(8192,stereo.size()-first);
            bytes_.resize(count*width);
            for(std::size_t i=0;i<count;++i){
                const float sample=stereo[first+i];
                if(!std::isfinite(sample))throw std::runtime_error("Non-finite export sample");
                stats_.peak=std::max(stats_.peak,std::abs(sample));
                std::uint32_t word{};
                if(pcm24_){
                    if(sample>1.f||sample< -1.f)++stats_.clipped_samples;
                    const auto scaled=std::clamp(static_cast<double>(sample),-1.,1.)*8388608.;
                    const auto quantized=std::clamp(std::llround(scaled),-8388608LL,8388607LL);
                    word=static_cast<std::uint32_t>(quantized);
                }else word=std::bit_cast<std::uint32_t>(sample);
                for(unsigned b=0;b<width;++b)bytes_[i*width+b]=static_cast<unsigned char>((word>>(8*b))&255);
            }
            out_.write(reinterpret_cast<const char*>(bytes_.data()),static_cast<std::streamsize>(bytes_.size()));
            if(!out_)throw std::runtime_error("Cannot write export WAV (disk full?)");
            first+=count;
        }
        written_+=static_cast<Sample>(stereo.size()/2);
    }
    WavExportResult finish(){
        if(written_!=expected_)throw std::runtime_error("Incomplete WAV render");
        out_.flush();out_.close();if(!out_)throw std::runtime_error("Cannot finalize export WAV");
        stats_.completed=true;stats_.frames=written_;return stats_;
    }
};
}
namespace {
WavExportResult export_file(const std::filesystem::path& requested,std::uint32_t source_rate,
        Sample source_frames,const MasterRenderSource& render,const WavExportCallbacks& callbacks,bool mp3){
    if(source_rate<8000||source_rate>768000||source_frames<=0||!render)
        throw std::invalid_argument("Invalid WAV export source");
    // Check multiplication before computing output geometry, including the
    // existing resampler's intermediate float32 RIFF size limit.
    const bool convert=source_rate!=export_rate;
    const bool stage_float=convert||mp3;
    if(static_cast<std::uint64_t>(source_frames)>riff_payload/(stage_float?8u:6u))
        throw std::invalid_argument("Export exceeds RIFF size limit");
    const auto frames=(static_cast<std::uint64_t>(source_frames)*export_rate+source_rate-1)/source_rate;
    if(!frames||frames>riff_payload/(stage_float?8u:6u))throw std::invalid_argument("Export exceeds RIFF size limit");
    if(callbacks.stop.stop_requested())return {};
    const auto destination=fs::absolute(requested).lexically_normal();
    if(destination.filename().empty()||occupied(destination))throw std::invalid_argument("Export destination already exists; choose a new filename");
    ExportStage staging(destination);
    const auto output=staging.directory/(mp3?"output.mp3":"output.wav");
    const auto input=stage_float?staging.directory/"source.wav":output;
    auto pcm=input;
    const auto check_cancel=[&]{if(callbacks.stop.stop_requested())throw ExportCancelled{};};
    const auto conversion_progress=[&](float p){
        check_cancel();
        if(callbacks.continue_conversion&&!callbacks.continue_conversion(p))throw ExportCancelled{};
    };
    try{
        WavExportResult result;
        {
            ExportWavWriter writer(input,source_rate,source_frames,!stage_float);
            const auto rendered=render({
                [&](std::span<const float> block){check_cancel();writer.write(block);},
                [&](const OfflineRenderProgress& p){
                    return !callbacks.stop.stop_requested()&&(!callbacks.continue_render||callbacks.continue_render(p));
                }
            });
            if(!rendered.completed)return {};
            check_cancel();
            if(rendered.frames_written!=source_frames)throw std::runtime_error("Unexpected rendered WAV duration");
            result=writer.finish();
        }
        if(convert){
            const auto resampled=staging.directory/"resampled.wav";
            conversion_progress(0.f);
            // Use the existing bounded windowed-sinc import SRC; no separate
            // resampling algorithm and no integer quantization before SRC.
            try{
                resample_wav_file(input,resampled,export_rate,callbacks.stop,[&](float p){conversion_progress(p*.5f);});
            }catch(const std::runtime_error& error){
                if(callbacks.stop.stop_requested()&&std::string_view(error.what())=="WAV import cancelled")throw ExportCancelled{};
                throw;
            }
            check_cancel();
            pcm=resampled;
        }
        if(mp3){
#ifdef _WIN32
            conversion_progress(convert?.5f:0.f);
            const auto wav=inspect_wav(pcm);
            if(wav.sample_rate!=export_rate||wav.channels!=2||wav.frame_count!=static_cast<Sample>(frames))
                throw std::runtime_error("Unexpected MP3 input geometry");
            result=detail::encode_mp3_windows(wav,output,[&](float p){conversion_progress(convert?.5f+.5f*p:p);});
#else
            throw std::runtime_error("MP3 encoder requires Windows Media Foundation");
#endif
        }else if(convert){
            const auto wav=inspect_wav(pcm);
            if(wav.sample_rate!=export_rate||wav.channels!=2||wav.frame_count!=static_cast<Sample>(frames))
                throw std::runtime_error("Unexpected export SRC geometry");
            ExportWavWriter writer(output,export_rate,wav.frame_count,true);
            std::vector<float> block(8192*2);
            for(Sample first=0;first<wav.frame_count;){
                check_cancel();
                const auto count=std::min<Sample>(8192,wav.frame_count-first);
                auto data=std::span<float>(block).first(static_cast<std::size_t>(count)*2);
                wav.read(first,data);writer.write(data);first+=count;
                conversion_progress(.5f+.5f*static_cast<float>(static_cast<double>(first)/wav.frame_count));
            }
            result=writer.finish();
        }
        check_cancel();
        // No-replace publication even if another writer creates the destination
        // during rendering. Staging is on the same filesystem.
#ifdef _WIN32
        if(!MoveFileExW(output.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot publish WAV; destination may already exist");
#else
        fs::create_hard_link(output,destination);
#endif
        return result;
    }catch(const ExportCancelled&){return {};}
}
}
WavExportResult export_wav24(const std::filesystem::path& destination,std::uint32_t source_rate,
        Sample source_frames,const MasterRenderSource& render,const WavExportCallbacks& callbacks){
    return export_file(destination,source_rate,source_frames,render,callbacks,false);
}
StemWavExportResult export_stem_wavs24(const std::filesystem::path& requested,
        std::uint32_t source_rate,Sample source_frames,std::size_t stem_count,
        const StemRenderSource& render,const WavExportCallbacks& callbacks){
    if(source_rate<8000||source_rate>768000||source_frames<=0||!render||
       !stem_count||stem_count>max_mixer_tracks)
        throw std::invalid_argument("Invalid stem export source");
    const bool convert=source_rate!=export_rate;
    const auto limit=riff_payload/(convert?8u:6u);
    if(static_cast<std::uint64_t>(source_frames)>limit)
        throw std::invalid_argument("Stem exceeds RIFF size limit");
    const auto frames=(static_cast<std::uint64_t>(source_frames)*export_rate+source_rate-1)/source_rate;
    if(!frames||frames>limit)throw std::invalid_argument("Stem exceeds RIFF size limit");
    if(callbacks.stop.stop_requested())return {};
    const auto destination=fs::absolute(requested).lexically_normal();
    if(destination.filename().empty()||occupied(destination))
        throw std::invalid_argument("Stem export directory already exists; choose a new directory");
    ExportStage staging(destination);
    const auto payload=staging.directory/"stems";
    if(!fs::create_directory(payload))throw std::runtime_error("Cannot create staged stem directory");
    const auto output=[&](std::size_t i){return payload/("stem-"+std::to_string(i+1)+".wav");};
    const auto input=[&](std::size_t i){return convert?
        staging.directory/("source-"+std::to_string(i+1)+".wav"):output(i);};
    const auto check_cancel=[&]{if(callbacks.stop.stop_requested())throw ExportCancelled{};};
    const auto conversion_progress=[&](std::size_t i,float p){
        check_cancel();
        if(callbacks.continue_conversion&&!callbacks.continue_conversion(
            (static_cast<float>(i)+p)/static_cast<float>(stem_count)))throw ExportCancelled{};
    };
    try{
        StemWavExportResult result;
        result.stems.reserve(stem_count);
        {
            std::vector<std::unique_ptr<ExportWavWriter>> writers;
            writers.reserve(stem_count);
            for(std::size_t i=0;i<stem_count;++i){
                check_cancel();
                writers.push_back(std::make_unique<ExportWavWriter>(input(i),source_rate,source_frames,!convert));
            }
            const auto rendered=render({
                [&](std::size_t index,std::span<const float> block){
                    check_cancel();
                    if(index>=writers.size())throw std::runtime_error("Unexpected stem sink index");
                    writers[index]->write(block);
                },
                [&](const OfflineRenderProgress& p){
                    return !callbacks.stop.stop_requested()&&(!callbacks.continue_render||callbacks.continue_render(p));
                }
            });
            if(!rendered.completed)return {};
            check_cancel();
            if(rendered.frames_written!=source_frames)throw std::runtime_error("Unexpected rendered stem duration");
            for(auto& writer:writers)result.stems.push_back(writer->finish());
        }
        if(convert){
            std::vector<float> block(8192*2);
            for(std::size_t i=0;i<stem_count;++i){
                const auto resampled=staging.directory/"resampled.wav";
                conversion_progress(i,0.f);
                try{
                    resample_wav_file(input(i),resampled,export_rate,callbacks.stop,
                        [&](float p){conversion_progress(i,p*.5f);});
                }catch(const std::runtime_error& error){
                    if(callbacks.stop.stop_requested()&&std::string_view(error.what())=="WAV import cancelled")throw ExportCancelled{};
                    throw;
                }
                const auto wav=inspect_wav(resampled);
                if(wav.sample_rate!=export_rate||wav.channels!=2||wav.frame_count!=static_cast<Sample>(frames))
                    throw std::runtime_error("Unexpected stem SRC geometry");
                ExportWavWriter writer(output(i),export_rate,wav.frame_count,true);
                for(Sample first=0;first<wav.frame_count;){
                    check_cancel();
                    const auto count=std::min<Sample>(8192,wav.frame_count-first);
                    auto data=std::span<float>(block).first(static_cast<std::size_t>(count)*2);
                    wav.read(first,data);writer.write(data);first+=count;
                    conversion_progress(i,.5f+.5f*static_cast<float>(static_cast<double>(first)/wav.frame_count));
                }
                result.stems[i]=writer.finish();
                fs::remove(resampled);
                fs::remove(input(i));
            }
        }
        check_cancel();
        // An atomic, no-replace directory move is required; a sequence of file
        // publications could expose a partial set or overwrite another export.
#ifdef _WIN32
        if(!MoveFileExW(payload.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot publish stem directory; destination may already exist");
#elif defined(__linux__) && defined(SYS_renameat2)
        if(::syscall(SYS_renameat2,AT_FDCWD,payload.c_str(),AT_FDCWD,destination.c_str(),RENAME_NOREPLACE)!=0)
            throw fs::filesystem_error("Cannot publish stem directory",payload,destination,std::error_code(errno,std::generic_category()));
#else
        throw std::runtime_error("Atomic no-replace stem directory publication is unavailable on this platform");
#endif
        result.completed=true;
        return result;
    }catch(const ExportCancelled&){return {};}
}
Mp3ExportResult export_mp3(const std::filesystem::path& destination,std::uint32_t source_rate,
        Sample source_frames,const MasterRenderSource& render,const WavExportCallbacks& callbacks){
#ifdef _WIN32
    return export_file(destination,source_rate,source_frames,render,callbacks,true);
#else
    (void)destination;(void)source_rate;(void)source_frames;(void)render;(void)callbacks;
    throw std::runtime_error("MP3 encoder requires Windows Media Foundation");
#endif
}
}
