#include <mrs/stretch_cache.hpp>
#include <mrs/arrangement.hpp>
#include <mrs/sha256.hpp>
#include <mr_timestretch_drums/stretch.hpp>
#include <algorithm>
#include <bit>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace mrs::audio {
namespace {
// Private cache files are retired by one control-owned worker, never by paint,
// Undo or the audio callback. The queue owns paths only, not PCM or DSP objects.
class DirectoryRetirement {
    std::mutex mutex_;std::condition_variable wake_;
    std::deque<std::filesystem::path> paths_;bool closing_{};
    std::thread worker_{[this]{
        for(;;){std::filesystem::path path;
            {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return closing_||!paths_.empty();});
             if(paths_.empty()){if(closing_)return;continue;}path=std::move(paths_.front());paths_.pop_front();}
            std::error_code error;std::filesystem::remove_all(path,error);
        }
    }};
public:
    ~DirectoryRetirement(){{std::lock_guard lock(mutex_);closing_=true;}wake_.notify_one();worker_.join();}
    void retire(std::filesystem::path path){{std::lock_guard lock(mutex_);paths_.push_back(std::move(path));}wake_.notify_one();}
};
DirectoryRetirement& directory_retirement(){static DirectoryRetirement worker;return worker;}
struct Directory {
    std::filesystem::path path;
    bool owned{};
    ~Directory(){if(owned){try{directory_retirement().retire(path);}catch(...){std::error_code error;std::filesystem::remove_all(path,error);}}}
};
auto stamp(const AudioData& source){
    if(!source.file)return std::pair{std::filesystem::file_time_type{},std::uintmax_t{}};
    return std::pair{std::filesystem::last_write_time(source.file->path),std::filesystem::file_size(source.file->path)};
}
bool plain_path(const std::filesystem::path& p) {
    const auto status=std::filesystem::symlink_status(p);
    if(std::filesystem::is_symlink(status))return false;
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(p.c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
#endif
    return true;
}
std::filesystem::path persistent_directory(const std::filesystem::path& root) {
    auto path=std::filesystem::canonical(root);
    for(const auto* name:{"Cache","TimePitch"}){
        path/=name;
        if(!plain_path(path))throw std::runtime_error("Time / pitch cache directory must not be a link");
        std::filesystem::create_directory(path);
        if(!plain_path(path)||!std::filesystem::is_directory(path))throw std::runtime_error("Invalid Time / pitch cache directory");
    }
    return path;
}
std::string semantic_key(const AudioData& source,const Clip& clip,stretch::Backend backend,std::string_view identity){
    validate_audio_stretch(clip);
    if(!clip.stretch)throw std::invalid_argument("Missing stretch state");
    std::ostringstream key;key.imbue(std::locale::classic());
    key<<"cache-3 "<<static_cast<int>(backend)<<' ';
    if(backend==stretch::Backend::drums)key<<"mr-timestretch-drums/"<<mr::timestretch::drums::version<<";grain24;search12;attack5;detect1";
    else key<<stretch::backend_revision();
    key<<' '<<identity<<' '<<source.sample_rate<<' '<<source.channels<<' '<<source.frames()<<' '
       <<clip.source_offset<<' '<<clip.stretch->source_frames<<' '<<stretch::serialize(clip.stretch->parameters);
    return key.str();
}
}
StretchSourceFingerprint fingerprint_stretch_source(const AudioData& source,std::stop_token stop){
    source.validate();
    const auto before=stamp(source);
    if(stop.stop_requested())throw std::runtime_error("Time / pitch preparation cancelled");
    std::string digest;
    if(source.file)digest=sha256_file(source.file->path,stop);
    else digest=sha256(std::string_view(reinterpret_cast<const char*>(source.samples.data()),source.samples.size()*sizeof(float)));
    if(stop.stop_requested()||stamp(source)!=before)throw std::runtime_error("Source changed or time / pitch cancelled during fingerprint");
    return {std::move(digest),before.first,before.second};
}
void validate_stretch_disk_space(std::uintmax_t pending_bytes,std::uintmax_t available_bytes){
    constexpr std::uintmax_t reserve=64ull*1024*1024;
    if(pending_bytes&&(available_bytes<reserve||pending_bytes>available_bytes-reserve))
        throw std::runtime_error("Not enough free disk space for Time / pitch cache (64 MiB reserve)");
}
bool StretchCache::source_current() const {
    try{return stamp(*source)==std::pair{source_time,source_bytes};}catch(const std::filesystem::filesystem_error&){return false;}
}
std::string stretch_cache_key(const AudioData& source,const Clip& clip,stretch::Backend backend){
    std::ostringstream identity;identity<<static_cast<const void*>(&source);
    return semantic_key(source,clip,backend,identity.str());
}
std::shared_ptr<const StretchCache> prepare_stretch_cache(std::shared_ptr<const AudioData> source,const Clip& clip,
    std::stop_token stop,std::function<void(float)> progress,stretch::Backend backend,
    const std::filesystem::path& project_root,const StretchSourceFingerprint* fingerprint){
    const auto cancel=[&]{if(stop.stop_requested())throw std::runtime_error("Time / pitch preparation cancelled");};
    cancel();if(!source)throw std::invalid_argument("Missing stretch source");source->validate();validate_audio_stretch(clip);
    if(!clip.stretch||stretch::neutral(clip.stretch->parameters))throw std::invalid_argument("No processed cache needed");
    if(!stretch::supports(backend,clip.stretch->parameters))throw std::invalid_argument("Unsupported channel stretch parameters");
    const auto frames=audio_output_extent(clip);
    const auto bytes=static_cast<std::uintmax_t>(frames)*source->channels*4;
    if(source->channels>2||clip.source_offset>source->frames()||clip.stretch->source_frames>source->frames()-clip.source_offset)
        throw std::invalid_argument("Unsupported stretch source / selection");
    if(bytes>512*1024*1024)throw std::invalid_argument("Time / pitch cache exceeds 512 MiB; split the source region first");
    const auto before=stamp(*source);
    const auto load=[&](const std::filesystem::path& filePath,std::shared_ptr<Directory> lifetime){
        auto file=std::make_shared<WavFile>(inspect_wav(filePath));
        if(file->sample_rate!=source->sample_rate||file->channels!=source->channels||file->frame_count!=frames||
           file->format!=3||file->bits!=32||std::filesystem::file_size(filePath)!=bytes+44)
            throw std::invalid_argument("Invalid generated Time / pitch WAV");
        file->lifetime=std::move(lifetime);
        auto data=std::make_shared<AudioData>();data->sample_rate=source->sample_rate;data->channels=source->channels;data->file=file;
        auto cancelled=std::make_shared<std::atomic<bool>>(false);
        std::stop_callback callback(stop,[cancelled]{*cancelled=true;});
        Waveform waveform(*data,cancelled);cancel();
        if(stamp(*source)!=before)throw std::runtime_error("Source changed during time / pitch preparation");
        return std::make_shared<StretchCache>(StretchCache{source,std::move(data),std::move(waveform),bytes,before.first,before.second});
    };
    auto directory=std::filesystem::temp_directory_path();
    std::filesystem::path persistent;
    StretchSourceFingerprint local;
    if(!project_root.empty()){
        directory=persistent_directory(project_root);
        if(!fingerprint){local=fingerprint_stretch_source(*source,stop);fingerprint=&local;}
        if(fingerprint->time!=before.first||fingerprint->bytes!=before.second)
            throw std::runtime_error("Source changed after Time / pitch fingerprint");
        persistent=directory/(sha256(semantic_key(*source,clip,backend,fingerprint->digest))+".wav");
        if(!plain_path(persistent))throw std::runtime_error("Time / pitch cache file must not be a link");
        if(std::filesystem::exists(persistent)){
            if(!std::filesystem::is_regular_file(persistent))throw std::runtime_error("Invalid Time / pitch cache path");
            try{auto cached=load(persistent,{});if(progress)progress(1);return cached;}
            catch(const std::exception&){cancel();if(stamp(*source)!=before)throw;}
            // A corrupt/truncated generated entry is replaced only after a
            // complete new render; never touch a source WAV or user media.
        }
    }
    validate_stretch_disk_space(bytes+44,std::filesystem::space(directory).available);
    if(progress)progress(0); // distinguish a cache miss from loading a valid entry
    (void)directory_retirement();
    auto owned=std::make_shared<Directory>();
    owned->path=directory/("mrs-stretch-"+new_id().value);
    if(!std::filesystem::create_directory(owned->path))throw std::runtime_error("Cannot create private stretch cache");
    owned->owned=true;
    const auto staging=owned->path/"preparing.wav";
    std::ofstream out(staging,std::ios::binary);
    if(!out)throw std::runtime_error("Cannot create stretch cache WAV");
    const auto put=[&](std::uint32_t n,int width){for(int i=0;i<width;++i)out.put(static_cast<char>((n>>(8*i))&255));};
    out.write("RIFF",4);put(static_cast<std::uint32_t>(bytes+36),4);out.write("WAVEfmt ",8);
    put(16,4);put(3,2);put(source->channels,2);put(source->sample_rate,4);put(source->sample_rate*source->channels*4,4);
    put(source->channels*4,2);put(32,2);out.write("data",4);put(static_cast<std::uint32_t>(bytes),4);
    Sample written{};
    stretch::RenderResult result;
    if(backend==stretch::Backend::drums){
        namespace drums=mr::timestretch::drums;
        const auto count=clip.stretch->source_frames;
        if(static_cast<std::uintmax_t>(count)*source->channels*sizeof(float)>256u*1024u*1024u)throw std::invalid_argument("Drums source exceeds 256 MiB; split the region");
        std::vector<float> input(static_cast<std::size_t>(count)*source->channels);
        for(Sample first=0;first<count;){
            cancel();const auto length=std::min<Sample>(4096,count-first);
            auto block=std::span<float>(input).subspan(static_cast<std::size_t>(first)*source->channels,static_cast<std::size_t>(length)*source->channels);
            if(source->file)source->file->read(clip.source_offset+first,block);
            else std::copy_n(source->samples.begin()+(clip.source_offset+first)*source->channels,block.size(),block.begin());
            first+=length;
        }
        drums::Config cfg;cfg.sample_rate=source->sample_rate;cfg.channels=source->channels;cfg.duration_ratio=clip.stretch->parameters.duration_ratio;
        auto rendered=drums::render(input,cfg,{},stop);cancel();
        if(rendered.status!=drums::Status::ok)throw std::runtime_error("MR Timestretch Drums render failed (status "+std::to_string(static_cast<int>(rendered.status))+")");
        for(std::size_t i=0;i<rendered.samples.size();++i){if((i&4095u)==0)cancel();put(std::bit_cast<std::uint32_t>(rendered.samples[i]),4);}
        written=static_cast<Sample>(rendered.frames);result={stretch::Status::ok,written};
    }else{
    auto engine=stretch::make_signalsmith_engine();
    const stretch::Config config{source->sample_rate,source->channels,32768,4096,true};
    result=stretch::render(*engine,config,clip.stretch->parameters,clip.stretch->source_frames,
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
    }
    cancel();
    if(result.status!=stretch::Status::ok||written!=frames)throw std::runtime_error("Time / pitch render failed (status "+std::to_string(static_cast<int>(result.status))+")");
    out.close();if(!out)throw std::runtime_error("Cannot finish stretch cache (disk full?)");
    if(stamp(*source)!=before)throw std::runtime_error("Source changed during time / pitch preparation");
    // Validate all PCM and prepare peaks before publishing the immutable WAV.
    auto entry=load(staging,owned);
    cancel();
    const auto ready=persistent.empty()?owned->path/"audio.wav":persistent;
    if(!plain_path(ready))throw std::runtime_error("Time / pitch cache file must not be a link");
    std::filesystem::rename(staging,ready);
    auto file=std::make_shared<WavFile>(*entry->data->file);file->path=ready;
    if(!persistent.empty())file->lifetime.reset();
    auto data=std::make_shared<AudioData>();data->sample_rate=source->sample_rate;data->channels=source->channels;data->file=std::move(file);
    entry->data=std::move(data);
    if(progress)progress(1);
    return entry;
}
} // namespace mrs::audio
