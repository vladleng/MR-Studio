#include "TranscriptionRuntime.h"
#include <iostream>
// Explicit Windows installer integration check. No Medium weights or user audio.
int main(int argc,char** argv){try{
    if(argc!=2)return 2;
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-python-install-check-"+juce::Uuid().toString());
    struct Cleanup{juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    // Regression: JUCE/Python can create paths longer than MAX_PATH even when
    // this console executable has no longPathAware application manifest.
    const auto inventoryRoot=root.getChildFile("long-path-inventory");
    auto nested=inventoryRoot;
    for(int i=0;i<12;++i)nested=nested.getChildFile("nested-package-directory");
    const auto payload=nested.getChildFile("dependency_links.txt");
    if(!nested.createDirectory().wasOk()||!payload.replaceWithText("self-generated long-path fixture"))throw std::runtime_error("Cannot create long-path fixture");
    std::atomic<bool> stop{};
    mr::separation::publishRuntimeInventory(inventoryRoot,"long-path-fixture",{},stop);
    mr::separation::verifyRuntimeInventory(inventoryRoot,"long-path-fixture",stop);
    std::cout<<"PASS long-path inventory with reparse-point checks\n";
    ui::TranscriptionRuntime runtime(root,juce::File(juce::String(argv[1])));try{runtime.install({});runtime.verify({});}catch(...){std::cerr<<root.getChildFile("runtime-last-operation.log").loadFileAsString().toStdString()<<'\n';throw;}
    {auto lease=runtime.acquire();bool denied=false;try{runtime.remove();}catch(const std::exception&){denied=true;}if(!denied)throw std::runtime_error("Runtime lease did not protect installation");}
    if(!runtime.directory().getChildFile("resolved-packages.txt").replaceWithText("corrupted fixture"))throw std::runtime_error("Cannot inject inventory corruption");
    bool corrupt=false;try{runtime.verify({});}catch(const std::exception&){corrupt=true;}if(!corrupt)throw std::runtime_error("Runtime corruption was not detected");
    runtime.remove();if(runtime.present())throw std::runtime_error("Runtime removal failed");
    std::cout<<"PASS Windows isolated Python Medium install/verify/lease/remove\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
