#include "imgui_manager.h"
#include "isfp_plugin.h"
#include "mouse_yoke.h"
#include "utils.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl2.h"
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <cctype>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <filesystem>

#include <windows.h>
#include <gl/GL.h>
#include <commdlg.h>
#include <wincodec.h>   // WIC for image loading
#include "logger.h"
#pragma comment(lib, "windowscodecs.lib")

namespace ISFP {

// File-scope clipboard tracking flag.
// Set by SetClipboardTextFn when ImGui copies text internally (e.g. selected text from InputTextEx).
// Reset at the start of each InputTextMultiline in efb.cpp.
static bool s_clipboard_set_this_frame = false;

void ValidateAndUpdateCSLConfig();

// XPLM key sniffer callback to block game hotkeys during binding or text input
static int KeySnifferCallback(char inChar, XPLMKeyFlags inFlags, char inVirtualKey, void* inRefcon) {
    ImGuiManager* self = static_cast<ImGuiManager*>(inRefcon);
    if (!self) return 1;
    if (self->IsBinding()) return 0;
    // Only block keys when an ImGui text-input field is actually focused.
    // Using WantCaptureKeyboard here would block ALL keys whenever the mouse
    // hovers the UI, making X-Plane unresponsive while the panel is open.
    if (ImGui::GetIO().WantTextInput) return 0;
    return 1; // pass through
}

// No-op draw callback for XPLM mouse capture window
void NoOpDraw(XPLMWindowID, void*) {}

// XPLM mouse capture callback - only consumes clicks that fall within the
// ImGui window bounds, so the user can still interact with X-Plane outside it.
int MouseCaptureHandler(XPLMWindowID window, int x, int y, int mouse, void* refcon) {
    ImGuiManager* self = static_cast<ImGuiManager*>(refcon);
    if (!self) return 0;
    if (self->IsVisible()) {
        // X-Plane coordinates: y-up, bottom-left origin.
        // imgui_win_x_ = left, imgui_win_y_ = bottom edge
        int wx = self->imgui_win_x_;
        int wy = self->imgui_win_y_;
        int ww = self->imgui_win_w_;
        int wh = self->imgui_win_h_;
        if (x >= wx && x <= wx + ww && y >= wy && y <= wy + wh) {
            return 1; // consumed - inside the ImGui window
        }
        // Click is outside the ImGui window → let X-Plane process it
        return 0;
    }
    return 0;
}

// ==================== Localization Strings ====================
static const char* const kStringsEN[(int)ImGuiManager::StringID::COUNT] = {
    "NAVIGATION",              // Navigation
    "Online\nInfo",            // OnlinePlayersATC_Sidebar
    "Aircraft\nMapping",       // CSLModels_Sidebar
    "Flight\nInquiry",         // FlightInquiry_Sidebar
    "Settings",                // Settings_Sidebar
    "Online Info",             // OnlinePlayersATC_Title
    "No players connected.",   // NoPlayersConnected
    "Callsign",                // Callsign
    "Aircraft",                // Aircraft
    "Latitude",                // Latitude
    "Longitude",               // Longitude
    "Altitude",                // Altitude
    "Heading",                 // Heading
    "Speed",                   // Speed
    "Aircraft Mapping",        // CSLMapping_Title
    "Reload CSL Mapping",      // ReloadCSLMapping
    "CSL config file not found.",       // CSLConfigNotFound
    "Failed to parse CSL config.",      // CSLConfigParseFailed
    "CSL Path: %s",            // CSLPath
    "No mapped aircraft found.",        // NoMappedAircraft
    "Mapped Aircraft (%zu):",  // MappedAircraftCount
    "Settings",                // Settings_Title
    "Enable:FSD Connect",              // EnableFSD
    "Enable:CSL Mapping",              // EnableCSL
    "Enable:Debug Logger",              // EnableLog
    "Startup",                 // Startup
    "Hide UI on startup",      // HideUIOnStartup
    "UI Font",                 // UIFont
    "\xE4\xB8\xAD\xE6\x96\x87", // LangToggle: "中文" (when current is EN)
    "ISFP-UI-Panel",           // ISFPBridgeTitle
    "Shortcut",                // ToggleUIHotkey
    "Show/Hide Mouse Roller",  // ToggleMouseRollerHotkey
    "Click to bind",           // HotkeyBind
    "Press keys...", // HotkeyBinding
    "Unbound",                 // HotkeyUnbound
    "UI Scale",                      // UIScale
};

static const char* const kStringsCN[(int)ImGuiManager::StringID::COUNT] = {
    "\xE5\xAF\xBC\xE8\x88\xAA",              // Navigation: 导航
    "\xE5\x9C\xA8\xE7\xBA\xBF\xE4\xBF\xA1\xE6\x81\xAF", // OnlinePlayersATC_Sidebar: 在线信息
    "\xE6\x9C\xBA\xE6\xA8\xA1\xE6\x98\xA0\xE5\xB0\x84", // CSLModels_Sidebar: 机模映射
    "\xE8\x88\xAA\xE6\x83\x85\xE6\x9F\xA5\xE8\xAF\xA2", // FlightInquiry_Sidebar: 航情查询
    "\xE8\xAE\xBE\xE7\xBD\xAE",              // Settings_Sidebar: 设置
    "\xE5\x9C\xA8\xE7\xBA\xBF\xE4\xBF\xA1\xE6\x81\xAF", // OnlinePlayersATC_Title: 在线信息
    "\xE6\x9A\x82\xE6\x97\xA0\xE5\x9C\xA8\xE7\xBA\xBF\xE7\x8E\xA9\xE5\xAE\xB6\xE3\x80\x82", // NoPlayersConnected: 暂无在线玩家。
    "\xE5\x91\xBC\xE5\x8F\xB7",              // Callsign: 呼号
    "\xE6\x9C\xBA\xE5\x9E\x8B",              // Aircraft: 机型
    "\xE7\xBA\xAC\xE5\xBA\xA6",              // Latitude: 纬度
    "\xE7\xBB\x8F\xE5\xBA\xA6",              // Longitude: 经度
    "\xE9\xAB\x98\xE5\xBA\xA6",              // Altitude: 高度
    "\xE8\x88\xAA\xE5\x90\x91",              // Heading: 航向
    "\xE9\x80\x9F\xE5\xBA\xA6",              // Speed: 速度
    "\xE6\x9C\xBA\xE6\xA8\xA1\xE6\x98\xA0\xE5\xB0\x84", // CSLMapping_Title: 机模映射
    "\xE9\x87\x8D\xE6\x96\xB0\xE5\x8A\xA0\xE8\xBD\xBD CSL \xE6\x98\xA0\xE5\xB0\x84", // ReloadCSLMapping: 重新加载 CSL 映射
    "\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0 CSL \xE9\x85\x8D\xE7\xBD\xAE\xE6\x96\x87\xE4\xBB\xB6\xE3\x80\x82", // CSLConfigNotFound: 未找到 CSL 配置文件。
    "CSL \xE9\x85\x8D\xE7\xBD\xAE\xE6\x96\x87\xE4\xBB\xB6\xE8\xA7\xA3\xE6\x9E\x90\xE5\xA4\xB1\xE8\xB4\xA5\xE3\x80\x82", // CSLConfigParseFailed: CSL 配置文件解析失败。
    "CSL \xE8\xB7\xAF\xE5\xBE\x84: %s",     // CSLPath: CSL 路径: %s
    "\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE6\x98\xA0\xE5\xB0\x84\xE7\x9A\x84\xE6\x9C\xBA\xE5\x9E\x8B\xE3\x80\x82", // NoMappedAircraft: 未找到映射的机型。
    "\xE5\xB7\xB2\xE6\x98\xA0\xE5\xB0\x84\xE6\x9C\xBA\xE5\x9E\x8B (%zu):", // MappedAircraftCount: 已映射机型 (%zu):
    "\xE8\xAE\xBE\xE7\xBD\xAE",              // Settings_Title: 设置
    "\xE5\x90\xAF\xE7\x94\xA8:FSD\xE8\xBF\x9E\xE6\x8E\xA5",          // EnableFSD: 启用:FSD连接
    "\xE5\x90\xAF\xE7\x94\xA8:CSL\xE6\x98\xA0\xE5\xB0\x84",          // EnableCSL: 启用:CSL映射
    "\xE5\x90\xAF\xE7\x94\xA8:\xE6\x97\xA5\xE5\xBF\x97\xE8\xAE\xB0\xE5\xBD\x95", // EnableLog: 启用日志记录
    "\xE5\x90\xAF\xE5\x8A\xA8",              // Startup: 启动
    "\xE5\x90\xAF\xE5\x8A\xA8\xE6\x97\xB6\xE9\x9A\x90\xE8\x97\x8F UI", // HideUIOnStartup: 启动时隐藏 UI
    "UI \xE5\xAD\x97\xE4\xBD\x93",           // UIFont: UI 字体
    "EN",                    // LangToggle: "EN" (when current is CN)
    "ISFP-UI\xe9\x9d\xa2\xe6\x9d\xbf", // ISFPBridgeTitle: ISFP-UI面板
    "\xe5\xbf\xab\xe6\x8d\xb7\xe9\x94\xae", // ToggleUIHotkey: 快捷键
    "\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f\xe8\x88\xb5\xe9\x9d\xa2\xe8\x81\x94\xe5\x8a\xa8\xe6\xa1\x86", // ToggleMouseRollerHotkey: 显示/隐藏舵面联动框
    "\xe7\x82\xb9\xe5\x87\xbb\xe7\xbb\x91\xe5\xae\x9a", // HotkeyBind: 点击绑定
    "\xe8\xaf\xb7\xe6\x8c\x89\xe4\xb8\x8b\xe6\x8c\x89\xe9\x94\xae...", // HotkeyBinding: 请按下按键...
    "\xe6\x9c\xaa\xe7\xbb\x91\xe5\xae\x9a", // HotkeyUnbound: 未绑定
    "UI \xe7\xbc\xa9\xe6\x94\xbe", // UIScale: UI 缩放
};

const char* ImGuiManager::GetText(StringID id) {
    if (g_language == Language::CN) {
        return kStringsCN[(int)id];
    }
    return kStringsEN[(int)id];
}

// ==================== Background Image ====================
// UnloadBGTexture() -> moved to separate file

// LoadBGTexture() -> moved to separate file

// CopyAndLoadBGImage() -> moved to separate file

// ==================== Theme ====================
// Preset themes: 0=Light Modern, 1=Light Clean, 2=Light Warm, 3=Dark Modern, 4=Dark Deep
struct ThemePreset {
    const char* name_cn;
    const char* name_en;
    float window[3];
    float child[3];
    float button[3];
    float btn_hover[3];
    float border[3];
    float btn_active[3];
    float btn_clicked[3];
    float frame[3];
    float frame_active[3];
    float popup[3];
    float dropdown_frame[3];
    float dropdown_border[3];
    float dropdown_active[3];
    float table_header_text[3];
    float table_header_bg[3];
    float table_content_text[3];
    float table_content_bg[3];
    float slider[3];
    float slider_track[3];
    float slider_track_hover[3];
    float slider_active[3];
    float text[3];
    float check[3];
};

static const ThemePreset kPresets[5] = {
    { "\xe6\xb5\x85\xe8\x89\xb2\xe7\x8e\xb0\xe4\xbb\xa3", "Light Modern",
      { 0.92f,0.93f,0.95f }, { 0.86f,0.88f,0.92f }, { 0.10f,0.50f,0.90f }, { 0.20f,0.60f,0.95f }, { 0.70f,0.72f,0.78f }, { 0.10f,0.50f,0.90f }, { 0.08f,0.40f,0.80f }, { 0.82f,0.84f,0.88f }, { 0.72f,0.74f,0.78f }, { 0.78f,0.80f,0.85f }, { 0.72f,0.74f,0.78f }, { 0.65f,0.67f,0.72f }, { 0.12f,0.52f,0.92f }, { 0.80f,0.82f,0.88f }, { 0.72f,0.74f,0.78f }, { 0.82f,0.84f,0.90f }, { 0.10f,0.50f,0.90f }, { 0.10f,0.10f,0.15f }, { 0.30f,0.30f,0.30f }, { 0.12f,0.52f,0.92f }, { 0.10f,0.10f,0.10f }, { 0.70f,0.70f,0.70f } },
    { "\xe6\xb5\x85\xe8\x89\xb2\xe6\xb4\x81\xe5\x87\x80", "Light Clean",
      { 0.96f,0.96f,0.96f }, { 0.90f,0.91f,0.92f }, { 0.20f,0.60f,0.40f }, { 0.30f,0.70f,0.50f }, { 0.75f,0.76f,0.78f }, { 0.20f,0.60f,0.40f }, { 0.15f,0.50f,0.30f }, { 0.86f,0.87f,0.89f }, { 0.76f,0.77f,0.79f }, { 0.82f,0.84f,0.87f }, { 0.76f,0.77f,0.79f }, { 0.70f,0.71f,0.73f }, { 0.25f,0.65f,0.45f }, { 0.84f,0.85f,0.88f }, { 0.76f,0.77f,0.79f }, { 0.86f,0.87f,0.90f }, { 0.20f,0.60f,0.40f }, { 0.10f,0.10f,0.12f }, { 0.30f,0.30f,0.30f }, { 0.25f,0.65f,0.45f }, { 0.10f,0.10f,0.10f }, { 0.30f,0.30f,0.30f } },
    { "\xe6\xb5\x85\xe8\x89\xb2\xe6\xb8\xa9\xe6\x9a\x96", "Light Warm",
      { 0.95f,0.92f,0.88f }, { 0.88f,0.86f,0.83f }, { 0.75f,0.45f,0.25f }, { 0.85f,0.55f,0.35f }, { 0.72f,0.70f,0.68f }, { 0.75f,0.45f,0.25f }, { 0.65f,0.35f,0.15f }, { 0.84f,0.82f,0.80f }, { 0.74f,0.72f,0.70f }, { 0.80f,0.78f,0.76f }, { 0.74f,0.72f,0.70f }, { 0.68f,0.66f,0.64f }, { 0.80f,0.50f,0.30f }, { 0.82f,0.80f,0.78f }, { 0.74f,0.72f,0.70f }, { 0.84f,0.82f,0.80f }, { 0.75f,0.45f,0.25f }, { 0.15f,0.10f,0.08f }, { 0.30f,0.25f,0.20f }, { 0.80f,0.50f,0.30f }, { 0.12f,0.08f,0.06f }, { 0.30f,0.25f,0.20f } },
    { "\xe6\xb7\xb1\xe8\x89\xb2\xe7\x8e\xb0\xe4\xbb\xa3", "Dark Modern",
      { 0.06f,0.06f,0.06f }, { 0.10f,0.10f,0.12f }, { 0.15f,0.45f,0.80f }, { 0.25f,0.55f,0.85f }, { 0.30f,0.30f,0.35f }, { 0.20f,0.55f,0.90f }, { 0.10f,0.35f,0.70f }, { 0.20f,0.20f,0.25f }, { 0.28f,0.28f,0.33f }, { 0.12f,0.12f,0.16f }, { 0.18f,0.18f,0.22f }, { 0.25f,0.25f,0.30f }, { 0.20f,0.40f,0.70f }, { 0.16f,0.16f,0.22f }, { 0.12f,0.12f,0.16f }, { 0.18f,0.18f,0.24f }, { 0.40f,0.60f,0.90f }, { 0.15f,0.15f,0.20f }, { 0.20f,0.20f,0.28f }, { 0.55f,0.75f,1.00f }, { 0.90f,0.90f,0.92f }, { 0.70f,0.70f,0.70f } },
    { "\xe6\xb7\xb1\xe8\x89\xb2\xe6\xb7\xb1\xe9\x82\x83", "Dark Deep",
      { 0.04f,0.04f,0.08f }, { 0.08f,0.08f,0.14f }, { 0.50f,0.20f,0.70f }, { 0.60f,0.30f,0.80f }, { 0.25f,0.25f,0.35f }, { 0.55f,0.25f,0.75f }, { 0.40f,0.15f,0.60f }, { 0.15f,0.15f,0.25f }, { 0.22f,0.22f,0.32f }, { 0.10f,0.10f,0.18f }, { 0.14f,0.14f,0.22f }, { 0.20f,0.20f,0.28f }, { 0.30f,0.25f,0.50f }, { 0.12f,0.12f,0.20f }, { 0.10f,0.10f,0.18f }, { 0.14f,0.14f,0.22f }, { 0.50f,0.30f,0.70f }, { 0.12f,0.12f,0.18f }, { 0.18f,0.18f,0.25f }, { 0.55f,0.35f,0.75f }, { 0.85f,0.80f,0.95f }, { 0.60f,0.50f,0.70f } },
};

// ApplyPresetTheme() -> moved to separate file

// PushThemeColors() -> moved to separate file

// PopThemeColors() -> moved to separate file

// ==================== ImGuiManager ====================
ImGuiManager::ImGuiManager() = default;
ImGuiManager::~ImGuiManager() { Shutdown(); }

void ImGuiManager::ResetClipboardFlag() { s_clipboard_set_this_frame = false; }
bool ImGuiManager::WasClipboardSetThisFrame() const { return s_clipboard_set_this_frame; }

bool ImGuiManager::Initialize() {
    if (initialized_) return true;

    // Ensure a clean slate — if a previous Shutdown() crashed mid-way, ImGui may
    // still have a stale context that will cause CreateContext() to crash.
    font_dirty_ = false;
    opengl_initialized_ = false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    // Windows clipboard handler for copy/paste support
    // IMPORTANT: Set BOTH io.SetClipboardTextFn (legacy) AND PlatformIO (ImGui 1.91.8+)
    // InputTextEx internally uses PlatformIO.Platform_SetClipboardTextFn for Ctrl+C copy.
    io.SetClipboardTextFn = [](void*, const char* text) {
        s_clipboard_set_this_frame = true;
        Logger::ImGui("ISFP-xLink:Debug:io.SetClipboardTextFn called\n");
        if (!text || !*text) return;
        size_t len = strlen(text) + 1;
        HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE, len);
        if (!hglb) return;
        char* pch = (char*)GlobalLock(hglb);
        if (pch) { memcpy(pch, text, len); GlobalUnlock(hglb); }
        if (OpenClipboard(nullptr)) { EmptyClipboard(); SetClipboardData(CF_TEXT, hglb); CloseClipboard(); }
        else { GlobalFree(hglb); }
    };
    io.GetClipboardTextFn = [](void*) -> const char* {
        if (!OpenClipboard(nullptr)) return "";
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (!hData) { CloseClipboard(); return ""; }
        char* pszText = (char*)GlobalLock(hData);
        if (!pszText) { CloseClipboard(); return ""; }
        static std::string s_clip;
        s_clip = pszText;
        GlobalUnlock(hData);
        CloseClipboard();
        return s_clip.c_str();
    };

