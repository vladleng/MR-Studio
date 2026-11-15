#include <mrs/processing.hpp>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace mrs::processing;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
#define CHECK(...) check((__VA_ARGS__),#__VA_ARGS__)
template<class F> void rejects(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}CHECK(rejected);}
GraphSnapshot snapshot(GraphState state){return {std::make_shared<const GraphState>(std::move(state)),0,false,false};}
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
    std::cout<<"Sub-patch settings: complete sound, processor identity, atomic rollback and slot guard PASS\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
