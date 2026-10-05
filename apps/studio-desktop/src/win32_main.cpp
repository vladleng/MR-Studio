#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>
#include <mrs/desktop.hpp>
#include <mrs/vst3.hpp>
#include <mrs/version.hpp>
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
constexpr COLORREF background = RGB(37,40,43), panel = RGB(54,58,62), border = RGB(74,79,84);
constexpr COLORREF ink = RGB(229,233,245), muted = RGB(154,167,190), accent = RGB(43,111,238), amber = RGB(246,193,97);
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
std::filesystem::path studio_folder() {
    PWSTR documents{};
    const auto result = SHGetKnownFolderPath(FOLDERID_Documents,KF_FLAG_CREATE,nullptr,&documents);
    if (FAILED(result)) throw std::runtime_error("Windows Documents folder is unavailable");
    auto path = std::filesystem::path(documents)/L"MR Studio"; CoTaskMemFree(documents); return path;
}
std::filesystem::path pick(HWND owner, bool save, bool wav = false, const std::filesystem::path& initial = {}, std::wstring_view suggested = {}) {
    std::array<wchar_t,32768> path{};
    if (suggested.size() >= path.size()) throw std::invalid_argument("project filename too long");
    std::copy(suggested.begin(),suggested.end(),path.begin());
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner;
    ofn.lpstrFile = path.data(); ofn.nMaxFile = static_cast<DWORD>(path.size());
    ofn.lpstrFilter = wav ? L"WAV audio\0*.wav\0\0" : L"Moon River project\0*.mrsproject\0All files\0*.*\0\0";
    ofn.lpstrDefExt = wav ? L"wav" : L"mrsproject";
    ofn.lpstrInitialDir = initial.empty() ? nullptr : initial.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn))) {
        if (CommDlgExtendedError() != 0) throw std::runtime_error("File dialog failed"); return {};
    }
    return std::filesystem::path(path.data());
}
std::filesystem::path impulse_folder(){wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);return std::filesystem::path(exe).parent_path()/L"Impulses";}
std::vector<std::filesystem::path> pick_wavs(HWND owner) {
    std::array<wchar_t,65536> paths{};
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner;
    ofn.lpstrFile = paths.data(); ofn.nMaxFile = static_cast<DWORD>(paths.size());
    ofn.lpstrFilter = L"WAV audio\0*.wav\0\0";
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) {
        if (CommDlgExtendedError() != 0) throw std::runtime_error("WAV selection failed"); return {};
    }
    std::filesystem::path first(paths.data()); auto p = paths.data()+wcslen(paths.data())+1;
    if (*p == 0) return {first};
    std::vector<std::filesystem::path> result;
    while (*p != 0) { result.push_back(first / p); p += wcslen(p)+1; }
    return result;
}
enum ControlId {
    nav_arrange = 100, nav_edit, nav_mix, nav_live, play = 110, pause, stop,
    previous, next, loop, undo, redo, open, save, save_as, demo, import,
    audio_settings, tracks = 140, rename_edit, rename,
    new_project_button = 160, import_batch, add_track, delete_track, track_up, track_down, zoom_in, zoom_out, zoom_fit, split_clip_button, delete_clip_button, snap_button,
    record_button = 172, arm_button, monitor_button, add_bus_button, files_exit = 180, studio_folder_button,
    device_combo = 200, rate_edit, buffer_edit, outputs_edit, input_edit,
    connect_button, disconnect_button, panel_button, refresh_button, profile_combo = 220, profile_name, profile_save, profile_load, profile_delete,
    fx_list=300, fx_kind, fx_add, fx_remove, fx_up, fx_down, fx_bypass, fx_value, fx_frequency, fx_q, fx_apply, fx_band, fx_band_enable, fx_scan, fx_editor, fx_status,
    browser_toggle=339, browser_tab=340, browser_tree, browser_scan, ir_load, ir_mix, ir_low, ir_high, ir_polarity, ir_preset, ir_name, fx_hint, fx_gain_label, fx_frequency_label, fx_q_label
};
// Hidden smoke transport has one explicit callback consumer: the test steps.
class SmokeDevice final : public audio::IAudioDevice {
    audio::DevicePhase phase_{audio::DevicePhase::closed};
public:
    std::vector<audio::DeviceInfo> enumerate() override { return {{0,"Smoke",{},{"L","R"},32,2048,128,-1}}; }
    void control_panel(int) override {}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>) override { phase_=audio::DevicePhase::open; }
    void start() override { phase_=audio::DevicePhase::running; }
    void stop() override { phase_=audio::DevicePhase::stopped; }
    void close() noexcept override { phase_=audio::DevicePhase::closed; }
    audio::DeviceStatus status() override { return {phase_,48000,0,0,0,{}}; }
};
struct UI {
    Application app;
    Preferences prefs;
    std::filesystem::path folder;
    Logger log;
    StudioFolders studio;
    HWND window{}, settings{}, fx_window{};
    std::optional<Id> fx_target{};
    std::size_t fx_selection{};
    bool fx_editable{};
    HWND eq_window{},vst_editor{};
    std::optional<Id> editor_target;Id editor_slot;
    std::uint64_t editor_generation{};
    std::size_t eq_band{2},vst_parameter{};
    std::optional<std::vector<NativeInsert>> fx_preview;
    POINT eq_origin{};EqBand eq_original{};bool eq_dragging{};
    std::vector<processing::VstPlugin> vst_catalog;
    RECT browser_area{};bool browser_visible{true};
    std::optional<std::size_t> plugin_drag;
    std::optional<Id> plugin_drop_track;
    bool plugin_drop_valid{};
    POINT plugin_drag_point{};
    static LRESULT CALLBACK tab_proc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    void browser_refresh();void browser_notify(NMHDR*);void browser_move(POINT);void browser_up(POINT);void browser_cancel();void browser_scan_poll();
    void open_insert(std::optional<Id>,std::size_t);void open_vst(std::optional<Id>,const Id&,const std::string&);
    std::vector<processing::ParameterInfo> fx_parameters;
    std::future<std::vector<processing::VstPlugin>> vst_scan;
    std::shared_ptr<std::atomic<bool>> vst_scan_cancel{std::make_shared<std::atomic<bool>>(false)};
    void fx_catalog_refresh();void close_vst_editor();
    RECT eq_plot() const;POINT eq_point(const NativeInsert&,std::size_t) const;
    std::optional<std::size_t> eq_hit(POINT) const;
    void eq_paint(HDC);void eq_down(POINT);void eq_move(POINT);void eq_up(POINT);void eq_cancel();void eq_wheel(POINT,int);
    void open_fx(std::optional<Id>);
    void fx_create(); void fx_refresh(); void fx_command(int);
    HFONT normal{}, heading{}, big{};
    HBRUSH panel_brush{CreateSolidBrush(panel)};
    UINT dpi{96}, settings_dpi{96};
    HFONT settings_font{};
    std::array<std::wstring,3> settings_status_text{};
    std::optional<DeviceProfile> staged_profile{};
    ULONGLONG settings_status_tick{};
    bool smoke{};
    bool render_preview{};
    int smoke_step{};
    unsigned error_count{};
    std::uint64_t button_paints{}; // smoke regression: native buttons must stay stable during canvas repaint
    std::uint64_t button_layout_events{};
    std::uint64_t unrelated_button_updates{};
    std::map<HWND,WNDPROC> smoke_button_procs;
    std::vector<audio::DeviceInfo> devices;
    std::string device_error;
    RECT canvas{};
    double visible_seconds{32}, view_start{};
    int track_height{112};
    std::array<int,4> wheel_remainder{};
    bool fit_view{true};
    std::size_t first_track{};
    double track_offset{}; // logical pixels inside the first visible row
    std::optional<Id> selected_track, selected_clip;
    bool snap{};
    enum class DragMode { move, left, right };
    struct Drag { Clip original, preview; DragMode mode; Sample anchor{}, frames{}; POINT origin{}; bool changed{}; };
    std::optional<Drag> drag;
    audio::MixerMeters mix_meters{};
    std::size_t first_mix_track{};
    struct MixDrag { std::optional<Id> track; Track::Mix mix; float master{1}; bool pan{}; RECT rect{}; bool vertical{}; POINT origin{}; float initial{}; };
    std::optional<MixDrag> mix_drag;
    explicit UI(bool test) : folder(data_folder()), log(folder / L"studio.log"), studio{studio_folder()}, smoke(test) {
        studio.ensure();
        if (!smoke) {
            try {
                std::ifstream file(folder / L"desktop.cfg",std::ios::binary);
                if (file) {
                    std::string bytes; std::vector<char> data(524289); file.read(data.data(),static_cast<std::streamsize>(data.size()));
                    if (file.gcount() > 524288) throw std::runtime_error("config too large");
                    bytes.assign(data.data(),static_cast<std::size_t>(file.gcount())); prefs = decode_preferences(bytes);
                }
            } catch (const std::exception& e) { log.write(e.what()); }
        }
        if (prefs.workspace == Workspace::live) prefs.workspace = Workspace::arrange;
        app.workspace(prefs.workspace); log.write("Studio shell started");
    }
    ~UI() { *vst_scan_cancel=true; if (normal) DeleteObject(normal); if (heading) DeleteObject(heading); if (big) DeleteObject(big); if (settings_font) DeleteObject(settings_font); DeleteObject(panel_brush); }
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
    static LRESULT CALLBACK smoke_button_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        const auto ui = reinterpret_cast<UI*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if (message == WM_WINDOWPOSCHANGING) ++ui->button_layout_events;
        const auto id = GetDlgCtrlID(hwnd);
        if ((message == WM_SETTEXT || message == WM_ENABLE) && id != undo && id != redo) ++ui->unrelated_button_updates;
        return CallWindowProcW(ui->smoke_button_procs.at(hwnd),hwnd,message,wparam,lparam);
    }
    void button(HWND parent, const wchar_t* text, int id) {
        const auto control = create(parent,L"BUTTON",text,id,BS_OWNERDRAW | WS_TABSTOP);
        if (smoke) {
            SetWindowLongPtrW(control,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(this));
            const auto proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(control,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(smoke_button_proc)));
            smoke_button_procs.emplace(control,proc);
        }
    }
    void fonts() {
        if (normal) DeleteObject(normal); if (heading) DeleteObject(heading); if (big) DeleteObject(big);
        normal = CreateFontW(-s(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        heading = CreateFontW(-s(18),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        big = CreateFontW(-s(26),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        for (HWND parent : {window}) if (parent)
            for (auto control = GetWindow(parent,GW_CHILD); control; control = GetWindow(control,GW_HWNDNEXT)) SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(normal),TRUE);
    }
    void file_menu() {
        const auto bar = CreateMenu(), files = CreatePopupMenu();
        if (!bar || !files) {
            if (bar) DestroyMenu(bar); if (files) DestroyMenu(files);
            throw std::runtime_error("Cannot create Files menu");
        }
        bool ok = true;
        const auto item = [&](UINT id, const wchar_t* label) { ok = AppendMenuW(files,MF_STRING,id,label) != FALSE && ok; };
        const auto separator = [&] { ok = AppendMenuW(files,MF_SEPARATOR,0,nullptr) != FALSE && ok; };
        item(new_project_button,L"&New project\tCtrl+N"); item(open,L"&Open project...\tCtrl+O");
        const auto recent = CreatePopupMenu();
        if (!recent) { DestroyMenu(files); DestroyMenu(bar); throw std::runtime_error("Cannot create recent projects menu"); }
        if (prefs.recent_projects.empty()) AppendMenuW(recent,MF_STRING | MF_GRAYED,0,L"No recent projects");
        for (std::size_t i=0; i<prefs.recent_projects.size(); ++i) AppendMenuW(recent,MF_STRING,3000+i,wide(prefs.recent_projects[i]).c_str());
        ok = AppendMenuW(files,MF_POPUP,reinterpret_cast<UINT_PTR>(recent),L"Open &recent project") != FALSE && ok;
        separator(); item(save,L"&Save\tCtrl+S"); item(save_as,L"Save &as...\tCtrl+Shift+S");
        separator(); item(import_batch,L"&Import WAVs...\tCtrl+I"); item(import,L"Open &WAV as new project...");
        item(demo,L"Open &demo project"); separator(); item(studio_folder_button,L"Open studio &folder"); item(files_exit,L"E&xit");
        if (!ok || !AppendMenuW(bar,MF_POPUP,reinterpret_cast<UINT_PTR>(files),L"&Files")) {
            DestroyMenu(files); DestroyMenu(bar); throw std::runtime_error("Cannot populate Files menu");
        }
        const auto previous_menu = GetMenu(window);
        if (!SetMenu(window,bar)) { DestroyMenu(bar); throw std::runtime_error("Cannot attach Files menu"); }
        if (previous_menu) DestroyMenu(previous_menu);
        DrawMenuBar(window); // native thin row: keyboard navigation and DPI handling
    }
    void initialize() {
        file_menu();
        for (auto [id,label] : std::array<std::pair<int,const wchar_t*>,12>{{
            {nav_arrange,L"Arrange"},{nav_edit,L"Edit"},{nav_mix,L"Mix"},
            {play,L"Play"},{pause,L"Pause"},{stop,L"Stop"},{previous,L"< Section"},{next,L"Section >"},
            {loop,L"Loop section"},{undo,L"Undo"},{redo,L"Redo"},{audio_settings,L"Audio settings"}}}) button(window,label,id);
        for (auto [id,label] : std::array<std::pair<int,const wchar_t*>,7>{{
            {add_track,L"+ Track"},{delete_track,L"Delete"},{track_up,L"Up"},{track_down,L"Down"},
            {zoom_in,L"Zoom +"},{zoom_out,L"Zoom -"},{zoom_fit,L"Fit"}}}) button(window,label,id);
        button(window,L"Record (R)",record_button); button(window,L"Arm track",arm_button); button(window,L"Monitor on",monitor_button);
        button(window,L"+ Bus",add_bus_button);button(window,L"BROWS",browser_toggle);
        create(window,WC_TABCONTROLW,L"",browser_tab,WS_TABSTOP);TCITEMW item{};item.mask=TCIF_TEXT;wchar_t label[]=L"VST3";item.pszText=label;TabCtrl_InsertItem(child(browser_tab),0,&item);SetWindowSubclass(child(browser_tab),tab_proc,1,reinterpret_cast<DWORD_PTR>(this));
        create(window,WC_TREEVIEWW,L"",browser_tree,WS_TABSTOP|WS_BORDER|TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS);
        TreeView_SetBkColor(child(browser_tree),background);TreeView_SetTextColor(child(browser_tree),ink);TreeView_SetLineColor(child(browser_tree),border);button(window,L"Scan VST3…",browser_scan);
#ifdef MRS_HAS_VST3
        if(!smoke&&vst_catalog.empty())try{vst_catalog=processing::load_vst3_cache(folder/L"vst3.cache");}catch(const std::exception& e){log.write(e.what());}
#endif
        browser_refresh();
        button(window,L"Split (S)",split_clip_button); button(window,L"Del clip",delete_clip_button); button(window,L"Snap off",snap_button);
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
        for (int i=0; i<3; ++i) move(nav_arrange+i,width-316+i*76,height-34,70,28);
        move(browser_toggle,width-88,height-34,76,28);
        move(undo,250,8,58,28); move(redo,314,8,58,28);
        int ax=388; for (auto id : {add_track,delete_track,track_up,track_down}) { move(id,ax,8,64,28); ax+=70; }
        move(split_clip_button,680,8,66,28); move(delete_clip_button,752,8,66,28); move(snap_button,824,8,68,28);
        move(audio_settings,width-166,44,150,28);
        move(rename_edit,12,44,170,28); move(rename,188,44,102,28);
        move(zoom_in,312,44,68,28); move(zoom_out,386,44,68,28); move(zoom_fit,460,44,48,28);
        move(arm_button,528,44,100,28); move(monitor_button,634,44,108,28);
        int x=12; for (auto id : {play,pause,stop,previous,next,loop}) { const int w=id >= previous ? 94 : 60; move(id,x,height-66,w,30); x+=w+6; }
        move(record_button,x,height-66,86,30);
        move(tracks,12,100,272,std::max(70,height-200));
        const int sidebar=width<1200?210:260;browser_area={area.right-s(sidebar+12),s(84),area.right-s(12),area.bottom-s(84)};
        canvas={s(300),s(84),browser_visible?browser_area.left-s(10):area.right-s(12),area.bottom-s(84)};
        move(browser_tab,width-sidebar-12,84,sidebar,30);move(browser_scan,width-sidebar-4,124,sidebar-16,28);move(browser_tree,width-sidebar-4,162,sidebar-16,std::max(60,height-278));
        sync_workspace_controls();
        InvalidateRect(window,nullptr,FALSE);
    }
    void sync_workspace_controls() {
        // Window mutations belong to state/layout updates, never to paint. Even
        // an unchanged MoveWindow generates native layout/erase messages.
        const auto workspace = app.workspace();
        const auto visible = [&](int id, bool show) {
            const auto control = child(id);
            const bool shown = (GetWindowLongPtrW(control,GWL_STYLE) & WS_VISIBLE) != 0;
            if (shown != show) ShowWindow(control,show ? SW_SHOWNA : SW_HIDE);
        };
        for(auto id:{browser_tab,browser_tree,browser_scan})visible(id,browser_visible);
        for (auto id : {add_track,delete_track,track_up,track_down,zoom_in,zoom_out,zoom_fit,split_clip_button,delete_clip_button,snap_button})
            visible(id,workspace == Workspace::arrange || workspace == Workspace::mix);
        visible(add_bus_button,workspace == Workspace::mix);
        visible(tracks,workspace != Workspace::arrange && workspace != Workspace::mix);
        visible(arm_button,false); visible(monitor_button,false);
        const bool enabled = !app.recording() && app.engine()->state().playback != PlaybackState::playing;
        if ((IsWindowEnabled(child(add_bus_button)) != FALSE) != enabled) EnableWindow(child(add_bus_button),enabled);
        if (canvas.bottom <= canvas.top) return;
        const auto area = mix_area();
        const RECT wanted{area.left+s(8),area.top+s(5),area.left+s(104),area.top+s(35)};
        RECT current{}; GetWindowRect(child(add_bus_button),&current);
        MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&current),2);
        if (!EqualRect(&current,&wanted)) MoveWindow(child(add_bus_button),wanted.left,wanted.top,wanted.right-wanted.left,wanted.bottom-wanted.top,TRUE);
    }
    void enable_changed(int id, bool enabled) {
        if ((IsWindowEnabled(child(id)) != FALSE) != enabled) EnableWindow(child(id),enabled);
    }
    void text_changed(HWND control, const std::wstring& label) {
        if (control_text(control) != label) SetWindowTextW(control,label.c_str());
    }
    void refresh_mix_controls() {
        const auto state = app.services().projects->state();
        if (fx_window) fx_refresh();
        enable_changed(undo,state.can_undo && !app.recording());
        enable_changed(redo,state.can_redo && !app.recording());
        const auto title = L"Moon River Studio " + wide(application_version) + L" — " + wide(state.project->title) + (app.dirty() ? L" *" : L"");
        text_changed(window,title); InvalidateRect(window,nullptr,FALSE);
    }
    void refresh_models(bool mix_only = false) {
        if(vst_editor&&editor_generation!=app.insert_generation())close_vst_editor();
        if (mix_only) { refresh_mix_controls(); return; }
        sync_workspace_controls();
        const auto project = app.services().projects->state().project;
        app.prepare_waveforms();
        SendMessageW(child(tracks),LB_RESETCONTENT,0,0);
        for (const auto& t : project->tracks) {
            auto name = (t.kind == TrackKind::bus ? L"[BUS] " : app.track_armed(t.id) ? L"[R] " : L"") + wide(t.name); SendMessageW(child(tracks),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
        }
        if (!project->tracks.empty()) {
            auto found = std::find_if(project->tracks.begin(),project->tracks.end(),[&](const auto& t) { return selected_track && t.id == *selected_track; });
            if (found == project->tracks.end()) found = project->tracks.begin();
            selected_track = found->id;
            SendMessageW(child(tracks),LB_SETCURSEL,static_cast<WPARAM>(found-project->tracks.begin()),0);
            text_changed(child(rename_edit),wide(found->name));
        } else { selected_track.reset(); text_changed(child(rename_edit),L""); }
        first_track = std::min(first_track,project->tracks.empty() ? std::size_t{0} : project->tracks.size()-1);
        scroll_tracks(0);
        if (selected_clip && std::none_of(project->clips.begin(),project->clips.end(),[&](const auto& c) { return c.id == *selected_clip; })) selected_clip.reset();
        enable_changed(split_clip_button,selected_clip.has_value() && !app.recording()); enable_changed(delete_clip_button,selected_clip.has_value() && !app.recording());
        prefs.rate = project->sample_rate;
        if (settings) text_changed(child(rate_edit,true),std::to_wstring(prefs.rate));
        for (auto id : {play,previous,next,loop,add_track,delete_track,track_up,track_down,rename,rename_edit,audio_settings}) enable_changed(id,!app.recording());
        const bool audio_selected = std::any_of(project->tracks.begin(),project->tracks.end(),[&](const auto& t) { return selected_track && t.id == *selected_track && t.kind == TrackKind::audio; });
        enable_changed(arm_button,audio_selected && !app.recording());
        enable_changed(record_button,app.recording() || (app.armed_track().has_value() && app.has_input() && app.audio_running()));
        enable_changed(monitor_button,app.has_input() && app.audio_running());
        text_changed(child(record_button),app.recording() ? L"End rec (R)" : L"Record (R)");
        const bool armed = selected_track && app.armed_track() && *selected_track == *app.armed_track();
        text_changed(child(arm_button),armed ? L"Disarm" : L"Arm track");
        text_changed(child(monitor_button),app.monitoring() ? L"Monitor on" : L"Monitor off");
        const auto files = GetSubMenu(GetMenu(window),0);
        bool menu_changed{};
        for (auto id : {new_project_button,open,save,save_as,import_batch,import,demo}) {
            const bool disabled = (GetMenuState(files,static_cast<UINT>(id),MF_BYCOMMAND) & (MF_GRAYED | MF_DISABLED)) != 0;
            if (disabled != app.recording()) { EnableMenuItem(files,static_cast<UINT>(id),MF_BYCOMMAND | (app.recording() ? MF_GRAYED : MF_ENABLED)); menu_changed = true; }
        }
        if (menu_changed) DrawMenuBar(window);
        refresh_mix_controls();
    }
    bool discard() {
        if (!app.dirty() || smoke) return true;
        const auto answer = MessageBoxW(window,L"Save changes before replacing the current project?",L"Moon River Studio",MB_YESNOCANCEL | MB_ICONQUESTION);
        if (answer == IDCANCEL) return false;
        if (answer == IDYES) return save_current(false); return true;
    }
    bool save_current(bool as) {
        auto path = as || app.path().empty() ? pick(window,true,false,studio.projects(),
            app.path().empty() ? wide(app.services().projects->state().project->title)+L".mrsproject" : app.path().filename().wstring()) : app.path();
        if (path.empty()) return false;
        const bool managed = !app.path().empty() &&
            (app.path().parent_path().filename() == app.path().stem() ||
             (std::filesystem::is_directory(app.path().parent_path()/"Media") && std::filesystem::is_directory(app.path().parent_path()/"Mixdown")));
        if (as || !managed) path = project_folder_file(path);
        if (path != app.path() && std::filesystem::exists(path)) throw std::runtime_error("This project folder already exists. Choose a new project name.");
        app.save_project(path); recent_project(); log.write("Project and Media saved"); refresh_models(); return true;
    }
    void recent_project() {
        if (app.path().empty()) return;
        remember_project(prefs,app.path()); file_menu(); if (!smoke) preferences();
    }
    void preferences() {
        prefs.workspace = app.workspace();
        // Only preferences, not the project. Invalid partial edits are never committed.
        auto bytes = encode_preferences(prefs);
        std::ofstream out(folder / L"desktop.cfg",std::ios::binary | std::ios::trunc);
        out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        if (!out) throw std::runtime_error("Cannot save desktop preferences");
    }
    void cancel_drag() {
        drag.reset(); if (GetCapture() == window) ReleaseCapture(); InvalidateRect(window,nullptr,FALSE);
    }
    int audio_top() const {
        const auto p = app.services().projects->state().project;
        return canvas.top+s(p->chords.empty() && p->sections.empty() ? 38 : 82);
    }
    int row_height() const { return s(track_height); }
    int clip_height() const { return row_height()-s(36); }
    int arrangement_bottom() const { return app.workspace() == Workspace::mix ? mix_area().top : canvas.bottom; }
    int track_offset_pixels() const { return s(static_cast<int>(std::round(track_offset))); }
    void scroll_tracks(double pixels) {
        const auto count=app.services().projects->state().project->tracks.size();
        const double visible=static_cast<double>(std::max(0,arrangement_bottom()-audio_top()))*96/dpi;
        const double maximum=std::max(0.0,static_cast<double>(count)*track_height-visible);
        const double position=std::clamp(static_cast<double>(first_track)*track_height+track_offset+pixels,0.0,maximum);
        first_track=static_cast<std::size_t>(position/track_height); track_offset=position-static_cast<double>(first_track)*track_height;
    }
    void space_action() { command(app.engine()->state().playback == PlaybackState::playing ? stop : play,0); }
    void wheel(POINT point, int delta, unsigned keys) {
        if (drag || mix_drag || (app.workspace() != Workspace::arrange && app.workspace() != Workspace::mix)) return;
        RECT region{s(8),canvas.top,canvas.right,arrangement_bottom()};
        const auto mixer=mix_area(); const bool in_mixer=app.workspace() == Workspace::mix && PtInRect(&mixer,point);
        if (!PtInRect(&region,point) && !in_mixer) return;
        const bool control=(keys & MK_CONTROL) != 0, shift=(keys & MK_SHIFT) != 0;
        const std::size_t action=control ? (shift ? 0 : 1) : (shift ? 2 : 3);
        if (action == 3) { scroll_tracks(-static_cast<double>(delta)*32/WHEEL_DELTA); InvalidateRect(window,nullptr,FALSE); return; }
        auto& remainder=wheel_remainder[action]; remainder+=delta; const int steps=remainder/WHEEL_DELTA; remainder%=WHEEL_DELTA;
        if (!steps) return;
        if (in_mixer && shift && !control) {
            const auto count=mix_tracks().size(); const auto next=static_cast<long long>(first_mix_track)-steps;
            first_mix_track=count ? static_cast<std::size_t>(std::clamp(next,0LL,static_cast<long long>(count-1))) : 0;
        } else if (action == 0) {
            fit_view=false; const double fraction=std::clamp(static_cast<double>(point.x-canvas.left)/std::max(1L,canvas.right-canvas.left),0.0,1.0);
            const double anchor=view_start+fraction*visible_seconds;
            visible_seconds=std::clamp(visible_seconds*std::pow(1.25,-steps),0.25,86400.0);
            view_start=std::max(0.0,anchor-fraction*visible_seconds);
        } else if (action == 1) track_height=static_cast<int>(std::clamp(std::round(track_height*std::pow(1.2,steps)),92.0,320.0));
        else if (action == 2) { fit_view=false; view_start=std::max(0.0,view_start-steps*visible_seconds/10); }
        scroll_tracks(0);
        InvalidateRect(window,nullptr,FALSE);
    }
    int sample_x(Sample sample) const {
        const auto rate = app.services().projects->state().project->sample_rate;
        return canvas.left+static_cast<int>(std::clamp((static_cast<double>(sample)/rate-view_start)/visible_seconds*(canvas.right-canvas.left),-100000.0,100000.0));
    }
    Sample sample_at(int x) const {
        const auto rate = app.services().projects->state().project->sample_rate;
        const auto seconds = view_start+static_cast<double>(x-canvas.left)/(canvas.right-canvas.left)*visible_seconds;
        return static_cast<Sample>(std::clamp(seconds*rate,0.0,static_cast<double>(max_sample)));
    }
    std::optional<Id> track_at(int y) const {
        if (y < audio_top() || y >= arrangement_bottom()) return {};
        const auto index = first_track+static_cast<std::size_t>((y-audio_top()+track_offset_pixels())/row_height());
        const auto p = app.services().projects->state().project;
        return index < p->tracks.size() ? std::optional<Id>{p->tracks[index].id} : std::nullopt;
    }
    std::optional<Clip> hit_clip(POINT point) const {
        if (!PtInRect(&canvas,point) || point.y < audio_top() || point.y >= arrangement_bottom()) return {};
        const auto row = (point.y-audio_top()+track_offset_pixels())%row_height();
        if (row < s(26) || row >= row_height()-s(10)) return {};
        const auto track = track_at(point.y); if (!track) return {};
        const auto p = app.services().projects->state().project;
        for (auto it = p->clips.rbegin(); it != p->clips.rend(); ++it)
            if (it->track == *track && point.x >= sample_x(it->start) && point.x <= sample_x(it->start+it->length)) return *it;
        return {};
    }
    Sample grid(Sample value) const {
        value = std::clamp(value,Sample{0},max_sample);
        return snap && !(GetKeyState(VK_SHIFT)&0x8000) ? snap_to_grid(*app.services().projects->state().project,value) : value;
    }
    void mouse_down(POINT point) {
        if (app.workspace() == Workspace::mix) { const auto area = mix_area(); if (PtInRect(&area,point)) { mixer_down(point); return; } }
        if ((app.workspace() == Workspace::arrange || app.workspace() == Workspace::mix) && mini_down(point)) return;
        if (app.recording() || (app.workspace() != Workspace::arrange && app.workspace() != Workspace::mix) || !PtInRect(&canvas,point)) return;
        SetFocus(window);
        if (auto clip = hit_clip(point)) {
            selected_clip = clip->id; selected_track = clip->track; refresh_models();
            if (app.engine()->state().playback == PlaybackState::playing) return; // selection is always available
            auto mode = DragMode::move;
            if (std::abs(point.x-sample_x(clip->start)) <= s(7)) mode = DragMode::left;
            else if (std::abs(point.x-sample_x(clip->start+clip->length)) <= s(7)) mode = DragMode::right;
            fit_view = false;
            drag = Drag{*clip,*clip,mode,sample_at(point.x),app.source_frames(clip->id),point,false};
            SetCapture(window);
        } else app.seek(grid(sample_at(point.x))); // ruler/empty space keeps clip selection for Split
    }
    void mouse_move(POINT point) {
        if (mix_drag) { mixer_move(point); return; }
        if (!drag) return;
        auto& d = *drag;
        if (!d.changed && std::abs(point.x-d.origin.x) < s(3) && std::abs(point.y-d.origin.y) < s(3)) return;
        d.changed = true; d.preview = d.original;
        const auto delta = sample_at(point.x)-d.anchor;
        if (d.mode == DragMode::move) {
            d.preview.start = std::clamp(grid(std::max(Sample{0},d.original.start+delta)),Sample{0},max_sample-d.original.length);
            if (auto target = track_at(point.y)) {
                const auto p = app.services().projects->state().project;
                auto it = std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t) { return t.id == *target; });
                if (it != p->tracks.end() && it->kind == TrackKind::audio) d.preview.track = *target;
            }
        } else if (d.mode == DragMode::left) {
            const auto finish = d.original.start+d.original.length;
            const auto lower = std::max(Sample{0},d.original.start-d.original.source_offset);
            d.preview.start = std::clamp(grid(std::max(Sample{0},d.original.start+delta)),lower,finish-1);
            d.preview.source_offset += d.preview.start-d.original.start; d.preview.length = finish-d.preview.start;
        } else {
            const auto maximum = std::min(max_sample,d.original.start+d.frames-d.original.source_offset);
            const auto finish = std::clamp(grid(std::max(Sample{0},d.original.start+d.original.length+delta)),d.original.start+1,maximum);
            d.preview.length = finish-d.original.start;
        }
        InvalidateRect(window,nullptr,FALSE);
    }
    void mouse_up(POINT point) {
        if (mix_drag) {
            mixer_move(point); const auto d = *mix_drag; mix_drag.reset(); ReleaseCapture();
            const auto project=app.services().projects->state().project;bool changed=false;
            if(d.track){for(const auto& t:project->tracks)if(t.id==d.track && t.mix!=d.mix)changed=true;if(changed)app.set_track_mix(*d.track,d.mix);}
            else if(project->master_gain!=d.master){changed=true;app.set_master_gain(d.master);}
            if(!changed)app.cancel_mix_preview();
            refresh_models(true); return;
        }
        if (!drag) return;
        mouse_move(point); auto d = *drag; cancel_drag();
        if (!d.changed || d.preview == d.original) return;
        if (d.mode == DragMode::move) app.move_clip(d.original.id,d.preview.track,d.preview.start);
        else app.trim_clip(d.original.id,d.preview.start,d.preview.start+d.preview.length);
        selected_track = d.preview.track; refresh_models();
    }
    void command(int id, int notification) {
        if(id==open||id==new_project_button||id==demo||id==import||id==import_batch)close_vst_editor();
        if (mix_drag) cancel_mix_drag();
        if (app.recording()) {
            for (auto blocked : {play,previous,next,loop,undo,redo,rename,new_project_button,import_batch,add_track,add_bus_button,delete_track,track_up,track_down,split_clip_button,delete_clip_button,open,import,demo,save,save_as,audio_settings,arm_button})
                if (id == blocked) return;
        }
        if (drag) cancel_drag();
        if (id >= 3000 && id < 3010) {
            const auto index = static_cast<std::size_t>(id-3000);
            if (app.recording() || index >= prefs.recent_projects.size() || !discard()) return;
            const auto name = prefs.recent_projects[index];
            app.open_project(std::filesystem::path(std::u8string(name.begin(),name.end())));
            recent_project(); fit_view = true; view_start = 0; first_track = 0; track_offset=0; refresh_models(); restore_audio(); return;
        }
        if(id==browser_toggle){browser_cancel();browser_visible=!browser_visible;layout();InvalidateRect(child(browser_toggle),nullptr,FALSE);return;}
        if (id >= nav_arrange && id <= nav_mix) { app.workspace(id == nav_mix && app.workspace() == Workspace::mix ? Workspace::arrange : static_cast<Workspace>(id-nav_arrange)); sync_workspace_controls(); for (int i = nav_arrange; i <= nav_mix; ++i) InvalidateRect(child(i),nullptr,FALSE); InvalidateRect(window,nullptr,FALSE); return; }
        if (id == tracks && notification == LBN_SELCHANGE) {
            const auto selection = SendMessageW(child(tracks),LB_GETCURSEL,0,0);
            const auto project = app.services().projects->state().project;
            if (selection >= 0 && static_cast<std::size_t>(selection) < project->tracks.size()) {
                selected_track = project->tracks[static_cast<std::size_t>(selection)].id;
                first_track = static_cast<std::size_t>(selection); track_offset=0;
                const bool armed = app.armed_track() && *selected_track == *app.armed_track();
                SetWindowTextW(child(arm_button),armed ? L"Disarm" : L"Arm track");
                EnableWindow(child(arm_button),project->tracks[static_cast<std::size_t>(selection)].kind == TrackKind::audio && !app.recording());
                SetWindowTextW(child(rename_edit),wide(project->tracks[static_cast<std::size_t>(selection)].name).c_str());
                InvalidateRect(window,nullptr,FALSE);
            }
            return;
        }
        switch (id) {
        case play: app.play(); break; case pause: app.pause(); refresh_models(); break; case stop: app.stop(); refresh_models(); break;
        case arm_button:
            if (selected_track) app.arm_track(app.armed_track() == selected_track ? std::nullopt : selected_track);
            refresh_models(); break;
        case monitor_button: app.monitoring(!app.monitoring()); refresh_models(); break;
        case record_button:
            if (app.recording()) (void)app.stop_recording();
            else {
                if (app.path().empty() && !save_current(false)) break;
                auto audio_folder = std::filesystem::absolute(app.path()).parent_path()/L"Media";
                std::filesystem::create_directories(audio_folder);
                app.start_recording(audio_folder/("Take-"+new_id().value+".wav"));
            }
            refresh_models(); break;
        case previous: app.musical().previous_section(); break; case next: app.musical().next_section(); break;
        case loop:
            if (app.services().transport->state().loop) app.musical().clear_loop();
            else if (auto section = app.musical().state().current_section) app.musical().loop_section(section->id);
            break;
        case undo: app.undo(); refresh_models(); break;
        case redo: app.redo(); refresh_models(); break;
        case rename: {
            const auto selection = SendMessageW(child(tracks),LB_GETCURSEL,0,0); const auto project = app.services().projects->state().project;
            if (selection >= 0 && static_cast<std::size_t>(selection) < project->tracks.size()) app.rename_track(project->tracks[static_cast<std::size_t>(selection)].id,narrow(control_text(child(rename_edit))));
            refresh_models(); break;
        }
        case new_project_button: if (discard()) {
            const auto selected = pick(window,true,false,studio.projects(),L"Untitled.mrsproject");
            if (!selected.empty()) {
                const auto path = project_folder_file(selected);
                if (std::filesystem::exists(path)) throw std::runtime_error("This project already exists. Choose a new name.");
                app.new_project(prefs.rate,narrow(path.stem().wstring())); app.save_project(path); recent_project();
                fit_view = true; view_start = 0; first_track = 0; track_offset=0; refresh_models(); restore_audio();
            }
        } break;
        case import_batch: {
            if (app.path().empty() && !save_current(false)) break;
            auto paths = pick_wavs(window);
            if (!paths.empty()) { app.import_wavs(paths); fit_view = true; refresh_models(); }
            break;
        }
        case add_track: selected_track = app.add_audio_track("Audio "+std::to_string(app.services().projects->state().project->tracks.size()+1)); first_track = app.services().projects->state().project->tracks.size()-1; refresh_models(); break;
        case add_bus_button: selected_track = app.add_bus("Bus "+std::to_string(mix_tracks().size()+1)); first_mix_track = mix_tracks().size()-1; refresh_models(); break;
        case delete_track:
            if (selected_track && MessageBoxW(window,L"Delete the selected track and its clips? Undo restores them.",L"Moon River Studio",MB_YESNO | MB_ICONQUESTION) == IDYES) {
                app.remove_track(*selected_track); refresh_models();
            }
            break;
        case track_up: case track_down: {
            const auto p = app.services().projects->state().project;
            auto it = std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t) { return selected_track && t.id == *selected_track; });
            if (it != p->tracks.end()) {
                auto index = static_cast<std::size_t>(it-p->tracks.begin());
                if (id == track_up && index > 0) --index;
                else if (id == track_down && index+1 < p->tracks.size()) ++index;
                app.reorder_track(it->id,index); first_track = index; track_offset=0; refresh_models();
            }
            break;
        }
        case split_clip_button:
            if (selected_clip) { (void)app.split_clip(*selected_clip,app.services().transport->state().sample); refresh_models(); }
            break;
        case delete_clip_button:
            if (selected_clip) { app.remove_clip(*selected_clip); refresh_models(); }
            break;
        case snap_button:
            snap = !snap; SetWindowTextW(child(snap_button),snap ? L"Snap 1/16" : L"Snap off"); break;
        case zoom_in: fit_view = false; visible_seconds = std::max(0.25,visible_seconds/2); break;
        case zoom_out: fit_view = false; visible_seconds = std::min(86400.0,visible_seconds*2); break;
        case zoom_fit: fit_view = true; view_start = 0; break;
        case open: if (discard()) { auto path = pick(window,false,false,studio.projects()); if (!path.empty()) { app.open_project(path); recent_project(); refresh_models(); restore_audio(); } } break;
        case import: if (discard()) { auto path = pick(window,false,true); if (!path.empty()) { app.import_wav(path); refresh_models(); restore_audio(); (void)save_current(false); } } break;
        case demo: if (discard()) { app.demo(); refresh_models(); restore_audio(); } break;
        case save: (void)save_current(false); break; case save_as: (void)save_current(true); break;
        case audio_settings: show_settings(); break;
        case studio_folder_button:
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",studio.root.c_str(),nullptr,nullptr,SW_SHOWNORMAL)) <= 32)
                throw std::runtime_error("Cannot open studio folder");
            break;
        case files_exit: PostMessageW(window,WM_CLOSE,0,0); break;
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
        if (smoke) ++button_paints;
        bool selected = item.CtlID==browser_toggle?browser_visible:item.CtlID >= nav_arrange && item.CtlID <= nav_mix && item.CtlID-nav_arrange == static_cast<UINT>(app.workspace());
        const bool rec = item.CtlID == record_button && app.recording();
        fill(item.hDC,item.rcItem,rec ? RGB(148,46,46) : selected ? accent : (item.itemState & ODS_SELECTED) ? border : RGB(46,50,54));
        const auto edge=CreateSolidBrush(border); FrameRect(item.hDC,&item.rcItem,edge); DeleteObject(edge);
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
        if (fit_view) { visible_seconds = static_cast<double>(end)/project->sample_rate; view_start = 0; }
        const int width = rect.right-rect.left;
        const auto start_tick = std::max<Tick>(0,context.tick-8*ppq);
        const auto pixel = [](double x) { return static_cast<int>(std::clamp(x,-100000.0,100000.0)); };
        const auto x_sample = [&](Sample sample) { return rect.left + pixel((static_cast<double>(sample)/project->sample_rate-view_start)/visible_seconds*width); };
        const auto x_tick = [&](Tick tick) {
            if (moving) return rect.left + pixel(static_cast<double>(tick-start_tick)/(16*ppq)*width);
            return x_sample(time.to_samples(tick));
        };
        fill(dc,rect,panel); const int saved = SaveDC(dc); IntersectClipRect(dc,rect.left,rect.top,rect.right,rect.bottom);
        const int top = rect.top;
        if (moving) text(dc,rect.left+s(12),top+s(4),width-s(24),s(30),L"Chord track — shared transport",normal,muted);
        int last_label = rect.left-s(64), last_grid = rect.left-s(8);
        for (int bar = 1; bar <= 256; ++bar) {
            const auto tick = time.to_ticks({bar,1,0}); const int x = x_tick(tick);
            if (x > rect.right) break; if (x < rect.left) continue;
            const auto next_tick=time.to_ticks({bar+1,1,0});
            if (x_tick(next_tick)-x >= s(48)) for (auto beat=tick+ppq; beat<next_tick; beat+=ppq) {
                const auto bx=x_tick(beat); if (bx >= rect.left && bx < rect.right) line(dc,bx,top+s(32),bx,rect.bottom,RGB(63,67,72));
            }
            if (x-last_grid >= s(8)) { line(dc,x,top+s(32),x,rect.bottom); last_grid = x; }
            if (x-last_label >= s(64)) {
                text(dc,x+s(6),top+s(4),s(58),s(22),std::to_wstring(bar),normal,muted); last_label = x;
            }
        }
        for (const auto& c : project->chords) {
            const bool compact = !moving;
            RECT block{x_tick(c.start)+1,top+s(compact ? 36 : 76),x_tick(c.end)-1,top+s(compact ? 56 : 138)};
            if (block.right <= rect.left || block.left >= rect.right) continue;
            fill(dc,block,context.current_chord && context.current_chord->id == c.id ? accent : RGB(46,48,75));
            text(dc,block.left+s(8),block.top,block.right-block.left-s(16),block.bottom-block.top,wide(c.symbol),moving ? big : compact ? normal : heading);
        }
        for (const auto& section : project->sections) {
            const auto color = RGB((section.color >> 16)&255,(section.color >> 8)&255,section.color&255);
            const bool compact = !moving;
            RECT block{x_tick(section.start)+1,top+s(compact ? 58 : 148),x_tick(section.end)-1,top+s(compact ? 80 : 183)}; fill(dc,block,color);
            text(dc,block.left+s(8),block.top,block.right-block.left-s(16),block.bottom-block.top,wide(section.name));
        }
        const auto row_clip=SaveDC(dc);
        if (!moving) IntersectClipRect(dc,rect.left,audio_top(),rect.right,arrangement_bottom());
        if (!moving) {
            int y = audio_top()-track_offset_pixels();
            for (std::size_t ti = first_track; ti < project->tracks.size(); ++ti) {
                const auto& track = project->tracks[ti];
                if (y+s(30) > arrangement_bottom()) break;
                const int row_y=y;
                if (selected_track && track.id == *selected_track) fill(dc,{rect.left,y,rect.right,y+row_height()-s(4)},RGB(63,68,74));
                text(dc,rect.left+s(12),y,width-s(24),s(24),(app.track_armed(track.id) ? L"[R] " : L"") + wide(track.name),normal,muted); y += s(26);
                for (const auto& stored_clip : project->clips) {
                    const auto clip = drag && drag->original.id == stored_clip.id ? drag->preview : stored_clip;
                    if (clip.track != track.id) continue;
                    RECT block{x_sample(clip.start)+1,y,x_sample(clip.start+clip.length)-1,y+clip_height()};
                    if (block.right <= rect.left || block.left >= rect.right) continue;
                    fill(dc,block,RGB(44,83,143));
                    const auto peaks = app.waveform(clip.source);
                    if (peaks) {
                        const auto channel_count = std::min<std::uint32_t>(peaks->channels(),2);
                        const int left = std::max(block.left,rect.left), right = std::min(block.right,rect.right);
                        auto peak_pen = CreatePen(PS_SOLID,1,RGB(155,194,239)); auto previous_pen = SelectObject(dc,peak_pen);
                        for (std::uint32_t c = 0; c < channel_count; ++c) {
                            const int ch = clip_height()/static_cast<int>(channel_count), mid = y+static_cast<int>(c)*ch+ch/2;
                            line(dc,left,mid,right,mid,RGB(60,89,106));
                            for (int x = left; x < right; ++x) {
                                const auto begin = clip.source_offset+static_cast<Sample>((view_start+static_cast<double>(x-rect.left)/width*visible_seconds)*project->sample_rate)-clip.start;
                                const auto finish = clip.source_offset+static_cast<Sample>((view_start+static_cast<double>(x+1-rect.left)/width*visible_seconds)*project->sample_rate)-clip.start;
                                const auto peak = peaks->range(begin,std::max(begin+1,finish),c);
                                MoveToEx(dc,x,mid-static_cast<int>(std::clamp(peak.maximum,-1.0f,1.0f)*(ch/2-2)),nullptr);
                                LineTo(dc,x,mid-static_cast<int>(std::clamp(peak.minimum,-1.0f,1.0f)*(ch/2-2))+1);
                            }
                        }
                        SelectObject(dc,previous_pen); DeleteObject(peak_pen);
                    } else text(dc,block.left+s(8),y,block.right-block.left-s(16),clip_height(),L"Building waveform...",normal,muted);
                    if (selected_clip && clip.id == *selected_clip) {
                        line(dc,block.left,block.top,block.right,block.top,amber);
                        line(dc,block.left,block.bottom-1,block.right,block.bottom-1,amber);
                        fill(dc,{block.left,block.top,block.left+s(4),block.bottom},amber);
                        fill(dc,{block.right-s(4),block.top,block.right,block.bottom},amber);
                    }
                    text(dc,block.left+s(7),block.top,block.right-block.left-s(14),s(18),wide(clip.name),normal,ink);
                }
                y=row_y+row_height();
                line(dc,rect.left,y-s(2),rect.right,y-s(2),border);
            }
        }
        if (!moving && app.recording()) for (const auto& armed : app.armed_tracks()) {
            const auto track = std::find_if(project->tracks.begin(),project->tracks.end(),[&](const auto& t) { return t.id == armed; });
            const auto row = static_cast<std::size_t>(track-project->tracks.begin());
            if (track != project->tracks.end() && row >= first_track) {
                const auto frames = static_cast<Sample>(app.recording_status().frames);
                const auto start = app.recording_start();
                const int y = audio_top()+static_cast<int>(row-first_track)*row_height()+s(26)-track_offset_pixels();
                RECT take{x_sample(start),y,x_sample(start+frames),y+clip_height()};
                if (take.right > take.left) { fill(dc,take,RGB(112,43,43)); text(dc,take.left+s(8),y,take.right-take.left-s(16),s(24),L"Recording..."); }
            }
        }
        RestoreDC(dc,row_clip);
        const auto position = moving ? x_tick(context.tick) : x_sample(context.transport.sample);
        line(dc,position,top+s(32),position,arrangement_bottom(),RGB(206,218,230));
        RestoreDC(dc,saved);
    }
    RECT mix_area() const {
        const auto h=canvas.bottom-canvas.top;
        const int mixer_height=std::clamp(static_cast<int>(h*0.65),s(380),s(460));
        return {s(8),std::max<LONG>(canvas.top+s(120),canvas.bottom-mixer_height),canvas.right,canvas.bottom};
    }
    std::vector<Track> mix_tracks() const {
        std::vector<Track> result;
        for (const auto& t : app.services().projects->state().project->tracks) if (t.kind != TrackKind::midi) result.push_back(t);
        return result;
    }
    RECT mix_strip(std::size_t index, bool master = false) const {
        const auto area = mix_area();
        const int x = master ? area.right-s(146) : area.left+s(8)+static_cast<int>(index)*s(120);
        return {x,area.top+s(44),x+s(112),area.bottom-s(8)};
    }
    std::size_t visible_insert_slots(RECT r) const {return r.bottom-r.top>=s(320)?3:1;}
    int mix_controls_top(RECT r) const {return s(63+static_cast<int>(visible_insert_slots(r))*23);}
    RECT mix_control(RECT r, int row) const {
        const auto top=mix_controls_top(r);
        if (row == 34) return {r.left+s(12),r.top+top,r.left+s(48),r.bottom-s(78)};
        if (row == 68) return {r.left+s(60),r.top+top-s(2),r.right-s(8),r.top+top+s(20)};
        if (row == 164) return {r.left+s(8),r.bottom-s(54),r.right-s(8),r.bottom-s(30)};
        if (row == 220) return {r.left+s(8),r.bottom-s(28),r.right-s(8),r.bottom-s(4)};
        return {r.left+s(10),r.top+s(row),r.right-s(10),r.top+s(row+22)};
    }
    RECT insert_control(RECT r) const { return {r.left+s(8),r.top+s(28),r.right-s(8),r.top+s(52)}; }
    RECT insert_slot(RECT r,std::size_t index) const {return {r.left+s(8),r.top+s(54+static_cast<int>(index)*23),r.right-s(8),r.top+s(75+static_cast<int>(index)*23)};}
    RECT output_control(RECT r, bool bus) const { auto result = mix_control(r,164); if (bus) result.right -= s(36); return result; }
    RECT remove_bus_control(RECT r) const { auto result = mix_control(r,164); result.left = result.right-s(32); return result; }
    RECT regulator_handle(RECT r,float value,bool vertical,bool pan) const {
        const float f=pan?(value+1)*0.5f:fader_position(value);
        if(vertical){const auto y=r.bottom-static_cast<int>(f*(r.bottom-r.top));return {r.left,y-s(6),r.right,y+s(6)};}
        const auto x=r.left+static_cast<int>(f*(r.right-r.left));return {x-s(5),r.top,x+s(5),r.bottom};
    }
    static float fader_position(float gain) { return gain <= 0 ? 0 : std::clamp((20*std::log10(gain)+60)/72,0.0f,1.0f); }
    static float fader_gain(float position) { return position <= 0 ? 0 : std::pow(10.0f,(-60+72*position)/20); }
    std::wstring gain_text(float gain) const {
        if (gain <= 0) return L"-inf dB";
        std::wostringstream out; out << std::fixed << std::setprecision(1) << 20*std::log10(gain) << L" dB"; return out.str();
    }
    void paint_mixer(HDC dc) {
        const auto area = mix_area(); fill(dc,area,panel); line(dc,area.left,area.top,area.right,area.top,accent);
        text(dc,area.left+s(112),area.top+s(4),area.right-area.left-s(120),s(30),L"Mixer  |  Outputs & sends  |  Shift+wheel: channels",normal,muted);
        const auto all = mix_tracks();
        first_mix_track = all.empty() ? 0 : std::min(first_mix_track,all.size()-1);
        const int saved = SaveDC(dc); IntersectClipRect(dc,area.left,area.top,area.right,area.bottom);
        const auto draw = [&](RECT r, const Track* track, audio::StereoPeak peak) {
            fill(dc,r,track && selected_track == track->id ? RGB(51,51,58) : RGB(37,37,37));
            auto mix = track ? track->mix : Track::Mix{};
            float gain = track ? mix.gain : app.services().projects->state().project->master_gain;
            if (mix_drag && ((track && mix_drag->track == track->id) || (!track && !mix_drag->track))) { mix = mix_drag->mix; gain = track ? mix.gain : mix_drag->master; }
            text(dc,r.left+s(8),r.top,r.right-r.left-s(16),s(28),track ? (track->kind == TrackKind::bus ? L"[BUS] " : L"")+wide(track->name) : L"MASTER",normal,track && track->kind != TrackKind::bus ? ink : amber);
            const auto effects=track ? track->inserts.size() : app.services().projects->state().project->master_inserts.size();
            const auto insert=insert_control(r); fill(dc,insert,border); text(dc,insert.left,insert.top,insert.right-insert.left,insert.bottom-insert.top,L"Inserts ("+std::to_wstring(effects)+L")...");
            const auto& chain=track?track->inserts:app.services().projects->state().project->master_inserts;
            for(std::size_t i=0;i<std::min(visible_insert_slots(r),chain.size());++i){auto slot=insert_slot(r,i);fill(dc,slot,RGB(29,33,38));text(dc,slot.left+s(3),slot.top,slot.right-slot.left-s(6),slot.bottom-slot.top,chain[i].kind==InsertKind::vst3?wide(chain[i].plugin_name):chain[i].kind==InsertKind::cab_ir?L"Cab IR":chain[i].kind==InsertKind::channel_eq?L"Channel EQ":L"Gain",normal,chain[i].bypass?muted:ink);}
            if(plugin_drag&&plugin_drop_valid&&((track&&plugin_drop_track==track->id)||(!track&&!plugin_drop_track))){line(dc,r.left,r.top,r.right,r.top,accent);line(dc,r.left,r.top,r.left,r.bottom,accent);line(dc,r.right-1,r.top,r.right-1,r.bottom,accent);}
            const auto g = mix_control(r,34); fill(dc,g,RGB(26,26,26));
            const int gy = g.bottom-static_cast<int>(fader_position(gain)*(g.bottom-g.top));
            line(dc,(g.left+g.right)/2,g.top+s(3),(g.left+g.right)/2,g.bottom-s(3),border);
            fill(dc,{g.left,gy-s(6),g.right,gy+s(6)},RGB(187,194,201));
            line(dc,g.left,gy,g.right,gy,RGB(81,86,92));
            text(dc,r.left+s(8),r.bottom-s(78),r.right-r.left-s(16),s(22),gain_text(gain));
            if (track) {
                const auto p = mix_control(r,68); fill(dc,p,border);
                line(dc,p.left,(p.top+p.bottom)/2,p.right,(p.top+p.bottom)/2,muted);
                fill(dc,regulator_handle(p,mix.pan,false,true),accent);
                const auto top=mix_controls_top(r);RECT m{r.left+s(60),r.top+top+s(24),r.right-s(8),r.top+top+s(44)}, solo{m.left,r.top+top+s(46),m.right,r.top+top+s(68)};
                fill(dc,m,mix.mute ? RGB(143,67,56) : border); fill(dc,solo,mix.solo ? RGB(132,105,44) : border);
                text(dc,m.left,m.top,m.right-m.left,m.bottom-m.top,L"Mute"); text(dc,solo.left,solo.top,solo.right-solo.left,solo.bottom-solo.top,L"Solo");
            }
            for (int c=0; c<2; ++c) {
                const float v = c == 0 ? peak.left : peak.right;
                RECT meter{r.left+s(64+c*18),std::min(r.top+mix_controls_top(r)+s(72),r.bottom-s(86)),r.left+s(74+c*18),r.bottom-s(80)}; fill(dc,meter,RGB(22,22,22));
                const float fraction = v > 0 ? std::clamp((20*std::log10(v)+60)/60,0.0f,1.0f) : 0;
                fill(dc,{meter.left,meter.bottom-static_cast<int>(fraction*(meter.bottom-meter.top)),meter.right,meter.bottom},v >= 1 ? RGB(230,70,60) : v > 0.7f ? amber : RGB(100,178,126));
            }
            if (track) {
                std::wstring destination = L"Master";
                for (const auto& bus : all) if (track->output == bus.id) destination = wide(bus.name);
                if (!track->hardware_outputs.empty()) {
                    destination = L"HW "; for (const auto c : track->hardware_outputs) { if (destination != L"HW ") destination += L"/"; destination += std::to_wstring(c+1); }
                }
                const auto route = output_control(r,track->kind == TrackKind::bus); fill(dc,route,border);
                text(dc,route.left,route.top,route.right-route.left,route.bottom-route.top,L"Out: "+destination);
                if (track->kind == TrackKind::bus) { auto remove = remove_bus_control(r); fill(dc,remove,border); text(dc,remove.left,remove.top,remove.right-remove.left,remove.bottom-remove.top,L"Del"); }
                const auto sends = mix_control(r,220); fill(dc,sends,border);
                text(dc,sends.left,sends.top,sends.right-sends.left,sends.bottom-sends.top,L"Sends ("+std::to_wstring(track->sends.size())+L")...");
            } else {
                const auto route = output_control(r,false); fill(dc,route,border);
                std::wstring label = L"Out: Default";
                if (!app.services().projects->state().project->master_outputs.empty()) {
                    label = L"Out: "; for (auto c : app.services().projects->state().project->master_outputs) { if (label != L"Out: ") label += L"/"; label += std::to_wstring(c+1); }
                }
                text(dc,route.left,route.top,route.right-route.left,route.bottom-route.top,label);
            }
        };
        for (std::size_t i=first_mix_track; i<all.size(); ++i) { const auto strip = mix_strip(i-first_mix_track); if (strip.right > area.right-s(154)) break; draw(strip,&all[i],mix_meters.tracks[i]); }
        draw(mix_strip(0,true),nullptr,mix_meters.master); RestoreDC(dc,saved);
    }
    void choose_sends(const Track& track, POINT point) {
        const bool editable = !app.recording() && app.engine()->state().playback != PlaybackState::playing;
        const auto project = app.services().projects->state().project;
        const auto menu = CreatePopupMenu(); if (!menu) throw std::runtime_error("Cannot create sends menu");
        struct Action { int kind; std::size_t index; float gain; std::optional<Id> bus; };
        std::vector<Action> actions;
        const auto item = [&](HMENU target, const wchar_t* label, Action action, bool enabled=true, bool checked=false) {
            actions.push_back(std::move(action)); AppendMenuW(target,MF_STRING | (enabled ? 0 : MF_GRAYED) | (checked ? MF_CHECKED : 0),actions.size(),label);
        };
        for (std::size_t i=0; i<track.sends.size(); ++i) {
            const auto& send = track.sends[i]; const auto sub = CreatePopupMenu();
            std::wstring name = L"Return"; for (const auto& bus : project->tracks) if (bus.id == send.bus) name = wide(bus.name);
            const auto label = name+L" | "+(send.pre_fader ? L"Pre " : L"Post ")+gain_text(send.gain);
            AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(sub),label.c_str());
            item(sub,L"Pre-fader",{1,i,0,{}},editable,send.pre_fader); item(sub,L"Post-fader",{2,i,0,{}},editable,!send.pre_fader);
            for (const int db : {-60,-24,-18,-12,-6,0,6,12}) { const auto level = std::pow(10.0f,static_cast<float>(db)/20); const auto value = std::to_wstring(db)+L" dB"; item(sub,value.c_str(),{0,i,level,{}},true,std::abs(level-send.gain)<0.00001f); }
            item(sub,L"Off (-inf)",{0,i,0,{}},true,send.gain == 0); item(sub,L"Remove send",{3,i,0,{}},editable);
        }
        const auto add = CreatePopupMenu(); AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(add),L"Add send to bus / return");
        bool available{};
        for (const auto& bus : project->tracks) if (bus.kind == TrackKind::bus) {
            auto candidate = *project; auto sends = track.sends; sends.push_back({bus.id,1,false}); bool valid=true;
            try { SetTrackSends{track.id,sends}.apply(candidate); candidate.validate(); } catch (const std::invalid_argument&) { valid=false; }
            item(add,wide(bus.name).c_str(),{4,0,1,bus.id},editable && valid); available=true;
        }
        if (!available) AppendMenuW(add,MF_STRING | MF_GRAYED,0,L"Create a bus / return first");
        item(menu,L"Create return and send",{5,0,1,{}},editable && track.sends.size()<8);
        ClientToScreen(window,&point); const auto choice = TrackPopupMenu(menu,TPM_RETURNCMD | TPM_NONOTIFY,point.x,point.y,0,window,nullptr); DestroyMenu(menu);
        if (!choice || choice>actions.size()) return;
        const auto action = actions[choice-1]; auto sends = track.sends;
        if (action.kind == 0) app.set_send_gain(track.id,action.index,action.gain);
        else {
            if (action.kind == 1 || action.kind == 2) sends[action.index].pre_fader = action.kind == 1;
            if (action.kind == 3) sends.erase(sends.begin()+static_cast<std::ptrdiff_t>(action.index));
            if (action.kind == 4) sends.push_back({*action.bus,1,false});
            if (action.kind == 5) (void)app.add_return_send(track.id,"Return "+std::to_string(mix_tracks().size()+1));
            else app.set_track_sends(track.id,std::move(sends));
        }
        refresh_models();
    }
    RECT mini_rect(std::size_t index) const { const int y = audio_top()+(static_cast<int>(index)-static_cast<int>(first_track))*row_height()-track_offset_pixels(); return {s(8),y,canvas.left-s(4),y+row_height()-s(4)}; }
    void paint_minis(HDC dc) {
        const auto project = app.services().projects->state().project;
        std::size_t channel{};
        const int saved = SaveDC(dc); IntersectClipRect(dc,s(8),audio_top(),canvas.left,arrangement_bottom());
        for (std::size_t i=0; i<project->tracks.size(); ++i) {
            const auto& track = project->tracks[i]; const auto meter_index = channel;
            if (track.kind != TrackKind::midi) ++channel;
            if (i<first_track) continue; const auto r = mini_rect(i); if (r.top>=arrangement_bottom()) break;
            fill(dc,r,selected_track == track.id ? RGB(75,81,88) : panel);
            fill(dc,{r.left,r.top,r.left+s(5),r.bottom},accent);
            auto mix = track.mix; if (mix_drag && mix_drag->track == track.id) mix = mix_drag->mix;
            text(dc,r.left+s(10),r.top,r.right-r.left-s(180),s(22),wide(track.name));
            if (track.kind == TrackKind::midi) continue;
            const RECT mute_rect{r.right-s(160),r.top,r.right-s(126),r.top+s(22)}, solo_rect{r.right-s(122),r.top,r.right-s(88),r.top+s(22)};
            fill(dc,mute_rect,mix.mute ? RGB(143,67,56) : border); fill(dc,solo_rect,mix.solo ? RGB(132,105,44) : border);
            text(dc,mute_rect.left,mute_rect.top,mute_rect.right-mute_rect.left,s(22),L"M"); text(dc,solo_rect.left,solo_rect.top,solo_rect.right-solo_rect.left,s(22),L"S");
            if (track.kind == TrackKind::audio) {
                const RECT arm{r.right-s(84),r.top,r.right-s(50),r.top+s(22)}, monitor{r.right-s(46),r.top,r.right-s(8),r.top+s(22)};
                fill(dc,arm,app.track_armed(track.id) ? RGB(160,55,55) : border); fill(dc,monitor,track.input_monitor ? accent : border);
                text(dc,arm.left,arm.top,arm.right-arm.left,s(22),L"R"); text(dc,monitor.left,monitor.top,monitor.right-monitor.left,s(22),L"I");
            }
            const RECT gain{r.left+s(8),r.top+s(26),r.right-s(44),r.top+s(44)}; fill(dc,gain,border);
            fill(dc,regulator_handle(gain,mix.gain,false,false),amber);
            text(dc,gain.left,gain.top,gain.right-gain.left,gain.bottom-gain.top,gain_text(mix.gain));
            const auto peak = mix_meters.tracks[meter_index];
            const bool stereo=app.stereo_track(track.id);
            for (int c=0; c<(stereo ? 2 : 1); ++c) {
                const float v=stereo ? (c == 0 ? peak.left : peak.right) : std::max(peak.left,peak.right);
                RECT meter{gain.left+s(stereo ? 14 : 0),r.top+s(48+c*9),gain.right,r.top+s(55+c*9)};
                fill(dc,meter,RGB(22,22,22)); const auto fraction=v > 0 ? std::clamp((20*std::log10(v)+60)/60,0.0f,1.0f) : 0;
                fill(dc,{meter.left,meter.top,meter.left+static_cast<int>(fraction*(meter.right-meter.left)),meter.bottom},v>=1 ? RGB(230,70,60) : RGB(100,178,126));
                if (stereo) text(dc,gain.left,r.top+s(44+c*9),s(12),s(16),c == 0 ? L"L" : L"R");
            }
            const int cx = r.right-s(23), cy = r.top+s(41), radius=s(15);
            const auto brush = CreateSolidBrush(border); const auto old = SelectObject(dc,brush); Ellipse(dc,cx-radius,cy-radius,cx+radius,cy+radius); SelectObject(dc,old); DeleteObject(brush);
            const double angle = static_cast<double>(mix.pan)*2.2; line(dc,cx,cy,cx+static_cast<int>(std::sin(angle)*radius),cy-static_cast<int>(std::cos(angle)*radius),accent);
            if (track.kind == TrackKind::audio) {
                RECT input{r.left+s(8),r.top+s(68),r.right-s(8),r.top+s(88)}; fill(dc,input,border);
                text(dc,input.left,input.top,input.right-input.left,input.bottom-input.top,track.input == -2 ? L"Input: default" : track.input == -1 ? L"Input: off" : (track.input_stereo ? L"Stereo "+std::to_wstring(track.input+1)+L"/"+std::to_wstring(track.input+2) : L"Mono "+std::to_wstring(track.input+1)));
            }
        }
        RestoreDC(dc,saved);
    }
    bool mini_down(POINT point, bool reset=false) {
        if (point.y<audio_top() || point.y>=arrangement_bottom()) return false;
        const auto project = app.services().projects->state().project;
        for (std::size_t i=first_track; i<project->tracks.size(); ++i) {
            const auto r = mini_rect(i); if (point.y >= arrangement_bottom() || !PtInRect(&r,point)) continue;
            const auto track = project->tracks[i];
            if (selected_track != track.id) { selected_track = track.id; refresh_models(); }
            if (track.kind == TrackKind::midi) return true;
            const RECT mute_rect{r.right-s(160),r.top,r.right-s(126),r.top+s(22)}, solo_rect{r.right-s(122),r.top,r.right-s(88),r.top+s(22)};
            if (PtInRect(&mute_rect,point) || PtInRect(&solo_rect,point)) {
                auto mix=track.mix; if (PtInRect(&mute_rect,point)) mix.mute=!mix.mute; else mix.solo=!mix.solo;
                app.set_track_mix(track.id,mix); refresh_mix_controls(); return true;
            }
            if (track.kind == TrackKind::audio) {
                const RECT arm{r.right-s(84),r.top,r.right-s(50),r.top+s(22)}, monitor{r.right-s(46),r.top,r.right-s(8),r.top+s(22)};
                if (PtInRect(&arm,point)) { app.set_track_armed(track.id,!app.track_armed(track.id)); refresh_models(); return true; }
                if (PtInRect(&monitor,point)) { app.set_track_monitoring(track.id,!track.input_monitor); refresh_mix_controls(); return true; }
            }
            const RECT g{r.left+s(8),r.top+s(26),r.right-s(44),r.top+s(44)}, p{r.right-s(40),r.top+s(24),r.right-s(6),r.top+s(57)};
            if (PtInRect(&g,point) || PtInRect(&p,point)) {
                const bool pan = PtInRect(&p,point); auto mix = track.mix;
                const auto grabbed=regulator_handle(g,mix.gain,false,false);if(!pan&&!PtInRect(&grabbed,point))return true;
                if (reset) { if (pan) mix.pan=0; else mix.gain=1; app.set_track_mix(track.id,mix); return true; }
                mix_drag = MixDrag{track.id,mix,project->master_gain,pan,pan ? p : g,false}; const auto handle=regulator_handle(g,mix.gain,false,false);if(!pan && !PtInRect(&handle,point)){mix_drag.reset();return true;}
                mix_drag->origin=point;mix_drag->initial=pan?(mix.pan+1)*0.5f:fader_position(mix.gain);SetCapture(window);return true;
            }
            if (track.kind == TrackKind::audio && point.y>=r.top+s(68) && !app.recording() && app.engine()->state().playback != PlaybackState::playing) {
                const auto names=app.input_names(); const auto menu=CreatePopupMenu();
                std::vector<std::pair<int,bool>> choices{{-2,false},{-1,false}};
                AppendMenuW(menu,MF_STRING | (track.input == -2 ? MF_CHECKED : 0),1,L"Default mono input from Audio settings");
                AppendMenuW(menu,MF_STRING | (track.input == -1 ? MF_CHECKED : 0),2,L"Off");
                const auto add=[&](int input,bool stereo,const std::wstring& label) {
                    choices.emplace_back(input,stereo); AppendMenuW(menu,MF_STRING | (track.input == input && track.input_stereo == stereo ? MF_CHECKED : 0),choices.size(),label.c_str());
                };
                for (std::size_t n=0; n<names.size(); ++n) {
                    add(static_cast<int>(n),false,L"Mono "+std::to_wstring(n+1)+L": "+wide(names[n]));
                    if (n+1<names.size()) add(static_cast<int>(n),true,L"Stereo "+std::to_wstring(n+1)+L"/"+std::to_wstring(n+2)+L": "+wide(names[n])+L" / "+wide(names[n+1]));
                }
                ClientToScreen(window,&point); const auto choice=TrackPopupMenu(menu,TPM_RETURNCMD | TPM_NONOTIFY,point.x,point.y,0,window,nullptr); DestroyMenu(menu);
                if (choice && choice <= choices.size()) { const auto [input,stereo]=choices[choice-1]; app.set_track_input(track.id,input,stereo); refresh_models(); }
            }
            return true;
        }
        return false;
    }
    void choose_bus(const Track* track, POINT point) {
        if (app.recording() || app.engine()->state().playback == PlaybackState::playing) return;
        const auto project = app.services().projects->state().project;
        const auto menu = CreatePopupMenu(); if (!menu) throw std::runtime_error("Cannot create output menu");
        std::vector<std::optional<Id>> destinations{std::nullopt};
        const auto& current = track ? track->hardware_outputs : project->master_outputs;
        AppendMenuW(menu,MF_STRING | (current.empty() && (!track || !track->output) ? MF_CHECKED : 0),1,track ? L"Master" : L"Default output pair");
        if (track) for (const auto& bus : project->tracks) if (bus.kind == TrackKind::bus) {
            auto candidate = *project; bool valid = true;
            try { SetTrackOutput{track->id,bus.id}.apply(candidate); candidate.validate(); } catch (const std::invalid_argument&) { valid = false; }
            destinations.push_back(bus.id);
            AppendMenuW(menu,MF_STRING | (valid ? 0 : MF_GRAYED) | (track->output == bus.id ? MF_CHECKED : 0),static_cast<UINT_PTR>(destinations.size()),wide(bus.name).c_str());
        }
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        const auto names = app.output_names(); const auto active = app.active_outputs();
        std::vector<std::vector<int>> hardware;
        const auto add = [&](std::vector<int> channels, std::wstring label) {
            const bool available = std::all_of(channels.begin(),channels.end(),[&](int c) { return std::find(active.begin(),active.end(),c) != active.end(); });
            hardware.push_back(channels);
            AppendMenuW(menu,MF_STRING | (available ? 0 : MF_GRAYED) | (current == channels ? MF_CHECKED : 0),1000+hardware.size(),label.c_str());
        };
        for (std::size_t i=0; i<names.size(); ++i) {
            add({static_cast<int>(i)},L"Mono "+std::to_wstring(i+1)+L": "+wide(names[i]));
            if (i+1<names.size()) add({static_cast<int>(i),static_cast<int>(i+1)},L"Stereo "+std::to_wstring(i+1)+L"/"+std::to_wstring(i+2)+L": "+wide(names[i])+L" / "+wide(names[i+1]));
        }
        // Non-adjacent or reversed selected pairs retain the physical order
        // chosen in Audio settings, rather than depending on stream indices.
        for (std::size_t i=0; i+1<active.size(); i+=2) if (active[i+1] != active[i]+1)
            add({active[i],active[i+1]},L"Selected stereo "+std::to_wstring(active[i]+1)+L"/"+std::to_wstring(active[i+1]+1));
        if (names.empty()) AppendMenuW(menu,MF_STRING | MF_GRAYED,0,L"Connect an audio device for physical outputs");
        ClientToScreen(window,&point);
        const auto choice = TrackPopupMenu(menu,TPM_RETURNCMD | TPM_NONOTIFY,point.x,point.y,0,window,nullptr); DestroyMenu(menu);
        if (choice && choice <= destinations.size()) {
            if (track) app.set_track_output(track->id,destinations[choice-1]); else app.set_hardware_output(std::nullopt,{});
            refresh_models();
        } else if (choice > 1000 && choice <= 1000+hardware.size()) {
            app.set_hardware_output(track ? std::optional<Id>{track->id} : std::nullopt,hardware[choice-1001]); refresh_models();
        }
    }
    void mixer_down(POINT point, bool reset = false) {
        const auto all = mix_tracks();
        for (std::size_t slot=0; slot<=all.size(); ++slot) {
            const bool master = slot == all.size();
            if (!master && slot < first_mix_track) continue;
            const auto r = mix_strip(master ? 0 : slot-first_mix_track,master);
            if (!master && r.right > mix_area().right-s(154)) continue;
            if (!PtInRect(&r,point)) continue;
            const auto t = master ? nullptr : &all[slot];
            auto mix = t ? t->mix : Track::Mix{};
            if (t && selected_track != t->id) { selected_track = t->id; refresh_models(); }
            const auto insert=insert_control(r); if (PtInRect(&insert,point)) { open_fx(t ? std::optional<Id>{t->id} : std::nullopt); return; }
            const auto& chain=t?t->inserts:app.services().projects->state().project->master_inserts;for(std::size_t i=0;i<std::min(visible_insert_slots(r),chain.size());++i){const auto hit=insert_slot(r,i);if(PtInRect(&hit,point)){open_insert(t?std::optional<Id>{t->id}:std::nullopt,i);return;}}
            const auto route = output_control(r,t && t->kind == TrackKind::bus), remove = remove_bus_control(r);
            if (PtInRect(&route,point)) { choose_bus(t,point); return; }
            if (t && t->kind == TrackKind::bus && PtInRect(&remove,point)) {
                if (!app.recording() && app.engine()->state().playback != PlaybackState::playing &&
                    MessageBoxW(window,L"Delete this bus? Its inputs will use its output. Undo restores routing.",L"Moon River Studio",MB_YESNO | MB_ICONQUESTION) == IDYES) { app.remove_track(t->id); refresh_models(); }
                return;
            }
            const auto sends = mix_control(r,220);
            if (t && PtInRect(&sends,point)) { choose_sends(*t,point); return; }
            const auto g = mix_control(r,34), p = mix_control(r,68);
            if (PtInRect(&g,point) || (t && PtInRect(&p,point))) {
                const bool pan = t && PtInRect(&p,point);
                const auto grabbed=regulator_handle(pan?p:g,pan?mix.pan:t?mix.gain:app.services().projects->state().project->master_gain,!pan,pan);
                if(!PtInRect(&grabbed,point))return;
                if (reset) {
                    if (t) { if (pan) mix.pan = 0; else mix.gain = 1; app.set_track_mix(t->id,mix); }
                    else app.set_master_gain(1);
                    refresh_models(); return;
                }
                mix_drag = MixDrag{t ? std::optional<Id>{t->id} : std::nullopt,mix,app.services().projects->state().project->master_gain,pan,pan ? p : g,!pan};
                const auto handle=regulator_handle(pan?p:g,pan?mix.pan:t?mix.gain:mix_drag->master,!pan,pan);
                if(!PtInRect(&handle,point)){mix_drag.reset();return;}
                mix_drag->origin=point;mix_drag->initial=pan?(mix.pan+1)*0.5f:fader_position(t?mix.gain:mix_drag->master);SetCapture(window);return;
            }
            if (t && point.x >= r.left+s(60) && point.y >= r.top+mix_controls_top(r)+s(24) && point.y < r.top+mix_controls_top(r)+s(68)) {
                if (point.y < r.top+mix_controls_top(r)+s(45)) mix.mute = !mix.mute; else mix.solo = !mix.solo;
                app.set_track_mix(t->id,mix); refresh_models();
            }
            return;
        }
    }
    void mixer_move(POINT point) {
        if (!mix_drag) return;
        auto& d = *mix_drag;
        const float pixels=static_cast<float>(d.vertical ? d.origin.y-point.y : point.x-d.origin.x);
        const float span=static_cast<float>(d.vertical ? d.rect.bottom-d.rect.top : d.rect.right-d.rect.left);
        const float fine=(GetKeyState(VK_CONTROL)&0x8000)?0.15f:1.f;
        const float position=std::clamp(d.initial+pixels/std::max(span,static_cast<float>(s(d.pan?180:240)))*fine,0.f,1.f);
        if (d.pan) d.mix.pan = position*2-1;
        else if (d.track) d.mix.gain = fader_gain(position); else d.master = fader_gain(position);
        (void)app.preview_mix(d.track,d.mix,d.master); InvalidateRect(window,nullptr,FALSE);
    }
    void cancel_mix_drag() {
        if (!mix_drag) return;
        mix_drag.reset();
        app.cancel_mix_preview();
        ReleaseCapture(); InvalidateRect(window,nullptr,FALSE);
    }
    void paint(HDC dc) {
        RECT area{}; GetClientRect(window,&area); fill(dc,area,background);
        if(browser_visible){fill(dc,browser_area,panel);text(dc,browser_area.left+s(8),browser_area.bottom-s(26),browser_area.right-browser_area.left-s(16),s(24),vst_scan.valid()?L"Scanning…":vst_catalog.empty()?L"Scan a folder to find VST3":L"Drag an effect onto a mixer channel",normal,muted);}
        fill(dc,{0,0,area.right,s(78)},panel); line(dc,0,s(78),area.right,s(78));
        const auto& context=app.musical().state();
        std::wostringstream position; position << L"Bar " << context.transport.musical.bar << L" : " << context.transport.musical.beat
            << L"   " << std::fixed << std::setprecision(2) << static_cast<double>(context.transport.sample)/context.project->sample_rate << L" s";
        text(dc,s(700),area.bottom-s(66),area.right-s(712),s(30),position.str(),heading,ink);
        text(dc,s(910),s(8),area.right-s(922),s(28),wide(context.project->title),normal,muted);
        auto workspace = app.workspace();
        if (workspace == Workspace::arrange || workspace == Workspace::mix) {
            timeline(dc,canvas,false); paint_minis(dc);
            if (workspace == Workspace::mix) paint_mixer(dc);
        } else {
            fill(dc,canvas,panel); int y = canvas.top+s(14);
            text(dc,canvas.left+s(16),y,canvas.right-canvas.left-s(32),s(36),workspace == Workspace::mix ? L"Mixer" : L"Clip inspector",heading); y += s(52);
            if (workspace == Workspace::mix) {
                paint_mixer(dc);
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
            << L"  |  " << context.project->sample_rate << L" Hz  |  callbacks " << metrics.callbacks << L"  |  underruns " << metrics.output_underflows
            << L"  |  disk underruns " << metrics.disk_underruns << L" / errors " << metrics.disk_errors;
        std::wostringstream input_status;
        input_status << L"Input " << std::fixed << std::setprecision(1) << (metrics.input_peak > 0 ? 20*std::log10(metrics.input_peak) : -120.0f) << L" dBFS";
        if (app.recording()) input_status << L"  |  Recording " << static_cast<double>(app.recording_status().frames)/context.project->sample_rate << L" s";
        if (!app.recording_error().empty()) input_status << L"  |  " << wide(app.recording_error());
        text(dc,s(754),s(44),std::max(0L,area.right-s(932)),s(24),input_status.str(),normal,app.recording_error().empty() ? muted : RGB(242,100,100));
        if (!app.waveform_error().empty()) bottom << L"  |  waveform: " << wide(app.waveform_error());
        line(dc,0,area.bottom-s(76),area.right,area.bottom-s(76));
        text(dc,s(20),area.bottom-s(30),area.right-s(40),s(24),bottom.str(),normal,(status.phase == audio::DevicePhase::error || metrics.disk_errors || metrics.disk_underruns) ? RGB(242,100,100) : muted);
    }
    void export_preview(HWND target=nullptr) {
        if (!target) target=window;
        RECT area{}; GetClientRect(target,&area);
        const auto dc=GetDC(target), buffer=CreateCompatibleDC(dc);
        const auto bitmap=CreateCompatibleBitmap(dc,area.right,area.bottom); const auto old=SelectObject(buffer,bitmap);
        if (target==window) paint(buffer); else fill(buffer,area,panel);
        for (auto control=GetWindow(target,GW_CHILD); control; control=GetWindow(control,GW_HWNDNEXT)) {
            if (!(GetWindowLongPtrW(control,GWL_STYLE) & WS_VISIBLE)) continue;
            RECT r{}; GetWindowRect(control,&r); MapWindowPoints(nullptr,target,reinterpret_cast<POINT*>(&r),2);
            const int saved=SaveDC(buffer); IntersectClipRect(buffer,r.left,r.top,r.right,r.bottom);
            fill(buffer,r,panel);
            wchar_t class_name[32]{}; GetClassNameW(control,class_name,32);
            if (std::wstring_view(class_name) == L"Edit" || std::wstring_view(class_name) == L"ComboBox") {
                const auto edge=CreateSolidBrush(border); FrameRect(buffer,&r,edge); DeleteObject(edge);
                text(buffer,r.left+s(6),r.top,r.right-r.left-s(12),r.bottom-r.top,control_text(control));
            } else {
                SetViewportOrgEx(buffer,r.left,r.top,nullptr);
                SendMessageW(control,WM_PRINT,reinterpret_cast<WPARAM>(buffer),PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
            }
            RestoreDC(buffer,saved);
        }
        SelectObject(buffer,old);
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=area.right; info.bmiHeader.biHeight=-area.bottom;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        std::vector<char> pixels(static_cast<std::size_t>(area.right)*area.bottom*4);
        if (!GetDIBits(dc,bitmap,0,static_cast<UINT>(area.bottom),pixels.data(),&info,DIB_RGB_COLORS)) throw std::runtime_error("Cannot capture UI preview");
        BITMAPFILEHEADER header{}; header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER); header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
        std::ofstream out(folder/(target==window ? L"0.1m-preview.bmp" : L"0.1m-inserts-preview.bmp"),std::ios::binary);
        out.write(reinterpret_cast<const char*>(&header),sizeof(header)); out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER)); out.write(pixels.data(),static_cast<std::streamsize>(pixels.size()));
        DeleteObject(bitmap); DeleteDC(buffer); ReleaseDC(target,dc);
        if (!out) throw std::runtime_error("Cannot write UI preview");
    }
    void restore_audio();
    void show_settings();
    void settings_command(int);
    void refresh_profiles();
    void settings_layout();
    void refresh_settings_status(bool force = false);
    void paint_settings(HDC);
    void enumerate_devices();
    void error(const std::exception& e) { ++error_count; log.write(e.what()); if (!smoke) MessageBoxW(window,wide(e.what()).c_str(),L"Moon River Studio",MB_OK | MB_ICONERROR); }
};
LRESULT CALLBACK main_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
void UI::refresh_settings_status(bool force) {
    if (!settings) return;
    const auto now = GetTickCount64();
    if (!force && now-settings_status_tick < 250) return;
    settings_status_tick = now;
    const auto status = app.device_status();
    std::wostringstream value; value << std::fixed << std::setprecision(2)
        << L"Output latency: " << status.output_latency_ms << L" ms   CPU: " << status.cpu_load*100 << L"%";
    const std::array<std::wstring,3> next{wide(app.audio_name()),value.str(),
        device_error.empty() ? L"Outputs: 1,2. Monitor input: 0 = off. Connect stops/reset transport." : wide(device_error)};
    if (next == settings_status_text) return;
    settings_status_text = next;
    RECT client{}; GetClientRect(settings,&client);
    const RECT status_area{0,ss(390),client.right,client.bottom};
    InvalidateRect(settings,&status_area,FALSE);
}
void UI::paint_settings(HDC dc) {
    RECT area{}; GetClientRect(settings,&area); fill(dc,area,background);
    const std::array<const wchar_t*,5> labels{L"Audio backend",L"Sample rate (Hz)",L"Buffer (frames)",L"Physical outputs",L"Monitor input"};
    const std::array<int,5> ys{24,76,118,160,202};
    for (std::size_t i=0; i<labels.size(); ++i) text(dc,ss(20),ss(ys[i]),ss(166),ss(30),labels[i],settings_font);
    text(dc,ss(20),ss(300),ss(160),ss(30),L"Device profile",settings_font);
    text(dc,ss(20),ss(342),ss(160),ss(30),L"Profile name",settings_font);
    for (std::size_t i=0; i<settings_status_text.size(); ++i)
        text(dc,ss(20),ss(395+static_cast<int>(i)*32),ss(540),ss(30),settings_status_text[i],settings_font,muted);
}
LRESULT CALLBACK settings_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK fx_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

