#include <mrs/wav_resample.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmreg.h>
#include <wrl/client.h>

namespace mrs::audio::detail {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT hr,const char* context){
    if(FAILED(hr))throw std::runtime_error(std::string(context)+" (HRESULT "+std::to_string(static_cast<long>(hr))+")");
}
// Load only trusted system DLLs when MP3 export is requested. Windows N or
// stripped systems may lack Media Foundation; WAV and Live must still start.
struct SystemModule {
    HMODULE handle{};
    explicit SystemModule(const wchar_t* name):handle(LoadLibraryExW(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){
        if(!handle)throw std::runtime_error("Windows Media Foundation unavailable; install Media Feature Pack");
    }
    ~SystemModule(){FreeLibrary(handle);}
    SystemModule(const SystemModule&)=delete;
    SystemModule& operator=(const SystemModule&)=delete;
    template<class F> F load(const char* name)const{
        const auto address=GetProcAddress(handle,name);
        if(!address)throw std::runtime_error(std::string("Windows MP3 API unavailable: ")+name);
        // GetProcAddress returns FARPROC; copying the representation avoids
        // MSVC C4191 while preserving the declared SDK calling convention.
        static_assert(sizeof(F)==sizeof(address));F function{};
        std::memcpy(&function,&address,sizeof(function));return function;
    }
};
struct MediaApi {
    SystemModule platform{L"mfplat.dll"},readwrite{L"mfreadwrite.dll"};
    decltype(&MFStartup) startup=platform.load<decltype(&MFStartup)>("MFStartup");
    decltype(&MFShutdown) shutdown=platform.load<decltype(&MFShutdown)>("MFShutdown");
    decltype(&MFCreateAttributes) attributes=platform.load<decltype(&MFCreateAttributes)>("MFCreateAttributes");
    decltype(&MFCreateMediaType) media_type=platform.load<decltype(&MFCreateMediaType)>("MFCreateMediaType");
    decltype(&MFInitMediaTypeFromWaveFormatEx) wave_type=platform.load<decltype(&MFInitMediaTypeFromWaveFormatEx)>("MFInitMediaTypeFromWaveFormatEx");
    decltype(&MFCreateMemoryBuffer) buffer=platform.load<decltype(&MFCreateMemoryBuffer)>("MFCreateMemoryBuffer");
    decltype(&MFCreateSample) sample=platform.load<decltype(&MFCreateSample)>("MFCreateSample");
    decltype(&MFCreateSinkWriterFromURL) writer=readwrite.load<decltype(&MFCreateSinkWriterFromURL)>("MFCreateSinkWriterFromURL");
};
struct ComApartment {
    bool owned{};
    ComApartment(){const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(hr!=RPC_E_CHANGED_MODE){check(hr,"COM initialization failed");owned=true;}}
    ~ComApartment(){if(owned)CoUninitialize();}
};
struct MediaFoundation {
    MediaApi& api;
    explicit MediaFoundation(MediaApi& a):api(a){check(api.startup(MF_VERSION,MFSTARTUP_FULL),"Windows Media Foundation unavailable");}
    ~MediaFoundation(){(void)api.shutdown();}
};
void set(IMFAttributes* type,REFGUID key,UINT32 value){check(type->SetUINT32(key,value),"MP3 media attribute");}
void set(IMFAttributes* type,REFGUID key,REFGUID value){check(type->SetGUID(key,value),"MP3 media attribute");}
// Validate all MPEG audio frames, allowing ID3 metadata. No silent bitrate
// downgrade, mono conversion, unsupported format or truncated output.
void validate_mp3(const std::filesystem::path& path,Sample frames){
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("Cannot inspect encoded MP3");
    in.seekg(0,std::ios::end);const auto size=in.tellg();in.seekg(0);
    if(size<10)throw std::runtime_error("Empty MP3 output");
    std::array<unsigned char,10> tag{};
    if(!in.read(reinterpret_cast<char*>(tag.data()),10))throw std::runtime_error("Incomplete MP3");
    std::uint64_t offset{};
    if(std::memcmp(tag.data(),"ID3",3)==0){
        for(std::size_t i=6;i<10;++i)if(tag[i]&128u)throw std::runtime_error("Invalid MP3 tag");
        offset=10u+(static_cast<std::uint64_t>(tag[6])<<21)+(static_cast<std::uint64_t>(tag[7])<<14)+(static_cast<std::uint64_t>(tag[8])<<7)+tag[9];
        if(tag[3]==4&&(tag[5]&16u))offset+=10;
    }
    const auto bytes=static_cast<std::uint64_t>(size);std::uint64_t count{};
    while(offset<bytes){
        in.seekg(static_cast<std::streamoff>(offset));
        std::array<unsigned char,4> h{};
        if(!in.read(reinterpret_cast<char*>(h.data()),4))throw std::runtime_error("Truncated MP3 frame");
        if(bytes-offset==128&&std::memcmp(h.data(),"TAG",3)==0){offset=bytes;break;}
        if(h[0]!=255||(h[1]&0xfeu)!=0xfau||(h[2]&0xfcu)!=0xe4u||(h[3]&0xc0u)==0xc0u)
            throw std::runtime_error("MP3 is not MPEG-1 Layer III stereo 48 kHz / 320 kbps CBR");
        const auto length=960u+((h[2]>>1)&1u);
        if(length>bytes-offset)throw std::runtime_error("Truncated MP3 frame");
        offset+=length;++count;
    }
    if(!count||count*1152u<static_cast<std::uint64_t>(frames))
        throw std::runtime_error("MP3 duration shorter than requested PCM");
}
}
Mp3ExportResult encode_mp3_windows(const WavFile& wav,const std::filesystem::path& output,
                                  const std::function<void(float)>& progress){
    if(wav.sample_rate!=48000||wav.channels!=2||wav.frame_count<=0)
        throw std::invalid_argument("MP3 input must be stereo 48 kHz");
    Mp3ExportResult stats{};
    {
        ComApartment apartment;MediaApi api;MediaFoundation foundation(api);
        ComPtr<IMFAttributes> attributes;
        check(api.attributes(&attributes,2),"Create MP3 writer attributes");
        set(attributes.Get(),MF_TRANSCODE_CONTAINERTYPE,MFTranscodeContainerType_MP3);
        set(attributes.Get(),MF_SINK_WRITER_DISABLE_THROTTLING,1u);
        ComPtr<IMFSinkWriter> writer;
        check(api.writer(output.c_str(),nullptr,attributes.Get(),&writer),"Create Windows MP3 writer");
        ComPtr<IMFMediaType> compressed;
        check(api.media_type(&compressed),"Create MP3 output media type");
        MPEGLAYER3WAVEFORMAT mp3{};
        mp3.wfx.wFormatTag=WAVE_FORMAT_MPEGLAYER3;mp3.wfx.nChannels=2;
        mp3.wfx.nSamplesPerSec=48000;mp3.wfx.nAvgBytesPerSec=40000;
        mp3.wfx.nBlockAlign=1;mp3.wfx.cbSize=MPEGLAYER3_WFX_EXTRA_BYTES;
        mp3.wID=MPEGLAYER3_ID_MPEG;mp3.fdwFlags=MPEGLAYER3_FLAG_PADDING_OFF;
        mp3.nBlockSize=960;mp3.nFramesPerBlock=1;
        check(api.wave_type(compressed.Get(),&mp3.wfx,static_cast<UINT32>(sizeof(mp3))),"Set CBR MP3 output type");
        DWORD stream{};
        check(writer->AddStream(compressed.Get(),&stream),"Configure 320 kbps MP3 stream");
        ComPtr<IMFMediaType> pcm;
        check(api.media_type(&pcm),"Create PCM input type");
        WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;
        format.nSamplesPerSec=48000;format.nAvgBytesPerSec=192000;format.nBlockAlign=4;format.wBitsPerSample=16;
        check(api.wave_type(pcm.Get(),&format,static_cast<UINT32>(sizeof(format))),"Set PCM16 input media type");
        check(writer->SetInputMediaType(stream,pcm.Get(),nullptr),"Configure Windows PCM to MP3 encoder");
        check(writer->BeginWriting(),"Start MP3 encoding");
        constexpr Sample block_frames=1152*8;
        std::vector<float> samples(static_cast<std::size_t>(block_frames)*2);
        std::vector<std::int16_t> encoded(samples.size());
        for(Sample first=0;first<wav.frame_count;){
            const auto count=std::min(block_frames,wav.frame_count-first);
            const auto sample_count=static_cast<std::size_t>(count)*2;
            wav.read(first,std::span<float>(samples).first(sample_count));
            for(std::size_t i=0;i<sample_count;++i){
                const auto v=samples[i];stats.peak=std::max(stats.peak,std::abs(v));
                if(v>1.f||v< -1.f)++stats.clipped_samples;
                encoded[i]=static_cast<std::int16_t>(std::clamp(std::llround(std::clamp(static_cast<double>(v),-1.,1.)*32768.),-32768LL,32767LL));
            }
            const auto bytes=static_cast<DWORD>(sample_count*sizeof(std::int16_t));
            ComPtr<IMFMediaBuffer> buffer;check(api.buffer(bytes,&buffer),"Allocate MP3 input buffer");
            BYTE* target{};check(buffer->Lock(&target,nullptr,nullptr),"Lock MP3 input buffer");
            std::memcpy(target,encoded.data(),bytes);check(buffer->Unlock(),"Unlock MP3 input buffer");
            check(buffer->SetCurrentLength(bytes),"Set MP3 input length");
            ComPtr<IMFSample> sample;check(api.sample(&sample),"Create MP3 input sample");
            check(sample->AddBuffer(buffer.Get()),"Attach MP3 input buffer");
            const auto start=first*10000000LL/48000;
            const auto end=(first+count)*10000000LL/48000;
            check(sample->SetSampleTime(start),"Set MP3 sample timestamp");
            check(sample->SetSampleDuration(end-start),"Set MP3 sample duration");
            check(writer->WriteSample(stream,sample.Get()),"Encode MP3 audio");
            first+=count;if(progress)progress(static_cast<float>(.99*static_cast<double>(first)/static_cast<double>(wav.frame_count)));
        }
        check(writer->Finalize(),"Finalize MP3");
    }
    validate_mp3(output,wav.frame_count);
    if(progress)progress(1.f);
    stats.frames=wav.frame_count;stats.completed=true;return stats;
}
}
