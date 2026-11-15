#include <mrs/processing.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace mrs::processing;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
#define CHECK(...) check((__VA_ARGS__),#__VA_ARGS__)
template<class F> void rejects(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}CHECK(rejected);}
GraphSnapshot snapshot(GraphState state){return {std::make_shared<const GraphState>(std::move(state)),0,false,false};}
class TwoControls final:public IProcessor {
    std::array<float,2> values{.5f,.5f};
public:
    inline static unsigned captures{},restores{};
    std::vector<ParameterInfo> parameters() const override{return {{100,0,1,.5f,true},{101,0,1,.5f,true}};}
    void prepare(ProcessConfig) override{}
    void restore(const PluginState& state) override{++restores;if(state.component.size()!=sizeof(values))throw std::runtime_error("bad two-control state");std::memcpy(values.data(),state.component.data(),sizeof(values));}
    PluginState capture() const override{++captures;PluginState state;state.class_id="0123456789ABCDEF0123456789ABCDEF";state.component.resize(sizeof(values));std::memcpy(state.component.data(),values.data(),sizeof(values));return state;}
    bool set_parameter(std::uint32_t id,float value) noexcept override{if(id<100||id>101||value<0||value>1)return false;values[id-100]=value;return true;}
    std::optional<float> parameter_value(std::uint32_t id) const noexcept override{return id>=100&&id<=101?std::optional<float>{values[id-100]}:std::nullopt;}
    std::uint32_t latency() const noexcept override{return 0;}
    bool live_safe() const noexcept override{return true;}
    void warm() override{}
    void reset() noexcept override{}
    void process(ProcessBlock block) noexcept override{for(const auto& p:block.parameters)(void)set_parameter(p.id,p.value);for(auto& value:block.audio)value*=values[0]*values[1];}
};

}
int main(){try {
    std::array<float,16> audio{};
    // #374: the same processors must render complete settings, with an
    // all-slot rollback when a later slot rejects a setting.
    mrs::NativeInsert first;first.id={"settings-a"};first.kind=mrs::InsertKind::gain;first.gain=.5f;
    auto second=first;second.id={"settings-b"};
    PreparedGraph settings{snapshot(insert_graph(std::array{first,second})),{48000,2,8}};
    const auto identity=settings.instance_identity();
    first.gain=.75f;second.gain=.25f;
    const auto desired=insert_graph(std::array{first,second});bool rollbackFailed=false;
    settings.restore_settings(desired,rollbackFailed);
    audio.fill(1);settings.process(audio.data(),8);
    CHECK(audio[0]==.1875f && !rollbackFailed && settings.instance_identity()==identity);
    auto invalid=desired;invalid.nodes[0].parameters[0].value=.9f;
    invalid.nodes[1].parameters[0].id=99;
    rejects([&]{settings.restore_settings(invalid,rollbackFailed);});
    audio.fill(1);settings.process(audio.data(),8);
    CHECK(audio[0]==.1875f && !rollbackFailed && *settings.snapshot().graph==desired);
    auto wrongSlot=desired;wrongSlot.nodes[0].processor_id="mrs.lowpass";
    rejects([&]{settings.restore_settings(wrongSlot,rollbackFailed);});
    audio.fill(1);settings.process(audio.data(),8);CHECK(audio[0]==.1875f);
    // A modern variant may legitimately override EVERY exposed parameter.
    // It must not be mistaken for a legacy stale full-parameter snapshot.
    auto twoState=desired;twoState.nodes.resize(1);twoState.edges={{std::nullopt,first.id,1}};twoState.outputs={first.id};
    twoState.nodes[0].format=ProcessorFormat::vst3;twoState.nodes[0].processor_id="test.two-controls";
    twoState.nodes[0].plugin=TwoControls{}.capture();twoState.nodes[0].parameters.clear();
    PreparedGraph two{snapshot(twoState),{48000,2,8},[](const NodeState&){return std::make_unique<TwoControls>();}};
    twoState.nodes[0].parameters={{100,.75f},{101,.25f}};
    two.restore_settings(twoState,rollbackFailed);
    audio.fill(1);two.process(audio.data(),8);CHECK(audio[0]==.1875f);
    const auto selected=two.capture();
    CHECK(two.enqueue_parameters(selected));
    audio.fill(1);two.process(audio.data(),8);
    check(audio[0]==.1875f,"Unrelated parameter publication cannot reset a materialized variant");
    const auto capturesBefore=TwoControls::captures,restoresBefore=TwoControls::restores;
    auto fastState=twoState;fastState.nodes[0].parameters={{100,.9f},{101,.1f}};
    CHECK(two.enqueue_parameters(fastState,true));
    // A newer baseline request must override a queued A even before A renders.
    fastState.nodes[0].parameters={{100,.5f},{101,.5f}};
    CHECK(two.enqueue_parameters(fastState,true));
    audio.fill(1);two.process(audio.data(),8);CHECK(audio[0]==.25f);
    fastState.nodes[0].parameters={{100,.75f},{101,.25f}};
    CHECK(two.enqueue_parameters(fastState,true));
    audio.fill(1);two.process(audio.data(),8);CHECK(audio[0]==.1875f);
    CHECK(TwoControls::captures==capturesBefore&&TwoControls::restores==restoresBefore);
    // Fresh Patch loading must interpret modern explicit controls as such.
    PreparedGraph freshlyLoaded{snapshot(twoState),{48000,2,8},[](const NodeState&){return std::make_unique<TwoControls>();},true};
    audio.fill(1);freshlyLoaded.process(audio.data(),8);
    check(audio[0]==.1875f,"Modern full-parameter variant survives a fresh Patch load");
    PreparedGraph legacy{snapshot(twoState),{48000,2,8},[](const NodeState&){return std::make_unique<TwoControls>();}};
    audio.fill(1);legacy.process(audio.data(),8);
    check(audio[0]==.25f,"Legacy opaque state remains authoritative over a historical parameter mirror");
    const auto materialized=freshlyLoaded.capture();
    CHECK(materialized.nodes[0].parameters.empty());
    PreparedGraph reopened{snapshot(materialized),{48000,2,8},[](const NodeState&){return std::make_unique<TwoControls>();}};
    audio.fill(1);reopened.process(audio.data(),8);
    check(audio[0]==.1875f,"Materialized modern variant survives Save/Reopen with legacy compatibility enabled");
    auto noControls=materialized;
    CHECK(reopened.enqueue_parameters(noControls));
    audio.fill(1);reopened.process(audio.data(),8);CHECK(audio[0]==.1875f);

    std::cout<<"Sub-patch settings: complete sound, processor identity, atomic rollback and slot guard PASS\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