std::vector<NativeInsert> fx_chain(const UI& ui) {
    const auto project=ui.app.services().projects->state().project;
    if (!ui.fx_target) return project->master_inserts;
    for (const auto& track : project->tracks) if (track.id==ui.fx_target) return track.inserts;
    throw std::invalid_argument("Insert channel was removed; reopen its editor");
}
LRESULT CALLBACK eq_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam);
LRESULT CALLBACK plugin_editor_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam);
std::wstring fx_name(InsertKind kind) {
    switch(kind){case InsertKind::gain:return L"Gain";case InsertKind::highpass:return L"High-pass (legacy)";case InsertKind::lowpass:return L"Low-pass (legacy)";case InsertKind::eq:return L"EQ (legacy)";case InsertKind::channel_eq:return L"Channel EQ · 3 bands + HP / LP";case InsertKind::cab_ir:return L"Cab IR";default:return L"VST3";}
}
LRESULT CALLBACK UI::tab_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR,DWORD_PTR reference){auto ui=reinterpret_cast<UI*>(reference);if(message==WM_ERASEBKGND)return 1;if(message==WM_PAINT||message==WM_PRINTCLIENT){PAINTSTRUCT ps{};auto dc=message==WM_PAINT?BeginPaint(hwnd,&ps):reinterpret_cast<HDC>(wparam);RECT r{};GetClientRect(hwnd,&r);ui->fill(dc,r,panel);ui->fill(dc,{0,0,ui->s(70),r.bottom},background);ui->text(dc,ui->s(8),0,ui->s(60),r.bottom,L"VST3",ui->normal,ink);ui->line(dc,0,r.bottom-1,ui->s(70),r.bottom-1,accent);if(message==WM_PAINT)EndPaint(hwnd,&ps);return 0;}return DefSubclassProc(hwnd,message,wparam,lparam);}
void UI::browser_refresh(){
    browser_cancel();auto tree=child(browser_tree);if(!tree)return;SendMessageW(tree,WM_SETREDRAW,FALSE,0);TreeView_DeleteAllItems(tree);
    std::map<std::wstring,std::vector<std::size_t>> vendors;for(std::size_t i=0;i<vst_catalog.size();++i)vendors[vst_catalog[i].vendor.empty()?L"Unknown vendor":wide(vst_catalog[i].vendor)].push_back(i);
    for(auto& [vendor,plugins]:vendors){TVINSERTSTRUCTW item{};item.hParent=TVI_ROOT;item.hInsertAfter=TVI_LAST;item.item.mask=TVIF_TEXT|TVIF_PARAM;auto vendor_label=vendor;item.item.pszText=vendor_label.data();auto parent=TreeView_InsertItem(tree,&item);
        std::sort(plugins.begin(),plugins.end(),[&](auto a,auto b){return vst_catalog[a].name<vst_catalog[b].name;});for(auto index:plugins){auto name=wide(vst_catalog[index].name);item.hParent=parent;item.item.pszText=name.data();item.item.lParam=static_cast<LPARAM>(index+1);TreeView_InsertItem(tree,&item);}}
    SendMessageW(tree,WM_SETREDRAW,TRUE,0);InvalidateRect(tree,nullptr,FALSE);InvalidateRect(window,&browser_area,FALSE);
}
void UI::browser_notify(NMHDR* header){if(header->idFrom!=browser_tree||header->code!=TVN_BEGINDRAGW)return;const auto event=reinterpret_cast<NMTREEVIEWW*>(header);if(event->itemNew.lParam<=0)return;const auto index=static_cast<std::size_t>(event->itemNew.lParam-1);if(index>=vst_catalog.size())return;
    if(app.workspace()!=Workspace::mix){command(nav_mix,0);}if(app.recording()||app.engine()->state().playback==PlaybackState::playing)throw std::invalid_argument("Pause/Stop before adding a plugin");plugin_drag=index;SetCapture(window);POINT point=event->ptDrag;MapWindowPoints(child(browser_tree),window,&point,1);browser_move(point);
}
void UI::browser_move(POINT point){if(!plugin_drag)return;plugin_drag_point=point;plugin_drop_valid=false;plugin_drop_track.reset();const auto all=mix_tracks();const auto area=mix_area();if(PtInRect(&area,point))for(std::size_t slot=0;slot<=all.size();++slot){const bool master=slot==all.size();if(!master&&slot<first_mix_track)continue;const auto rect=mix_strip(master?0:slot-first_mix_track,master);if(!master&&rect.right>area.right-s(154))continue;if(PtInRect(&rect,point)){plugin_drop_valid=true;if(!master)plugin_drop_track=all[slot].id;break;}}
    SetCursor(LoadCursorW(nullptr,plugin_drop_valid?IDC_CROSS:IDC_NO));InvalidateRect(window,&area,FALSE);
}
void UI::browser_cancel(){if(!plugin_drag)return;plugin_drag.reset();plugin_drop_valid=false;plugin_drop_track.reset();if(GetCapture()==window)ReleaseCapture();const auto area=mix_area();InvalidateRect(window,&area,FALSE);}
void UI::browser_up(POINT point){if(!plugin_drag)return;browser_move(point);const auto index=*plugin_drag;const auto target=plugin_drop_track;const bool valid=plugin_drop_valid;browser_cancel();if(!valid||index>=vst_catalog.size())return;
    const auto project=app.services().projects->state().project;auto chain=project->master_inserts;if(target)for(const auto& track:project->tracks)if(track.id==target)chain=track.inserts;const auto& plugin=vst_catalog[index];NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::vst3;fx.plugin_path=plugin.path;fx.class_id=plugin.class_id;fx.plugin_name=plugin.name;chain.push_back(std::move(fx));close_vst_editor();app.set_inserts(target,std::move(chain));refresh_models();
}
void UI::browser_scan_poll(){if(!vst_scan.valid()||vst_scan.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;vst_catalog=vst_scan.get();browser_refresh();fx_catalog_refresh();fx_refresh();enable_changed(browser_scan,true);}
void UI::open_insert(std::optional<Id> target,std::size_t index){const auto project=app.services().projects->state().project;const std::vector<NativeInsert>* chain=&project->master_inserts;if(target)for(const auto& track:project->tracks)if(track.id==target)chain=&track.inserts;if(index>=chain->size())return;const auto effect=(*chain)[index];if(effect.kind==InsertKind::vst3)open_vst(target,effect.id,effect.plugin_name);else{open_fx(target);fx_selection=index;fx_refresh();}}
void UI::open_vst(std::optional<Id> target,const Id& slot,const std::string& name){
    if(vst_editor&&editor_target==target&&editor_slot==slot){if(!smoke){ShowWindow(vst_editor,SW_SHOW);SetForegroundWindow(vst_editor);}return;}close_vst_editor();WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=plugin_editor_proc;cls.lpszClassName=L"MRStudioPluginEditor";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
    vst_editor=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,wide(name).c_str(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,s(600),s(400),window,nullptr,cls.hInstance,this);if(!vst_editor)throw std::runtime_error("Cannot create plugin editor");int w{},h{};
    if(!app.open_plugin_editor(target,slot,vst_editor,w,h)){close_vst_editor();if(!fx_window||fx_target!=target)open_fx(target);const auto chain=fx_chain(*this);for(std::size_t i=0;i<chain.size();++i)if(chain[i].id==slot)fx_selection=i;fx_refresh();text_changed(GetDlgItem(fx_window,fx_status),L"No native editor available · Connect audio to prepare the plugin / use generic parameters");return;}
    editor_target=target;editor_slot=slot;editor_generation=app.insert_generation();RECT rect{0,0,w,h};AdjustWindowRect(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);SetWindowPos(vst_editor,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER);if(!smoke){ShowWindow(vst_editor,SW_SHOW);SetForegroundWindow(vst_editor);}
}
void UI::open_fx(std::optional<Id> target) {
    if(fx_window)DestroyWindow(fx_window);fx_target=std::move(target);fx_selection=0;eq_band=2;fx_preview.reset();
    std::wstring name=L"Master";const auto project=app.services().projects->state().project;if(fx_target)for(const auto& t:project->tracks)if(t.id==fx_target)name=wide(t.name);
    fx_window=CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_TOOLWINDOW,L"MRStudioInserts",(L"Inserts — "+name).c_str(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,s(760),s(770),window,nullptr,GetModuleHandleW(nullptr),this);
    if(!fx_window)throw std::runtime_error("Cannot open insert editor");if(!smoke){ShowWindow(fx_window,SW_SHOW);SetForegroundWindow(fx_window);}
}
void UI::fx_create(){
    const auto make=[&](const wchar_t* cls,const wchar_t* label,int id,int x,int y,int w,int h,DWORD style){auto control=CreateWindowExW(cls==std::wstring_view(L"EDIT")?WS_EX_CLIENTEDGE:0,cls,label,WS_CHILD|WS_VISIBLE|style,s(x),s(y),s(w),s(h),fx_window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);if(!control)throw std::runtime_error("Cannot create FX control");SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(normal),TRUE);return control;};
    make(L"LISTBOX",L"",fx_list,20,20,700,100,LBS_NOTIFY|WS_BORDER|WS_VSCROLL|WS_TABSTOP);
    make(L"COMBOBOX",L"",fx_kind,20,144,390,240,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP);
    make(L"BUTTON",L"Add",fx_add,440,144,130,28,BS_OWNERDRAW|WS_TABSTOP);
    make(L"BUTTON",L"Scan VST3…",fx_scan,590,144,130,28,BS_OWNERDRAW|WS_TABSTOP);
    const std::array<std::pair<int,const wchar_t*>,4> buttons{{{fx_remove,L"Remove"},{fx_up,L"Move up"},{fx_down,L"Move down"},{fx_bypass,L"Bypass"}}};for(std::size_t n=0;n<buttons.size();++n)make(L"BUTTON",buttons[n].second,buttons[n].first,20+static_cast<int>(n)*126,188,120,28,BS_OWNERDRAW|WS_TABSTOP);
    make(L"BUTTON",L"Plugin editor…",fx_editor,540,188,180,28,BS_OWNERDRAW|WS_TABSTOP);
    WNDCLASSW cls{};cls.lpfnWndProc=eq_proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"MRStudioEqCurve";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Cannot register EQ curve");
    eq_window=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE,s(20),s(230),s(700),s(270),fx_window,nullptr,cls.hInstance,this);
    make(L"STATIC",L"Drag a point: frequency / gain · Wheel: Q · Ctrl: fine adjustment",fx_hint,20,510,700,26,0);
    make(L"COMBOBOX",L"",fx_band,20,542,350,200,CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL);
    make(L"BUTTON",L"Band enabled",fx_band_enable,400,542,180,28,BS_OWNERDRAW|WS_TABSTOP);
    make(L"STATIC",L"Gain (dB) / VST value (0–1)",fx_gain_label,20,580,230,24,0);make(L"EDIT",L"0",fx_value,20,606,220,28,WS_TABSTOP|ES_AUTOHSCROLL);
    make(L"STATIC",L"Frequency (Hz)",fx_frequency_label,260,580,220,24,0);make(L"EDIT",L"1000",fx_frequency,260,606,220,28,WS_TABSTOP|ES_AUTOHSCROLL);
    make(L"STATIC",L"Q (0.1–10)",fx_q_label,500,580,220,24,0);make(L"EDIT",L"0.7071",fx_q,500,606,220,28,WS_TABSTOP|ES_AUTOHSCROLL);
    make(L"EDIT",L"1",ir_mix,20,542,100,28,WS_TABSTOP|ES_AUTOHSCROLL);
    make(L"BUTTON",L"Polarity +",ir_polarity,140,542,160,28,BS_OWNERDRAW|WS_TABSTOP);
    auto presets=make(L"COMBOBOX",L"",ir_preset,320,542,160,160,CBS_DROPDOWNLIST|WS_TABSTOP);for(auto label:{L"Preset…",L"Neutral",L"Warm",L"Bright"})SendMessageW(presets,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));SendMessageW(presets,CB_SETCURSEL,0,0);
    make(L"BUTTON",L"Load IR WAV…",ir_load,500,542,220,28,BS_OWNERDRAW|WS_TABSTOP);
    make(L"BUTTON",L"Apply parameters",fx_apply,500,650,220,30,BS_OWNERDRAW|WS_TABSTOP);
    make(L"STATIC",L"",fx_status,20,694,700,28,0);
