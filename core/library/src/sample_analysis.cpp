#include <mrs/sample_preview.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>

namespace mrs::library {
namespace {
constexpr double pi=std::numbers::pi;
// Original non-RT radix-2 implementation; no third-party DSP dependency.
void fft(std::vector<std::complex<double>>& bins){
    const auto n=bins.size();
    for(std::size_t i=1,j=0;i<n;++i){auto bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(bins[i],bins[j]);}
    for(std::size_t length=2;length<=n;length*=2){
        const auto root=std::polar(1.,-2.*pi/static_cast<double>(length));
        for(std::size_t start=0;start<n;start+=length){std::complex<double> phase{1.,0.};
            for(std::size_t j=0;j<length/2;++j){const auto even=bins[start+j],odd=bins[start+j+length/2]*phase;
                bins[start+j]=even+odd;bins[start+j+length/2]=even-odd;phase*=root;}
        }
    }
}
void tempo(AudioFeatures& out,const std::vector<double>& envelope,double tick){
    if(envelope.empty()||out.peak<.00003)return;
    const auto highest=*std::max_element(envelope.begin(),envelope.end());
    std::vector<double> attacks;if(envelope.front()>highest*.12)attacks.push_back(0.);
    for(std::size_t i=1;i<envelope.size();++i){
        const auto first=i>5?i-5:0;double prior{};for(auto j=first;j<i;++j)prior+=envelope[j]/static_cast<double>(i-first);
        const auto time=static_cast<double>(i)*tick;
        if(envelope[i]>highest*.12&&envelope[i]>prior*1.45&&(attacks.empty()||time-attacks.back()>.09))attacks.push_back(time);
    }
    out.onset_density=static_cast<double>(attacks.size())/out.duration;
    if(attacks.size()<5||attacks.back()-attacks.front()<1.5)return;
    double best{},chosen{};
    // Fit onset times to a beat/subdivision grid. The preferred octave is a
    // convention (80–160 BPM), never a claim that audio resolves metrical intent.
    std::vector<double> intervals;for(std::size_t i=1;i<attacks.size();++i)intervals.push_back(attacks[i]-attacks[i-1]);
    std::sort(intervals.begin(),intervals.end());const auto median=intervals[intervals.size()/2];
    for(int index=0;index<=400;++index){
        const double bpm=80.+static_cast<double>(index)*.2;
        const auto step=60./bpm/2.;double x{},y{};
        for(const auto at:attacks){const auto angle=2.*pi*at/step;x+=std::cos(angle);y+=std::sin(angle);}
        const auto octave=std::log2(median/(60./bpm));
        const auto preference=1./(1.+2.*std::abs(octave-std::round(octave)));
        const auto fit=std::sqrt(x*x+y*y)/static_cast<double>(attacks.size())*(.85+.15*preference);
        const auto span=(attacks.back()-attacks.front())/(60./bpm);
        if(span<2.)continue;
        if(fit>best){best=fit;chosen=bpm;}
    }
    if(best<.83)return;
    out.bpm=std::round(chosen*10.)/10.;out.bpm_confidence=std::clamp(best*.9,0.,.9);
    for(double multiplier:{.5,1.,2.})if(out.bpm*multiplier>=40.&&out.bpm*multiplier<=300.)out.tempo_candidates.push_back(out.bpm*multiplier);
}
double correlation(const std::array<double,12>& values,const std::array<double,12>& pattern,int root){
    double aMean{},bMean{};for(auto x:values)aMean+=x/12.;for(auto x:pattern)bMean+=x/12.;
    double dot{},a{},b{};
    for(int i=0;i<12;++i){const auto x=values[static_cast<std::size_t>((i+root)%12)]-aMean,y=pattern[static_cast<std::size_t>(i)]-bMean;dot+=x*y;a+=x*x;b+=y*y;}
    return a>0&&b>0?dot/std::sqrt(a*b):0.;
}
}
void complete_audio_features(AudioFeatures& out,const std::vector<double>& envelope,double tick,const std::vector<float>& mono,double rate,const std::function<bool()>& cancelled){
    tempo(out,envelope,tick);
    if(out.type.label.empty()&&out.bpm_confidence>.8&&std::abs(out.duration*out.bpm/60.-std::round(out.duration*out.bpm/60.))<.08){
        out.type={"Loop","Consistent onset grid and beat-aligned duration",.86};out.reason=out.type.reason;
    }
    constexpr std::size_t size=4096;
    if(mono.size()<64||out.peak<.00003)return;
    std::vector<std::complex<double>> bins(size);
    std::array<double,12> chroma{};
    const auto windows=mono.size()>size?std::min<std::size_t>(64,1+(mono.size()-size)/(size/2)):1;
    for(std::size_t frame=0;frame<windows;++frame){
        if(cancelled&&cancelled())throw std::runtime_error("Audio feature analysis cancelled");
        const auto start=windows>1?frame*(mono.size()-size)/(windows-1):0;
        const auto span=std::min(size,mono.size()-start);
        for(std::size_t i=0;i<size;++i)bins[i]=i<span?static_cast<double>(mono[start+i])*(.5-.5*std::cos(2.*pi*static_cast<double>(i)/static_cast<double>(span-1))):0.;
        fft(bins);double power{},weighted{},logs{};std::size_t count{};
        for(std::size_t i=1;i<size/2-1;++i){
            const double frequency=static_cast<double>(i)*rate/static_cast<double>(size),value=std::norm(bins[i]);
            if(frequency<65.||frequency>3500.)continue;
            power+=value;weighted+=value*frequency;logs+=std::log(std::max(value,1.e-15));++count;
            if(value>std::norm(bins[i-1])&&value>=std::norm(bins[i+1])){
                const auto midi=static_cast<int>(std::lround(69.+12.*std::log2(frequency/440.)));
                if(midi>=36&&midi<=108)chroma[static_cast<std::size_t>(midi%12)]+=std::sqrt(value);
            }
        }
        if(power>0&&count){out.centroid+=weighted/power/static_cast<double>(windows);out.flatness+=std::exp(logs/static_cast<double>(count))/(power/static_cast<double>(count))/static_cast<double>(windows);}
    }
    // Published Krumhansl-Kessler profile coefficients; implementation is original.
    // Reject percussion/noise, isolated pitch/chord, and close relative-key ties.
    if(out.duration<3.||out.flatness>.12||out.type.label!="Loop"){out.reason+=" | Key: needs a sufficiently long tonal loop";return;}
    const auto peak=*std::max_element(chroma.begin(),chroma.end());
    if(std::count_if(chroma.begin(),chroma.end(),[&](double x){return x>peak*.1;})<4){out.reason+=" | Key: insufficient pitch-class diversity";return;}
    constexpr std::array<double,12> major={6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88};
    constexpr std::array<double,12> minor={6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17};
    double first=-1.,second=-1.;int root{};bool isMinor{};
    for(bool mode:{false,true})for(int key=0;key<12;++key){const auto score=correlation(chroma,mode?minor:major,key);
        if(score>first){second=first;first=score;root=key;isMinor=mode;}else second=std::max(second,score);}
    if(first<.65||first-second<.05){out.reason+=" | Key: uncertain profile or relative-key tie";return;}
    constexpr std::array<const char*,12> names={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    out.key=std::string(names[static_cast<std::size_t>(root)])+(isMinor?"m":" major");out.key_confidence=std::clamp(first*(.5+first-second),0.,.95);
}
} // namespace mrs::library
