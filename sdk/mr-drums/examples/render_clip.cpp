// Worker-thread example. No audio device, DAW state or GUI is created.
#include "mr_drums/stretch.hpp"
#include <iostream>
int main() {
    mr::drums::Config config;
    config.sample_rate=48000;
    config.channels=2;
    config.duration_ratio=105.0/120.0;
    std::vector<float> source(48000*2,0);
    for(std::size_t frame=0;frame<48000;frame+=12000)
        source[frame*2]=source[frame*2+1]=0.8f;
    auto rendered=mr::drums::render(source,config);
    if(rendered.status!=mr::drums::Status::ok) return 1;
    std::cout<<rendered.frames<<" frames, SDK "<<mr::drums::version<<'\n';
    return rendered.frames==42000?0:1;
}