#ifdef MRS_HAS_VST3
    if(!smoke&&vst_catalog.empty())try{vst_catalog=processing::load_vst3_cache(folder/L"vst3.cache");}catch(const std::exception& e){log.write(e.what());}
#endif
    if(vst_scan.valid())SetTimer(fx_window,3,100,nullptr);
    fx_catalog_refresh();fx_refresh();
}
void UI::fx_catalog_refresh(){
    if(!fx_window)return;const auto combo=GetDlgItem(fx_window,fx_kind);SendMessageW(combo,CB_RESETCONTENT,0,0);
    for(auto kind:{InsertKind::gain,InsertKind::channel_eq,InsertKind::cab_ir})SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(fx_name(kind).c_str()));
#ifdef MRS_HAS_VST3
    for(const auto& p:vst_catalog){const auto label=L"VST3 · "+wide(p.name)+L"  ["+wide(p.vendor)+L"]";SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
#endif
    SendMessageW(combo,CB_SETCURSEL,0,0);
}
void UI::fx_refresh(){
    if(!fx_window)return;const auto project=app.services().projects->state().project;if(fx_target && std::none_of(project->tracks.begin(),project->tracks.end(),[&](const auto& t){return t.id==fx_target;})){DestroyWindow(fx_window);return;}
    const auto chain=fx_preview?*fx_preview:fx_chain(*this);fx_selection=chain.empty()?0:std::min(fx_selection,chain.size()-1);const auto child=[&](int id){return GetDlgItem(fx_window,id);};
    SendMessageW(child(fx_list),LB_RESETCONTENT,0,0);for(std::size_t n=0;n<chain.size();++n){const auto label=std::to_wstring(n+1)+L". "+(chain[n].kind==InsertKind::vst3?wide(chain[n].plugin_name):fx_name(chain[n].kind))+(chain[n].bypass?L" [bypass]":L"");SendMessageW(child(fx_list),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}SendMessageW(child(fx_list),LB_SETCURSEL,fx_selection,0);
    fx_editable=!app.recording()&&app.engine()->state().playback!=PlaybackState::playing;const bool selected=!chain.empty();
    const auto enable=[&](int id,bool active){if((IsWindowEnabled(child(id))!=FALSE)!=active)EnableWindow(child(id),active);};
    const auto show=[&](int id,bool active){ShowWindow(child(id),active?SW_SHOW:SW_HIDE);};
    enable(fx_add,fx_editable&&chain.size()<8);enable(fx_remove,fx_editable&&selected);enable(fx_up,fx_editable&&selected&&fx_selection>0);enable(fx_down,fx_editable&&selected&&fx_selection+1<chain.size());
    const bool eq=selected&&chain[fx_selection].kind==InsertKind::channel_eq,vst=selected&&chain[fx_selection].kind==InsertKind::vst3;const bool cab=selected&&chain[fx_selection].kind==InsertKind::cab_ir;
    for(auto id:{ir_mix,ir_polarity,ir_preset,ir_load})show(id,cab);enable(ir_load,fx_editable);
    text_changed(child(fx_hint),cab?L"Mix (0–1) · Mono / independent L-R · Embedded in project · Live controls":L"Drag a point: frequency / gain · Wheel: Q · Ctrl: fine adjustment");
    text_changed(child(fx_gain_label),cab?L"Gain (dB)":L"Gain (dB) / VST value (0–1)");text_changed(child(fx_frequency_label),cab?L"Low cut (Hz; 20 = off)":L"Frequency (Hz)");text_changed(child(fx_q_label),cab?L"High cut (Hz; 20000 = off)":L"Q (0.1–10)");
    enable(fx_bypass,selected&&(!vst||fx_editable));show(fx_editor,vst);show(fx_band,eq||vst);show(fx_band_enable,eq);enable(fx_apply,selected);enable(fx_scan,!vst_scan.valid());
    SendMessageW(child(fx_band),CB_RESETCONTENT,0,0);
    if(eq){for(const auto label:{L"HP · Low cut",L"Band 1 · Low",L"Band 2 · Mid",L"Band 3 · High",L"LP · High cut"})SendMessageW(child(fx_band),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));SendMessageW(child(fx_band),CB_SETCURSEL,eq_band,0);text_changed(child(fx_band_enable),chain[fx_selection].bands[eq_band].enabled?L"Band enabled":L"Band disabled");}
    if(vst){fx_parameters=app.plugin_parameters(fx_target,chain[fx_selection].id);std::erase_if(fx_parameters,[](const auto& p){return p.hidden;});for(const auto& p:fx_parameters){const auto label=p.name.empty()?L"Parameter "+std::to_wstring(p.id):wide(p.name);SendMessageW(child(fx_band),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}vst_parameter=fx_parameters.empty()?0:std::min(vst_parameter,fx_parameters.size()-1);SendMessageW(child(fx_band),CB_SETCURSEL,vst_parameter,0);enable(fx_apply,!fx_parameters.empty()&&fx_parameters[vst_parameter].automatable);}
    const auto format=[](float v){std::wostringstream out;out<<std::setprecision(7)<<v;return out.str();};
    if(selected){const auto& effect=chain[fx_selection];const auto& band=effect.bands[eq_band];const bool filter=effect.kind!=InsertKind::gain;
        enable(fx_value,(cab || !filter || effect.kind==InsertKind::eq || (eq&&eq_band>0&&eq_band<4) || (vst&&!fx_parameters.empty()&&fx_parameters[vst_parameter].automatable)));
        enable(fx_frequency,filter&&!vst);enable(fx_q,filter&&!vst);
        float value=eq?band.gain:effect.gain;
        if(vst&&!fx_parameters.empty()){value=fx_parameters[vst_parameter].initial;for(const auto& p:effect.parameters)if(p.id==fx_parameters[vst_parameter].id)value=p.value;}
        text_changed(child(fx_value),(effect.kind==InsertKind::gain||cab)?(effect.gain==0?L"-inf":format(20*std::log10(effect.gain))):format(value));text_changed(child(fx_frequency),format(cab?effect.ir.low_cut:eq?band.frequency:effect.frequency));text_changed(child(fx_q),format(cab?effect.ir.high_cut:eq?band.q:effect.q));text_changed(child(fx_bypass),effect.bypass?L"Enable":L"Bypass");
    }else for(auto id:{fx_value,fx_frequency,fx_q})enable(id,false);
    std::wstring status=L"Parameters work during playback · Add/remove/reorder after Pause/Stop";
    if(cab){const auto& ir=chain[fx_selection].ir;text_changed(child(ir_mix),format(ir.mix));text_changed(child(ir_polarity),ir.invert?L"Polarity −":L"Polarity +");status=L"Cab IR · "+wide(ir.name)+L" · algorithmic latency: 0 samples";}
    if(vst)status=L"VST3 latency: "+std::to_wstring(app.plugin_latency(fx_target,chain[fx_selection].id))+L" samples · Save after Pause/Stop";
    if(vst_scan.valid())status=L"Scanning VST3 folder in separate processes…";
    if(app.plugin_failed())status=L"A VST3 process call failed; effect bypassed. Pause/Stop, remove and reload it.";
    text_changed(child(fx_status),status);InvalidateRect(eq_window,nullptr,FALSE);
}
void UI::fx_command(int id){
    if(id==fx_list){auto n=SendMessageW(GetDlgItem(fx_window,fx_list),LB_GETCURSEL,0,0);if(n>=0)fx_selection=static_cast<std::size_t>(n);fx_refresh();auto chain=fx_chain(*this);if(fx_selection<chain.size()&&chain[fx_selection].kind==InsertKind::vst3)open_vst(fx_target,chain[fx_selection].id,chain[fx_selection].plugin_name);return;}
    if(id==fx_band){auto n=SendMessageW(GetDlgItem(fx_window,fx_band),CB_GETCURSEL,0,0);if(n>=0){auto chain=fx_chain(*this);if(fx_selection<chain.size()&&chain[fx_selection].kind==InsertKind::vst3)vst_parameter=static_cast<std::size_t>(n);else eq_band=static_cast<std::size_t>(n);}fx_refresh();return;}
    if(id==fx_scan){
#ifdef MRS_HAS_VST3
        if(vst_scan.valid())return;IFileOpenDialog* dialog{};if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog))))throw std::runtime_error("Cannot open scan folder selector");dialog->SetOptions(FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);dialog->SetTitle(L"Select a VST3 folder or .vst3 bundle to scan");std::filesystem::path root;
        if(SUCCEEDED(dialog->Show(window))){IShellItem* item{};if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path{};if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){root=path;CoTaskMemFree(path);}item->Release();}}dialog->Release();if(root.empty())return;
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);const auto helper=std::filesystem::path(exe).parent_path()/L"mrs_vst3_scan.exe";const auto cache=folder/L"vst3.cache";
        *vst_scan_cancel=false;const auto cancel=vst_scan_cancel;vst_scan=std::async(std::launch::async,[root,helper,cache,cancel]{return processing::scan_vst3(root,helper,cache,cancel);});enable_changed(browser_scan,false);InvalidateRect(window,&browser_area,FALSE);fx_refresh();
