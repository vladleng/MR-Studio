#include "mr_timestretch_drums/stretch.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

namespace mr::timestretch::drums {
namespace {
constexpr std::size_t max_frames = 100000000;
std::size_t mapped(std::size_t n, double ratio) {
    return static_cast<std::size_t>(std::floor(static_cast<double>(n)*ratio+0.5));
}
float envelope(std::span<const float> x, std::size_t frame, std::size_t channels) {
    float peak=0;
    for(std::size_t c=0;c<channels;++c) peak=std::max(peak,std::abs(x[frame*channels+c]));
    return peak;
}
// Time-domain similarity search; do not sum channels before correlation, which
// would cancel anti-phase stereo. A single offset is used for the entire layout.
bool segment(std::span<const float> x, std::span<float> y, std::size_t channels,
             std::size_t grain, std::size_t search, std::size_t attack,
             std::size_t edge, std::stop_token stop) {
    const auto n=x.size()/channels, m=y.size()/channels;
    if(!m || !n) return true;
    if(n==m) { std::copy(x.begin(),x.end(),y.begin()); return !stop.stop_requested(); }
    grain=std::min({grain,n,m});
    if(grain<4) { // Degenerate fragment: sample hold, not a tonal-quality path.
        for(std::size_t f=0;f<m;++f)
            for(std::size_t c=0;c<channels;++c)
                y[f*channels+c]=x[std::min(n-1,static_cast<std::size_t>(
                    static_cast<double>(f)*static_cast<double>(n)/static_cast<double>(m)))*channels+c];
        return !stop.stop_requested();
    }
    const auto hop=grain/2, overlap=grain-hop;
    std::copy_n(x.begin(),grain*channels,y.begin());
    for(std::size_t out=hop;out<m;out+=hop) {
        if(stop.stop_requested()) return false;
        const auto length=std::min(grain,m-out);
        const auto nominal=std::min(n-length,mapped(out,static_cast<double>(n)/static_cast<double>(m)));
        const auto lo=nominal>search?nominal-search:0;
        const auto hi=std::min(n-length,nominal+search);
        const auto compare=std::min(overlap,length);
        std::size_t best=nominal;
        double best_score=-1e100;
        const auto score=[&](std::size_t candidate) {
            double xy=0,xx=0,yy=0;
            // Decimation bounds the search cost; final offset refinement is 1 sample.
            for(std::size_t j=0;j<compare;j+=4) for(std::size_t c=0;c<channels;++c) {
                const double a=y[(out+j)*channels+c], b=x[(candidate+j)*channels+c];
                xy+=a*b; xx+=a*a; yy+=b*b;
            }
            const double displacement=std::abs(static_cast<double>(candidate)-static_cast<double>(nominal));
            const double s=xy/std::sqrt(std::max(1e-30,xx*yy))-0.02*displacement/static_cast<double>(search+1);
            if(s>best_score) { best_score=s; best=candidate; }
        };
        score(nominal);
        for(auto p=lo;p<=hi;p+=4) score(p);
        const auto coarse=best;
        for(auto p=coarse>3?std::max(lo,coarse-3):lo;p<=std::min(hi,coarse+3);++p) score(p);
        for(std::size_t j=0;j<length;++j) {
            const float mix=j<compare?static_cast<float>(j)/static_cast<float>(compare):1.0f;
            for(std::size_t c=0;c<channels;++c) {
                auto& dest=y[(out+j)*channels+c];
                dest=dest*(1-mix)+x[(best+j)*channels+c]*mix;
            }
        }
    }
    // Preserve the attack at its explicitly mapped position, then blend into
    // WSOLA. Budget at most half the shorter interval, avoiding overlapping guards.
    const auto guard=std::min({attack,n/2,m/2});
    const auto fade=std::min({edge,n-guard,m-guard});
    for(std::size_t f=0;f<guard+fade;++f) {
        const float mix=f<guard?0.0f:static_cast<float>(f-guard)/static_cast<float>(std::max<std::size_t>(1,fade));
        for(std::size_t c=0;c<channels;++c)
            y[f*channels+c]=x[f*channels+c]*(1-mix)+y[f*channels+c]*mix;
    }
    // Match the pre-onset context at the next boundary, without extending duration.
    const auto tail=std::min({edge,n/4,m/4});
    for(std::size_t f=0;f<tail;++f) {
        const float mix=static_cast<float>(f+1)/static_cast<float>(tail);
        for(std::size_t c=0;c<channels;++c)
            y[(m-tail+f)*channels+c]=y[(m-tail+f)*channels+c]*(1-mix)+x[(n-tail+f)*channels+c]*mix;
    }
    return true;
}
}
Result render(std::span<const float> source,const Config& cfg,
              std::span<const std::size_t> markers,std::stop_token stop) {
    const auto fail=[](Status s) { Result r; r.status=s; return r; };
    if(!cfg.channels || cfg.channels>8 || cfg.sample_rate<8000 || cfg.sample_rate>192000 ||
       source.size()%cfg.channels || !std::isfinite(cfg.duration_ratio) ||
       cfg.duration_ratio<0.25 || cfg.duration_ratio>4 ||
       !std::isfinite(cfg.grain_ms) || cfg.grain_ms<8 || cfg.grain_ms>80 ||
       !std::isfinite(cfg.search_ms) || cfg.search_ms<0 || cfg.search_ms>12 ||
       !std::isfinite(cfg.attack_ms) || cfg.attack_ms<0 || cfg.attack_ms>20)
        return fail(Status::invalid_argument);
    const auto n=source.size()/cfg.channels;
    if(n>max_frames) return fail(Status::resource_limit);
    if(markers.size()>n) return fail(Status::invalid_argument);
    const auto m=mapped(n,cfg.duration_ratio);
    if(m>max_frames || m>std::numeric_limits<std::size_t>::max()/cfg.channels/sizeof(float))
        return fail(Status::resource_limit);
    const auto pcm_bytes=m*cfg.channels*sizeof(float);
    // Fixed-capacity markers: ceil(n / refractory) plus endpoints, or explicit count.
    const auto refractory=std::max<std::size_t>(1,cfg.sample_rate/100); // 10 ms
    const auto capacity=markers.empty() ? n/refractory+3 : markers.size()+2;
    if(capacity>std::numeric_limits<std::size_t>::max()/sizeof(std::size_t) ||
       pcm_bytes>cfg.memory_budget_bytes ||
       capacity*sizeof(std::size_t)>cfg.memory_budget_bytes-pcm_bytes)
        return fail(Status::resource_limit);
    std::size_t previous=0;
    for(const auto marker:markers) {
        if(marker==0 || marker>=n || marker<=previous) return fail(Status::invalid_argument);
        previous=marker;
    }
    float peak=0;
    for(std::size_t i=0;i<source.size();++i) {
        if((i&4095u)==0 && stop.stop_requested()) return fail(Status::cancelled);
        if(!std::isfinite(source[i])) return fail(Status::invalid_argument);
        peak=std::max(peak,std::abs(source[i]));
    }
    if(stop.stop_requested()) return fail(Status::cancelled);
    try {
        Result result;
        result.transients.reserve(capacity);
        result.transients.push_back(0);
        if(!markers.empty()) result.transients.insert(result.transients.end(),markers.begin(),markers.end());
        else if(cfg.detect_transients && peak>0 && cfg.duration_ratio!=1) {
            // Envelope novelty detector. It is deliberately not advertised as
            // beat tracking or spectral transient detection.
            double slow=0;
            const double decay=std::exp(-1.0/(0.020*cfg.sample_rate));
            bool armed=true;
            std::size_t last=0;
            for(std::size_t f=0;f<n;++f) {
                if((f&4095u)==0 && stop.stop_requested()) return fail(Status::cancelled);
                const double a=envelope(source,f,cfg.channels);
                const bool rising=a>std::max(static_cast<double>(peak)*0.015,slow*4);
                if(rising && armed && f-last>=refractory) {
                    result.transients.push_back(f); last=f; armed=false;
                }
                if(!rising) armed=true;
                slow=decay*slow+(1-decay)*a;
            }
        }
        result.transients.push_back(n);
        result.samples.resize(m*cfg.channels);
        result.frames=m;
        if(cfg.duration_ratio==1) {
            std::copy(source.begin(),source.end(),result.samples.begin());
        } else {
            const auto ms=[&](double value) {return static_cast<std::size_t>(value*cfg.sample_rate/1000.0);};
            for(std::size_t i=1;i<result.transients.size();++i) {
                const auto a=result.transients[i-1], b=result.transients[i];
                const auto oa=mapped(a,cfg.duration_ratio), ob=mapped(b,cfg.duration_ratio);
                if(!segment(source.subspan(a*cfg.channels,(b-a)*cfg.channels),
                            std::span<float>(result.samples).subspan(oa*cfg.channels,(ob-oa)*cfg.channels),
                            cfg.channels,ms(cfg.grain_ms),ms(cfg.search_ms),ms(cfg.attack_ms),
                            std::max<std::size_t>(1,ms(1)),stop))
                    return fail(Status::cancelled);
            }
        }
        // Public markers omit synthetic interval endpoints.
        result.transients.erase(result.transients.begin());
        result.transients.pop_back();
        if(stop.stop_requested()) return fail(Status::cancelled);
        return result;
    } catch(const std::bad_alloc&) { return fail(Status::resource_limit); }
      catch(const std::length_error&) { return fail(Status::resource_limit); }
}
} // namespace mr::timestretch::drums
