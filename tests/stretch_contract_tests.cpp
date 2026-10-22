#include <mrs/stretch.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace mrs::stretch;
void check(bool ok,const char* what) { if (!ok) throw std::runtime_error(what); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
// Deliberately links mrs_core only: no Signalsmith or adapter headers/library.
class Reference final : public ITimeStretchEngine {
public:
    Capabilities capabilities() const noexcept override { return {1,false,true,false,false}; }
    Status prepare(const Config&,const Parameters& p) override { p_=p; return validate(p); }
    Status reset(std::uint64_t g) override { g_=g; return Status::ok; }
    Status seek(std::span<const float* const>,std::uint32_t,std::uint64_t g) override { return reset(g); }
    Status set_parameters(const Parameters& p) noexcept override { p_=p; return validate(p); }
    Latency latency() const noexcept override { return {}; }
    std::uint64_t generation() const noexcept override { return g_; }
    std::uint32_t input_required(std::uint32_t n) const noexcept override { return n; }
    Result process(std::span<const float* const> in,std::uint32_t available,
                   std::span<float* const> out,std::uint32_t n) noexcept override {
        if (available < n) return {Status::need_input};
        if (in.size()!=1 || out.size()!=1) return {Status::invalid_buffer};
        for (std::uint32_t i=0;i<n;++i) out[0][i]=in[0][i];
        return {Status::ok,n,n};
    }
    Result flush(std::span<float* const>,std::uint32_t) override { return {Status::drained}; }
private: Parameters p_; std::uint64_t g_{};
};
void run() {
    Parameters p;
    p.duration_ratio=1.123456789; p.cents=13.25; p.formants=Formants::shift;
    p.formant_semitones=-3.5; p.formant_base_hz=177.7; p.quality=Quality::economical;
    CHECK(deserialize(serialize(p))==p);
    for (const auto text : {"MRS_STRETCH 2 1 1 0 0 0 0 0 0","MRS_STRETCH 1 2 1 0 0 0 0 0 0",
                           "MRS_STRETCH 1 1 1 0 0 7 0 0 0","MRS_STRETCH 1 1 1 0 0 0 0 0 0 extra"}) {
        bool rejected=false;
        try { (void)deserialize(text); } catch (const std::invalid_argument&) { rejected=true; }
        CHECK(rejected);
    }
    for (double r : {0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        auto bad=p;bad.duration_ratio=r;CHECK(validate(bad)==Status::invalid_parameter);
    }
    std::int64_t n{};
    CHECK(output_frames(3,0.5,n)==Status::ok && n==2);
    CHECK(output_frames(-1,1,n)==Status::invalid_parameter);
    CHECK(output_frames(INT64_MAX,2,n)==Status::invalid_parameter);
    CHECK(output_frames(1000000000,1.25,n)==Status::ok && n==1250000000);
    // Shared rounded endpoints preserve total extent across a split.
    std::int64_t left{},whole{};
    CHECK(output_frames(12345,1.1,left)==Status::ok);
    CHECK(output_frames(99999,1.1,whole)==Status::ok);
    CHECK(left+(whole-left)==whole);
    for (double r : {0.125,0.5,0.75,0.9,1.,1.1,1.25,1.5,2.,8.}) {
        FrameDemand mapper;
        std::uint64_t consumed{},produced{};
        for (unsigned i=0;i<100000;++i) {
            const auto block=1+(i*17)%509;
            consumed+=mapper.peek(block,r);mapper.advance(block,r);produced+=block;
        }
        const auto target=static_cast<long double>(produced)/r;
        CHECK(std::abs(static_cast<long double>(consumed)-target)<1.001L);
    }
    Reference reference; Config config; config.channels=1;
    CHECK(reference.prepare(config,{})==Status::ok);
    CHECK(reference.reset(42)==Status::ok && reference.generation()==42);
    std::array<float,3> input{1,2,3},output{};
    std::array<const float*,1> in{input.data()};std::array<float*,1> out{output.data()};
    CHECK(reference.process(in,2,out,3).status==Status::need_input);
    CHECK(reference.process(in,3,out,3).produced==3 && input==output);
}
}
int main() { try {run(); std::cout<<"stretch contract PASS\n";} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