#endif
        return;
    }
    if(id==fx_editor){auto chain=fx_chain(*this);if(fx_selection<chain.size())open_vst(fx_target,chain[fx_selection].id,chain[fx_selection].plugin_name);return;}
    if(id==ir_load){auto path=pick(fx_window,false,true,impulse_folder());if(path.empty())return;auto chain=fx_chain(*this);if(fx_selection>=chain.size())return;chain[fx_selection].ir=audio::load_cab_ir(path);close_vst_editor();app.set_inserts(fx_target,std::move(chain));fx_refresh();refresh_models();return;}
    if(id==ir_polarity||id==ir_preset){auto chain=fx_chain(*this);if(fx_selection>=chain.size())return;auto& ir=chain[fx_selection].ir;if(id==ir_polarity)ir.invert=!ir.invert;else{auto preset=SendMessageW(GetDlgItem(fx_window,ir_preset),CB_GETCURSEL,0,0);if(preset<=0)return;ir.mix=1;ir.invert=false;chain[fx_selection].gain=1;ir.low_cut=preset==2?80.f:preset==3?50.f:20.f;ir.high_cut=preset==2?5000.f:preset==3?10000.f:20000.f;}app.set_inserts(fx_target,std::move(chain));fx_refresh();refresh_mix_controls();return;}
    if(id==fx_kind || (id>=fx_value&&id<=fx_q))return;
    auto chain=fx_chain(*this);const auto index=fx_selection;
    if(id==fx_add){auto kind=SendMessageW(GetDlgItem(fx_window,fx_kind),CB_GETCURSEL,0,0);if(kind<0)return;NativeInsert effect;effect.id=new_id();effect.kind=kind==0?InsertKind::gain:InsertKind::channel_eq;
#ifdef MRS_HAS_VST3
        if(kind>=3){auto at=static_cast<std::size_t>(kind-3);if(at>=vst_catalog.size())return;const auto& p=vst_catalog[at];effect.kind=InsertKind::vst3;effect.plugin_path=p.path;effect.class_id=p.class_id;effect.plugin_name=p.name;}
#endif
        if(kind==2){auto path=pick(fx_window,false,true,impulse_folder());if(path.empty())return;effect.kind=InsertKind::cab_ir;effect.ir=audio::load_cab_ir(path);}
        chain.push_back(effect);fx_selection=chain.size()-1;
    }else{if(index>=chain.size())return;
        if(id==fx_remove){close_vst_editor();chain.erase(chain.begin()+static_cast<std::ptrdiff_t>(index));}
        else if(id==fx_up&&index>0){close_vst_editor();std::swap(chain[index],chain[index-1]);--fx_selection;}
        else if(id==fx_down&&index+1<chain.size()){close_vst_editor();std::swap(chain[index],chain[index+1]);++fx_selection;}
        else if(id==fx_bypass)chain[index].bypass=!chain[index].bypass;
        else if(id==fx_band_enable)chain[index].bands[eq_band].enabled=!chain[index].bands[eq_band].enabled;
        else if(id==fx_apply){const auto parse=[&](int control){auto text=narrow(control_text(GetDlgItem(fx_window,control)));std::size_t n{};auto v=std::stof(text,&n);if(n!=text.size()||!std::isfinite(v))throw std::invalid_argument("Enter a finite number with a dot for decimals");return v;};auto& effect=chain[index];
            if(effect.kind==InsertKind::gain){if(control_text(GetDlgItem(fx_window,fx_value))==L"-inf")effect.gain=0;else{auto db=parse(fx_value);if(db<-60||db>12)throw std::invalid_argument("Gain range: -60 to +12 dB, or -inf");effect.gain=std::pow(10.f,db/20);}}
            else if(effect.kind==InsertKind::cab_ir){const auto db=parse(fx_value);if(db<-60||db>12)throw std::invalid_argument("Cab IR gain range: -60 to +12 dB");effect.gain=std::pow(10.f,db/20);effect.ir.mix=parse(ir_mix);effect.ir.low_cut=parse(fx_frequency);effect.ir.high_cut=parse(fx_q);}
            else if(effect.kind==InsertKind::channel_eq){auto& band=effect.bands[eq_band];band.frequency=parse(fx_frequency);band.q=parse(fx_q);if(eq_band>0&&eq_band<4)band.gain=parse(fx_value);}
            else if(effect.kind==InsertKind::vst3){if(fx_parameters.empty())return;app.set_plugin_parameter(fx_target,effect.id,fx_parameters[vst_parameter].id,parse(fx_value));fx_refresh();refresh_mix_controls();return;}
            else{effect.frequency=parse(fx_frequency);effect.q=parse(fx_q);if(effect.kind==InsertKind::eq)effect.gain=parse(fx_value);}
        }else return;
    }
    if(id==fx_add||id==fx_remove||id==fx_up||id==fx_down)close_vst_editor();
    app.set_inserts(fx_target,std::move(chain));fx_refresh();refresh_mix_controls();
}
void UI::close_vst_editor(){editor_target.reset();editor_slot={};app.close_plugin_editors();if(vst_editor){auto h=vst_editor;vst_editor=nullptr;DestroyWindow(h);}}
RECT UI::eq_plot() const {RECT r{};GetClientRect(eq_window,&r);return {s(42),s(18),r.right-s(18),r.bottom-s(32)};}
POINT UI::eq_point(const NativeInsert& fx,std::size_t band) const{auto r=eq_plot();const auto& b=fx.bands[band];return {r.left+static_cast<int>(std::log(b.frequency/20.f)/std::log(1000.f)*(r.right-r.left)),(r.top+r.bottom)/2-static_cast<int>((band>0&&band<4?b.gain:0.f)/48.f*(r.bottom-r.top))};}
std::optional<std::size_t> UI::eq_hit(POINT p) const{const auto chain=fx_preview?*fx_preview:fx_chain(*this);if(fx_selection>=chain.size()||chain[fx_selection].kind!=InsertKind::channel_eq)return {};std::optional<std::size_t> best;long distance=s(13)*s(13);for(std::size_t i=0;i<5;++i){auto at=eq_point(chain[fx_selection],i);auto d=(at.x-p.x)*(at.x-p.x)+(at.y-p.y)*(at.y-p.y);if(d<=distance){best=i;distance=d;}}return best;}
void UI::eq_paint(HDC dc){RECT area{};GetClientRect(eq_window,&area);fill(dc,area,RGB(25,28,32));const auto r=eq_plot();const auto chain=fx_preview?*fx_preview:fx_chain(*this);const bool eq=fx_selection<chain.size()&&chain[fx_selection].kind==InsertKind::channel_eq;
    if(fx_selection<chain.size()&&chain[fx_selection].kind==InsertKind::cab_ir){const auto& ir=chain[fx_selection].ir;text(dc,r.left,r.top,r.right-r.left,s(28),wide(ir.name),heading,ink);const auto frames=ir.samples.size()/ir.channels;for(std::size_t ch=0;ch<ir.channels;++ch){const int center=r.top+s(70)+static_cast<int>(ch)*s(90);line(dc,r.left,center,r.right,center,border);for(int x=r.left;x<r.right;++x){const auto first=static_cast<std::size_t>(x-r.left)*frames/(r.right-r.left),last=std::max(first+1,static_cast<std::size_t>(x+1-r.left)*frames/(r.right-r.left));float peak{};for(auto f=first;f<std::min(last,frames);++f)peak=std::max(peak,std::abs(ir.samples[f*ir.channels+ch]));const int h=static_cast<int>(std::min(1.f,peak)*s(38));line(dc,x,center-h,x,center+h,ch?amber:accent);}}text(dc,r.left,r.bottom-s(28),r.right-r.left,s(28),std::to_wstring(ir.sample_rate)+L" Hz · "+std::to_wstring(ir.channels)+L" ch · "+std::to_wstring(frames)+L" samples · filters apply to wet signal",normal,muted);return;}
    for(int db=-24;db<=24;db+=6){const auto y=(r.top+r.bottom)/2-db*(r.bottom-r.top)/48;line(dc,r.left,y,r.right,y,db==0?border:RGB(42,46,52));text(dc,0,y-s(9),s(36),s(18),std::to_wstring(db),normal,muted);}
    for(int hz:{20,50,100,200,500,1000,2000,5000,10000,20000}){const auto x=r.left+static_cast<int>(std::log(hz/20.)/std::log(1000.)*(r.right-r.left));line(dc,x,r.top,x,r.bottom,RGB(42,46,52));text(dc,x-s(23),r.bottom+s(4),s(46),s(22),hz>=1000?std::to_wstring(hz/1000)+L"k":std::to_wstring(hz),normal,muted);}
    if(!eq){text(dc,r.left,r.top+s(65),r.right-r.left,s(80),L"Select Channel EQ to edit three bands and HP / LP filters",normal,muted);return;}
    const auto& fx=chain[fx_selection];const auto pen=CreatePen(PS_SOLID,s(2),fx.bypass?muted:RGB(98,190,245));const auto old=SelectObject(dc,pen);const auto saved=SaveDC(dc);IntersectClipRect(dc,r.left,r.top,r.right+1,r.bottom+1);
    for(int x=r.left;x<=r.right;++x){const double hz=20*std::pow(1000.,static_cast<double>(x-r.left)/(r.right-r.left));const auto db=processing::eq_response_db(fx,hz,app.services().projects->state().project->sample_rate);const auto y=(r.top+r.bottom)/2-static_cast<int>(std::clamp(db,-100.,100.)/48*(r.bottom-r.top));if(x==r.left)MoveToEx(dc,x,y,nullptr);else LineTo(dc,x,y);}RestoreDC(dc,saved);SelectObject(dc,old);DeleteObject(pen);
    for(std::size_t i=0;i<5;++i){auto p=eq_point(fx,i);const COLORREF color=fx.bands[i].enabled?(i==0||i==4?amber:accent):muted;auto brush=CreateSolidBrush(color);auto outline=CreatePen(PS_SOLID,s(i==eq_band?2:1),i==eq_band?ink:color);auto a=SelectObject(dc,brush),b=SelectObject(dc,outline);Ellipse(dc,p.x-s(7),p.y-s(7),p.x+s(7),p.y+s(7));SelectObject(dc,a);SelectObject(dc,b);DeleteObject(brush);DeleteObject(outline);text(dc,p.x-s(22),p.y-s(30),s(44),s(22),i==0?L"HP":i==4?L"LP":std::to_wstring(i),normal,color);}
}
void UI::eq_down(POINT p){const auto hit=eq_hit(p);if(!hit)return;eq_band=*hit;fx_preview=fx_chain(*this);eq_original=(*fx_preview)[fx_selection].bands[eq_band];eq_origin=p;eq_dragging=true;SetFocus(eq_window);SetCapture(eq_window);fx_refresh();}
void UI::eq_move(POINT p){if(!eq_dragging||!fx_preview)return;auto r=eq_plot();auto& band=(*fx_preview)[fx_selection].bands[eq_band];const float fine=(GetKeyState(VK_CONTROL)&0x8000)?0.15f:1.f;band.frequency=std::clamp(eq_original.frequency*static_cast<float>(std::pow(1000.,static_cast<double>(p.x-eq_origin.x)*fine/(r.right-r.left))),20.f,20000.f);if(eq_band>0&&eq_band<4)band.gain=std::clamp(eq_original.gain-static_cast<float>(p.y-eq_origin.y)*fine*48/(r.bottom-r.top),-24.f,24.f);app.preview_inserts(fx_target,*fx_preview);fx_refresh();}
void UI::eq_up(POINT p){if(!eq_dragging)return;eq_move(p);auto chain=*fx_preview;eq_dragging=false;fx_preview.reset();ReleaseCapture();app.set_inserts(fx_target,std::move(chain));refresh_mix_controls();}
void UI::eq_cancel(){if(!eq_dragging)return;eq_dragging=false;fx_preview.reset();app.cancel_insert_preview();if(GetCapture()==eq_window)ReleaseCapture();fx_refresh();}
void UI::eq_wheel(POINT p,int delta){auto chain=fx_preview?*fx_preview:fx_chain(*this);if(fx_selection>=chain.size()||chain[fx_selection].kind!=InsertKind::channel_eq)return;if(auto hit=eq_hit(p))eq_band=*hit;auto r=eq_plot();if(!PtInRect(&r,p))return;auto& b=chain[fx_selection].bands[eq_band];b.q=std::clamp(b.q*static_cast<float>(std::pow(1.2,static_cast<double>(delta)/WHEEL_DELTA*((GetKeyState(VK_CONTROL)&0x8000)?0.15:1.))),0.1f,10.f);if(eq_dragging){fx_preview=chain;eq_original.q=b.q;app.preview_inserts(fx_target,chain);}else app.set_inserts(fx_target,std::move(chain));fx_refresh();}
LRESULT CALLBACK eq_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){auto ui=reinterpret_cast<UI*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(message==WM_NCCREATE){ui=static_cast<UI*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ui));}if(!ui)return DefWindowProcW(hwnd,message,wparam,lparam);try{switch(message){case WM_ERASEBKGND:return 1;case WM_PRINTCLIENT:ui->eq_paint(reinterpret_cast<HDC>(wparam));return 0;case WM_PAINT:{PAINTSTRUCT ps{};auto dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);auto buffer=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(buffer,bitmap);ui->eq_paint(buffer);BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(hwnd,&ps);return 0;}case WM_LBUTTONDOWN:ui->eq_down({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});return 0;case WM_MOUSEMOVE:ui->eq_move({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});return 0;case WM_LBUTTONUP:ui->eq_up({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});return 0;case WM_CAPTURECHANGED:ui->eq_cancel();return 0;case WM_MOUSEWHEEL:{POINT p{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};ScreenToClient(hwnd,&p);ui->eq_wheel(p,GET_WHEEL_DELTA_WPARAM(wparam));return 0;}case WM_KEYDOWN:if(wparam==VK_ESCAPE){ui->eq_cancel();return 0;}break;}}catch(const std::exception& e){ui->eq_cancel();ui->error(e);if(ui->smoke)PostQuitMessage(1);}return DefWindowProcW(hwnd,message,wparam,lparam);}
LRESULT CALLBACK plugin_editor_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){auto ui=reinterpret_cast<UI*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(message==WM_NCCREATE){ui=static_cast<UI*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ui));}if(ui&&message==WM_CLOSE){ui->close_vst_editor();return 0;}if(ui&&message==WM_DESTROY&&ui->vst_editor==hwnd){ui->app.close_plugin_editors();ui->vst_editor=nullptr;}return DefWindowProcW(hwnd,message,wparam,lparam);}

