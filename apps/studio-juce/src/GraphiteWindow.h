#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#if JUCE_WINDOWS
#include <bit>
#include <windows.h>
#endif

namespace ui {
inline void applyGraphiteWindowCaption(juce::Component &window) {
#if JUCE_WINDOWS
    // Keep native resize/snap/accessibility. Unsupported DWM attributes simply
    // retain the system caption; no custom window frame or new link dependency.
    if (auto *peer = window.getPeer()) {
        if (auto library = LoadLibraryW(L"dwmapi.dll")) {
            using SetAttribute = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
            if (auto set = std::bit_cast<SetAttribute>(GetProcAddress(library, "DwmSetWindowAttribute"))) {
                const auto handle = static_cast<HWND>(peer->getNativeHandle());
                const BOOL dark = TRUE;
                if (FAILED(set(handle, 20, &dark, sizeof(dark))))
                    set(handle, 19, &dark, sizeof(dark));
                const COLORREF background = RGB(36, 38, 41), text = RGB(218, 229, 244);
                set(handle, 35, &background, sizeof(background));
                set(handle, 36, &text, sizeof(text));
            }
            FreeLibrary(library);
        }
    }
#else
    juce::ignoreUnused(window);
#endif
}
} // namespace ui
