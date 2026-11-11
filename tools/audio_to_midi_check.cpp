#include <mrs/audio_to_midi_worker.hpp>
#include <iostream>
#include <map>
#include <mrs/sha256.hpp>
#include <cstdlib>
#include <utility>

// Developer-only NON-RT driver. All trust hashes are explicit arguments; no
// download, automatic trust-on-first-use or installed-model claim.
int main(int argc,char** argv){
    if(argc!=8&&argc!=10&&argc!=13){std::cerr<<"usage: mrs_audio_to_midi_check <python> <model-dir> <source.wav> <temp-root> <exe-sha256> <weights-sha256> <config-sha256>\n";return 2;}
    mrs::transcription::Request r;r.executable=std::filesystem::absolute(argv[1]);r.model_directory=std::filesystem::absolute(argv[2]);r.source=std::filesystem::absolute(argv[3]);r.temporary_root=std::filesystem::absolute(argv[4]);
    r.executable_sha256=argv[5];r.weights_sha256=argv[6];r.config_sha256=argv[7];
    r.model_id="MuScriptor/muscriptor-medium";r.model_revision="f32236969308476e01fd3aae67357de5feb05a2d";r.model_license="CC BY-NC 4.0 + official HF specific conditions";
    if(argc>=10){r.cpu_threads=static_cast<unsigned>(std::stoul(argv[8]));if(std::string(argv[9])!="auto")r.stem_instrument=argv[9];}
    if(argc==13)r.range=mrs::transcription::SourceRange{std::stoll(argv[10]),std::stoll(argv[11]),static_cast<std::uint32_t>(std::stoul(argv[12]))};
    if(const auto* python=std::getenv("MR_TRANSCRIPTION_PYTHON_WORKER")){r.backend=mrs::transcription::Backend::python_medium;r.worker_script=std::filesystem::absolute(python);r.worker_sha256=mrs::sha256_file(r.worker_script);r.memory_bytes=8ULL*1024*1024*1024;if(!r.stem_instrument.empty())r.instruments.push_back(std::exchange(r.stem_instrument,{}));}
    if(const auto* compatibility=std::getenv("MR_TRANSCRIPTION_COMPATIBILITY_CPU"))r.compatibility_cpu=std::string(compatibility)=="1";
    if(std::getenv("MR_TRANSCRIPTION_MUSICAL_CHECK")){r.musical.bpm=120;r.musical.quantize=true;r.musical.subdivision=3;r.musical.strength=.5;r.musical.fill_gaps=.5;}
    if(r.worker_script.empty()){std::cerr<<"Set MR_TRANSCRIPTION_PYTHON_WORKER to the packaged Python adapter\n";return 2;}
    const auto result=mrs::transcription::run(r,{},[](int completed,int total){std::cout<<"progress "<<completed<<'/'<<total<<'\n'<<std::flush;});
    if(result.state!=mrs::transcription::State::ready&&result.state!=mrs::transcription::State::empty){std::cerr<<"failed: "<<result.error<<'\n'<<result.diagnostics;return 1;}
    std::cout<<(result.state==mrs::transcription::State::empty?"empty":"ready")<<" raw_notes="<<result.raw_notes<<" source_bounded_notes="<<result.notes.size()<<" midi_sha256="<<result.midi_sha256<<'\n';
    std::cout<<"cpu_threads="<<result.cpu_threads<<" analysis_seconds="<<result.analysis_seconds<<" elapsed_seconds="<<result.elapsed_seconds<<" source_offset_seconds="<<result.source_offset_seconds<<'\n';
    std::cout<<"peak_memory_bytes="<<result.peak_memory_bytes<<" cpu_capability="<<result.cpu_capability<<" model_load="<<result.model_load_seconds<<" inference="<<result.inference_seconds<<"\n";
    std::map<std::string,std::size_t> roles;for(const auto& n:result.notes)++roles[n.instrument];for(const auto& [role,count]:roles)std::cout<<role<<' '<<count<<'\n';
    return 0;
}
