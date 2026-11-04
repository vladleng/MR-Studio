#include "sw_frontend.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <string>

using namespace mrs::separation::native;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f) {
    try { f(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Expected invalid geometry rejection");
}
int main() {
    try {
        require(swChunkStarts(0, 8, 4).empty(), "Empty schedule");
        require(swChunkStarts(5, 8, 4) == std::vector<std::size_t>{0}, "Short input");
        require(swChunkStarts(8, 8, 4) == std::vector<std::size_t>{0}, "Exact chunk must run once");
        require(swChunkStarts(11, 8, 4) == std::vector<std::size_t>({0,3}), "Tail must align to end");
        require(swChunkStarts(17, 8, 4) == std::vector<std::size_t>({0,4,8,9}), "Tail must not repeat");
        rejects([] { (void)swChunkStarts(8, 4, 5); });
        rejects([] { (void)swChunkStarts(8, 0, 1); });
        SwOverlapAdd accumulator(11, 2, 8);
        std::array<float,16> first{}, second{};
        first.fill(1); second.fill(3);
        accumulator.add(0,first.data(),8,8);
        accumulator.add(3,second.data(),8,8);
        const auto mixed=accumulator.finish();
        // Independent NumPy symmetric Hamming Float32 overlap/add golden values.
        const std::array<float,11> expected{1,1,1,1.1546721458f,1.4193212986f,2,2.5806787014f,2.8453278542f,3,3,3};
        for(std::size_t i=0;i<expected.size();++i)
            for(std::size_t ch=0;ch<2;++ch)
                require(std::abs(mixed[i*2+ch]-expected[i])<1e-6f,"Hamming overlap/add mismatch");
        rejects([&] { (void)accumulator.finish(); });
        SwOverlapAdd shortInput(7,1,8);
        shortInput.add(0,second.data(),7,4);
        const auto shortened=shortInput.finish();
        for(std::size_t i=0;i<7;++i)require(std::abs(shortened[i]-(i<4?3.0f:0.0f))<1e-6f,"ISTFT valid extent");
        SwOverlapAdd bad(8,1,8);
        rejects([&] { bad.add(1,first.data(),8,8); });
        first[0]=std::numeric_limits<float>::infinity();
        rejects([&] { bad.add(0,first.data(),8,8); });
        std::cout<<"PASS SW reference scheduling, Hamming OLA, short ISTFT and bounds\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
