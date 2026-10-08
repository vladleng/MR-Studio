#pragma once
#include <string>
// Migration fixtures remove the schema-14 track section before assigning an older header.
inline std::string without_midi_outputs(std::string bytes){
    for(auto at=bytes.find("MIDIOUT ");at!=std::string::npos;at=bytes.find("MIDIOUT ")){
        const auto end=bytes.find('\n',at);bytes.erase(at,end-at+1);
    }
    return bytes;
}
