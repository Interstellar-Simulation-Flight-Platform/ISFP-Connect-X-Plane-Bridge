#ifndef IMGUI_UTILS_H
#define IMGUI_UTILS_H

#include <string>
#include <imgui.h>

namespace ISFP {

// ==================== UI Utility Functions ====================

// Helper: centered text label
void CenterText(const char* text, float region_width = -1.0f);

// Helper: draw a colored text
void ColoredText(float r, float g, float b, const char* text);
void ColoredText(const ImVec4& color, const char* text);

// Helper: create a small clickable link-style button
bool LinkButton(const char* label);

// Helper: create a standardized section separator with title
void SectionHeader(const char* title);

// Helper: create a full-width button with custom height
bool FullWidthButton(const char* label, float height = 28.0f);

// Helper: create a toggle button that shows active/inactive state
bool ToggleButton(const char* label, bool active, float width = 0.0f, float height = 28.0f);

// Helper: draw a bordered info box with content
void InfoBox(const char* content, const ImVec2& size = ImVec2(0, 0));

// Helper: draw tooltip on hovered item
void DrawTooltip(const char* text);

// Helper: push/pop disabled visual style
void BeginDisabled();
void EndDisabled();

// Helper: create a centered multi-line button with custom draw
bool CenteredMultiLineButton(const char* label, float width, float height,
                             float bg_r, float bg_g, float bg_b,
                             float txt_r, float txt_g, float txt_b);

// Helper: draw rounded rect filled (custom draw)
void DrawRoundedRectFilled(const ImVec2& p_min, const ImVec2& p_max,
                           ImU32 color, float rounding = 4.0f);

} // namespace ISFP

#endif // IMGUI_UTILS_H

