#include <mrs/metronome.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mrs::audio {
void ClickSettings::validate() const {if(level<0||level>100||count_bars<0||count_bars>4)throw std::invalid_argument("invalid metronome settings");}
void Metronome::prepare(const TimeMap& map,std::uint32_t rate){
    map.validate();if(rate<8000||rate>768000)throw std::invalid_argument("invalid metronome rate");
    tempos_.clear();meters_.clear();double frame=0;Tick previous=0;double per=0;
    for(const auto& t:map.tempos){frame+=(t.tick-previous)*per;per=60.*rate/(t.bpm*ppq);tempos_.push_back({t.tick,frame,per});previous=t.tick;}
    Tick tick=0;std::int64_t bar=1;Tick bar_ticks=0;
    for(const auto& m:map.meters){tick+=(m.bar-bar)*bar_ticks;meters_.push_back({tick,m.numerator,4*ppq/m.denominator});bar=m.bar;bar_ticks=m.numerator*(4*ppq/m.denominator);}
    normal_.resize(rate/40);accent_.resize(normal_.size());
    for(std::size_t i=0;i<normal_.size();++i){const double t=static_cast<double>(i)/rate,envelope=std::min(1.,t/.0005)*std::pow(1.-static_cast<double>(i)/normal_.size(),3);normal_[i]=static_cast<float>(std::sin(t*1400*6.283185307179586)*envelope);accent_[i]=static_cast<float>(std::sin(t*2200*6.283185307179586)*envelope);}
    reset();
}
double Metronome::tick_at(double frame) const noexcept {const auto it=std::upper_bound(tempos_.begin(),tempos_.end(),std::max(0.,frame),[](double f,const Tempo& t){return f<t.frame;});const auto& t=*std::prev(it);return t.tick+(frame-t.frame)/t.per_tick;}
Sample Metronome::frame_at(Tick tick) const noexcept {const auto it=std::upper_bound(tempos_.begin(),tempos_.end(),tick,[](Tick n,const Tempo& t){return n<t.tick;});const auto& t=*std::prev(it);return static_cast<Sample>(std::min(static_cast<double>(max_sample),std::round(t.frame+(tick-t.tick)*t.per_tick)));}
void Metronome::schedule(Sample frame) noexcept {
    if(tick_at(static_cast<double>(frame)-.5)>max_tick){next_=max_sample;next_strong_=false;return;}
    const auto tick=std::clamp(tick_at(static_cast<double>(frame)-.5),0.,static_cast<double>(max_tick));
    const auto it=std::upper_bound(meters_.begin(),meters_.end(),tick,[](double t,const Meter& m){return t<m.tick;});const auto& m=*std::prev(it);
    next_tick_=m.tick+static_cast<Tick>(std::ceil((tick-m.tick)/m.step-1e-9))*m.step;
    if(it!=meters_.end())next_tick_=std::min(next_tick_,it->tick);
    next_tick_=std::min(max_tick,next_tick_);next_=frame_at(next_tick_);
    if(next_<frame&&next_tick_<max_tick){next_tick_=std::min(max_tick,next_tick_+m.step);if(it!=meters_.end())next_tick_=std::min(next_tick_,it->tick);next_=frame_at(next_tick_);}
    const auto meter=std::upper_bound(meters_.begin(),meters_.end(),next_tick_,[](Tick t,const Meter& v){return t<v.tick;});const auto& active=*std::prev(meter);next_strong_=(next_tick_-active.tick)%(active.step*active.beats)==0;
}
float Metronome::tone(bool enabled,bool accent,float gain) noexcept {if(!enabled){cursor_=normal_.size();return 0;}if(cursor_>=normal_.size())return 0;const auto i=cursor_++;return (accent&&strong_?accent_[i]:normal_[i])*(accent&&!strong_?.65f:1.f)*gain;}
float Metronome::sample(Sample frame,bool enabled,bool accent,float gain) noexcept {
    if(!enabled){reset();return 0;}
    if(tempos_.empty())return 0;
    if(frame!=last_+1){reset();schedule(frame);}last_=frame;
    if(frame>=next_){strong_=next_strong_;cursor_=0;if(frame<max_sample)schedule(frame+1);}
    return tone(enabled,accent,gain);
}
float Metronome::count_sample(Sample frame,double width,int beats,bool accent,float gain) noexcept {
    if(frame!=last_+1){reset();next_=static_cast<Sample>(std::round(std::ceil(frame/width-1e-9)*width));}last_=frame;
    if(frame>=next_){const auto beat=static_cast<std::int64_t>(std::llround(frame/width));strong_=beat%beats==0;cursor_=0;next_=static_cast<Sample>(std::round((beat+1)*width));}
    return tone(true,accent,gain);
}
}
