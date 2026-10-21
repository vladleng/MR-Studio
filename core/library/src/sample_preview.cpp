#include <mrs/sample_preview.hpp>
#include <mrs/audio.hpp>
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
    if(relative.empty()||relative.is_absolute()||relative.has_root_path()||relative.extension()!=L".wav"&&
        relative.extension()!=L".WAV")return {};
    for(const auto& component:relative)
        if(component=="."||component=="..")return {};
    const auto root=source->root.lexically_normal();
    const auto resolved=(root/relative).lexically_normal();
    if(resolved.parent_path()==resolved||resolved==root)return {};
    // Reparse points must not redirect a catalog-relative path outside source.
    // This is control/worker only. Do not perform filesystem I/O in audio callback.
    std::error_code ec;
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
} // namespace mrs::library
