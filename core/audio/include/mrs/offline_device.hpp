#pragma once
#include <mrs/device.hpp>
namespace mrs::audio {
// Shared offline clock for development/headless use. Never opens a hardware output.
// Calls the SAME AudioEngine on a dedicated thread; UI stalls do not pause its clock.
std::unique_ptr<IAudioDevice> make_offline_device();
}
