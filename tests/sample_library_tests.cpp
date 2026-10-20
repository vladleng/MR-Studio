#include <mrs/sample_library.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
using namespace mrs::library;
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Fixture {
    fs::path root;
    Fixture() {
        root=fs::temp_directory_path()/("mrs-sample-index-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    }
    ~Fixture() { std::error_code ec;fs::remove_all(root,ec); }
};
void wav(const fs::path& file,int frames=400) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file,std::ios::binary|std::ios::trunc);
    const auto put=[&](std::uint32_t value,int width){for(int i=0;i<width;++i)out.put(static_cast<char>((value>>(8*i))&255));};
    out.write("RIFF",4);put(36+frames*2,4);out.write("WAVEfmt ",8);
    put(16,4);put(1,2);put(1,2);put(48000,4);put(96000,4);put(2,2);put(16,2);
    out.write("data",4);put(frames*2,4);
    for(int i=0;i<frames;++i)put(0,2);
    check(static_cast<bool>(out),"Cannot write WAV fixture");
}
template<class Pred> std::shared_ptr<const Snapshot> wait_for(SampleLibrary& library,Pred pred) {
    for(int i=0;i<1200;++i) {
        auto result=library.snapshot();
        if(pred(*result))return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("Sample Library async worker timeout: "+library.snapshot()->message);
}
const Source& only(const Snapshot& s) {
    check(s.sources.size()==1,"Expected exactly one source");
    return s.sources.front();
}
void test() {
    Fixture f;
    const auto libraryRoot=f.root/"samples";
    wav(libraryRoot/"Jazz"/"Brush_120BPM.wav");
    wav(libraryRoot/fs::path(u8"Кириллица")/fs::path(u8"Барабан.wav"),800);
    wav(libraryRoot/"Ambient"/"Brush_120BPM.wav"); // identical basename in a different folder
    std::ofstream(libraryRoot/"bad.wav")<<"broken";
    std::ofstream(libraryRoot/"not-midi.mid")<<"Not indexed";
    const auto database=f.root/"db"/"samples.sqlite";
    std::int64_t stableId{};
    {
        SampleLibrary lib(database);
        wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle;});
        lib.add_source(libraryRoot);
        auto s=wait_for(lib,[](const Snapshot& s){return s.phase==Phase::idle&&s.sources.size()==1&&s.sources.front().available==3;});
        check(only(*s).invalid==1 && only(*s).missing==0,"WAV count/invalid failed");
        stableId=only(*s).id;
        const auto version=s->revision;
        lib.rescan();
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>version;});
        check(s->changed==0,"Unchanged rescans must avoid re-inspecting files");
        lib.add_source(libraryRoot); // duplicate source must be idempotent
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>s->revision;});
        check(s->sources.size()==1,"Duplicate source was created");
        fs::remove(libraryRoot/"Ambient"/"Brush_120BPM.wav");
        wav(libraryRoot/"Jazz"/"Brush_120BPM.wav",801);
        const auto revision=s->revision;
        lib.rescan();
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>revision&&x.sources.front().missing==1;});
        check(only(*s).available==2&&only(*s).invalid==1&&only(*s).missing==1,"Missing/update state incorrect");
        const auto revision2=s->revision;
        lib.enable_source(stableId,false);
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>revision2;});
        check(!only(*s).enabled,"Source enable switch not persisted");
    }
    {
        SampleLibrary lib(database);
        auto s=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
        check(only(*s).id==stableId&&!only(*s).enabled&&only(*s).missing==1,"Restart/persistence failed");
        const auto moved=f.root/"relocated";
        fs::rename(libraryRoot,moved);
        lib.relink_source(stableId,moved);
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.sources.front().root==moved;});
        check(only(*s).id==stableId,"Relink lost stable ID");
        lib.enable_source(stableId,true);
        s=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle&&x.sources.front().enabled;});
        const auto revision=s->revision;
        lib.remove_source(stableId);
        s=wait_for(lib,[&](const Snapshot& x){return x.phase==Phase::idle&&x.revision>revision&&x.sources.empty();});
        check(fs::exists(moved/"Jazz"/"Brush_120BPM.wav"),"Source removal altered user files");
    }
    // A missing root is reported, but cannot wipe a previously committed index.
    {
        SampleLibrary lib(database);
        wait_for(lib,[](const Snapshot& x){return x.phase==Phase::idle;});
        lib.add_source(f.root/"not-there");
        auto s=wait_for(lib,[](const Snapshot& x){return x.phase==Phase::error;});
        check(only(*s).available==0,"Missing root should not create phantom samples");
    }
    std::cout<<"MR Library #252: SQLite, unicode, duplicate names, incremental scan, relink, restart PASS\n";
}
}
int main() { try {test();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;} }
