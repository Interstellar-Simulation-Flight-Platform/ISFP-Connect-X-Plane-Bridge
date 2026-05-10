/*
 * ISFP Connect Bridge - X-Plane Native Plugin
 * Native X-Plane plugin based on XPSDK430
 * Communicates with ISFP-Connect Python app via TCP
 */

#include "isfp_plugin.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include <cstring>
#include <sstream>
#include <iomanip>

// X-Plane SDK callback
static float FlightLoopCallback(float inElapsedSinceLastCall, 
                                 float inElapsedTimeSinceLastFlightLoop, 
                                 int inCounter, 
                                 void* inRef);

namespace ISFP {

// Global instances
NetworkManager* g_network = nullptr;
DataRefManager* g_datarefs = nullptr;

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

} // namespace ISFP

using namespace ISFP;

// ==================== X-Plane Plugin MenuHandler ====================

void MenuHandler(void *inMenuRef, void *inItemRef)
{
    int cmd = (int)inItemRef;
    switch (cmd)
    {
    case 0:
        g_fsd_enabled = !g_fsd_enabled;
        //当前FSD状态判断
        if (!g_fsd_enabled && g_network) {
            g_network->StopServer();
            XPLMDebugString("ISFP Connect Bridge: FSD Is Close\n");
        }
        else if (g_fsd_enabled && g_network) {
            g_network->StartServer(g_port);
            XPLMDebugString("ISFP Connect Bridge: FSD Is Activate\n");
        }
        //更新菜单
        XPLMCheckMenuItem(g_SecondMenu, 0, 
            g_fsd_enabled ? xplm_Menu_Checked : xplm_Menu_NoCheck);
        break;
    case 1:
        XPLMReloadPlugins();
        break;
    default:
        break;
    }
}

// ==================== X-Plane Plugin Entry Points ====================

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    // Set plugin info
    strcpy(outName, PLUGIN_NAME);
    strcpy(outSig, PLUGIN_SIGNATURE);
    strcpy(outDesc, PLUGIN_DESCRIPTION);

    g_Menu = XPLMFindPluginsMenu();

    //菜单
    g_FirstMenu = XPLMAppendMenuItem(g_Menu, "ISFP Connect Bridge", nullptr, 0);

    //二级菜单
    g_SecondMenu = XPLMCreateMenu("ISFP Connect Bridge", g_Menu, g_FirstMenu, MenuHandler, nullptr);
    int Secret0 = XPLMAppendMenuItem(g_SecondMenu, "Activate:FSD", reinterpret_cast<void*>(0), 0);
    int Secret1 = XPLMAppendMenuItem(g_SecondMenu, "*Reload All Plugins", reinterpret_cast<void*>(1), 0);
    XPLMCheckMenuItem(g_SecondMenu, Secret0, xplm_Menu_Checked);
    
    XPLMDebugString("ISFP Connect Bridge: Plugin starting...\n");
    
    // Create manager instances
    g_network = new NetworkManager();
    g_datarefs = new DataRefManager();
    
    // Initialize datarefs
    if (!g_datarefs->Initialize()) {
        XPLMDebugString("ISFP Connect Bridge: Failed to initialize DataRef manager\n");
        return 0;
    }
    
    // Initialize network
    if (!g_network->Initialize()) {
        XPLMDebugString("ISFP Connect Bridge: Failed to initialize Network manager\n");
        delete g_datarefs;
        delete g_network;
        return 0;
    }
    
    // Register flight loop callback (2Hz)
    XPLMCreateFlightLoop_t flightLoop;
    flightLoop.structSize = sizeof(XPLMCreateFlightLoop_t);
    flightLoop.phase = xplm_FlightLoop_Phase_AfterFlightModel;
    flightLoop.callbackFunc = FlightLoopCallback;
    flightLoop.refcon = nullptr;
    
    g_flight_loop_id = XPLMCreateFlightLoop(&flightLoop);
    
    // Start flight loop immediately
    if (g_flight_loop_id) {
        XPLMScheduleFlightLoop(g_flight_loop_id, -1, 1);
    }
    
    // Start TCP server immediately
    if (g_network) {
        g_network->StartServer(g_port);
    }
    
    XPLMDebugString("ISFP Connect Bridge: Plugin started successfully\n");
    
    return 1;
}

PLUGIN_API void XPluginStop(void) {
    XPLMDebugString("ISFP Connect Bridge: Plugin stopping...\n");
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
    
    XPLMDebugString("ISFP Connect Bridge: Plugin stopped\n");
}

PLUGIN_API int XPluginEnable(void) {
    XPLMDebugString("ISFP Connect Bridge: Plugin enabling...\n");

    g_plugin_enabled = true;

    // Server already started in XPluginStart, just make sure flight loop is running
    if (g_flight_loop_id) {
        XPLMScheduleFlightLoop(g_flight_loop_id, -1, 1);
    }

    XPLMDebugString("ISFP Connect Bridge: Plugin enabled\n");
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    XPLMDebugString("ISFP Connect Bridge: Plugin disabling...\n");

    g_plugin_enabled = false;

    // Stop server
    if (g_network) {
        g_network->StopServer();
    }

    XPLMDebugString("ISFP Connect Bridge: Plugin disabled\n");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) {
    // Handle messages from X-Plane
    switch (inMsg) {
        case XPLM_MSG_PLANE_LOADED:
            XPLMDebugString("ISFP Connect Bridge: Aircraft loaded\n");
            break;
            
        case XPLM_MSG_AIRPORT_LOADED:
            XPLMDebugString("ISFP Connect Bridge: Airport loaded\n");
            break;
            
        case XPLM_MSG_SCENERY_LOADED:
            XPLMDebugString("ISFP Connect Bridge: Scenery loaded\n");
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
        return -1;
        
    }
    
    // Get flight data
    FlightData data = g_datarefs->GetFlightData();
    
    // Send data to connected client
    if (data.valid && g_network->IsClientConnected()) {
        g_network->SendData(data);
    }
    
    // Return next callback interval (seconds) - 10Hz = 0.1s
    return 0.1f;
}
