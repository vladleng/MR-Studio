#pragma once
#include <string>
#include <sstream>
inline std::string without_marker_colors(std::string bytes){
    for(auto pos=bytes.find("TRACKCOLOR ");pos!=std::string::npos;pos=bytes.find("TRACKCOLOR "))bytes.erase(pos,bytes.find('\n',pos)-pos+1);
    for(auto pos=bytes.find("AUDIOFADE ");pos!=std::string::npos;pos=bytes.find("AUDIOFADE "))bytes.erase(pos,bytes.find('\n',pos)-pos+1);
    for(auto pos=bytes.find("CLIPCOLOR ");pos!=std::string::npos;pos=bytes.find("CLIPCOLOR "))bytes.erase(pos,bytes.find('\n',pos)-pos+1);
    const auto at=bytes.find("MARKERS ");if(at==std::string::npos)return bytes;
    auto line=bytes.find('\n',at)+1;std::size_t count{};std::istringstream(bytes.substr(at+8))>>count;
    for(std::size_t i=0;i<count;++i){auto end=bytes.find('\n',line);auto last=bytes.rfind(' ',end);auto first=bytes.rfind(' ',last-1);bytes.erase(first,end-first);line=first+1;}
    return bytes;
}
inline std::string without_fx_roles(std::string bytes){
    for(auto at=bytes.find("FX_ROLE ");at!=std::string::npos;at=bytes.find("FX_ROLE "))bytes.erase(at,bytes.find('\n',at)-at+1);
    return without_marker_colors(std::move(bytes));
}
// Migration fixtures remove the schema-14 track section before assigning an older header.
inline std::string without_midi_outputs(std::string bytes){
    for(auto at=bytes.find("PRESET_NAME ");at!=std::string::npos;at=bytes.find("PRESET_NAME ")){
        const auto end=bytes.find('\n',at);bytes.erase(at,end-at+1);
    }
    for(auto at=bytes.find("MIDIOUT ");at!=std::string::npos;at=bytes.find("MIDIOUT ")){
        const auto end=bytes.find('\n',at);bytes.erase(at,end-at+1);
    }
    return without_fx_roles(std::move(bytes));
}
