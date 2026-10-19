#include <mrs/midi_import.hpp>
#include <algorithm>
#include <deque>
#include <stdexcept>
#include <fstream>
namespace mrs {
namespace {
void need(bool ok){if(!ok)throw std::invalid_argument("Invalid or unsupported MIDI file");}
struct Reader {
    std::span<const std::byte> bytes; std::size_t pos{};
    unsigned byte(){need(pos<bytes.size());return std::to_integer<unsigned>(bytes[pos++]);}
    unsigned be(int count){unsigned n{};while(count--)n=(n<<8)|byte();return n;}
    unsigned vlq(){unsigned n{};for(int i=0;i<4;++i){const auto b=byte();n=(n<<7)|(b&127);if(!(b&128))return n;}need(false);return 0;}
    std::span<const std::byte> take(std::size_t n){need(n<=bytes.size()-pos);const auto result=bytes.subspan(pos,n);pos+=n;return result;}
};
}
std::vector<MidiPart> parse_midi_file(std::span<const std::byte> bytes){
    need(bytes.size()<=16*1024*1024);Reader file{bytes};need(file.be(4)==0x4d546864);const auto size=file.be(4);need(size>=6);Reader header{file.take(size)};
    const auto format=header.be(2),tracks=header.be(2),division=header.be(2);
    need(format<=1&&tracks>0&&tracks<=128&&(format!=0||tracks==1)&&division>0&&!(division&0x8000));
    std::vector<MidiPart> result;std::size_t eventCount{},noteCount{};
    for(unsigned track=0;track<tracks;++track){
        need(file.be(4)==0x4d54726b);const auto length=file.be(4);Reader r{file.take(length)};
        std::array<MidiPart,16> parts;std::array<std::array<std::deque<std::size_t>,128>,16> active;
        Tick sourceTick{};unsigned running{};bool ended=false;std::string name;
        const auto tick=[&]{need(sourceTick<=(max_tick-division/2)/ppq);return (sourceTick*ppq+division/2)/division;};
        while(r.pos<r.bytes.size()){
            sourceTick+=r.vlq();const Tick now=tick();unsigned status=r.byte();
            if(status<128){need(running>=0x80&&running<0xf0);--r.pos;status=running;}
            else if(status<0xf0)running=status;else running=0;
            need(++eventCount<=200000);
            if(status==0xff){const auto type=r.byte();const auto n=r.vlq();const auto data=r.take(n);
                if(type==3){need(n<=4096);name.assign(reinterpret_cast<const char*>(data.data()),data.size());std::erase(name,'\0');}
                if(type==0x2f){need(n==0&&r.pos==r.bytes.size());ended=true;break;}continue;
            }
            if(status==0xf0||status==0xf7){const auto n=r.vlq();r.take(n);continue;}
            need(status>=0x80&&status<0xf0);const int channel=static_cast<int>(status&15);const auto kind=status>>4;
            const auto a=r.byte(),b=kind==0xc||kind==0xd?0:r.byte();need(a<128&&b<128);auto& part=parts[static_cast<std::size_t>(channel)];
            part.channel=channel;part.midi.length=std::max(part.midi.length,now+1);
            if(kind==9&&b){if(++noteCount>max_project_midi_notes)throw std::invalid_argument("MIDI file exceeds the 32768-note project budget");auto& queue=active[static_cast<std::size_t>(channel)][a];queue.push_back(part.midi.notes.size());part.midi.notes.push_back({new_id(),now,1,static_cast<int>(a),static_cast<int>(b),channel});}
            else if(kind==8||(kind==9&&!b)){auto& queue=active[static_cast<std::size_t>(channel)][a];if(!queue.empty()){auto& note=part.midi.notes[queue.front()];note.length=std::max(Tick{1},now-note.start);queue.pop_front();}}
            else {need(part.midi.events.size()<8192);const int eventKind=kind==0xa?6:kind==0xb?2:kind==0xc?3:kind==0xd?4:5;part.midi.events.push_back({new_id(),now,eventKind,channel,static_cast<int>(a),static_cast<int>(b)});}
        }
        need(ended);const auto end=std::max(Tick{1},tick());
        for(std::size_t channel=0;channel<parts.size();++channel){auto& part=parts[channel];if(part.midi.notes.empty()&&part.midi.events.empty())continue;
            for(const auto& queue:active[channel])for(const auto index:queue){auto& note=part.midi.notes[index];note.length=std::max(Tick{1},end-note.start);}
            part.name=(name.empty()?"MIDI track "+std::to_string(track+1):name)+" / Ch "+std::to_string(channel+1);
            part.midi.length=std::max(end,Tick{1});for(const auto& n:part.midi.notes)part.midi.length=std::max(part.midi.length,n.start+n.length);for(const auto& e:part.midi.events)part.midi.length=std::max(part.midi.length,e.start+1);
            need(result.size()<128);result.push_back(std::move(part));
        }
    }
    need(file.pos==file.bytes.size());need(!result.empty());return result;
}
std::vector<MidiPart> load_midi_file(const std::filesystem::path& path){
    std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::invalid_argument("Cannot open MIDI file");
    const auto size=in.tellg();need(size>0&&size<=16*1024*1024);std::vector<std::byte> bytes(static_cast<std::size_t>(size));in.seekg(0);
    in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));need(static_cast<bool>(in));return parse_midi_file(bytes);
}
}
