#include <mrs/desktop.hpp>
#include <mrs/arrangement.hpp>
#include <algorithm>
#include <stdexcept>

namespace mrs::desktop {
void Application::poll_clip_updates(){
    std::erase_if(clip_updates_,[&](const auto& handle){
        const auto phase=engine_->transition_phase(handle);
        if(phase==audio::AudioEngine::TransitionPhase::adopted){engine_->acknowledge_transition(handle);return true;}
        return phase==audio::AudioEngine::TransitionPhase::cancelled||phase==audio::AudioEngine::TransitionPhase::acknowledged;
    });
}
std::vector<audio::Voice> Application::prepare_clip_voices(const Project& project){
    std::vector<audio::Voice> voices;voices.reserve(project.clips.size());
    for(const auto& clip:project.clips){
        if(clip.midi)continue;
        const auto original=asset(clip.source);
        if(clip.source_offset>original->frames()||audio_source_span(clip)>original->frames()-clip.source_offset)
            throw std::invalid_argument("Clip exceeds source audio");
        auto data=clip_audio(clip,original);if(!data)continue; // pending worker, never raw wrong-speed playback
        const auto track=std::find(mixer_tracks_.begin(),mixer_tracks_.end(),clip.track);
        if(track==mixer_tracks_.end())throw std::invalid_argument("Missing audio channel");
        audio::Voice voice{data,clip.start,clip_playback_offset(clip),clip.length,{}};
        voice.fade_in=clip.fade_in;voice.fade_out=clip.fade_out;voice.mixer_track=static_cast<std::size_t>(track-mixer_tracks_.begin());
        const auto gain=clip.source=="mrs:demo-tone"?.15f:1.f;
        const auto outputs=std::min<std::uint32_t>(2,static_cast<std::uint32_t>(device_config_->outputs.size()));
        if(data->channels==1){for(std::uint32_t out=0;out<outputs;++out)voice.routes.push_back({0,out,gain});}
        else for(std::uint32_t c=0;c<data->channels;++c)voice.routes.push_back({c,c%outputs,gain});
        voices.push_back(std::move(voice));
    }
    return voices;
}
bool Application::edit_clip_state(const Project& candidate,const ICommand* command,std::optional<bool> history){
    require_not_playing();require_not_recording();poll_clip_updates();
    const auto& before=*services_.projects->state().project;
    if(!before.ara_bindings.empty()||!candidate.ara_bindings.empty()||performance_plan_||recording())return false;
    candidate.validate();
    std::shared_ptr<audio::AudioEngine::PreparedTransition> plan;
    if(device_){
        // Withdraw only unconsumed plans. Acquired plans stay owned until their
        // callback acknowledgement; no destructor can run on that callback.
        for(const auto& pending:clip_updates_)engine_->cancel_transition(pending);
        poll_clip_updates();
        plan=engine_->prepare_voice_transition(prepare_clip_voices(candidate));
    }
    auto transaction=services_.projects->prepare_transaction(command,history);
    if(!transaction)return false;
    if(!device_){transaction->commit();transaction->finish();return true;}
    if(!device_config_->process_buffer_frames&&engine_->async_voice_transition_available()){
        clip_updates_.push_back(plan); // allocate before publication
        transaction->commit();
        if(!engine_->stage_transition(plan)){transaction->rollback();clip_updates_.pop_back();throw std::runtime_error("Clip publication busy; retry edit");}
        transaction->finish();return true;
    }
    // Process Buffer owns speculative copies. Quiesce that owner, but keep DSP,
    // PDC, prepared buffers and worker calibration instead of rebuilding them.
    bool swapped=false;
    try{device_->stop();engine_->quiesce();engine_->swap_voice_transition_stopped(plan);swapped=true;
        transaction->commit();device_->start();engine_->acknowledge_transition(plan);
    }catch(...){const auto error=std::current_exception();
        try{device_->stop();engine_->quiesce();if(swapped)engine_->rollback_voice_transition_stopped(plan);transaction->rollback();device_->start();}
        catch(...){transaction->rollback();disconnect();throw std::runtime_error("Clip edit rolled back; audio could not restart");}
        std::rethrow_exception(error);
    }
    transaction->finish();return true;
}
bool Application::refresh_clip_audio(const Project& project){
    if(!device_||!project.ara_bindings.empty()||performance_plan_)return false;
    require_not_playing();poll_clip_updates();
    for(const auto& pending:clip_updates_)engine_->cancel_transition(pending);
    poll_clip_updates();
    auto plan=engine_->prepare_voice_transition(prepare_clip_voices(project));
    if(!device_config_->process_buffer_frames&&engine_->async_voice_transition_available()){
        clip_updates_.push_back(plan);
        if(!engine_->stage_transition(plan)){clip_updates_.pop_back();throw std::runtime_error("Clip publication busy");}
    }else{
        bool swapped=false;
        try{device_->stop();engine_->quiesce();engine_->swap_voice_transition_stopped(plan);swapped=true;device_->start();engine_->acknowledge_transition(plan);}
        catch(...){const auto error=std::current_exception();try{device_->stop();engine_->quiesce();if(swapped)engine_->rollback_voice_transition_stopped(plan);device_->start();}catch(...){disconnect();}std::rethrow_exception(error);}
    }
    return true;
}
}
