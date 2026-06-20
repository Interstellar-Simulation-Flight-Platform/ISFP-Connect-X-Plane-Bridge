/*
 * ISFP-xLink - X-Plane Native Plugin
 * Native X-Plane plugin based on XPSDK430
 * Communicates with ISFP-Connect Python app via TCP
 */

 /*
 更新日志:
  - **v1.2.8 (2026-06-19)**:
  - 修复:功能:管制员信息不显示
  - **v1.2.7 (2026-06-19)**:
  - 修复:功能:首次启动游戏快捷键不生效
  - **v1.2.6 (2026-06-13)**:
  - 修复:功能:同一个ISFP-Connect重复连接导致连接失败
  - 修复:功能:获取的COM1/COM2数值错误
  - 修复:功能:获取的Squawk模式错误
  - **v1.2.3 (2026-06-06)**:
  - 新增:功能:UI面板
  - 修复:功能:多客户端连接时游戏卡死
  - 修复:功能:飞机轨迹预测修复为10s
  - 修复:功能:CSL映射所有支持的飞机(CSL映射包)
  - **v1.1.2 (2026-05-17)**:
  - 修复:功能:CSL映射其他飞机(插件端)
  - 修复:功能:当断开FSD连接后仍然显示机组标牌
 - **v1.1.0 (2026-05-16)**:
  - 新增:功能:CSL映射(目前所有飞机均映射为A319)。
  - 新增:UI:菜单插件功能控制面板,支持控制FSD/CSL状态。
- **v1.0.0 (2026-05-10)**:
  - 首发正式版本，支持 X-Plane 全版本连飞接入、完整 FSD 协议适配、模拟器内置菜单管理全套插件能力。
*/

#include "isfp_plugin.h"
#include "imgui_manager.h"
#include "mouse_yoke.h"
#include "utils.h"
#include "logger.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include <XPLMDisplay.h>
#include "XPLMGraphics.h"
#include "XPLMUtilities.h"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <vector>
#include <fstream>

// Draw callback state
static bool g_draw_callback_registered = false;
static XPLMDataRef g_mat_world = nullptr;
static XPLMDataRef g_mat_proj = nullptr;
static XPLMDataRef g_viewport = nullptr;
static XPLMDataRef g_screen_h = nullptr;

// Forward declarations
void RenderLabels();
static int DrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon);

// Forward declaration for flight loop callback
static float FlightLoopCallback(float inElapsedSinceLastCall, 
                                 float inElapsedTimeSinceLastFlightLoop, 
                                 int inCounter, 
                                 void* inRef);

namespace ISFP {
	class CSLAircraft;

    NetworkManager* g_network = nullptr;
    DataRefManager* g_datarefs = nullptr;
    CSLManager* g_csl = nullptr;

    // ATC / Controller data
    std::vector<ATCData> g_atc_list;
    std::mutex g_atc_mutex;

    // Plugin state
    static std::atomic<bool> g_plugin_enabled{false};
    static XPLMFlightLoopID g_flight_loop_id = nullptr;

    // Configuration
    static std::string g_host = DEFAULT_HOST;
    int g_port = DEFAULT_PORT;

    // MenuHandler
    static XPLMMenuID g_Menu ;
    static int g_FirstMenu;
    static XPLMMenuID g_SecondMenu;

    //FSD Socket Activate
    std::atomic<bool> g_fsd_enabled = true;

    //CSL Activate
    std::atomic<bool> g_csl_enabled = true;

    //CSL Log Activate
    std::atomic<bool> g_csl_log_enabled{false};

    // Language
    Language g_language = Language::CN;

    // Hotkey
    HotkeyBinding g_hotkey;
    HotkeyBinding g_mouseyoke_hotkey;
    static bool g_hotkey_was_down = false;  // prevent repeated toggles while held
    static bool g_mouseyoke_hotkey_was_down = false;

    //CSL Config
    std::vector<FlightData> g_valid_players;
    std::mutex g_player_mutex;
    std::vector<FlightData> g_draw_players;
    std::mutex g_draw_mutex;

    // EFB Query Results
    std::string g_efb_route_result;
    std::mutex g_efb_route_mutex;
    std::string g_efb_weather_result;
    std::mutex g_efb_weather_mutex;

    // ImGui Manager
    ImGuiManager* g_imgui = nullptr;

