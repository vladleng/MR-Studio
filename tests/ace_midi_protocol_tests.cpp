#include <mrs/ace_midi_protocol.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace mrs;
void check(bool b){if(!b)throw std::runtime_error("ace-midi protocol assertion");}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught);}
const std::string initial="{\"type\":\"progress\",\"completed\":0,\"total\":1}\n";
const std::string start="{\"type\":\"note_start\",\"index\":0,\"pitch\":60,\"time\":0.25,\"instrument\":\"acoustic_piano\"}\n";
const std::string end="{\"type\":\"note_end\",\"index\":0,\"time\":1.5}\n";
const std::string progress="{\"type\":\"progress\",\"completed\":1,\"total\":1}\n";
const std::string done="{\"type\":\"done\",\"notes\":1,\"midi_bytes\":52}\n";
void invalid(std::string s){rejects([&]{AceMidiProtocol p(1);p.feed(s);p.finish();});}
}
int main(){try{
    const auto stream=initial+start+progress+end+done;
    for(const std::size_t block:{1U,2U,7U,4096U}){
        AceMidiProtocol p(1);for(std::size_t i=0;i<stream.size();i+=block)p.feed(std::string_view(stream).substr(i,block));p.finish();
        check(p.completed()==1&&p.total()==1&&p.raw_count()==1&&p.midi_bytes()==52);
        check(p.raw_notes()[0].end==1.5&&p.notes().size()==1&&p.notes()[0].end==1.);
        rejects([&]{p.feed("\n");});rejects([&]{p.finish();});
    }
    AceMidiProtocol empty(1);empty.feed(initial+progress+"{\"type\":\"done\",\"notes\":0,\"midi_bytes\":33}\n");empty.finish();check(empty.notes().empty());
    auto after=start;after.replace(after.find("0.25"),4,"2.25");auto late=end;late.replace(late.find("1.5"),3,"2.5");
    // Pinned resampler truncates 240001 frames @48k to 80000 @16k:
    // one chunk, even though the full-source duration is slightly above 5s.
    AceMidiProtocol edge(240001./48000);edge.feed(initial+start+end+progress+done);edge.finish();check(edge.total()==1);
    AceMidiProtocol exact(240001./48000,80000);exact.feed(initial+start+end+progress+done);exact.finish();check(exact.total()==1);
    check(AceMidiProtocol(80001./16000,80001).total()==2);
    rejects([]{AceMidiProtocol p(1,0);});rejects([]{AceMidiProtocol p(1,80000);});
    AceMidiProtocol padded(1);padded.feed(initial+after+late+progress+done);padded.finish();check(padded.notes().empty()&&padded.raw_count()==1);
    rejects([]{AceMidiProtocol p(0);});rejects([]{AceMidiProtocol p(std::numeric_limits<double>::infinity());});
    invalid("");invalid(initial);invalid(start+end+progress+done);invalid(initial+start+progress+done);
    invalid(initial+end);invalid(initial+start+start);invalid(initial+start+end+end);invalid(initial+progress+start);
    invalid(initial+start+end+done);invalid(stream+initial);invalid(stream.substr(0,stream.size()-1));
    invalid(initial+"{\"type\":\"done\",\"notes\":0,\"midi_bytes\":33,\"x\":1}\n");
    invalid("{\"type\":\"progress\",\"completed\":0,\"completed\":0,\"total\":1}\n");
    for(const auto value:{"-1","128","1e1","1.0","true","null","\"60\"","NaN","01"}){auto bad=start;bad.replace(bad.find(":60"),3,std::string(":")+value);invalid(initial+bad);}
    for(const auto value:{"-1","NaN","1e999","6","\"0.25\"",".2","1.","+1"}){auto bad=start;bad.replace(bad.find("0.25"),4,value);invalid(initial+bad);}
    auto role=start;role.replace(role.find("acoustic_piano"),14,"unknown");invalid(initial+role);
    invalid(initial+"{\"type\":\"progress\",\"completed\":1,\"total\":2}\n");
    invalid(initial+start+end+progress+"{\"type\":\"done\",\"notes\":2,\"midi_bytes\":52}\n");
    invalid(std::string(4097,' '));invalid(std::string("\0",1));
    rejects([]{AceMidiProtocol p(1);p.feed(std::string(16*1024*1024+1,' '));});
    rejects([&]{AceMidiProtocol p(1);p.feed(initial);for(std::size_t i=0;i<=max_project_midi_notes;++i){auto event=start;event.replace(event.find("\"index\":0"),9,"\"index\":"+std::to_string(i));p.feed(event);}});
    // Property stress: every truncated prefix must fail, never become success.
    for(std::size_t i=0;i<stream.size();++i)invalid(stream.substr(0,i));
    std::cout<<"PASS bounded ace-midi JSONL protocol\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
