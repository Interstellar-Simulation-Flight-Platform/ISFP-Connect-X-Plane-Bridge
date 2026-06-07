#include <imgui.h>
#include "imgui_manager.h"
#include <nlohmann/json.hpp>
#include <XPLMUtilities.h>
#include <Windows.h>
#include "logger.h"

using json = nlohmann::json;

namespace ISFP {
void ImGuiManager::BuildAiracList() {
    airac_list_.clear();
    airac_built_ = true;

    // Helper: SYSTEMTIME → 64-bit FILETIME (100-ns intervals since 1601-01-01)
    auto st_to_ft = [](const SYSTEMTIME& st) -> ULARGE_INTEGER {
        FILETIME ft;
        SystemTimeToFileTime(&st, &ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        return uli;
    };

    // Base: AIRAC2605 = 2026-05-14
    SYSTEMTIME base_st;
    ZeroMemory(&base_st, sizeof(base_st));
    base_st.wYear = 2026;
    base_st.wMonth = 5;
    base_st.wDay = 14;
    ULARGE_INTEGER base_ft = st_to_ft(base_st);

    // Today
    SYSTEMTIME today_st;
    GetSystemTime(&today_st);
    ULARGE_INTEGER today_ft = st_to_ft(today_st);

    // One AIRAC cycle = 28 days = 28 * 24 * 60 * 60 * 10000000 100-ns units
    const ULONGLONG CYCLE_TICKS = (ULONGLONG)28 * 24 * 60 * 60 * 10000000;

    // Collect all cycles: (filetime_value, numeric_value)
    std::vector<std::pair<ULONGLONG, int>> cycles;

    // Backward 4 from base
    {
        int cy = 26, cc = 5;
        ULONGLONG ft = base_ft.QuadPart;
        for (int i = 0; i < 4; i++) {
            ft -= CYCLE_TICKS;
            cc--;
            if (cc < 1) { cc = 13; cy--; }
            cycles.push_back({ft, cy * 100 + cc});
        }
    }

    // Forward from base past today (include base itself)
    {
        int cy = 26, cc = 5;
        ULONGLONG ft = base_ft.QuadPart;
        cycles.push_back({ft, cy * 100 + cc});
        while (true) {
            ft += CYCLE_TICKS;
            cc++;
            if (cc > 13) { cc = 1; cy++; }
            if (ft > today_ft.QuadPart) break;
            cycles.push_back({ft, cy * 100 + cc});
        }
    }

    // Sort by filetime descending (newest first)
    std::sort(cycles.begin(), cycles.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // Build list with formatted labels using FILETIME→SYSTEMTIME conversion
    for (const auto& c : cycles) {
        FILETIME ft;
        ft.dwLowDateTime = (DWORD)(c.first & 0xFFFFFFFF);
        ft.dwHighDateTime = (DWORD)(c.first >> 32);
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);

        int y = c.second / 100;
        int cyc = c.second % 100;
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "AIRAC%02d%02d (%d.%d)", y, cyc, st.wMonth, st.wDay);
        airac_list_.push_back({std::string(lbl), c.second});
    }
    airac_index_ = 0; // default to newest
}

void ImGuiManager::RenderFlightInquiryPage() {
    ImGui::Text("%s", g_language == Language::CN ? "\xe8\x88\xaa\xe6\x83\x85\xe6\x9f\xa5\xe8\xaf\xa2" : "Flight Inquiry");
    ImGui::Separator();

    // --- Edge detection using Win32 APIs (before any ImGui widget consumes events) ---
    static bool s_prev_rclick = false, s_prev_ctrlc = false;
    // Reset on first call
    static int s_edge_version = 0;
    if (s_edge_version != reload_version_) {
        s_prev_rclick = false;
        s_prev_ctrlc = false;
        s_edge_version = reload_version_;
    }
    bool rclick_down = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    bool rclick_edge = rclick_down && !s_prev_rclick;
    s_prev_rclick = rclick_down;
    bool ctrl_down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool c_down = (GetAsyncKeyState(0x43) & 0x8000) != 0;
    bool ctrlc_edge = ctrl_down && c_down && !s_prev_ctrlc;
    s_prev_ctrlc = ctrl_down && c_down;

    static ImVec2 s_r_min, s_r_max, s_m_min, s_m_max, s_t_min, s_t_max;
    static double s_route_toast = 0.0, s_metar_toast = 0.0, s_taf_toast = 0.0;

    ImGui::Dummy(ImVec2(0, 10));

    // Check client connection status
    bool client_ok = g_network && g_network->IsClientConnected();
    if (!client_ok) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.4f, 0.2f, 1.0f));
        ImGui::TextWrapped("%s",
            g_language == Language::CN
                ? "[!] \xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf\xe6\x9c\xaa\xe8\xbf\x9e\xe6\x8e\xa5\xef\xbc\x8c\xe6\x97\xa0\xe6\xb3\x95\xe6\x9f\xa5\xe8\xaf\xa2\xe6\x95\xb0\xe6\x8d\xae"
                : "[!] Client not connected, cannot query data");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 10));
    }

    // === Route Query ===
    ImGui::Text("%s", g_language == Language::CN ? "\xe8\x88\xaa\xe8\xb7\xaf\xe6\x9f\xa5\xe8\xaf\xa2" : "Route Query");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("  %s", g_language == Language::CN ? "\xe8\xb5\xb7\xe9\xa3\x9e\xe6\x9c\xba\xe5\x9c\xba ICAO:" : "Departure ICAO:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(170.0f);
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 8.0f);
    ImGui::InputText("##dep", dep_buf_, sizeof(dep_buf_));
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 6));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("  %s", g_language == Language::CN ? "\xe5\x88\xb0\xe8\xbe\xbe\xe6\x9c\xba\xe5\x9c\xba ICAO:" : "Arrival ICAO:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(170.0f);
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 8.0f);
    ImGui::InputText("##arr", arr_buf_, sizeof(arr_buf_));
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 6));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("  %s", g_language == Language::CN ? "\xe5\xaf\xbc\xe8\x88\xaa\xe7\x89\x88\xe6\x9c\xac:" : "Nav Data:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(170.0f);
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 8.0f);
    if (!airac_list_.empty()) {
        std::vector<const char*> items;
        for (const auto& a : airac_list_) items.push_back(a.label.c_str());
        ImGui::Combo("##airac", &airac_index_, items.data(), (int)items.size());
    }
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 12));

    bool route_ready = client_ok && strlen(dep_buf_) > 0 && strlen(arr_buf_) > 0;
    if (!route_ready) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    if (ImGui::Button(g_language == Language::CN ? "\xe6\x9f\xa5\xe8\xaf\xa2\xe8\x88\xaa\xe8\xb7\xaf" : "Query Route", ImVec2(140, 30)) && route_ready) {
        if (strlen(dep_buf_) > 0 && strlen(arr_buf_) > 0) {
            json req;
            req["type"] = "Query:Route";
            req["dep"] = std::string(dep_buf_);
            req["arr"] = std::string(arr_buf_);
            if (airac_index_ >= 0 && airac_index_ < (int)airac_list_.size()) {
                req["airac"] = airac_list_[airac_index_].value;
            }
            std::string req_str = req.dump();
            g_network->SendJson(req_str);
            route_query_time_ = XPLMGetElapsedTime();
            Logger::ImGui(("ISFP-xLink:Inquiry:Route query sent: " + req_str + "\n").c_str());
        }
    }
    if (!route_ready) {
        ImGui::PopStyleVar();
    }

    // Display route query result or pending state
    {
        std::lock_guard<std::mutex> lock(g_efb_route_mutex);
        bool has_result = !g_efb_route_result.empty();
        bool pending = (route_query_time_ > 0.0 && !has_result);

        if (pending) {
            ImGui::Dummy(ImVec2(0, 8));
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
                g_language == Language::CN ? "\xe6\xad\xa3\xe5\x9c\xa8\xe6\x9f\xa5\xe8\xaf\xa2\xe4\xb8\xad..." : "Querying...");
        } else if (has_result) {
            ImGui::Dummy(ImVec2(0, 8));
            float avail_h = ImGui::GetContentRegionAvail().y;
            ImGui::BeginChild("##route_display", ImVec2(-FLT_MIN, (std::max)(avail_h * 0.5f, 60.0f)), ImGuiChildFlags_Border);
            ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(g_efb_route_result.c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            s_r_min = ImGui::GetItemRectMin();
            s_r_max = ImGui::GetItemRectMax();
            // Extract route body: text between SID and STAR
            auto route_body = [](const std::string& r) -> std::string {
                size_t sid = r.find("SID");
                size_t star = r.find("STAR");
                if (sid != std::string::npos && star != std::string::npos && star > sid) {
                    size_t start = sid + 3; // after "SID"
                    while (start < r.size() && r[start] == ' ') start++;
                    size_t end = star;
                    while (end > start && r[end-1] == ' ') end--;
                    if (end > start) return r.substr(start, end - start);
                }
                return r;
            };
            std::string route_copy = route_body(g_efb_route_result);
            // Inline copy button (copies route body)
            ImGui::SameLine();
            if (ImGui::SmallButton(g_language == Language::CN ? "\xe5\xa4\x8d\xe5\x88\xb6" : "Copy")) {
                ImGui::SetClipboardText(route_copy.c_str());
                s_route_toast = ImGui::GetTime() + 2.0;
            }
            // Right-click: copies route body (SID...STAR)
            bool rc_in_bounds = ImGui::GetIO().MousePos.x >= s_r_min.x && ImGui::GetIO().MousePos.x <= s_r_max.x &&
                                ImGui::GetIO().MousePos.y >= s_r_min.y && ImGui::GetIO().MousePos.y <= s_r_max.y;
            if (rclick_edge && rc_in_bounds) {
                ImGui::SetClipboardText(route_copy.c_str());
                s_route_toast = ImGui::GetTime() + 2.0;
            }
            // Toast notification
            if (s_route_toast > ImGui::GetTime()) {
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 40), ImGuiCond_Always, ImVec2(0.5f, 0));
                ImGui::SetNextWindowBgAlpha(0.85f);
                ImGui::Begin("##route_toast", nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
                ImGui::TextColored(ImVec4(0.2f,0.9f,0.3f,1), "%s",
                    g_language == Language::CN ? "\xe5\xb7\xb2\xe5\xa4\x8d\xe5\x88\xb6\xe5\x88\xb0\xe5\x89\xaa\xe8\xb4\xb4\xe6\x9d\xbf!" : "Copied to clipboard!");
                ImGui::End();
            }
        }
    }

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    // === Weather Query ===
    ImGui::Text("%s", g_language == Language::CN ? "\xe6\xb0\x94\xe8\xb1\xa1\xe6\x9f\xa5\xe8\xaf\xa2" : "Weather Query");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::Text("  %s", g_language == Language::CN ? "\xe6\x9c\xba\xe5\x9c\xba ICAO:" : "Airport ICAO:");
    ImGui::SameLine();
    ImGui::PushItemWidth(120);
    ImGui::InputText("##wx", wx_buf_, sizeof(wx_buf_));
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 12));

    bool wx_ready = client_ok && strlen(wx_buf_) > 0;
    if (!wx_ready) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    if (ImGui::Button(g_language == Language::CN ? "\xe6\x9f\xa5\xe8\xaf\xa2\xe5\xa4\xa9\xe6\xb0\x94" : "Query Weather", ImVec2(140, 30)) && wx_ready) {
        if (strlen(wx_buf_) > 0) {
            json req;
            req["type"] = "Query:Weather";
            req["apt"] = std::string(wx_buf_);
            std::string req_str = req.dump();
            g_network->SendJson(req_str);
            weather_query_time_ = XPLMGetElapsedTime();
            Logger::ImGui(("ISFP-xLink:Inquiry:Weather query sent: " + req_str + "\n").c_str());
        }
    }
    if (!wx_ready) {
        ImGui::PopStyleVar();
    }

    // Display weather query result or pending state (METAR + TAF split, copyable)
    {
        std::lock_guard<std::mutex> lock(g_efb_weather_mutex);
        bool has_result = !g_efb_weather_result.empty();
        bool pending = (weather_query_time_ > 0.0 && !has_result);

        if (pending) {
            ImGui::Dummy(ImVec2(0, 8));
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
                g_language == Language::CN ? "\xe6\xad\xa3\xe5\x9c\xa8\xe6\x9f\xa5\xe8\xaf\xa2\xe4\xb8\xad..." : "Querying...");
        } else if (has_result) {
            ImGui::Dummy(ImVec2(0, 8));

            // Parse JSON data object with metar/taf fields
            std::string metar_text, taf_text;
            try {
                json wx_json = json::parse(g_efb_weather_result);
                if (wx_json.contains("metar")) {
                    metar_text = wx_json["metar"].get<std::string>();
                }
                if (wx_json.contains("taf")) {
                    taf_text = wx_json["taf"].get<std::string>();
                }
            } catch (...) {
                metar_text = g_efb_weather_result;
            }

            float avail_h = ImGui::GetContentRegionAvail().y;
            float half_h = (std::max)(avail_h * 0.45f, 60.0f);
            float box_w = ImGui::GetContentRegionAvail().x * 0.95f;
            float center_offset = (ImGui::GetContentRegionAvail().x - box_w) * 0.5f;

            // --- METAR ---
            ImGui::SetCursorPosX(center_offset);
            ImGui::BeginChild("##metar_display", ImVec2(box_w, half_h), ImGuiChildFlags_Border);
            ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(metar_text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndChild();
            s_m_min = ImGui::GetItemRectMin();
            s_m_max = ImGui::GetItemRectMax();
            // Toast notification
            if (s_metar_toast > ImGui::GetTime()) {
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 60), ImGuiCond_Always, ImVec2(0.5f, 0));
                ImGui::SetNextWindowBgAlpha(0.85f);
                ImGui::Begin("##metar_toast", nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
                ImGui::TextColored(ImVec4(0.2f,0.9f,0.3f,1), "%s",
                    g_language == Language::CN ? "\xe5\xb7\xb2\xe5\xa4\x8d\xe5\x88\xb6\xe5\x88\xb0\xe5\x89\xaa\xe8\xb4\xb4\xe6\x9d\xbf!" : "Copied to clipboard!");
                ImGui::End();
            }

            ImGui::Dummy(ImVec2(0, 6));

            // --- TAF ---
            ImGui::SetCursorPosX(center_offset);
            ImGui::BeginChild("##taf_display", ImVec2(box_w, half_h), ImGuiChildFlags_Border);
            ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(taf_text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndChild();
            s_t_min = ImGui::GetItemRectMin();
            s_t_max = ImGui::GetItemRectMax();
            // Toast notification
            if (s_taf_toast > ImGui::GetTime()) {
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 80), ImGuiCond_Always, ImVec2(0.5f, 0));
                ImGui::SetNextWindowBgAlpha(0.85f);
                ImGui::Begin("##taf_toast", nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
                ImGui::TextColored(ImVec4(0.2f,0.9f,0.3f,1), "%s",
                    g_language == Language::CN ? "\xe5\xb7\xb2\xe5\xa4\x8d\xe5\x88\xb6\xe5\x88\xb0\xe5\x89\xaa\xe8\xb4\xb4\xe6\x9d\xbf!" : "Copied to clipboard!");
                ImGui::End();
            }
        }
    }
}
} // namespace ISFP