    // Mouse Yoke Manager
    MouseYokeManager* g_mouseyoke = nullptr;
    std::atomic<bool> g_mouseyoke_enabled{false};

} // namespace ISFP

using namespace ISFP;

// Reload counter — incremented on every XPluginStart.
// Used by RenderLabels() to detect stale static state across reloads.
static int g_plugin_reload_count = 0;

// ==================== Sync Menu Checkmarks ====================
void ISFP::SyncMenuCheckmarks() {
        if (!g_SecondMenu) return;
        XPLMCheckMenuItem(g_SecondMenu, 0,
            (g_imgui && g_imgui->IsVisible()) ? xplm_Menu_Checked : xplm_Menu_NoCheck);
        XPLMCheckMenuItem(g_SecondMenu, 1,
            g_fsd_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
        XPLMCheckMenuItem(g_SecondMenu, 2,
            g_csl_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
        XPLMCheckMenuItem(g_SecondMenu, 3,
            g_csl_log_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
    }

// ==================== X-Plane Plugin MenuHandler ====================

void MenuHandler(void *inMenuRef, void *inItemRef)
{
    int cmd = (int)(intptr_t)inItemRef;
    switch (cmd)
    {
    case 0:
        // Show/Hide UI: immediate toggle
        if (g_imgui) {
            g_imgui->ToggleVisibility();
        }
        XPLMCheckMenuItem(g_SecondMenu, 0,
            (g_imgui && g_imgui->IsVisible()) ? xplm_Menu_Checked : xplm_Menu_NoCheck);
        SyncMenuCheckmarks();
        break;
    case 1:
        g_fsd_enabled = !g_fsd_enabled;
        if (!g_fsd_enabled && g_network) {
            g_network->StopServer();
            Logger::Main("ISFP-xLink:Menu:FSD服务已关闭\n");
        }
        else if (g_fsd_enabled && g_network) {
            g_network->StartServer(g_port);
            Logger::Main("ISFP-xLink:Menu:FSD服务已启用\n");
        }
        SyncMenuCheckmarks();
        if (g_config) { g_config->SetBool("plugin.fsd.enabled", g_fsd_enabled.load()); g_config->Save(); }
        break;
    case 2:
        g_csl_enabled = !g_csl_enabled;
        if (!g_csl_enabled) {
            g_csl->Stop();
            Logger::Main("ISFP-xLink:Menu:CSL模型已关闭\n");
        }
        else if (g_csl_enabled) {
            g_csl->Start();
            Logger::Main("ISFP-xLink:Menu:CSL模型已启用\n");
        }
        SyncMenuCheckmarks();
        if (g_config) { g_config->SetBool("plugin.csl.enabled", g_csl_enabled.load()); g_config->Save(); }
        break;
    case 3:
        g_csl_log_enabled = !g_csl_log_enabled;
        Logger::SetEnabled(g_csl_log_enabled.load());
        Logger::Main(g_csl_log_enabled ?
            "ISFP-xLink:Menu:Log_Enabled\n" :
            "ISFP-xLink:Menu:Log_Disabled\n");
        SyncMenuCheckmarks();
        if (g_config) { g_config->SetBool("plugin.csl.log_enabled", g_csl_log_enabled.load()); g_config->Save(); }
        break;
    case 4:
        XPLMReloadPlugins();
        Logger::Main("ISFP-xLink:Menu:Reloaded all plugins\n");
        break;
    default:
        break;
    }
}

// ==================== X-Plane Plugin Draw Callbacks ====================
// Draw labels before X-Plane's window layer
static int DrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon)
{
    if (inPhase == xplm_Phase_Window && inIsBefore)
    {
        RenderLabels();
    }
    return 1;
}

// Render ImGui after X-Plane's window layer (using SEH to catch any crashes)
static int DrawCallbackImGui(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon)
{
    if (inPhase == xplm_Phase_Window && !inIsBefore)
    {
        if (g_imgui) {
            g_imgui->Render();
        }
    }
    return 1;
}

// ==================== X-Plane Plugin Entry Points ====================

// Actual initialization logic — called from XPluginStart's SEH wrapper.
// Kept separate to avoid C2712 (__try with C++ object unwinding).
static int DoXPluginStart(char* outName, char* outSig, char* outDesc) {
    strcpy(outName, PLUGIN_NAME);
    strcpy(outSig, PLUGIN_SIGNATURE);
    strcpy(outDesc, PLUGIN_DESCRIPTION);

    ++g_plugin_reload_count;

    {
        char xp_root[512] = {};
        XPLMGetSystemPath(xp_root);
        std::string logs_dir = std::string(xp_root) + "Resources\\plugins\\ISFP_xLink\\logs";
        Logger::Init(logs_dir, false);
    }
    XPLMDebugString("ISFP-xLink:Start:Logger init done\n");

    Logger::Main("ISFP-xLink:Start:0/9 XPluginStart entered\n");
    Logger::Main("=== XPluginStart ===");
    XPLMDebugString("ISFP-xLink:Start:0/9 entered\n");
    
    g_Menu = XPLMFindPluginsMenu();
    g_FirstMenu = XPLMAppendMenuItem(g_Menu, "ISFP-xLink", nullptr, 0);
    g_SecondMenu = XPLMCreateMenu("ISFP-xLink", g_Menu, g_FirstMenu, MenuHandler, nullptr);
    int Secret0 = XPLMAppendMenuItem(g_SecondMenu, "Show/Hide UI", reinterpret_cast<void*>(0), 0);
    int Secret1 = XPLMAppendMenuItem(g_SecondMenu, "Enable:FSD", reinterpret_cast<void*>(1), 0);
    int Secret2 = XPLMAppendMenuItem(g_SecondMenu, "Enable:CSL", reinterpret_cast<void*>(2), 0);
    int Secret3 = XPLMAppendMenuItem(g_SecondMenu, "Enable:Log", reinterpret_cast<void*>(3), 0);
    int Secret4 = XPLMAppendMenuItem(g_SecondMenu, "*Reload All Plugins", reinterpret_cast<void*>(4), 0);
    XPLMCheckMenuItem(g_SecondMenu, 0, g_imgui && g_imgui->IsVisible() ? xplm_Menu_Checked : xplm_Menu_NoCheck);
    XPLMCheckMenuItem(g_SecondMenu, 1, g_fsd_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
    XPLMCheckMenuItem(g_SecondMenu, 2, g_csl_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
    XPLMCheckMenuItem(g_SecondMenu, 3, g_csl_log_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
    XPLMDebugString("ISFP-xLink:Start:1/9 Menu created\n");

    g_network = new NetworkManager();
    g_datarefs = new DataRefManager();
    g_csl = new CSLManager();
    ValidateAndUpdateCSLConfig();
    XPLMDebugString("ISFP-xLink:Start:2-3/9 Managers created\n");
    
    if (!g_datarefs->Initialize()) {
        XPLMDebugString("ISFP-xLink:Start:FAIL DataRef::Initialize\n");
        return 0;
    }
    XPLMDebugString("ISFP-xLink:Start:4/9 DataRef initialized\n");
    
    if (!g_network->Initialize()) {
        XPLMDebugString("ISFP-xLink:Start:FAIL Network::Initialize\n");
        delete g_datarefs;
        delete g_network;
        return 0;
    }
    XPLMDebugString("ISFP-xLink:Start:5/9 Network initialized\n");

    if (!g_csl->Initialize()) {
        XPLMDebugString("ISFP-xLink:Start:FAIL CSL::Initialize\n");
        return 0;
    }
    XPLMDebugString("ISFP-xLink:Start:6/9 CSL initialized\n");

    g_config = new ConfigManager();
    g_config->Load();
    {
        std::string lang = g_config->GetString("ui.language", "CN");
        g_language = (lang == "CN") ? Language::CN : Language::EN;
        g_fsd_enabled         = g_config->GetBool("plugin.fsd.enabled", true);
        g_csl_enabled         = g_config->GetBool("plugin.csl.enabled", true);
        g_mouseyoke_enabled   = g_config->GetBool("plugin.mouseyoke.hidden", false);
        g_csl_log_enabled     = g_config->GetBool("plugin.csl.log_enabled", false);
        g_hotkey.vkey  = g_config->GetInt("hotkey.toggle_ui.vkey", 'I');
        g_hotkey.ctrl  = g_config->GetBool("hotkey.toggle_ui.ctrl", false);
        g_hotkey.shift = g_config->GetBool("hotkey.toggle_ui.shift", true);
        g_hotkey.alt   = g_config->GetBool("hotkey.toggle_ui.alt", false);
        g_mouseyoke_hotkey.vkey  = g_config->GetInt("hotkey.mouseyoke.vkey", 0);
        g_mouseyoke_hotkey.ctrl  = g_config->GetBool("hotkey.mouseyoke.ctrl", false);
        g_mouseyoke_hotkey.shift = g_config->GetBool("hotkey.mouseyoke.shift", false);
        g_mouseyoke_hotkey.alt   = g_config->GetBool("hotkey.mouseyoke.alt", false);
    }
    Logger::SetEnabled(g_csl_log_enabled.load());
    SyncMenuCheckmarks();
    XPLMDebugString("ISFP-xLink:Start:7/9 Config loaded\n");

    g_imgui = new ImGuiManager();
    if (!g_imgui->Initialize()) {
        XPLMDebugString("ISFP-xLink:Start:FAIL ImGui::Initialize\n");
        delete g_imgui;
        g_imgui = nullptr;
    } else {
        g_imgui->SetVisible(!g_imgui->GetHideOnStartup());
    }
    g_mouseyoke = new MouseYokeManager();
    if (g_mouseyoke->Initialize()) {
        if (g_mouseyoke_enabled.load()) g_mouseyoke->SetHidden(true);
    } else {
        XPLMDebugString("ISFP-xLink:Start:FAIL MouseYoke::Initialize\n");
        delete g_mouseyoke;
        g_mouseyoke = nullptr;
    }
    XPLMDebugString("ISFP-xLink:Start:8/9 ImGui+MouseYoke done\n");

    XPLMCreateFlightLoop_t flightLoop = {0};
    flightLoop.structSize       = sizeof(XPLMCreateFlightLoop_t);
    flightLoop.phase            = xplm_FlightLoop_Phase_AfterFlightModel;
    flightLoop.callbackFunc     = FlightLoopCallback;
    flightLoop.refcon           = nullptr;
    g_flight_loop_id = XPLMCreateFlightLoop(&flightLoop);
    if (g_flight_loop_id) XPLMScheduleFlightLoop(g_flight_loop_id, 0.1f, 1);
    if (g_network) g_network->StartServer(g_port);
    if (g_csl) g_csl->Start();
    XPLMDebugString("ISFP-xLink:Start:9/9 FlightLoop+Server+CSL done\n");

    XPLMRegisterDrawCallback(DrawCallback, xplm_Phase_Window, 1, nullptr);
    XPLMRegisterDrawCallback(DrawCallbackImGui, xplm_Phase_Window, 0, nullptr);
    g_draw_callback_registered = true;
    XPLMDebugString("ISFP-xLink:Start:Draw callbacks registered\n");

    Logger::Main("ISFP-xLink:Plugin:插件启动成功\n");
    return 1;
}

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
#pragma warning(push)
#pragma warning(disable: 2712)
    __try {
        return DoXPluginStart(outName, outSig, outDesc);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        XPLMDebugString("ISFP-xLink:Start:CRASH in XPluginStart! Returning 0\n");
        return 0;
    }
#pragma warning(pop)
}

PLUGIN_API void XPluginStop(void) {
    XPLMDebugString("ISFP-xLink:Stop:ENTER XPluginStop\n");

    // Mark plugin as disabled first to stop flight loop callbacks immediately
    g_plugin_enabled = false;

#pragma warning(push)
#pragma warning(disable: 2712)
    __try {
        if (g_flight_loop_id) {
            XPLMDestroyFlightLoop(g_flight_loop_id);
            g_flight_loop_id = nullptr;
        }
        if(g_draw_callback_registered)
        {
            XPLMUnregisterDrawCallback(DrawCallback, xplm_Phase_Window, 1, nullptr);
            XPLMUnregisterDrawCallback(DrawCallbackImGui, xplm_Phase_Window, 0, nullptr);
            g_draw_callback_registered = false;
        }
        if (g_SecondMenu) {
            XPLMDestroyMenu(g_SecondMenu);
            g_SecondMenu = nullptr;
        }
        if (g_imgui)
        {
            g_imgui->Shutdown();
            delete g_imgui;
            g_imgui = nullptr;
        }
        if (g_mouseyoke) {
            g_mouseyoke->Shutdown();
            delete g_mouseyoke;
            g_mouseyoke = nullptr;
        }
        if (g_network) {
            g_network->Shutdown();
            delete g_network;
            g_network = nullptr;
        }
        if (g_csl)
        {
            g_csl->Stop();
            g_csl->Shutdown();
            delete g_csl;
            g_csl = nullptr;
        }
        if (g_datarefs) {
            g_datarefs->Shutdown();
            delete g_datarefs;
            g_datarefs = nullptr;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        XPLMDebugString("ISFP-xLink:Stop:CRASH in cleanup block\n");
        g_flight_loop_id = nullptr;
        g_imgui = nullptr;
        g_mouseyoke = nullptr;
        g_network = nullptr;
        g_csl = nullptr;
        g_datarefs = nullptr;
    }
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 2712)
    __try {
        if (g_config) {
            g_config->Save();
            delete g_config;
            g_config = nullptr;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        XPLMDebugString("ISFP-xLink:Stop:CRASH in ConfigManager\n");
        g_config = nullptr;
    }
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 2712)
    __try {
        Logger::Main("=== XPluginStop ===");
        Logger::FlushAll();
        Logger::Shutdown();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        XPLMDebugString("ISFP-xLink:Stop:CRASH in Logger shutdown\n");
    }
#pragma warning(pop)

    XPLMDebugString("ISFP-xLink:Stop:EXIT XPluginStop\n");
    Logger::Main("ISFP-xLink:Plugin:插件已停止\n");
}

PLUGIN_API int XPluginEnable(void) {
    Logger::Main("ISFP-xLink:Plugin:插件正在启用...\n");

    g_plugin_enabled = true;

    // Server already started in XPluginStart, just make sure flight loop is running
    if (g_flight_loop_id)
    {
        XPLMScheduleFlightLoop(g_flight_loop_id, 0.1f, 1);
    }

    Logger::Main("ISFP-xLink:Plugin:插件已启用\n");
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    XPLMDebugString("ISFP-xLink:Disable:ENTER\n");
    g_plugin_enabled = false;
    if (g_network) {
        g_network->StopServer();
    }
    XPLMDebugString("ISFP-xLink:Disable:EXIT\n");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) {
    // Handle messages from X-Plane
    switch (inMsg) {
        case XPLM_MSG_PLANE_LOADED:
            Logger::Main("ISFP-xLink:Plugin:飞机已加载\n");
            break;
            
        case XPLM_MSG_AIRPORT_LOADED:
            Logger::Main("ISFP-xLink:Plugin:机场已加载\n");
            break;
            
        case XPLM_MSG_SCENERY_LOADED:
            Logger::Main("ISFP-xLink:Plugin:地景已加载\n");
            break;
    }
}

// ==================== Flight Loop Callback ====================
static float FlightLoopCallback(float inElapsedSinceLastCall, 
                                 float inElapsedTimeSinceLastFlightLoop, 
                                 int inCounter, 
                                 void* inRef) {

    // Safety: check plugin state FIRST before any access (avoids crash during reload)
    if (!g_plugin_enabled.load()) {
        return 0.1f;
    }

    // ---- Hotkey polling (Shift+I by default) ----
    if (g_hotkey.vkey > 0) {
        bool ctrl_ok  = g_hotkey.ctrl  ? (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 : true;
        bool shift_ok = g_hotkey.shift ? (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0 : true;
        bool alt_ok   = g_hotkey.alt   ? (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0 : true;
        bool key_down = (GetAsyncKeyState(g_hotkey.vkey) & 0x8000) != 0;
        bool hotkey_down = ctrl_ok && shift_ok && alt_ok && key_down;
        if (hotkey_down && !g_hotkey_was_down) {
            // Toggle UI
            if (g_imgui) {
                g_imgui->ToggleVisibility();
                SyncMenuCheckmarks();
            }
        }
        g_hotkey_was_down = hotkey_down;
    }
    // Mouse-yoke hotkey
    if (g_mouseyoke_hotkey.vkey > 0) {
        bool ctrl_ok  = g_mouseyoke_hotkey.ctrl  ? (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 : true;
        bool shift_ok = g_mouseyoke_hotkey.shift ? (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0 : true;
        bool alt_ok   = g_mouseyoke_hotkey.alt   ? (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0 : true;
        bool key_down = (GetAsyncKeyState(g_mouseyoke_hotkey.vkey) & 0x8000) != 0;
        bool hotkey_down = ctrl_ok && shift_ok && alt_ok && key_down;
        if (hotkey_down && !g_mouseyoke_hotkey_was_down) {
            g_mouseyoke_enabled = !g_mouseyoke_enabled;
            if (g_mouseyoke) {
                if (g_mouseyoke_enabled) {
                    g_mouseyoke->SetHidden(false);
                } else {
                    g_mouseyoke->SetHidden(true);
                }
            }
        }
        g_mouseyoke_hotkey_was_down = hotkey_down;
    }
    // ---- End hotkey polling ----

    if (!g_network || !g_datarefs) {
        return 0.1f;
    }

    if (!g_fsd_enabled) {
        return 0.1f;
    }

    // Get flight data
    FlightData data = g_datarefs->GetFlightData();
    
    // Send data to connected client
    if (data.valid && g_network->IsClientConnected()) {
        g_network->SendData(data);
    }

    // Re-check plugin state (may have changed during SendData)
    if (!g_plugin_enabled.load() || !g_network || !g_datarefs) {
        return 0.1f;
    }

    // CSL更新逻辑
    if (!g_fsd_enabled || !g_csl || !g_csl_enabled) {
        return 0.1f;
    }

    std::vector<ISFP::FlightData> current_players;
    {
        std::lock_guard<std::mutex> lock(ISFP::g_player_mutex);
        current_players = ISFP::g_valid_players;
    }

    // CSLManager
    for (size_t i = 0; i < current_players.size(); ++i) {
        if (i >= ISFP::g_csl->GetAircraftCount()) {
            std::string model_name = current_players[i].aircraft;
            std::string airline_code = ExtractAirlineCode(current_players[i].callsign);
            std::string aircraft_family = current_players[i].aircraft_family;
            CSLAircraft* ac = ISFP::g_csl->CreateAircraftByModelName(model_name, airline_code, aircraft_family);

            // Log aircraft position if logging enabled
            if (ac && g_csl_log_enabled) {
                Logger::Main(("ISFP-xLink:CSL_LOG:Aircraft position -> callsign: " + current_players[i].callsign +
                    " | lat: " + std::to_string(current_players[i].latitude) +
                    " | lon: " + std::to_string(current_players[i].longitude) +
                    " | alt(ft): " + std::to_string(current_players[i].altitude_msl) +
                    " | heading: " + std::to_string(current_players[i].heading) +
                    " | speed: " + std::to_string(current_players[i].groundspeed) + "\n").c_str());
            }

            if (!ac) continue;
        }
        CSLAircraft* ac = ISFP::g_csl->GetAircraft(i);
        if (ac) {
            FlightData predicted_pos = current_players[i];
            // 计算时间差：当前时间 - 最后一次网络更新时间
            double now = XPLMGetElapsedTime();
            double delta_time = now - predicted_pos.last_update_time;

            // 限制最大预测时间（防止无数据时飞机飞丢）
            if (delta_time > 10.0) delta_time = 10.0;

            // 执行位置预测
            PredictAircraftPosition(
                (predicted_pos.latitude),
                (predicted_pos.longitude),
                static_cast<float>(predicted_pos.heading),
                static_cast<float>(predicted_pos.groundspeed),
                delta_time
            );
            // 用预测后的平滑位置更新飞机
            ac->UpdatePosition(predicted_pos);
        }
    }

    for (size_t i = current_players.size(); i < ISFP::g_csl->GetAircraftCount(); ++i) {
        CSLAircraft* ac = ISFP::g_csl->GetAircraft(i);
        if (ac) ac->Hide();
    }

    // Return next callback interval (seconds) - 10Hz = 0.1s
    return 0.1f;
}

void OnFlightDataReceived(const ISFP::FlightData* data) {
    if (g_csl) {
        g_csl->UpdateAircraft(data);
    }
}

// ===================== Labels Drawing =====================
void RenderLabels()
{
    // Logger::Main("ISFP-xLink:Render:开始执行标签绘制\n");

    if (!g_plugin_enabled) {
        Logger::Main("ISFP-xLink:Render:插件未启用，退出绘制\n");
        return;
    }

    // Initialize datarefs on first call (and re-init after each reload)
    static int s_last_reload_count = -1;
    static XPLMDataRef s_world_matrix = nullptr;
    static XPLMDataRef s_proj_matrix = nullptr;
    static XPLMDataRef s_viewport = nullptr;

    if (s_last_reload_count != g_plugin_reload_count) {
        s_world_matrix = XPLMFindDataRef("sim/graphics/view/world_matrix");
        s_proj_matrix = XPLMFindDataRef("sim/graphics/view/projection_matrix");
        s_viewport = XPLMFindDataRef("sim/graphics/view/viewport");
        s_last_reload_count = g_plugin_reload_count;
    }

    if (!s_world_matrix || !s_proj_matrix || !s_viewport) {
        Logger::Main("ISFP-xLink:Render:矩阵数据引用初始化失败\n");
        return;
    }

    // Get matrices and viewport
    float world_mat[16], proj_mat[16];
    int viewport[4];
    XPLMGetDatavf(s_world_matrix, world_mat, 0, 16);
    XPLMGetDatavf(s_proj_matrix, proj_mat, 0, 16);
    XPLMGetDatavi(s_viewport, viewport, 0, 4);

    // Draw state setup
    XPLMSetGraphicsState(0, 0, 0, 1, 1, 0, 0);
    float color[] = { 0.2f, 1.0f, 0.2f };

    // Get player data for drawing
    std::vector<ISFP::FlightData> players;
    {
        std::lock_guard<std::mutex> lock(g_draw_mutex);
        players = g_draw_players;
    }

    char log_buf[256];
    snprintf(log_buf, sizeof(log_buf), "ISFP-xLink:Render:读取到飞机数量：%zu\n", players.size());
    // Logger::Main(log_buf);

    for (auto& ac : players)
    {
        if (!ac.valid || ac.callsign.empty()) continue;

        // 标签高度修正
        double world_x, world_y, world_z;
        double plane_alt_m = ac.altitude_msl * 0.3048; // 英尺转米
        XPLMWorldToLocal(ac.latitude, ac.longitude, plane_alt_m, &world_x, &world_y, &world_z);
        world_y += 10.0; // 飞机头顶偏移15米

        // 矩阵乘法：3D→裁剪空间
        float vec[4] = { (float)world_x, (float)world_y, (float)world_z, 1.0f };
        float view[4];
        for (int i = 0; i < 4; ++i) {
            view[i] = vec[0] * world_mat[i] + vec[1] * world_mat[i+4] + vec[2] * world_mat[i+8] + vec[3] * world_mat[i+12];
        }

        float clip[4];
        for (int i = 0; i < 4; ++i) {
            clip[i] = view[0] * proj_mat[i] + view[1] * proj_mat[i+4] + view[2] * proj_mat[i+8] + view[3] * proj_mat[i+12];
        }

        // 齐次除法：裁剪空间→NDC坐标
        if (clip[3] <= 0) continue;
        float ndc_x = clip[0] / clip[3];
        float ndc_y = clip[1] / clip[3];
        float ndc_z = clip[2] / clip[3];

        // 绘制在可视范围内的飞机
        if (ndc_x < -1 || ndc_x > 1 || ndc_y < -1 || ndc_y > 1 || ndc_z < -1 || ndc_z > 1) continue;

        int screen_x = static_cast<int>(viewport[0] + (ndc_x + 1.0f) * viewport[2] / 2.0f);
        int screen_y = static_cast<int>(viewport[1] + (ndc_y + 1.0f) * viewport[3] / 2.0f);

        // Text Alignment: Centered
        int text_width = static_cast<int>(ac.callsign.size()) * 8;
        screen_x -= text_width / 2;

        // Draw Callsign
        XPLMDrawString(color, screen_x, screen_y, (char*)ac.callsign.c_str(), nullptr, xplmFont_Proportional);
        snprintf(log_buf, sizeof(log_buf), "ISFP-xLink:Render:绘制呼号：%s 坐标：(%d, %d)\n", ac.callsign.c_str(), screen_x, screen_y);
        // Logger::Main(log_buf);
    }

    // Default state restore
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 1, 0);
    // Logger::Main("ISFP-xLink:Render:标签绘制执行完毕\n");
}
