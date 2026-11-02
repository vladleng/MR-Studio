#include "mr_timestretch_drums/stretch.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace mr::timestretch::drums;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
// Measure the spacing between interpolated positive-going crossings in the
// interior of a steady bass body, excluding the protected attack and clip edges.
double body_frequency(const Result& y,const Config& c) {
    const auto first=static_cast<std::size_t>(.22*c.duration_ratio*c.sample_rate);
    const auto last=static_cast<std::size_t>(.55*c.duration_ratio*c.sample_rate);
    double begin=0,end=0;std::size_t count=0;
    for(auto i=first;i<last;++i){
        const auto a=y.samples[(i-1)*c.channels],b=y.samples[i*c.channels];
        if(a<=0&&b>0){const double at=static_cast<double>(i-1)-a/(b-a);if(!count)begin=at;end=at;++count;}
    }
    check(count>=4,"too few bass periods");
    return static_cast<double>(count-1)*c.sample_rate/(end-begin);
}
void bass_regression(){
    constexpr double tau=6.283185307179586;
    for(const auto rate:{44100u,48000u,96000u})for(const auto channels:{1u,2u})
    for(const double hz:{40.,55.,80.,100.}){
        std::vector<float> source(rate*channels,0);
        for(std::size_t i=rate/10;i<rate*8/10;++i){
            const auto sample=static_cast<float>(std::sin(tau*hz*static_cast<double>(i-rate/10)/rate));
            for(std::size_t ch=0;ch<channels;++ch)source[i*channels+ch]=ch?-sample:sample;
        }
        for(const double ratio:{.5,.75,.9,1.1,1.25,1.5,2.}){
            Config cfg;cfg.sample_rate=rate;cfg.channels=channels;cfg.duration_ratio=ratio;
            const auto output=render(source,cfg);check(output.status==Status::ok,"bass render failed");
            check(std::abs(body_frequency(output,cfg)/hz-1)<.01,"bass body tuning changed");
            if(channels==2)for(std::size_t i=0;i<output.frames;++i)
                check(output.samples[i*2]==-output.samples[i*2+1],"bass stereo phase changed");
        }
    }
    // A swept/decaying 55 Hz kick under synthetic hats and a snare. Search for
    // the dominant body peak, rather than counting high-frequency noise crossings.
    std::vector<float> mix(48000,0);double phase=0,previous=0;std::uint32_t rng=42;
    for(std::size_t i=0;i<mix.size();++i){
        if(i>=4800&&i<38400){const double t=static_cast<double>(i-4800)/48000;
            phase+=tau*(55+100*std::exp(-t/.025))/48000;mix[i]=static_cast<float>(std::sin(phase)*std::exp(-t/.16));}
        rng=rng*1664525u+1013904223u;const double noise=static_cast<double>(rng>>8)/16777216*2-1;
        mix[i]+=static_cast<float>(.14*(noise-previous)*std::exp(-static_cast<double>(i%6000)/180));previous=noise;
        if(i>=16800&&i<21000)mix[i]+=static_cast<float>(.18*noise*std::exp(-static_cast<double>(i-16800)/800));
    }
    for(const double ratio:{.5,.75,.9,1.1,1.25,1.5,2.}){
        Config cfg;cfg.channels=1;cfg.duration_ratio=ratio;const auto output=render(mix,cfg);
        check(output.status==Status::ok,"mixed kick render failed");
        const auto first=static_cast<std::size_t>(.22*ratio*48000),last=static_cast<std::size_t>(.55*ratio*48000);
        double best=0,peak=0;
        for(double hz=25;hz<=130;hz+=.25){double re=0,im=0;
            for(auto i=first;i<last;i+=16){const double window=.5-.5*std::cos(tau*static_cast<double>(i-first)/static_cast<double>(last-first-1));
                const double sample=output.samples[i]*window,angle=tau*hz*static_cast<double>(i-first)/48000;
                re+=sample*std::cos(angle);im+=sample*std::sin(angle);}
            const double power=re*re+im*im;if(power>best){best=power;peak=hz;}
        }
        check(std::abs(peak-55)<1,"mixed kick body tuning changed");
    }
}
int main() {
try {
    bass_regression();
    Config c; c.channels=2;
    std::vector<float> x(48000*2);
    for(std::size_t i=0;i<x.size()/2;++i) {
        x[2*i]=static_cast<float>(0.5*std::sin(6.283185307179586*440*static_cast<double>(i)/48000));
        x[2*i+1]=-x[2*i];
    }
    auto identity=render(x,c);
    check(identity.status==Status::ok && identity.samples==x,"neutral must be bit-identical");
    c.detect_transients=false;
    for(const double ratio:{0.25,0.5,0.75,0.9,1.0,1.1,1.25,1.5,2.0,4.0}) {
        c.duration_ratio=ratio;
        const auto y=render(x,c), again=render(x,c);
        check(y.status==Status::ok,"ratio render failed");
        check(y.frames==static_cast<std::size_t>(std::floor(48000*ratio+0.5)),"logical output extent");
        check(y.samples==again.samples,"same-build determinism");
        for(std::size_t i=0;i<y.frames;++i) {
            check(std::isfinite(y.samples[2*i]),"nonfinite output");
            check(y.samples[2*i]==-y.samples[2*i+1],"anti-phase stereo coherence");
            check(std::abs(y.samples[2*i])<=0.501f,"unexpected gain");
        }
        // Broad frequency sanity gate, NOT a perceptual quality claim.
        std::size_t crossings=0;
        for(std::size_t i=1;i<y.frames;++i)
            if(y.samples[2*(i-1)]<=0 && y.samples[2*i]>0) ++crossings;
        const double hz=static_cast<double>(crossings)*48000/static_cast<double>(y.frames);
        check(std::abs(hz-440)<25,"pitch changed like tape speed");
    }
    for(const auto rate:{44100u,48000u,96000u}) for(const auto channels:{1u,2u,8u}) {
        c={}; c.sample_rate=rate; c.channels=channels; c.duration_ratio=1.25;
        const std::size_t n=rate/2;
        const std::vector<std::size_t> marks{rate/10,rate/5,rate*3/10};
        std::vector<float> impulses(n*channels,0);
        for(const auto p:marks) for(std::size_t ch=0;ch<channels;++ch)
            impulses[p*channels+ch]=static_cast<float>(ch+1)/static_cast<float>(channels);
        for(const double ratio:{0.5,0.75,0.9,1.1,1.25,1.5,2.0}) {
            c.duration_ratio=ratio;
            const auto y=render(impulses,c,marks);
            check(y.status==Status::ok && y.transients==marks,"explicit markers");
            for(const auto p:marks) {
                const auto at=static_cast<std::size_t>(std::floor(static_cast<double>(p)*ratio+0.5));
                for(std::size_t ch=0;ch<channels;++ch)
                    check(y.samples[at*channels+ch]==impulses[p*channels+ch],"transient position/amplitude");
            }
        }
        const auto detected=render(impulses,c);
        check(detected.status==Status::ok && detected.transients==marks,"isolated impulse detector");
    }
    c={}; c.channels=1; c.duration_ratio=1.25;
    for(const std::size_t n:{0u,1u,2u,3u,7u,31u,1023u}) {
        std::vector<float> silent(n);
        const auto y=render(silent,c);
        check(y.status==Status::ok,"empty/short clip");
        check(y.frames==static_cast<std::size_t>(std::floor(static_cast<double>(n)*1.25+0.5)),"short length");
        check(std::all_of(y.samples.begin(),y.samples.end(),[](float v){return v==0;}),"silence changed");
    }
    c.channels=2;
    check(render(std::vector<float>{1},c).status==Status::invalid_argument,"partial frame accepted");
    c.channels=0; check(render(x,c).status==Status::invalid_argument,"zero channels");
    c={};
    for(const double ratio:{0.0,-1.0,5.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        c.duration_ratio=ratio; check(render(x,c).status==Status::invalid_argument,"invalid ratio accepted");
    }
    c={}; c.memory_budget_bytes=8;
    auto limited=render(x,c);
    check(limited.status==Status::resource_limit && limited.samples.empty(),"budget/atomic failure");
    c={};
    auto bad=x; bad[100]=std::numeric_limits<float>::quiet_NaN();
    check(render(bad,c).status==Status::invalid_argument,"NaN PCM accepted");
    const std::vector<std::size_t> invalid{100,99};
    check(render(x,c,invalid).status==Status::invalid_argument,"unsorted markers");
    const std::vector<std::size_t> duplicate{100,100};
    check(render(x,c,duplicate).status==Status::invalid_argument,"duplicate markers");
    std::stop_source stop; stop.request_stop();
    const auto cancelled=render(x,c,{},stop.get_token());
    check(cancelled.status==Status::cancelled && cancelled.samples.empty(),"cancelled output published");
    std::cout<<"PASS: duration, bypass, deterministic render, pitch sanity, linked channels, explicit/automatic impulses, silence, short clips, errors, budget, cancellation\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