LRESULT CALLBACK fx_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto ui=reinterpret_cast<UI*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if (message==WM_NCCREATE) { ui=static_cast<UI*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams); ui->fx_window=hwnd; SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ui)); }
    if (!ui) return DefWindowProcW(hwnd,message,wparam,lparam);
    try {
        switch (message) {
        case WM_CREATE: ui->fx_create(); return 0;
        case WM_TIMER:
            if(wparam==3)ui->browser_scan_poll();return 0;
        case WM_COMMAND: if (HIWORD(wparam)==BN_CLICKED || ((LOWORD(wparam)==fx_list && HIWORD(wparam)==LBN_SELCHANGE)||((LOWORD(wparam)==fx_band||LOWORD(wparam)==ir_preset) && HIWORD(wparam)==CBN_SELCHANGE))) ui->fx_command(LOWORD(wparam)); return 0;
        case WM_DRAWITEM: ui->draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)); return TRUE;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
            SetTextColor(reinterpret_cast<HDC>(wparam),ink); SetBkColor(reinterpret_cast<HDC>(wparam),panel); return reinterpret_cast<LRESULT>(ui->panel_brush);
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_DESTROY: ui->eq_cancel();ui->close_vst_editor();ui->fx_window=nullptr;ui->eq_window=nullptr;return 0;
        }
    } catch (const std::exception& e) { ui->error(e); if (ui->smoke) PostQuitMessage(1); }
    return DefWindowProcW(hwnd,message,wparam,lparam);
}

