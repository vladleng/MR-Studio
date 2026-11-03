#include "SeparationProject.h"
#include <mrs/desktop.hpp>
#include <mrs/sha256.hpp>
#include <bit>
#include <fstream>
#include <iostream>

namespace {
using namespace mr::separation;namespace fs=std::filesystem;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
fs::path path(const char* value){const std::string s=value;return fs::path(std::u8string(s.begin(),s.end()));}
std::string text(const fs::path& p){const auto s=p.generic_u8string();return {s.begin(),s.end()};}
juce::File file(const fs::path& p){return juce::File(juce::String::fromUTF8(text(p).c_str()));}
void wav(const fs::path& p){std::ofstream o(p,std::ios::binary);const auto put=[&](std::uint32_t v,int n){for(int i=0;i<n;++i)o.put(static_cast<char>((v>>(8*i))&255));};o.write("RIFF",4);put(36+96000*8,4);o.write("WAVEfmt ",8);put(16,4);put(3,2);put(2,2);put(48000,4);put(384000,4);put(8,2);put(32,2);o.write("data",4);put(96000*8,4);for(int i=0;i<192000;++i)put(std::bit_cast<std::uint32_t>(i%2?.1f:-.1f),4);check(static_cast<bool>(o),"Cannot write project source");}
void wait(ProjectJob& job){const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);juce::String phase;
    while(job.status().state==State::running&&std::chrono::steady_clock::now()<deadline){const auto s=job.status();if(s.phase!=phase){phase=s.phase;std::cout<<"project phase: "<<phase.toStdString()<<std::endl;}std::this_thread::sleep_for(std::chrono::milliseconds(20));}
    const auto s=job.status();if(s.state!=State::ready)throw std::runtime_error("Project job failed: "+s.error.toStdString());}
void run(const fs::path& cache,const fs::path& worker,const fs::path& requirements,const fs::path& evidence){
    check(!fs::exists(evidence),"Choose a new project evidence directory");fs::create_directories(evidence);const auto original=evidence/fs::path(u8"source [\u0410\u0443\u0434\u0438\u043e].wav");wav(original);const auto hash=mrs::sha256_file(original);
    mrs::desktop::Application app;app.import_wav(original);auto clip=app.services().projects->state().project->clips.front();clip.start=123456;clip.source_offset=12000;clip.length=48000;clip.fade_in=100;clip.fade_out=200;
    struct Range final:mrs::ICommand{mrs::Clip clip;explicit Range(mrs::Clip c):clip(std::move(c)){}std::string_view name()const override{return "Fixture trimmed source";}void apply(mrs::Project& p)const override{p.clips.front()=clip;}};
    app.services().projects->execute(Range{clip});app.save_project(evidence/"first"/"song.mrsproject");
    auto manager=std::make_shared<ModelManager>(file(cache),file(worker),file(requirements),[](const auto&,const auto&,const auto&,auto){throw std::runtime_error("Project processing must never download");});
    ProjectJob job(manager);ProjectInput input;input.clip=clip;input.source=app.source_file(clip.source);input.projectFile=app.path();input.rate=48000;input.session="project-integration";input.revision=juce::String(app.services().projects->state().revision);input.model=modelCatalogue().front().id;input.sourceLifetime=app.source_lifetime(clip.source);input.sourceGuard=app.separation_media_guard();
    check(job.separate(input),"Start actual project separation");wait(job);auto out=job.take();check(out&&out->result,"Actual project result");auto result=out->result;
    app.keep_separation(result->expected,result->record,result->media);result->media->accepted=true;
    check(app.services().projects->state().project->tracks.size()==1&&app.services().projects->state().project->clips.front()==clip,"Default hidden result/source unchanged");
    const auto r=result->record;std::vector<fs::path> paths;for(const auto& stem:r.stems)paths.push_back(app.source_file(stem.source));paths.push_back(app.source_file(r.manifest.source));
    check(job.preparePlacement(r,std::move(paths),app.separation_media_guard()),"Prepare real project stems");wait(job);out=job.take();check(out&&out->placement,"Verified project placement");app.place_separation(out->placement->expected,std::move(out->placement->audio));
    const auto placed=app.services().projects->state().project->clips;check(placed.size()==3&&placed[1].start==123456&&placed[1].length==48000&&placed[1].fade_in==100,"Actual stem geometry");
    check(app.undo()&&app.services().projects->state().project->clips.size()==1,"Actual placement Undo");check(app.redo()&&app.services().projects->state().project->clips==placed,"Actual placement Redo stable IDs");
    app.save_project(app.path());const auto moved=evidence/"moved"/"moved.mrsproject";app.save_project(moved);fs::remove_all(evidence/"first");fs::remove(original);
    mrs::desktop::Application reopened;reopened.open_project(moved);const auto saved=reopened.services().projects->state().project;check(saved->separation_results.size()==1&&saved->clips.size()==3,"Save As/Open after removal of old project");
    for(const auto& stem:saved->separation_results.front().stems)check(fs::path(stem.source).is_relative()&&mrs::sha256_file(reopened.source_file(stem.source))==stem.sha256,"Moved project content integrity");
    check(mrs::sha256_file(reopened.source_file(saved->clips.front().source))==hash,"Original content preserved in Media");
    std::cout<<"PASS actual separation project: offline managed CPU, trimmed source, hidden default, optional tracks, stable Undo/Redo, verified project assets, Save/Open/Save As and relocated project\n";
}
}
int main(int argc,char** argv){try{check(argc==5,"Usage: mrs_separation_project_check CACHE WORKER REQUIREMENTS NEW_EVIDENCE");run(path(argv[1]),path(argv[2]),path(argv[3]),path(argv[4]));return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