    // ImGui 1.91.8+ uses PlatformIO.Platform_SetClipboardTextFn internally (InputTextEx, etc.)
    // The legacy io.SetClipboardTextFn compatibility wrapper only kicks in during
    // ImGui::CreateContext() / Initialize(), which runs BEFORE we set io.SetClipboardTextFn.
    // So we must set PlatformIO directly here.
    {
        ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
        pio.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
            s_clipboard_set_this_frame = true;
            Logger::ImGui("ISFP-xLink:Debug:Platform_SetClipboardTextFn called\n");
            if (!text || !*text) return;
            size_t len = strlen(text) + 1;
            HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE, len);
            if (!hglb) return;
            char* pch = (char*)GlobalLock(hglb);
            if (pch) { memcpy(pch, text, len); GlobalUnlock(hglb); }
            if (OpenClipboard(nullptr)) { EmptyClipboard(); SetClipboardData(CF_TEXT, hglb); CloseClipboard(); }
            else { GlobalFree(hglb); }
        };
        pio.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
            if (!OpenClipboard(nullptr)) return "";
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (!hData) { CloseClipboard(); return ""; }
            char* pszText = (char*)GlobalLock(hData);
            if (!pszText) { CloseClipboard(); return ""; }
            static std::string s_clip;
            s_clip = pszText;
            GlobalUnlock(hData);
            CloseClipboard();
            return s_clip.c_str();
        };
    }

    // Determine Fonts directory path via X-Plane system path + relative plugin path
    char xp_root[512] = {};
    XPLMGetSystemPath(xp_root);
    font_dir_ = std::string(xp_root) + "Resources\\plugins\\ISFP_xLink\\Data\\Fonts\\";
    Logger::ImGui(("ISFP-xLink:ImGui:Fonts dir: " + font_dir_ + "\n").c_str());

    // Detect DPI scale first
    {
        HDC screen_dc = GetDC(nullptr);
        if (screen_dc) {
            int dpi = GetDeviceCaps(screen_dc, LOGPIXELSY);
            ReleaseDC(nullptr, screen_dc);
            dpi_scale_ = dpi / 96.0f;
            if (dpi_scale_ < 1.0f) dpi_scale_ = 1.0f;
            Logger::ImGui(("ISFP-xLink:ImGui:DPI scale = " +
                std::to_string(dpi_scale_) + "\n").c_str());
        }
    }

    // Load saved UI settings (includes ui_scale_)
    ScanFonts();
    LoadUISettings();

    // Load font at scaled size with CJK fallback
    float init_size = base_font_size_ * dpi_scale_ * ui_scale_;
    if (init_size < 8.0f) init_size = 8.0f;

    if (current_font_index_ > 0 && current_font_index_ < (int)available_fonts_.size()) {
        std::string fp = font_dir_ + available_fonts_[current_font_index_];
        io.Fonts->AddFontFromFileTTF(fp.c_str(), init_size, nullptr, nullptr);
    } else {
        // Prefer msyh.ttc as primary font (has both Latin and CJK glyphs)
        const char* base_candidates[] = {
            "C:\\Windows\\Fonts\\msyh.ttc", "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf", nullptr
        };
        bool loaded = false;
        for (const char* fp : base_candidates) {
            if (fp && GetFileAttributesA(fp) != INVALID_FILE_ATTRIBUTES) {
                io.Fonts->AddFontFromFileTTF(fp, init_size, nullptr, nullptr);
                loaded = true;
                break;
            }
        }
        if (!loaded) io.Fonts->AddFontDefault();
    }
    MergeCJKFallbackFont(init_size);

    // Modern dark style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.45f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);

    // Create full-screen XPLM capture window (click blocker — game-integrated, not system-wide)
    {
        int screen_w, screen_h;
        XPLMGetScreenSize(&screen_w, &screen_h);
        XPLMCreateWindow_t capParams = {};
        capParams.structSize = sizeof(capParams);
        capParams.left = 0;
        capParams.top = screen_h;
        capParams.right = screen_w;
        capParams.bottom = 0;
        capParams.visible = 1;
        capParams.drawWindowFunc = NoOpDraw;
        capParams.handleMouseClickFunc = MouseCaptureHandler;
        capParams.refcon = this;
        capParams.decorateAsFloatingWindow = 0;
        mouse_capture_window_ = (void*)XPLMCreateWindowEx(&capParams);
        if (mouse_capture_window_) {
            XPLMBringWindowToFront((XPLMWindowID)mouse_capture_window_);
            Logger::ImGui("ISFP-xLink:ImGui:全屏捕获窗口已创建\n");
        } else {
            Logger::ImGui("ISFP-xLink:ImGui:全屏捕获窗口创建失败\n");
        }
    }

    // Register key sniffer to block X-Plane hotkeys during binding
    XPLMRegisterKeySniffer(KeySnifferCallback, 1, this);

    BuildAiracList();
    // Use a global counter so each reload gets a unique version number,
    // ensuring all static state caches (s_static_version, ctrl_map.version, etc.)
    // are properly invalidated and reset on every Initialize() call.
    { static int s_global_reload_counter = 0; reload_version_ = ++s_global_reload_counter; }

    initialized_ = true;
    Logger::ImGui("ISFP-xLink:ImGui:ImGui manager initialized\n");
    return true;
}

