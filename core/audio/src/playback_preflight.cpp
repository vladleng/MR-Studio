#include <mrs/playback_preflight.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mrs::audio {
std::vector<PlaybackCheckResult> check_playback_sources(std::span<const PlaybackSourceCheck> requests,
    std::uint32_t rate,std::span<const int> outputs,std::stop_token stop){
    if(requests.size()>32768||rate<8000||rate>768000||outputs.size()>max_channels)
        throw std::invalid_argument("Invalid Playback inspection request");
    std::vector<PlaybackCheckResult> results;results.reserve(requests.size());
    for(const auto& request:requests){
        if(stop.stop_requested())return {};
        PlaybackCheckResult result;result.key=request.key;
        const auto fail=[&](PlaybackCheckCode code,const char* detail){result.code=code;result.detail=detail;};
        try{
            if(!std::filesystem::is_regular_file(request.file))fail(PlaybackCheckCode::missing,"WAV is missing. Choose a replacement source.");
            else{
                const PlaybackFileStamp before{std::filesystem::file_size(request.file),std::filesystem::last_write_time(request.file)};
                const auto info=inspect_wav(request.file);result.source_rate=info.sample_rate;result.resample=info.sample_rate!=rate;
                const long double frames=std::ceil(static_cast<long double>(info.frame_count)*rate/info.sample_rate);
                if(frames<=0||frames>max_sample)throw std::invalid_argument("WAV extent is unsupported");
                result.frames=static_cast<Sample>(frames);
                const auto last=request.end<0?static_cast<double>(result.frames)/rate:request.end;
                if(!std::isfinite(request.begin)||!std::isfinite(request.end)||request.begin<0||request.end < -1||
                   last<=request.begin||request.begin>static_cast<double>(max_sample)/rate||last>static_cast<double>(max_sample)/rate||
                   std::llround(last*rate)>result.frames)
                    fail(PlaybackCheckCode::trim,"Trim exceeds this WAV. Relink a compatible source or edit its range.");
                if(result.code==PlaybackCheckCode::ok&&request.regions){
                    if(request.regions->size()>max_voices)fail(PlaybackCheckCode::regions,"Too many audio regions.");
                    for(const auto& region:*request.regions)if(region.offset<0||region.length<=0||region.offset>result.frames||region.length>result.frames-region.offset)
                        fail(PlaybackCheckCode::regions,"An audio region exceeds its WAV. Relink the source or edit its range.");
                }
                if(result.code==PlaybackCheckCode::ok&&request.expected&&before!=*request.expected)
                    fail(PlaybackCheckCode::changed,"WAV changed since preparation. Choose it again to reload.");
                const PlaybackFileStamp after{std::filesystem::file_size(request.file),std::filesystem::last_write_time(request.file)};
                if(before!=after)fail(PlaybackCheckCode::changed,"WAV changed during inspection. Check again.");
            }
        }catch(const std::exception& error){result.code=PlaybackCheckCode::unreadable;result.detail=std::string("Cannot inspect WAV: ")+error.what();}
        if(result.code==PlaybackCheckCode::ok){
            if(request.outputs.size()>2||std::any_of(request.outputs.begin(),request.outputs.end(),[&](int output){return std::find(outputs.begin(),outputs.end(),output)==outputs.end();}))
                fail(PlaybackCheckCode::outputs,"A physical output is unavailable. Choose Master or enable that output.");
        }
        if(result.code==PlaybackCheckCode::ok)result.detail=result.resample?"Source checked; resampling is required during preparation.":"Source checked; audio preparation is validated on activation.";
        results.push_back(std::move(result));
    }
    return results;
}
}
