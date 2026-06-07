#ifndef IMGUI_MANAGER_H
#define IMGUI_MANAGER_H

#include <string>
#include <vector>

#include <imgui.h>
#include "isfp_plugin.h"

namespace ISFP {

// Forward declarations
struct FlightData;

// ImGui Manager - handles the Dear ImGui UI overlay window
class ImGuiManager {
public:
    ImGuiManager();
    ~ImGuiManager();

    bool Initialize();
    void Shutdown();

    bool IsInitialized() const { return initialized_; }

    // Called from phase draw callback to render ImGui
    void Render();

    // Toggle visibility
    void ToggleVisibility() { visible_ = !visible_; }
    bool IsVisible() const { return visible_; }
    void SetVisible(bool v) { visible_ = v; }

    // Key binding mode (used by XPLM key sniffer)
    bool IsBinding() const { return binding_mode_; }

    // Hide on startup preference
    bool GetHideOnStartup() const { return hide_on_startup_; }
    void SetHideOnStartup(bool v) { hide_on_startup_ = v; }

    // Save/Load UI settings to Profiles/UI_Config.json
    void SaveUISettings();
    void LoadUISettings();

    // Whether the main UI window is considered "open" (used for the X close-button).
    // ImGui::Begin() writes false here when the user clicks X.
    bool ui_open_ = true;

    // XPLM window ID for mouse capture (prevents click-through to other plugins)
    void* mouse_capture_window_ = nullptr;

    // Current ImGui window bounds (for mouse capture hit-test)
    int imgui_win_x_ = 0, imgui_win_y_ = 0;
    int imgui_win_w_ = 820, imgui_win_h_ = 560;

    // String IDs for localization
    enum class StringID {
        Navigation,
        OnlinePlayersATC_Sidebar,
        CSLModels_Sidebar,
        FlightInquiry_Sidebar,
        Settings_Sidebar,
        OnlinePlayersATC_Title,
        NoPlayersConnected,
        Callsign,
        Aircraft,
        Latitude,
        Longitude,
        Altitude,
        Heading,
        Speed,
        CSLMapping_Title,
        ReloadCSLMapping,
        CSLConfigNotFound,
        CSLConfigParseFailed,
        CSLPath,
        NoMappedAircraft,
        MappedAircraftCount,
        Settings_Title,
        EnableFSD,
        EnableCSL,
        EnableLog,
        Startup,
        HideUIOnStartup,
        UIFont,
        LangToggle,      // "中文" in EN mode, "EN" in CN mode
        ISFPBridgeTitle, // "ISFP Connect Bridge" window title
        ToggleUIHotkey,  // "Toggle UI Shortcut" / "切换UI快捷键"
        ToggleMouseRollerHotkey, // "Show/Hide Mouse Roller" / "显示/隐藏舵面联动框"
        HotkeyBind,      // "Click to bind" / "点击绑定"
        HotkeyBinding,   // "Press keys... (ESC to cancel)" / "按下按键...(ESC取消)"
        HotkeyUnbound,   // "Unbound" / "未绑定"
        UIScale,         // "UI Scale" / "UI缩放"
        COUNT
    };

    // Get localized string
    static const char* GetText(StringID id);

    // Merge CJK fallback font (e.g. Microsoft YaHei) so Chinese chars display correctly
    static void MergeCJKFallbackFont(float font_size = 14.0f);

private:
    // UI pages
    enum class Page {
        Players,    // 在线玩家及管制
        CSLModels,  // CSL机型映射
        FlightInquiry, // 行情查询
        Settings    // 设置
    };

    // Render the main window
    void RenderMainWindow();

    // Render each page
    void RenderPlayersPage();
    void RenderCSLModelsPage();
    void RenderFlightInquiryPage();
    void RenderSettingsPage();

    // Scan Fonts directory for available fonts
    void ScanFonts();
    // Apply selected font (rebuild atlas)
    void ApplyFont(int index);

    // Which hotkey is currently being bound
    enum class BindTarget { NONE, TOGGLE_UI, TOGGLE_MOUSE_ROLLER };

    // Hotkey
    void CheckGlobalHotkey();
    void CheckGlobalMouseRollerHotkey();
    void HandleKeyBindingCapture();
    void RenderHotkeyBindingPopup();

    // CSL file browser
    void RenderCSLFileBrowser();

    // Apply combined DPI + user UI scale to ImGui
    void ApplyUIScale();

    // Build AIRAC cycle list (called once from Initialize)
    void BuildAiracList();
    struct AiracItem {
        std::string label;
        int value; // e.g. 2605
    };
    std::vector<AiracItem> airac_list_;
    int airac_index_ = 0;
    bool airac_built_ = false;

    // Flight inquiry input buffers
    char dep_buf_[16] = "";
    char arr_buf_[16] = "";
    char wx_buf_[16] = "";
    // Query pending timestamps
    double route_query_time_ = 0.0;
    double weather_query_time_ = 0.0;
    // Version counter increments on each Initialize() to detect ImGui context recreation
    int reload_version_ = 0;

