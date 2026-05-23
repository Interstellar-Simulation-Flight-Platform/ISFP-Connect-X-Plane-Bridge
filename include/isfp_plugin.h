#ifndef ISFP_PLUGIN_H
#define ISFP_PLUGIN_H

// Prevent Windows Socket API conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <cmath>
#include <vector>
#include <filesystem>

// X-Plane SDK Headers
#include "XPLMDataAccess.h"
#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMInstance.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#pragma comment(lib, "ws2_32.lib")

namespace ISFP {
    // Flight data structure
    struct FlightData {
        double latitude;
        double longitude;
        double altitude;
        double elevation;
        double pitch;
        double bank;
        double heading;
        double indicated_airspeed;
        double true_airspeed;
        double groundspeed;
        double vertical_speed;
        double altitude_msl;
        double altitude_agl;
        double mag_heading;
        double true_heading;
        int com1_freq;
        int com2_freq;
        int transponder;
        int gear_deploy;
        float flaps_ratio;
        float throttle_ratio;
        bool valid;
        double last_update_time; //用于CSL玩家数据过期判断->断开连接
        std::string callsign;

        std::string aircraft; // 飞机模型名称（用于CSL映射）


        FlightData() : valid(false) {}
    };

    // CSL Extern
    extern std::vector<FlightData> g_valid_players;
    extern std::mutex g_player_mutex;
    extern std::vector<FlightData> g_draw_players;
    extern std::mutex g_draw_mutex;

    // Plugin version info
    constexpr const char* PLUGIN_NAME = "ISFP Connect Bridge";
    constexpr const char* PLUGIN_SIGNATURE = "com.isfp.connect";
    constexpr const char* PLUGIN_DESCRIPTION = "ISFP Connect Plugin for X-Plane - Native TCP Server";
    constexpr int PLUGIN_VERSION = 110;

    // Default server config - plugin acts as server
    constexpr const char* DEFAULT_HOST = "0.0.0.0";  // Listen on all interfaces
    constexpr int DEFAULT_PORT = 51001;
    constexpr int DATA_SEND_INTERVAL_MS = 500; // 2Hz data send frequency

    // MainThread
    enum class MainThreadTaskType {
        DESTROY_ALL_AIRCRAFT,
        CLEAR_PLAYER_DATA,
        DESTROY_AND_CLEAR
    };

    struct MainThreadTask {
        MainThreadTaskType type;
    };

    // Network manager class - acts as TCP server
    class NetworkManager {
    public:
        NetworkManager();
        ~NetworkManager();
        
        bool Initialize();
        void Shutdown();
        
        bool StartServer(int port);  // Start listening
        void StopServer();
        bool IsClientConnected() const { return client_connected_; }
        
        bool SendData(const FlightData& data);

        // MainTread
        void QueueMainThreadTask(MainThreadTaskType task_type);
        static float MainThreadFlightLoop(
            float inElapsedSinceLastCall,
            float inElapsedTimeSinceLastFlightLoop,
            int inCounter,
            void* inRefcon
        );
         
        // For receiving data from client
        void StartRcvThread();
        void StopRcvThread();
        
    private:
        void ServerLoop();  // Accept connections
        void ClientLoop();  // Handle client communication
        
        SOCKET listen_socket_;
        SOCKET client_socket_;
        std::atomic<bool> server_running_;
        std::atomic<bool> client_connected_;
        
        int port_;
        
        std::thread server_thread_;
        std::mutex socket_mutex_;
        
        WSADATA wsa_data_;
        bool wsa_initialized_;

        // MainThread
        std::queue<MainThreadTask> main_thread_tasks_;
        std::mutex main_thread_mutex_;
        std::condition_variable main_thread_cv_;
        bool main_thread_callback_registered_ = false;
        XPLMFlightLoopID flight_loop_id_ = nullptr;

        // For receiving data from client
        std::string Rcv_Buffer;
        std::thread Rcv_Thread;
        bool running_ = true;
        void RevData();
        void ProcessRcvJson(const std::string& json_str);
    };

