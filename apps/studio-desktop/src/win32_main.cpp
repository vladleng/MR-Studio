#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <mrs/desktop.hpp>
#include <mrs/offline_device.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
using namespace mrs;
using namespace mrs::desktop;
constexpr COLORREF background = RGB(19,23,33), panel = RGB(29,35,49), border = RGB(51,60,78);
constexpr COLORREF ink = RGB(229,233,245), muted = RGB(154,167,190), accent = RGB(118,104,237), amber = RGB(246,193,97);
std::wstring wide(std::string_view text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);
    if (count <= 0) throw std::runtime_error("invalid UTF-8 text");
    std::wstring out(static_cast<std::size_t>(count),L'\0');
    MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),count); return out;
}
std::string narrow(std::wstring_view text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if (count <= 0) throw std::runtime_error("invalid Unicode text");
    std::string out(static_cast<std::size_t>(count),'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),count,nullptr,nullptr); return out;
}
std::wstring control_text(HWND control) {
    const int n = GetWindowTextLengthW(control); std::wstring out(static_cast<std::size_t>(n)+1,L'\0');
    GetWindowTextW(control,out.data(),n+1); out.resize(static_cast<std::size_t>(n)); return out;
}
std::uint32_t number(HWND control) {
    auto text = narrow(control_text(control));
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) throw std::invalid_argument("Enter a whole positive number");
    const auto value = std::stoull(text); if (value > 768000) throw std::invalid_argument("Number out of range");
    return static_cast<std::uint32_t>(value);
}
std::filesystem::path data_folder() {
    std::array<wchar_t,32768> buffer{};
    const auto n = GetEnvironmentVariableW(L"LOCALAPPDATA",buffer.data(),static_cast<DWORD>(buffer.size()));
    if (n == 0 || n >= buffer.size()) throw std::runtime_error("LOCALAPPDATA is unavailable");
    auto path = std::filesystem::path(buffer.data()) / L"MoonRiverStudio"; std::filesystem::create_directories(path); return path;
}
std::filesystem::path pick(HWND owner, bool save, bool wav = false) {
    std::array<wchar_t,32768> path{};
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner;
    ofn.lpstrFile = path.data(); ofn.nMaxFile = static_cast<DWORD>(path.size());
    ofn.lpstrFilter = wav ? L"WAV audio\0*.wav\0\0" : L"Moon River project\0*.mrsproject\0All files\0*.*\0\0";
    ofn.lpstrDefExt = wav ? L"wav" : L"mrsproject";
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn))) {
        if (CommDlgExtendedError() != 0) throw std::runtime_error("File dialog failed"); return {};
    }
    return std::filesystem::path(path.data());
}
enum ControlId {
    nav_arrange = 100, nav_edit, nav_mix, nav_live, play = 110, pause, stop,
    previous, next, loop, undo, redo, open, save, save_as, demo, import,
    audio_settings, tracks = 140, rename_edit, rename,
    device_combo = 200, rate_edit, buffer_edit, outputs_edit, input_edit,
    connect_button, disconnect_button, panel_button, refresh_button
};
struct UI {
    Application app;
    Preferences prefs;
    std::filesystem::path folder;
    Logger log;
    HWND window{}, settings{};
    HFONT normal{}, heading{}, big{};
    HBRUSH panel_brush{CreateSolidBrush(panel)};
    UINT dpi{96}, settings_dpi{96};
    HFONT settings_font{};
    bool smoke{};
    int smoke_step{};
    unsigned error_count{};
    std::vector<audio::DeviceInfo> devices;
    std::string device_error;
    RECT canvas{};
    double visible_seconds{32};
    explicit UI(bool test) : folder(data_folder()), log(folder / L"studio.log"), smoke(test) {
        if (!smoke) {
            try {
                std::ifstream file(folder / L"desktop.cfg",std::ios::binary);
                if (file) {
                    std::string bytes; std::array<char,16385> data{}; file.read(data.data(),static_cast<std::streamsize>(data.size()));
                    if (file.gcount() > 16384) throw std::runtime_error("config too large");
                    bytes.assign(data.data(),static_cast<std::size_t>(file.gcount())); prefs = decode_preferences(bytes);
                }
            } catch (const std::exception& e) { log.write(e.what()); }
        }
        app.workspace(prefs.workspace); log.write("Studio shell started");
    }
    ~UI() { if (normal) DeleteObject(normal); if (heading) DeleteObject(heading); if (big) DeleteObject(big); if (settings_font) DeleteObject(settings_font); DeleteObject(panel_brush); }
    int s(int dip) const { return MulDiv(dip,static_cast<int>(dpi),96); }
    int ss(int dip) const { return MulDiv(dip,static_cast<int>(settings_dpi),96); }
    void settings_fonts() {
        if (settings_font) DeleteObject(settings_font);
        settings_font = CreateFontW(-ss(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if (settings) for (auto c = GetWindow(settings,GW_CHILD); c; c = GetWindow(c,GW_HWNDNEXT)) SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(settings_font),TRUE);
    }
    HWND child(int id, bool audio = false) const { return GetDlgItem(audio ? settings : window,id); }
    HWND create(HWND parent, const wchar_t* cls, const wchar_t* name, int id, DWORD extra = 0) {
        auto result = CreateWindowExW(0,cls,name,WS_CHILD | WS_VISIBLE | extra,0,0,1,1,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if (!result) throw std::runtime_error("Cannot create window control"); return result;
    }
    void button(HWND parent, const wchar_t* text, int id) { create(parent,L"BUTTON",text,id,BS_OWNERDRAW | WS_TABSTOP); }
    void fonts() {
        if (normal) DeleteObject(normal); if (heading) DeleteObject(heading); if (big) DeleteObject(big);
        normal = CreateFontW(-s(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        heading = CreateFontW(-s(23),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        big = CreateFontW(-s(34),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        for (HWND parent : {window}) if (parent)
            for (auto control = GetWindow(parent,GW_CHILD); control; control = GetWindow(control,GW_HWNDNEXT)) SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(normal),TRUE);
    }
    void initialize() {
        for (auto [id,label] : std::array<std::pair<int,const wchar_t*>,18>{{
            {nav_arrange,L"Arrange"},{nav_edit,L"Edit"},{nav_mix,L"Mix"},{nav_live,L"Live"},
            {play,L"Play"},{pause,L"Pause"},{stop,L"Stop"},{previous,L"< Section"},{next,L"Section >"},
            {loop,L"Loop section"},{undo,L"Undo"},{redo,L"Redo"},{open,L"Open project"},
            {save,L"Save"},{save_as,L"Save as"},{demo,L"Demo"},{import,L"Open WAV"},{audio_settings,L"Audio settings"}}}) button(window,label,id);
        create(window,L"LISTBOX",L"",tracks,WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT);
        create(window,L"EDIT",L"",rename_edit,WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER);
        SendMessageW(child(rename_edit),EM_SETLIMITTEXT,1024,0); button(window,L"Rename track",rename);
        dpi = GetDpiForWindow(window); fonts(); refresh_models(); layout();
        SetTimer(window,1,33,nullptr); if (smoke) SetTimer(window,2,100,nullptr);
        else PostMessageW(window,WM_APP+1,0,0);
    }
    void move(int id, int x, int y, int width, int height) { MoveWindow(child(id),s(x),s(y),s(width),s(height),TRUE); }
    void layout() {
        RECT area{}; GetClientRect(window,&area); const int width = MulDiv(area.right,96,static_cast<int>(dpi));
        const int height = MulDiv(area.bottom,96,static_cast<int>(dpi));
        for (int i = 0; i < 4; ++i) move(nav_arrange+i,240+i*100,18,92,34);
        move(audio_settings,width-166,18,150,34);
        int x = 20; for (auto id : {play,pause,stop,previous,next,loop}) { int w = id >= previous ? 115 : 76; move(id,x,80,w,34); x += w+8; }
        move(undo,20,140,80,32); move(redo,108,140,80,32);
        move(open,20,188,168,32); move(save,20,228,80,32); move(save_as,108,228,80,32);
        move(demo,20,272,80,32); move(import,108,272,80,32);
        move(tracks,20,354,168,std::max(70,height-514));
        move(rename_edit,20,height-148,168,32); move(rename,20,height-108,168,32);
        canvas = {s(220),s(290),area.right-s(20),area.bottom-s(68)};
        InvalidateRect(window,nullptr,FALSE);
    }
    void refresh_models() {
        const auto project = app.services().projects->state().project;
        SendMessageW(child(tracks),LB_RESETCONTENT,0,0);
        for (const auto& t : project->tracks) {
            auto name = wide(t.name); SendMessageW(child(tracks),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
        }
        if (!project->tracks.empty()) { SendMessageW(child(tracks),LB_SETCURSEL,0,0); SetWindowTextW(child(rename_edit),wide(project->tracks.front().name).c_str()); }
        prefs.rate = project->sample_rate;
        if (settings) SetWindowTextW(child(rate_edit,true),std::to_wstring(prefs.rate).c_str());
        EnableWindow(child(undo),app.services().projects->state().can_undo); EnableWindow(child(redo),app.services().projects->state().can_redo);
        std::wstring title = L"Moon River Studio 0.1 — " + wide(project->title) + (app.dirty() ? L" *" : L"");
        SetWindowTextW(window,title.c_str()); InvalidateRect(window,nullptr,FALSE);
    }
    bool discard() {
        if (!app.dirty() || smoke) return true;
        const auto answer = MessageBoxW(window,L"Save changes before replacing the current project?",L"Moon River Studio",MB_YESNOCANCEL | MB_ICONQUESTION);
        if (answer == IDCANCEL) return false;
        if (answer == IDYES) return save_current(false); return true;
    }
    bool save_current(bool as) {
        auto path = as || app.path().empty() ? pick(window,true) : app.path(); if (path.empty()) return false;
        app.save_project(path); log.write("Project saved"); refresh_models(); return true;
    }
    void preferences() {
        prefs.workspace = app.workspace();
        // Only preferences, not the project. Invalid partial edits are never committed.
        auto bytes = encode_preferences(prefs);
        std::ofstream out(folder / L"desktop.cfg",std::ios::binary | std::ios::trunc);
        out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        if (!out) throw std::runtime_error("Cannot save desktop preferences");
    }
    void command(int id, int notification) {
        if (id >= nav_arrange && id <= nav_live) { app.workspace(static_cast<Workspace>(id-nav_arrange)); for (int i = nav_arrange; i <= nav_live; ++i) InvalidateRect(child(i),nullptr,TRUE); InvalidateRect(window,nullptr,FALSE); return; }
        if (id == tracks && notification == LBN_SELCHANGE) {
            const auto selection = SendMessageW(child(tracks),LB_GETCURSEL,0,0);
            const auto project = app.services().projects->state().project;
            if (selection >= 0 && static_cast<std::size_t>(selection) < project->tracks.size()) SetWindowTextW(child(rename_edit),wide(project->tracks[static_cast<std::size_t>(selection)].name).c_str());
            return;
        }
        switch (id) {
        case play: app.play(); break; case pause: app.pause(); break; case stop: app.stop(); break;
        case previous: app.musical().previous_section(); break; case next: app.musical().next_section(); break;
        case loop:
            if (app.services().transport->state().loop) app.musical().clear_loop();
            else if (auto section = app.musical().state().current_section) app.musical().loop_section(section->id);
            break;
        case undo: app.services().projects->undo(); refresh_models(); break;
        case redo: app.services().projects->redo(); refresh_models(); break;
        case rename: {
            const auto selection = SendMessageW(child(tracks),LB_GETCURSEL,0,0); const auto project = app.services().projects->state().project;
            if (selection >= 0 && static_cast<std::size_t>(selection) < project->tracks.size()) app.rename_track(project->tracks[static_cast<std::size_t>(selection)].id,narrow(control_text(child(rename_edit))));
            refresh_models(); break;
        }
        case open: if (discard()) { auto path = pick(window,false); if (!path.empty()) { app.open_project(path); refresh_models(); restore_audio(); } } break;
        case import: if (discard()) { auto path = pick(window,false,true); if (!path.empty()) { app.import_wav(path); refresh_models(); restore_audio(); } } break;
        case demo: if (discard()) { app.demo(); refresh_models(); restore_audio(); } break;
        case save: (void)save_current(false); break; case save_as: (void)save_current(true); break;
        case audio_settings: show_settings(); break;
        }
        InvalidateRect(window,nullptr,FALSE);
    }
    void fill(HDC dc, RECT rect, COLORREF color) {
        const auto brush = CreateSolidBrush(color); FillRect(dc,&rect,brush); DeleteObject(brush);
    }
    void text(HDC dc, int x, int y, int width, int height, std::wstring value, HFONT font = nullptr, COLORREF color = ink) {
        auto previous_font = SelectObject(dc,font ? font : normal); SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT);
        RECT rect{x,y,x+width,y+height}; DrawTextW(dc,value.c_str(),static_cast<int>(value.size()),&rect,DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc,previous_font);
    }
    void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color = border) {
        auto pen = CreatePen(PS_SOLID,1,color); auto old = SelectObject(dc,pen);
        MoveToEx(dc,x1,y1,nullptr); LineTo(dc,x2,y2); SelectObject(dc,old); DeleteObject(pen);
    }
    void draw_button(const DRAWITEMSTRUCT& item) {
        bool selected = item.CtlID >= nav_arrange && item.CtlID <= nav_live && item.CtlID-nav_arrange == static_cast<UINT>(app.workspace());
        fill(item.hDC,item.rcItem,selected ? accent : (item.itemState & ODS_SELECTED) ? border : panel);
        auto label = control_text(item.hwndItem); auto old = SelectObject(item.hDC,GetParent(item.hwndItem) == settings ? settings_font : normal);
        SetBkMode(item.hDC,TRANSPARENT); SetTextColor(item.hDC,(item.itemState & ODS_DISABLED) ? muted : ink);
        RECT rect = item.rcItem; DrawTextW(item.hDC,label.c_str(),static_cast<int>(label.size()),&rect,DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (item.itemState & ODS_FOCUS) { InflateRect(&rect,-3,-3); DrawFocusRect(item.hDC,&rect); }
        SelectObject(item.hDC,old);
    }
    void timeline(HDC dc, RECT rect, bool moving) {
        const auto& context = app.musical().state(); const auto project = context.project;
        Timeline time(project->time,project->sample_rate);
        Sample end = 32*static_cast<Sample>(project->sample_rate);
        for (const auto& c : project->clips) end = std::max(end,c.start+c.length);
        for (const auto& section : project->sections) end = std::max(end,time.to_samples(section.end));
        visible_seconds = static_cast<double>(end)/project->sample_rate;
        const int width = rect.right-rect.left;
        const auto start_tick = std::max<Tick>(0,context.tick-8*ppq);
        const auto pixel = [](double x) { return static_cast<int>(std::clamp(x,-100000.0,100000.0)); };
        const auto x_sample = [&](Sample sample) { return rect.left + pixel((static_cast<double>(sample)/project->sample_rate)/visible_seconds*width); };
        const auto x_tick = [&](Tick tick) {
            if (moving) return rect.left + pixel(static_cast<double>(tick-start_tick)/(16*ppq)*width);
            return x_sample(time.to_samples(tick));
        };
        fill(dc,rect,panel); const int saved = SaveDC(dc); IntersectClipRect(dc,rect.left,rect.top,rect.right,rect.bottom);
        const int top = rect.top;
        text(dc,rect.left+s(12),top+s(4),width-s(24),s(30),moving ? L"Chord track — shared transport" : L"Timeline — click to seek",normal,muted);
        int last_label = rect.left-s(64), last_grid = rect.left-s(8);
        for (int bar = 1; bar <= 256; ++bar) {
            const auto tick = time.to_ticks({bar,1,0}); const int x = x_tick(tick);
            if (x > rect.right) break; if (x < rect.left) continue;
            if (x-last_grid >= s(8)) { line(dc,x,top+s(38),x,rect.bottom); last_grid = x; }
            if (x-last_label >= s(64)) {
                text(dc,x+s(6),top+s(36),s(58),s(25),std::to_wstring(bar),normal,muted); last_label = x;
            }
        }
        for (const auto& c : project->chords) {
            RECT block{x_tick(c.start)+1,top+s(76),x_tick(c.end)-1,top+s(138)};
            if (block.right <= rect.left || block.left >= rect.right) continue;
            fill(dc,block,context.current_chord && context.current_chord->id == c.id ? accent : RGB(46,48,75));
            text(dc,block.left+s(8),block.top,block.right-block.left-s(16),block.bottom-block.top,wide(c.symbol),moving ? big : heading);
        }
        for (const auto& section : project->sections) {
            const auto color = RGB((section.color >> 16)&255,(section.color >> 8)&255,section.color&255);
            RECT block{x_tick(section.start)+1,top+s(148),x_tick(section.end)-1,top+s(183)}; fill(dc,block,color);
            text(dc,block.left+s(8),block.top,block.right-block.left-s(16),block.bottom-block.top,wide(section.name));
        }
        if (!moving) {
            int y = top+s(185);
            for (const auto& track : project->tracks) {
                if (y+s(70) > rect.bottom) break;
                text(dc,rect.left+s(12),y,width-s(24),s(25),wide(track.name),normal,muted); y += s(30);
                for (const auto& clip : project->clips) if (clip.track == track.id) {
                    RECT block{x_sample(clip.start)+1,y,x_sample(clip.start+clip.length)-1,y+s(40)};
                    fill(dc,block,RGB(46,87,110)); text(dc,block.left+s(8),y,block.right-block.left-s(16),s(40),wide(clip.name));
                }
                y += s(60);
            }
        }
        const auto position = moving ? x_tick(context.tick) : x_sample(context.transport.sample);
        line(dc,position,top+s(62),position,rect.bottom,amber);
        RestoreDC(dc,saved);
    }
    void paint(HDC dc) {
        RECT area{}; GetClientRect(window,&area); fill(dc,area,background);
        fill(dc,{0,s(126),s(204),area.bottom},RGB(23,29,41));
        text(dc,s(20),s(16),s(214),s(38),L"Moon River Studio",heading);
        line(dc,0,s(64),area.right,s(64));
        text(dc,s(220),s(146),area.right-s(240),s(40),wide(app.services().projects->state().project->title),heading);
        const auto& context = app.musical().state();
        std::wostringstream position;
        position << L"Bar " << context.transport.musical.bar << L"  Beat " << context.transport.musical.beat
            << L"    " << std::fixed << std::setprecision(2) << static_cast<double>(context.transport.sample)/context.project->sample_rate << L" s";
        text(dc,s(220),s(196),s(480),s(35),position.str(),heading,amber);
        text(dc,s(20),s(318),s(168),s(28),L"Project tracks",normal,muted);
        auto workspace = app.workspace();
        if (workspace == Workspace::arrange || workspace == Workspace::live) {
            auto section = context.current_section ? wide(context.current_section->name) : L"No section";
            auto next_section = context.next_section ? wide(context.next_section->name) : L"End";
            text(dc,s(220),s(240),area.right-s(240),s(30),section + L"   →   " + next_section,normal,muted);
            timeline(dc,canvas,workspace == Workspace::live);
        } else {
            fill(dc,canvas,panel); int y = canvas.top+s(14);
            text(dc,canvas.left+s(16),y,canvas.right-canvas.left-s(32),s(36),workspace == Workspace::mix ? L"Shared processor graph" : L"Clip inspector",heading); y += s(52);
            if (workspace == Workspace::mix) {
                const auto graph = app.graphs()->state().graph;
                text(dc,canvas.left+s(16),y,canvas.right-canvas.left-s(32),s(30),wide(graph->patch_name)); y += s(44);
                for (const auto& n : graph->nodes) {
                    text(dc,canvas.left+s(16),y,canvas.right-canvas.left-s(32),s(30),wide(n.processor_id)+(n.bypass ? L"  [bypass]" : L""),heading); y += s(36);
                    for (const auto& p : n.parameters) { text(dc,canvas.left+s(16),y,canvas.right-canvas.left-s(32),s(25),L"Parameter "+std::to_wstring(p.id)+L" = "+std::to_wstring(p.value),normal,muted); y += s(28); }
                }
                text(dc,canvas.left+s(16),y+s(20),canvas.right-canvas.left-s(32),s(30),L"Processor and mixer state view. Editing follows in the Mix stage.",normal,muted);
            } else {
                for (const auto& c : context.project->clips) {
                    text(dc,canvas.left+s(16),y,canvas.right-canvas.left-s(32),s(32),wide(c.name),heading); y += s(34);
                    text(dc,canvas.left+s(16),y,canvas.right-canvas.left-s(32),s(28),L"Start: "+std::to_wstring(c.start)+L"   Length: "+std::to_wstring(c.length)+L" samples",normal,muted); y += s(38);
                }
                text(dc,canvas.left+s(16),y+s(20),canvas.right-canvas.left-s(32),s(30),L"Shared clip state. Detailed editing follows in the Arrangement stage.",normal,muted);
            }
        }
        const auto status = app.device_status(); const auto metrics = app.engine()->metrics();
        std::wostringstream bottom; bottom << wide(app.audio_name()) << L"  |  " << (context.transport.playback == PlaybackState::playing ? L"Playing" : context.transport.playback == PlaybackState::paused ? L"Paused" : L"Stopped")
            << L"  |  " << context.project->sample_rate << L" Hz  |  callbacks " << metrics.callbacks << L"  |  underruns " << metrics.output_underflows;
        line(dc,0,area.bottom-s(48),area.right,area.bottom-s(48));
        text(dc,s(20),area.bottom-s(44),area.right-s(40),s(36),bottom.str(),normal,status.phase == audio::DevicePhase::error ? RGB(242,100,100) : muted);
    }
    void restore_audio();
    void show_settings();
    void settings_command(int);
    void settings_layout();
    void enumerate_devices();
    void error(const std::exception& e) { ++error_count; log.write(e.what()); if (!smoke) MessageBoxW(window,wide(e.what()).c_str(),L"Moon River Studio",MB_OK | MB_ICONERROR); }
};
LRESULT CALLBACK main_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK settings_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
void UI::restore_audio() {
    if (!prefs.reconnect_audio || prefs.device_name.empty()) return;
#ifdef MRS_HAS_ASIO
    auto device = audio::make_asio_device();
    const auto available = device->enumerate();
    const auto found = std::find_if(available.begin(),available.end(),[&](const auto& info) { return info.name == prefs.device_name; });
    if (found == available.end()) throw std::runtime_error("Saved ASIO device is unavailable. Choose a device in Audio settings.");
    // Resolve the saved name afresh: enumeration indices may change between sessions.
    audio::DeviceConfig config{found->index,app.services().projects->state().project->sample_rate,prefs.buffer,{},prefs.outputs};
    if (prefs.monitor_input >= 0) config.inputs = {prefs.monitor_input};
    app.connect(std::move(device),config); prefs.rate = config.sample_rate; preferences();
    log.write("Saved ASIO connection restored; transport stopped");
    if (settings) {
        SetWindowTextW(child(rate_edit,true),std::to_wstring(prefs.rate).c_str());
        InvalidateRect(settings,nullptr,FALSE);
    }
#else
    throw std::runtime_error("Saved ASIO connection requires the ASIO build.");
#endif
}
void UI::enumerate_devices() {
    devices.clear(); device_error.clear();
    SendMessageW(child(device_combo,true),CB_RESETCONTENT,0,0);
    SendMessageW(child(device_combo,true),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Offline clock (no sound)"));
#ifdef MRS_HAS_ASIO
    try { auto asio = audio::make_asio_device(); devices = asio->enumerate(); }
    catch (const std::exception& e) { device_error = e.what(); log.write(e.what()); }
#else
    device_error = "ASIO support is not included in this build.";
#endif
    int selected{};
    for (std::size_t i = 0; i < devices.size(); ++i) {
        auto label = wide(devices[i].name); SendMessageW(child(device_combo,true),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        if (devices[i].name == prefs.device_name) selected = static_cast<int>(i)+1;
    }
    SendMessageW(child(device_combo,true),CB_SETCURSEL,static_cast<WPARAM>(selected),0);
}
void UI::show_settings() {
    if (settings) { ShowWindow(settings,SW_SHOW); SetForegroundWindow(settings); return; }
    settings = CreateWindowExW(WS_EX_CONTROLPARENT,L"MRStudioAudio",L"Audio settings",WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,s(600),s(475),window,nullptr,GetModuleHandleW(nullptr),this);
    if (!settings) throw std::runtime_error("Cannot open audio settings");
    settings_dpi = GetDpiForWindow(settings);
    create(settings,L"COMBOBOX",L"",device_combo,WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL);
    for (auto id : {rate_edit,buffer_edit,outputs_edit,input_edit}) { create(settings,L"EDIT",L"",id,WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER); SendMessageW(child(id,true),EM_SETLIMITTEXT,256,0); }
    for (auto [id,label] : std::array<std::pair<int,const wchar_t*>,4>{{{connect_button,L"Connect"},{disconnect_button,L"Disconnect"},{panel_button,L"ASIO panel"},{refresh_button,L"Refresh"}}}) button(settings,label,id);
    SetWindowTextW(child(rate_edit,true),std::to_wstring(prefs.rate).c_str()); SetWindowTextW(child(buffer_edit,true),std::to_wstring(prefs.buffer).c_str());
    std::wstring outputs; for (auto o : prefs.outputs) { if (!outputs.empty()) outputs += L","; outputs += std::to_wstring(o+1); }
    SetWindowTextW(child(outputs_edit,true),outputs.c_str()); SetWindowTextW(child(input_edit,true),std::to_wstring(prefs.monitor_input+1).c_str());
    settings_fonts(); enumerate_devices(); settings_layout();
    SetWindowPos(settings,nullptr,0,0,ss(600),ss(475),SWP_NOMOVE | SWP_NOZORDER);
    ShowWindow(settings,SW_SHOW);
}
void UI::settings_layout() {
    auto move = [&](int id, int x, int y, int w, int h) { MoveWindow(child(id,true),ss(x),ss(y),ss(w),ss(h),TRUE); };
    move(device_combo,190,24,355,250); move(rate_edit,190,76,150,30); move(buffer_edit,190,118,150,30);
    move(outputs_edit,190,160,150,30); move(input_edit,190,202,150,30);
    move(connect_button,20,252,110,34); move(disconnect_button,140,252,120,34); move(panel_button,270,252,120,34); move(refresh_button,400,252,110,34);
    InvalidateRect(settings,nullptr,FALSE);
}
void UI::settings_command(int id) {
    if (id == refresh_button) { enumerate_devices(); InvalidateRect(settings,nullptr,FALSE); return; }
    if (id == disconnect_button) { app.disconnect(); prefs.reconnect_audio = false; preferences(); InvalidateRect(settings,nullptr,FALSE); return; }
    // Edit/combo initialization and typing notifications are not device actions.
    if (id != connect_button && id != panel_button) return;
    const auto selection = SendMessageW(child(device_combo,true),CB_GETCURSEL,0,0);
    if (selection < 0 || static_cast<std::size_t>(selection) > devices.size()) throw std::runtime_error("Select an available audio device");
    if (id == panel_button) {
#ifdef MRS_HAS_ASIO
        if (selection == 0) throw std::runtime_error("Offline clock has no ASIO panel");
        // Panel/reconfigure is quiescent; user reconnects after driver changes.
        app.disconnect(); auto device = audio::make_asio_device(); device->control_panel(devices[static_cast<std::size_t>(selection)-1].index);
        InvalidateRect(settings,nullptr,FALSE); return;
#else
        throw std::runtime_error("This build has no ASIO backend");
#endif
    }
    if (id != connect_button) return;
    auto next_prefs = prefs; next_prefs.rate = number(child(rate_edit,true)); next_prefs.buffer = number(child(buffer_edit,true));
    next_prefs.outputs = parse_outputs(narrow(control_text(child(outputs_edit,true))));
    const auto input = number(child(input_edit,true)); if (input > 64) throw std::invalid_argument("Input must be 0 (off) or a channel 1..64");
    next_prefs.monitor_input = static_cast<int>(input)-1;
    next_prefs.device_name = selection == 0 ? "" : devices[static_cast<std::size_t>(selection)-1].name; next_prefs.reconnect_audio = selection != 0; next_prefs.validate();
    audio::DeviceConfig config{selection == 0 ? 0 : devices[static_cast<std::size_t>(selection)-1].index,next_prefs.rate,next_prefs.buffer,{},next_prefs.outputs};
    if (next_prefs.monitor_input >= 0) config.inputs = {next_prefs.monitor_input};
    std::unique_ptr<audio::IAudioDevice> device;
    if (selection == 0) device = audio::make_offline_device();
#ifdef MRS_HAS_ASIO
    else device = audio::make_asio_device();
#endif
    app.connect(std::move(device),config); prefs = std::move(next_prefs); preferences(); log.write("Audio connected");
    InvalidateRect(settings,nullptr,FALSE);
}
LRESULT CALLBACK settings_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto ui = reinterpret_cast<UI*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if (message == WM_NCCREATE) { ui = static_cast<UI*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams); SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ui)); }
    if (!ui) return DefWindowProcW(hwnd,message,wparam,lparam);
    try {
        switch (message) {
        case WM_DPICHANGED: {
            ui->settings_dpi = HIWORD(wparam); const auto rect = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(hwnd,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER | SWP_NOACTIVATE);
            ui->settings_fonts(); ui->settings_layout(); return 0;
        }
        case WM_COMMAND: ui->settings_command(LOWORD(wparam)); return 0;
        case WM_DRAWITEM: ui->draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)); return TRUE;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
            SetTextColor(reinterpret_cast<HDC>(wparam),ink); SetBkColor(reinterpret_cast<HDC>(wparam),panel); return reinterpret_cast<LRESULT>(ui->panel_brush);
        case WM_PAINT: {
            PAINTSTRUCT ps{}; auto dc = BeginPaint(hwnd,&ps); RECT area{}; GetClientRect(hwnd,&area); ui->fill(dc,area,background);
            const std::array<const wchar_t*,5> labels{L"Audio backend",L"Sample rate (Hz)",L"Buffer (frames)",L"Physical outputs",L"Monitor input"};
            const std::array<int,5> ys{24,76,118,160,202};
            for (std::size_t i = 0; i < labels.size(); ++i) ui->text(dc,ui->ss(20),ui->ss(ys[i]),ui->ss(166),ui->ss(30),labels[i],ui->settings_font);
            auto status = ui->app.device_status(); 
            ui->text(dc,ui->ss(20),ui->ss(305),ui->ss(540),ui->ss(30),wide(ui->app.audio_name()),ui->settings_font,muted);
            ui->text(dc,ui->ss(20),ui->ss(337),ui->ss(540),ui->ss(30),L"Output latency: "+std::to_wstring(status.output_latency_ms)+L" ms   CPU: "+std::to_wstring(status.cpu_load*100)+L"%",ui->settings_font,muted);
            ui->text(dc,ui->ss(20),ui->ss(370),ui->ss(540),ui->ss(30),ui->device_error.empty() ? L"Outputs: 1,2. Monitor input: 0 = off. Connect stops/reset transport." : wide(ui->device_error),ui->settings_font,muted);
            EndPaint(hwnd,&ps); return 0;
        }
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_DESTROY: ui->settings = nullptr; return 0;
        }
    } catch (const std::exception& e) { ui->error(e); }
    return DefWindowProcW(hwnd,message,wparam,lparam);
}
LRESULT CALLBACK main_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto ui = reinterpret_cast<UI*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        ui = static_cast<UI*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams); ui->window = hwnd;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ui));
    }
    if (!ui) return DefWindowProcW(hwnd,message,wparam,lparam);
    try {
        switch (message) {
        case WM_CREATE: ui->initialize(); return 0;
        case WM_APP+1: ui->restore_audio(); InvalidateRect(hwnd,nullptr,FALSE); return 0;
        case WM_SIZE: if (ui->normal) ui->layout(); return 0;
        case WM_GETMINMAXINFO: {
            auto info = reinterpret_cast<MINMAXINFO*>(lparam); info->ptMinTrackSize = {ui->s(1000),ui->s(620)}; return 0;
        }
        case WM_DPICHANGED: {
            ui->dpi = HIWORD(wparam); const auto rect = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(hwnd,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER | SWP_NOACTIVATE);
            ui->fonts(); ui->layout(); if (ui->settings) ui->settings_layout(); return 0;
        }
        case WM_COMMAND: ui->command(LOWORD(wparam),HIWORD(wparam)); return 0;
        case WM_DRAWITEM: ui->draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)); return TRUE;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
            SetTextColor(reinterpret_cast<HDC>(wparam),ink); SetBkColor(reinterpret_cast<HDC>(wparam),panel); return reinterpret_cast<LRESULT>(ui->panel_brush);
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; auto dc = BeginPaint(hwnd,&ps); RECT area{}; GetClientRect(hwnd,&area);
            auto buffer = CreateCompatibleDC(dc); auto bitmap = CreateCompatibleBitmap(dc,std::max<LONG>(1,area.right),std::max<LONG>(1,area.bottom));
            auto old = SelectObject(buffer,bitmap); ui->paint(buffer); BitBlt(dc,0,0,area.right,area.bottom,buffer,0,0,SRCCOPY);
            SelectObject(buffer,old); DeleteObject(bitmap); DeleteDC(buffer); EndPaint(hwnd,&ps); return 0;
        }
        case WM_TIMER:
            if (wparam == 1) {
                try { ui->app.poll(); } catch (const std::runtime_error&) { return 0; } // bounded mailbox can be busy
                InvalidateRect(hwnd,nullptr,FALSE); if (ui->settings) InvalidateRect(ui->settings,nullptr,FALSE); return 0;
            }
            if (wparam == 2 && ui->smoke) {
                ++ui->smoke_step;
                if (ui->smoke_step <= 4) ui->command(nav_arrange+ui->smoke_step-1,BN_CLICKED);
                if (ui->smoke_step == 5) {
                    ui->app.rename_track(ui->app.services().projects->state().project->tracks.front().id,"Smoke track"); ui->refresh_models();
                    ui->show_settings(); // offline CI build skips enumeration of physical ASIO
                }
                if (ui->smoke_step == 6) {
                    // Editing incomplete values, including an unselected combo, must not
                    // validate/open a device until the explicit Connect action.
                    SendMessageW(ui->child(device_combo,true),CB_SETCURSEL,static_cast<WPARAM>(-1),0);
                    SetWindowTextW(ui->child(rate_edit,true),L"");
                    SetWindowTextW(ui->child(outputs_edit,true),L"1,");
                    SendMessageW(ui->settings,WM_COMMAND,MAKEWPARAM(device_combo,CBN_SELCHANGE),reinterpret_cast<LPARAM>(ui->child(device_combo,true)));
                    SendMessageW(ui->child(device_combo,true),CB_SETCURSEL,0,0);
                    SetWindowTextW(ui->child(rate_edit,true),L"48000");
                    SetWindowTextW(ui->child(outputs_edit,true),L"1,2");
                    if (!ui->child(play) || !ui->child(device_combo,true) || ui->app.workspace() != Workspace::live || ui->error_count != 0)
                        throw std::runtime_error("GUI initialization/typing produced an unexpected error");
                    // Exercise DPI layout with the same path as a monitor change.
                    ui->dpi = 144; ui->fonts();
                    SetWindowPos(hwnd,nullptr,0,0,ui->s(1000),ui->s(620),SWP_NOMOVE | SWP_NOZORDER);
                    ui->layout(); ui->settings_dpi = 144; ui->settings_fonts();
                    SetWindowPos(ui->settings,nullptr,0,0,ui->ss(600),ui->ss(475),SWP_NOMOVE | SWP_NOZORDER); ui->settings_layout();
                    for (auto parent : {hwnd,ui->settings}) {
                        RECT area{}; GetClientRect(parent,&area);
                        for (auto control = GetWindow(parent,GW_CHILD); control; control = GetWindow(control,GW_HWNDNEXT)) {
                            RECT rect{}; GetWindowRect(control,&rect); MapWindowPoints(nullptr,parent,reinterpret_cast<POINT*>(&rect),2);
                            if (rect.left < 0 || rect.top < 0 || rect.right > area.right || rect.bottom > area.bottom)
                                throw std::runtime_error("DPI layout extends outside the client area");
                        }
                    }
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            break;
        case WM_LBUTTONDOWN: {
            const POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
            if (ui->app.workspace() == Workspace::arrange && PtInRect(&ui->canvas,point)) {
                const auto fraction = static_cast<double>(point.x-ui->canvas.left)/(ui->canvas.right-ui->canvas.left);
                const auto rate = ui->app.services().projects->state().project->sample_rate;
                ui->app.seek(static_cast<Sample>(fraction*ui->visible_seconds*rate));
            }
            return 0;
        }
        case WM_CLOSE:
            if (ui->discard()) { ui->preferences(); DestroyWindow(hwnd); } return 0;
        case WM_DESTROY:
            KillTimer(hwnd,1); KillTimer(hwnd,2); if (ui->settings) DestroyWindow(ui->settings);
            ui->app.disconnect(); PostQuitMessage(0); return 0;
        }
    } catch (const std::exception& e) {
        ui->error(e);
        if (ui->smoke || message == WM_CREATE) { PostQuitMessage(1); if (message == WM_CREATE) return -1; }
    }
    return DefWindowProcW(hwnd,message,wparam,lparam);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int show) {
    try {
        const bool smoke = std::wstring_view(command_line).find(L"--smoke-test") != std::wstring_view::npos;
        UI ui(smoke);
        WNDCLASSW main{}; main.lpfnWndProc = main_proc; main.hInstance = instance; main.lpszClassName = L"MRStudioDesktop";
        main.hCursor = LoadCursorW(nullptr,IDC_ARROW);
        if (!RegisterClassW(&main)) throw std::runtime_error("Cannot register main window");
        WNDCLASSW settings = main; settings.lpfnWndProc = settings_proc; settings.lpszClassName = L"MRStudioAudio";
        if (!RegisterClassW(&settings)) throw std::runtime_error("Cannot register settings window");
        const auto window = CreateWindowExW(WS_EX_CONTROLPARENT,main.lpszClassName,L"Moon River Studio",WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT,CW_USEDEFAULT,1280,850,nullptr,nullptr,instance,&ui);
        if (!window) throw std::runtime_error("Cannot create Studio window");
        MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor); GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        SetWindowPos(window,nullptr,monitor.rcWork.left,monitor.rcWork.top,std::min<LONG>(ui.s(1280),monitor.rcWork.right-monitor.rcWork.left),std::min<LONG>(ui.s(850),monitor.rcWork.bottom-monitor.rcWork.top),SWP_NOZORDER);
        ShowWindow(window,show); UpdateWindow(window);
        MSG msg{};
        for (;;) {
            const auto result = GetMessageW(&msg,nullptr,0,0); if (result < 0) return 1; if (result == 0) break;
            if (msg.message == WM_KEYDOWN && !(ui.settings && IsChild(ui.settings,msg.hwnd))) { try {
                const bool editing = msg.hwnd == ui.child(rename_edit);
                if (!editing && msg.wParam == VK_SPACE) { ui.command(ui.app.services().transport->state().playback == PlaybackState::playing ? pause : play,0); continue; }
                if (GetKeyState(VK_CONTROL) & 0x8000) {
                    int id{}; if (msg.wParam == 'S') id = save; if (!editing && msg.wParam == 'Z') id = undo; if (!editing && msg.wParam == 'Y') id = redo;
                    if (id) { ui.command(id,0); continue; }
                }
            } catch (const std::exception& e) { ui.error(e); continue; } }
            if (ui.settings && IsDialogMessageW(ui.settings,&msg)) continue;
            if (IsDialogMessageW(window,&msg)) continue;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    } catch (const std::exception& e) {
        if (std::wstring_view(command_line).find(L"--smoke-test") == std::wstring_view::npos) MessageBoxW(nullptr,wide(e.what()).c_str(),L"Moon River Studio",MB_OK | MB_ICONERROR);
        return 1;
    }
}
