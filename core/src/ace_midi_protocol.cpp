#include <mrs/ace_midi_protocol.hpp>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <stdexcept>

namespace mrs {
namespace {
void need(bool value){if(!value)throw std::invalid_argument("invalid ace-midi JSONL");}
struct Value { std::string text; bool string{}; };
// Deliberately accepts only the pinned flat, ASCII, unescaped-key/value schema.
// No recursion, coercion, duplicate members, arrays or ignored extension fields.
class Object {
    std::string_view input_;
    std::size_t pos_{};
    void ws(){while(pos_<input_.size()&&(input_[pos_]==' '||input_[pos_]=='\t'||input_[pos_]=='\r'))++pos_;}
    char take(){need(pos_<input_.size());return input_[pos_++];}
    std::string string(){
        need(take()=='"');std::string s;
        while(true){const auto c=take();if(c=='"')break;need(c>=32&&c<127&&c!='\\'&&s.size()<64);s+=c;}
        return s;
    }
    std::map<std::string,Value> fields_;
public:
    explicit Object(std::string_view input):input_(input){
        ws();need(take()=='{');ws();
        for(;;){
            need(fields_.size()<8);auto key=string();ws();need(take()==':');ws();
            need(pos_<input_.size());Value v;
            if(input_[pos_]=='"'){v.string=true;v.text=string();}
            else{
                const auto start=pos_;
                while(pos_<input_.size()&&input_[pos_]!=','&&input_[pos_]!='}'&&input_[pos_]!=' '&&input_[pos_]!='\t'&&input_[pos_]!='\r')++pos_;
                need(pos_>start&&pos_-start<=48);v.text=input_.substr(start,pos_-start);
                // Strict JSON number grammar, including leading-zero rules.
                std::size_t at=0;const auto& n=v.text;
                if(n[at]=='-')++at;
                need(at<n.size());
                if(n[at]=='0')++at;
                else{need(n[at]>='1'&&n[at]<='9');while(at<n.size()&&n[at]>='0'&&n[at]<='9')++at;}
                if(at<n.size()&&n[at]=='.'){++at;const auto begin=at;while(at<n.size()&&n[at]>='0'&&n[at]<='9')++at;need(at>begin);}
                if(at<n.size()&&(n[at]=='e'||n[at]=='E')){++at;if(at<n.size()&&(n[at]=='+'||n[at]=='-'))++at;const auto begin=at;while(at<n.size()&&n[at]>='0'&&n[at]<='9')++at;need(at>begin);}
                need(at==n.size());
            }
            need(fields_.emplace(std::move(key),std::move(v)).second);ws();const auto next=take();
            if(next=='}')break;
            need(next==',');ws();
        }
        ws();need(pos_==input_.size());
    }
    void size(std::size_t count)const{need(fields_.size()==count);}
    const Value& get(const char* key)const{const auto i=fields_.find(key);need(i!=fields_.end());return i->second;}
    std::string text(const char* key)const{const auto& v=get(key);need(v.string);return v.text;}
    int integer(const char* key,int maximum)const{
        const auto& v=get(key);need(!v.string);int n{};
        const auto [end,ec]=std::from_chars(v.text.data(),v.text.data()+v.text.size(),n);
        need(ec==std::errc{}&&end==v.text.data()+v.text.size()&&n>=0&&n<=maximum);return n;
    }
    double seconds(const char* key,double maximum)const{
        const auto& v=get(key);need(!v.string);double n{};
        const auto [end,ec]=std::from_chars(v.text.data(),v.text.data()+v.text.size(),n);
        need(ec==std::errc{}&&end==v.text.data()+v.text.size()&&std::isfinite(n)&&n>=0&&n<=maximum);return n;
    }
};
}
AceMidiProtocol::AceMidiProtocol(double seconds,Sample frames,bool allow_zero_duration):seconds_(seconds),allow_zero_duration_(allow_zero_duration){
    need(std::isfinite(seconds)&&seconds>0&&seconds<=4*60*60);
    if(frames==-1)frames=static_cast<Sample>(seconds*16000);
    need(frames>0&&frames<=4*60*60*16000&&std::abs(static_cast<double>(frames)-seconds*16000)<=1.01);
    total_=static_cast<int>((frames+79999)/80000);
}
void AceMidiProtocol::feed(std::string_view bytes){
    need(!finished_&&bytes.size()<=16*1024*1024-bytes_);bytes_+=bytes.size();
    for(const char c:bytes){
        if(c=='\n'){need(!pending_.empty()&&++lines_<=100000);line(pending_);pending_.clear();}
        else{need(pending_.size()<4096&&c!='\0');pending_+=c;}
    }
}
void AceMidiProtocol::line(std::string_view input){
    need(!done_);const Object o(input);const auto type=o.text("type");
    if(type=="progress"){
        o.size(3);const auto completed=o.integer("completed",total_);
        need(o.integer("total",total_)==total_&&completed==completed_+1);
        if(completed_>=0)need(ended_<=notes_.size());
        completed_=completed;
    }else if(type=="note_start"){
        o.size(5);need(completed_>=0&&completed_<total_&&notes_.size()<max_project_midi_notes);
        need(o.integer("index",max_project_midi_notes)==static_cast<int>(notes_.size()));
        const auto role=o.text("instrument");const auto roles=ace_midi_instruments();
        need(std::any_of(roles.begin(),roles.end(),[&](const auto& r){return r.role==role;}));
        notes_.push_back({o.integer("pitch",127),o.seconds("time",total_*5.),-1,role});
    }else if(type=="note_end"){
        o.size(3);need(completed_>=0);const auto index=static_cast<std::size_t>(o.integer("index",max_project_midi_notes));
        need(index<notes_.size());auto& n=notes_[index];need(n.end<0);
        n.end=o.seconds("time",total_*5.+.01);need(n.end>n.start||(allow_zero_duration_&&n.end==n.start));++ended_;
    }else{
        need(type=="done");o.size(3);need(completed_==total_&&ended_==notes_.size());
        need(static_cast<std::size_t>(o.integer("notes",max_project_midi_notes))==notes_.size());
        midi_bytes_=static_cast<std::size_t>(o.integer("midi_bytes",16*1024*1024));need(midi_bytes_>=26);done_=true;
    }
}
void AceMidiProtocol::finish(){need(!finished_&&pending_.empty()&&done_);finished_=true;}
std::vector<TranscribedNote> AceMidiProtocol::notes()const{
    need(finished_);std::vector<TranscribedNote> result;
    for(auto n:notes_){n.end=std::min(n.end,seconds_);if(n.start<n.end)result.push_back(std::move(n));}
    return result;
}
}