// Build AIRAC cycle list using Windows FILETIME for reliable date arithmetic
// BuildAiracList() -> moved to separate file

void ImGuiManager::Shutdown() {
    if (!initialized_) return;
    initialized_ = false;

#pragma warning(push)
#pragma warning(disable: 2712)
    __try {
        // Unregister key sniffer FIRST (needs ImGui::GetIO)
        XPLMUnregisterKeySniffer(KeySnifferCallback, 1, this);

        // Destroy XPLM capture window
        if (mouse_capture_window_) {
            XPLMDestroyWindow((XPLMWindowID)mouse_capture_window_);
            mouse_capture_window_ = nullptr;
        }

        // Clean up background texture
        UnloadBGTexture();

        if (opengl_initialized_) {
            ImGui_ImplOpenGL2_Shutdown();
            opengl_initialized_ = false;
        }

        // Destroy ImGui context last (after all ImGui-dependent cleanup)
        ImGui::DestroyContext();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Logger::ImGui("ISFP-xLink:ImGui:Shutdown caught exception, forcing state reset\n");
        mouse_capture_window_ = nullptr;
        opengl_initialized_ = false;
    }
#pragma warning(pop)

    // Reset all state to defaults so a second reload starts clean
    visible_ = true;
    hide_on_startup_ = false;
    font_dirty_ = false;
    window_geom_loaded_ = false;
    binding_mode_ = false;
    csl_browser_open_ = false;
    reload_version_ = 0;

    Logger::ImGui("ISFP-xLink:ImGui:ImGui manager shutdown\n");
}

