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
        std::string squawk_mode; // "S"=未开启, "N"=已开启
        int gear_deploy;
        float flaps_ratio;
        float throttle_ratio;
        bool valid;
        double last_update_time; //用于CSL玩家数据过期判断->断开连接
        std::string callsign;
        std::string aircraft; // 飞机模型名称（用于CSL映射）
        std::string aircraft_family; // 飞机机型家族（如A320、B738），来自JSON的_aircraft_family字段

        FlightData() : valid(false) {}
    };

    // CSL Extern
    extern std::vector<FlightData> g_valid_players;
    extern std::mutex g_player_mutex;
    extern std::vector<FlightData> g_draw_players;
    extern std::mutex g_draw_mutex;
    extern std::atomic<bool> g_csl_log_enabled;

    // EFB Query Results (thread-safe)
    extern std::string g_efb_route_result;
    extern std::mutex g_efb_route_mutex;
    extern std::string g_efb_weather_result;
    extern std::mutex g_efb_weather_mutex;

    // Plugin version info
    constexpr const char* PLUGIN_NAME = "ISFP-xLink";
    constexpr const char* PLUGIN_SIGNATURE = "com.isfp.xlink";
    constexpr const char* PLUGIN_DESCRIPTION = "ISFP-xLink - X-Plane Native Plugin";
    constexpr int PLUGIN_VERSION = 1260;

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
        bool SendJson(const std::string& json_str);

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
        std::atomic<bool> running_{true};
        int rcv_epoch_{0};  // Incremented on each StartRcvThread() to detect stale RevData instances
        void RevData(int epoch);
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
        XPLMDataRef dr_transponder_mode_;
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
        XPLMProbeRef terrain_probe_ = nullptr;
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

        CSLAircraft* CreateAircraftByModelName(const std::string& model_name, const std::string& airline_code = "", const std::string& aircraft_family = "");

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

    // Profiles path helper
    std::string GetProfilesPath();

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

    // Language
    enum class Language { EN, CN };
    extern Language g_language;

    // Hotkey binding
    struct HotkeyBinding {
        int vkey = 'I';       // Virtual key code
        bool ctrl = false;    // Ctrl modifier
        bool shift = true;    // Shift modifier
        bool alt = false;     // Alt modifier
    };
    extern HotkeyBinding g_hotkey;
    extern HotkeyBinding g_mouseyoke_hotkey;

    // Sync menu checkmarks with current state
    void SyncMenuCheckmarks();

    // Global instances
    extern NetworkManager* g_network;
    extern DataRefManager* g_datarefs;
    extern CSLManager* g_csl;
    extern int g_port;
    extern std::atomic<bool> g_fsd_enabled;
    extern std::atomic<bool> g_csl_enabled;

    // Mouse Yoke Manager
    class MouseYokeManager;
    extern MouseYokeManager* g_mouseyoke;
    extern std::atomic<bool> g_mouseyoke_enabled;

    // ==================== Schema-Driven ConfigManager ====================
    // All settings are defined in a single schema table (RegisterDefaults) with:
    //   - key: dot-notation path, e.g. "window.position.x"
    //   - default: typed default value
    //   - description: human-readable documentation
    //   - section: group name for logical organization
    //
    // JSON output follows a clean, hierarchical config format:
    //   {
    //       "meta": { "version": 1, ... },
    //       "window": { "position": { "x": 50, "y": 50 }, ... },
    //       "ui": { "language": "EN", "scale": 1.0, ... },
    //       ...
    //   }
    //
    // To add a new setting: just add one Register() call in RegisterDefaults().
    struct ConfigSchemaEntry {
        const char* key;            // dot-notation path, e.g. "window.position.x"
        json        default_value;  // typed default
        const char* description;    // human-readable description
        const char* section;        // group name (window, ui, plugin, hotkey, meta)
    };

    class ConfigManager {
    public:
        ConfigManager();
        ~ConfigManager();

        // Register all known settings with defaults (called once after construction)
        void RegisterDefaults();

        // Load from Settings.json, then ApplyDefaults() for any missing keys
        bool Load();
        // Save entire data_ to Settings.json with clean formatting
        bool Save();

        // --- Typed getters (read from data_, fallback to default if not found) ---
        int         GetInt(const std::string& key, int default_val = 0) const;
        float       GetFloat(const std::string& key, float default_val = 0.0f) const;
        bool        GetBool(const std::string& key, bool default_val = false) const;
        std::string GetString(const std::string& key, const std::string& default_val = "") const;

        // --- Typed setters (write into data_) ---
        void SetInt(const std::string& key, int val);
        void SetFloat(const std::string& key, float val);
        void SetBool(const std::string& key, bool val);
        void SetString(const std::string& key, const std::string& val);

        // Key existence check
        bool HasKey(const std::string& key) const;
        void RemoveKey(const std::string& key);

        // Access all registered schema entries (for introspection / migration)
        const std::vector<ConfigSchemaEntry>& GetSchema() const { return schema_; }

    private:
        json data_;
        std::string file_path_;
        std::vector<ConfigSchemaEntry> schema_;

        // Fill in any missing keys from their registered defaults
        void ApplyDefaults();

        // Register a single schema entry
        void Register(
            const char* key,
            json default_value,
            const char* description,
            const char* section);

        // Navigate dot-notation path to parent node, extract leaf key name
        json*       ResolvePtr(std::string& out_leaf, const std::string& key, bool create);
        const json* ResolvePtr(std::string& out_leaf, const std::string& key) const;
    };

    extern ConfigManager* g_config;

} // namespace ISFP

#endif // ISFP_PLUGIN_H