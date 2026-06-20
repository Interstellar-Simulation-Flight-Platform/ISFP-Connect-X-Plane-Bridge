#include <imgui.h>
#include "imgui_manager.h"
#include "logger.h"

namespace ISFP {

void ImGuiManager::RenderPlayersPage() {
    ImGui::Text("%s", GetText(StringID::OnlinePlayersATC_Title));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 5));

    bool client_connected = g_network && g_network->IsClientConnected();

    std::vector<FlightData> players;
    {
        std::lock_guard<std::mutex> lock(g_player_mutex);
        players = g_valid_players;
    }

    // Left: Players / Right: ATC (split into two columns)
    float avail_w = ImGui::GetContentRegionAvail().x;
    float half_w = (avail_w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    // === Left column: Players ===
    ImGui::BeginChild("##players_col", ImVec2(half_w, 0), true);
    ImGui::Text("%s", g_language == Language::CN ? "\xE7\x8E\xA9\xE5\xAE\xB6" : "Players");
    ImGui::Separator();

    if (!client_connected) {
        ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.2f, 1.0f), "%s",
            g_language == Language::CN ? "\xE5\xAE\xA2\xE6\x88\xB7\xE7\xAB\xAF\xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5" : "Client not connected");
    } else if (players.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
            g_language == Language::CN ? "\xE5\xBD\x93\xE5\x89\x8D\xE6\x97\xA0\xE7\x8E\xA9\xE5\xAE\xB6" : "No players");
    } else {
        for (const auto& p : players) {
            std::string label = p.callsign.empty() ? "Unknown" : p.callsign;
            if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("  %s: %s", GetText(StringID::Aircraft), p.aircraft.c_str());
                ImGui::Text("  %s: %.4f, %.4f", GetText(StringID::Latitude), p.latitude, p.longitude);
                ImGui::Text("  %s: %.0f ft", GetText(StringID::Altitude), p.altitude_msl);
                ImGui::Text("  %s: %.1f\xB0  %s: %.0f kts",
                    GetText(StringID::Heading), p.heading,
                    GetText(StringID::Speed), p.groundspeed);
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === Right column: ATC ===
    ImGui::BeginChild("##atc_col", ImVec2(0, 0), true);
    ImGui::Text("%s", g_language == Language::CN ? "\xE7\xAE\xA1\xE5\x88\xB6" : "ATC");
    ImGui::Separator();

    if (!client_connected) {
        ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.2f, 1.0f), "%s",
            g_language == Language::CN ? "\xE5\xAE\xA2\xE6\x88\xB7\xE7\xAB\xAF\xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5" : "Client not connected");
    } else {
        std::vector<ATCData> atc_list;
        {
            std::lock_guard<std::mutex> lock(g_atc_mutex);
            atc_list = g_atc_list;
        }

        if (atc_list.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
                g_language == Language::CN ? "\xE5\xBD\x93\xE5\x89\x8D\xE6\x97\xA0\xE7\xAE\xA1\xE5\x88\xB6" : "No ATC");
        } else {
            // 按席位排序优先级: DEL, GND, APN, TWR, APP, CTR, PRC_FSS, ATIS, OBS(末位)
            static const char* s_order[] = {
                "DEL", "GND", "APN", "TWR", "APP", "CTR", "PRC_FSS", "FSS", "ATIS", "OBS"
            };
            static const int s_order_count = 10;
            bool has_obs = false;

            // 遍历优先级顺序，匹配并显示
            for (int i = 0; i < s_order_count; ++i) {
                std::string target_type = s_order[i];
                bool found = false;
                for (const auto& atc : atc_list) {
                    // 类型匹配：完全一致或包含关系
                    bool type_match = false;
                    if (atc.controller_type == target_type) {
                        type_match = true;
                    } else if (target_type == "PRC_FSS" &&
                               (atc.controller_type.find("FSS") != std::string::npos ||
                                atc.controller_type.find("PRC") != std::string::npos)) {
                        type_match = true;
                    } else if (target_type == "OBS" &&
                               (atc.controller_type == "OBS" || atc.controller_type == "OBSERVER")) {
                        type_match = true;
                    }
                    if (!type_match) continue;

                    found = true;
                    if (target_type == "OBS") {
                        // OBS 格式: 呼号_OBS，不显示频率
                        has_obs = true;
                        // 收集所有OBS，末尾一起显示
                        continue;
                    }
                    std::string freq = atc.frequency.empty() ? "---" : atc.frequency;
                    std::string line = atc.callsign + "(" + freq + ")";
                    ImGui::Text("%s", line.c_str());
                }
            }
            // 末尾显示所有 OBS
            if (has_obs) {
                for (const auto& atc : atc_list) {
                    if (atc.controller_type == "OBS" || atc.controller_type == "OBSERVER") {
                        std::string line = atc.callsign + "_OBS";
                        ImGui::Text("%s", line.c_str());
                    }
                }
            }
        }
    }
    ImGui::EndChild();
}

} // namespace ISFP
