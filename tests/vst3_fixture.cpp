#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <windows.h>
#include <cstring>
extern void* moduleHandle;
bool InitModule(){wchar_t path[32768]{};GetModuleFileNameW(static_cast<HMODULE>(moduleHandle),path,32768);if(wcsstr(path,L"crash.vst3"))RaiseException(0xE0000042,EXCEPTION_NONCONTINUABLE,0,nullptr);if(wcsstr(path,L"hang.vst3"))Sleep(30000);return true;}
bool DeinitModule(){return true;}
using namespace Steinberg;
using namespace Steinberg::Vst;
class Fixture final : public SingleComponentEffect {
    float gain_{0.5f};
public:
    static FUnknown* create(void*){return static_cast<IComponent*>(new Fixture);}
    tresult PLUGIN_API initialize(FUnknown* context) override{auto result=SingleComponentEffect::initialize(context);if(result!=kResultOk)return result;addAudioInput(STR16("Input"),SpeakerArr::kStereo);addAudioOutput(STR16("Output"),SpeakerArr::kStereo);parameters.addParameter(STR16("Gain"),nullptr,0,0.5,ParameterInfo::kCanAutomate,100);return kResultOk;}
    tresult PLUGIN_API getState(IBStream* s) override{return s->write(&gain_,sizeof(gain_),nullptr);}
    tresult PLUGIN_API setState(IBStream* s) override{float v{};int32 n{};if(s->read(&v,sizeof(v),&n)!=kResultOk||n!=sizeof(v)||v<0||v>1)return kInvalidArgument;gain_=v;setParamNormalized(100,v);return kResultOk;}
    tresult PLUGIN_API setComponentState(IBStream* s) override{return setState(s);}
    uint32 PLUGIN_API getLatencySamples() override{return 7;}
    tresult PLUGIN_API setProcessing(TBool) override{return kResultOk;}
    tresult PLUGIN_API process(ProcessData& d) override{
        if(d.inputParameterChanges)for(int32 i=0;i<d.inputParameterChanges->getParameterCount();++i){auto q=d.inputParameterChanges->getParameterData(i);if(q->getParameterId()==100)for(int32 j=0;j<q->getPointCount();++j){int32 offset{};ParamValue value{};if(q->getPoint(j,offset,value)==kResultOk)gain_=static_cast<float>(value);}}
        for(int32 c=0;c<d.outputs[0].numChannels;++c)for(int32 f=0;f<d.numSamples;++f)d.outputs[0].channelBuffers32[c][f]=d.inputs[0].channelBuffers32[c][f]*gain_;d.outputs[0].silenceFlags=0;return kResultOk;
    }
    void edit(){setParamNormalized(100,0.75);if(componentHandler){componentHandler->beginEdit(100);componentHandler->performEdit(100,0.75);componentHandler->endEdit(100);}}
    class View final : public CPluginView{
        Fixture* owner_;HWND child_{};
        static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){auto owner=reinterpret_cast<Fixture*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_COMMAND&&owner){owner->edit();return 0;}return DefWindowProcW(hwnd,msg,wp,lp);}
    public:
        explicit View(Fixture* owner):owner_(owner){rect={0,0,320,120};owner_->addRef();}
        ~View() override{owner_->release();}
        tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override{return std::strcmp(type,kPlatformTypeHWND)==0?kResultOk:kResultFalse;}
        tresult PLUGIN_API attached(void* parent,FIDString type) override{if(isPlatformTypeSupported(type)!=kResultOk)return kResultFalse;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"MRSVstFixtureEditor";RegisterClassW(&cls);child_=CreateWindowW(cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE,0,0,320,120,static_cast<HWND>(parent),nullptr,cls.hInstance,nullptr);SetWindowLongPtrW(child_,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(owner_));CreateWindowW(L"BUTTON",L"Set gain to 0.75",WS_CHILD|WS_VISIBLE,20,30,250,40,child_,reinterpret_cast<HMENU>(1),cls.hInstance,nullptr);return kResultOk;}
        tresult PLUGIN_API removed() override{if(child_)DestroyWindow(child_);child_=nullptr;return kResultOk;}
    };
    IPlugView* PLUGIN_API createView(FIDString name) override{return std::strcmp(name,ViewType::kEditor)==0?new View(this):nullptr;}
};
static const FUID uid(0x923044A1,0x68E94512,0xAD018284,0xC1B0BE32);
BEGIN_FACTORY_DEF("MR Studio Tests","","vlad@example.invalid")
DEF_CLASS2(INLINE_UID_FROM_FUID(uid),PClassInfo::kManyInstances,kVstAudioEffectClass,"MRS Test Gain",Vst::kDistributable,"Fx","1.0",kVstVersionString,Fixture::create)
END_FACTORY