// ScanFonts() -> moved to separate file

// ApplyFont() -> moved to separate file

// MergeCJKFallbackFont() -> moved to separate file

// Helper: true if the X-Plane window is the foreground window
static bool IsGameInFocus() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

void ImGuiManager::CheckGlobalHotkey() {
    // Block hotkey while binding popup is open
    if (binding_mode_) return;
    // Block hotkey when an ImGui input widget has focus (prevent accidental toggle while typing)
    if (ImGui::IsAnyItemActive()) return;
    // Ignore when game is not in focus (e.g. Alt+Tab)
    if (!IsGameInFocus()) return;

    // Always check global hotkey, even when UI is hidden
    bool shift_down = (GetAsyncKeyState(VK_SHIFT) < 0);
    bool ctrl_down = (GetAsyncKeyState(VK_CONTROL) < 0);
    bool alt_down = (GetAsyncKeyState(VK_MENU) < 0);

    // Validate g_hotkey.vkey is a real key before checking
    bool vkey_valid = (g_hotkey.vkey >= '0' && g_hotkey.vkey <= '9') ||
                      (g_hotkey.vkey >= 'A' && g_hotkey.vkey <= 'Z') ||
                      g_hotkey.vkey == VK_SPACE || g_hotkey.vkey == VK_RETURN ||
                      g_hotkey.vkey == VK_BACK || g_hotkey.vkey == VK_TAB ||
                      g_hotkey.vkey == VK_DELETE || g_hotkey.vkey == VK_INSERT ||
                      g_hotkey.vkey == VK_HOME || g_hotkey.vkey == VK_END ||
                      g_hotkey.vkey == VK_PRIOR || g_hotkey.vkey == VK_NEXT ||
                      g_hotkey.vkey == VK_LEFT || g_hotkey.vkey == VK_RIGHT ||
                      g_hotkey.vkey == VK_UP || g_hotkey.vkey == VK_DOWN ||
                      (g_hotkey.vkey >= VK_F1 && g_hotkey.vkey <= VK_F12) ||
                      g_hotkey.vkey == VK_OEM_PLUS || g_hotkey.vkey == VK_OEM_MINUS ||
                      g_hotkey.vkey == VK_OEM_COMMA || g_hotkey.vkey == VK_OEM_PERIOD;

    bool modifiers_match = vkey_valid &&
                            (shift_down == g_hotkey.shift &&
                             ctrl_down == g_hotkey.ctrl &&
                             alt_down == g_hotkey.alt);

    bool key_down = false;
    if (modifiers_match) {
        key_down = (GetAsyncKeyState(g_hotkey.vkey) < 0);
    }

    // Edge-triggered toggle
    if (key_down && !hotkey_was_down_) {
        ToggleVisibility();
        SyncMenuCheckmarks();
    }
    hotkey_was_down_ = key_down || (modifiers_match && key_down);
}

void ImGuiManager::CheckGlobalMouseRollerHotkey() {
    if (g_mouseyoke_hotkey.vkey == 0) return; // not bound
    // Block while binding popup is open
    if (binding_mode_) return;
    // Block hotkey when an ImGui input widget has focus
    if (ImGui::IsAnyItemActive()) return;
    // Ignore when game is not in focus
    if (!IsGameInFocus()) return;

    bool shift_down = (GetAsyncKeyState(VK_SHIFT) < 0);
    bool ctrl_down  = (GetAsyncKeyState(VK_CONTROL) < 0);
    bool alt_down   = (GetAsyncKeyState(VK_MENU) < 0);

    bool mods_match = (shift_down == g_mouseyoke_hotkey.shift &&
                       ctrl_down  == g_mouseyoke_hotkey.ctrl &&
                       alt_down   == g_mouseyoke_hotkey.alt);
    bool key_down = mods_match && (GetAsyncKeyState(g_mouseyoke_hotkey.vkey) < 0);

    if (key_down && !mouseyoke_hk_was_down_) {
        bool new_val = !g_mouseyoke_enabled.load();
        g_mouseyoke_enabled = new_val;
        if (g_mouseyoke) g_mouseyoke->SetHidden(new_val);
        if (g_config) {
            g_config->SetBool("plugin.mouseyoke.hidden", new_val);
            g_config->Save();
        }
    }
    mouseyoke_hk_was_down_ = key_down;
}

