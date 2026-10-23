#include <mrs/stretch_cache.hpp>
#include <mrs/arrangement.hpp>
#include <algorithm>
#include <bit>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mrs::audio {
namespace {
struct Directory {
    std::filesystem::path path;
    bool owned{};
    ~Directory(){if(owned){std::error_code ec;std::filesystem::remove_all(path,ec);}}
};
auto stamp(const AudioData& source){
    if(!source.file)return std::pair{std::filesystem::file_time_type{},std::uintmax_t{}};
    return std::pair{std::filesystem::last_write_time(source.file->path),std::filesystem::file_size(source.file->path)};
}
}
bool StretchCache::source_current() const {
    try{return stamp(*source)==std::pair{source_time,source_bytes};}catch(const std::filesystem::filesystem_error&){return false;}
}
std::string stretch_cache_key(const AudioData& source,const Clip& clip){
    validate_audio_stretch(clip);
    if(!clip.stretch)throw std::invalid_argument("Missing stretch state");
    // Full semantic key, no lossy hash. Windows/split windows share the same PCM.
    std::ostringstream key;key.imbue(std::locale::classic());
    key<<"cache-2 "<<stretch::backend_revision()<<' '<<static_cast<const void*>(&source)<<' '
       <<source.sample_rate<<' '<<source.channels<<' '<<source.frames()<<' '
       <<clip.source_offset<<' '<<clip.stretch->source_frames<<' '<<stretch::serialize(clip.stretch->parameters);
    return key.str();
}
std::shared_ptr<const StretchCache> prepare_stretch_cache(std::shared_ptr<const AudioData> source,const Clip& clip,
    std::stop_token stop,std::function<void(float)> progress){
    const auto cancel=[&]{if(stop.stop_requested())throw std::runtime_error("Time / pitch preparation cancelled");};
    cancel();if(!source)throw std::invalid_argument("Missing stretch source");source->validate();validate_audio_stretch(clip);
    if(!clip.stretch||stretch::neutral(clip.stretch->parameters))throw std::invalid_argument("No processed cache needed");
    const auto frames=audio_output_extent(clip);
    const auto bytes=static_cast<std::uintmax_t>(frames)*source->channels*4;
    if(source->channels>2||clip.source_offset>source->frames()||clip.stretch->source_frames>source->frames()-clip.source_offset)
        throw std::invalid_argument("Unsupported stretch source / selection");
    if(bytes>512*1024*1024)throw std::invalid_argument("Time / pitch cache exceeds 512 MiB; split the source region first");
    const auto before=stamp(*source);
    auto owned=std::make_shared<Directory>();
    owned->path=std::filesystem::temp_directory_path()/("mrs-stretch-"+new_id().value);
    if(!std::filesystem::create_directory(owned->path))throw std::runtime_error("Cannot create private stretch cache");
    owned->owned=true;
    const auto staging=owned->path/"preparing.wav",ready=owned->path/"audio.wav";
    std::ofstream out(staging,std::ios::binary);
    if(!out)throw std::runtime_error("Cannot create stretch cache WAV");
    const auto put=[&](std::uint32_t n,int width){for(int i=0;i<width;++i)out.put(static_cast<char>((n>>(8*i))&255));};
    out.write("RIFF",4);put(static_cast<std::uint32_t>(bytes+36),4);out.write("WAVEfmt ",8);
    put(16,4);put(3,2);put(source->channels,2);put(source->sample_rate,4);put(source->sample_rate*source->channels*4,4);
    put(source->channels*4,2);put(32,2);out.write("data",4);put(static_cast<std::uint32_t>(bytes),4);
    auto engine=stretch::make_signalsmith_engine();
    const stretch::Config config{source->sample_rate,source->channels,32768,4096,true};
    Sample written{};
    const auto result=stretch::render(*engine,config,clip.stretch->parameters,clip.stretch->source_frames,
        [&](Sample first,std::span<float> pcm){
            cancel();first+=clip.source_offset;
            if(source->file)source->file->read(first,pcm);
            else std::copy_n(source->samples.begin()+static_cast<std::ptrdiff_t>(first*source->channels),pcm.size(),pcm.begin());
            return true;
        },[&](Sample first,std::span<const float> pcm){
            cancel();if(first!=written)throw std::runtime_error("Nonsequential stretch cache output");
            for(float sample:pcm)put(std::bit_cast<std::uint32_t>(sample),4);
            written+=static_cast<Sample>(pcm.size()/source->channels);
            if(progress)progress(static_cast<float>(static_cast<double>(written)/static_cast<double>(frames))*.9f);
            return static_cast<bool>(out);
        },stop);
    cancel();
    if(result.status!=stretch::Status::ok||written!=frames)throw std::runtime_error("Time / pitch render failed (status "+std::to_string(static_cast<int>(result.status))+")");
    out.close();if(!out)throw std::runtime_error("Cannot finish stretch cache (disk full?)");
    if(stamp(*source)!=before)throw std::runtime_error("Source changed during time / pitch preparation");
    std::filesystem::rename(staging,ready);
    auto file=std::make_shared<WavFile>(inspect_wav(ready));file->lifetime=owned;
    auto data=std::make_shared<AudioData>();data->sample_rate=source->sample_rate;data->channels=source->channels;data->file=file;
    auto cancelled=std::make_shared<std::atomic<bool>>(false);
    std::stop_callback callback(stop,[cancelled]{*cancelled=true;});
    Waveform waveform(*data,cancelled);cancel();
    if(progress)progress(1);
    return std::make_shared<StretchCache>(StretchCache{std::move(source),std::move(data),std::move(waveform),bytes,before.first,before.second});
}
} // namespace mrs::audio
