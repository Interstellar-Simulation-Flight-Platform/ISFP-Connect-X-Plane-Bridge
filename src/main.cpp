/*
 * ISFPConnectBridge - X-Plane Native Plugin
 * Native X-Plane plugin based on XPSDK430
 * Communicates with ISFP-Connect Python app via TCP
 */

#include "isfp_plugin.h"
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

// Draw callback state
static bool g_draw_callback_registered = false;
static XPLMDataRef g_mat_world = nullptr;
static XPLMDataRef g_mat_proj = nullptr;
static XPLMDataRef g_viewport = nullptr;
static XPLMDataRef g_screen_h = nullptr;

// Forward declarations
void RenderLabels();
static int DrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon);

namespace ISFP {
	class CSLAircraft;
}

// X-Plane SDK callback
static float FlightLoopCallback(float inElapsedSinceLastCall, 
                                 float inElapsedTimeSinceLastFlightLoop, 
                                 int inCounter, 
                                 void* inRef);

namespace ISFP {
// Global instances
NetworkManager* g_network = nullptr;
DataRefManager* g_datarefs = nullptr;
CSLManager* g_csl = nullptr;

// Plugin state
static std::atomic<bool> g_plugin_enabled{false};
static XPLMFlightLoopID g_flight_loop_id = nullptr;

// Configuration
static std::string g_host = DEFAULT_HOST;
static int g_port = DEFAULT_PORT;

// MenuHandler
static XPLMMenuID g_Menu ;
static int g_FirstMenu;
static XPLMMenuID g_SecondMenu;

//FSD Socket Activate
static std::atomic<bool> g_fsd_enabled = true;

//CSL Activate
static std::atomic<bool> g_csl_enabled = true;

//CSL Config
std::vector<FlightData> g_valid_players;
std::mutex g_player_mutex;
std::vector<FlightData> g_draw_players;
std::mutex g_draw_mutex;

} // namespace ISFP

using namespace ISFP;

// ==================== X-Plane Plugin MenuHandler ====================

