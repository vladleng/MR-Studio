#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <windows.h>
#include <cstring>
#include <array>
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
extern void* moduleHandle;
bool InitModule(){wchar_t path[32768]{};GetModuleFileNameW(static_cast<HMODULE>(moduleHandle),path,32768);if(wcsstr(path,L"crash.vst3"))RaiseException(0xE0000042,EXCEPTION_NONCONTINUABLE,0,nullptr);if(wcsstr(path,L"hang.vst3"))Sleep(30000);return true;}
bool DeinitModule(){return true;}
using namespace Steinberg;
using namespace Steinberg::Vst;
class Fixture final : public SingleComponentEffect, public IMidiMapping {
    float gain_{0.5f};bool activationReset_{},instrument_{},sustain_{};float bend_{0.5f};
    std::array<std::array<float,128>,16> notes_{};std::array<std::array<bool,128>,16> keys_{};
public:
    tresult PLUGIN_API queryInterface(const TUID iid,void** obj) override{if(FUnknownPrivate::iidEqual(iid,IMidiMapping::iid)){*obj=static_cast<IMidiMapping*>(this);SingleComponentEffect::addRef();return kResultOk;}return SingleComponentEffect::queryInterface(iid,obj);}
    uint32 PLUGIN_API addRef() override{return SingleComponentEffect::addRef();}
    uint32 PLUGIN_API release() override{return SingleComponentEffect::release();}
    tresult PLUGIN_API getMidiControllerAssignment(int32 bus,int16,int16 cc,ParamID& id) override{if(!instrument_||bus!=0)return kResultFalse;if(cc==64)id=300;else if(cc==129)id=301;else if(cc==128)id=302;else return kResultFalse;return kResultOk;}
    static FUnknown* create(void*){return static_cast<IComponent*>(new Fixture);}
    tresult PLUGIN_API initialize(FUnknown* context) override{auto result=SingleComponentEffect::initialize(context);if(result!=kResultOk)return result;wchar_t kindPath[32768]{};GetModuleFileNameW(static_cast<HMODULE>(moduleHandle),kindPath,32768);instrument_=wcsstr(kindPath,L"instrument.vst3")!=nullptr;if(!instrument_)addAudioInput(STR16("Input"),SpeakerArr::kStereo);addAudioOutput(STR16("Output"),SpeakerArr::kStereo);if(!instrument_)addAudioInput(STR16("Sidechain"),SpeakerArr::kStereo,kAux,0);else {addEventInput(STR16("MIDI"),16);parameters.addParameter(STR16("Sustain"),nullptr,0,0,ParameterInfo::kCanAutomate,300);parameters.addParameter(STR16("Bend"),nullptr,0,0.5,ParameterInfo::kCanAutomate,301);parameters.addParameter(STR16("Pressure"),nullptr,0,0,ParameterInfo::kCanAutomate,302);}parameters.addParameter(STR16("Gain"),nullptr,0,0.5,ParameterInfo::kCanAutomate,100);parameters.addParameter(STR16("Program"),nullptr,0,0,ParameterInfo::kIsProgramChange,200);wchar_t modulePath[32768]{};GetModuleFileNameW(static_cast<HMODULE>(moduleHandle),modulePath,32768);activationReset_=wcsstr(modulePath,L"activation-state.vst3")!=nullptr;if(wcsstr(modulePath,L"legacy-state.vst3"))for(ParamID id=1000;id<1300;++id)parameters.addParameter(STR16("Legacy control"),nullptr,0,0,ParameterInfo::kCanAutomate,id);return kResultOk;}
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* ins,int32 ni,SpeakerArrangement* outs,int32 no) override{if(ni!=(instrument_?0:2)||no!=1)return kResultFalse;return SingleComponentEffect::setBusArrangements(ins,ni,outs,no);}
    // Re-setting the program selector also reloads its default sound, even at the same value.
    tresult PLUGIN_API setParamNormalized(ParamID id,ParamValue value) override{if(id==200)gain_=0.5f;return SingleComponentEffect::setParamNormalized(id,value);}
    tresult PLUGIN_API getState(IBStream* s) override{return s->write(&gain_,sizeof(gain_),nullptr);}
    tresult PLUGIN_API setState(IBStream* s) override{float v{};int32 n{};if(s->read(&v,sizeof(v),&n)!=kResultOk||n!=sizeof(v)||v<0||v>1)return kInvalidArgument;gain_=v;setParamNormalized(100,v);return kResultOk;}
    tresult PLUGIN_API setComponentState(IBStream* s) override{return setState(s);}
    tresult PLUGIN_API setActive(TBool active) override{if(active&&activationReset_)gain_=0.5f;return SingleComponentEffect::setActive(active);}
    uint32 PLUGIN_API getLatencySamples() override{return 7;}
    tresult PLUGIN_API setProcessing(TBool) override{return kResultOk;}
    tresult PLUGIN_API process(ProcessData& d) override{
        if(instrument_){
            for(int32 f=0;f<d.numSamples;++f){
                if(d.inputParameterChanges)for(int32 i=0;i<d.inputParameterChanges->getParameterCount();++i){auto q=d.inputParameterChanges->getParameterData(i);for(int32 j=0;j<q->getPointCount();++j){int32 offset{};ParamValue v{};if(q->getPoint(j,offset,v)!=kResultOk||offset!=f)continue;if(q->getParameterId()==300){sustain_=v>=0.5;if(!sustain_)for(int ch=0;ch<16;++ch)for(int n=0;n<128;++n)if(!keys_[ch][n])notes_[ch][n]=0;}else if(q->getParameterId()==301)bend_=static_cast<float>(v);else if(q->getParameterId()==100)gain_=static_cast<float>(v);}}
                if(d.inputEvents)for(int32 i=0;i<d.inputEvents->getEventCount();++i){Event e{};if(d.inputEvents->getEvent(i,e)!=kResultOk||e.sampleOffset!=f)continue;if(e.type==Event::kNoteOnEvent){notes_[e.noteOn.channel][e.noteOn.pitch]=e.noteOn.velocity;keys_[e.noteOn.channel][e.noteOn.pitch]=true;}else if(e.type==Event::kNoteOffEvent){keys_[e.noteOff.channel][e.noteOff.pitch]=false;if(!sustain_)notes_[e.noteOff.channel][e.noteOff.pitch]=0;}}
                float v=0;for(const auto& ch:notes_)for(float note:ch)v+=note;v*=gain_*(0.5f+bend_);
                for(int32 c=0;c<d.outputs[0].numChannels;++c)d.outputs[0].channelBuffers32[c][f]=v;
            }d.outputs[0].silenceFlags=0;return kResultOk;
        }
        if(d.inputParameterChanges)for(int32 i=0;i<d.inputParameterChanges->getParameterCount();++i){auto q=d.inputParameterChanges->getParameterData(i);if(q->getParameterId()==100)for(int32 j=0;j<q->getPointCount();++j){int32 offset{};ParamValue value{};if(q->getPoint(j,offset,value)==kResultOk)gain_=static_cast<float>(value);}}
        for(int32 c=0;c<d.outputs[0].numChannels;++c)for(int32 f=0;f<d.numSamples;++f)d.outputs[0].channelBuffers32[c][f]=d.inputs[0].channelBuffers32[c][f]*gain_;d.outputs[0].silenceFlags=0;return kResultOk;
    }
    void edit(){setParamNormalized(100,0.75);if(componentHandler){componentHandler->beginEdit(100);componentHandler->performEdit(100,0.75);componentHandler->endEdit(100);}}
    class View final : public CPluginView{
        Fixture* owner_;HWND child_{};inline static int attachedViews_{};
        static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){auto owner=reinterpret_cast<Fixture*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_COMMAND&&owner){if(LOWORD(wp)==2){owner->gain_=0.625f;owner->setParamNormalized(100,0.625);if(owner->componentHandler)owner->componentHandler->restartComponent(kParamValuesChanged);}else owner->edit();return 0;}return DefWindowProcW(hwnd,msg,wp,lp);}
    public:
        explicit View(Fixture* owner):owner_(owner){rect={0,0,320,120};owner_->addRef();}
        ~View() override{owner_->release();}
        tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override{return std::strcmp(type,kPlatformTypeHWND)==0?kResultOk:kResultFalse;}
        tresult PLUGIN_API attached(void* parent,FIDString type) override{if(isPlatformTypeSupported(type)!=kResultOk)return kResultFalse;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"MRSVstFixtureEditor";RegisterClassW(&cls);child_=CreateWindowW(cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE,0,0,320,120,static_cast<HWND>(parent),nullptr,cls.hInstance,nullptr);if(child_)++attachedViews_;SetWindowLongPtrW(child_,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(owner_));CreateWindowW(L"BUTTON",L"Set gain to 0.75",WS_CHILD|WS_VISIBLE,20,30,250,40,child_,reinterpret_cast<HMENU>(1),cls.hInstance,nullptr);return kResultOk;}
        tresult PLUGIN_API removed() override{if(child_){DestroyWindow(child_);if(--attachedViews_==0)UnregisterClassW(L"MRSVstFixtureEditor",GetModuleHandleW(nullptr));}child_=nullptr;return kResultOk;}
    };
    IPlugView* PLUGIN_API createView(FIDString name) override{return std::strcmp(name,ViewType::kEditor)==0?new View(this):nullptr;}
};
static const FUID uid(0x923044A1,0x68E94512,0xAD018284,0xC1B0BE32);
BEGIN_FACTORY_DEF("MR Studio Tests","","vlad@example.invalid")
DEF_CLASS2(INLINE_UID_FROM_FUID(uid),PClassInfo::kManyInstances,kVstAudioEffectClass,"MRS Test Gain",Vst::kDistributable,"Fx","1.0",kVstVersionString,Fixture::create)
END_FACTORY
