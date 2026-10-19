#include <mrs/ai_tools.hpp>
#include <mrs/musical.hpp>
#include <algorithm>
#include <charconv>
#include <set>
#include <stdexcept>
#include <tuple>

namespace mrs::ai {
ProjectEdit::ProjectEdit(std::shared_ptr<const Project> before,const std::vector<std::shared_ptr<const ICommand>>& commands)
    :before_(std::move(before)),name_("AI proposal ("+std::to_string(commands.size())+" commands)") {
    if(!before_||commands.empty()||commands.size()>8)throw std::invalid_argument("Invalid AI command group");
    auto candidate=*before_;
    for(const auto& command:commands){if(!command)throw std::invalid_argument("Missing AI command");command->apply(candidate);candidate.validate();}
    after_=std::make_shared<const Project>(std::move(candidate));
}
void ProjectEdit::apply(Project& project) const {
    if(project!=*before_)throw std::invalid_argument("AI proposal prior state changed");
    project=*after_;
}
namespace {
constexpr std::string_view ara_clip_schema=R"({"type":"object","additionalProperties":false,"required":["clip_id"],"properties":{"clip_id":{"type":"string","minLength":1,"maxLength":128}}})";
constexpr std::string_view ara_attach_schema=R"({"type":"object","additionalProperties":false,"required":["clip_id","plugin_id"],"properties":{"clip_id":{"type":"string","minLength":1,"maxLength":128},"plugin_id":{"type":"string","minLength":1,"maxLength":128}}})";
constexpr std::string_view ara_bypass_schema=R"({"type":"object","additionalProperties":false,"required":["clip_id","bypass"],"properties":{"clip_id":{"type":"string","minLength":1,"maxLength":128},"bypass":{"type":"boolean"}}})";
constexpr std::string_view ara_read_schema=R"({"type":"object","additionalProperties":false,"properties":{"clip_id":{"type":"string","minLength":1,"maxLength":128},"cursor":{"type":"integer","minimum":0,"maximum":65536},"limit":{"type":"integer","minimum":1,"maximum":256}}})";
constexpr std::string_view ara_cap_schema=R"({"type":"object","additionalProperties":false,"properties":{"cursor":{"type":"integer","minimum":0,"maximum":65536},"limit":{"type":"integer","minimum":1,"maximum":256}}})";
constexpr std::string_view marker_add=R"({"type":"object","additionalProperties":false,"required":["name","tick"],"properties":{"name":{"type":"string","minLength":1,"maxLength":4096},"tick":{"type":"integer","minimum":0,"maximum":1000000000000}}})";
constexpr std::string_view marker_rename=R"({"type":"object","additionalProperties":false,"required":["marker_id","name"],"properties":{"marker_id":{"type":"string","minLength":1,"maxLength":128},"name":{"type":"string","minLength":1,"maxLength":4096}}})";
constexpr std::string_view selection_schema=R"({"type":"object","additionalProperties":false,"required":["entities"],"properties":{"entities":{"type":"array","maxItems":256,"items":{"type":"object","additionalProperties":false,"required":["kind","id"],"properties":{"kind":{"enum":["track","folder","clip","chord","section","marker"]},"id":{"type":"string","minLength":1,"maxLength":128}}}},"sample_start":{"type":"integer","minimum":0,"maximum":4503599627370496},"sample_end":{"type":"integer","minimum":1,"maximum":4503599627370496}},"dependentRequired":{"sample_start":["sample_end"],"sample_end":["sample_start"]}})";
constexpr std::string_view transpose_schema=R"({"type":"object","additionalProperties":false,"required":["clip_id","note_ids","semitones"],"properties":{"clip_id":{"type":"string","minLength":1,"maxLength":128},"note_ids":{"type":"array","minItems":1,"maxItems":256,"uniqueItems":true,"items":{"type":"string","minLength":1,"maxLength":128}},"semitones":{"type":"integer","minimum":-24,"maximum":24}}})";
constexpr std::string_view velocity_schema=R"({"type":"object","additionalProperties":false,"required":["clip_id","note_ids","velocity"],"properties":{"clip_id":{"type":"string","minLength":1,"maxLength":128},"note_ids":{"type":"array","minItems":1,"maxItems":256,"uniqueItems":true,"items":{"type":"string","minLength":1,"maxLength":128}},"velocity":{"type":"integer","minimum":1,"maximum":127}}})";
constexpr std::string_view draft_schema=R"({"type":"object","additionalProperties":false,"required":["mode","tick_start","tick_end","notes","cc"],"properties":{"mode":{"enum":["insert","replace","edit"]},"track_id":{"type":"string","minLength":1,"maxLength":128},"clip_id":{"type":"string","minLength":1,"maxLength":128},"name":{"type":"string","minLength":1,"maxLength":256},"tick_start":{"type":"integer","minimum":0,"maximum":1000000000000},"tick_end":{"type":"integer","minimum":1,"maximum":1000000000000},"notes":{"type":"array","maxItems":256,"items":{"type":"object","additionalProperties":false,"required":["start","duration","pitch","velocity","channel"],"properties":{"id":{"type":"string","minLength":1,"maxLength":128},"start":{"type":"integer","minimum":0},"duration":{"type":"integer","minimum":1},"pitch":{"type":"integer","minimum":0,"maximum":127},"velocity":{"type":"integer","minimum":1,"maximum":127},"channel":{"type":"integer","minimum":0,"maximum":15}}}},"cc":{"type":"array","maxItems":256,"items":{"type":"object","additionalProperties":false,"required":["start","controller","value","channel"],"properties":{"id":{"type":"string","minLength":1,"maxLength":128},"start":{"type":"integer","minimum":0},"controller":{"type":"integer","minimum":0,"maximum":127},"value":{"type":"integer","minimum":0,"maximum":127},"channel":{"type":"integer","minimum":0,"maximum":15}}}}},"oneOf":[{"required":["track_id","name"],"properties":{"mode":{"const":"insert"}},"not":{"required":["clip_id"]}},{"required":["clip_id"],"properties":{"mode":{"enum":["replace","edit"]}},"not":{"anyOf":[{"required":["track_id"]},{"required":["name"]}]}}]})";
constexpr std::string_view query_schema=R"({"type":"object","additionalProperties":false,"required":["query"],"properties":{"query":{"enum":["tracks","folders","clips","midi_notes","midi_events","chords","sections","markers","tempos","meters","mixer","routing","plugins","stored_plugin_parameters","graph_nodes","graph_edges","graph_outputs","midi_routes","live_bindings","live_actions","live_patches","stored_graph_parameters","logical_midi_ports","list_ara_capabilities","query_ara_binding","get_ara_status","automation","analysis","live_performance"]},"id":{"type":"string","minLength":1,"maxLength":128},"track_id":{"type":"string","minLength":1,"maxLength":128},"clip_id":{"type":"string","minLength":1,"maxLength":128},"plugin_id":{"type":"string","minLength":1,"maxLength":128},"master":{"type":"boolean"},"selected_only":{"type":"boolean"},"cursor":{"type":"integer","minimum":0,"maximum":65536},"limit":{"type":"integer","minimum":1,"maximum":256},"tick_start":{"type":"integer","minimum":0,"maximum":1000000000000},"tick_end":{"type":"integer","minimum":1,"maximum":1000000000000}},"dependentRequired":{"tick_start":["tick_end"],"tick_end":["tick_start"]}})";
bool utf8(std::string_view s){
    for(std::size_t i=0;i<s.size();){const auto a=static_cast<unsigned char>(s[i++]);if(a<128)continue;unsigned n{},cp{};
        if(a>=0xC2 && a<=0xDF){n=1;cp=a&31;}else if(a>=0xE0 && a<=0xEF){n=2;cp=a&15;}else if(a>=0xF0 && a<=0xF4){n=3;cp=a&7;}else return false;
        if(n>s.size()-i)return false;for(unsigned j=0;j<n;++j){const auto b=static_cast<unsigned char>(s[i++]);if((b&0xC0)!=0x80)return false;cp=(cp<<6)|(b&63);}
        if((n==1 && cp<128)||(n==2 && cp<2048)||(n==3 && cp<65536)||cp>0x10FFFF||(cp>=0xD800 && cp<=0xDFFF))return false;
    }return true;
}
bool text_ok(std::string_view value,std::size_t max){return !value.empty() && value.size()<=max && value.find('\0')==std::string_view::npos && utf8(value);}
bool identifier(const Id& id){return text_ok(id.value,128);}
void require(bool ok){if(!ok)throw std::invalid_argument("invalid structured tool call");}
struct Value {
    enum class Kind { string, integer, boolean, object, array } kind{Kind::object};
    std::string text;std::int64_t number{};bool boolean{};
    std::map<std::string,Value> object;std::vector<Value> array;
};
class Parser {
    std::string_view input_;std::size_t at_{},nodes_{};
    void space(){while(at_<input_.size() && (input_[at_]==' '||input_[at_]=='\n'||input_[at_]=='\r'||input_[at_]=='\t'))++at_;}
    char get(){require(at_<input_.size());return input_[at_++];}
    bool consume(char c){space();if(at_<input_.size() && input_[at_]==c){++at_;return true;}return false;}
    unsigned hex(){unsigned value{};for(int i=0;i<4;++i){const auto c=get();unsigned digit{};
        if(c>='0' && c<='9')digit=c-'0';else if(c>='a' && c<='f')digit=c-'a'+10;else if(c>='A' && c<='F')digit=c-'A'+10;else throw std::invalid_argument("invalid JSON escape");value=value*16+digit;}return value;}
    std::string string(){require(get()=='"');std::string out;bool closed{};
        while(at_<input_.size()){const auto c=get();if(c=='"'){closed=true;break;}require(static_cast<unsigned char>(c)>=32);
            if(c!='\\'){out+=c;continue;}const auto escaped=get();switch(escaped){
            case '"':case '\\':case '/':out+=escaped;break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;case 'b':out+='\b';break;case 'f':out+='\f';break;
            case 'u':{auto cp=hex();if(cp>=0xD800 && cp<=0xDBFF){require(get()=='\\' && get()=='u');const auto low=hex();require(low>=0xDC00 && low<=0xDFFF);cp=0x10000+(cp-0xD800)*1024+(low-0xDC00);}else require(cp<0xDC00 || cp>0xDFFF);
                if(cp<128)out+=static_cast<char>(cp);else if(cp<2048){out+=static_cast<char>(0xC0|(cp>>6));out+=static_cast<char>(0x80|(cp&63));}
                else if(cp<65536){out+=static_cast<char>(0xE0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
                else{out+=static_cast<char>(0xF0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&63));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}break;}
            default:throw std::invalid_argument("invalid JSON escape");}
        }require(closed && out.size()<=4096 && utf8(out));return out;
    }
    Value value(unsigned depth){space();require(depth<=8 && ++nodes_<=4096 && at_<input_.size());Value v;const auto c=input_[at_];
        if(c=='"'){v.kind=Value::Kind::string;v.text=string();}
        else if(consume('{')){v.kind=Value::Kind::object;if(!consume('}')){do{space();const auto key=string();require(consume(':'));require(v.object.size()<32 && !v.object.contains(key));v.object.emplace(key,value(depth+1));}while(consume(','));require(consume('}'));}}
        else if(consume('[')){v.kind=Value::Kind::array;if(!consume(']')){do{require(v.array.size()<256);v.array.push_back(value(depth+1));}while(consume(','));require(consume(']'));}}
        else if(input_.substr(at_,4)=="true"){v.kind=Value::Kind::boolean;v.boolean=true;at_+=4;}
        else if(input_.substr(at_,5)=="false"){v.kind=Value::Kind::boolean;at_+=5;}
        else{v.kind=Value::Kind::integer;const auto start=at_;if(c=='-')++at_;require(at_<input_.size() && input_[at_]>='0' && input_[at_]<='9');
            if(input_[at_]=='0')++at_;else while(at_<input_.size() && input_[at_]>='0' && input_[at_]<='9')++at_;
            const auto r=std::from_chars(input_.data()+start,input_.data()+at_,v.number);require(r.ec==std::errc{} && r.ptr==input_.data()+at_);}
        return v;
    }
public:
    explicit Parser(std::string_view input):input_(input){require(!input.empty() && input.size()<=32768);}
    Value parse(){auto v=value(0);space();require(at_==input_.size());return v;}
};
const Value& member(const Value& v,std::string_view name){require(v.kind==Value::Kind::object);const auto at=v.object.find(std::string(name));require(at!=v.object.end());return at->second;}
std::string string(const Value& v){require(v.kind==Value::Kind::string);return v.text;}
std::int64_t integer(const Value& v){require(v.kind==Value::Kind::integer);return v.number;}
void fields(const Value& v,std::initializer_list<std::string_view> allowed){require(v.kind==Value::Kind::object);for(const auto& [key,value]:v.object){(void)value;require(std::find(allowed.begin(),allowed.end(),key)!=allowed.end());}}
std::uint64_t counter(const Value& v){const auto text=string(v);std::uint64_t result{};require(!text.empty() && text.size()<=20 && (text.size()==1 || text.front()!='0'));
    const auto r=std::from_chars(text.data(),text.data()+text.size(),result);require(r.ec==std::errc{} && r.ptr==text.data()+text.size());return result;}
Id id(const Value& v){Id result{string(v)};require(identifier(result));return result;}
std::optional<Id> optional_id(const Value& args,std::string_view key){const auto at=args.object.find(std::string(key));return at==args.object.end()?std::optional<Id>{}:id(at->second);}
template<class T> const T* find(const std::vector<T>& values,const Id& id){const auto at=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.id==id;});return at==values.end()?nullptr:&*at;}
void validate_draft(const MidiDraftArgs& a){
    require(a.start>=0&&a.end>a.start&&a.end<=max_tick&&a.notes.size()+a.cc.size()<=256);
    require(a.mode==MidiDraftMode::insert||a.mode==MidiDraftMode::replace||a.mode==MidiDraftMode::edit);
    if(a.mode==MidiDraftMode::insert)require(a.track&&identifier(*a.track)&&!a.clip&&text_ok(a.name,256));
    else require(a.clip&&identifier(*a.clip)&&!a.track&&a.name.empty());
    if(a.mode==MidiDraftMode::edit)require(!a.notes.empty()||!a.cc.empty());
    std::set<std::string> ids;const auto length=a.end-a.start;
    auto check_id=[&](const auto& id){if(a.mode==MidiDraftMode::edit)require(id&&identifier(*id)&&ids.insert(id->value).second);else require(!id);};
    for(const auto& n:a.notes){check_id(n.id);require(n.start>=0&&n.start<length&&n.duration>0&&n.duration<=length-n.start&&n.pitch>=0&&n.pitch<=127&&n.velocity>=1&&n.velocity<=127&&n.channel>=0&&n.channel<16);}
    for(const auto& e:a.cc){check_id(e.id);require(e.start>=0&&e.start<length&&e.controller>=0&&e.controller<128&&e.value>=0&&e.value<128&&e.channel>=0&&e.channel<16);}
}
std::shared_ptr<const ICommand> prepare_draft(const MidiDraftArgs& a,const Project& p){
    validate_draft(a);std::optional<Clip> expected;Clip result;Tick source{};
    if(a.mode==MidiDraftMode::insert){const auto* track=find(p.tracks,*a.track);require(track&&track->kind==TrackKind::instrument);result.id=new_id();result.track=*a.track;result.name=a.name;result.midi=MidiClip{a.start,a.end-a.start,0,{}};}
    else{const auto* clip=find(p.clips,*a.clip);require(clip&&clip->midi);expected=*clip;result=*clip;const auto& m=*result.midi;require(a.start>=m.start&&a.end<=m.start+m.length);source=m.source_offset+a.start-m.start;}
    auto& m=*result.midi;const auto stop=source+a.end-a.start;
    if(a.mode==MidiDraftMode::replace){
        // Never remove the outside part of a note crossing the confirmed boundary.
        for(const auto& n:m.notes)if(n.start<stop&&n.start+n.length>source)require(n.start>=source&&n.start+n.length<=stop);
        std::erase_if(m.notes,[&](const auto& n){return n.start>=source&&n.start<stop;});
        std::erase_if(m.events,[&](const auto& e){return e.kind==2&&e.start>=source&&e.start<stop;});
    }
    for(const auto& n:a.notes){MidiNote value{n.id.value_or(new_id()),source+n.start,n.duration,n.pitch,n.velocity,n.channel};
        if(a.mode==MidiDraftMode::edit){auto at=std::find_if(m.notes.begin(),m.notes.end(),[&](const auto& old){return old.id==*n.id;});require(at!=m.notes.end()&&at->start>=source&&at->start+at->length<=stop);*at=std::move(value);}else m.notes.push_back(std::move(value));}
    for(const auto& e:a.cc){MidiChannelEvent value{e.id.value_or(new_id()),source+e.start,2,e.channel,e.controller,e.value};
        if(a.mode==MidiDraftMode::edit){auto at=std::find_if(m.events.begin(),m.events.end(),[&](const auto& old){return old.id==*e.id;});require(at!=m.events.end()&&at->kind==2&&at->start>=source&&at->start<stop);*at=std::move(value);}else m.events.push_back(std::move(value));}
    // Polyphony is allowed. Same-pitch/channel overlap is ambiguous for note-off.
    auto ordered=m.notes;std::sort(ordered.begin(),ordered.end(),[](const auto& l,const auto& r){return std::tie(l.channel,l.pitch,l.start)<std::tie(r.channel,r.pitch,r.start);});
    Tick end{};int channel=-1,pitch=-1;for(const auto& n:ordered){if(n.channel==channel&&n.pitch==pitch)require(n.start>=end);else{channel=n.channel;pitch=n.pitch;}end=n.start+n.length;}
    std::set<std::tuple<Tick,int,int>> cc;for(const auto& e:m.events)if(e.kind==2)require(cc.emplace(e.start,e.channel,e.data1).second);
    auto candidate=p;if(expected){SetMidiNotes{result.id,m.notes}.apply(candidate);SetMidiEvents{result.id,m.events}.apply(candidate);}else AddMidiClip{result}.apply(candidate);candidate.validate();
    struct DraftCommand final:ICommand {std::optional<Clip> before;Clip after;DraftCommand(std::optional<Clip> b,Clip a):before(std::move(b)),after(std::move(a)){}
        std::string_view name()const override{return "AI MIDI draft";}
        void apply(Project& project)const override{if(!before){AddMidiClip{after}.apply(project);return;}const auto* old=find(project.clips,before->id);require(old&&*old==*before);SetMidiNotes{after.id,after.midi->notes}.apply(project);SetMidiEvents{after.id,after.midi->events}.apply(project);}};
    return std::make_shared<DraftCommand>(std::move(expected),std::move(result));
}
}
void PermissionPolicy::validate() const {if(static_cast<int>(mode)<0 || static_cast<int>(mode)>3 || automatic_tools>31)throw std::invalid_argument("invalid AI permissions");}
std::string_view permission_name(PermissionMode mode){switch(mode){case PermissionMode::read:return "READ";case PermissionMode::suggest:return "SUGGEST";case PermissionMode::edit:return "EDIT";case PermissionMode::automatic:return "AUTO";}return "UNKNOWN";}
bool read_tool(ToolKind kind){return kind==ToolKind::query||(kind>=ToolKind::list_ara_capabilities&&kind<=ToolKind::get_ara_status);}
bool ara_action(ToolKind kind){return kind>=ToolKind::open_ara_editor&&kind<=ToolKind::reanalyse_ara;}
const std::array<ToolDescriptor,15>& tool_registry(){static const std::array<ToolDescriptor,15> registry{{
    {ToolKind::query,"query_project",query_schema,"Application::ai_query",false,false},
    {ToolKind::add_marker,"add_marker",marker_add,"Application::edit_metadata / AddMarker",true,false},
    {ToolKind::rename_marker,"rename_marker",marker_rename,"Application::edit_metadata / UpdateMarker",true,false},
    {ToolKind::set_selection,"set_selection",selection_schema,"UI preview selection (deferred #163)",false,true},
    {ToolKind::transpose_notes,"transpose_notes",transpose_schema,"Application::edit / EditMidiNotes",true,false},
    {ToolKind::set_note_velocity,"set_note_velocity",velocity_schema,"Application::edit / EditMidiNotes",true,false},
    {ToolKind::midi_draft,"midi_draft",draft_schema,"Application::edit_timing / Core MIDI Commands",true,false},
    {ToolKind::list_ara_capabilities,"list_ara_capabilities",ara_cap_schema,"Application::ai_tool_query",false,false},
    {ToolKind::query_ara_binding,"query_ara_binding",ara_read_schema,"Application::ai_tool_query",false,false},
    {ToolKind::get_ara_status,"get_ara_status",ara_read_schema,"Application::ai_tool_query",false,false},
    {ToolKind::open_ara_editor,"open_ara_editor",ara_clip_schema,"Application / trusted editor handler",false,true},
    {ToolKind::attach_ara,"attach_ara",ara_attach_schema,"Application::attach_ara_source",true,false},
    {ToolKind::set_ara_bypass,"set_ara_bypass",ara_bypass_schema,"Application::bypass_ara_source",true,false},
    {ToolKind::remove_ara,"remove_ara",ara_clip_schema,"Application::detach_ara_source",true,false},
    {ToolKind::reanalyse_ara,"reanalyse_ara",ara_clip_schema,"UNSUPPORTED until confirmed vendor capability",false,false}
}};return registry;}
const ToolDescriptor* tool_descriptor(std::string_view name){for(const auto& d:tool_registry())if(d.name==name)return &d;return nullptr;}
const ToolDescriptor* tool_descriptor(ToolKind kind){for(const auto& d:tool_registry())if(d.kind==kind)return &d;return nullptr;}
Decision authorize(const PermissionPolicy& policy,ToolKind tool,Intent intent,RuntimeSafety runtime,ExecutionGates gates){
    try{policy.validate();}catch(...){return {Error::invalid_input,Block::mode};}
    const auto* descriptor=tool_descriptor(tool);if(!descriptor)return {Error::unsupported,Block::unknown_tool};
    if(!policy.enabled)return {Error::permission_denied,Block::disabled};
    if(intent==Intent::read)return read_tool(tool)?Decision{}:Decision{Error::permission_denied,Block::mode};
    if(intent!=Intent::suggest && intent!=Intent::apply)return {Error::invalid_input,Block::mode};
    if(policy.mode==PermissionMode::read)return {Error::permission_denied,Block::mode};
    if(intent==Intent::suggest)return {};
    if(read_tool(tool))return {Error::unsupported,Block::unknown_tool};
    if(policy.mode==PermissionMode::suggest)return {Error::permission_denied,Block::mode};
    if(runtime.live && (runtime.playing || runtime.recording || runtime.count_in))return {Error::permission_denied,Block::live_performance};
    if(runtime.recording || runtime.count_in)return {Error::permission_denied,Block::recording};
    if(policy.mode==PermissionMode::edit && !gates.confirmed)return {Error::permission_denied,Block::confirmation};
    if(policy.mode==PermissionMode::automatic){const auto bit=1u<<(static_cast<unsigned>(tool)-1);if((policy.automatic_tools&bit)==0)return {Error::permission_denied,Block::automatic_allowlist};}
    if(ara_action(tool)&&runtime.playing)return {Error::permission_denied,Block::midi_playback};
    if(!gates.preview)return {Error::unsupported,Block::preview};
    if(descriptor->project_write && !gates.undo)return {Error::unsupported,Block::undo};
    if((tool==ToolKind::transpose_notes || tool==ToolKind::set_note_velocity || tool==ToolKind::midi_draft) && !gates.midi)return {Error::unsupported,Block::midi_contract};
    return {};
}
std::string tool_envelope_schema(){
    std::string schema=R"({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","additionalProperties":false,"required":["version","request_id","operation_id","context_key","tool","arguments"],"properties":{"version":{"const":1},"request_id":{"type":"string","pattern":"^[1-9][0-9]{0,19}$"},"operation_id":{"type":"string","minLength":1,"maxLength":128},"context_key":{"type":"object","additionalProperties":false,"required":["project_id","session_id","project_revision","graph_revision"],"properties":{"project_id":{"type":"string","minLength":1,"maxLength":128},"session_id":{"type":"string","minLength":1,"maxLength":128},"project_revision":{"type":"string","pattern":"^(0|[1-9][0-9]{0,19})$"},"graph_revision":{"type":"string","pattern":"^(0|[1-9][0-9]{0,19})$"}}},"tool":{"type":"string"},"arguments":{"type":"object"}},"oneOf":[)";
    bool first=true;for(const auto& d:tool_registry()){if(!first)schema+=',';first=false;schema+=R"({"properties":{"tool":{"const":")";schema+=d.name;schema+=R"("},"arguments":)";schema+=d.argument_schema;schema+="}}";}return schema+"]}";
}
DecodedTool decode_tool_call(std::string_view bytes){try{
    const auto root=Parser(bytes).parse();fields(root,{"version","request_id","operation_id","context_key","tool","arguments"});ToolCall call;
    if(integer(member(root,"version"))!=1)return {Error::unsupported,{}};
    const auto* descriptor=tool_descriptor(string(member(root,"tool")));if(!descriptor)return {Error::unsupported,{}};call.tool=descriptor->kind;
    call.request_id=counter(member(root,"request_id"));require(call.request_id!=0);call.operation_id=string(member(root,"operation_id"));require(text_ok(call.operation_id,128));
    const auto& key=member(root,"context_key");fields(key,{"project_id","session_id","project_revision","graph_revision"});call.key={id(member(key,"project_id")),id(member(key,"session_id")),counter(member(key,"project_revision")),counter(member(key,"graph_revision"))};
    const auto& args=member(root,"arguments");
    switch(call.tool){
    case ToolKind::list_ara_capabilities:case ToolKind::query_ara_binding:case ToolKind::get_ara_status:{
        if(call.tool==ToolKind::list_ara_capabilities)fields(args,{"cursor","limit"});else fields(args,{"clip_id","cursor","limit"});Query q;
        q.kind=call.tool==ToolKind::list_ara_capabilities?QueryKind::ara_capabilities:call.tool==ToolKind::query_ara_binding?QueryKind::ara_binding:QueryKind::ara_status;
        q.clip=optional_id(args,"clip_id");if(args.object.contains("cursor")){const auto n=integer(member(args,"cursor"));require(n>=0&&n<=65536);q.cursor=static_cast<std::size_t>(n);}if(args.object.contains("limit")){const auto n=integer(member(args,"limit"));require(n>=1&&n<=256);q.limit=static_cast<std::size_t>(n);}call.arguments=std::move(q);break;}
    case ToolKind::open_ara_editor:case ToolKind::attach_ara:case ToolKind::set_ara_bypass:case ToolKind::remove_ara:case ToolKind::reanalyse_ara:{
        if(call.tool==ToolKind::attach_ara)fields(args,{"clip_id","plugin_id"});else if(call.tool==ToolKind::set_ara_bypass)fields(args,{"clip_id","bypass"});else fields(args,{"clip_id"});AraArgs a;a.clip=id(member(args,"clip_id"));
        if(call.tool==ToolKind::attach_ara)a.plugin=id(member(args,"plugin_id"));if(call.tool==ToolKind::set_ara_bypass){const auto& v=member(args,"bypass");require(v.kind==Value::Kind::boolean);a.bypass=v.boolean;}call.arguments=std::move(a);break;}
    case ToolKind::midi_draft:{fields(args,{"mode","track_id","clip_id","name","tick_start","tick_end","notes","cc"});MidiDraftArgs a;const auto mode=string(member(args,"mode"));require(mode=="insert"||mode=="replace"||mode=="edit");a.mode=mode=="insert"?MidiDraftMode::insert:mode=="replace"?MidiDraftMode::replace:MidiDraftMode::edit;a.track=optional_id(args,"track_id");a.clip=optional_id(args,"clip_id");if(args.object.contains("name"))a.name=string(member(args,"name"));a.start=integer(member(args,"tick_start"));a.end=integer(member(args,"tick_end"));
        auto number=[&](const Value& v){const auto n=integer(v);require(n>=0&&n<=127);return static_cast<int>(n);};
        const auto& notes=member(args,"notes");const auto& events=member(args,"cc");require(notes.kind==Value::Kind::array&&events.kind==Value::Kind::array);
        for(const auto& n:notes.array){fields(n,{"id","start","duration","pitch","velocity","channel"});a.notes.push_back({optional_id(n,"id"),integer(member(n,"start")),integer(member(n,"duration")),number(member(n,"pitch")),number(member(n,"velocity")),number(member(n,"channel"))});}
        for(const auto& e:events.array){fields(e,{"id","start","controller","value","channel"});a.cc.push_back({optional_id(e,"id"),integer(member(e,"start")),number(member(e,"controller")),number(member(e,"value")),number(member(e,"channel"))});}
        if(a.mode!=MidiDraftMode::insert)require(!args.object.contains("name"));validate_draft(a);call.arguments=std::move(a);break;}
    case ToolKind::add_marker:fields(args,{"name","tick"});call.arguments=MarkerAddArgs{string(member(args,"name")),integer(member(args,"tick"))};break;
    case ToolKind::rename_marker:fields(args,{"marker_id","name"});call.arguments=MarkerRenameArgs{id(member(args,"marker_id")),string(member(args,"name"))};break;
    case ToolKind::set_selection:{fields(args,{"entities","sample_start","sample_end"});SelectionArgs s;const auto& values=member(args,"entities");require(values.kind==Value::Kind::array);
        for(const auto& value:values.array){fields(value,{"kind","id"});const auto kind=string(member(value,"kind"));const std::array<std::string_view,6> names{"track","folder","clip","chord","section","marker"};const auto at=std::find(names.begin(),names.end(),kind);require(at!=names.end());s.entities.push_back({static_cast<EntityKind>(at-names.begin()),id(member(value,"id"))});}
        if(args.object.contains("sample_start") || args.object.contains("sample_end"))s.range=LoopRange{integer(member(args,"sample_start")),integer(member(args,"sample_end"))};call.arguments=std::move(s);break;}
    case ToolKind::transpose_notes:case ToolKind::set_note_velocity:{const auto value_name=call.tool==ToolKind::transpose_notes?"semitones":"velocity";fields(args,{"clip_id","note_ids",value_name});MidiArgs m;m.clip=id(member(args,"clip_id"));const auto& values=member(args,"note_ids");require(values.kind==Value::Kind::array);for(const auto& value:values.array)m.notes.push_back(id(value));const auto number=integer(member(args,value_name));require(number>=-24 && number<=127);m.value=static_cast<int>(number);call.arguments=std::move(m);break;}
    case ToolKind::query:{fields(args,{"query","id","track_id","clip_id","plugin_id","master","selected_only","cursor","limit","tick_start","tick_end"});Query q;bool found{};const auto name=string(member(args,"query"));
        for(int n=0;n<=static_cast<int>(QueryKind::ara_status);++n)if(query_name(static_cast<QueryKind>(n))==name){q.kind=static_cast<QueryKind>(n);found=true;break;}if(!found)return {Error::unsupported,{}};
        q.id=optional_id(args,"id");q.track=optional_id(args,"track_id");q.clip=optional_id(args,"clip_id");q.plugin=optional_id(args,"plugin_id");
        for(const auto flag:{"master","selected_only"})if(args.object.contains(flag)){const auto& v=member(args,flag);require(v.kind==Value::Kind::boolean);if(std::string_view(flag)=="master")q.master=v.boolean;else q.selected_only=v.boolean;}
        if(args.object.contains("cursor")){const auto n=integer(member(args,"cursor"));require(n>=0 && n<=65536);q.cursor=static_cast<std::size_t>(n);}
        if(args.object.contains("limit")){const auto n=integer(member(args,"limit"));require(n>=1 && n<=256);q.limit=static_cast<std::size_t>(n);}
        if(args.object.contains("tick_start") || args.object.contains("tick_end"))q.ticks={{integer(member(args,"tick_start")),integer(member(args,"tick_end"))}};call.arguments=std::move(q);break;}
    }
    // Reject schema-level bounds here; domain/reference checks are repeated at prepare/query.
    if(call.tool==ToolKind::add_marker){const auto& a=std::get<MarkerAddArgs>(call.arguments);require(text_ok(a.name,4096) && a.tick>=0 && a.tick<=max_tick);}
    if(call.tool==ToolKind::rename_marker)require(text_ok(std::get<MarkerRenameArgs>(call.arguments).name,4096));
    if(call.tool==ToolKind::transpose_notes || call.tool==ToolKind::set_note_velocity){const auto& a=std::get<MidiArgs>(call.arguments);require(!a.notes.empty());
        require(call.tool==ToolKind::transpose_notes?(a.value>=-24 && a.value<=24):(a.value>=1 && a.value<=127));std::set<std::string> ids;for(const auto& note:a.notes)require(ids.insert(note.value).second);}
    if(call.tool==ToolKind::set_selection){const auto& a=std::get<SelectionArgs>(call.arguments);if(a.range)require(a.range->start>=0 && a.range->end>a.range->start && a.range->end<=max_sample);
        std::set<std::pair<int,std::string>> ids;for(const auto& entity:a.entities)require(ids.emplace(static_cast<int>(entity.kind),entity.id.value).second);}
    if(call.tool==ToolKind::query){const auto& q=std::get<Query>(call.arguments);if(q.ticks)require(q.ticks->first>=0 && q.ticks->second>q.ticks->first && q.ticks->second<=max_tick);}
    return {Error::none,std::move(call)};
}catch(const std::exception&){return {Error::invalid_input,{}};}}
PreparedTool prepare_tool(const ToolCall& call,const ProjectState& state,const ContextKey& current,const PermissionPolicy& policy,
    std::uint64_t policy_revision,RuntimeSafety runtime){
    if(call.version!=1)return {Error::unsupported,Block::unknown_tool,{}};
    if(!call.request_id || !text_ok(call.operation_id,128))return {Error::invalid_input,Block::none,{}};
    if(call.key!=current || !state.project || state.project->id!=current.project || state.revision!=current.project_revision)return {Error::stale_context,Block::none,{}};
    const auto access=authorize(policy,call.tool,Intent::suggest,runtime);if(!access.allowed())return {access.error,access.block,{}};
    std::shared_ptr<const ICommand> command;std::size_t affected{};const auto& p=*state.project;
    try{switch(call.tool){
    case ToolKind::list_ara_capabilities:case ToolKind::query_ara_binding:case ToolKind::get_ara_status:return {Error::invalid_input,Block::none,{}};
    case ToolKind::open_ara_editor:case ToolKind::attach_ara:case ToolKind::set_ara_bypass:case ToolKind::remove_ara:case ToolKind::reanalyse_ara:{
        const auto& a=std::get<AraArgs>(call.arguments);require(identifier(a.clip));const auto* clip=find(p.clips,a.clip);if(!clip)return {Error::not_found,Block::none,{}};if(clip->midi||call.tool==ToolKind::reanalyse_ara)return {Error::unsupported,Block::none,{}};
        require(call.tool==ToolKind::attach_ara?(a.plugin&&identifier(*a.plugin)):!a.plugin);require(call.tool==ToolKind::set_ara_bypass||!a.bypass);
        const bool bound=std::any_of(p.ara_bindings.begin(),p.ara_bindings.end(),[&](const auto& b){return b.source==clip->source;});if(call.tool==ToolKind::attach_ara?bound:!bound)return {Error::invalid_input,Block::none,{}};
        affected=call.tool==ToolKind::open_ara_editor?1:static_cast<std::size_t>(std::count_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return !c.midi&&c.source==clip->source;}));break;}
    case ToolKind::midi_draft:{const auto& args=std::get<MidiDraftArgs>(call.arguments);command=prepare_draft(args,p);affected=args.notes.size()+args.cc.size();break;}
    case ToolKind::query:return {Error::invalid_input,Block::none,{}};
    case ToolKind::add_marker:{const auto& args=std::get<MarkerAddArgs>(call.arguments);require(text_ok(args.name,4096) && args.tick>=0 && args.tick<=max_tick);command=std::make_shared<AddMarker>(Marker{new_id(),args.name,args.tick});affected=1;break;}
    case ToolKind::rename_marker:{const auto& args=std::get<MarkerRenameArgs>(call.arguments);require(identifier(args.marker) && text_ok(args.name,4096));const auto* marker=find(p.markers,args.marker);if(!marker)return {Error::not_found,Block::none,{}};auto value=*marker;value.name=args.name;command=std::make_shared<UpdateMarker>(*marker,std::move(value));affected=1;break;}
    case ToolKind::set_selection:{const auto& args=std::get<SelectionArgs>(call.arguments);auto context=make_context(state,current,{},Selection{args.entities,args.range});if(context.error!=Error::none)return {context.error,Block::none,{}};affected=args.entities.size();break;}
    case ToolKind::transpose_notes:case ToolKind::set_note_velocity:{const auto& args=std::get<MidiArgs>(call.arguments);require(identifier(args.clip) && !args.notes.empty() && args.notes.size()<=256);const auto* clip=find(p.clips,args.clip);if(!clip)return {Error::not_found,Block::none,{}};if(!clip->midi)return {Error::unsupported,Block::none,{}};
        require(call.tool==ToolKind::transpose_notes?(args.value>=-24 && args.value<=24):(args.value>=1 && args.value<=127));std::set<std::string> unique;
        for(const auto& id:args.notes){require(identifier(id) && unique.insert(id.value).second);const auto* note=find(clip->midi->notes,id);if(!note)return {Error::not_found,Block::none,{}};
            require(note->start+note->length>clip->midi->source_offset && note->start<clip->midi->source_offset+clip->midi->length);
            if(call.tool==ToolKind::transpose_notes)require(note->pitch+args.value>=0 && note->pitch+args.value<=127);}
        NoteEdit edit;edit.kind=call.tool==ToolKind::transpose_notes?NoteEditKind::transpose:NoteEditKind::velocity;edit.mode=call.tool==ToolKind::transpose_notes?NoteValueMode::add:NoteValueMode::set;edit.value=args.value;
        (void)transform_midi_notes(*clip->midi,args.notes,edit);command=std::make_shared<EditMidiNotes>(args.clip,args.notes,edit);affected=args.notes.size();break;}
    }
    auto decision=authorize(policy,call.tool,Intent::apply,runtime,{});
    return {Error::none,Block::none,std::shared_ptr<const ToolPlan>(new ToolPlan(call,policy_revision,affected,decision,std::move(command)))};
    }catch(const std::exception&){return {Error::invalid_input,Block::none,{}};}
}
} // namespace mrs::ai