    bool initialized_ = false;
    bool visible_ = true;
    bool hide_on_startup_ = false;
    bool opengl_initialized_ = false;
    bool font_dirty_ = false;
    float ui_scale_ = 1.0f;          // user UI scale (0.75/1.0/1.25/1.5)
    float dpi_scale_ = 1.0f;         // auto-detected DPI scale
    float base_font_size_ = 14.0f;   // base font size in pixels
    // Saved window geometry (persisted to UI_Config.json)
    bool window_geom_loaded_ = false;
    int window_x_ = 50, window_y_ = 50;
    int window_w_ = 820, window_h_ = 560;
    Page current_page_ = Page::Players;
    std::string font_dir_;
    std::vector<std::string> available_fonts_;
    int current_font_index_ = -1; // -1 = default
    // CSL file browser state
    bool csl_browser_open_ = false;
    std::string csl_browser_path_;
    std::vector<std::string> csl_browser_entries_;
    std::vector<std::string> csl_browser_folders_;
    int csl_browser_selected_ = -1;
    std::vector<std::string> csl_browser_back_;
    std::vector<std::string> csl_browser_forward_;
    char csl_path_buf_[1024] = "";  // shared CSL path input buffer

    bool binding_mode_ = false;     // true when waiting for key binding input
    BindTarget binding_target_ = BindTarget::NONE; // which hotkey is being bound
    bool hotkey_was_down_ = false;   // previous frame hotkey state for edge detection
    bool mouseyoke_hk_was_down_ = false; // edge detection for mouse-roller hotkey
    bool bind_captured_ = false;     // true when key has been pressed and released
    bool bind_cleared_ = false;      // true when ESC cleared the key (shows gray)
    bool bind_just_entered_ = false; // true on first frame after entering binding mode
    int bind_prev_key_ = 0;          // previous frame key for edge detection
    bool bind_peak_shift_ = false;   // peak Shift state while key was held
    bool bind_peak_ctrl_ = false;    // peak Ctrl state while key was held
    bool bind_peak_alt_ = false;     // peak Alt state while key was held
    HotkeyBinding pending_hotkey_;   // hotkey being set in binding popup

    // Hotkey table sort state
    int sort_column_ = 1;          // 1=name
    bool sort_ascending_ = true;   // true=A-Z, false=Z-A

    // Background image
    unsigned int bg_texture_id_ = 0;
    int bg_tex_w_ = 0, bg_tex_h_ = 0;
    float bg_alpha_ = 0.5f;
    std::string bg_texture_path_;

    void UnloadBGTexture();
    void LoadBGTexture(const std::string& filepath);
    void CopyAndLoadBGImage(const std::string& src_path);

    // Theme: -1 = custom, 0-4 = presets (3 light, 2 dark)
    int theme_preset_ = 4;       // default: Dark Modern
    bool theme_custom_open_ = false;
    // Custom RGB colors (0.0~1.0)
    float theme_custom_window_[3] = { 0.06f, 0.06f, 0.06f };
    float theme_custom_child_[3]  = { 0.10f, 0.10f, 0.12f };
    float theme_custom_button_[3] = { 0.15f, 0.45f, 0.80f };
    float theme_custom_btn_clicked_[3] = { 0.10f, 0.35f, 0.70f };
    float theme_custom_btn_hover_[3] = { 0.25f, 0.55f, 0.85f };
    float theme_custom_btn_active_[3] = { 0.20f, 0.55f, 0.90f };
    float theme_custom_border_[3] = { 0.30f, 0.30f, 0.35f };
    float theme_custom_frame_[3]  = { 0.20f, 0.20f, 0.25f };
    float theme_custom_frame_active_[3] = { 0.30f, 0.30f, 0.35f };
    float theme_custom_popup_[3]  = { 0.12f, 0.12f, 0.16f };
    float theme_custom_dropdown_frame_[3] = { 0.18f, 0.18f, 0.22f };
    float theme_custom_dropdown_border_[3] = { 0.25f, 0.25f, 0.30f };
    float theme_custom_dropdown_active_[3] = { 0.20f, 0.40f, 0.70f };
    float theme_custom_slider_[3] = { 0.40f, 0.60f, 0.90f };
    float theme_custom_slider_track_[3] = { 0.15f, 0.15f, 0.20f };
    float theme_custom_slider_track_hover_[3] = { 0.20f, 0.20f, 0.28f };
    float theme_custom_slider_active_[3] = { 0.55f, 0.75f, 1.00f };
    float theme_custom_text_[3]   = { 0.90f, 0.90f, 0.92f };
    float theme_custom_check_[3]  = { 0.70f, 0.70f, 0.70f };
    float theme_custom_table_header_text_[3] = { 0.90f, 0.90f, 0.92f };
    float theme_custom_table_header_bg_[3] = { 0.15f, 0.15f, 0.20f };
    float theme_custom_table_content_text_[3] = { 0.90f, 0.90f, 0.92f };
    float theme_custom_table_content_bg_[3] = { 0.06f, 0.06f, 0.08f };

    // Color table sort state
    int color_sort_col_ = -1;    // -1=no sort, 0=name, 1=brightness
    bool color_sort_asc_ = true;

    // Apply a preset theme by index
    void ApplyPresetTheme(int index);
    // Push theme colors for current frame (called in RenderMainWindow)
    void PushThemeColors();
    void PopThemeColors();
};

} // namespace ISFP

#endif // IMGUI_MANAGER_H