    // DataRef manager class
    class DataRefManager {
    public:
        DataRefManager();
        ~DataRefManager();
        
        bool Initialize();
        void Shutdown();
        
        FlightData GetFlightData();
        
        void SetCom1Freq(int freq);
        void SetCom2Freq(int freq);
        void SetTransponder(int code);
        
    private:
        void FindDataRefs();
        
        XPLMDataRef dr_latitude_;
        XPLMDataRef dr_longitude_;
        XPLMDataRef dr_altitude_;
        XPLMDataRef dr_elevation_;
        XPLMDataRef dr_pitch_;
        XPLMDataRef dr_roll_;
        XPLMDataRef dr_heading_;
        XPLMDataRef dr_indicated_airspeed_;
        XPLMDataRef dr_true_airspeed_;
        XPLMDataRef dr_groundspeed_;
        XPLMDataRef dr_vertical_speed_;
        XPLMDataRef dr_altitude_msl_;
        XPLMDataRef dr_altitude_agl_;
        XPLMDataRef dr_mag_heading_;
        XPLMDataRef dr_true_heading_;
        XPLMDataRef dr_com1_freq_;
        XPLMDataRef dr_com2_freq_;
        XPLMDataRef dr_transponder_;
        XPLMDataRef dr_gear_deploy_;
        XPLMDataRef dr_flaps_ratio_;
        XPLMDataRef dr_throttle_ratio_;
    };

    // CSL Struct
    struct CSLConfig
    {
        std::string csl_path;
        std::vector<std::string> aircraft_mapped;
    };

    // CSL Aircraft
    class CSLAircraft {
        public:
        CSLAircraft();
        ~CSLAircraft();
        bool LoadModel(const char* objFilePath);
        void UpdatePosition(const FlightData& data);
        void Hide();
        void Destroy();

        const std::string& GetCallsign() const { return m_callsign; }
        double GetLatitude() const { return m_latitude; }
        double GetLongitude() const { return m_longitude; }
        double GetAltitude() const { return m_altitude; }
    private:
        XPLMObjectRef obj_ = nullptr;
        XPLMInstanceRef instance_ = nullptr;
        XPLMDrawInfo_t draw_info_;

        std::string m_callsign; // 模型的呼号
        double m_latitude;      // 模型实际纬度
        double m_longitude;     // 模型实际经度
        double m_altitude;      // 模型实际高度（米）
    };

    // CSL Manager
    class CSLManager{
    public:
        CSLManager();
        ~CSLManager();
        bool Initialize();
        bool Start();
        void Stop();
        void Shutdown();
        void DestroyAllAircraft();

        //批量接口
        void CreateAircraftBatch(const std::vector<std::string>& model_paths);
        void UpdateAllAircraft(const std::vector<FlightData>& flight_list);
        void UpdateAircraft(const FlightData* flight_data);

        static void PredictAircraftPosition(
            double& lat, double& lon,
            float heading, float groundspeed,
            double delta_time
        );

        CSLAircraft* CreateAircraftByModelName(const std::string& model_name, const std::string& airline_code = "");

        CSLAircraft* GetAircraft(size_t index);

        void RemoveAircraft(CSLAircraft* ac);

        size_t GetAircraftCount() const;

    private:
        bool initialized_ = false;
        bool running_ = false;
        std::vector<CSLAircraft*> aircraft_list_;
    };

    // CSL Config management
    std::string GetConfigFilePath();
    bool LoadCSLConfig(CSLConfig& outCfg);
    void SaveCSLConfig(const CSLConfig& cfg);
    void ValidateAndUpdateCSLConfig();
    std::string ExtractAirlineCode(const std::string& callsign);

    //调用接口
    CSLAircraft* SpawnCSLAircraftAtLocation(
        const std::string& model_name,
        double longitude,
        double latitude,
        double altitude_msl,
        float heading = 0.0f,
        float pitch = 0.0f,
        float roll = 0.0f
    );

    // Global instances
    extern NetworkManager* g_network;
    extern DataRefManager* g_datarefs;
    extern CSLManager* g_csl;

} // namespace ISFP

#endif // ISFP_PLUGIN_H