#include <mrs/desktop.hpp>
#include <mrs/sha256.hpp>
#include <iostream>
#include <thread>

// Developer-only full offline path. No model download or implicit terms consent.
int main(int argc,char** argv){
    if(argc!=7||std::string(argv[6])!="--acknowledge-model-terms"){
        std::cerr<<"usage: mrs_transcription_controller_check <ace-midi> <official-medium-dir> <source.wav> <NEW-work-dir> <exe-sha256> --acknowledge-model-terms\n";return 2;
    }
    namespace fs=std::filesystem;using namespace mrs;using namespace mrs::transcription;
    struct Cleanup {fs::path path;bool keep{};~Cleanup(){if(!keep&&!path.empty()){std::error_code ec;fs::remove_all(path,ec);}}} cleanup;
    try{
        const auto work=fs::absolute(argv[4]);if(!fs::create_directory(work))throw std::runtime_error("Work directory must be new");cleanup.path=work;
        const auto jobs=work/"jobs";fs::create_directory(jobs);ModelStore models(work/"models");
        std::cout<<"Installing official local Medium into private model store\n"<<std::flush;
        models.install(fs::absolute(argv[2]),true);
        Request request;request.executable=fs::absolute(argv[1]);request.executable_sha256=argv[5];request.temporary_root=jobs;
        const auto source=fs::absolute(argv[3]);const auto source_hash=sha256_file(source);const auto info=audio::inspect_wav(source);
        desktop::Application app;app.new_project(info.sample_rate);app.import_wavs({source});
        const auto before=*app.services().projects->state().project;
        app.queue_transcription(before.clips.front().id,models,request);
        int previous=-1;const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(35);
        for(;;){
            app.poll();const auto status=app.transcription_status();
            if(status.completed!=previous){previous=status.completed;std::cout<<"progress "<<status.completed<<'/'<<status.total<<'\n'<<std::flush;}
            if(status.state!=JobState::running&&status.state!=JobState::cancelling)break;
            if(std::chrono::steady_clock::now()>deadline){app.cancel_transcription();throw std::runtime_error("Controller check deadline");}
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if(app.transcription_status().state!=JobState::ready)throw std::runtime_error("No ready notes: "+app.transcription_status().error);
        const auto* preview=app.transcription_preview();std::cout<<"raw_notes "<<preview->raw_notes<<" source_bounded_notes "<<preview->notes.size()<<'\n';
        const auto placed=app.place_transcription();const auto after=*app.services().projects->state().project;
        if(!app.undo()||*app.services().projects->state().project!=before||!app.redo()||*app.services().projects->state().project!=after)throw std::runtime_error("Undo/Redo mismatch");
        const auto first=work/"converted.mrsproject";app.save_project(first);const auto saved=app.snapshot().project;app.open_project(first);
        if(*app.services().projects->state().project!=saved)throw std::runtime_error("Save/Open mismatch");
        const auto second=work/"converted-as.mrsproject";app.save_project(second);const auto saved_as=app.snapshot().project;app.open_project(second);
        if(*app.services().projects->state().project!=saved_as||sha256_file(source)!=source_hash||!fs::is_empty(jobs))throw std::runtime_error("Save As/source/cleanup mismatch");
        models.remove();cleanup.keep=true;
        std::cout<<"PASS controller placement: "<<placed.size()<<" parts, Undo/Redo, Save/Open/Save As, source preserved, jobs/model cleaned\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
