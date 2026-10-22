#include <mrs/sample_preview.hpp>
#include <mrs/audio.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace mrs::library {
namespace fs=std::filesystem;
std::optional<fs::path> asset_path(const Snapshot& snap,std::int64_t id){
    const auto item=std::find_if(snap.assets.begin(),snap.assets.end(),
        [id](const AudioAsset& a){return a.id==id;});
    if(item==snap.assets.end()||item->status!="ok")return {};
    const auto source=std::find_if(snap.sources.begin(),snap.sources.end(),
        [&](const Source& s){return s.id==item->source_id&&s.enabled;});
    if(source==snap.sources.end()||source->root.empty())return {};
    const auto relative=fs::path(std::u8string(item->relative_path.begin(),item->relative_path.end()));
    auto extension=relative.extension().u8string();
    for(auto& letter:extension)if(letter>=u8'A'&&letter<=u8'Z')letter+=u8'a'-u8'A';
    if(relative.empty()||relative.is_absolute()||relative.has_root_path()||
        extension!=u8".wav")return {};
    for(const auto& component:relative)
        if(component=="."||component=="..")return {};
    const auto root=source->root.lexically_normal();
    const auto resolved=(root/relative).lexically_normal();
    if(resolved.parent_path()==resolved||resolved==root)return {};
    // Reparse points must not redirect a catalog-relative path outside source.
    // This is control/worker only. Do not perform filesystem I/O in audio callback.
    std::error_code ec;
    if(!fs::is_regular_file(resolved,ec)||ec)return {};
    for(auto walk=resolved;walk!=root;walk=walk.parent_path()){
        const auto status=fs::symlink_status(walk,ec);
        if(ec||fs::is_symlink(status))return {};
        if(walk==walk.root_path()||walk==walk.parent_path())return {};
#ifdef _WIN32
        const auto attributes=GetFileAttributesW(walk.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return {};
#endif
    }
    return resolved;
}
WavPreview build_wav_preview(const fs::path& path,
                             const std::shared_ptr<std::atomic<bool>>& cancel){
    if(cancel&&cancel->load())throw std::runtime_error("Waveform cancelled");
    const auto wav=mrs::audio::inspect_wav(path);
    WavPreview out;
    out.path=path;out.rate=wav.sample_rate;out.channels=wav.channels;out.bits=wav.bits;
    out.frames=static_cast<std::uint64_t>(wav.frame_count);
    const auto frameStep=std::max<std::uint64_t>(1,(out.frames+255)/256);
    std::vector<float> block(static_cast<std::size_t>(4096)*wav.channels);
    for(std::uint64_t first=0;first<out.frames;first+=4096){
        if(cancel&&cancel->load())throw std::runtime_error("Waveform cancelled");
        const auto frames=static_cast<std::uint32_t>(std::min<std::uint64_t>(4096,out.frames-first));
        wav.read(static_cast<mrs::Sample>(first),
                 std::span<float>(block).first(static_cast<std::size_t>(frames)*wav.channels));
        for(std::uint32_t i=0;i<frames;++i){
            float peak{};
            for(std::uint32_t c=0;c<wav.channels;++c)
                peak=std::max(peak,std::abs(block[static_cast<std::size_t>(i)*wav.channels+c]));
            const auto index=std::min<std::uint64_t>(255,(first+i)/frameStep);
            out.peaks[static_cast<std::size_t>(index)]=
                std::max(out.peaks[static_cast<std::size_t>(index)],peak);
        }
    }
    return out;
}
AudioTypeAnalysis analyse_wav_type(const fs::path& path,const std::function<bool()>& cancelled){
    const auto check=[&]{if(cancelled&&cancelled())throw std::runtime_error("Type analysis cancelled");};
    check();const auto wav=mrs::audio::inspect_wav(path);
    const auto frames=static_cast<std::uint64_t>(wav.frame_count);
    if(!frames||!wav.sample_rate)return {{},"Empty audio",0.};
    if(wav.channels>32||frames>static_cast<std::uint64_t>(wav.sample_rate)*90||frames>16000000u/wav.channels)
        return {{},"Audio exceeds bounded Type analysis budget",0.};
    const auto hop=std::max<std::uint32_t>(1,wav.sample_rate/100); // approximately 10 ms, rate independent
    std::vector<double> energy(static_cast<std::size_t>((frames+hop-1)/hop),0.);
    std::vector<float> pcm(static_cast<std::size_t>(4096)*wav.channels);
    for(std::uint64_t first=0;first<frames;first+=4096){
        check();const auto count=static_cast<std::uint32_t>(std::min<std::uint64_t>(4096,frames-first));
        wav.read(static_cast<mrs::Sample>(first),std::span<float>(pcm).first(static_cast<std::size_t>(count)*wav.channels));
        for(std::uint32_t i=0;i<count;++i){
            double sum{};for(std::uint32_t c=0;c<wav.channels;++c){const double value=pcm[static_cast<std::size_t>(i)*wav.channels+c];if(std::isfinite(value))sum+=value*value;}
            energy[static_cast<std::size_t>((first+i)/hop)]+=sum/wav.channels;
        }
    }
    for(std::size_t i=0;i<energy.size();++i)energy[i]=std::sqrt(energy[i]/static_cast<double>(std::min<std::uint64_t>(hop,frames-i*hop)));
    // Suppress carrier beating between the audio frequency and 10 ms windows;
    // only slower musical envelopes may count as repetition.
    auto smooth=energy;
    for(std::size_t i=0;i<energy.size();++i){const auto first=i>2?i-2:0,last=std::min(energy.size(),i+3);double sum{};
        for(auto j=first;j<last;++j)sum+=energy[j];
        smooth[i]=sum/static_cast<double>(last-first);}
    energy=std::move(smooth);
    const auto peak=*std::max_element(energy.begin(),energy.end());
    if(peak<.00003)return {{},"Silence or insufficient signal",0.};
    const double tick=static_cast<double>(hop)/wav.sample_rate,duration=static_cast<double>(frames)/wav.sample_rate;
    std::vector<double> attacks;
    for(std::size_t i=0;i<energy.size();++i){
        double baseline{};for(std::size_t j=i>3?i-3:0;j<i;++j)baseline+=energy[j]/3.;
        const auto time=static_cast<double>(i)*tick;
        if(energy[i]>peak*.12&&energy[i]>baseline*1.8&&(attacks.empty()||time-attacks.back()>=.08))attacks.push_back(time);
    }
    // At least three separate attacks on a repeated time grid, with the complete
    // file ending on that grid. Tempo alone and duration alone are not evidence.
    if(duration>=.65&&attacks.size()>=3){
        for(std::size_t i=1;i<attacks.size();++i){
            const double step=attacks[i]-attacks[i-1];if(step<.12||step>1.5)continue;
            std::size_t aligned{};
            for(const auto attack:attacks)if(std::abs((attack-attacks.front())/step-std::round((attack-attacks.front())/step))*step<.035)++aligned;
            const bool boundary=std::abs(duration/step-std::round(duration/step))*step<.06;
            if(aligned*10>=attacks.size()*9&&boundary&&attacks.back()-attacks.front()>=step*2-.035)
                return {"Loop","Repeated attacks and beat-aligned duration",.94};
        }
    }
    // Detect repeating envelopes in legato/material without sharp attacks.
    std::vector<double> change(energy.size(),0.);
    for(std::size_t i=3;i<energy.size();++i)change[i]=energy[i]-energy[i-3];
    double mean{};for(const auto value:change)mean+=value/static_cast<double>(change.size());
    double variance{};for(const auto value:change)variance+=(value-mean)*(value-mean);
    if(duration>=1.&&variance>peak*peak*static_cast<double>(energy.size())*.0001){
        for(std::size_t lag=static_cast<std::size_t>(.25/tick);lag<energy.size()/2&&static_cast<double>(lag)*tick<=4.;++lag){
            check();double cross{},a{},b{};
            for(std::size_t i=lag;i<energy.size();++i){const auto x=change[i]-mean,y=change[i-lag]-mean;cross+=x*y;a+=x*x;b+=y*y;}
            const auto cycles=duration/(static_cast<double>(lag)*tick);
            if(a>0&&b>0&&a/b>.65&&a/b<1.55&&cross/std::sqrt(a*b)>.93&&std::abs(cycles-std::round(cycles))<.05)
                return {"Loop","Repeating energy envelope and cycle-aligned duration",.88};
        }
    }
    double tail{};const auto tailBins=std::min<std::size_t>(energy.size(),std::max<std::size_t>(1,static_cast<std::size_t>(.1/tick)));
    for(std::size_t i=energy.size()-tailBins;i<energy.size();++i)tail+=energy[i]/static_cast<double>(tailBins);
    if(duration<.65)return {"One-shot","Short isolated sound",.9};
    if(attacks.size()<=2&&tail<peak*.2)return {"One-shot","Isolated attack or swell with a decaying tail",.91};
    return {{},"No reliable isolated-hit or repeating-loop evidence",0.};
}
} // namespace mrs::library
