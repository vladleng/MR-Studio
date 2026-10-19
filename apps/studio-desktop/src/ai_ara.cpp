#include <mrs/desktop.hpp>
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace mrs::desktop {
#ifdef MRS_HAS_ARA
namespace {bool module_exists(const std::string& module){std::error_code ec;return std::filesystem::exists(std::filesystem::path(std::u8string(module.begin(),module.end())),ec)&&!ec;}}
#endif
void Application::set_ai_selection(ai::Selection selection){require_ai_owner();ai_selection_=std::move(selection);}
void Application::set_ai_ara_editor_handler(std::function<void(Id)> handler,std::function<void(Id)> close){require_ai_owner();ai_ara_open_=std::move(handler);ai_ara_close_=std::move(close);}
void Application::set_ai_ara_catalog(std::vector<AraToolPlugin> plugins){
    require_ai_owner();if(plugins.size()>1024)throw std::invalid_argument("ARA tool catalog exceeds budget");
    std::set<std::pair<std::string,std::string>> unique;std::vector<AraToolEntry> next;
    for(auto& p:plugins){if(p.module.empty()||p.module.size()>32768||p.class_id.empty()||p.class_id.size()>128||p.name.empty()||p.name.size()>4096||p.version.size()>4096||!unique.emplace(p.module,p.class_id).second)throw std::invalid_argument("Invalid ARA tool catalog");
        const auto old=std::find_if(ai_ara_catalog_.begin(),ai_ara_catalog_.end(),[&](const auto& e){return e.plugin.module==p.module&&e.plugin.class_id==p.class_id;});
        next.push_back({old==ai_ara_catalog_.end()?new_id():old->id,std::move(p)});
    }
    if(next.size()==ai_ara_catalog_.size()&&std::equal(next.begin(),next.end(),ai_ara_catalog_.begin(),[](const auto& a,const auto& b){return a.id==b.id&&a.plugin==b.plugin;}))return;
    if(ai_permission_revision_->load()==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("AI authority overflow");
    ai_ara_catalog_=std::move(next);++*ai_permission_revision_;if(ai_worker_)ai_worker_->cancel_all();
    ai_receipts_.clear();ai_operations_.clear();for(auto& p:ai_previews_)if(p->state_==ai::PreviewState::preview)p->state_=ai::PreviewState::stale;
}
ai::AraReadState Application::ai_ara_read_state() const {
    require_ai_owner();ai::AraReadState result;
#ifdef MRS_HAS_ARA
    result.enabled=true;
    for(const auto& e:ai_ara_catalog_){const bool available=module_exists(e.plugin.module);result.capabilities.push_back({e.id,e.plugin.name,e.plugin.version,available});}
    const auto p=services_.projects->state().project;
    for(const auto& clip:p->clips)if(!clip.midi){ai::AraClipState c;c.id=clip.id;c.track=clip.track;
        const auto binding=std::find_if(p->ara_bindings.begin(),p->ara_bindings.end(),[&](const auto& b){return b.source==clip.source;});c.bound=binding!=p->ara_bindings.end();c.bypass=c.bound&&binding->bypass;
        const auto raw=ara_status(clip.source);c.status="unavailable";for(const auto* state:{"unbound","ready","analysing","disabled","pending","unavailable","failed"})if(raw.starts_with(state)){c.status=state;break;}
        if(c.bound){const auto e=std::find_if(ai_ara_catalog_.begin(),ai_ara_catalog_.end(),[&](const auto& v){return v.plugin.module==binding->module&&v.plugin.class_id==binding->class_id;});if(e!=ai_ara_catalog_.end())c.plugin=e->id;}
        c.can_open=c.bound&&static_cast<bool>(ai_ara_open_)&&c.status!="failed"&&c.status!="unavailable";c.can_remove=c.bound;c.can_attach=!c.bound&&std::any_of(result.capabilities.begin(),result.capabilities.end(),[](const auto& v){return v.available;});result.clips.push_back(std::move(c));
    }
#endif
    return result;
}
ai::Error Application::ai_ara_validate(const ai::ToolCall& call) const {
    require_ai_owner();
#ifndef MRS_HAS_ARA
    (void)call;return ai::Error::unsupported;
#else
    if(!ai::ara_action(call.tool)||!std::holds_alternative<ai::AraArgs>(call.arguments))return ai::Error::invalid_input;
    if(ai_selection_){const auto receipt=ai_receipts_.find(call.request_id);if(receipt==ai_receipts_.end()||receipt->second.selection!=*ai_selection_)return ai::Error::stale_context;}
    const auto& a=std::get<ai::AraArgs>(call.arguments);const auto p=services_.projects->state().project;
    const auto clip=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==a.clip;});if(clip==p->clips.end())return ai::Error::not_found;if(clip->midi||call.tool==ai::ToolKind::reanalyse_ara)return ai::Error::unsupported;
    const auto binding=std::find_if(p->ara_bindings.begin(),p->ara_bindings.end(),[&](const auto& b){return b.source==clip->source;});
    if(call.tool==ai::ToolKind::attach_ara){if(binding!=p->ara_bindings.end()||!a.plugin)return ai::Error::invalid_input;
        const auto e=std::find_if(ai_ara_catalog_.begin(),ai_ara_catalog_.end(),[&](const auto& v){return v.id==*a.plugin;});if(e==ai_ara_catalog_.end())return ai::Error::not_found;
        if(!module_exists(e->plugin.module))return ai::Error::not_found;
    }else {if(binding==p->ara_bindings.end())return ai::Error::not_found;
        const auto status=ara_status(clip->source);if(call.tool!=ai::ToolKind::remove_ara&&(status.starts_with("failed")||status.starts_with("unavailable")||status=="unbound"))return ai::Error::unsupported;
        if(call.tool==ai::ToolKind::open_ara_editor&&!ai_ara_open_)return ai::Error::unsupported;
        if(call.tool==ai::ToolKind::set_ara_bypass&&binding->bypass==a.bypass)return ai::Error::invalid_input;
    }return ai::Error::none;
#endif
}
ai::PreviewResult Application::ai_ara_preview(const std::vector<std::shared_ptr<const ai::ToolPlan>>& plans){
    // Runtime/plugin transactions are deliberately one action per review. They
    // must not be mixed with model-only commands or partially commit a group.
    if(plans.size()!=1||!plans.front()||!ai::ara_action(plans.front()->call.tool))return {ai::Error::unsupported,{}};
    const auto& plan=plans.front();const auto error=ai_ara_validate(plan->call);if(error!=ai::Error::none)return {error,{}};
    const auto& args=std::get<ai::AraArgs>(plan->call.arguments);const auto& clips=services_.projects->state().project->clips;const auto clip=std::find_if(clips.begin(),clips.end(),[&](const auto& c){return c.id==args.clip;});
    const auto tool=plan->call.tool;std::string before,after,scope;
    if(tool==ai::ToolKind::open_ara_editor){before="Editor closed or on another clip";after="Select clip and open/raise its ARA editor";scope="Transient editor action; project unchanged; no host Undo";}
    else {scope="All "+std::to_string(plan->affected)+" clips using this audio source; host Undo available";
        if(tool==ai::ToolKind::attach_ara){const auto e=std::find_if(ai_ara_catalog_.begin(),ai_ara_catalog_.end(),[&](const auto& v){return v.id==*args.plugin;});before="No ARA binding";after="Attach "+e->plugin.name;}
        else if(tool==ai::ToolKind::set_ara_bypass){before=args.bypass?"ARA active":"ARA bypassed";after=args.bypass?"ARA bypassed":"ARA active";}
        else {before="ARA binding and saved edits";after="Remove ARA; original audio retained";}
    }
    auto preview=std::shared_ptr<ai::Preview>(new ai::Preview(ai_key(),ai_permission_revision_->load(),{{args.clip,std::string(ai::tool_descriptor(tool)->name),std::move(before),std::move(after),clip->name+"; "+scope}},plans,{}));ai_previews_.push_back(preview);return {ai::Error::none,std::move(preview)};
}
void Application::ai_ara_execute(const ai::ToolCall& call){
    require_ai_owner();
#ifdef MRS_HAS_ARA
    const auto& a=std::get<ai::AraArgs>(call.arguments);const auto p=services_.projects->state().project;const auto clip=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==a.clip;});
    if(clip==p->clips.end())throw std::runtime_error("ARA target changed");
    if(call.tool==ai::ToolKind::open_ara_editor){ai_ara_open_(a.clip);return;}
    if(call.tool==ai::ToolKind::set_ara_bypass){bypass_ara_source(clip->source,a.bypass);return;}
    if(call.tool==ai::ToolKind::remove_ara){if(ai_ara_close_)ai_ara_close_(a.clip);detach_ara_source(clip->source);return;}
    if(call.tool==ai::ToolKind::attach_ara){const auto e=std::find_if(ai_ara_catalog_.begin(),ai_ara_catalog_.end(),[&](const auto& v){return v.id==*a.plugin;});if(e==ai_ara_catalog_.end())throw std::runtime_error("ARA plugin changed");attach_ara_source(clip->source,e->plugin.module,e->plugin.class_id);return;}
#else
    (void)call;
#endif
    throw std::runtime_error("Unsupported ARA action");
}
} // namespace mrs::desktop
