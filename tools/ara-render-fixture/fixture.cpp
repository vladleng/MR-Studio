// Test-only extension of the Apache-2.0 Celemony ARA_Examples processor.
// SDK implementation stays in build/_deps; original notices stay with it.
#include "TestVST3Processor.h"
#include "ARATestMainFactory.h"
#include "ARATestAudioSource.h"
#include "TestPlugInConfig.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/base/ibstream.h"
#include <cmath>
#include <array>
#include <cstring>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <string>
#include <vector>
#include <windows.h>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
std::mutex transportMutex;
ProcessContext lastTransport{};
std::uint64_t transportCalls{};
ARA::PlugIn::HostPlaybackController* requestHost{};
ARA::PlugIn::DocumentController* fixtureDocument{};
std::mutex patchMutex;
std::string patchSource;
double patchFirst{},patchLast{};float patchGain{1.f};
std::atomic<std::uint32_t> renderLatency{17},renderTail{};
std::atomic<bool> liveFault{};
std::atomic<float> auditionLevel{};
std::atomic<bool> requestFromRender{};
std::atomic<std::uint64_t> patchEpoch{},publishedHash{};
std::atomic<double> publishedFirst{},publishedLast{};
std::atomic<float> publishedGain{1.f};
std::uint64_t sourceHash(const char* text){std::uint64_t hash=14695981039346656037ull;for(;*text;++text){hash^=static_cast<unsigned char>(*text);hash*=1099511628211ull;}return hash;}
void publishPatch(){++patchEpoch;publishedHash=patchSource.empty()?0:sourceHash(patchSource.c_str());publishedFirst=patchFirst;publishedLast=patchLast;publishedGain=patchGain;++patchEpoch;}
bool editorMode(const wchar_t* mode){wchar_t value[32]{};GetEnvironmentVariableW(L"MRS_ARA_TEST_EDITOR_MODE",value,32);return std::wcscmp(value,mode)==0;}
void requestTransport(int action,double position,double duration,bool enabled){if(!requestHost)return;switch(action){case 0:requestHost->requestStartPlayback();break;case 1:requestHost->requestStopPlayback();break;case 2:requestHost->requestSetPlaybackPosition(position);break;case 3:requestHost->requestSetCycleRange(position,duration);break;case 4:requestHost->requestEnableCycle(enabled);break;}}
class FixtureView final : public CPluginView {
    EditController& controller;
    HWND window{};
    static LRESULT CALLBACK dispatch(HWND h,UINT m,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<FixtureView*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<FixtureView*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(self&&m==WM_COMMAND&&LOWORD(w)==179){const auto value=self->controller.getParamNormalized(179)>.5?.25:1.;self->controller.beginEdit(179);self->controller.setParamNormalized(179,value);self->controller.performEdit(179,value);self->controller.endEdit(179);return 0;}
        if(self&&m==WM_COMMAND&&LOWORD(w)>=181&&LOWORD(w)<=183){requestTransport(LOWORD(w)==181?0:LOWORD(w)==182?1:2,1.,0,false);return 0;}
        if(self&&m==WM_TIMER){ProcessContext state{};{std::lock_guard lock(transportMutex);state=lastTransport;}wchar_t text[128]{};swprintf_s(text,L"Sample %lld | %s",static_cast<long long>(state.projectTimeSamples),(state.state&ProcessContext::kPlaying)?L"Playing":L"Stopped");SetWindowTextW(GetDlgItem(h,180),text);return 0;}
        return DefWindowProcW(h,m,w,l);
    }
public:
    explicit FixtureView(EditController& c):controller(c){controller.addRef();rect={0,0,640,320};}
    ~FixtureView() override {if(window)DestroyWindow(window);UnregisterClassW(L"MRSAraFixture179",GetModuleHandleW(nullptr));controller.release();}
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {return std::strcmp(type,kPlatformTypeHWND)==0?kResultOk:kResultFalse;}
    tresult PLUGIN_API canResize() override {return editorMode(L"resize")?kResultTrue:kResultFalse;}
    tresult PLUGIN_API checkSizeConstraint(ViewRect* r) override {if(!editorMode(L"resize"))return kNotImplemented;r->right=r->left+std::clamp(r->getWidth(),320,1600);r->bottom=r->top+std::clamp(r->getHeight(),160,1000);return kResultOk;}
    tresult PLUGIN_API onSize(ViewRect* r) override {const auto result=CPluginView::onSize(r);if(window&&editorMode(L"resize")){
        RECT parentSize{};GetClientRect(GetParent(window),&parentSize);
        if(parentSize.right!=r->getWidth()||parentSize.bottom!=r->getHeight())SetPropW(window,L"MRS_ARA_PARENT_SIZE_MISMATCH",reinterpret_cast<HANDLE>(1));
        SetWindowPos(window,nullptr,0,0,r->getWidth(),r->getHeight(),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        // Exercise native editors that repaint synchronously during onSize.
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    }return result;}
    tresult PLUGIN_API getSize(ViewRect* size) override {
        if(editorMode(L"invalid")||(editorMode(L"late")&&!window)){*size={0,0,0,0};return kResultOk;}
        return CPluginView::getSize(size);
    }
    tresult PLUGIN_API attached(void* parent,FIDString type) override {
        if(editorMode(L"attach-fail"))return kResultFalse;
        if(isPlatformTypeSupported(type)!=kResultOk)return kResultFalse;
        WNDCLASSW wc{};wc.lpfnWndProc=dispatch;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MRSAraFixture179";wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
        window=CreateWindowW(wc.lpszClassName,L"ARA fixture editor",WS_CHILD|WS_VISIBLE,0,0,640,320,static_cast<HWND>(parent),nullptr,wc.hInstance,this);
        CreateWindowW(L"BUTTON",L"Toggle gain 1.0 / 0.25",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,24,24,240,40,window,reinterpret_cast<HMENU>(179),wc.hInstance,nullptr);
        CreateWindowW(L"BUTTON",L"Play",WS_CHILD|WS_VISIBLE,24,80,100,32,window,reinterpret_cast<HMENU>(181),wc.hInstance,nullptr);
        CreateWindowW(L"BUTTON",L"Stop",WS_CHILD|WS_VISIBLE,132,80,100,32,window,reinterpret_cast<HMENU>(182),wc.hInstance,nullptr);
        CreateWindowW(L"BUTTON",L"Seek 1s",WS_CHILD|WS_VISIBLE,240,80,100,32,window,reinterpret_cast<HMENU>(183),wc.hInstance,nullptr);
        CreateWindowW(L"STATIC",L"Waiting for transport",WS_CHILD|WS_VISIBLE,24,140,580,32,window,reinterpret_cast<HMENU>(180),wc.hInstance,nullptr);SetTimer(window,1,30,nullptr);
        return window?CPluginView::attached(parent,type):kResultFalse;
    }
    tresult PLUGIN_API removed() override {if(window)DestroyWindow(window);window=nullptr;return CPluginView::removed();}
};
class FixtureController;
std::vector<FixtureController*> fixtureControllers;
class FixtureController final : public EditController {
public:
    ~FixtureController() override {std::erase(fixtureControllers,this);}
    void latencyChanged(){if(componentHandler)componentHandler->restartComponent(kLatencyChanged);}
    tresult PLUGIN_API initialize(FUnknown* context) override {const auto result=EditController::initialize(context);parameters.addParameter(STR16("Fixture gain"),nullptr,0,1.,ParameterInfo::kCanAutomate,179);if(result==kResultOk)fixtureControllers.push_back(this);return result;}
    tresult PLUGIN_API setComponentState(IBStream* stream) override {float gain{};int32 bytes{};if(!stream||stream->read(&gain,sizeof(gain),&bytes)!=kResultOk||bytes!=static_cast<int32>(sizeof(gain))||!std::isfinite(gain)||gain<0||gain>1)return kResultFalse;return setParamNormalized(179,gain);}
    IPlugView* PLUGIN_API createView(FIDString type) override {return std::strcmp(type,ViewType::kEditor)==0?new FixtureView(*this):nullptr;}
    static FUnknown* create(void*){return static_cast<IEditController*>(new FixtureController);}
};
class DelayedGain final : public TestVST3Processor {
    std::array<std::array<float,64>,2> delay_{};
    std::size_t cursor_{};
    float editorGain{1.f};
    std::uint64_t currentHash{};double currentFirst{},currentLast{};float currentGain{1.f};
public:
    ~DelayedGain() override {if(auto* editor=_araPlugInExtension.getEditorRenderer()){if(requestHost==editor->getDocumentController()->getHostPlaybackController())requestHost=nullptr;if(fixtureDocument==editor->getDocumentController())fixtureDocument=nullptr;}}
    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentControllerWithRoles(ARA::ARADocumentControllerRef doc,ARA::ARAPlugInInstanceRoleFlags known,ARA::ARAPlugInInstanceRoleFlags assigned) override {
        const auto* result=TestVST3Processor::bindToDocumentControllerWithRoles(doc,known,assigned);
        if(auto* editor=_araPlugInExtension.getEditorRenderer()){requestHost=editor->getDocumentController()->getHostPlaybackController();fixtureDocument=editor->getDocumentController();}
        {std::lock_guard lock(transportMutex);lastTransport={};transportCalls=0;}return result;
    }
    uint32 PLUGIN_API getLatencySamples() override { return renderLatency.load(); }
    uint32 PLUGIN_API getTailSamples() override {return renderTail.load();}
    tresult PLUGIN_API getState(IBStream* stream) override {wchar_t fail[2]{};if(GetEnvironmentVariableW(L"MRS_ARA_TEST_STATE_FAIL",fail,2))return kResultFalse;int32 bytes{};return stream&&stream->write(&editorGain,sizeof(editorGain),&bytes)==kResultOk&&bytes==static_cast<int32>(sizeof(editorGain))?kResultOk:kResultFalse;}
    tresult PLUGIN_API setState(IBStream* stream) override {float gain{};int32 bytes{};if(!stream||stream->read(&gain,sizeof(gain),&bytes)!=kResultOk||bytes!=static_cast<int32>(sizeof(gain))||!std::isfinite(gain)||gain<0||gain>1)return kResultFalse;editorGain=gain;return kResultOk;}
    tresult PLUGIN_API setActive(TBool on) override {
        if(on){if(auto* editor=_araPlugInExtension.getEditorRenderer()){
            if(editor->getRegionSequences().empty())return kResultFalse;
            wchar_t required[2]{};
            if(GetEnvironmentVariableW(L"MRS_ARA_TEST_REQUIRE_ANALYSIS",required,2))
                for(auto* sequence:editor->getRegionSequences())for(auto* region:sequence->getPlaybackRegions())
                    if(!region->getAudioModification()->getAudioSource<ARATestAudioSource>()->getNoteContent())return kResultFalse;
        }}
        if (on) { delay_ = {}; cursor_ = 0; }
        return TestVST3Processor::setActive(on);
    }
    tresult PLUGIN_API process(ProcessData& data) override {
        // A zero-frame parameter flush is legal, but need not run the plugin's
        // audio callback, where native ARA editors commonly read their playhead.
        if(data.numSamples==0)return kResultOk;
        const bool editor=_araPlugInExtension.getEditorRenderer()!=nullptr;
        if(data.processMode==kRealtime&&!editor&&liveFault.load())return kResultFalse;
        if(data.processMode==kRealtime&&editor){
            if(requestFromRender.load())requestTransport(0,0,0,false);
            wchar_t fail[2]{};if(GetEnvironmentVariableW(L"MRS_ARA_TEST_TRANSPORT_FAIL",fail,2))return kResultFalse;
            if(!data.processContext||data.numOutputs==0||!data.outputs)return kResultFalse;
            std::lock_guard lock(transportMutex);lastTransport=*data.processContext;++transportCalls;
        }
        if(data.inputParameterChanges)for(int32 i=0;i<data.inputParameterChanges->getParameterCount();++i){auto* q=data.inputParameterChanges->getParameterData(i);if(q&&q->getParameterId()==179){int32 offset{};ParamValue value{};if(q->getPoint(q->getPointCount()-1,offset,value)==kResultOk)editorGain=static_cast<float>(value);}}
        wchar_t slow[2]{};
        if ((data.processMode==kOffline||editor)&&GetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",slow,2)) Sleep(slow[0]==L'2'?20:2);
        auto result = TestVST3Processor::process(data);
        if (result != kResultOk) return result;
        bool patched=false;double first{},last{},shift{};float changedGain{1.f};
        for(int attempt=0;attempt<2;++attempt){const auto before=patchEpoch.load();if(before&1)continue;const auto hash=publishedHash.load();const auto begin=publishedFirst.load(),end=publishedLast.load();const auto value=publishedGain.load();if(before==patchEpoch.load()){currentHash=hash;currentFirst=begin;currentLast=end;currentGain=value;break;}}
        if(auto* renderer=_araPlugInExtension.getPlaybackRenderer())for(auto* region:renderer->getPlaybackRegions()){
            if(currentHash==sourceHash(region->getAudioModification()->getAudioSource()->getPersistentID().c_str())){patched=true;first=currentFirst;last=currentLast;changedGain=currentGain;shift=region->getStartInAudioModificationTime()-region->getStartInPlaybackTime();}break;
        }
        for (int32 f = 0; f < data.numSamples; ++f) {
            for (int32 c = 0; c < data.outputs[0].numChannels; ++c) {
                auto& value = data.outputs[0].channelBuffers32[c][f];
                // Default 120bpm/4 beats gives gain0.5. Deliberately depend on
                // transport context to test cache invalidation and boundaries.
                const auto gain = static_cast<float>(data.processContext->tempo/240.)*
                    static_cast<float>(data.processContext->timeSigNumerator)/4.f;
                const auto time=static_cast<double>(data.processContext->projectTimeSamples+f)/data.processContext->sampleRate+shift;
                const auto dry = value*gain*editorGain*(patched&&time>=first&&time<last?changedGain:1.f);if(renderLatency.load()){value=delay_[c][cursor_];delay_[c][cursor_]=dry;}else value=dry;
            }
            const auto latency=renderLatency.load();cursor_=latency?(cursor_+1)%latency:0;
        }
        data.outputs[0].silenceFlags = 0;
        if(editor&&data.processMode==kRealtime)for(int32 c=0;c<data.outputs[0].numChannels;++c){
            const auto level=auditionLevel.load();if(level!=0)data.outputs[0].silenceFlags=0;
            for(int32 f=0;f<data.numSamples;++f)data.outputs[0].channelBuffers32[c][f]+=level;
        }
        return kResultOk;
    }
    static FUnknown* create(void*) { return static_cast<IAudioProcessor*>(new DelayedGain); }
};
}
extern "C" __declspec(dllexport) bool MRSAraReadTransport(std::int64_t* sample,std::uint32_t* flags,double* ppqValue,double* tempo,std::int32_t* meter,double* cycleStart,double* cycleEnd,std::uint64_t* calls){std::lock_guard lock(transportMutex);*sample=lastTransport.projectTimeSamples;*flags=lastTransport.state;*ppqValue=lastTransport.projectTimeMusic;*tempo=lastTransport.tempo;*meter=lastTransport.timeSigNumerator;*cycleStart=lastTransport.cycleStartMusic;*cycleEnd=lastTransport.cycleEndMusic;*calls=transportCalls;return transportCalls!=0;}
extern "C" __declspec(dllexport) void MRSAraRequestTransport(int action,double position,double duration,bool enabled){requestTransport(action,position,duration,enabled);}
extern "C" __declspec(dllexport) void MRSAraNotifyDocument(){if(fixtureDocument)fixtureDocument->notifyDocumentDataChanged();}
extern "C" __declspec(dllexport) void MRSAraLiveFault(bool failed){liveFault=failed;}
extern "C" __declspec(dllexport) void MRSAraAudition(float level){auditionLevel.store(level);}
extern "C" __declspec(dllexport) void MRSAraRenderRequest(bool enabled){requestFromRender.store(enabled);}
extern "C" __declspec(dllexport) bool MRSAraObjectSelection(){
    if(!fixtureDocument||fixtureDocument->getEditorViews().empty())return false;
    const auto& selection=fixtureDocument->getEditorViews().front()->getViewSelection();
    return selection.getPlaybackRegions().size()==1&&selection.getRegionSequences().size()==1&&!selection.getTimeRange();
}
extern "C" __declspec(dllexport) void MRSAraLatencyChanged(){for(auto* controller:fixtureControllers)controller->latencyChanged();}
extern "C" __declspec(dllexport) bool MRSAraRenderSettings(std::uint32_t latency,std::uint32_t tail){if(latency>63)return false;renderLatency.store(latency);renderTail.store(tail);return true;}
extern "C" __declspec(dllexport) void MRSAraNotifySignal(){if(fixtureDocument)for(auto* source:fixtureDocument->getDocument()->getAudioSources<ARATestAudioSource>())for(auto* modification:source->getAudioModifications()){fixtureDocument->notifyAudioModificationContentChanged(modification,ARA::ContentUpdateScopes::samplesAreAffected());return;}}
extern "C" __declspec(dllexport) bool MRSAraPatch(const char* persistent,double first,double last,float gain,int scope){
    if(!persistent)return false;
    if(scope<0){std::lock_guard lock(patchMutex);patchSource.clear();patchGain=1;publishPatch();return true;}
    if(!fixtureDocument)return false;
    for(auto* source:fixtureDocument->getDocument()->getAudioSources<ARATestAudioSource>())if(source->getPersistentID()==persistent){
        double dirtyFirst=first,dirtyLast=last;
        {std::lock_guard lock(patchMutex);if(patchSource==persistent&&patchGain!=1.f){dirtyFirst=std::min(dirtyFirst,patchFirst);dirtyLast=std::max(dirtyLast,patchLast);}patchSource=persistent;patchFirst=first;patchLast=last;patchGain=gain;publishPatch();}
        for(auto* modification:source->getAudioModifications()){
            const ARA::ARAContentTimeRange range{dirtyFirst,dirtyLast-dirtyFirst};
            if(scope==1)for(auto* region:modification->getPlaybackRegions()){auto playbackRange=range;playbackRange.start+=region->getStartInPlaybackTime()-region->getStartInAudioModificationTime();fixtureDocument->getHostModelUpdateController()->notifyPlaybackRegionContentChanged(region->getHostRef(),&playbackRange,ARA::ContentUpdateScopes::samplesAreAffected());}
            else fixtureDocument->getHostModelUpdateController()->notifyAudioModificationContentChanged(modification->getHostRef(),scope==2?nullptr:&range,ARA::ContentUpdateScopes::samplesAreAffected());
        }return true;
    }return false;
}
// Control-thread fixture hooks: exercise real SDK note archives with distinct
// source content, independently from companion gain/parameter persistence.
extern "C" __declspec(dllexport) bool MRSAraSourceNote(const char* persistent,float* frequency,bool set){
    if(!fixtureDocument||!persistent||!frequency)return false;
    for(auto* source:fixtureDocument->getDocument()->getAudioSources<ARATestAudioSource>())if(source->getPersistentID()==persistent){
        if(set){auto notes=std::make_unique<TestNoteContent>();notes->push_back({*frequency,.5f,0.,.05});source->setNoteContent(std::move(notes),ARA::kARAContentGradeApproved,false);fixtureDocument->notifyAudioSourceContentChanged(source,ARA::ContentUpdateScopes::notesAreAffected());}
        const auto* notes=source->getNoteContent();if(!notes||notes->empty())return false;*frequency=notes->front()._frequency;return true;
    }return false;
}
bool InitModule() { return true; }
bool DeinitModule() { return true; }
DEF_CLASS_IID(ARA::IMainFactory)
DEF_CLASS_IID(ARA::IPlugInEntryPoint)
DEF_CLASS_IID(ARA::IPlugInEntryPoint2)
BEGIN_FACTORY_DEF(TEST_MANUFACTURER_NAME,TEST_INFORMATION_URL,TEST_MAILTO_URL)
DEF_CLASS2(INLINE_UID_FROM_FUID(TestVST3Processor::getClassFUID()),PClassInfo::kManyInstances,
    kVstAudioEffectClass,TEST_PLUGIN_NAME,0,"Fx","1.0",kVstVersionString,DelayedGain::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(TestVST3Processor::getEditControllerClassFUID()),PClassInfo::kManyInstances,
    kVstComponentControllerClass,TEST_PLUGIN_NAME,0,"","1.0",kVstVersionString,FixtureController::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(ARATestMainFactory::getClassFUID()),PClassInfo::kManyInstances,
    kARAMainFactoryClass,TEST_PLUGIN_NAME,0,"","1.0",kVstVersionString,ARATestMainFactory::createInstance)
END_FACTORY