void ImGuiManager::HandleKeyBindingCapture() {
    // Reset tracking when not binding
    if (!binding_mode_) {
        bind_just_entered_ = true;
        bind_prev_key_ = 0;
        return;
    }

    // First frame after entering binding mode: clean start
    if (bind_just_entered_) {
        bind_just_entered_ = false;
        pending_hotkey_.vkey = 0;
        pending_hotkey_.shift = false;
        pending_hotkey_.ctrl = false;
        pending_hotkey_.alt = false;
        bind_captured_ = false;
        bind_prev_key_ = 0;
        bind_peak_shift_ = false;
        bind_peak_ctrl_ = false;
        bind_peak_alt_ = false;
        return; // Skip detection this frame to let state settle
    }

    // Once captured, freeze everything — no more updates
    if (bind_captured_) return;

    // While any key is held, accumulate peak modifier states
    bool shift_now = (GetAsyncKeyState(VK_SHIFT) < 0);
    bool ctrl_now  = (GetAsyncKeyState(VK_CONTROL) < 0);
    bool alt_now   = (GetAsyncKeyState(VK_MENU) < 0);

    // Find which non-modifier key is currently pressed (whitelist only)
    int current_key = 0;
    static const int kKnownKeys[] = {
        '0','1','2','3','4','5','6','7','8','9',
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        VK_SPACE, VK_RETURN, VK_BACK, VK_TAB, VK_DELETE,
        VK_INSERT, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
        VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
        VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8,
        VK_F9, VK_F10, VK_F11, VK_F12,
        VK_OEM_1, VK_OEM_2, VK_OEM_3, VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7,
        VK_OEM_PLUS, VK_OEM_MINUS, VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_8,
        0
    };
    for (int i = 0; kKnownKeys[i] != 0; i++) {
        if (GetAsyncKeyState(kKnownKeys[i]) < 0) {
            current_key = kKnownKeys[i];
            break;
        }
    }

    bool key_held = (current_key != 0);              // key currently down
    bool key_was_held = (bind_prev_key_ != 0);       // key was down last frame

    // New key just pressed → reset peaks and show key
    if (key_held && !key_was_held) {
        pending_hotkey_.vkey = current_key;
        bind_peak_shift_ = false;
        bind_peak_ctrl_ = false;
        bind_peak_alt_ = false;
        bind_captured_ = false;
    }

    // While any key is held (or was held last frame), OR in current modifiers
    if (key_held || key_was_held) {
        if (shift_now) bind_peak_shift_ = true;
        if (ctrl_now)  bind_peak_ctrl_ = true;
        if (alt_now)   bind_peak_alt_ = true;
    }

    // Update live display with PEAK modifier values (so released modifiers stay)
    pending_hotkey_.shift = bind_peak_shift_;
    pending_hotkey_.ctrl  = bind_peak_ctrl_;
    pending_hotkey_.alt   = bind_peak_alt_;

    // Key just released (was held, now released) → capture with peak modifiers
    if (!key_held && key_was_held && pending_hotkey_.vkey != 0) {
        bind_captured_ = true;
    }

    bind_prev_key_ = current_key;
}

// RenderHotkeyBindingPopup() -> moved to separate file

// ApplyUIScale() -> moved to separate file

// SaveUISettings() -> moved to separate file

// LoadUISettings() -> moved to separate file

