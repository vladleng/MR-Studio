#pragma once
#include "Desktop.h"
#include <windows.h>

namespace ui {
// Message-thread only. Native children bypass JUCE's component key routing.
// Never subclass a third-party HWND or invoke transport inside its dispatch.
class NativeEditorSpace final {
public:
    NativeEditorSpace(juce::Component& w,std::function<void()> f,HWND scope=nullptr,std::function<void()> full={}):window(w),toggle(std::move(f)),scope(scope),fullScreen(std::move(full)){
        if(entries.empty()){
            hook=SetWindowsHookExW(WH_GETMESSAGE,dispatch,nullptr,GetCurrentThreadId());
            if(!hook)throw std::runtime_error("Cannot install plugin editor keyboard bridge");
        }
        try{entries.push_back(this);}catch(...){if(entries.empty()&&hook){UnhookWindowsHookEx(hook);hook=nullptr;}throw;}
    }
    ~NativeEditorSpace(){std::erase(entries,this);if(entries.empty()&&hook){UnhookWindowsHookEx(hook);hook=nullptr;}}
    NativeEditorSpace(const NativeEditorSpace&)=delete;
    NativeEditorSpace& operator=(const NativeEditorSpace&)=delete;
private:
    static bool textFocus(HWND focus){
        for(auto h=focus;h;h=GetParent(h)){
            wchar_t name[128]{};GetClassNameW(h,name,128);const juce::String c(name);
            if(c.equalsIgnoreCase("Edit")||c.startsWithIgnoreCase("RichEdit")||c.equalsIgnoreCase("ComboBox"))return true;
        }
        // Custom native text editors can expose an OS caret without an Edit HWND.
        // Do not synchronously query UI Automation providers on their own UI thread.
        GUITHREADINFO info{sizeof(GUITHREADINFO)};
        return GetGUIThreadInfo(GetCurrentThreadId(),&info)&&info.hwndCaret&&
            (info.hwndCaret==focus||IsChild(focus,info.hwndCaret));
    }
    bool intercept(MSG& msg){
        if(!window.isShowing()||!window.getPeer())return false;
        const auto peerRoot=static_cast<HWND>(window.getPeer()->getNativeHandle());
        const auto root=scope?scope:peerRoot;
        auto* focused=juce::Component::getCurrentlyFocusedComponent();const bool ownFocus=focused&&(focused==&window||window.isParentOf(focused));
        if(msg.hwnd!=root&&!IsChild(root,msg.hwnd)&&!(scope&&ownFocus&&msg.hwnd==peerRoot))return false;
        if(fullScreen&&msg.wParam==VK_F11){
            if(msg.message==WM_KEYUP){const bool consumed=fullHeld;fullHeld=false;return consumed;}
            if(msg.message==WM_KEYDOWN&&!juce::Component::getCurrentlyModalComponent()&&GetKeyState(VK_CONTROL)>=0&&GetKeyState(VK_MENU)>=0){if(!(msg.lParam&(LPARAM{1}<<30)))fullScreen();fullHeld=true;return true;}
        }
        if(msg.message==WM_KEYUP&&msg.wParam==VK_SPACE){const bool consumed=held;held=false;return consumed;}
        if(msg.message!=WM_KEYDOWN||msg.wParam!=VK_SPACE)return false;
        if((msg.lParam&(LPARAM{1}<<30))!=0)return held;
        held=false;
        if(juce::Component::getCurrentlyModalComponent()||GetKeyState(VK_CONTROL)<0||GetKeyState(VK_SHIFT)<0||GetKeyState(VK_MENU)<0||GetKeyState(VK_LWIN)<0||GetKeyState(VK_RWIN)<0)return false;
        const auto focus=GetFocus();if(focus!=root&&!IsChild(root,focus)&&!(scope&&ownFocus&&focus==peerRoot))return false;
        if(auto* c=focused;c&&(c==&window||window.isParentOf(c)))
            for(;c;c=c->getParentComponent())if(dynamic_cast<juce::TextEditor*>(c)||dynamic_cast<juce::ComboBox*>(c))return false;
        if(textFocus(focus))return false;
        toggle();held=true;return true;
    }
    static LRESULT CALLBACK dispatch(int code,WPARAM remove,LPARAM data){
        if(code>=0&&remove==PM_REMOVE){try{auto& msg=*reinterpret_cast<MSG*>(data);for(auto* entry:entries)if(entry->intercept(msg)){msg.message=WM_NULL;msg.wParam=0;msg.lParam=0;break;}}catch(...){/* Do not unwind through Windows/plugin dispatch. */}}
        return CallNextHookEx(hook,code,remove,data);
    }
    juce::Component& window;std::function<void()> toggle;HWND scope{};std::function<void()> fullScreen;bool held{},fullHeld{};
    inline static HHOOK hook{};inline static std::vector<NativeEditorSpace*> entries;
};
}
