#include "mr_timestretch_drums/stretch.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace mr::timestretch::drums;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
int main() {
try {
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