void ImGuiManager::Render() {
    // Clamp bg_alpha_ to safe range to prevent NaN crashes
    if (bg_alpha_ != bg_alpha_ || bg_alpha_ < 0.0f) bg_alpha_ = 0.05f;
    if (bg_alpha_ > 1.0f) bg_alpha_ = 1.0f;

    // Always check global hotkeys (works even when UI is hidden)
    CheckGlobalHotkey();
    CheckGlobalMouseRollerHotkey();

    // Close binding popup if UI is hidden
    if (!visible_ && binding_mode_) {
        binding_mode_ = false;
        bind_captured_ = false;
        bind_just_entered_ = false;
        bind_prev_key_ = 0;
        bind_peak_shift_ = false;
        bind_peak_ctrl_ = false;
        bind_peak_alt_ = false;
    }

    if (!initialized_ || !visible_) {
        // Ensure stale capture flags don't linger after the UI is hidden —
        // otherwise the key sniffer blocks X-Plane keys permanently.
        if (initialized_) {
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
        }
        return;
    }

    if (!opengl_initialized_) {
        __try {
            if (ImGui_ImplOpenGL2_Init()) {
                opengl_initialized_ = true;
                Logger::ImGui("ISFP-xLink:ImGui:OpenGL2 backend initialized\n");
            } else {
                Logger::ImGui("ISFP-xLink:ImGui:OpenGL2 backend init FAILED\n");
                return;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::ImGui("ISFP-xLink:ImGui:OpenGL2 backend CRASHED\n");
            return;
        }
    }

    // Rebuild font atlas if font was changed
    if (font_dirty_) {
        font_dirty_ = false;
        ImGui_ImplOpenGL2_DestroyDeviceObjects();
    }

    __try {
        int screen_w, screen_h;
        XPLMGetScreenSize(&screen_w, &screen_h);
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)screen_w, (float)screen_h);

        // Only process mouse when X-Plane is the foreground window
        // (prevents interacting with the UI when clicking outside the game)
        // Cache the X-Plane main window handle on first successful detection
        static HWND s_xp_hwnd = NULL;
        HWND fg_wnd = GetForegroundWindow();
        if (!s_xp_hwnd && fg_wnd) {
            // Check if this foreground window belongs to our process
            DWORD fg_pid = 0, our_pid = GetCurrentProcessId();
            GetWindowThreadProcessId(fg_wnd, &fg_pid);
            if (fg_pid == our_pid)
                s_xp_hwnd = fg_wnd;
        }
        bool xp_is_active = (s_xp_hwnd && fg_wnd == s_xp_hwnd);

        if (xp_is_active) {
            POINT cursor_pt;
            GetCursorPos(&cursor_pt);
            ScreenToClient(s_xp_hwnd, &cursor_pt);
            if (cursor_pt.x < 0) cursor_pt.x = 0;
            if (cursor_pt.x >= screen_w) cursor_pt.x = screen_w - 1;
            if (cursor_pt.y < 0) cursor_pt.y = 0;
            if (cursor_pt.y >= screen_h) cursor_pt.y = screen_h - 1;
            io.MousePos = ImVec2((float)cursor_pt.x, (float)cursor_pt.y);
            io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
            if (cursor_pt.x < 0) cursor_pt.x = 0;
            if (cursor_pt.x >= screen_w) cursor_pt.x = screen_w - 1;
            if (cursor_pt.y < 0) cursor_pt.y = 0;
            if (cursor_pt.y >= screen_h) cursor_pt.y = screen_h - 1;
            io.MousePos = ImVec2((float)cursor_pt.x, (float)cursor_pt.y);
            io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
        } else {
            // X-Plane not focused: release all mouse buttons so UI can't be clicked
            io.MouseDown[0] = false;
            io.MouseDown[1] = false;
            io.MouseDown[2] = false;
            // Also release keyboard capture to prevent typing into UI
            io.WantCaptureKeyboard = false;
        }

        // Forward keyboard input to ImGui (reliable: uses GetAsyncKeyState, not message queue)
        // Track ALL virtual key states for proper edge detection
        static bool s_key_prev[256] = {};
        static int s_static_version = 0;
        if (s_static_version != reload_version_) {
            memset(s_key_prev, 0, sizeof(s_key_prev));
            s_static_version = reload_version_;
        }

        if (xp_is_active && io.WantCaptureKeyboard) {
            BYTE keyboard_state[256] = {};
            GetKeyboardState(keyboard_state);

            // --- Character keys (A-Z, 0-9) ---
            for (int vk = 0x30; vk <= 0x5A; vk++) {
                if (vk == VK_LWIN || vk == VK_RWIN) continue;
                bool down = (GetAsyncKeyState(vk) < 0);
                if (down && !s_key_prev[vk]) {
                    WORD ch = 0;
                    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
                    if (ToAscii((UINT)vk, sc, keyboard_state, &ch, 0) > 0) {
                        if (ch >= 0x20 && ch < 0x10000)
                            io.AddInputCharacter((unsigned int)ch);
                    }
                }
                s_key_prev[vk] = down;
            }

            // --- OEM punctuation keys ---
            static const int oem_keys[] = {
                VK_SPACE, VK_OEM_1, VK_OEM_2, VK_OEM_3, VK_OEM_4, VK_OEM_5,
                VK_OEM_6, VK_OEM_7, VK_OEM_PLUS, VK_OEM_MINUS,
                VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_8, 0
            };
            for (int i = 0; oem_keys[i] != 0; i++) {
                int vk = oem_keys[i];
                bool down = (GetAsyncKeyState(vk) < 0);
                if (down && !s_key_prev[vk]) {
                    WORD ch = 0;
                    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
                    if (ToAscii((UINT)vk, sc, keyboard_state, &ch, 0) > 0) {
                        if (ch >= 0x20 && ch < 0x10000)
                            io.AddInputCharacter((unsigned int)ch);
                    }
                }
                s_key_prev[vk] = down;
            }

            // --- Control keys (Backspace, Enter, Tab, arrows, etc.) ---
            // Track each control key's previous state independently
            static struct { int vk; ImGuiKey key; bool prev; int version; } ctrl_map[] = {
                {VK_BACK,    ImGuiKey_Backspace, false, 0},
                {VK_DELETE,  ImGuiKey_Delete,    false, 0},
                {VK_RETURN,  ImGuiKey_Enter,     false, 0},
                {VK_TAB,     ImGuiKey_Tab,       false, 0},
                {VK_LEFT,    ImGuiKey_LeftArrow, false, 0},
                {VK_RIGHT,   ImGuiKey_RightArrow,false, 0},
                {VK_HOME,    ImGuiKey_Home,      false, 0},
                {VK_END,     ImGuiKey_End,       false, 0},
                {VK_INSERT,  ImGuiKey_Insert,    false, 0},
                {VK_CONTROL, ImGuiKey_LeftCtrl,   false, 0},
                {VK_SHIFT,   ImGuiKey_LeftShift,  false, 0},
                {VK_MENU,    ImGuiKey_LeftAlt,    false, 0},
                // Letter keys for shortcut detection
                {0x43,       ImGuiKey_C,         false, 0},
                {0x56,       ImGuiKey_V,         false, 0},
                {0x58,       ImGuiKey_X,         false, 0},
                {0x41,       ImGuiKey_A,         false, 0},
                {0, ImGuiKey_None, false, 0}
            };
            // Reset all prev states when ImGui context is recreated (plugin reload)
            if (ctrl_map[0].version != reload_version_) {
                for (int i = 0; ctrl_map[i].vk != 0; i++)
                    ctrl_map[i].prev = false;
                ctrl_map[0].version = reload_version_;
            }
            for (int i = 0; ctrl_map[i].vk != 0; i++) {
                bool down = (GetAsyncKeyState(ctrl_map[i].vk) < 0);
                if (down != ctrl_map[i].prev) {
                    ctrl_map[i].prev = down;
                    io.AddKeyEvent(ctrl_map[i].key, down);
                }
            }
        }

        // Drain mouse-wheel events BEFORE NewFrame() so ImGui processes them in
        // the CURRENT frame (no delay).  Only drain when the cursor is over the
        // ImGui window (using imgui_win_* from the previous frame) — otherwise
        // the game feels sluggish because wheel events are stolen from X-Plane.
        // Render()/EndFrame() clears io.MouseWheel later, but NewFrame() already
        // read it by then, so the scrollbar scrolls correctly.
        io.MouseWheel = 0.0f;
        if (imgui_win_w_ > 0 && imgui_win_h_ > 0) {
            int tmp_w, sh;
            XPLMGetScreenSize(&tmp_w, &sh);
            float ui_top = (float)(sh - imgui_win_y_ - imgui_win_h_);
            float ui_bot = (float)(sh - imgui_win_y_);
            bool in_ui = (io.MousePos.x >= (float)imgui_win_x_ &&
                          io.MousePos.x <  (float)(imgui_win_x_ + imgui_win_w_) &&
                          io.MousePos.y >= ui_top &&
                          io.MousePos.y <  ui_bot);
            if (in_ui) {
                MSG msg;
                while (PeekMessage(&msg, nullptr, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_REMOVE)) {
                    short wheel_delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
                    io.MouseWheel += wheel_delta / (float)WHEEL_DELTA;
                }
            }
        }

        ImGui_ImplOpenGL2_NewFrame();
        ImGui::NewFrame();
        // Handle key binding capture (runs every frame when binding_mode_ is active)
        HandleKeyBindingCapture();

        // Wrap the render + draw in SEH to catch any OpenGL crashes after plugin reload
#pragma warning(push)
#pragma warning(disable: 2712)
        __try {
            RenderMainWindow();
            RenderHotkeyBindingPopup();
            RenderToastOverlay();

            // When Enter is pressed while an ImGui text input field is focused,
            // release the keyboard capture so the Enter passes through to X-Plane.
            if (io.WantCaptureKeyboard) {
                static bool s_enter_was_down = false;
                bool enter_down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
                if (enter_down && !s_enter_was_down) {
                    io.WantCaptureKeyboard = false;
                    s_enter_was_down = true;
                }
                if (!enter_down) s_enter_was_down = false;
            }

            ImGui::Render();
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::ImGui("ISFP-xLink:ImGui:Render CRASHED, recovering...\n");
            // Force-reset ImGui internal state so it can render next frame
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
        }

        // Keep default arrow cursor in UI (no hand/pointer over clickable items)
        if (!io.MouseDrawCursor && imgui_win_w_ > 0 && imgui_win_h_ > 0) {
            int tmp_w, screen_h;
            XPLMGetScreenSize(&tmp_w, &screen_h);
            float ui_top = (float)(screen_h - imgui_win_y_ - imgui_win_h_);
            float ui_bot = (float)(screen_h - imgui_win_y_);
            bool in_ui = (io.MousePos.x >= (float)imgui_win_x_ && io.MousePos.x < (float)(imgui_win_x_ + imgui_win_w_) &&
                          io.MousePos.y >= ui_top && io.MousePos.y < ui_bot);
            if (in_ui) {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Logger::ImGui("ISFP-xLink:ImGui:CRASHED during render\n");
        // If the ImGui context was destroyed out from under us, mark backend
        // as uninitialized so the next frame tries to re-init cleanly.
        opengl_initialized_ = false;
    }
}

void ImGuiManager::RenderToastOverlay() {
    if (toast_message_.empty()) return;

    double elapsed = XPLMGetElapsedTime() - toast_timestamp_;
    if (elapsed >= 3.0) {
        toast_message_.clear();
        return;
    }

    float alpha = (elapsed > 2.5f) ? (3.0f - (float)elapsed) * 2.0f : 1.0f; // fade out last 0.5s

    // Use theme toast text color
    float toast_r = theme_custom_toast_text_[0];
    float toast_g = theme_custom_toast_text_[1];
    float toast_b = theme_custom_toast_text_[2];

    // Position at cursor bottom-right with 10px offset
    ImVec2 cursor_pos = ImGui::GetIO().MousePos;
    ImGui::SetNextWindowPos(ImVec2(cursor_pos.x + 10.0f, cursor_pos.y + 10.0f), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f); // fully transparent background

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoInputs; // no mouse/keyboard capture

    ImGui::Begin("##toast", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(toast_r, toast_g, toast_b, alpha));
    ImGui::Text("%s", toast_message_.c_str());
    ImGui::PopStyleColor();
    ImGui::End();
}

void ImGuiManager::RenderMainWindow() {
    // Set window position and size — use saved values once, then let user drag/resize freely
    if (window_geom_loaded_)
        ImGui::SetNextWindowPos(ImVec2((float)window_x_, (float)window_y_), ImGuiCond_Once);
    else
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    if (window_geom_loaded_)
        ImGui::SetNextWindowSize(ImVec2((float)window_w_, (float)window_h_), ImGuiCond_Once);
    else
        ImGui::SetNextWindowSize(ImVec2(820, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(450, 350), ImVec2(FLT_MAX, FLT_MAX));

    // Use fixed English title as window ID to prevent position reset on language switch
    // NoResize disables ALL built-in border/corner resize — only custom grip works
    // Transparency: background (image + dark panel) max 75%, controls 50→100%
    // bg_alpha_ 0→1:  background 0.05→0.75,  controls 0.5→1.0
    float bg_a = 0.05f + 0.70f * bg_alpha_;
    if (bg_a < 0.05f) bg_a = 0.05f;
    if (bg_a > 0.75f) bg_a = 0.75f;
    float ctrl_a = 0.5f + 0.5f * bg_alpha_;
    if (ctrl_a < 0.5f) ctrl_a = 0.5f;
    if (ctrl_a > 1.0f) ctrl_a = 1.0f;
    current_ctrl_alpha_ = ctrl_a;

    // Background colors need compensation: we want effective alpha = bg_a after ctrl_a multiplier
    float bg_comp = bg_a / ctrl_a;
    if (bg_comp > 1.0f) bg_comp = 1.0f;

    // Apply theme colors (if using a preset) — alpha is baked into each color
    PushThemeColors();

    // Read theme background/border colors and push them BEFORE Begin()
    // (Begin() draws the window background immediately, so colors must be active)
    float win_r = 0.06f, win_g = 0.06f, win_b = 0.06f;
    float chd_r = 0.10f, chd_g = 0.10f, chd_b = 0.12f;
    float bdr_r = 0.30f, bdr_g = 0.30f, bdr_b = 0.35f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& p = kPresets[theme_preset_];
        win_r = p.window[0]; win_g = p.window[1]; win_b = p.window[2];
        chd_r = p.child[0];  chd_g = p.child[1];  chd_b = p.child[2];
        bdr_r = p.border[0]; bdr_g = p.border[1]; bdr_b = p.border[2];
    } else if (theme_preset_ < 0) {
        win_r = theme_custom_window_[0]; win_g = theme_custom_window_[1]; win_b = theme_custom_window_[2];
        chd_r = theme_custom_child_[0];  chd_g = theme_custom_child_[1];  chd_b = theme_custom_child_[2];
        bdr_r = theme_custom_border_[0]; bdr_g = theme_custom_border_[1]; bdr_b = theme_custom_border_[2];
    }
    // Always push main background colors before Begin() so window bg is correct
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(win_r, win_g, win_b, bg_comp));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(chd_r, chd_g, chd_b, bg_comp));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(bdr_r, bdr_g, bdr_b, bg_comp));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.4f, 0.4f, 0.5f, bg_comp));

    // Localized display title with stable ##windowId to preserve position across language switch
    std::string win_title = std::string(GetText(StringID::ISFPBridgeTitle)) + "##ISFPPanel";
    ImGui::Begin(win_title.c_str(), &ui_open_,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (!ui_open_) {
        visible_ = false;
        SyncMenuCheckmarks();
        ui_open_ = true;
        ImGui::End();
        ImGui::PopStyleColor(4); // Separator + Border + ChildBg + WindowBg
        PopThemeColors();
        return;
    }

    if (bg_texture_id_) {
        // Draw background image with bg_a (not compensated, drawn directly)
        int alpha = (int)(bg_a * 255.0f + 0.5f);
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetWindowPos();
        ImVec2 p1 = ImVec2(p0.x + ImGui::GetWindowSize().x, p0.y + ImGui::GetWindowSize().y);
        dl->AddImage((ImTextureID)(intptr_t)bg_texture_id_,
            p0, p1, ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32(255, 255, 255, alpha));
        // Override ChildBg with transparent so image shows through child windows
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    } else {
        // Background colors already pushed before Begin() — no extra push needed
        // Just ensure child bg override is handled (transparent ChildBg for image path)
    }

    float sidebar_width = 170.0f;
    float button_height = 42.0f;

    // Sidebar with rounded corners
    ImGui::BeginChild("Sidebar", ImVec2(sidebar_width, 0), true);
    float sb_text_w = ImGui::CalcTextSize(GetText(StringID::Navigation)).x;
    ImGui::SetCursorPosX((sidebar_width - sb_text_w) * 0.5f);
    ImGui::Text("%s", GetText(StringID::Navigation));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    // Page buttons with centered multi-line text
    auto sidebar_btn = [&](const char* label, Page page) {
        bool is_active = (current_page_ == page);
        float btn_w = sidebar_width - 22;
        ImGui::SetCursorPosX(11.0f);
        ImVec2 btn_size(btn_w, button_height);
        ImVec2 bb_min = ImGui::GetCursorScreenPos();
        ImVec2 bb_max = ImVec2(bb_min.x + btn_size.x, bb_min.y + btn_size.y);

        // Hit detection via invisible button
        if (is_active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.80f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
        bool clicked = ImGui::InvisibleButton(label, btn_size);
        ImGui::PopStyleColor();
        if (is_active)
            ImGui::PopStyleColor();

        if (clicked) current_page_ = page;

        // Draw button background (using theme colors)
        ImDrawList* dl = ImGui::GetWindowDrawList();
        int btn_alpha = (int)(ctrl_a * 255.0f + 0.5f);
        if (btn_alpha < 0) btn_alpha = 0;
        if (btn_alpha > 255) btn_alpha = 255;
        float btn_r = 0.15f, btn_g = 0.45f, btn_b = 0.80f;
        float bh_r = 0.25f, bh_g = 0.55f, bh_b = 0.85f;
        float ba_r = 0.15f, ba_g = 0.45f, ba_b = 0.80f;
        float bc_r = 0.10f, bc_g = 0.35f, bc_b = 0.70f;
        if (theme_preset_ >= 0 && theme_preset_ < 5) {
            const auto& p = kPresets[theme_preset_];
            btn_r = p.button[0]; btn_g = p.button[1]; btn_b = p.button[2];
            bh_r = p.btn_hover[0]; bh_g = p.btn_hover[1]; bh_b = p.btn_hover[2];
            ba_r = p.btn_active[0]; ba_g = p.btn_active[1]; ba_b = p.btn_active[2];
            bc_r = p.btn_clicked[0]; bc_g = p.btn_clicked[1]; bc_b = p.btn_clicked[2];
        } else if (theme_preset_ < 0) {
            btn_r = theme_custom_button_[0]; btn_g = theme_custom_button_[1]; btn_b = theme_custom_button_[2];
            bh_r = theme_custom_btn_hover_[0]; bh_g = theme_custom_btn_hover_[1]; bh_b = theme_custom_btn_hover_[2];
            ba_r = theme_custom_btn_active_[0]; ba_g = theme_custom_btn_active_[1]; ba_b = theme_custom_btn_active_[2];
            bc_r = theme_custom_btn_clicked_[0]; bc_g = theme_custom_btn_clicked_[1]; bc_b = theme_custom_btn_clicked_[2];
        }
        ImU32 bg_col = is_active
            ? IM_COL32((int)(255*ba_r), (int)(255*ba_g), (int)(255*ba_b), btn_alpha)
            : ImGui::IsItemActive()
                ? IM_COL32((int)(255*bc_r), (int)(255*bc_g), (int)(255*bc_b), btn_alpha)
                : ImGui::IsItemHovered()
                ? IM_COL32((int)(255*bh_r), (int)(255*bh_g), (int)(255*bh_b), btn_alpha)
                : IM_COL32((int)(255*btn_r), (int)(255*btn_g), (int)(255*btn_b), btn_alpha);
        dl->AddRectFilled(bb_min, bb_max, bg_col, 4.0f);

        // Draw centered multi-line text — use ImGuiCol_Text from style stack
        // to guarantee exact match with right-panel text color.
        const ImVec4& txt_col_style = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        int text_alpha = 255;
        ImU32 text_col = IM_COL32(
            (int)(255 * txt_col_style.x),
            (int)(255 * txt_col_style.y),
            (int)(255 * txt_col_style.z),
            text_alpha);
        ImFont* font = ImGui::GetFont();
        float font_size = ImGui::GetFontSize();

        // Split label by \n and draw each line centered
        const char* p = label;
        std::vector<std::string> lines;
        while (*p) {
            const char* next = strchr(p, '\n');
            if (next) {
                lines.push_back(std::string(p, next - p));
                p = next + 1;
            } else {
                lines.push_back(std::string(p));
                break;
            }
        }

        float total_text_h = (float)lines.size() * font_size;
        float start_y = bb_min.y + (btn_size.y - total_text_h) * 0.5f;
        for (size_t i = 0; i < lines.size(); i++) {
            float text_w = ImGui::CalcTextSize(lines[i].c_str()).x;
            float x = bb_min.x + (btn_size.x - text_w) * 0.5f;
            float y = start_y + (float)i * font_size;
            dl->AddText(font, font_size, ImVec2(x, y), text_col, lines[i].c_str());
        }

        ImGui::Dummy(ImVec2(0, 4));
    };

    sidebar_btn(GetText(StringID::OnlinePlayersATC_Sidebar), Page::Players);
    sidebar_btn(GetText(StringID::FlightInquiry_Sidebar), Page::FlightInquiry);
    sidebar_btn(GetText(StringID::CSLModels_Sidebar), Page::CSLModels);
    sidebar_btn(GetText(StringID::Settings_Sidebar), Page::Settings);

    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::BeginChild("Content", ImVec2(0, 0), ImGuiChildFlags_Borders);

    switch (current_page_) {
        case Page::Players:      RenderPlayersPage();      break;
        case Page::CSLModels:    RenderCSLModelsPage();    break;
        case Page::FlightInquiry: RenderFlightInquiryPage(); break;
        case Page::Settings:     RenderSettingsPage();     break;
    }

    bool content_has_active = ImGui::IsAnyItemActive();
    ImGui::EndChild();
    ImGui::PopStyleVar(); // WindowPadding

    // Bottom-right resize grip
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_sz = ImGui::GetWindowSize();
    float grip_size = 12.0f;
    ImGui::SetCursorScreenPos(ImVec2(win_pos.x + win_sz.x - grip_size,
                                     win_pos.y + win_sz.y - grip_size));
    ImGui::InvisibleButton("##resize_grip", ImVec2(grip_size, grip_size));
    bool grip_hovered = ImGui::IsItemHovered();
    bool grip_active = ImGui::IsItemActive();

    if (grip_active && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        float new_w = win_sz.x + delta.x;
        float new_h = win_sz.y + delta.y;
        if (new_w < 450.0f) new_w = 450.0f;
        if (new_h < 350.0f) new_h = 350.0f;
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImGuiWindowFlags old_flags = win->Flags;
        win->Flags &= ~ImGuiWindowFlags_NoResize;
        ImGui::SetWindowSize(ImVec2(new_w, new_h));
        win->Flags = old_flags;
    }

    // Draw small triangle grip at bottom-right corner (rounded hypotenuse edge)
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetItemRectMax(); // bottom-right corner
    ImU32 grip_col;
    if (grip_active)       grip_col = IM_COL32(60, 130, 220, (int)(230 * ctrl_a));
    else if (grip_hovered) grip_col = IM_COL32(50, 100, 180, (int)(210 * ctrl_a));
    else                   grip_col = IM_COL32(40, 40, 60, (int)(150 * ctrl_a));
    // Right triangle: bottom-left → corner → top-right
    dl->AddTriangleFilled(
        ImVec2(c.x - grip_size, c.y),  // left point on bottom edge
        c,                              // corner
        ImVec2(c.x, c.y - grip_size),   // top point on right edge
        grip_col);
    // Rounded hypotenuse highlight
    ImU32 line_col = grip_active ? IM_COL32(200,220,255,(int)(200 * ctrl_a))
                   : (grip_hovered ? IM_COL32(180,200,240,(int)(180 * ctrl_a))
                                   : IM_COL32(80,80,100,(int)(120 * ctrl_a)));
    dl->AddLine(
        ImVec2(c.x - grip_size + 2.0f, c.y - 1.0f),
        ImVec2(c.x - 1.0f, c.y - grip_size + 2.0f),
        line_col, 2.0f);

    // Overlay close-button (X) — drawn last so it sits on top of everything.
    // The hit-area covers the full title-bar height (GetFrameHeight).
    {
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        float btn_sz = ImGui::GetFrameHeight();
        ImVec2 btn_min(wp.x + ws.x - btn_sz, wp.y);
        ImVec2 btn_max(wp.x + ws.x, wp.y + btn_sz);
        ImVec2 mouse = ImGui::GetIO().MousePos;

        bool hover = (mouse.x >= btn_min.x && mouse.x <= btn_max.x &&
                      mouse.y >= btn_min.y && mouse.y <= btn_max.y);
        if (hover && ImGui::IsMouseClicked(0)) {
            visible_ = false;
            SyncMenuCheckmarks();
        }

        ImDrawList* dl2 = ImGui::GetWindowDrawList();
        int close_a = (int)(ctrl_a * 255.0f + 0.5f);
        if (close_a < 0) close_a = 0; if (close_a > 255) close_a = 255;
        ImU32 x_col = hover ? IM_COL32(255, 80, 80, close_a) : IM_COL32(180, 180, 190, close_a);
        float pad  = btn_sz * 0.28f;
        float thick = 2.0f;
        dl2->AddLine(ImVec2(btn_min.x + pad, btn_min.y + pad),
                     ImVec2(btn_max.x - pad, btn_max.y - pad), x_col, thick);
        dl2->AddLine(ImVec2(btn_max.x - pad, btn_min.y + pad),
                     ImVec2(btn_min.x + pad, btn_max.y - pad), x_col, thick);
    }

    // Body drag: runs AFTER all child windows and the resize grip.
    // Excludes the title bar (handled by ImGui's built-in drag) and the
    // resize grip (!grip_active) to prevent position-shifting during resize.
    // The 8px threshold prevents accidental drags from sidebar button clicks.
    {
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImVec2 ms = ImGui::GetIO().MousePos;
        float title_h = ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y;
        bool in_body = (ms.x >= wp.x && ms.x < wp.x + ws.x &&
                        ms.y >= wp.y + title_h && ms.y < wp.y + ws.y);
        // Block body-drag when an interactive widget in the Content area is active
        // (scrollbar thumb, text input, combo box, etc.).  However, IF the active
        // item is in the sidebar (mouse X within sidebar width), still allow drag.
        bool sidebar_active = (content_has_active && ms.x < wp.x + sidebar_width);
        if (in_body && !grip_active && (!content_has_active || sidebar_active) && ImGui::IsMouseDragging(0, 8.0f)) {
            ImGui::SetWindowPos(ImVec2(
                wp.x + ImGui::GetIO().MouseDelta.x,
                wp.y + ImGui::GetIO().MouseDelta.y
            ));
        }
    }

    // Capture window bounds BEFORE ImGui::End() while the window context is still
    // valid.  GetWindowPos/Size after End() is unreliable.
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 sz = ImGui::GetWindowSize();
    int screen_h;
    XPLMGetScreenSize(&imgui_win_w_, &screen_h);
    imgui_win_x_ = (int)pos.x;
    imgui_win_w_ = (int)sz.x;
    imgui_win_h_ = (int)sz.y;
    imgui_win_y_ = screen_h - (int)(pos.y + sz.y); // bottom edge in X-Plane coords

    // Auto-save window geometry when position or size changes
    if ((int)pos.x != window_x_ || (int)pos.y != window_y_ ||
        (int)sz.x != window_w_ || (int)sz.y != window_h_) {
        window_x_ = (int)pos.x;
        window_y_ = (int)pos.y;
        window_w_ = (int)sz.x;
        window_h_ = (int)sz.y;
        SaveUISettings();
    }

    if (bg_texture_id_)
        ImGui::PopStyleColor(1); // extra transparent ChildBg (image path)
    ImGui::End();
    ImGui::PopStyleColor(4); // WindowBg + ChildBg + Border + Separator (pushed before Begin)
    PopThemeColors();

    // Keep XPLM capture window on top every frame
    if (mouse_capture_window_) {
        XPLMBringWindowToFront((XPLMWindowID)mouse_capture_window_);
    }
}

// RenderPlayersPage() -> moved to separate file

// RenderCSLModelsPage() -> moved to separate file

// RenderFlightInquiryPage() -> moved to separate file

// RenderCSLFileBrowser() -> moved to separate file

// RenderSettingsPage() -> moved to separate file

} // namespace ISFP