void UI::restore_audio() {
    if (!prefs.reconnect_audio || prefs.device_name.empty()) return;
#ifdef MRS_HAS_ASIO
    auto device = audio::make_asio_device();
    const auto available = device->enumerate();
    const auto found = std::find_if(available.begin(),available.end(),[&](const auto& info) { return info.name == prefs.device_name; });
    if (found == available.end()) throw std::runtime_error("Saved ASIO device is unavailable. Choose a device in Audio settings.");
    // Resolve the saved name afresh: enumeration indices may change between sessions.
    audio::DeviceConfig config{found->index,app.services().projects->state().project->sample_rate,prefs.buffer,{},prefs.outputs,prefs.processing_workers,prefs.process_buffer_frames};
    if (prefs.monitor_input >= 0) config.inputs = {prefs.monitor_input};
    app.connect(std::move(device),config); prefs.rate = config.sample_rate; preferences();
    refresh_models(); log.write("Saved ASIO connection restored; transport stopped");
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
    settings = CreateWindowExW(WS_EX_CONTROLPARENT,L"MRStudioAudio",L"Audio settings",WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,s(600),s(565),window,nullptr,GetModuleHandleW(nullptr),this);
    if (!settings) throw std::runtime_error("Cannot open audio settings");
    settings_dpi = GetDpiForWindow(settings);
    create(settings,L"COMBOBOX",L"",device_combo,WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL);
    for (auto id : {rate_edit,buffer_edit,outputs_edit,input_edit}) { create(settings,L"EDIT",L"",id,WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER); SendMessageW(child(id,true),EM_SETLIMITTEXT,256,0); }
    for (auto [id,label] : std::array<std::pair<int,const wchar_t*>,4>{{{connect_button,L"Connect"},{disconnect_button,L"Disconnect"},{panel_button,L"ASIO panel"},{refresh_button,L"Refresh"}}}) button(settings,label,id);
    SetWindowTextW(child(rate_edit,true),std::to_wstring(prefs.rate).c_str()); SetWindowTextW(child(buffer_edit,true),std::to_wstring(prefs.buffer).c_str());
    std::wstring outputs; for (auto o : prefs.outputs) { if (!outputs.empty()) outputs += L","; outputs += std::to_wstring(o+1); }
    SetWindowTextW(child(outputs_edit,true),outputs.c_str()); SetWindowTextW(child(input_edit,true),std::to_wstring(prefs.monitor_input+1).c_str());
    create(settings,L"COMBOBOX",L"",profile_combo,WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL);
    create(settings,L"EDIT",L"",profile_name,WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER); SendMessageW(child(profile_name,true),EM_SETLIMITTEXT,128,0);
    button(settings,L"Save",profile_save); button(settings,L"Load",profile_load); button(settings,L"Delete",profile_delete);
    settings_fonts(); enumerate_devices(); refresh_profiles(); settings_layout();
    SetWindowPos(settings,nullptr,0,0,ss(600),ss(565),SWP_NOMOVE | SWP_NOZORDER);
    refresh_settings_status(true); ShowWindow(settings,SW_SHOW);
}
void UI::settings_layout() {
    auto move = [&](int id, int x, int y, int w, int h) { MoveWindow(child(id,true),ss(x),ss(y),ss(w),ss(h),TRUE); };
    move(device_combo,190,24,355,250); move(rate_edit,190,76,150,30); move(buffer_edit,190,118,150,30);
    move(outputs_edit,190,160,150,30); move(input_edit,190,202,150,30);
    move(connect_button,20,252,110,34); move(disconnect_button,140,252,120,34); move(panel_button,270,252,120,34); move(refresh_button,400,252,110,34);
    move(profile_combo,190,300,225,180); move(profile_load,425,300,90,30);
    move(profile_name,190,342,225,30); move(profile_save,425,342,70,30); move(profile_delete,500,342,70,30);
    InvalidateRect(settings,nullptr,FALSE);
}
void UI::refresh_profiles() {
    SendMessageW(child(profile_combo,true),CB_RESETCONTENT,0,0);
    for (const auto& profile : prefs.profiles) { const auto label = wide(profile.name); SendMessageW(child(profile_combo,true),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str())); }
    if (!prefs.profiles.empty()) SendMessageW(child(profile_combo,true),CB_SETCURSEL,0,0);
}
void UI::settings_command(int id) {
    if (app.recording()) return;
    if (id == refresh_button) { enumerate_devices(); InvalidateRect(settings,nullptr,FALSE); return; }
    if (id == disconnect_button) { app.disconnect(); prefs.reconnect_audio = false; preferences(); refresh_models(); InvalidateRect(settings,nullptr,FALSE); return; }
    if (id == profile_load || id == profile_delete) {
        const auto index = SendMessageW(child(profile_combo,true),CB_GETCURSEL,0,0);
        if (index < 0 || static_cast<std::size_t>(index) >= prefs.profiles.size()) throw std::invalid_argument("Select a device profile");
        const auto profile = prefs.profiles[static_cast<std::size_t>(index)];
        if (id == profile_delete) { prefs.profiles.erase(prefs.profiles.begin()+index); if (!smoke) preferences(); refresh_profiles(); return; }
        const auto info = std::find_if(devices.begin(),devices.end(),[&](const auto& v) { return v.name == profile.device_name; });
        if (info == devices.end()) throw std::invalid_argument("Profile device is unavailable; connect it and Refresh");
        const auto config = resolve_profile(profile,*info);
        if (config.sample_rate != app.services().projects->state().project->sample_rate) throw std::invalid_argument("Profile rate differs from the project; resampling is not available");
        SendMessageW(child(device_combo,true),CB_SETCURSEL,static_cast<WPARAM>(info-devices.begin()+1),0);
        const auto assign = [&](int control, const std::wstring& value) { SetWindowTextW(child(control,true),value.c_str()); };
        assign(rate_edit,std::to_wstring(config.sample_rate)); assign(buffer_edit,std::to_wstring(config.buffer_frames)); assign(input_edit,std::to_wstring(profile.monitor_input+1));
        std::wstring outputs; for (const auto c : config.outputs) { if (!outputs.empty()) outputs += L","; outputs += std::to_wstring(c+1); }
        assign(outputs_edit,outputs); assign(profile_name,wide(profile.name)); staged_profile = profile; return;
    }
    if (id == profile_save) {
        const auto selection = SendMessageW(child(device_combo,true),CB_GETCURSEL,0,0);
        if (selection <= 0 || static_cast<std::size_t>(selection) > devices.size()) throw std::invalid_argument("Select an ASIO device before saving its profile");
        const auto& info = devices[static_cast<std::size_t>(selection)-1];
        audio::DeviceConfig config{info.index,number(child(rate_edit,true)),number(child(buffer_edit,true)),{},parse_outputs(narrow(control_text(child(outputs_edit,true))))};
        const auto input = number(child(input_edit,true)); if (input > 64) throw std::invalid_argument("Input must be 0..64"); if (input) config.inputs = {static_cast<int>(input)-1};
        auto profile = capture_profile(narrow(control_text(child(profile_name,true))),info,config);
        auto next = prefs; const auto found = std::find_if(next.profiles.begin(),next.profiles.end(),[&](const auto& p) { return p.name == profile.name; });
        if (found == next.profiles.end()) next.profiles.push_back(profile); else *found = profile;
        next.validate(); prefs = std::move(next); if (!smoke) preferences(); refresh_profiles();
        const auto saved = std::find_if(prefs.profiles.begin(),prefs.profiles.end(),[&](const auto& p) { return p.name == profile.name; });
        SendMessageW(child(profile_combo,true),CB_SETCURSEL,static_cast<WPARAM>(saved-prefs.profiles.begin()),0); return;
    }
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
    audio::DeviceConfig config{selection == 0 ? 0 : devices[static_cast<std::size_t>(selection)-1].index,next_prefs.rate,next_prefs.buffer,{},next_prefs.outputs,next_prefs.processing_workers,next_prefs.process_buffer_frames};
    if (next_prefs.monitor_input >= 0) config.inputs = {next_prefs.monitor_input};
    std::unique_ptr<audio::IAudioDevice> device;
    if (selection == 0) device = audio::make_offline_device();
#ifdef MRS_HAS_ASIO
    else device = audio::make_asio_device();
#endif
    if (staged_profile && next_prefs.device_name == staged_profile->device_name && config.sample_rate == staged_profile->rate && config.buffer_frames == staged_profile->buffer && config.outputs == staged_profile->outputs && next_prefs.monitor_input == staged_profile->monitor_input) {
        const auto fresh = device->enumerate();
        const auto found = std::find_if(fresh.begin(),fresh.end(),[&](const auto& info) { return info.name == staged_profile->device_name; });
        if (found == fresh.end()) throw std::invalid_argument("Profile device is unavailable");
        const auto resolved = resolve_profile(*staged_profile,*found); config.device = resolved.device;
    }
    app.connect(std::move(device),config); prefs = std::move(next_prefs); if (!smoke) preferences(); refresh_models(); log.write("Audio connected");
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
        case WM_COMMAND: ui->settings_command(LOWORD(wparam)); ui->refresh_settings_status(true); return 0;
        case WM_DRAWITEM: ui->draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)); return TRUE;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
            SetTextColor(reinterpret_cast<HDC>(wparam),ink); SetBkColor(reinterpret_cast<HDC>(wparam),panel); return reinterpret_cast<LRESULT>(ui->panel_brush);
        case WM_ERASEBKGND: return 1;
        case WM_PRINTCLIENT: ui->paint_settings(reinterpret_cast<HDC>(wparam)); return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; const auto dc = BeginPaint(hwnd,&ps); RECT area{}; GetClientRect(hwnd,&area);
            const auto buffer = CreateCompatibleDC(dc);
            const auto bitmap = CreateCompatibleBitmap(dc,std::max<LONG>(1,area.right),std::max<LONG>(1,area.bottom));
            const auto old = SelectObject(buffer,bitmap); ui->paint_settings(buffer);
            BitBlt(dc,0,0,area.right,area.bottom,buffer,0,0,SRCCOPY);
            SelectObject(buffer,old); DeleteObject(bitmap); DeleteDC(buffer); EndPaint(hwnd,&ps); return 0;
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
        case WM_NOTIFY:ui->browser_notify(reinterpret_cast<NMHDR*>(lparam));return 0;
        case WM_COMMAND: if(LOWORD(wparam)==browser_scan)ui->fx_command(fx_scan);else ui->command(LOWORD(wparam),HIWORD(wparam)); return 0;
        case WM_DRAWITEM: ui->draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)); return TRUE;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
            SetTextColor(reinterpret_cast<HDC>(wparam),ink); SetBkColor(reinterpret_cast<HDC>(wparam),panel); return reinterpret_cast<LRESULT>(ui->panel_brush);
        case WM_ERASEBKGND: return 1;
        case WM_PRINTCLIENT: ui->paint(reinterpret_cast<HDC>(wparam)); return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; auto dc = BeginPaint(hwnd,&ps); RECT area{}; GetClientRect(hwnd,&area);
            auto buffer = CreateCompatibleDC(dc); auto bitmap = CreateCompatibleBitmap(dc,std::max<LONG>(1,area.right),std::max<LONG>(1,area.bottom));
            auto old = SelectObject(buffer,bitmap); ui->paint(buffer); BitBlt(dc,0,0,area.right,area.bottom,buffer,0,0,SRCCOPY);
            SelectObject(buffer,old); DeleteObject(bitmap); DeleteDC(buffer); EndPaint(hwnd,&ps); return 0;
        }
        case WM_TIMER:
            if (wparam == 1) {
                ui->browser_scan_poll();
                if(ui->vst_editor&&ui->editor_generation!=ui->app.insert_generation())ui->close_vst_editor();
                const auto peaks = ui->app.engine()->take_meters();
                const auto decay = [](audio::StereoPeak& value, audio::StereoPeak next) { value.left = std::max(next.left,value.left*0.86f); value.right = std::max(next.right,value.right*0.86f); };
                for (std::size_t i=0; i<audio::max_mixer_tracks; ++i) decay(ui->mix_meters.tracks[i],peaks.tracks[i]);
                decay(ui->mix_meters.master,peaks.master);
                if (ui->mix_drag) (void)ui->app.preview_mix(ui->mix_drag->track,ui->mix_drag->mix,ui->mix_drag->master);
                try { const bool recording = ui->app.recording(); ui->app.poll(); if (recording != ui->app.recording()) ui->refresh_models(); ui->sync_workspace_controls(); if (ui->fx_window && ui->fx_editable!=(!ui->app.recording() && ui->app.engine()->state().playback!=PlaybackState::playing)) ui->fx_refresh(); } catch (const std::runtime_error&) { return 0; } // bounded mailbox can be busy
                InvalidateRect(hwnd,nullptr,FALSE); ui->refresh_settings_status(); return 0;
            }
            if (wparam == 2 && ui->smoke) {
                ++ui->smoke_step;
                if (ui->smoke_step <= 3) ui->command(nav_arrange+ui->smoke_step-1,BN_CLICKED);
                if (ui->smoke_step == 5) {
                    ui->command(nav_arrange,BN_CLICKED);
                    const auto revision = ui->app.services().projects->state().revision;
                    POINT origin{(ui->canvas.left+ui->canvas.right)/2,ui->audio_top()+ui->s(42)};
                    POINT destination{origin.x+ui->s(30),origin.y};
                    ui->mouse_down(origin); ui->mouse_move(destination);
                    if (!ui->drag || ui->app.services().projects->state().revision != revision)
                        throw std::runtime_error("Drag preview unexpectedly changed project state");
                    ui->mouse_up(destination);
                    if (ui->app.services().projects->state().revision != revision+1)
                        throw std::runtime_error("Drag did not commit exactly one edit");
                    ui->command(undo,0);
                    const auto undone = ui->app.services().projects->state().revision;
                    ui->mouse_down(origin); ui->mouse_move(destination); ui->cancel_drag();
                    if (ui->app.services().projects->state().revision != undone)
                        throw std::runtime_error("Cancelled drag changed project state");
                    ui->command(nav_mix,BN_CLICKED);
                    const auto mix_before = ui->app.services().projects->state();
                    const auto strip = ui->mix_strip(0), fader = ui->mix_control(strip,34);
                    const POINT ignored_point{fader.left+(fader.right-fader.left)/2,fader.top+ui->s(12)};
                    ui->mouse_down(ignored_point);if(ui->mix_drag || ui->app.services().projects->state().revision!=mix_before.revision)throw std::runtime_error("Fader jumped on rail click");
                    const auto handle=ui->regulator_handle(fader,mix_before.project->tracks.front().mix.gain,true,false);
                    const POINT gain_point{(handle.left+handle.right)/2,(handle.top+handle.bottom)/2};
                    ui->mouse_down(gain_point);
                    if (!ui->mix_drag || ui->app.services().projects->state().revision != mix_before.revision)
                        throw std::runtime_error("Mixer preview changed project state");
                    ui->mouse_up({gain_point.x,gain_point.y-ui->s(5)});
                    if (ui->app.services().projects->state().revision != mix_before.revision+1)
                        throw std::runtime_error("Mixer gesture did not commit one shared command");
                    ui->command(undo,0);
                    ui->mouse_down(gain_point); ui->cancel_mix_drag();
                    if (ui->app.services().projects->state().project->tracks.front().mix != mix_before.project->tracks.front().mix)
                        throw std::runtime_error("Mixer cancellation changed saved parameters");
                    // Flush the selection/focus change once, then verify that repeated
                    // fader previews and timer paints do not redraw native buttons.
                    ui->mouse_down(gain_point);
                    RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                    const auto preview_button_paints = ui->button_paints;
                    const auto preview_button_layout = ui->button_layout_events;
                    for (int n=0; n<4; ++n) {
                        ui->mouse_move({gain_point.x,gain_point.y-ui->s(n+1)}); UpdateWindow(hwnd);
                        const auto dc = GetDC(hwnd); SendMessageW(hwnd,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT); ReleaseDC(hwnd,dc);
                        for (auto control = GetWindow(hwnd,GW_CHILD); control; control = GetWindow(control,GW_HWNDNEXT)) UpdateWindow(control);
                    }
                    if (ui->button_paints != preview_button_paints) throw std::runtime_error("Fader preview repainted unrelated native buttons");
                    if (ui->button_layout_events != preview_button_layout) throw std::runtime_error("Fader repaint triggered native button layout");
                    const auto release_button_updates = ui->unrelated_button_updates;
                    ui->mouse_up({gain_point.x,gain_point.y-ui->s(5)});
                    if (ui->unrelated_button_updates != release_button_updates) throw std::runtime_error("Fader release updated unrelated native buttons");
                    ui->command(undo,0);
                    ui->cancel_mix_drag();
                    RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                    const auto idle_button_paints = ui->button_paints;
                    const auto idle_button_layout = ui->button_layout_events;
                    for (int n=0; n<4; ++n) {
                        SendMessageW(hwnd,WM_TIMER,1,0); UpdateWindow(hwnd);
                        const auto dc = GetDC(hwnd); SendMessageW(hwnd,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT); ReleaseDC(hwnd,dc);
                        for (auto control = GetWindow(hwnd,GW_CHILD); control; control = GetWindow(control,GW_HWNDNEXT)) UpdateWindow(control);
                    }
                    if (ui->button_paints != idle_button_paints) throw std::runtime_error("Timer/menu idle repainted unrelated native buttons");
                    if (ui->button_layout_events != idle_button_layout) throw std::runtime_error("Timer/menu repaint triggered native button layout");
                    ui->mouse_down(gain_point);
                    const auto initial_gain = ui->mix_drag->mix.gain;
                    ui->mouse_move({gain_point.x,gain_point.y-ui->s(10)});
                    if (!ui->mix_drag->vertical || ui->mix_drag->mix.gain <= initial_gain)
                        throw std::runtime_error("Vertical fader did not follow vertical movement");
                    ui->cancel_mix_drag();
                    ui->command(nav_mix,BN_CLICKED);
                    if (ui->app.workspace() != Workspace::arrange) throw std::runtime_error("Mix toggle did not reveal arrangement");
                    const auto header = ui->mini_rect(0);
                    const RECT mini_rail{header.left+ui->s(8),header.top+ui->s(26),header.right-ui->s(44),header.top+ui->s(44)};
                    const auto mini_handle=ui->regulator_handle(mini_rail,ui->app.services().projects->state().project->tracks.front().mix.gain,false,false);
                    const POINT mini_gain{(mini_handle.left+mini_handle.right)/2,(mini_handle.top+mini_handle.bottom)/2};
                    const auto mini_revision = ui->app.services().projects->state().revision;
                    ui->mouse_down(mini_gain); ui->mouse_move({mini_gain.x+ui->s(10),mini_gain.y});
                    if (!ui->mix_drag || ui->mix_drag->vertical || ui->mix_drag->pan) throw std::runtime_error("Track header gain did not use horizontal preview");
                    const auto mini_release_updates = ui->unrelated_button_updates;
                    ui->mouse_up({mini_gain.x+ui->s(10),mini_gain.y});
                    if (ui->unrelated_button_updates != mini_release_updates) throw std::runtime_error("Mini fader release updated unrelated buttons");
                    if (ui->app.services().projects->state().revision != mini_revision+1) throw std::runtime_error("Mini fader did not commit one edit");
                    ui->command(undo,0);
                    const POINT knob{header.right-ui->s(14),header.top+ui->s(40)};
                    ui->mouse_down(knob); ui->mouse_up({knob.x+ui->s(20),knob.y});
                    if (ui->app.services().projects->state().project->tracks.front().mix.pan <= 0) throw std::runtime_error("Track pan knob did not update shared pan");
                    ui->command(undo,0); ui->command(nav_mix,BN_CLICKED);
                    const auto overlay = ui->mix_area();
                    if (overlay.top <= ui->canvas.top || GetParent(hwnd) != nullptr) throw std::runtime_error("Mixer did not overlay the main arrangement");
                    const auto master_fader = ui->mix_control(ui->mix_strip(0,true),34);
                    const auto master_handle=ui->regulator_handle(master_fader,ui->app.services().projects->state().project->master_gain,true,false);
                    const POINT master_point{(master_handle.left+master_handle.right)/2,(master_handle.top+master_handle.bottom)/2};
                    ui->mouse_down(master_point);
                    const auto master_release_updates = ui->unrelated_button_updates;
                    ui->mouse_up(master_point);
                    if (ui->unrelated_button_updates != master_release_updates) throw std::runtime_error("Master release updated unrelated buttons");
                    ui->command(undo,0);
                    const auto bar = GetMenu(hwnd), files = GetSubMenu(bar,0);
                    if (!bar || !files || GetMenuItemID(files,0) != new_project_button || GetMenuItemID(files,4) != save || !GetSubMenu(files,2))
                        throw std::runtime_error("Files menu did not retain project commands");
                    if (GetMenuItemID(files,11) != studio_folder_button || !std::filesystem::is_directory(ui->studio.projects()) || !std::filesystem::is_directory(ui->studio.lives()))
                        throw std::runtime_error("Studio content folders/menu missing");
                    const auto recent_menu = GetSubMenu(files,2);
                    if (!recent_menu || GetMenuItemCount(recent_menu) < 1) throw std::runtime_error("Recent project submenu missing");
                    for (auto id : {record_button,arm_button,monitor_button}) if (!ui->child(id)) throw std::runtime_error("Recording controls missing");
                    ui->command(arm_button,0);
                    if (!ui->app.armed_track()) throw std::runtime_error("Arm track did not use shared application");
                    ui->command(arm_button,0);
                    if (ui->app.armed_track()) throw std::runtime_error("Disarm did not clear shared application");
                    ui->command(add_bus_button,0);
                    const auto bus_id = *ui->selected_track;
                    const auto audio_id = ui->app.services().projects->state().project->tracks.front().id;
                    ui->app.set_track_output(audio_id,bus_id); ui->refresh_models();
                    ui->app.set_track_sends(audio_id,{{bus_id,0.5f,true}});
                    if (IsWindowEnabled(ui->child(arm_button))) throw std::runtime_error("Bus can be armed for recording");
                    const auto bus_strip = ui->mix_strip(0), bus_gain = ui->mix_control(bus_strip,34);
                    const auto bus_handle=ui->regulator_handle(bus_gain,ui->app.services().projects->state().project->tracks.back().mix.gain,true,false);
                    const POINT bus_point{(bus_handle.left+bus_handle.right)/2,(bus_handle.top+bus_handle.bottom)/2};
                    ui->mouse_down(bus_point); ui->mouse_up({bus_point.x,bus_point.y-ui->s(10)});
                    if (ui->app.services().projects->state().project->tracks.back().mix.gain == 1) throw std::runtime_error("Bus fader did not change shared mix");
                    ui->command(undo,0); ui->app.remove_track(bus_id); ui->refresh_models();
                    if (ui->app.services().projects->state().project->tracks.front().output) throw std::runtime_error("Deleted bus retained a dangling route");
                    if (!ui->app.services().projects->state().project->tracks.front().sends.empty()) throw std::runtime_error("Deleted return retained a dangling send");
                    for (auto id : {nav_live,open,save,save_as,demo,import,new_project_button,import_batch})
                        if (ui->child(id)) throw std::runtime_error("File/Live controls still occupy the workspace");
                    ui->app.rename_track(ui->app.services().projects->state().project->tracks.front().id,"Smoke track"); ui->refresh_models();
                    ui->show_settings(); // offline CI build skips enumeration of physical ASIO
                }
                if (ui->smoke_step == 6) {
                    // Exercise real profile controls without opening physical hardware
                    // or touching the user's stored preferences.
                    const auto saved_devices = ui->devices;
                    ui->devices = {{7,"Profile smoke",{"Mic"},{"Main L","Main R","Cue L","Cue R"},32,2048,128,-1}};
                    const auto label = L"Profile smoke";
                    SendMessageW(ui->child(device_combo,true),CB_RESETCONTENT,0,0);
                    SendMessageW(ui->child(device_combo,true),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Offline"));
                    SendMessageW(ui->child(device_combo,true),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
                    SendMessageW(ui->child(device_combo,true),CB_SETCURSEL,1,0);
                    SetWindowTextW(ui->child(profile_name,true),L"Smoke profile");
                    SetWindowTextW(ui->child(rate_edit,true),std::to_wstring(ui->app.services().projects->state().project->sample_rate).c_str());
                    SetWindowTextW(ui->child(buffer_edit,true),L"128"); SetWindowTextW(ui->child(input_edit,true),L"1");
                    SetWindowTextW(ui->child(outputs_edit,true),L"1,2,3,4");
                    ui->settings_command(profile_save);
                    if (ui->prefs.profiles.size() != 1) throw std::runtime_error("Profile Save did not store its configuration");
                    SetWindowTextW(ui->child(outputs_edit,true),L"1,2"); ui->settings_command(profile_load);
                    if (control_text(ui->child(outputs_edit,true)) != L"1,2,3,4" || control_text(ui->child(input_edit,true)) != L"1") throw std::runtime_error("Profile Load did not restore channels");
                    ui->settings_command(profile_delete);
                    if (!ui->prefs.profiles.empty()) throw std::runtime_error("Profile Delete retained the profile");
                    ui->staged_profile.reset(); ui->devices = saved_devices;
                    // Editing incomplete values, including an unselected combo, must not
                    // validate/open a device until the explicit Connect action.
                    SendMessageW(ui->child(device_combo,true),CB_SETCURSEL,static_cast<WPARAM>(-1),0);
                    SetWindowTextW(ui->child(rate_edit,true),L"");
                    SetWindowTextW(ui->child(outputs_edit,true),L"1,");
                    SendMessageW(ui->settings,WM_COMMAND,MAKEWPARAM(device_combo,CBN_SELCHANGE),reinterpret_cast<LPARAM>(ui->child(device_combo,true)));
                    SendMessageW(ui->child(device_combo,true),CB_SETCURSEL,0,0);
                    SetWindowTextW(ui->child(rate_edit,true),L"48000");
                    SetWindowTextW(ui->child(outputs_edit,true),L"1,2");
                    ValidateRect(ui->settings,nullptr);
                    ui->settings_status_text[1] = L"stale status"; ui->settings_status_tick = 0;
                    SendMessageW(hwnd,WM_TIMER,1,0);
                    RECT settings_update{};
                    if (GetUpdateRect(ui->settings,&settings_update,FALSE) && settings_update.top < ui->ss(390))
                        throw std::runtime_error("Audio settings timer invalidated static labels/input controls");
                    if (!GetUpdateRect(ui->settings,&settings_update,FALSE) || ui->settings_status_text[1] == L"stale status") throw std::runtime_error("Audio settings status did not refresh");
                    ValidateRect(ui->settings,nullptr); ui->refresh_settings_status(true);
                    if (GetUpdateRect(ui->settings,&settings_update,FALSE)) throw std::runtime_error("Unchanged audio status repainted the dialog");
                    if (!(GetWindowLongPtrW(ui->settings,GWL_STYLE) & WS_CLIPCHILDREN)) throw std::runtime_error("Audio settings paint does not protect child controls");
                    if (SendMessageW(ui->settings,WM_ERASEBKGND,0,0) != 1) throw std::runtime_error("Audio settings background erase was not suppressed");
                    if (!ui->child(play) || !ui->child(device_combo,true) || ui->app.workspace() != Workspace::mix || ui->error_count != 0)
                        throw std::runtime_error("GUI initialization/typing produced an unexpected error");
                    const auto mini=ui->mini_rect(0); const auto mini_id=ui->app.services().projects->state().project->tracks.front().id;
                    ui->mouse_down({mini.right-ui->s(150),mini.top+ui->s(10)});
                    if (!ui->app.services().projects->state().project->tracks.front().mix.mute) throw std::runtime_error("Header M did not mute its track");
                    ui->app.undo(); ui->mouse_down({mini.right-ui->s(110),mini.top+ui->s(10)});
                    if (!ui->app.services().projects->state().project->tracks.front().mix.solo || ui->selected_track != mini_id) throw std::runtime_error("Header S did not solo its selected track");
                    ui->app.undo();
                    // Drive the actual WM_MOUSEWHEEL dispatch with every modifier.
                    ui->fit_view=false; ui->visible_seconds=32; ui->view_start=8; ui->first_track=0;
                    const POINT wheel_point{(ui->canvas.left+ui->canvas.right)/2,ui->audio_top()+ui->s(40)};
                    POINT wheel_screen=wheel_point; ClientToScreen(hwnd,&wheel_screen);
                    const auto scroll=[&](unsigned keys, int delta) { SendMessageW(hwnd,WM_MOUSEWHEEL,MAKEWPARAM(keys,static_cast<WORD>(delta)),MAKELPARAM(wheel_screen.x,wheel_screen.y)); };
                    const auto project_revision=ui->app.services().projects->state().revision;
                    const auto height=ui->track_height; const auto anchor=ui->sample_at(wheel_point.x); scroll(MK_CONTROL | MK_SHIFT,WHEEL_DELTA);
                    if (std::abs(ui->visible_seconds-25.6)>0.001 || ui->track_height != height || std::abs(ui->sample_at(wheel_point.x)-anchor)>1) throw std::runtime_error("Horizontal wheel zoom lost its anchor or changed track height");
                    const auto seconds=ui->visible_seconds, start=ui->view_start; scroll(MK_CONTROL,WHEEL_DELTA);
                    if (ui->track_height <= height || ui->visible_seconds != seconds || ui->view_start != start) throw std::runtime_error("Vertical wheel zoom changed timeline scale");
                    const auto track_pixels=ui->row_height(); scroll(MK_SHIFT,-WHEEL_DELTA);
                    if (ui->view_start <= start || ui->row_height() != track_pixels) throw std::runtime_error("Shift wheel did not scroll horizontally");
                    if (ui->app.services().projects->state().revision != project_revision) throw std::runtime_error("View changes modified project history");
                    for (int n=0; n<6; ++n) (void)ui->app.add_audio_track("Track "+std::to_string(n+2));
                    ui->first_track=0; const auto scroll_start=ui->view_start;
                    scroll(0,-WHEEL_DELTA/2); if (ui->first_track || ui->track_offset != 16) throw std::runtime_error("Partial wheel delta did not scroll sixteen pixels");
                    scroll(0,-WHEEL_DELTA/2); if (ui->first_track || ui->track_offset != 32 || ui->view_start != scroll_start) throw std::runtime_error("Plain wheel did not scroll thirty-two pixels independently");
                    scroll(0,-1); if (ui->track_offset<=32 || ui->track_offset>=33) throw std::runtime_error("High resolution wheel lost its fractional movement");
                    scroll(0,1); if (std::abs(ui->track_offset-32)>0.001) throw std::runtime_error("Fine wheel scroll did not reverse precisely");
                    const auto hit=ui->track_at(ui->audio_top()+ui->row_height()+ui->s(30));
                    if (!hit || *hit != ui->app.services().projects->state().project->tracks[1].id || ui->mini_rect(1).top != ui->audio_top()+ui->row_height()-ui->track_offset_pixels()) throw std::runtime_error("Zoomed track hit test and header geometry disagree");
                    scroll(MK_CONTROL,-WHEEL_DELTA*100); if (ui->track_height != 92) throw std::runtime_error("Track zoom minimum failed");
                    scroll(MK_CONTROL,WHEEL_DELTA*100); if (ui->track_height != 320) throw std::runtime_error("Track zoom maximum failed");
                    const auto mixer_region=ui->mix_area(); const POINT mixer_point{mixer_region.left+ui->s(150),mixer_region.top+ui->s(100)};
                    ui->first_mix_track=0; ui->first_track=0; ui->track_offset=0; ui->wheel(mixer_point,-WHEEL_DELTA,MK_SHIFT);
                    if (ui->first_mix_track != 1 || ui->first_track) throw std::runtime_error("Shift wheel did not scroll mixer channels");
                    ui->wheel(mixer_point,-WHEEL_DELTA,0);
                    if (ui->first_track || ui->track_offset != 32 || ui->first_mix_track != 1) throw std::runtime_error("Plain wheel changed mixer horizontal position");
                    ui->track_height=112; ui->first_track=0; ui->track_offset=0; ui->fit_view=true; ui->refresh_models();
                    const auto tracks=ui->app.services().projects->state().project->tracks;
                    if (GetWindowLongPtrW(ui->child(arm_button),GWL_STYLE) & WS_VISIBLE || GetWindowLongPtrW(ui->child(monitor_button),GWL_STYLE) & WS_VISIBLE)
                        throw std::runtime_error("Global Arm/Monitor controls remain visible");
                    for (std::size_t n=0; n<2; ++n) {
                        const auto header=ui->mini_rect(n);
                        if (!ui->mini_down({header.right-ui->s(65),header.top+ui->s(10)}) || !ui->app.track_armed(tracks[n].id))
                            throw std::runtime_error("Track Arm click did not arm independently");
                    }
                    if (ui->app.armed_tracks().size() != 2) throw std::runtime_error("Arming a second track cleared the first");
                    ui->app.set_track_monitoring(tracks[0].id,false); ui->app.set_track_monitoring(tracks[1].id,false);
                    const auto header=ui->mini_rect(1); const auto monitor_revision=ui->app.services().projects->state().revision;
                    if (!ui->mini_down({header.right-ui->s(25),header.top+ui->s(10)})) throw std::runtime_error("Track Monitor click missed");
                    const auto monitored=ui->app.services().projects->state().project;
                    if (monitored->tracks[0].input_monitor || !monitored->tracks[1].input_monitor || ui->app.services().projects->state().revision != monitor_revision+1)
                        throw std::runtime_error("Track Monitor did not use an independent project command");
                    if (!ui->app.undo() || ui->app.services().projects->state().project->tracks[1].input_monitor) throw std::runtime_error("Monitor Undo failed");
                    ui->app.set_track_armed(tracks[0].id,false); ui->app.set_track_armed(tracks[1].id,false);
                    if (!ui->app.stereo_track(tracks[0].id)) throw std::runtime_error("Stereo playback did not select L/R track meters");
                    const auto saved_peaks=ui->mix_meters;
                    ui->mix_meters.tracks[0]={1,0};
                    RECT client{}; GetClientRect(hwnd,&client); const auto screen=GetDC(hwnd), meter_dc=CreateCompatibleDC(screen);
                    const auto bitmap=CreateCompatibleBitmap(screen,client.right,client.bottom); const auto old_bitmap=SelectObject(meter_dc,bitmap);
                    SendMessageW(hwnd,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(meter_dc),PRF_CLIENT);
                    const auto first_header=ui->mini_rect(0); const auto left_pixel=GetPixel(meter_dc,first_header.left+ui->s(40),first_header.top+ui->s(51));
                    const auto right_pixel=GetPixel(meter_dc,first_header.left+ui->s(40),first_header.top+ui->s(60));
                    SelectObject(meter_dc,old_bitmap); DeleteObject(bitmap); DeleteDC(meter_dc); ReleaseDC(hwnd,screen); ui->mix_meters=saved_peaks;
                    if (left_pixel != RGB(230,70,60) || right_pixel != RGB(22,22,22)) throw std::runtime_error("Stereo L/R track meters did not draw independently");

                    ui->first_mix_track=0;
                    const auto insert_button=ui->insert_control(ui->mix_strip(0));
                    ui->mixer_down({insert_button.left+ui->s(20),insert_button.top+ui->s(10)});
                    if (!ui->fx_window || ui->fx_target!=tracks[0].id) throw std::runtime_error("Track Inserts click did not open its chain");
                    const auto fx_command=[&](int id) { SendMessageW(ui->fx_window,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(ui->fx_window,id))); };
                    fx_command(fx_add); SetWindowTextW(GetDlgItem(ui->fx_window,fx_value),L"-6"); fx_command(fx_apply);
                    if (std::abs(fx_chain(*ui).front().gain-std::pow(10.0f,-6.0f/20))>0.00001f) throw std::runtime_error("Native Gain parameter Apply failed");
                    SendMessageW(GetDlgItem(ui->fx_window,fx_kind),CB_SETCURSEL,1,0); fx_command(fx_add);
                    SetWindowTextW(GetDlgItem(ui->fx_window,fx_value),L"3"); SetWindowTextW(GetDlgItem(ui->fx_window,fx_frequency),L"1500"); fx_command(fx_apply);
                    fx_command(fx_up); if (fx_chain(*ui).front().kind!=InsertKind::channel_eq) throw std::runtime_error("Insert reorder failed");
                    fx_command(fx_bypass); if (!fx_chain(*ui).front().bypass) throw std::runtime_error("Insert bypass failed");
                    ui->command(undo,0); if (fx_chain(*ui).front().bypass) throw std::runtime_error("Insert Undo failed");
                    fx_command(fx_down); fx_command(fx_remove); if (fx_chain(*ui).size()!=1) throw std::runtime_error("Insert remove failed");
                    ui->command(undo,0); if (fx_chain(*ui).size()!=2) throw std::runtime_error("Insert remove Undo failed");
                    ui->fx_selection=1;ui->fx_refresh();
                    const auto point=ui->eq_point(fx_chain(*ui)[1],2);const auto eq_revision=ui->app.services().projects->state().revision;
                    ui->eq_down(point);ui->eq_move({point.x+ui->s(20),point.y-ui->s(20)});
                    if(ui->app.services().projects->state().revision!=eq_revision)throw std::runtime_error("EQ preview created Undo entries");
                    ui->eq_up({point.x+ui->s(20),point.y-ui->s(20)});
                    if(fx_chain(*ui)[1].bands[2].frequency<=1500||fx_chain(*ui)[1].bands[2].gain<=3)throw std::runtime_error("EQ points did not change frequency/gain");
                    const auto q=fx_chain(*ui)[1].bands[2].q;ui->eq_wheel(ui->eq_point(fx_chain(*ui)[1],2),WHEEL_DELTA);if(fx_chain(*ui)[1].bands[2].q<=q)throw std::runtime_error("EQ wheel did not change Q");
                    const auto before_cancel=fx_chain(*ui);ui->eq_down(ui->eq_point(before_cancel[1],2));ui->eq_move({point.x,point.y});ui->eq_cancel();if(fx_chain(*ui)!=before_cancel)throw std::runtime_error("EQ cancel changed project");
                    if (ui->render_preview) ui->export_preview(ui->fx_window);
                    for (auto control=GetWindow(ui->fx_window,GW_CHILD);control;control=GetWindow(control,GW_HWNDNEXT)) {
                        RECT fx_client{},bounds{}; GetClientRect(ui->fx_window,&fx_client); GetWindowRect(control,&bounds); MapWindowPoints(nullptr,ui->fx_window,reinterpret_cast<POINT*>(&bounds),2);
                        if (bounds.bottom>fx_client.bottom || bounds.right>fx_client.right) throw std::runtime_error("Insert editor control clipped");
                    }
                    ui->open_fx(std::nullopt); fx_command(fx_add);
                    if (ui->app.services().projects->state().project->master_inserts.size()!=1 || ui->app.services().projects->state().project->tracks[0].inserts.size()!=2) throw std::runtime_error("Master Inserts changed the track chain");
                    DestroyWindow(ui->fx_window);

                    NativeInsert cab;cab.id=new_id();cab.kind=InsertKind::cab_ir;cab.ir.name="MRS test stereo impulse";cab.ir.channels=2;cab.ir.samples.assign(1024,0.f);cab.ir.samples[0]=1;cab.ir.samples[1]=.5f;cab.ir.samples[256]=.2f;
                    ui->app.set_inserts(tracks[0].id,{cab});ui->open_insert(tracks[0].id,0);if(!ui->fx_window||!(GetWindowLongPtrW(GetDlgItem(ui->fx_window,ir_mix),GWL_STYLE)&WS_VISIBLE))throw std::runtime_error("Cab IR did not open its native editor");
                    SetWindowTextW(GetDlgItem(ui->fx_window,ir_mix),L"0.6");SetWindowTextW(GetDlgItem(ui->fx_window,fx_value),L"-3");fx_command(fx_apply);fx_command(ir_polarity);if(std::abs(fx_chain(*ui)[0].ir.mix-.6f)>1e-6f||!fx_chain(*ui)[0].ir.invert)throw std::runtime_error("Cab controls failed");
                    SendMessageW(GetDlgItem(ui->fx_window,ir_preset),CB_SETCURSEL,2,0);fx_command(ir_preset);if(fx_chain(*ui)[0].ir.low_cut!=80||fx_chain(*ui)[0].ir.high_cut!=5000)throw std::runtime_error("Cab preset failed");
                    if(ui->render_preview){ui->export_preview(ui->fx_window);std::filesystem::copy_file(ui->folder/L"0.1m-inserts-preview.bmp",ui->folder/L"0.1m-cab-ir-preview.bmp",std::filesystem::copy_options::overwrite_existing);}DestroyWindow(ui->fx_window);
#ifdef MRS_HAS_VST3
                    wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);const auto fixture=std::filesystem::path(module).parent_path()/L"mrs_vst3_fixture.vst3";
                    if(std::filesystem::exists(fixture)){
                        ui->vst_catalog=processing::probe_vst3(narrow(fixture.wstring()));if(ui->vst_catalog.empty())throw std::runtime_error("Browser fixture missing");ui->browser_refresh();auto tree=ui->child(browser_tree);auto vendor=TreeView_GetRoot(tree);if(!vendor||!TreeView_GetChild(tree,vendor))throw std::runtime_error("Browser vendor grouping failed");if(TreeView_GetItemState(tree,vendor,TVIS_EXPANDED)&TVIS_EXPANDED)throw std::runtime_error("Browser folders should start collapsed");
                        const auto shown_canvas=ui->canvas;ui->command(browser_toggle,BN_CLICKED);if(ui->browser_visible||(GetWindowLongPtrW(tree,GWL_STYLE)&WS_VISIBLE)||ui->canvas.right<=shown_canvas.right)throw std::runtime_error("BROWS hide/arrangement resize failed");
                        ui->command(browser_toggle,BN_CLICKED);if(!ui->browser_visible||!EqualRect(&ui->canvas,&shown_canvas))throw std::runtime_error("BROWS show failed");
                        TreeView_Expand(tree,vendor,TVE_EXPAND);
                        ui->app.connect(std::make_unique<SmokeDevice>(),{0,48000,128,{}, {0,1}});const auto initial_revision=ui->app.services().projects->state().revision;
                        NMTREEVIEWW event{};event.hdr.hwndFrom=tree;event.hdr.idFrom=browser_tree;event.hdr.code=TVN_BEGINDRAGW;event.itemNew.lParam=1;SendMessageW(hwnd,WM_NOTIFY,browser_tree,reinterpret_cast<LPARAM>(&event));
                        if(!ui->plugin_drag||GetCapture()!=hwnd)throw std::runtime_error("Tree drag notification failed");ui->browser_up({-5,-5});if(ui->app.services().projects->state().revision!=initial_revision)throw std::runtime_error("Invalid drop changed project");
                        SendMessageW(hwnd,WM_NOTIFY,browser_tree,reinterpret_cast<LPARAM>(&event));ui->browser_cancel();if(ui->app.services().projects->state().revision!=initial_revision)throw std::runtime_error("Cancelled drag changed project");
                        auto strip=ui->mix_strip(0);POINT drop{strip.left+ui->s(40),strip.top+ui->s(60)};SendMessageW(hwnd,WM_NOTIFY,browser_tree,reinterpret_cast<LPARAM>(&event));SendMessageW(hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(drop.x,drop.y));SendMessageW(hwnd,WM_LBUTTONUP,0,MAKELPARAM(drop.x,drop.y));
                        if(ui->app.services().projects->state().project->tracks[0].inserts.size()!=2||ui->app.services().projects->state().revision!=initial_revision+1)throw std::runtime_error("Plugin drop did not append one Undo command");
                        const auto slot=ui->insert_slot(strip,1);ui->mixer_down({slot.left+ui->s(4),slot.top+ui->s(4)});if(!ui->vst_editor)throw std::runtime_error("Single insert click did not open native VST3 editor");auto editor=ui->vst_editor;ui->mixer_down({slot.left+ui->s(4),slot.top+ui->s(4)});if(ui->vst_editor!=editor)throw std::runtime_error("Repeated click recreated plugin editor");ui->command(undo,0);if(ui->vst_editor)throw std::runtime_error("Undo left an obsolete plugin window open");ui->command(redo,0);ui->mixer_down({slot.left+ui->s(4),slot.top+ui->s(4)});if(!ui->vst_editor)throw std::runtime_error("Redo did not permit reopening plugin editor");ui->close_vst_editor();
                        ui->open_fx(tracks[0].id);SendMessageW(GetDlgItem(ui->fx_window,fx_list),LB_SETCURSEL,1,0);SendMessageW(ui->fx_window,WM_COMMAND,MAKEWPARAM(fx_list,LBN_SELCHANGE),0);if(!ui->vst_editor)throw std::runtime_error("Insert list click did not open VST editor");DestroyWindow(ui->fx_window);
                        strip=ui->mix_strip(0,true);drop={strip.left+ui->s(20),strip.top+ui->s(65)};SendMessageW(hwnd,WM_NOTIFY,browser_tree,reinterpret_cast<LPARAM>(&event));ui->browser_up(drop);if(ui->app.services().projects->state().project->master_inserts.size()!=2)throw std::runtime_error("Master plugin drop failed");if(!ui->app.undo()||ui->app.services().projects->state().project->master_inserts.size()!=1)throw std::runtime_error("Drop Undo failed");
                        const auto bus=ui->app.add_bus("Browser bus");ui->first_mix_track=ui->mix_tracks().size()-1;strip=ui->mix_strip(0);drop={strip.left+ui->s(20),strip.top+ui->s(60)};SendMessageW(hwnd,WM_NOTIFY,browser_tree,reinterpret_cast<LPARAM>(&event));ui->browser_up(drop);bool found=false;for(const auto& t:ui->app.services().projects->state().project->tracks)if(t.id==bus)found=t.inserts.size()==1;if(!found)throw std::runtime_error("Bus plugin drop failed");ui->first_mix_track=0;ui->app.disconnect();ui->refresh_models();
                    }
#endif
                    if (ui->render_preview) { ui->first_mix_track=0; ui->export_preview(); }
                    // Exercise DPI layout with the same path as a monitor change.
                    ui->dpi = 144; ui->fonts();
                    SetWindowPos(hwnd,nullptr,0,0,ui->s(1000),ui->s(620),SWP_NOMOVE | SWP_NOZORDER);
                    ui->layout(); ui->settings_dpi = 144; ui->settings_fonts();
                    const auto route_bounds = ui->output_control(ui->mix_strip(0),true), remove_bounds = ui->remove_bus_control(ui->mix_strip(0));
                    if (route_bounds.bottom > ui->mix_strip(0).bottom || remove_bounds.bottom > ui->mix_strip(0).bottom) throw std::runtime_error("Routing controls clipped at minimum DPI layout");
                    SetWindowPos(ui->settings,nullptr,0,0,ui->ss(600),ui->ss(475),SWP_NOMOVE | SWP_NOZORDER); ui->settings_layout();
                    for (auto parent : {hwnd,ui->settings}) {
                        RECT area{}; GetClientRect(parent,&area);
                        for (auto control = GetWindow(parent,GW_CHILD); control; control = GetWindow(control,GW_HWNDNEXT)) {
                            RECT rect{}; GetWindowRect(control,&rect); MapWindowPoints(nullptr,parent,reinterpret_cast<POINT*>(&rect),2);
                            if (rect.left < 0 || rect.top < 0 || rect.right > area.right || rect.bottom > area.bottom)
                                throw std::runtime_error("DPI layout extends outside the client area");
                        }
                    }
                    ui->app.connect(std::make_unique<SmokeDevice>(),{0,48000,128,{},{0,1}});
                    ui->app.seek(500); std::array<float,256> idle_output{}; ui->app.engine()->process(nullptr,idle_output.data(),128);
                    PostMessageW(hwnd,WM_KEYDOWN,VK_SPACE,0);
                }
                if (ui->smoke_step == 7) {
                    std::array<float,256> output{}; ui->app.engine()->process(nullptr,output.data(),128);
                    if (ui->app.engine()->state().playback!=PlaybackState::playing || ui->app.engine()->state().sample!=628) throw std::runtime_error("Space did not start playback");
                    PostMessageW(hwnd,WM_KEYDOWN,VK_SPACE,static_cast<LPARAM>(1LL<<30));
                }
                if (ui->smoke_step == 8) {
                    std::array<float,256> output{}; ui->app.engine()->process(nullptr,output.data(),128);
                    if (ui->app.engine()->state().playback!=PlaybackState::playing) throw std::runtime_error("Held Space toggled transport");
                    PostMessageW(hwnd,WM_KEYDOWN,VK_SPACE,0);
                }
                if (ui->smoke_step == 9) {
                    std::array<float,256> output{}; ui->app.engine()->process(nullptr,output.data(),128);
                    if (ui->app.engine()->state().playback!=PlaybackState::stopped || ui->app.engine()->state().sample!=500) throw std::runtime_error("Space did not Stop/return to start");
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            break;
        case WM_MOUSEWHEEL: {
            POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}; ScreenToClient(hwnd,&point);
            ui->wheel(point,GET_WHEEL_DELTA_WPARAM(wparam),GET_KEYSTATE_WPARAM(wparam)); return 0;
        }
        case WM_LBUTTONDOWN: ui->mouse_down({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}); return 0;
        case WM_LBUTTONDBLCLK: { POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}; const auto area = ui->mix_area(); if (ui->app.workspace() == Workspace::mix && PtInRect(&area,point)) ui->mixer_down(point,true); else if (ui->app.workspace() == Workspace::arrange || ui->app.workspace() == Workspace::mix) (void)ui->mini_down(point,true); return 0; }
        case WM_MOUSEMOVE: if(ui->plugin_drag)ui->browser_move({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});else ui->mouse_move({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}); return 0;
        case WM_LBUTTONUP: if(ui->plugin_drag)ui->browser_up({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});else ui->mouse_up({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}); return 0;
        case WM_CAPTURECHANGED: ui->browser_cancel();ui->drag.reset(); ui->cancel_mix_drag(); InvalidateRect(hwnd,nullptr,FALSE); return 0;
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT && ui->app.workspace() == Workspace::arrange) {
                POINT point{}; GetCursorPos(&point); ScreenToClient(hwnd,&point);
                if (auto clip = ui->hit_clip(point)) {
                    const bool edge = std::abs(point.x-ui->sample_x(clip->start)) <= ui->s(7) || std::abs(point.x-ui->sample_x(clip->start+clip->length)) <= ui->s(7);
                    SetCursor(LoadCursorW(nullptr,edge ? IDC_SIZEWE : IDC_SIZEALL)); return TRUE;
                }
            }
            break;
        case WM_CLOSE:
            if (ui->app.recording()) { (void)ui->app.stop_recording(); ui->refresh_models(); }
            if (ui->discard()) { ui->preferences(); DestroyWindow(hwnd); } return 0;
        case WM_DESTROY:
            KillTimer(hwnd,1); KillTimer(hwnd,2); if (ui->settings) DestroyWindow(ui->settings); if (ui->fx_window) DestroyWindow(ui->fx_window);
            ui->close_vst_editor();ui->app.disconnect(); PostQuitMessage(0); return 0;
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
        struct ComScope{HRESULT result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);ComScope(){if(FAILED(result))throw std::runtime_error("Cannot initialize Windows COM for VST3/dialogs");}~ComScope(){CoUninitialize();}} com;
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_TREEVIEW_CLASSES|ICC_TAB_CLASSES};if(!InitCommonControlsEx(&controls))throw std::runtime_error("Cannot initialize plugin browser controls");
        const bool smoke = std::wstring_view(command_line).find(L"--smoke-test") != std::wstring_view::npos;
        UI ui(smoke); ui.render_preview=std::wstring_view(command_line).find(L"--render-preview") != std::wstring_view::npos;
        WNDCLASSW main{}; main.lpfnWndProc = main_proc; main.hInstance = instance; main.lpszClassName = L"MRStudioDesktop";
        main.style = CS_DBLCLKS;
        main.hCursor = LoadCursorW(nullptr,IDC_ARROW);
        if (!RegisterClassW(&main)) throw std::runtime_error("Cannot register main window");
        WNDCLASSW settings = main; settings.lpfnWndProc = settings_proc; settings.lpszClassName = L"MRStudioAudio";
        if (!RegisterClassW(&settings)) throw std::runtime_error("Cannot register settings window");
        WNDCLASSW effects=main; effects.lpfnWndProc=fx_proc; effects.lpszClassName=L"MRStudioInserts"; effects.hbrBackground=ui.panel_brush;
        if (!RegisterClassW(&effects)) throw std::runtime_error("Cannot register insert window");
        const auto window = CreateWindowExW(WS_EX_CONTROLPARENT,main.lpszClassName,L"Moon River Studio",WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT,CW_USEDEFAULT,1280,980,nullptr,nullptr,instance,&ui);
        if (!window) throw std::runtime_error("Cannot create Studio window");
        MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor); GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        SetWindowPos(window,nullptr,monitor.rcWork.left,monitor.rcWork.top,std::min<LONG>(ui.s(1280),monitor.rcWork.right-monitor.rcWork.left),std::min<LONG>(ui.s(980),monitor.rcWork.bottom-monitor.rcWork.top),SWP_NOZORDER);
        ShowWindow(window,show); UpdateWindow(window);
        MSG msg{};
        for (;;) {
            const auto result = GetMessageW(&msg,nullptr,0,0); if (result < 0) return 1; if (result == 0) break;

            if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE&&ui.plugin_drag){ui.browser_cancel();continue;}
            if(ui.eq_window&&msg.message==WM_MOUSEWHEEL){POINT p{GET_X_LPARAM(msg.lParam),GET_Y_LPARAM(msg.lParam)};ScreenToClient(ui.eq_window,&p);RECT r{};GetClientRect(ui.eq_window,&r);if(PtInRect(&r,p)){SendMessageW(ui.eq_window,msg.message,msg.wParam,msg.lParam);continue;}}
            if(ui.vst_editor&&(msg.hwnd==ui.vst_editor||IsChild(ui.vst_editor,msg.hwnd))){TranslateMessage(&msg);DispatchMessageW(&msg);continue;}
            if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE&&ui.eq_dragging){ui.eq_cancel();continue;}
            if (msg.message == WM_KEYDOWN && !(ui.settings && IsChild(ui.settings,msg.hwnd)) && !(ui.fx_window && (msg.hwnd==ui.fx_window || IsChild(ui.fx_window,msg.hwnd)))) { try {
                const bool editing = msg.hwnd == ui.child(rename_edit);
                if (!editing && msg.wParam == VK_ESCAPE && ui.drag) { ui.cancel_drag(); continue; }
                if (!editing && msg.wParam == VK_ESCAPE && ui.mix_drag) { ui.cancel_mix_drag(); continue; }
                if (!editing && ui.app.workspace() == Workspace::arrange && !(GetKeyState(VK_CONTROL)&0x8000)) {
                    if (msg.wParam == 'R') { ui.command(record_button,0); continue; }
                    if (msg.wParam == 'S') { ui.command(split_clip_button,0); continue; }
                    if (msg.wParam == VK_DELETE) { ui.command(delete_clip_button,0); continue; }
                }
                if (!editing && msg.wParam == VK_SPACE) { if (!(msg.lParam & (1LL<<30))) ui.space_action(); continue; }
                if (GetKeyState(VK_CONTROL) & 0x8000) {
                    int id{}; if (msg.wParam == 'S') id = (GetKeyState(VK_SHIFT)&0x8000) ? save_as : save;
                    if (!editing && msg.wParam == 'N') id = new_project_button;
                    if (!editing && msg.wParam == 'O') id = open;
                    if (!editing && msg.wParam == 'I') id = import_batch; if (!editing && msg.wParam == 'Z') id = undo; if (!editing && msg.wParam == 'Y') id = redo;
                    if (id) { ui.command(id,0); continue; }
                }
            } catch (const std::exception& e) { ui.error(e); continue; } }
            if (ui.settings && IsDialogMessageW(ui.settings,&msg)) continue;
            if (ui.fx_window && IsDialogMessageW(ui.fx_window,&msg)) continue;
            if (IsDialogMessageW(window,&msg)) continue;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    } catch (const std::exception& e) {
        if (std::wstring_view(command_line).find(L"--smoke-test") == std::wstring_view::npos) MessageBoxW(nullptr,wide(e.what()).c_str(),L"Moon River Studio",MB_OK | MB_ICONERROR);
        return 1;
    }
}
