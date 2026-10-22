#include <mrs/sample_tag_rules.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <string_view>

namespace mrs::library {
namespace {
struct Token { std::string lower, raw; };
char fold_ascii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
// A small UTF-8 case fold for Cyrillic А..Я and Ё (no locale or external runtime).
std::string fold(const std::string& value) {
    std::string result;
    for (std::size_t i=0;i<value.size();) {
        const auto a=static_cast<unsigned char>(value[i]);
        if(a<0x80){result.push_back(fold_ascii(value[i++]));continue;}
        if(i+1<value.size()&&a==0xd0) {
            const auto b=static_cast<unsigned char>(value[i+1]);
            if(b>=0x90&&b<=0x9f){result.push_back(static_cast<char>(0xd0));result.push_back(static_cast<char>(b+0x20));i+=2;continue;}
            if(b>=0xa0&&b<=0xaf){result.push_back(static_cast<char>(0xd1));result.push_back(static_cast<char>(b-0x20));i+=2;continue;}
            if(b==0x81){result.push_back(static_cast<char>(0xd1));result.push_back(static_cast<char>(0x91));i+=2;continue;}
        }
        result.push_back(value[i++]);
    }
    return result;
}
std::vector<Token> tokens(const std::string& value) {
    std::vector<Token> out;std::string word;
    auto flush=[&]{if(!word.empty()){out.push_back({fold(word),word});word.clear();}};
    for (std::size_t i=0;i<value.size();++i) {
        const auto c=static_cast<unsigned char>(value[i]);
        const bool keep=std::isalnum(c)!=0||c>=0x80||c=='#';
        if(!keep){flush();continue;}
        if(c<128&&std::isupper(c)!=0&&!word.empty()){
            const auto before=static_cast<unsigned char>(word.back());
            const bool from_lower=before<128&&std::islower(before)!=0;
            const bool acronym_break=before<128&&std::isupper(before)!=0&&i+1<value.size()&&
                std::islower(static_cast<unsigned char>(value[i+1]))!=0&&word.size()>1;
            if(from_lower||acronym_break)flush();
        }
        word.push_back(static_cast<char>(c));
    }
    flush();
    return out;
}
bool exact_sequence(const std::vector<Token>& source,const std::vector<Token>& what) {
    if(what.empty()||what.size()>source.size())return false;
    for(std::size_t i=0;i+what.size()<=source.size();++i){
        bool same=true;
        for(std::size_t j=0;j<what.size();++j)if(source[i+j].lower!=what[j].lower){same=false;break;}
        if(same)return true;
    }
    return false;
}
struct Builtin {const char* group;const char* label;const char* alias;};
constexpr Builtin builtins[] = {
    {"Instrument","Upright Bass","upright bass"},{"Instrument","Upright Bass","double bass"},
    {"Instrument","Upright Bass","uprightbass"},{"Instrument","Kick","bass drum"},
    {"Instrument","Kick","kick"},{"Instrument","Kick","bd"},
    {"Instrument","Snare","snare"},{"Instrument","Snare","sn"},
    {"Instrument","Hi-Hat","hihat"},{"Instrument","Hi-Hat","hi hat"},{"Instrument","Hi-Hat","hat"},
    {"Instrument","Ride","ride"},{"Instrument","Toms","tom"},{"Instrument","Bass","bass"},
    {"Instrument","Guitar","guitar"},{"Instrument","Piano","piano"},
    {"Instrument","Strings","strings"},{"Instrument","Synth","synth"},
    {"Instrument","Percussion","perc"},{"Instrument","Percussion","percussion"},
    {"Genre","Jazz","jazz"},{"Genre","Funk","funk"},{"Genre","Rock","rock"},
    {"Genre","Ambient","ambient"},{"Genre","Cinematic","cinematic"},
    {"Genre","Hip Hop","hip hop"},{"Genre","House","house"},{"Genre","Latin","latin"},
    {"Instrument","SFX","sfx"},{"Instrument","Fill","fill"},{"Instrument","FX","fx"},
    {"Instrument","Transition","transition"},
    {"Technique","Brushes","brush"},{"Technique","Brushes","brushes"},{"Technique","Brushes","brsh"},
    {"Technique","Sticks","sticks"},{"Technique","Fingerstyle","fingerstyle"},
    {"Technique","Slap","slap"},{"Rhythm","Swing","swing"},
    {"Rhythm","Shuffle","shuffle"},{"Rhythm","Straight","straight"}
};
enum class TypeMatch {none,one_shot,loop,conflict};
TypeMatch sample_type(const std::vector<Token>& source){
    auto words=source;
    for(auto& word:words){const auto end=word.lower.find_last_not_of("0123456789");
        if(end!=std::string::npos&&end+1<word.lower.size())word.lower.resize(end+1);}
    bool one=false,loop=false;
    for(const auto* alias:{"one shot","one shots","oneshot","oneshots","1 shot","1 shots","1shot","1shots"})
        one=one||exact_sequence(words,tokens(alias));
    for(const auto* alias:{"loop","loops"})loop=loop||exact_sequence(words,tokens(alias));
    return one&&loop?TypeMatch::conflict:one?TypeMatch::one_shot:loop?TypeMatch::loop:TypeMatch::none;
}
int bpm_from_token(const std::string& s) {
    if(s.size()>3||s.empty())return 0;
    for(const auto c:s)if(c<'0'||c>'9')return 0;
    int n=0;for(const auto c:s)n=n*10+(c-'0');
    return n>=45&&n<=250?n:0;
}
std::optional<int> infer_bpm(const std::vector<Token>& source) {
    for(std::size_t i=0;i<source.size();++i){
        const auto& x=source[i].lower;
        if(x.size()>3&&x.size()<=6&&x.substr(x.size()-3)=="bpm"){
            if(auto n=bpm_from_token(x.substr(0,x.size()-3)))return n;
        }
        if(i+1<source.size()&&source[i+1].lower=="bpm")
            if(auto n=bpm_from_token(x))return n;
    }
    // Bare tempo must be a distinctly delimited filename component (_95_ / -95-),
    // not an ID like 001 or a suffix from another word.
    for(std::size_t i=1;i+1<source.size();++i)
        if(auto n=bpm_from_token(source[i].lower))return n;
    return std::nullopt;
}
std::optional<std::string> infer_key(const std::vector<Token>& source) {
    for(const auto& part:source){
        const auto& w=part.lower;
        if(w.empty()||w[0]<'a'||w[0]>'g')continue;
        std::size_t pos=1;std::string accidental;
        if(pos<w.size()&&(w[pos]=='#'||w[pos]=='b'))accidental=w[pos++];
        const auto suffix=w.substr(pos);
        if(suffix!="m"&&suffix!="min"&&suffix!="minor"&&suffix!="maj"&&suffix!="major")continue;
        const bool minor=suffix=="m"||suffix=="min"||suffix=="minor";
        const auto note=static_cast<char>(w[0]-'a'+'A');
        return std::string(1,note)+accidental+(minor?"m":" major");
    }
    return std::nullopt;
}
} // namespace
Classification classify_sample_path(const std::string& relative,const std::vector<TagRule>& rules) {
    Classification result;
    auto last=relative.find_last_of("/\\");
    const auto file=last==std::string::npos?relative:relative.substr(last+1);
    auto dot=file.find_last_of('.');
    const auto filename=dot==std::string::npos?file:file.substr(0,dot);
    const auto name=tokens(filename);
    const auto directories=tokens(last==std::string::npos?"":relative.substr(0,last));
    std::set<std::pair<std::string,std::string>> found;
    auto suggest=[&](const std::string& group,const std::string& label,const char* origin,double confidence){
        const auto category=group=="Type"&&label!="Loop"&&label!="One-shot"?std::string("Instrument"):group;
        if(!category.empty()&&!label.empty()&&found.emplace(category,label).second)
            result.tags.push_back({category,label,origin,confidence});
    };
    // Custom rules win exact group/label collisions; builtins are still independently visible.
    for(const auto& rule:rules){
        if(exact_sequence(rule.match_folder?directories:name,tokens(rule.phrase)))
            suggest(rule.group,rule.label,"rule",.99);
    }
    for(const auto& built:builtins){
        const auto alias=tokens(built.alias);
        if(exact_sequence(name,alias))suggest(built.group,built.label,"filename",.92);
        else if(exact_sequence(directories,alias))suggest(built.group,built.label,"folder",.69);
    }
    // One exclusive automatic type: explicit custom rule, then filename,
    // then the nearest labelled ancestor. Never infer One-shot from missing tags.
    const auto customTypes=std::count_if(result.tags.begin(),result.tags.end(),[](const auto& tag){return tag.group=="Type";});
    if(customTypes>1)std::erase_if(result.tags,[](const auto& tag){return tag.group=="Type";});
    if(customTypes==0){
        auto type=sample_type(name);const bool fromFilename=type!=TypeMatch::none;const char* origin="filename";
        auto parent=last==std::string::npos?std::string{}:relative.substr(0,last);
        while(type==TypeMatch::none&&!parent.empty()){
            const auto at=parent.find_last_of("/\\");
            type=sample_type(tokens(parent.substr(at==std::string::npos?0:at+1)));
            origin="folder";
            parent=at==std::string::npos?"":parent.substr(0,at);
        }
        if(type==TypeMatch::one_shot||type==TypeMatch::loop)
            suggest("Type",type==TypeMatch::one_shot?"One-shot":"Loop",origin,fromFilename?.95:.85);
    }
    result.metadata.bpm=infer_bpm(name);
    result.metadata.key=infer_key(name);
    return result;
}
} // namespace mrs::library
