#include "isfp_plugin.h"
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <corecrt_math_defines.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <XPLMMap.h>
#include <XPLMGraphics.h>
#include <XPLMScenery.h>
#include "logger.h"

#undef min
#undef max

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace ISFP
{
    // Global singleton instance
    CSLManager g_CSLManager;

    size_t CSLManager::GetAircraftCount() const {
        return aircraft_list_.size();
    }

    // ==================== Profiles Path ====================
    std::string GetProfilesPath() {
        char xp_root[512] = { 0 };
        XPLMGetSystemPath(xp_root);
        fs::path profiles = fs::path(xp_root) / "Resources" / "plugins" / "ISFP_xLink" / "Profiles";
        return profiles.string();
    }

    // ==================== CSL Config File Operations ====================
    // Get full path of CSL configuration file
    std::string GetConfigFilePath() {
        fs::path cfg_path = fs::path(GetProfilesPath()) / "CSL_Config.json";
        return cfg_path.string();
    }

    // Load CSL config from JSON file
    bool LoadCSLConfig(CSLConfig& outCfg) {
        std::string config_path = GetConfigFilePath();
        if (!fs::exists(config_path)) {
            Logger::CSL("ISFP-xLink:CSL:配置文件不存在\n");
            return false;
        }

        try {
            std::ifstream file(config_path);
            json j = json::parse(file, nullptr, false);
            if (j.is_discarded()) {
                Logger::CSL("ISFP-xLink:CSL:配置文件解析失败\n");
                return false;
            }

            outCfg.csl_path = j["csl_path"].get<std::string>();
            outCfg.aircraft_mapped.clear();
            Logger::CSL("ISFP-xLink:CSL:配置加载成功\n");
            return true;
        }
        catch (...) {
            Logger::CSL("ISFP-xLink:CSL:配置加载异常\n");
            return false;
        }
    }

    // Save CSL config to JSON file
    void SaveCSLConfig(const CSLConfig& cfg) {
        std::string config_path = GetConfigFilePath();
        try {
            json j;
            j["csl_path"] = cfg.csl_path;
            j["aircraft_mapped"] = json::object();
            std::ofstream file(config_path);
            file << std::setw(4) << j << std::endl;
        }
        catch (...) {
            Logger::CSL("ISFP-xLink:CSL:配置保存失败\n");
        }
    }

    // Scan CSL directory and generate folder-to-OBJ mapping config
    void ValidateAndUpdateCSLConfig() {
        CSLConfig cfg;
        if (!LoadCSLConfig(cfg)) {
            cfg.csl_path = "Resources/plugins/IVAO_CSL/CSL";
            cfg.aircraft_mapped.clear();
        }

        char xp_root[512] = { 0 };
        XPLMGetSystemPath(xp_root);
        fs::path full_csl_path = fs::path(xp_root) / cfg.csl_path;

        if (!fs::exists(full_csl_path)) {
            Logger::CSL("ISFP-xLink:CSL:CSL根目录不存在\n");
            SaveCSLConfig(cfg);
            return;
        }

        json aircraft_map;
        try {
            for (const auto& folder_entry : fs::directory_iterator(full_csl_path)) {
                if (!folder_entry.is_directory()) continue;

                std::string folder_name = folder_entry.path().filename().string();
                std::vector<std::string> obj_list;

                for (const auto& file_entry : fs::directory_iterator(folder_entry.path())) {
                    if (!file_entry.is_regular_file()) continue;

                    std::string ext = file_entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".obj") {
                        obj_list.push_back(file_entry.path().filename().string());
                    }
                }

                if (!obj_list.empty()) {
                    aircraft_map[folder_name] = obj_list;
                }
            }
        }
        catch (...) {
            Logger::CSL("ISFP-xLink:CSL:扫描模型目录失败\n");
            return;
        }

        std::string config_path = GetConfigFilePath();
        try {
            json j;
            j["csl_path"] = cfg.csl_path;
            j["aircraft_mapped"] = aircraft_map;
            std::ofstream file(config_path);
            file << std::setw(4) << j << std::endl;
            Logger::CSL("ISFP-xLink:CSL:模型配置已生成\n");
        }
        catch (...) {
            Logger::CSL("ISFP-xLink:CSL:配置写入失败\n");
        }
    }

    // ==================== CSLAircraft Class ====================
    CSLAircraft::CSLAircraft() {
        memset(&draw_info_, 0, sizeof(draw_info_));
        draw_info_.structSize = sizeof(XPLMDrawInfo_t);
        draw_info_.x = 0;
        draw_info_.y = 0;
        draw_info_.z = -10000;
        terrain_probe_ = XPLMCreateProbe(xplm_ProbeY);
    }

    CSLAircraft::~CSLAircraft() {
        Destroy();
    }

    // Load OBJ model and create graphics instance
    bool CSLAircraft::LoadModel(const char* objFilePath) {
        Destroy();

        Logger::CSL(("ISFP-xLink:CSL:加载模型: " + std::string(objFilePath) + "\n").c_str());
        
        obj_ = XPLMLoadObject(objFilePath);
        if (!obj_) {
            Logger::CSL("ISFP-xLink:CSL:模型加载失败\n");
            return false;
        }

        instance_ = XPLMCreateInstance(obj_, NULL);
        if (!instance_) {
            XPLMUnloadObject(obj_);
            obj_ = nullptr;
            Logger::CSL("ISFP-xLink:CSL:实例创建失败\n");
            return false;
        }

        Logger::CSL("ISFP-xLink:CSL:模型加载成功\n");
        return instance_ != nullptr;
    }

    // Update aircraft position using world-to-local conversion
    void CSLAircraft::UpdatePosition(const FlightData& data) {
        if (!instance_ || !data.valid) return;

        double altitude_m = data.altitude_msl * 0.3048;
        double local_x, local_y, local_z;
        XPLMWorldToLocal(data.latitude, data.longitude, altitude_m, &local_x, &local_y, &local_z);

        // Probe terrain to check if aircraft is near ground
        if (terrain_probe_) {
            // Get probe position at same lat/lon (use MSL 0 for probe x/z)
            double probe_x, probe_y0, probe_z;
            XPLMWorldToLocal(data.latitude, data.longitude, 0.0, &probe_x, &probe_y0, &probe_z);
            XPLMProbeInfo_t pi;
            pi.structSize = sizeof(pi);
            if (XPLMProbeTerrainXYZ(terrain_probe_, (float)probe_x, 5000.0f, (float)probe_z, &pi) == xplm_ProbeHitTerrain) {
                double agl_m = local_y - pi.locationY;
                if (fabs(agl_m) < 30.48) { // within 100 feet
                    local_y = (double)pi.locationY + 1.0; // snap to 1m above ground
                }
            }
        }

        draw_info_.x = (float)local_x;
        draw_info_.y = (float)local_y;
        draw_info_.z = (float)local_z;

        draw_info_.pitch = static_cast<float>(data.pitch);
        draw_info_.heading = static_cast<float>(data.heading);
        draw_info_.roll = static_cast<float>(data.bank);

        XPLMInstanceSetPosition(instance_, &draw_info_, NULL);
    }

    // Hide aircraft by moving it far away
    void CSLAircraft::Hide() {
        if (!instance_) return;
        draw_info_.x = 0.0f;
        draw_info_.y = 0.0f;
        draw_info_.z = -10000.0f;
        XPLMInstanceSetPosition(instance_, &draw_info_, NULL);
    }

    // Destroy instance and unload model
    void CSLAircraft::Destroy() {
        if (terrain_probe_) {
            XPLMDestroyProbe(terrain_probe_);
            terrain_probe_ = nullptr;
        }
        if (instance_) {
            XPLMDestroyInstance(instance_);
            instance_ = nullptr;
        }
        if (obj_) {
            XPLMUnloadObject(obj_);
            obj_ = nullptr;
        }
    }

    // ==================== CSLManager Class ====================
    CSLManager::CSLManager() = default;
    CSLManager::~CSLManager() {
        Shutdown();
    }

    // Initialize CSL manager and load config
    bool CSLManager::Initialize() {
        if (initialized_) return true;
        
        initialized_ = true;
        ValidateAndUpdateCSLConfig();
        Logger::CSL("ISFP-xLink:CSL:管理器初始化完成\n");
        return true;
    }

    // Start CSL rendering
    bool CSLManager::Start() {
        if (!initialized_ || running_) return false;
        
        running_ = true;
        Logger::CSL("ISFP-xLink:CSL:管理器已启动\n");
        return true;
    }

    // Stop CSL rendering and hide all aircraft
    void CSLManager::Stop() {
        if (!initialized_ || !running_) return;
        
        running_ = false;
        for (auto* ac : aircraft_list_) {
            if (ac) ac->Hide();
        }
        Logger::CSL("ISFP-xLink:CSL:管理器已停止\n");
    }

    // Destroy all aircraft instances
    void CSLManager::DestroyAllAircraft() {
        for (auto* ac : aircraft_list_) {
            if (ac) delete ac;
        }
        aircraft_list_.clear();
        Logger::CSL("ISFP-xLink:CSL:所有飞机模型已销毁\n");
    }

    // Full shutdown of CSL system
    void CSLManager::Shutdown() {
        Stop();
        DestroyAllAircraft();
        initialized_ = false;
    }

    // Create multiple aircraft models from path list
    void CSLManager::CreateAircraftBatch(const std::vector<std::string>& model_paths) {
        if (!initialized_) return;
        DestroyAllAircraft();
        for (const auto& path : model_paths) {
            CSLAircraft* ac = new CSLAircraft();
            if (ac->LoadModel(path.c_str())) {
                aircraft_list_.push_back(ac);
            } else {
                delete ac;
            }
        }
    }

    // Batch update all aircraft positions
    void CSLManager::UpdateAllAircraft(const std::vector<FlightData>& flight_list) {
        if (!initialized_ || !running_) return;
        
        size_t count = std::min<size_t>(aircraft_list_.size(), flight_list.size());
        for (size_t i = 0; i < count; i++) {
            aircraft_list_[i]->UpdatePosition(flight_list[i]);
        }
    }

    // Update single aircraft position
    void CSLManager::UpdateAircraft(const FlightData* flight_data) {
        if (!initialized_ || !running_ || !flight_data || !flight_data->valid) return;
        if (!aircraft_list_.empty()) {
            aircraft_list_[0]->UpdatePosition(*flight_data);
        }
    }

    // Create aircraft by matching model name and airline code (folder -> OBJ lookup)
    CSLAircraft* CSLManager::CreateAircraftByModelName(const std::string& model_name, const std::string& airline_code, const std::string& aircraft_family)
    {
        if (!initialized_ || model_name.empty()) return nullptr;

        std::string config_path = GetConfigFilePath();
        std::ifstream file(config_path);
        if (!file.is_open()) return nullptr;

        json cfg_json = json::parse(file, nullptr, false);
        file.close();
        if (cfg_json.is_discarded()) return nullptr;

        std::string csl_path = cfg_json["csl_path"].get<std::string>();
        std::replace(csl_path.begin(), csl_path.end(), '\\', '/');
        json aircraft_map = cfg_json["aircraft_mapped"];

        std::string final_path;
        bool found = false;

        // Build relative path from X-Plane root (XPLMLoadObject resolves relative paths)
        auto build_path = [&](const std::string& folder, const std::string& obj_file) -> std::string {
            return csl_path + "/" + folder + "/" + obj_file;
            // e.g. "Resources/plugins/CSL/A319/A319_AAF.obj"
        };

        // First: find folder matching model name (e.g. "B738")
        for (auto& item : aircraft_map.items()) {
            std::string folder = item.key();
            if (folder.find(model_name) != std::string::npos) {
                // Folder found — pick obj: prefer airline match (e.g. "CES"), fallback to first
                for (auto& obj : item.value()) {
                    std::string obj_file = obj.get<std::string>();
                    if (!airline_code.empty() && obj_file.find(airline_code) != std::string::npos) {
                        final_path = build_path(folder, obj_file);
                        found = true;
                        break;
                    }
                    if (final_path.empty())
                        final_path = build_path(folder, obj_file);
                }
                if (!found && !final_path.empty())
                    found = true;
                break; // matched folder, done regardless
            }
        }

        // Not found → directly load A319 as fallback
        if (!found) {
            for (auto& item : aircraft_map.items()) {
                std::string folder = item.key();
                if (folder.find("A319") != std::string::npos) {
                    for (auto& obj : item.value()) {
                        std::string obj_file = obj.get<std::string>();
                        final_path = build_path(folder, obj_file);
                        found = true;
                        break;
                    }
                    if (found) break;
                }
            }
        }

        // No matching aircraft found after all four passes
        if (!found) {
            Logger::CSL(("ISFP-xLink:CSL:No matching aircraft found for model=" + model_name +
                (airline_code.empty() ? "" : " airline=" + airline_code) +
                (aircraft_family.empty() ? "" : " family=" + aircraft_family) + "\n").c_str());
        }

        CSLAircraft* ac = new CSLAircraft();
        if (ac->LoadModel(final_path.c_str())) {
            aircraft_list_.push_back(ac);

            // Log spawned aircraft model and path if logging enabled
            if (g_csl_log_enabled) {
                Logger::CSL(("ISFP-xLink:CSL_LOG:Spawn aircraft -> obj: " + final_path +
                    " | model: " + model_name +
                    (airline_code.empty() ? "" : " | airline: " + airline_code) +
                    (aircraft_family.empty() ? "" : " | family: " + aircraft_family) + "\n").c_str());
            }

            return ac;
        }

        delete ac;
        return nullptr;
    }

    // Get aircraft instance by index
    CSLAircraft* CSLManager::GetAircraft(size_t index) {
        if (index >= aircraft_list_.size()) return nullptr;
        return aircraft_list_[index];
    }

    // Remove and destroy specific aircraft
    void CSLManager::RemoveAircraft(CSLAircraft* ac) {
        if (!ac) return;
        auto it = std::find(aircraft_list_.begin(), aircraft_list_.end(), ac);
        if (it != aircraft_list_.end()) {
            delete ac;
            aircraft_list_.erase(it);
            Logger::CSL("ISFP-xLink:CSL:飞机已移除\n");
        }
    }

} // namespace ISFP
