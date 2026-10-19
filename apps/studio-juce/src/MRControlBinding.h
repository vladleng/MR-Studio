#pragma once
#include "MRControls.h"
#include <mrs/desktop.hpp>
namespace ui::kit {
enum class ParameterDomain { nativeInsert, trackGain, trackPan, masterGain };
struct NativeParameterKey {
    std::optional<mrs::Id> track;
    mrs::Id insert;
    std::optional<mrs::Id> module; // Stable module UUID, never private rack transport index.
    std::uint32_t parameter{};
    ParameterDomain domain{ParameterDomain::nativeInsert};
};
struct NativeParameterMetadata {
    ParameterSpec control;
    bool automatable{};
    bool sampleAccurateUIWrites{}; // Existing Application preview is a block-boundary UI path.
    double hostToDisplay{1};
};
class MRNativeParameterHost final {
public:
    struct State;
    // Resolver MUST be GUI-thread, lifetime-checked (weak owner / Component::SafePointer).
    // One shared coordinator per Application; it serializes the existing global preview.
    explicit MRNativeParameterHost(std::function<mrs::desktop::Application*()> resolver);
    ~MRNativeParameterHost();
    MRNativeParameterHost(const MRNativeParameterHost&)=delete;
    MRNativeParameterHost& operator=(const MRNativeParameterHost&)=delete;
    NativeParameterMetadata describe(const NativeParameterKey&)const;
private:
    std::shared_ptr<State> state;
    friend class MRControlBinding;
};
using MRParameterHost=MRNativeParameterHost;
using MRHostParameterKey=NativeParameterKey;
class MRControlBinding final : private juce::Timer {
public:
    MRControlBinding(MRNativeParameterHost&,NativeParameterKey,MRParameterControl&);
    ~MRControlBinding()override;
    MRControlBinding(const MRControlBinding&)=delete;
    MRControlBinding& operator=(const MRControlBinding&)=delete;
    void sync(); // Optional explicit refresh, also polled on the message thread at 15 Hz.
    juce::String status()const;
    bool active()const;
private:
    struct Impl;
    std::shared_ptr<Impl> impl;
    void timerCallback()override;
};
}
