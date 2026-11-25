#include <mrs/wav_resample.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
using namespace mrs;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
    fs::path root=fs::temp_directory_path()/("mrs-mp3-"+new_id().value);
    Fixture(){if(!fs::create_directory(root))throw std::runtime_error("MP3 test directory");}
    ~Fixture(){std::error_code ec;fs::remove_all(root,ec);}
};
}
int main(){
    try{
        Fixture f;
        const auto source=[](const audio::OfflineRenderCallbacks& cb){
            std::vector<float> block(441*2);
            Sample first{};
            for(int b=0;b<100;++b){
                for(std::size_t i=0;i<block.size();++i)block[i]=.5f*static_cast<float>(std::sin((static_cast<double>(first)+static_cast<double>(i/2))*0.0626893772));
                cb.write_stereo(block);first+=441;
                if(cb.continue_render&&!cb.continue_render({first,44100,first,44100}))return audio::OfflineRenderResult{false,first,.5f};
            }
            return audio::OfflineRenderResult{true,44100,.5f};
        };
        const auto file=f.root/"mix.mp3";
        const auto result=audio::export_mp3(file,44100,44100,source);
        check(result.completed&&result.frames==48000&&result.clipped_samples==0,"MP3 conversion result");
        check(fs::file_size(file)>=40000&&fs::file_size(file)<50000,"MP3 size for one second of 320 kbps");
        const auto cancelled=f.root/"cancel.mp3";
        check(!audio::export_mp3(cancelled,44100,44100,source,{{},[](float p){return p<.75f;}}).completed&&!fs::exists(cancelled),"MP3 cancellation");
        bool rejected=false;try{(void)audio::export_mp3(file,44100,44100,source);}catch(const std::exception&){rejected=true;}
        check(rejected&&fs::exists(file),"MP3 destination protection");
        for(const auto& entry:fs::directory_iterator(f.root))check(!entry.path().filename().string().starts_with(".mrs-export-"),"MP3 staging cleanup");
        std::cout<<"MP3 48 kHz / 320 kbps CBR export PASS\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
