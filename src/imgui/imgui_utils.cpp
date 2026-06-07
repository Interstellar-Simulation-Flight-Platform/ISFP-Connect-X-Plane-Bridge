#include "imgui/imgui_utils.h"

namespace ISFP {

void CenterText(const char* text, float region_width) {
    if (region_width < 0.0f)
        region_width = ImGui::GetContentRegionAvail().x;
    float tw = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (region_width - tw) * 0.5f);
    ImGui::Text("%s", text);
}

void ColoredText(float r, float g, float b, const char* text) {
    ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%s", text);
}

void ColoredText(const ImVec4& color, const char* text) {
    ImGui::TextColored(color, "%s", text);
}

bool LinkButton(const char* label) {
    return ImGui::SmallButton(label);
}

void SectionHeader(const char* title) {
    ImGui::Separator();
    ImGui::Text("%s", title);
    ImGui::Dummy(ImVec2(0, 4));
}

void SubHeader(const char* title) {
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Text("  %s", title);
}

bool FullWidthButton(const char* label, float height) {
    float w = ImGui::GetContentRegionAvail().x;
    return ImGui::Button(label, ImVec2(w, height));
}

bool ToggleButton(const char* label, bool active, float width, float height) {
    if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.30f, 1.0f));
    bool clicked = (width > 0)
        ? ImGui::Button(label, ImVec2(width, height))
        : ImGui::Button(label, ImVec2(0, height));
    if (active) ImGui::PopStyleColor();
    return clicked;
}

void InfoBox(const char* content, const ImVec2& size) {
    ImGui::BeginChild("##infobox", size, ImGuiChildFlags_Borders);
    ImGui::TextWrapped("%s", content);
    ImGui::EndChild();
}

void DrawTooltip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();
    }
}

void BeginDisabled() {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}

void EndDisabled() {
    ImGui::PopStyleVar();
}

bool CenteredMultiLineButton(const char* label, float width, float height,
                              float bg_r, float bg_g, float bg_b,
                              float txt_r, float txt_g, float txt_b) {
    ImVec2 bb_min = ImGui::GetCursorScreenPos();
    ImVec2 bb_max = ImVec2(bb_min.x + width, bb_min.y + height);
    bool clicked = ImGui::InvisibleButton(label, ImVec2(width, height));
    ImU32 col = clicked ? IM_COL32((int)(255*0.8f*bg_r), (int)(255*0.8f*bg_g), (int)(255*0.8f*bg_b), 200)
              : ImGui::IsItemHovered() ? IM_COL32((int)(255*1.2f*bg_r), (int)(255*1.2f*bg_g), (int)(255*1.2f*bg_b), 200)
              : IM_COL32((int)(255*bg_r), (int)(255*bg_g), (int)(255*bg_b), 200);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(bb_min, bb_max, col, 6.0f);
    ImU32 tc = IM_COL32((int)(255*txt_r), (int)(255*txt_g), (int)(255*txt_b), 200);
    float tw = ImGui::CalcTextSize(label).x;
    float x = bb_min.x + (width - tw) * 0.5f;
    float y = bb_min.y + (height - ImGui::GetFontSize()) * 0.5f;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x, y), tc, label);
    return clicked;
}

void DrawRoundedRectFilled(const ImVec2& p_min, const ImVec2& p_max,
                           ImU32 color, float rounding) {
    ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, color, rounding);
}

} // namespace ISFP
