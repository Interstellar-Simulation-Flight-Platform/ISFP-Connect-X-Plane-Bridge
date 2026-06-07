#include <imgui.h>
#include "imgui_internal.h"
#include "imgui_manager.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include "logger.h"

using json = nlohmann::json;

namespace ISFP {

// ==================== CSL Models Page ====================
void ImGuiManager::RenderCSLModelsPage() {
    ImGui::Text("%s", GetText(StringID::CSLMapping_Title));
    ImGui::Separator();

    // Load current CSL config
    CSLConfig cur_cfg;
    bool cfg_ok = LoadCSLConfig(cur_cfg);

    // Initialize path buffer
    static bool s_path_inited = false;
    if (!s_path_inited) {
        strncpy_s(csl_path_buf_, cfg_ok ? cur_cfg.csl_path.c_str() : "", sizeof(csl_path_buf_) - 1);
        s_path_inited = true;
    }

    // CSL path input + browse button
    ImGui::Text("CSL %s", g_language == Language::CN ? "\xe8\xb7\xaf\xe5\xbe\x84" : "Path");
    ImGui::Dummy(ImVec2(0, 4));

    float btn_w = 110.0f;
    float prefix_w = ImGui::CalcTextSize("{GameRoot}\\ ").x;
    float input_w = ImGui::GetContentRegionAvail().x - btn_w - ImGui::GetStyle().ItemSpacing.x - prefix_w;
    ImGui::PushItemWidth(input_w);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1.0f), "{GameRoot}\\");
    ImGui::SameLine();
    ImGui::InputText("##cslpath", csl_path_buf_, sizeof(csl_path_buf_));
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button(g_language == Language::CN ? "\xe6\xb5\x8f\xe8\xa7\x88" : "Browse", ImVec2(btn_w, 0))) {
        char xp_root[512] = {};
        XPLMGetSystemPath(xp_root);
        csl_browser_path_ = std::string(xp_root) + "Resources\\plugins";
        csl_browser_back_.clear();
        csl_browser_forward_.clear();
        csl_browser_selected_ = -1;
        csl_browser_open_ = true;
    }

    if (csl_browser_open_) {
        RenderCSLFileBrowser();
    }

    ImGui::Dummy(ImVec2(0, 6));

    if (ImGui::Button(g_language == Language::CN ? "\xe5\xba\x94\xe7\x94\xa8\xe8\xb7\xaf\xe5\xbe\x84\xe5\xb9\xb6\xe6\x89\xab\xe6\x8f\x8f" : "Apply && Scan", ImVec2(200, 28))) {
        if (strlen(csl_path_buf_) > 0) {
            CSLConfig new_cfg;
            new_cfg.csl_path = csl_path_buf_;
            SaveCSLConfig(new_cfg);
            ValidateAndUpdateCSLConfig();
            Logger::ImGui(("ISFP-xLink:CSL:Path changed to: " + std::string(csl_path_buf_) + "\n").c_str());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(g_language == Language::CN ? "\xe9\x87\x8d\xe6\x96\xb0\xe6\x89\xab\xe6\x8f\x8f" : "Rescan", ImVec2(120, 28))) {
        ValidateAndUpdateCSLConfig();
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Separator();

    std::ifstream cfg_file(GetConfigFilePath());
    if (!cfg_file.is_open()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", GetText(StringID::NoMappedAircraft));
        return;
    }

    json cfg_json = json::parse(cfg_file, nullptr, false);
    cfg_file.close();
    if (cfg_json.is_discarded() || !cfg_json.contains("aircraft_mapped")) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", GetText(StringID::NoMappedAircraft));
        return;
    }

    json aircraft_map = cfg_json["aircraft_mapped"];
    if (aircraft_map.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", GetText(StringID::NoMappedAircraft));
        return;
    }

    ImGui::Text(GetText(StringID::MappedAircraftCount), aircraft_map.size());
    ImGui::Dummy(ImVec2(0, 5));

    for (auto& item : aircraft_map.items()) {
        std::string folder_name = item.key();
        auto& obj_files = item.value();
        int obj_count = (int)obj_files.size();
        char node_label[128];
        snprintf(node_label, sizeof(node_label), "%s  (%d .obj)", folder_name.c_str(), obj_count);
        if (ImGui::TreeNode(node_label)) {
            for (auto& obj_file : obj_files) {
                std::string obj_name = obj_file.get<std::string>();
                ImGui::BulletText("%s", obj_name.c_str());
            }
            ImGui::TreePop();
        }
    }
}

// ==================== CSL File Browser ====================
void ImGuiManager::RenderCSLFileBrowser() {
    // File browser modal for selecting CSL directory
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(g_language == Language::CN ? "\xe9\x80\x89\xe6\x8b\xa9 CSL \xe7\x9b\xae\xe5\xbd\x95" : "Select CSL Directory",
                      &csl_browser_open_)) {
        ImGui::End();
        return;
    }

    // Navigation: back/forward buttons + current path
    if (ImGui::Button(g_language == Language::CN ? "\xe2\x86\x90 \xe8\xbf\x94\xe5\x9b\x9e" : "\xe2\x86\x90 Back") && !csl_browser_back_.empty()) {
        csl_browser_forward_.push_back(csl_browser_path_);
        csl_browser_path_ = csl_browser_back_.back();
        csl_browser_back_.pop_back();
        csl_browser_selected_ = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button(g_language == Language::CN ? "\xe2\x86\x92 \xe5\x89\x8d\xe8\xbf\x9b" : "\xe2\x86\x92 Forward") && !csl_browser_forward_.empty()) {
        csl_browser_back_.push_back(csl_browser_path_);
        csl_browser_path_ = csl_browser_forward_.back();
        csl_browser_forward_.pop_back();
        csl_browser_selected_ = -1;
    }
    ImGui::SameLine();
    ImGui::Text("  %s", csl_browser_path_.c_str());

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    // List directories
    std::vector<std::string> dirs;
    try {
        for (auto& entry : std::filesystem::directory_iterator(csl_browser_path_)) {
            if (entry.is_directory()) {
                dirs.push_back(entry.path().filename().string());
            }
        }
    } catch (...) {}

    std::sort(dirs.begin(), dirs.end(),
        [](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });

    ImGui::BeginChild("##csl_dir_list", ImVec2(0, ImGui::GetContentRegionAvail().y - 40), ImGuiChildFlags_Borders);

    for (int i = 0; i < (int)dirs.size(); i++) {
        bool is_selected = (i == csl_browser_selected_);
        if (ImGui::Selectable(dirs[i].c_str(), is_selected)) {
            csl_browser_selected_ = i;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            csl_browser_back_.push_back(csl_browser_path_);
            csl_browser_path_ = (std::filesystem::path(csl_browser_path_) / dirs[i]).string();
            csl_browser_forward_.clear();
            csl_browser_selected_ = -1;
        }
    }
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, 4));

    // Select button
    if (ImGui::Button(g_language == Language::CN ? "\xe9\x80\x89\xe6\x8b\xa9\xe6\xad\xa4\xe7\x9b\xae\xe5\xbd\x95" : "Select This Directory", ImVec2(200, 28))) {
        strncpy_s(csl_path_buf_, csl_browser_path_.c_str(), sizeof(csl_path_buf_) - 1);
        csl_browser_open_ = false;
    }

    ImGui::End();
}

} // namespace ISFP