void MenuHandler(void *inMenuRef, void *inItemRef)
{
    int cmd = (int)(intptr_t)inItemRef;
    switch (cmd)
    {
    case 0:
        g_fsd_enabled = !g_fsd_enabled;
        //当前FSD状态判断
        if (!g_fsd_enabled && g_network) {
            g_network->StopServer();
            XPLMDebugString("ISFPConnectBridge:Menu:FSD服务已关闭\n");
        }
        else if (g_fsd_enabled && g_network) {
            g_network->StartServer(g_port);
            XPLMDebugString("ISFPConnectBridge:Menu:FSD服务已启用\n");
        }
        //更新菜单
        XPLMCheckMenuItem(g_SecondMenu, 0, 
            g_fsd_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
        break;
    case 1:
        g_csl_enabled = !g_csl_enabled;
        //当前CSL状态判断
        if (!g_csl_enabled) {
            g_csl->Stop();
            XPLMDebugString("ISFPConnectBridge:Menu:CSL模型已关闭\n");
        }
        else if (g_csl_enabled) {
            g_csl->Start();
            XPLMDebugString("ISFPConnectBridge:Menu:CSL模型已启用\n");
        }
        //更新菜单
        XPLMCheckMenuItem(g_SecondMenu, 1, 
            g_csl_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
        break;
    case 2:
        XPLMReloadPlugins();
        XPLMDebugString("ISFPConnectBridge:Menu:已重新加载所有插件\n");
        break;
    default:
        break;
    }
}

// ==================== X-Plane Plugin Draw Callback ====================
static int DrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon)
{
    if (inPhase == xplm_Phase_Window && inIsBefore)
    {
        RenderLabels();
    }
    return 1;
}

// ==================== X-Plane Plugin Entry Points ====================

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    // Set plugin info
    strcpy(outName, PLUGIN_NAME);
    strcpy(outSig, PLUGIN_SIGNATURE);
    strcpy(outDesc, PLUGIN_DESCRIPTION);
    
    g_Menu = XPLMFindPluginsMenu();

    //菜单
    g_FirstMenu = XPLMAppendMenuItem(g_Menu, "ISFPConnectBridge", nullptr, 0);

    //二级菜单
    g_SecondMenu = XPLMCreateMenu("ISFPConnectBridge", g_Menu, g_FirstMenu, MenuHandler, nullptr);
    int Secret0 = XPLMAppendMenuItem(g_SecondMenu, "Activate:FSD", reinterpret_cast<void*>(0), 0);
    int Secret1 = XPLMAppendMenuItem(g_SecondMenu, "Activate:CSL", reinterpret_cast<void*>(1), 0);
    int Secret2 = XPLMAppendMenuItem(g_SecondMenu, "*Reload All Plugins", reinterpret_cast<void*>(2), 0);
    XPLMCheckMenuItem(g_SecondMenu, 0, g_fsd_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
    XPLMCheckMenuItem(g_SecondMenu, 1, g_csl_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
    
    XPLMDebugString("ISFPConnectBridge:Plugin:插件正在启动...\n");
    
    // Create manager instances
    g_network = new NetworkManager();
    g_datarefs = new DataRefManager();
    g_csl = new CSLManager();

    ValidateAndUpdateCSLConfig();
    
    
    // Initialize datarefs
    if (!g_datarefs->Initialize()) {
        XPLMDebugString("ISFPConnectBridge:DataRef:数据引用管理器初始化失败\n");
        return 0;
    }
    
    // Initialize network
    if (!g_network->Initialize()) {
        XPLMDebugString("ISFPConnectBridge:Network:网络管理器初始化失败\n");
        delete g_datarefs;
        delete g_network;
        return 0;
    }

    // Initialize CSL
    if (!g_csl->Initialize()) {
        XPLMDebugString("ISFPConnectBridge:CSL:CSL管理器初始化失败\n");
        return 0;
    }
    
    // Register flight loop callback (2Hz)
    XPLMCreateFlightLoop_t flightLoop = {0};
    flightLoop.structSize       = sizeof(XPLMCreateFlightLoop_t);
    flightLoop.phase            = xplm_FlightLoop_Phase_AfterFlightModel;
    flightLoop.callbackFunc     = FlightLoopCallback;
    flightLoop.refcon           = nullptr;
    
    g_flight_loop_id = XPLMCreateFlightLoop(&flightLoop);

    
    // Start flight loop immediately
    if (g_flight_loop_id) {
        XPLMScheduleFlightLoop(g_flight_loop_id, 0.1f, 1);
    }
    
    // Start TCP server immediately
    if (g_network) {
        g_network->StartServer(g_port);
    }

    // Start CSL
    if (g_csl) {
        g_csl->Start();
    }

    XPLMRegisterDrawCallback(DrawCallback, xplm_Phase_Window, 1, nullptr);
    g_draw_callback_registered = true;
    XPLMDebugString("ISFPConnectBridge:Draw:绘制回调已注册\n");
    
    XPLMDebugString("ISFPConnectBridge:Plugin:插件启动成功\n");
    
    return 1;
}

PLUGIN_API void XPluginStop(void) {
    XPLMDebugString("ISFPConnectBridge:Plugin:插件正在停止...\n");

    if(g_draw_callback_registered)
    {
        XPLMUnregisterDrawCallback(DrawCallback, xplm_Phase_Window, 1, nullptr);
        g_draw_callback_registered = false;
    }

    // Destory Menu
    if (g_SecondMenu) {
        XPLMDestroyMenu(g_SecondMenu);
        g_SecondMenu = nullptr;
    }
    
    // Destroy flight loop
    if (g_flight_loop_id) {
        XPLMDestroyFlightLoop(g_flight_loop_id);
        g_flight_loop_id = nullptr;
    }

    // Destory CSL
    if (g_csl)
    {
        g_csl->Stop();
        g_csl->Shutdown();
        delete g_csl;
        g_csl = nullptr;
    }
    
    // Cleanup
    if (g_network) {
        g_network->Shutdown();
        delete g_network;
        g_network = nullptr;
    }
    
    if (g_datarefs) {
        g_datarefs->Shutdown();
        delete g_datarefs;
        g_datarefs = nullptr;
    }
    
    XPLMDebugString("ISFPConnectBridge:Plugin:插件已停止\n");
}

PLUGIN_API int XPluginEnable(void) {
    XPLMDebugString("ISFPConnectBridge:Plugin:插件正在启用...\n");

    g_plugin_enabled = true;

    // Server already started in XPluginStart, just make sure flight loop is running
    if (g_flight_loop_id)
    {
        XPLMScheduleFlightLoop(g_flight_loop_id, 0.1f, 1);
    }

    XPLMDebugString("ISFPConnectBridge:Plugin:插件已启用\n");
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    XPLMDebugString("ISFPConnectBridge:Plugin:插件正在禁用...\n");

    g_plugin_enabled = false;

    // Stop server
    if (g_network) {
        g_network->StopServer();
    }

    XPLMDebugString("ISFPConnectBridge:Plugin:插件已禁用\n");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) {
    // Handle messages from X-Plane
    switch (inMsg) {
        case XPLM_MSG_PLANE_LOADED:
            XPLMDebugString("ISFPConnectBridge:Plugin:飞机已加载\n");
            break;
            
        case XPLM_MSG_AIRPORT_LOADED:
            XPLMDebugString("ISFPConnectBridge:Plugin:机场已加载\n");
            break;
            
        case XPLM_MSG_SCENERY_LOADED:
            XPLMDebugString("ISFPConnectBridge:Plugin:地景已加载\n");
            break;
    }
}

// ==================== Flight Loop Callback ====================
static float FlightLoopCallback(float inElapsedSinceLastCall, 
                                 float inElapsedTimeSinceLastFlightLoop, 
                                 int inCounter, 
                                 void* inRef) {

    if (!g_fsd_enabled) {
        return 0.1f;
    }

    if (!g_plugin_enabled || !g_network || !g_datarefs) {
        return 0.1f;
    }
    
    // Get flight data
    FlightData data = g_datarefs->GetFlightData();
    
    // Send data to connected client
    if (data.valid && g_network->IsClientConnected()) {
        g_network->SendData(data);
    }

    // CSL Update
    if (!g_fsd_enabled || !g_plugin_enabled || !g_network || !g_datarefs) {
        return 0.1f;
    }

    // CSL更新逻辑
    if (!ISFP::g_csl || !ISFP::g_csl_enabled) {
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
            CSLAircraft* ac = ISFP::g_csl->CreateAircraftByModelName("A320fCFM_CDN");
            if (!ac) continue;
        }
        CSLAircraft* ac = ISFP::g_csl->GetAircraft(i);
        if (ac) {
            FlightData predicted_pos = current_players[i];
            // 计算时间差：当前时间 - 最后一次网络更新时间
            double now = XPLMGetElapsedTime();
            double delta_time = now - predicted_pos.last_update_time;

            // 限制最大预测时间（防止无数据时飞机飞丢）
            if (delta_time > 6.0) delta_time = 6.0;

            // 执行位置预测
            CSLManager::PredictAircraftPosition(
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
    // XPLMDebugString("ISFPConnectBridge:Render:开始执行标签绘制\n");

    if (!g_plugin_enabled) {
        XPLMDebugString("ISFPConnectBridge:Render:插件未启用，退出绘制\n");
        return;
    }

    // Initialize datarefs on first call
    static bool s_inited = false;
    static XPLMDataRef s_world_matrix = nullptr;
    static XPLMDataRef s_proj_matrix = nullptr;
    static XPLMDataRef s_viewport = nullptr;

    if (!s_inited) {
        s_world_matrix = XPLMFindDataRef("sim/graphics/view/world_matrix");
        s_proj_matrix = XPLMFindDataRef("sim/graphics/view/projection_matrix");
        s_viewport = XPLMFindDataRef("sim/graphics/view/viewport");
        s_inited = true;
    }

    if (!s_world_matrix || !s_proj_matrix || !s_viewport) {
        XPLMDebugString("ISFPConnectBridge:Render:矩阵数据引用初始化失败\n");
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
    snprintf(log_buf, sizeof(log_buf), "ISFPConnectBridge:Render:读取到飞机数量：%zu\n", players.size());
    // XPLMDebugString(log_buf);

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
        snprintf(log_buf, sizeof(log_buf), "ISFPConnectBridge:Render:绘制呼号：%s 坐标：(%d, %d)\n", ac.callsign.c_str(), screen_x, screen_y);
        // XPLMDebugString(log_buf);
    }

    // Default state restore
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 1, 0);
    // XPLMDebugString("ISFPConnectBridge:Render:标签绘制执行完毕\n");
}