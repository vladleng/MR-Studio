#include <ARA_API/ARAVST3.h>
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include <atomic>
using namespace Steinberg;
namespace ARA { DEF_CLASS_IID(IMainFactory) }
namespace {
class InvalidFactory final : public ARA::IMainFactory {
    std::atomic<uint32> references_{1};
public:
    tresult PLUGIN_API queryInterface(const TUID iid, void** out) override {
        if (FUnknownPrivate::iidEqual(iid, ARA::IMainFactory::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
            *out = this; addRef(); return kResultOk;
        }
        *out = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return ++references_; }
    uint32 PLUGIN_API release() override { const auto count = --references_; if (!count) delete this; return count; }
    const ARA::ARAFactory* PLUGIN_API getFactory() override {
        static const ARA::ARAFactory invalid{}; // truncated factory, no callable functions
        return &invalid;
    }
};
FUnknown* createFactory(void*) { return static_cast<ARA::IMainFactory*>(new InvalidFactory); }
FUnknown* createEffect(void*) { return nullptr; } // must never be reached
const FUID effect(0x5AD13710, 0x21834E08, 0xB5811F00, 0x9AA00001);
const FUID factory(0x5AD13710, 0x21834E08, 0xB5811F00, 0x9AA00002);
}
BEGIN_FACTORY_DEF("MR Studio Tests", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(effect), PClassInfo::kManyInstances, kVstAudioEffectClass,
    "Invalid ARA", 0, "Fx", "1.0", kVstVersionString, createEffect)
DEF_CLASS2(INLINE_UID_FROM_FUID(factory), PClassInfo::kManyInstances, kARAMainFactoryClass,
    "Invalid ARA", 0, "", "1.0", kVstVersionString, createFactory)
END_FACTORY
bool InitDll() { return true; }
bool ExitDll() { return true; }
