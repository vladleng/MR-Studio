#include "TranscriptionRuntime.h"
#include <iostream>
// Explicit Windows installer integration check. No Medium weights or user audio.
int main(int argc,char** argv){try{
    if(argc!=2)return 2;
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-python-install-check-"+juce::Uuid().toString());
    struct Cleanup{juce::File root;~Cleanup(){mr::separation::nativeRuntimeFile(root).deleteRecursively();}} cleanup{root};
    // Failed preflight must leave actionable diagnostics, not a stale pip log.
    const auto failureRoot=root.getChildFile("failed-preflight");
    ui::TranscriptionRuntime missing(failureRoot,failureRoot.getChildFile("absent-requirements.txt"));
    bool failed=false;
    try{missing.install({});}catch(const std::exception& e){
        failed=juce::String(e.what()).contains("runtime_requirements_missing")&&juce::String(e.what()).contains("preflight");
    }
    const auto failureLog=failureRoot.getChildFile("runtime-last-operation.log").loadFileAsString();
    if(!failed||!failureLog.contains("START preflight")||!failureLog.contains("FAILED preflight: runtime_requirements_missing")||missing.present())
        throw std::runtime_error("Failed installation lost its stage/error diagnostics");
    std::cout<<"PASS failed runtime preflight retains stage/error diagnostics\n";
    // Regression: JUCE/Python can create paths longer than MAX_PATH even when
    // this console executable has no longPathAware application manifest.
    const auto inventoryRoot=mr::separation::nativeRuntimeFile(root.getChildFile("long-path-inventory"));
    auto nested=inventoryRoot;
    for(int i=0;i<12;++i)nested=nested.getChildFile("nested-package-directory");
    const auto payload=nested.getChildFile("dependency_links.txt");
    if(!nested.createDirectory().wasOk()||!payload.replaceWithText("self-generated long-path fixture"))throw std::runtime_error("Cannot create long-path fixture");
    std::atomic<bool> stop{};
    mr::separation::publishRuntimeInventory(inventoryRoot,"long-path-fixture",{},stop);
    mr::separation::verifyRuntimeInventory(inventoryRoot,"long-path-fixture",stop);
    std::cout<<"PASS long-path inventory with reparse-point checks\n";
    ui::TranscriptionRuntime runtime(root,juce::File(juce::String(argv[1])));try{runtime.install({});runtime.verify({});}catch(...){std::cerr<<root.getChildFile("runtime-last-operation.log").loadFileAsString().toStdString()<<'\n';throw;}
    const auto installLog=root.getChildFile("runtime-last-operation.log").loadFileAsString();
    for(const auto* marker:{"START install pip","START install Torch CPU","START install requirements","START runtime probe","SUCCESS"})
        if(!installLog.contains(marker))throw std::runtime_error("Installation log lost an earlier stage");
    {auto lease=runtime.acquire();bool denied=false;try{runtime.remove();}catch(const std::exception&){denied=true;}if(!denied)throw std::runtime_error("Runtime lease did not protect installation");}
    if(!runtime.directory().getChildFile("resolved-packages.txt").replaceWithText("corrupted fixture"))throw std::runtime_error("Cannot inject inventory corruption");
    bool corrupt=false;try{runtime.verify({});}catch(const std::exception&){corrupt=true;}if(!corrupt)throw std::runtime_error("Runtime corruption was not detected");
    runtime.remove();if(runtime.present())throw std::runtime_error("Runtime removal failed");
    std::cout<<"PASS Windows isolated Python Medium install/verify/lease/remove\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
