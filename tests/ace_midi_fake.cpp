#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

int main(int argc,char** argv){
    namespace fs=std::filesystem;fs::path input,output,model,options;std::string instrument;
    for(int i=1;i+1<argc;++i){const std::string arg=argv[i];if(arg=="--transcribe")input=argv[++i];else if(arg=="--out")output=argv[++i];else if(arg=="--model")model=argv[++i];else if(arg=="--instrument"||arg=="--instruments")instrument=argv[++i];else if(arg=="--options")options=argv[++i];}
    const auto env=[](const char* k){const auto* v=std::getenv(k);return v?std::string(v):std::string{};};
    if(!env("GGML_BACKEND").empty()||env("OMP_NUM_THREADS").empty()||std::stoi(env("OMP_NUM_THREADS"))<1||std::stoi(env("OMP_NUM_THREADS"))>32||!env("HF_TOKEN").empty()||!env("ACE_MIDI_PROBE").empty())return 80;
    std::ifstream wav(input,std::ios::binary);wav.seekg(44);const auto mode=wav.get();
    if(mode==1){std::this_thread::sleep_for(std::chrono::seconds(30));return 1;}
    if(mode==2){std::cout<<std::string(5000,'x')<<std::flush;return 0;}
    if(mode==3){for(int i=0;i<1100;++i)std::cerr<<std::string(4096,'x')<<std::flush;return 0;}
    if(mode==4)return 3;
    if(mode==16){wav.seekg(24);unsigned char rate[4]{},size[4]{};wav.read(reinterpret_cast<char*>(rate),4);wav.seekg(40);wav.read(reinterpret_cast<char*>(size),4);const auto number=[](const unsigned char* p){return static_cast<unsigned>(p[0])|(static_cast<unsigned>(p[1])<<8)|(static_cast<unsigned>(p[2])<<16)|(static_cast<unsigned>(p[3])<<24);};if(number(size)!=2*number(rate))return 81;}
    const bool constrained=!instrument.empty()&&mode!=19;
    const unsigned program=constrained&&instrument=="electric_bass"?33U:0U;
    const std::string role=constrained?instrument:"acoustic_piano";
    const bool empty=mode==5;const bool padded=mode==6;const bool drums=mode==14||mode==15||(constrained&&instrument=="drums");
    const unsigned start=padded?1440:240,end=padded?1920:drums?250:720;
    std::vector<unsigned char> track;
    const auto vlq=[&](unsigned n){unsigned char data[4]{};int i=0;data[i++]=static_cast<unsigned char>(n&127);while(n>>=7)data[i++]=static_cast<unsigned char>((n&127)|128);while(i)track.push_back(data[--i]);};
    if(!empty){track={0,static_cast<unsigned char>(drums?0xc9:0xc0),static_cast<unsigned char>(mode==14?99:program)};vlq(start);track.insert(track.end(),{static_cast<unsigned char>(drums?0x99:0x90),static_cast<unsigned char>(mode==7?61:60),100});vlq(end-start);track.insert(track.end(),{static_cast<unsigned char>(drums?0x89:0x80),static_cast<unsigned char>(mode==7?61:60),0});}
    track.insert(track.end(),{0,0xff,0x2f,0});
    std::vector<unsigned char> midi{'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,'M','T','r','k',0,0,0,static_cast<unsigned char>(track.size())};midi.insert(midi.end(),track.begin(),track.end());
    if(mode!=8){std::ofstream file(output,std::ios::binary);if(mode==9)file<<"MThd";else file.write(reinterpret_cast<const char*>(midi.data()),static_cast<std::streamsize>(midi.size()));}
    if(mode==10){std::ofstream f(model/"config.json",std::ios::app);f<<'x';}
    std::ostringstream protocol;
    protocol<<"{\"type\":\"progress\",\"completed\":0,\"total\":1}\n";
    if(!empty){protocol<<"{\"type\":\"note_start\",\"index\":0,\"pitch\":60,\"time\":"<<(padded?"1.5":"0.25")<<",\"instrument\":\""<<(drums?std::string("drums"):role)<<"\"}\n";
        protocol<<"{\"type\":\"note_end\",\"index\":0,\"time\":"<<(padded?"2":drums?"0.26":"0.75")<<"}\n";}
    protocol<<"{\"type\":\"progress\",\"completed\":1,\"total\":1}\n";
    if(mode!=11)protocol<<"{\"type\":\"done\",\"notes\":"<<(empty?0:1)<<",\"midi_bytes\":"<<midi.size()<<"}\n";
    if(!options.empty()){
        std::ofstream post(output.parent_path()/"processed.jsonl",std::ios::binary);post<<protocol.str();
        std::ofstream meta(output.parent_path()/"music.txt");meta<<"MR-MUSCRIPTOR-2 DEFAULT "<<env("OMP_NUM_THREADS")<<" 0.1 0.2 0 0 0 0\n";
    }
    std::cout<<protocol.str();
    std::cout<<std::flush;return mode==12?3:mode==13?259:0;
}
