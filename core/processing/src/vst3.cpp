#include <mrs/vst3.hpp>
#include <mrs/audio.hpp>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/gui/iplugview.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
namespace mrs::processing {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
HostApplication& host(){static auto p=owned(new HostApplication);return *p;}
void ok(tresult result,const char* operation){if(result!=kResultOk)throw std::runtime_error(std::string("VST3: ")+operation);}
class FixedQueue final : public IParamValueQueue {
public:
    ParamID id{};int32 count{};
    struct Point{int32 offset;ParamValue value;};std::array<Point,16> points{};
    tresult PLUGIN_API queryInterface(const TUID iid,void** out) override{if(FUnknownPrivate::iidEqual(iid,IParamValueQueue::iid)||FUnknownPrivate::iidEqual(iid,FUnknown::iid)){*out=this;return kResultOk;}*out=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef() override{return 1;}uint32 PLUGIN_API release() override{return 1;}
    ParamID PLUGIN_API getParameterId() override{return id;}
    int32 PLUGIN_API getPointCount() override{return count;}
    tresult PLUGIN_API getPoint(int32 i,int32& offset,ParamValue& value) override{if(i<0||i>=count)return kInvalidArgument;offset=points[i].offset;value=points[i].value;return kResultOk;}
    tresult PLUGIN_API addPoint(int32 offset,ParamValue value,int32& index) override{if(count>=16)return kOutOfMemory;index=count;points[count++]={offset,value};return kResultOk;}
};
class FixedChanges final : public IParameterChanges {
public:
    std::array<FixedQueue,256> queues{};int32 count{};
    void clear(){for(int32 i=0;i<count;++i)queues[i].count=0;count=0;}
    tresult PLUGIN_API queryInterface(const TUID iid,void** out) override{if(FUnknownPrivate::iidEqual(iid,IParameterChanges::iid)||FUnknownPrivate::iidEqual(iid,FUnknown::iid)){*out=this;return kResultOk;}*out=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef() override{return 1;}uint32 PLUGIN_API release() override{return 1;}
    int32 PLUGIN_API getParameterCount() override{return count;}
    IParamValueQueue* PLUGIN_API getParameterData(int32 i) override{return i>=0&&i<count?&queues[i]:nullptr;}
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID& id,int32& index) override{for(int32 i=0;i<count;++i)if(queues[i].id==id){index=i;return &queues[i];}if(count>=256)return nullptr;index=count;auto& q=queues[count++];q.id=id;q.count=0;return &q;}
    void add(ParamID id,float v,int32 offset){int32 index{};if(auto* q=addParameterData(id,index))q->addPoint(offset,v,index);}
};
bool safe_process(IAudioProcessor* processor,ProcessData* data) noexcept {
#ifdef _MSC_VER
    __try {return processor->process(*data)==kResultOk;} __except(EXCEPTION_EXECUTE_HANDLER){return false;}
#else
    return processor->process(*data)==kResultOk;
#endif
}
class Plugin final : public IProcessor, public IComponentHandler, public IPlugFrame {
    VST3::Hosting::Module::Ptr module_;
    std::unique_ptr<PlugProvider> provider_;
    IPtr<IComponent> component_;
    IPtr<IEditController> controller_;
    FUnknownPtr<IAudioProcessor> processor_;
    IPtr<IPlugView> view_;
    std::string uid_;
    ProcessConfig config_{};
    HostProcessData data_;
    Vst::ProcessContext context_{};
    FixedChanges input_,output_;
    std::vector<mrs::processing::ParameterInfo> infos_;
    std::array<std::atomic<float>,4096> values_{};
    audio::SpscQueue<ParameterChange,1024> editor_changes_;
    std::array<bool,4096> initial_pending_{};std::size_t initial_count_{};
    std::unordered_map<ParamID,std::size_t> parameter_index_;
    std::atomic<bool> edited_{},fault_{},refresh_values_{};
    HWND parent_{};
    uint32 latency_{};bool active_{},processing_{};
    std::optional<std::size_t> index(ParamID id) const noexcept{auto it=parameter_index_.find(id);return it==parameter_index_.end()?std::nullopt:std::optional<std::size_t>{it->second};}
public:
    explicit Plugin(const NodeState& node):uid_(node.plugin.class_id){
        std::string error;module_=VST3::Hosting::Module::create(node.processor_id,error);if(!module_)throw std::runtime_error("Cannot load VST3: "+error);
        auto factory=module_->getFactory();factory.setHostContext(&host());PluginContextFactory::instance().setPluginContext(&host());
        for(const auto& info:factory.classInfos())if(info.category()==kVstAudioEffectClass && info.ID().toString()==uid_)provider_=std::make_unique<PlugProvider>(factory,info,true);
        if(!provider_ || !provider_->initialize())throw std::runtime_error("VST3 component/controller initialization failed: "+uid_);
        component_=provider_->getComponentPtr();controller_=provider_->getControllerPtr();processor_=FUnknownPtr<IAudioProcessor>(component_);
        if(!processor_ || !controller_)throw std::runtime_error("VST3 requires audio processor and controller");
        controller_->setComponentHandler(this);
        auto n=controller_->getParameterCount();if(n<0||n>4096)throw std::runtime_error("VST3 parameter count "+std::to_string(n)+" exceeds 4096 parameter limit");
        for(int32 i=0;i<n;++i){Vst::ParameterInfo p{};if(controller_->getParameterInfo(i,p)!=kResultOk)continue;auto v=static_cast<float>(controller_->getParamNormalized(p.id));infos_.push_back({p.id,0,1,v,(p.flags&Vst::ParameterInfo::kCanAutomate)!=0});infos_.back().name=StringConvert::convert(p.title);infos_.back().hidden=(p.flags&Vst::ParameterInfo::kIsHidden)!=0;parameter_index_[p.id]=infos_.size()-1;values_[infos_.size()-1]=v;}
    }
    ~Plugin() override{close_editor();if(controller_)controller_->setComponentHandler(nullptr);if(processor_&&processing_)processor_->setProcessing(false);if(component_&&active_)component_->setActive(false);processor_=nullptr;controller_=nullptr;component_=nullptr;provider_.reset();}
    void prepare(ProcessConfig c) override{
        config_=c;const SpeakerArrangement arrangement=c.channels==1?SpeakerArr::kMono:SpeakerArr::kStereo;
        if(component_->getBusCount(kAudio,kInput)<1||component_->getBusCount(kAudio,kOutput)<1)throw std::runtime_error("VST3 slice supports mono/stereo audio effects, not instruments");
        // Negotiate every declared bus, including inactive sidechain/auxiliary buses.
        std::vector<SpeakerArrangement> inputs(component_->getBusCount(kAudio,kInput)),outputs(component_->getBusCount(kAudio,kOutput));
        for(auto dir:{kInput,kOutput}){auto& buses=dir==kInput?inputs:outputs;for(int32 i=0;i<static_cast<int32>(buses.size());++i){ok(processor_->getBusArrangement(dir,i,buses[i]),"cannot query audio bus layout");if(i==0)buses[i]=arrangement;}}
        ok(processor_->setBusArrangements(inputs.data(),static_cast<int32>(inputs.size()),outputs.data(),static_cast<int32>(outputs.size())),"effect must support selected mono/stereo layout");
        for(auto dir:{kInput,kOutput}){for(int32 i=0;i<component_->getBusCount(kAudio,dir);++i)ok(component_->activateBus(kAudio,dir,i,i==0),"audio bus activation failed");for(int32 i=0;i<component_->getBusCount(kEvent,dir);++i)component_->activateBus(kEvent,dir,i,false);}
        ProcessSetup setup{kRealtime,kSample32,static_cast<int32>(c.max_block),static_cast<double>(c.sample_rate)};ok(processor_->canProcessSampleSize(kSample32),"32-bit processing unsupported");ok(processor_->setupProcessing(setup),"setup processing failed");
        if(!data_.prepare(*component_,static_cast<int32>(c.max_block),kSample32))throw std::runtime_error("VST3 audio buffer preparation failed");
        const int32 channels=c.channels==1?1:2;if(data_.inputs[0].numChannels!=channels||data_.outputs[0].numChannels!=channels)throw std::runtime_error("VST3 bus layout mismatch");
        context_.sampleRate=c.sample_rate;data_.processContext=&context_;data_.processMode=kRealtime;data_.inputParameterChanges=&input_;data_.outputParameterChanges=&output_;
        // Restore into an activated component, as desktop hosts do. TH-U resets
        // DSP internals on activation even while getState/UI retains the preset.
        // Activating after setState therefore loses the audible state.
        ok(component_->setActive(true),"activation failed");active_=true;
        latency_=processor_->getLatencySamples();
    }
    void restore(const PluginState& state) override{
        ParameterChange stale;while(editor_changes_.pop(stale)){}initial_pending_.fill(false);initial_count_=0;
        edited_=false;refresh_values_=false;

        if(!state.component.empty()){MemoryStream s(const_cast<std::byte*>(state.component.data()),state.component.size());ok(component_->setState(&s),"restore component state failed");s.seek(0,IBStream::kIBSeekSet,nullptr);controller_->setComponentState(&s);}
        if(!state.controller.empty()){MemoryStream s(const_cast<std::byte*>(state.controller.data()),state.controller.size());ok(controller_->setState(&s),"restore controller state failed");}
        for(std::size_t i=0;i<infos_.size();++i){const auto v=static_cast<float>(controller_->getParamNormalized(infos_[i].id));values_[i]=v;infos_[i].initial=v;}
        latency_=processor_->getLatencySamples();
    }
    PluginState capture() const override{
        // VST3's zero-sample parameter flush, off the audio thread with callbacks
        // stopped. Do not render/advance audio merely to commit pending controls.
        auto& self=*const_cast<Plugin*>(this);
        for(int pass=0;pass<20;++pass){
            self.input_.clear();self.output_.clear();std::size_t emitted=0;
            for(std::size_t i=0;i<infos_.size()&&emitted<256;++i)if(self.initial_pending_[i]){self.input_.add(infos_[i].id,values_[i].load(),0);self.initial_pending_[i]=false;--self.initial_count_;++emitted;}
            ParameterChange change;while(emitted<256&&self.editor_changes_.pop(change)){self.input_.add(change.id,change.value,0);++emitted;}
            if(!emitted)break;
            self.data_.numSamples=0;if(!safe_process(processor_,&self.data_))throw std::runtime_error("VST3: pending parameter state flush failed");
        }
        PluginState state;state.class_id=uid_;
        const auto copy=[](MemoryStream& stream){if(stream.getSize()>1024*1024)throw std::runtime_error("VST3 state exceeds 1 MiB budget");std::vector<std::byte> v(static_cast<std::size_t>(stream.getSize()));if(!v.empty())std::memcpy(v.data(),stream.getData(),v.size());return v;};
        MemoryStream component,controller;ok(component_->getState(&component),"capture component state failed");state.component=copy(component);if(controller_->getState(&controller)==kResultOk)state.controller=copy(controller);return state;
    }
    std::vector<mrs::processing::ParameterInfo> parameters() const override{return infos_;}
    bool set_parameter(std::uint32_t id,float value) noexcept override{const auto i=index(id);if(!i||!std::isfinite(value)||value<0||value>1)return false;values_[*i]=value;if(!initial_pending_[*i]){initial_pending_[*i]=true;++initial_count_;}return true;}
    std::optional<float> parameter_value(std::uint32_t id) const noexcept override{auto i=index(id);return i?std::optional<float>{values_[*i].load()}:std::nullopt;}
    std::uint32_t latency() const noexcept override{return latency_;}bool live_safe() const noexcept override{return false;}
    void warm() override{for(std::size_t i=0;i<infos_.size();++i)if(initial_pending_[i])controller_->setParamNormalized(infos_[i].id,values_[i].load());if(!active_){ok(component_->setActive(true),"activation failed");active_=true;}ok(processor_->setProcessing(true),"start processing failed");processing_=true;latency_=processor_->getLatencySamples();}
    void reset() noexcept override{context_.projectTimeSamples=0;}
    bool edited() noexcept override{if(refresh_values_.exchange(false))for(std::size_t i=0;i<infos_.size();++i)values_[i]=static_cast<float>(controller_->getParamNormalized(infos_[i].id));return edited_.exchange(false);}
    void sync_controller(std::uint32_t id,float value) override{controller_->setParamNormalized(id,value);}
    bool failed() const noexcept override{return fault_.load();}
    bool open_editor(void* parent,int& width,int& height) override{
        close_editor();view_=owned(controller_->createView(ViewType::kEditor));if(!view_ || view_->isPlatformTypeSupported(kPlatformTypeHWND)!=kResultOk){view_=nullptr;return false;}
        parent_=static_cast<HWND>(parent);view_->setFrame(this);ViewRect rect{};ok(view_->getSize(&rect),"editor size unavailable");width=rect.getWidth();height=rect.getHeight();ok(view_->attached(parent,kPlatformTypeHWND),"editor attachment failed");return true;
    }
    void close_editor() noexcept override{if(view_){view_->removed();view_->setFrame(nullptr);view_=nullptr;}parent_=nullptr;}
    tresult PLUGIN_API queryInterface(const TUID iid,void** out) override{
        if(FUnknownPrivate::iidEqual(iid,IComponentHandler::iid)||FUnknownPrivate::iidEqual(iid,FUnknown::iid))*out=static_cast<IComponentHandler*>(this);
        else if(FUnknownPrivate::iidEqual(iid,IPlugFrame::iid))*out=static_cast<IPlugFrame*>(this);else{*out=nullptr;return kNoInterface;}return kResultOk;
    }
    uint32 PLUGIN_API addRef() override{return 1;}uint32 PLUGIN_API release() override{return 1;}
    tresult PLUGIN_API beginEdit(ParamID) override{return kResultOk;}
    tresult PLUGIN_API performEdit(ParamID id,ParamValue value) override{const auto i=index(id);if(!i||!std::isfinite(value)||value<0||value>1)return kInvalidArgument;values_[*i]=static_cast<float>(value);edited_=true;return editor_changes_.push({0,id,static_cast<float>(value)})?kResultOk:kOutOfMemory;}
    tresult PLUGIN_API endEdit(ParamID) override{edited_=true;return kResultOk;}
    tresult PLUGIN_API restartComponent(int32 flags) override{if(flags&(kIoChanged|kLatencyChanged|kReloadComponent))return kNotImplemented;if(flags&kParamValuesChanged)refresh_values_=true;edited_=true;return kResultOk;}
    tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* rect) override{if(!parent_||!rect||rect->getWidth()<1||rect->getHeight()<1||rect->getWidth()>4096||rect->getHeight()>4096)return kInvalidArgument;RECT r{0,0,rect->getWidth(),rect->getHeight()};AdjustWindowRect(&r,static_cast<DWORD>(GetWindowLongPtrW(parent_,GWL_STYLE)),FALSE);SetWindowPos(parent_,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);return view->onSize(rect);}
    void process(ProcessBlock block) noexcept override{
        if(fault_)return;
        input_.clear();output_.clear();if(initial_count_){std::size_t emitted=0;for(std::size_t i=0;i<infos_.size()&&emitted<256;++i)if(initial_pending_[i]){input_.add(infos_[i].id,values_[i].load(),0);initial_pending_[i]=false;--initial_count_;++emitted;}}
        for(const auto& p:block.parameters){if(auto i=index(p.id))values_[*i]=p.value;input_.add(p.id,p.value,static_cast<int32>(p.offset));}
        ParameterChange change;for(int i=0;i<256&&editor_changes_.pop(change);++i)input_.add(change.id,change.value,0);
        for(int32 b=0;b<data_.numInputs;++b){data_.inputs[b].silenceFlags=0;for(int32 c=0;c<data_.inputs[b].numChannels;++c){auto* out=data_.inputs[b].channelBuffers32[c];for(std::uint32_t f=0;f<block.frames;++f)out[f]=b==0?block.audio[static_cast<std::size_t>(f)*block.channels+c]:0;}}
        for(int32 b=0;b<data_.numOutputs;++b){data_.outputs[b].silenceFlags=0;for(int32 c=0;c<data_.outputs[b].numChannels;++c)std::fill_n(data_.outputs[b].channelBuffers32[c],block.frames,0.f);}
        data_.numSamples=static_cast<int32>(block.frames);
        context_.projectTimeSamples=block.position;context_.tempo=block.tempo;context_.projectTimeMusic=block.quarter;
        context_.state=(block.playing?Vst::ProcessContext::kPlaying:0)|Vst::ProcessContext::kTempoValid|Vst::ProcessContext::kProjectTimeMusicValid;
        if(!safe_process(processor_,&data_)){fault_=true;return;}
        for(std::uint32_t f=0;f<block.frames;++f)for(int32 c=0;c<data_.outputs[0].numChannels;++c){const auto v=(data_.outputs[0].silenceFlags&(uint64{1}<<c))?0.f:data_.outputs[0].channelBuffers32[c][f];block.audio[static_cast<std::size_t>(f)*block.channels+c]=std::isfinite(v)?v:0;}
        context_.projectTimeSamples+=block.frames;
    }
};
}
std::vector<VstPlugin> probe_vst3(const std::string& path){std::string error;auto m=VST3::Hosting::Module::create(path,error);if(!m)throw std::runtime_error(error);std::vector<VstPlugin> list;for(const auto& c:m->getFactory().classInfos())if(c.category()==kVstAudioEffectClass)list.push_back({path,c.ID().toString(),c.name(),c.vendor(),c.version()});return list;}
std::unique_ptr<IProcessor> vst3_factory(const NodeState& node){return std::make_unique<Plugin>(node);}
std::unique_ptr<IProcessor> hosted_factory(const NodeState& node){return node.format==ProcessorFormat::vst3?vst3_factory(node):native_factory(node);}
}
