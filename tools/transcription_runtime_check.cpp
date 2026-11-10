#include "TranscriptionRuntime.h"
#include <iostream>
// Explicit Windows installer integration check. No Medium weights or user audio.
int main(int argc,char** argv){try{
    if(argc!=2)return 2;
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-python-install-check-"+juce::Uuid().toString());
    struct Cleanup{juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    ui::TranscriptionRuntime runtime(root,juce::File(juce::String(argv[1])));try{runtime.install({});runtime.verify({});}catch(...){std::cerr<<root.getChildFile("runtime-last-operation.log").loadFileAsString().toStdString()<<'\n';throw;}
    {auto lease=runtime.acquire();bool denied=false;try{runtime.remove();}catch(const std::exception&){denied=true;}if(!denied)throw std::runtime_error("Runtime lease did not protect installation");}
    if(!runtime.directory().getChildFile("resolved-packages.txt").replaceWithText("corrupted fixture"))throw std::runtime_error("Cannot inject inventory corruption");
    bool corrupt=false;try{runtime.verify({});}catch(const std::exception&){corrupt=true;}if(!corrupt)throw std::runtime_error("Runtime corruption was not detected");
    runtime.remove();if(runtime.present())throw std::runtime_error("Runtime removal failed");
    std::cout<<"PASS Windows isolated Python Medium install/verify/lease/remove\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
