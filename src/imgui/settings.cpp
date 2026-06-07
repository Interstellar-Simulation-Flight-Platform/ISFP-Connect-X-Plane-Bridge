#include <imgui.h>
#include "imgui_internal.h"
#include "imgui_impl_opengl2.h"
#include "imgui_manager.h"
#include "utils.h"
#include <windows.h>
#include <GL/gl.h>
#include <wincodec.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMUtilities.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <cctype>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <filesystem>
#include "logger.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

struct ThemePreset {
    const char* name_cn;
    const char* name_en;
    float window[3];
    float child[3];
    float button[3];
    float btn_hover[3];
    float border[3];
    float btn_active[3];
    float btn_clicked[3];
    float frame[3];
    float frame_active[3];
    float popup[3];
    float dropdown_frame[3];
    float dropdown_border[3];
    float dropdown_active[3];
    float table_header_text[3];
    float table_header_bg[3];
    float table_content_text[3];
    float table_content_bg[3];
    float slider[3];
    float slider_track[3];
    float slider_track_hover[3];
    float slider_active[3];
    float text[3];
    float toast_text[3];
    float check[3];
};

extern const ThemePreset kPresets[5] = {
    { "\xe6\xb5\x85\xe8\x89\xb2\xe5\xa4\xa9\xe7\xa9\xba", "Light Sky", {0.95f,0.97f,0.99f}, {0.90f,0.93f,0.96f}, {0.12f,0.48f,0.85f}, {0.22f,0.58f,0.92f}, {0.70f,0.74f,0.80f}, {0.15f,0.52f,0.88f}, {0.08f,0.38f,0.72f}, {0.85f,0.88f,0.92f}, {0.78f,0.82f,0.88f}, {0.88f,0.91f,0.95f}, {0.82f,0.85f,0.90f}, {0.68f,0.72f,0.78f}, {0.20f,0.52f,0.85f}, {0.08f,0.12f,0.18f}, {0.82f,0.86f,0.92f}, {0.12f,0.16f,0.22f}, {0.92f,0.94f,0.97f}, {0.12f,0.48f,0.85f}, {0.72f,0.76f,0.82f}, {0.68f,0.72f,0.78f}, {0.22f,0.58f,0.92f}, {0.75f,0.50f,0.15f}, {0.08f,0.12f,0.18f}, {0.12f,0.48f,0.85f} },
    { "\xe6\xb5\x85\xe8\x89\xb2\xe6\xb8\xa9\xe6\x9a\x96", "Light Warm", {0.97f,0.95f,0.91f}, {0.92f,0.89f,0.85f}, {0.72f,0.42f,0.22f}, {0.80f,0.52f,0.32f}, {0.74f,0.70f,0.65f}, {0.78f,0.48f,0.28f}, {0.62f,0.32f,0.15f}, {0.88f,0.85f,0.80f}, {0.82f,0.78f,0.73f}, {0.90f,0.88f,0.84f}, {0.85f,0.82f,0.77f}, {0.72f,0.68f,0.63f}, {0.75f,0.45f,0.28f}, {0.22f,0.16f,0.10f}, {0.85f,0.82f,0.77f}, {0.28f,0.20f,0.14f}, {0.94f,0.92f,0.88f}, {0.72f,0.42f,0.22f}, {0.76f,0.72f,0.67f}, {0.72f,0.68f,0.63f}, {0.82f,0.55f,0.35f}, {0.70f,0.40f,0.10f}, {0.22f,0.16f,0.10f}, {0.72f,0.42f,0.22f} },
    { "\xe6\xb7\xb1\xe8\x89\xb2\xe7\x9f\xb3\xe6\x9d\xbf", "Dark Slate", {0.07f,0.08f,0.11f}, {0.10f,0.12f,0.15f}, {0.18f,0.45f,0.78f}, {0.28f,0.55f,0.85f}, {0.20f,0.22f,0.28f}, {0.22f,0.50f,0.82f}, {0.12f,0.35f,0.65f}, {0.13f,0.15f,0.19f}, {0.17f,0.19f,0.23f}, {0.09f,0.10f,0.14f}, {0.15f,0.17f,0.21f}, {0.22f,0.24f,0.30f}, {0.22f,0.48f,0.80f}, {0.88f,0.90f,0.96f}, {0.10f,0.12f,0.16f}, {0.84f,0.87f,0.94f}, {0.06f,0.08f,0.10f}, {0.28f,0.55f,0.85f}, {0.16f,0.18f,0.24f}, {0.20f,0.22f,0.28f}, {0.35f,0.62f,0.92f}, {1.0f,0.70f,0.20f}, {0.88f,0.90f,0.96f}, {0.28f,0.55f,0.85f} },
    { "\xe6\xb7\xb1\xe8\x89\xb2\xe7\xb4\xab\xe7\xbd\x97\xe5\x85\xb0", "Dark Violet", {0.04f,0.03f,0.07f}, {0.07f,0.06f,0.12f}, {0.55f,0.25f,0.75f}, {0.62f,0.35f,0.82f}, {0.16f,0.14f,0.24f}, {0.58f,0.30f,0.78f}, {0.42f,0.18f,0.62f}, {0.10f,0.08f,0.16f}, {0.14f,0.12f,0.20f}, {0.06f,0.05f,0.10f}, {0.12f,0.10f,0.18f}, {0.18f,0.16f,0.26f}, {0.55f,0.28f,0.72f}, {0.85f,0.80f,0.92f}, {0.08f,0.07f,0.14f}, {0.82f,0.78f,0.90f}, {0.04f,0.03f,0.08f}, {0.58f,0.30f,0.78f}, {0.14f,0.12f,0.22f}, {0.18f,0.16f,0.26f}, {0.65f,0.40f,0.85f}, {1.0f,0.60f,0.80f}, {0.85f,0.80f,0.92f}, {0.60f,0.32f,0.80f} },
    { "\xe6\xb7\xb1\xe8\x89\xb2\xe6\xa3\xae\xe6\x9e\x97", "Dark Forest", {0.04f,0.07f,0.06f}, {0.07f,0.10f,0.09f}, {0.20f,0.58f,0.45f}, {0.30f,0.68f,0.55f}, {0.14f,0.18f,0.16f}, {0.25f,0.62f,0.50f}, {0.15f,0.45f,0.32f}, {0.10f,0.13f,0.11f}, {0.14f,0.17f,0.15f}, {0.06f,0.09f,0.08f}, {0.12f,0.15f,0.13f}, {0.16f,0.20f,0.18f}, {0.25f,0.55f,0.45f}, {0.82f,0.88f,0.85f}, {0.08f,0.12f,0.10f}, {0.78f,0.85f,0.82f}, {0.04f,0.07f,0.06f}, {0.30f,0.65f,0.52f}, {0.13f,0.16f,0.14f}, {0.17f,0.20f,0.18f}, {0.40f,0.75f,0.62f}, {1.0f,0.70f,0.20f}, {0.82f,0.88f,0.85f}, {0.25f,0.62f,0.50f} },
};

namespace ISFP {
void ImGuiManager::UnloadBGTexture() {
    if (bg_texture_id_) {
        glDeleteTextures(1, &bg_texture_id_);
        bg_texture_id_ = 0;
    }
    bg_tex_w_ = 0;
    bg_tex_h_ = 0;
    bg_texture_path_.clear();
}

void ImGuiManager::LoadBGTexture(const std::string& filepath) {
    UnloadBGTexture();

    if (filepath.empty() || !std::filesystem::exists(filepath))
        return;

    // Convert to wide char
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, nullptr, 0);
    std::wstring wpath(static_cast<size_t>(wlen) - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, &wpath[0], wlen);

    // Use WIC to load image (supports PNG, JPEG, BMP, TIFF, GIF, etc.)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool com_inited = SUCCEEDED(hr);

    IWICImagingFactory* factory = nullptr;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) { if (com_inited) CoUninitialize(); return; }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) { factory->Release(); if (com_inited) CoUninitialize(); return; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) { decoder->Release(); factory->Release(); if (com_inited) CoUninitialize(); return; }

    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    if (w == 0 || h == 0) { frame->Release(); decoder->Release(); factory->Release(); if (com_inited) CoUninitialize(); return; }
    if (w > 4096) w = 4096;
    if (h > 4096) h = 4096;

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr) && converter) {
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (SUCCEEDED(hr)) {
            UINT stride = w * 4;
            UINT buf_size = stride * h;
            std::vector<unsigned char> pixels(buf_size);
            hr = converter->CopyPixels(nullptr, stride, buf_size, pixels.data());
            if (SUCCEEDED(hr)) {
                // BGRA → RGBA for OpenGL
                for (size_t i = 0; i < buf_size; i += 4)
                    std::swap(pixels[i], pixels[i + 2]);

                GLuint tex = 0;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

                bg_texture_id_ = (unsigned int)tex;
                bg_tex_w_ = (int)w;
                bg_tex_h_ = (int)h;
                bg_texture_path_ = filepath;
            }
        }
        converter->Release();
    }

    frame->Release();
    decoder->Release();
    factory->Release();
    if (com_inited) CoUninitialize();
}

void ImGuiManager::CopyAndLoadBGImage(const std::string& src_path) {
    if (src_path.empty()) return;

    // Determine target path: <plugin_root>/Data/Texture/ui_background.png
    char xp_root[512] = {0};
    XPLMGetSystemPath(xp_root);
    std::string texture_dir = std::string(xp_root) + "Resources\\plugins\\ISFP_xLink\\Data\\Texture\\";
    std::string dst_path = texture_dir + "ui_background.png";

    // Create directory if needed
    std::filesystem::create_directories(texture_dir);

    // Copy file
    try {
        std::filesystem::copy_file(src_path, dst_path,
            std::filesystem::copy_options::overwrite_existing);
    } catch (...) {
        Logger::ImGui(("ISFP-xLink:ImGui:Failed to copy background image: " + src_path + "\n").c_str());
        return;
    }

    LoadBGTexture(dst_path);
    SaveUISettings();
}

void ImGuiManager::ApplyPresetTheme(int index) {
    if (index < 0 || index >= 5) return;
    theme_preset_ = index;
    const auto& p = kPresets[index];
    for (int i = 0; i < 3; i++) {
        theme_custom_window_[i] = p.window[i];
        theme_custom_child_[i]  = p.child[i];
        theme_custom_button_[i] = p.button[i];
        theme_custom_btn_hover_[i] = p.btn_hover[i];
        theme_custom_btn_active_[i] = p.btn_active[i];
        theme_custom_btn_clicked_[i] = p.btn_clicked[i];
        theme_custom_border_[i] = p.border[i];
        theme_custom_frame_[i]  = p.frame[i];
        theme_custom_frame_active_[i] = p.frame_active[i];
        theme_custom_popup_[i]  = p.popup[i];
        theme_custom_dropdown_frame_[i] = p.dropdown_frame[i];
        theme_custom_dropdown_border_[i] = p.dropdown_border[i];
        theme_custom_dropdown_active_[i] = p.dropdown_active[i];
        theme_custom_table_header_text_[i] = p.table_header_text[i];
        theme_custom_table_header_bg_[i] = p.table_header_bg[i];
        theme_custom_table_content_text_[i] = p.table_content_text[i];
        theme_custom_table_content_bg_[i] = p.table_content_bg[i];
        theme_custom_slider_[i] = p.slider[i];
        theme_custom_slider_track_[i] = p.slider_track[i];
        theme_custom_slider_track_hover_[i] = p.slider_track_hover[i];
        theme_custom_slider_active_[i] = p.slider_active[i];
        theme_custom_text_[i]   = p.text[i];
        theme_custom_toast_text_[i] = p.toast_text[i];
        theme_custom_check_[i]  = p.check[i];
    }
}

void ImGuiManager::PushThemeColors() {
    float btn_r = 0.15f, btn_g = 0.45f, btn_b = 0.80f;
    float bh_r = 0.25f, bh_g = 0.55f, bh_b = 0.85f;
    float ba_r = 0.20f, ba_g = 0.55f, ba_b = 0.90f;
    float bc_r = 0.10f, bc_g = 0.35f, bc_b = 0.70f;
    float fr_r = 0.20f, fr_g = 0.20f, fr_b = 0.25f;
    float pp_r = 0.12f, pp_g = 0.12f, pp_b = 0.16f;
    float sl_r = 0.40f, sl_g = 0.60f, sl_b = 0.90f;
    float tx_r = 0.90f, tx_g = 0.90f, tx_b = 0.92f;
    float win_r = 0.06f, win_g = 0.06f, win_b = 0.06f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& p = kPresets[theme_preset_];
        btn_r = p.button[0]; btn_g = p.button[1]; btn_b = p.button[2];
        bh_r = p.btn_hover[0]; bh_g = p.btn_hover[1]; bh_b = p.btn_hover[2];
        ba_r = p.btn_active[0]; ba_g = p.btn_active[1]; ba_b = p.btn_active[2];
        bc_r = p.btn_clicked[0]; bc_g = p.btn_clicked[1]; bc_b = p.btn_clicked[2];
        fr_r = p.frame[0]; fr_g = p.frame[1]; fr_b = p.frame[2];
        pp_r = p.popup[0]; pp_g = p.popup[1]; pp_b = p.popup[2];
        sl_r = p.slider[0]; sl_g = p.slider[1]; sl_b = p.slider[2];
        tx_r = p.text[0]; tx_g = p.text[1]; tx_b = p.text[2];
        win_r = p.window[0]; win_g = p.window[1]; win_b = p.window[2];
    } else if (theme_preset_ < 0) {
        btn_r = theme_custom_button_[0]; btn_g = theme_custom_button_[1]; btn_b = theme_custom_button_[2];
        bh_r = theme_custom_btn_hover_[0]; bh_g = theme_custom_btn_hover_[1]; bh_b = theme_custom_btn_hover_[2];
        ba_r = theme_custom_btn_active_[0]; ba_g = theme_custom_btn_active_[1]; ba_b = theme_custom_btn_active_[2];
        bc_r = theme_custom_btn_clicked_[0]; bc_g = theme_custom_btn_clicked_[1]; bc_b = theme_custom_btn_clicked_[2];
        fr_r = theme_custom_frame_[0]; fr_g = theme_custom_frame_[1]; fr_b = theme_custom_frame_[2];
        pp_r = theme_custom_popup_[0]; pp_g = theme_custom_popup_[1]; pp_b = theme_custom_popup_[2];
        sl_r = theme_custom_slider_[0]; sl_g = theme_custom_slider_[1]; sl_b = theme_custom_slider_[2];
        tx_r = theme_custom_text_[0]; tx_g = theme_custom_text_[1]; tx_b = theme_custom_text_[2];
        win_r = theme_custom_window_[0]; win_g = theme_custom_window_[1]; win_b = theme_custom_window_[2];
    }
    float t_r = win_r + 0.04f; if (t_r > 1.0f) t_r = 1.0f;
    float t_g = win_g + 0.04f; if (t_g > 1.0f) t_g = 1.0f;
    float t_b = win_b + 0.04f; if (t_b > 1.0f) t_b = 1.0f;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(tx_r, tx_g, tx_b, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(t_r, t_g, t_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(t_r, t_g, t_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(btn_r, btn_g, btn_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bh_r, bh_g, bh_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ba_r, ba_g, ba_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(fr_r, fr_g, fr_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(pp_r, pp_g, pp_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(sl_r, sl_g, sl_b, current_ctrl_alpha_));
    float sa_r = 0.55f, sa_g = 0.75f, sa_b = 1.00f;
    float st_r = 0.15f, st_g = 0.15f, st_b = 0.20f;
    float sth_r = 0.20f, sth_g = 0.20f, sth_b = 0.28f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& p = kPresets[theme_preset_];
        sa_r = p.slider_active[0]; sa_g = p.slider_active[1]; sa_b = p.slider_active[2];
        st_r = p.slider_track[0]; st_g = p.slider_track[1]; st_b = p.slider_track[2];
        sth_r = p.slider_track_hover[0]; sth_g = p.slider_track_hover[1]; sth_b = p.slider_track_hover[2];
    } else if (theme_preset_ < 0) {
        sa_r = theme_custom_slider_active_[0]; sa_g = theme_custom_slider_active_[1]; sa_b = theme_custom_slider_active_[2];
        st_r = theme_custom_slider_track_[0]; st_g = theme_custom_slider_track_[1]; st_b = theme_custom_slider_track_[2];
        sth_r = theme_custom_slider_track_hover_[0]; sth_g = theme_custom_slider_track_hover_[1]; sth_b = theme_custom_slider_track_hover_[2];
    }
    // Scrollbar grab hover: brighter than normal grab for visibility
    float sh_r = min(sl_r * 1.35f, 1.0f);
    float sh_g = min(sl_g * 1.35f, 1.0f);
    float sh_b = min(sl_b * 1.35f, 1.0f);
    float ck_r = 0.70f, ck_g = 0.70f, ck_b = 0.70f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& p = kPresets[theme_preset_];
        ck_r = p.check[0]; ck_g = p.check[1]; ck_b = p.check[2];
    } else if (theme_preset_ < 0) {
        ck_r = theme_custom_check_[0]; ck_g = theme_custom_check_[1]; ck_b = theme_custom_check_[2];
    }
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(sa_r, sa_g, sa_b, current_ctrl_alpha_));
    // Scrollbar colors (separate from slider in custom mode)
    float sb_r = st_r, sb_g = st_g, sb_b = st_b;      // track bg
    float sg_r = sl_r, sg_g = sl_g, sg_b = sl_b;      // grab
    float sgh_r = sh_r, sgh_g = sh_g, sgh_b = sh_b;   // grab hover
    float sga_r = sa_r, sga_g = sa_g, sga_b = sa_b;   // grab active
    if (theme_preset_ < 0) {
        sb_r = theme_custom_scrollbar_bg_[0]; sb_g = theme_custom_scrollbar_bg_[1]; sb_b = theme_custom_scrollbar_bg_[2];
        sg_r = theme_custom_scrollbar_grab_[0]; sg_g = theme_custom_scrollbar_grab_[1]; sg_b = theme_custom_scrollbar_grab_[2];
        sgh_r = theme_custom_scrollbar_grab_hover_[0]; sgh_g = theme_custom_scrollbar_grab_hover_[1]; sgh_b = theme_custom_scrollbar_grab_hover_[2];
        sga_r = theme_custom_scrollbar_grab_active_[0]; sga_g = theme_custom_scrollbar_grab_active_[1]; sga_b = theme_custom_scrollbar_grab_active_[2];
    }
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(sb_r, sb_g, sb_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(sg_r, sg_g, sg_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(sgh_r, sgh_g, sgh_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(sga_r, sga_g, sga_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(ck_r, ck_g, ck_b, current_ctrl_alpha_));
    // Table header/content background colors
    float thb_r = 0.15f, thb_g = 0.15f, thb_b = 0.20f;
    float tcb_r = 0.06f, tcb_g = 0.06f, tcb_b = 0.08f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& cp = kPresets[theme_preset_];
        thb_r = cp.table_header_bg[0]; thb_g = cp.table_header_bg[1]; thb_b = cp.table_header_bg[2];
        tcb_r = cp.table_content_bg[0]; tcb_g = cp.table_content_bg[1]; tcb_b = cp.table_content_bg[2];
    } else if (theme_preset_ < 0) {
        thb_r = theme_custom_table_header_bg_[0]; thb_g = theme_custom_table_header_bg_[1]; thb_b = theme_custom_table_header_bg_[2];
        tcb_r = theme_custom_table_content_bg_[0]; tcb_g = theme_custom_table_content_bg_[1]; tcb_b = theme_custom_table_content_bg_[2];
    }
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(thb_r, thb_g, thb_b, current_ctrl_alpha_));
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(tcb_r, tcb_g, tcb_b, current_ctrl_alpha_));
    // HeaderActive for dropdown list clicked/active item
    float hd_r = 0.20f, hd_g = 0.40f, hd_b = 0.70f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& cp = kPresets[theme_preset_];
        hd_r = cp.dropdown_active[0]; hd_g = cp.dropdown_active[1]; hd_b = cp.dropdown_active[2];
    } else if (theme_preset_ < 0) {
        hd_r = theme_custom_dropdown_active_[0]; hd_g = theme_custom_dropdown_active_[1]; hd_b = theme_custom_dropdown_active_[2];
    }
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(hd_r, hd_g, hd_b, current_ctrl_alpha_));
    // FrameBgActive for input box clicked/focused state
    float fa_r = fr_r + 0.08f; if (fa_r > 1.0f) fa_r = 1.0f;
    float fa_g = fr_g + 0.08f; if (fa_g > 1.0f) fa_g = 1.0f;
    float fa_b = fr_b + 0.08f; if (fa_b > 1.0f) fa_b = 1.0f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& cp = kPresets[theme_preset_];
        fa_r = cp.frame_active[0]; fa_g = cp.frame_active[1]; fa_b = cp.frame_active[2];
    } else if (theme_preset_ < 0) {
        fa_r = theme_custom_frame_active_[0]; fa_g = theme_custom_frame_active_[1]; fa_b = theme_custom_frame_active_[2];
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(fa_r, fa_g, fa_b, current_ctrl_alpha_));
    // FrameBgHovered for input box hover
    float fh_r = fr_r + 0.05f; if (fh_r > 1.0f) fh_r = 1.0f;
    float fh_g = fr_g + 0.05f; if (fh_g > 1.0f) fh_g = 1.0f;
    float fh_b = fr_b + 0.05f; if (fh_b > 1.0f) fh_b = 1.0f;
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(fh_r, fh_g, fh_b, current_ctrl_alpha_));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 8.0f);
}

void ImGuiManager::PopThemeColors() {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(20); // +HeaderActive+TableHeaderBg+TableRowBg
}

void ImGuiManager::ScanFonts() {
    available_fonts_.clear();
    available_fonts_.push_back("Default"); // index 0 = default font

    DWORD attr = GetFileAttributesA(font_dir_.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return;

    // Search for .ttf and .ttc files
    std::string search = font_dir_ + "*.*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        std::string name = ffd.cFileName;
        std::string ext = name.size() > 4 ? name.substr(name.size() - 4) : "";
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".ttf" || ext == ".ttc") {
            available_fonts_.push_back(name);
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);

    Logger::ImGui(("ISFP-xLink:ImGui:Found " + std::to_string(available_fonts_.size() - 1) + " font(s)\n").c_str());
}

void ImGuiManager::ApplyFont(int index) {
    if (index < 0 || index >= (int)available_fonts_.size()) return;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // Use scaled font size
    float font_size = base_font_size_ * dpi_scale_ * ui_scale_;
    if (font_size < 8.0f) font_size = 8.0f;

    if (index == 0) {
        // Prefer msyh.ttc as primary font (Latin + CJK in one)
        const char* base_candidates[] = {
            "C:\\Windows\\Fonts\\msyh.ttc", "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf", nullptr
        };
        bool loaded = false;
        for (const char* fp : base_candidates) {
            if (fp && GetFileAttributesA(fp) != INVALID_FILE_ATTRIBUTES) {
                io.Fonts->AddFontFromFileTTF(fp, font_size, nullptr, nullptr);
                loaded = true;
                break;
            }
        }
        if (!loaded) io.Fonts->AddFontDefault();
    } else {
        std::string font_path = font_dir_ + available_fonts_[index];
        io.Fonts->AddFontFromFileTTF(font_path.c_str(), font_size, nullptr, nullptr);
    }

    // Merge CJK fallback font for Chinese character support
    MergeCJKFallbackFont(font_size);

    // Mark font dirty so it gets rebuilt on next render
    font_dirty_ = true;
    current_font_index_ = index;
    Logger::ImGui(("ISFP-xLink:ImGui:Applied font: " + available_fonts_[index] +
        " @" + std::to_string(font_size) + "px\n").c_str());
}

void ImGuiManager::MergeCJKFallbackFont(float font_size) {
    ImGuiIO& io = ImGui::GetIO();

    const char* cjk_candidates[] = {
        "C:\\Windows\\Fonts\\msyhbd.ttc", "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simsun.ttc", "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\deng.ttf",
    };

    std::string cjk_path;
    for (const char* path : cjk_candidates) {
        DWORD attr = GetFileAttributesA(path);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            cjk_path = path; break;
        }
    }

    if (cjk_path.empty()) {
        Logger::ImGui("ISFP-xLink:ImGui:No CJK fallback font found\n");
        return;
    }

    ImFontConfig config;
    config.MergeMode = true;
    const ImWchar* cjk_ranges = io.Fonts->GetGlyphRangesChineseFull();
    ImFont* cjk_font = io.Fonts->AddFontFromFileTTF(cjk_path.c_str(), font_size, &config, cjk_ranges);

    if (cjk_font) {
        Logger::ImGui(("ISFP-xLink:ImGui:Merged CJK: " + cjk_path + " @" +
            std::to_string(font_size) + "px\n").c_str());
    }
}

void ImGuiManager::ApplyUIScale() {
    ImGuiIO& io = ImGui::GetIO();

    // Calculate target font size (base × DPI × user scale)
    float target_size = base_font_size_ * dpi_scale_ * ui_scale_;
    if (target_size < 8.0f) target_size = 8.0f;

    // Only rebuild if size actually changed
    // Track reload_version so this cache is invalidated on plugin reload
    static float s_last_size = 0;
    static int s_size_version = 0;
    if (s_size_version != reload_version_) {
        s_last_size = 0;
        s_size_version = reload_version_;
    }
    if (fabs(target_size - s_last_size) < 0.5f) return;
    s_last_size = target_size;

    // Reload current font at target size (instead of using FontGlobalScale)
    io.Fonts->Clear();
    if (current_font_index_ <= 0) {
        // Prefer msyh.ttc as primary font (Latin + CJK in one)
        const char* base_candidates[] = {
            "C:\\Windows\\Fonts\\msyh.ttc", "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf", nullptr
        };
        bool loaded = false;
        for (const char* fp : base_candidates) {
            if (fp && GetFileAttributesA(fp) != INVALID_FILE_ATTRIBUTES) {
                io.Fonts->AddFontFromFileTTF(fp, target_size, nullptr, nullptr);
                loaded = true;
                break;
            }
        }
        if (!loaded) io.Fonts->AddFontDefault();
    } else {
        std::string font_path = font_dir_ + available_fonts_[current_font_index_];
        io.Fonts->AddFontFromFileTTF(font_path.c_str(), target_size, nullptr, nullptr);
    }

    // Merge CJK fallback at same target size
    {
        const char* cjk_candidates[] = {
            "C:\\Windows\\Fonts\\msyhbd.ttc", "C:\\Windows\\Fonts\\msyh.ttc",
            "C:\\Windows\\Fonts\\simsun.ttc", "C:\\Windows\\Fonts\\simhei.ttf",
            "C:\\Windows\\Fonts\\deng.ttf",
        };
        std::string cjk_path;
        for (const char* path : cjk_candidates) {
            DWORD attr = GetFileAttributesA(path);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                cjk_path = path; break;
            }
        }
        if (!cjk_path.empty()) {
            ImFontConfig cfg;
            cfg.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(cjk_path.c_str(), target_size, &cfg,
                io.Fonts->GetGlyphRangesChineseFull());
        }
    }

    font_dirty_ = true;
    Logger::ImGui(("ISFP-xLink:ImGui:Font resized to " +
        std::to_string(target_size) + "px\n").c_str());
}

void ImGuiManager::RenderHotkeyBindingPopup() {
    if (!binding_mode_) return;

    ImGui::SetNextWindowSize(ImVec2(420, 220));
    ImGui::OpenPopup(GetText(StringID::HotkeyBind));

    if (ImGui::BeginPopupModal(GetText(StringID::HotkeyBind), nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        float win_w = ImGui::GetWindowSize().x;
        const char* hint = GetText(StringID::HotkeyBinding);
        float hint_w = ImGui::CalcTextSize(hint).x;
        ImGui::SetCursorPosX((win_w - hint_w) * 0.5f);
        ImGui::TextWrapped("%s", hint);
        ImGui::Dummy(ImVec2(0, 12));

        // Build real-time display string
        std::string key_display;
        if (pending_hotkey_.vkey == 0 && !pending_hotkey_.shift && !pending_hotkey_.ctrl && !pending_hotkey_.alt) {
            key_display = GetText(StringID::HotkeyUnbound);
        } else if (bind_captured_ || pending_hotkey_.vkey != 0) {
            key_display = GetHotkeyDisplayName(pending_hotkey_);
        } else {
            if (pending_hotkey_.ctrl)  key_display += "Ctrl+";
            if (pending_hotkey_.shift) key_display += "Shift+";
            if (pending_hotkey_.alt)   key_display += "Alt+";
            key_display += "...";
        }

        float btn_w = 260.0f;
        ImGui::SetCursorPosX((win_w - btn_w) * 0.5f);
        // Blue = awaiting key; Gray = ESC cleared
        bool is_unbound = bind_cleared_;
        ImVec2 key_size(btn_w, 44);
        if (is_unbound) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.80f, 1.0f));
        }
        // Click the key display to re-enter detection (when cleared by ESC)
        if (ImGui::Button(key_display.c_str(), key_size) && is_unbound) {
            bind_cleared_ = false;
            bind_captured_ = false;
            pending_hotkey_ = HotkeyBinding();
            pending_hotkey_.vkey = 0;
            bind_just_entered_ = true;
            bind_prev_key_ = 0;
            bind_peak_shift_ = false;
            bind_peak_ctrl_ = false;
            bind_peak_alt_ = false;
        }
        ImGui::PopStyleColor();
        if (is_unbound) ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 16));

        float total_btn_w = 150.0f + 150.0f + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX((win_w - total_btn_w) * 0.5f);

        // Confirm always enabled — no alpha flicker during key press
        if (ImGui::Button("\xE7\xA1\xAE\xE8\xAE\xA4  Confirm", ImVec2(150, 30))) {
            // Apply the binding to the correct target
            HotkeyBinding* target_hk = nullptr;
            const char* cfg_prefix = "";
            if (binding_target_ == BindTarget::TOGGLE_UI) {
                target_hk = &g_hotkey;
                cfg_prefix = "hotkey.toggle_ui";
            } else if (binding_target_ == BindTarget::TOGGLE_MOUSE_ROLLER) {
                target_hk = &g_mouseyoke_hotkey;
                cfg_prefix = "hotkey.mouseyoke";
            }
            if (target_hk) {
                *target_hk = pending_hotkey_;
                if (g_config) {
                    g_config->SetInt(std::string(cfg_prefix) + ".vkey", target_hk->vkey);
                    g_config->SetBool(std::string(cfg_prefix) + ".ctrl", target_hk->ctrl);
                    g_config->SetBool(std::string(cfg_prefix) + ".shift", target_hk->shift);
                    g_config->SetBool(std::string(cfg_prefix) + ".alt", target_hk->alt);
                    g_config->Save();
                }
            }
            binding_mode_ = false;
            bind_captured_ = false;
            binding_target_ = BindTarget::NONE;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("\xE5\x8F\x96\xE6\xB6\x88  Cancel", ImVec2(150, 30))) {
            binding_mode_ = false;
            bind_captured_ = false;
            binding_target_ = BindTarget::NONE;
            ImGui::CloseCurrentPopup();
        }

        // ESC clears the key instead of closing (edge-triggered)
        static bool s_esc_was_down = false;
        bool esc_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (esc_down && !s_esc_was_down) {
            pending_hotkey_ = HotkeyBinding(); // clear key
            pending_hotkey_.vkey = 0;
            bind_cleared_ = true;
            bind_captured_ = true;  // allow confirming the cleared state
        }
        s_esc_was_down = esc_down;

        ImGui::EndPopup();
    }
}

void ImGuiManager::SaveUISettings() {
    if (!g_config) return;
    g_config->SetInt("window.position.x", window_x_);
    g_config->SetInt("window.position.y", window_y_);
    g_config->SetInt("window.size.width", window_w_);
    g_config->SetInt("window.size.height", window_h_);
    g_config->SetFloat("ui.scale", ui_scale_);
    g_config->SetString("ui.font",
        (current_font_index_ < 0 || current_font_index_ >= (int)available_fonts_.size())
        ? "Default" : available_fonts_[current_font_index_]);
    g_config->SetBool("ui.hide_on_startup", hide_on_startup_);
    g_config->SetFloat("ui.bg_alpha", bg_alpha_);
    g_config->SetInt("ui.theme_preset", theme_preset_);
    auto save_color = [&](const char* prefix, float col[3]) {
        char key[64];
        snprintf(key, sizeof(key), "ui.%s_r", prefix); g_config->SetFloat(key, col[0]);
        snprintf(key, sizeof(key), "ui.%s_g", prefix); g_config->SetFloat(key, col[1]);
        snprintf(key, sizeof(key), "ui.%s_b", prefix); g_config->SetFloat(key, col[2]);
    };
    save_color("theme_window", theme_custom_window_);
    save_color("theme_child", theme_custom_child_);
    save_color("theme_button", theme_custom_button_);
    save_color("theme_btn_hover", theme_custom_btn_hover_);
    save_color("theme_btn_active", theme_custom_btn_active_);
    save_color("theme_btn_clicked", theme_custom_btn_clicked_);
    save_color("theme_border", theme_custom_border_);
    save_color("theme_frame", theme_custom_frame_);
    save_color("theme_frame_active", theme_custom_frame_active_);
    save_color("theme_popup", theme_custom_popup_);
    save_color("theme_dropdown_frame", theme_custom_dropdown_frame_);
    save_color("theme_dropdown_border", theme_custom_dropdown_border_);
    save_color("theme_dropdown_active", theme_custom_dropdown_active_);
    save_color("theme_slider", theme_custom_slider_);
    save_color("theme_slider_track", theme_custom_slider_track_);
    save_color("theme_slider_track_hover", theme_custom_slider_track_hover_);
    save_color("theme_slider_active", theme_custom_slider_active_);
    save_color("theme_check", theme_custom_check_);
    save_color("theme_text", theme_custom_text_);
    save_color("theme_toast_text", theme_custom_toast_text_);
    save_color("theme_table_header_text", theme_custom_table_header_text_);
    save_color("theme_table_header_bg", theme_custom_table_header_bg_);
    save_color("theme_table_content_text", theme_custom_table_content_text_);
    save_color("theme_table_content_bg", theme_custom_table_content_bg_);
    save_color("theme_text", theme_custom_text_);
    g_config->Save();
}

void ImGuiManager::LoadUISettings() {
    if (!g_config) return;

    // Window geometry
    window_x_ = g_config->GetInt("window.position.x", 50);
    window_y_ = g_config->GetInt("window.position.y", 50);
    window_w_ = g_config->GetInt("window.size.width", 820);
    window_h_ = g_config->GetInt("window.size.height", 560);
    window_geom_loaded_ = true;

    // UI appearance
    ui_scale_ = g_config->GetFloat("ui.scale", 1.0f);
    std::string font_name = g_config->GetString("ui.font", "Default");
    if (font_name != "Default") {
        for (int i = 1; i < (int)available_fonts_.size(); i++) {
            if (available_fonts_[i] == font_name) {
                current_font_index_ = i;
                break;
            }
        }
    }

    // Behavior
    hide_on_startup_ = g_config->GetBool("ui.hide_on_startup", false);

    // Background
    bg_alpha_ = g_config->GetFloat("ui.bg_alpha", 0.5f);
    // Try to load saved background texture
    char xp_root[512] = {0};
    XPLMGetSystemPath(xp_root);
    std::string tex_path = std::string(xp_root) + "Resources\\plugins\\ISFP_xLink\\Data\\Texture\\ui_background.png";
    if (std::filesystem::exists(tex_path)) {
        LoadBGTexture(tex_path);
    }

    // Theme
    auto load_color = [&](const char* prefix, float col[3], float def_r, float def_g, float def_b) {
        char key[64];
        snprintf(key, sizeof(key), "ui.%s_r", prefix); col[0] = g_config->GetFloat(key, def_r);
        snprintf(key, sizeof(key), "ui.%s_g", prefix); col[1] = g_config->GetFloat(key, def_g);
        snprintf(key, sizeof(key), "ui.%s_b", prefix); col[2] = g_config->GetFloat(key, def_b);
    };
    theme_preset_ = g_config->GetInt("ui.theme_preset", 4);
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        ApplyPresetTheme(theme_preset_);
    }
    load_color("theme_window",   theme_custom_window_,   0.06f, 0.06f, 0.06f);
    load_color("theme_child",    theme_custom_child_,    0.10f, 0.10f, 0.12f);
    load_color("theme_button",   theme_custom_button_,   0.15f, 0.45f, 0.80f);
    load_color("theme_btn_hover",theme_custom_btn_hover_,0.25f, 0.55f, 0.85f);
    load_color("theme_btn_active",theme_custom_btn_active_,0.15f,0.45f,0.80f);
    load_color("theme_btn_clicked",theme_custom_btn_clicked_,0.10f,0.35f,0.70f);
    load_color("theme_border",   theme_custom_border_,   0.30f, 0.30f, 0.35f);
    load_color("theme_frame",    theme_custom_frame_,    0.20f, 0.20f, 0.25f);
    load_color("theme_frame_active", theme_custom_frame_active_, 0.28f, 0.28f, 0.33f);
    load_color("theme_popup",    theme_custom_popup_,    0.12f, 0.12f, 0.16f);
    load_color("theme_dropdown_frame",  theme_custom_dropdown_frame_,  0.18f, 0.18f, 0.22f);
    load_color("theme_dropdown_border", theme_custom_dropdown_border_, 0.25f, 0.25f, 0.30f);
    load_color("theme_dropdown_active", theme_custom_dropdown_active_, 0.20f, 0.40f, 0.70f);
    load_color("theme_slider",   theme_custom_slider_,   0.40f, 0.60f, 0.90f);
    load_color("theme_slider_track", theme_custom_slider_track_, 0.15f, 0.15f, 0.20f);
    load_color("theme_slider_track_hover",theme_custom_slider_track_hover_,0.20f,0.20f,0.28f);
    load_color("theme_slider_active",theme_custom_slider_active_,0.55f, 0.75f, 1.00f);
    load_color("theme_check",    theme_custom_check_,    0.70f, 0.70f, 0.70f);
    load_color("theme_toast_text", theme_custom_toast_text_, 1.0f, 0.70f, 0.20f);
    load_color("theme_table_header_text",  theme_custom_table_header_text_,  0.90f, 0.90f, 0.92f);
    load_color("theme_table_header_bg",    theme_custom_table_header_bg_,    0.15f, 0.15f, 0.20f);
    load_color("theme_table_content_text", theme_custom_table_content_text_, 0.90f, 0.90f, 0.92f);
    load_color("theme_table_content_bg",   theme_custom_table_content_bg_,   0.06f, 0.06f, 0.08f);
    load_color("theme_text",     theme_custom_text_,     0.90f, 0.90f, 0.92f);

    Logger::ImGui("ISFP-xLink:ImGui:UI settings loaded\n");
}

void ImGuiManager::RenderSettingsPage() {
    ImGui::Text("%s", GetText(StringID::Settings_Title));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    // ========== UI 设置 ==========
    ImGui::Dummy(ImVec2(0, 5));

    // Language toggle
    const char* lang_label = GetText(StringID::LangToggle);
    bool is_cn = (g_language == Language::CN);
    ImGui::Text("%s", g_language == Language::CN ? "\xe8\xaf\xad\xe8\xa8\x80" : "Language");
    ImGui::SameLine();
    if (ImGui::SmallButton(lang_label)) {
        if (g_language == Language::EN && (current_font_index_ < 0 || current_font_index_ == 0)) {
            // Font is Default — cannot switch to Chinese
            toast_message_ = g_language == Language::CN ? "\xe5\xbd\x93\xe5\x89\x8d\xe5\xad\x97\xe4\xbd\x93\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe8\xaf\xa5\xe8\xaf\xad\xe8\xa8\x80" : "Current font does not support this language";
            toast_timestamp_ = XPLMGetElapsedTime();
        } else {
            g_language = (g_language == Language::EN) ? Language::CN : Language::EN;
            if (g_config) { g_config->SetString("ui.language", (g_language == Language::CN) ? "CN" : "EN"); g_config->Save(); }
        }
    }
    ImGui::Dummy(ImVec2(0, 8));

    // ========== 主题 / Theme ==========
    ImGui::Separator();
    ImGui::Text("%s", g_language == Language::CN ? "\xe4\xb8\xbb\xe9\xa2\x98" : "Theme");
    ImGui::Dummy(ImVec2(0, 4));

    // Preset theme buttons (5 presets in a row, with generous left/right margin)
    const char* preset_names[5];
    for (int i = 0; i < 5; i++) {
        preset_names[i] = (g_language == Language::CN) ? kPresets[i].name_cn : kPresets[i].name_en;
    }
    float theme_margin = 14.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float theme_avail = ImGui::GetContentRegionAvail().x;
    float theme_btn_w = (theme_avail - theme_margin * 2.0f - 4 * spacing) / 5.0f;
    if (theme_btn_w < 30.0f) theme_btn_w = 30.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + theme_margin);
    for (int i = 0; i < 5; i++) {
        if (i > 0) ImGui::SameLine();
        bool is_active = (theme_preset_ == i);
        if (is_active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.30f, 1.0f));
        if (ImGui::Button(preset_names[i], ImVec2(theme_btn_w, 28))) {
            ApplyPresetTheme(i);
            theme_custom_open_ = false; // close custom when preset is selected
            SaveUISettings();
        }
        if (is_active) ImGui::PopStyleColor();
    }

    // Custom toggle
    ImGui::Dummy(ImVec2(0, 4));
    if (ImGui::Checkbox(g_language == Language::CN ? "\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89" : "Custom", &theme_custom_open_)) {
        if (theme_custom_open_) theme_preset_ = -1; // switch to custom
        SaveUISettings();
    }

    // Custom color pickers — table with groups & collapsible sub-items
    if (theme_custom_open_ && theme_preset_ < 0) {
        ImGui::Dummy(ImVec2(0, 2));

        // Build item list
        struct ColorTableItem {
            const char* name_cn;
            const char* name_en;
            float* color;       // RGB array (nullptr for parent with children)
            int group;          // 0=global, 1=independent
            int parent_idx;     // -1=top, otherwise parent item index
            bool has_sub;       // has collapsible children
            float* def_color;   // default color for parent items (nullptr for leaves)
        };
        // Flat list: global items, then independent parents with their children
        ColorTableItem items[] = {
            // Global group
            {"\xe4\xb8\xbb\xe7\xaa\x97\xe5\x8f\xa3", "Window",       theme_custom_window_, 0, -1, false, nullptr},
            {"\xe5\xad\x90\xe7\xaa\x97\xe5\x8f\xa3",  "Child",        theme_custom_child_,  0, -1, false, nullptr},
            {"\xe8\xbe\xb9\xe6\xa1\x86",              "Border",       theme_custom_border_, 0, -1, false, nullptr},
            {"\xe6\x96\x87\xe5\xad\x97",              "Text",         theme_custom_text_,   0, -1, false, nullptr},
            {"\xe6\x8f\x90\xe7\xa4\xba\xe6\x96\x87\xe5\xad\x97","Hint Text",theme_custom_toast_text_, 0, -1, false, nullptr},
            // Independent: Button group
            {"\xe6\x8c\x89\xe9\x92\xae",              "Button",       nullptr,              1, -1, true,  theme_custom_button_},
            {"  - \xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Background", theme_custom_button_, 1, 4, false, nullptr},
            {"  - \xe7\x82\xb9\xe5\x87\xbb","- Clicked", theme_custom_btn_clicked_, 1, 4, false, nullptr},
            {"  - \xe6\xbf\x80\xe6\xb4\xbb","- Active",  theme_custom_btn_active_, 1, 4, false, nullptr},
            {"  - \xe6\x82\xac\xe5\x81\x9c","- Hover",   theme_custom_btn_hover_, 1, 4, false, nullptr},
            // Independent: Table group
            {"\xe8\xa1\xa8\xe6\xa0\xbc",              "Table",        nullptr,              1, -1, true,  theme_custom_table_header_bg_},
            {"  - \xe6\xa0\x87\xe9\xa2\x98.\xe6\x96\x87\xe5\xad\x97\xe9\xa2\x9c\xe8\x89\xb2","  - Header.Text", theme_custom_table_header_text_, 1, 9, false, nullptr},
            {"  - \xe6\xa0\x87\xe9\xa2\x98.\xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Header.Background", theme_custom_table_header_bg_, 1, 9, false, nullptr},
            {"  - \xe5\x86\x85\xe5\xae\xb9.\xe6\x96\x87\xe5\xad\x97\xe9\xa2\x9c\xe8\x89\xb2","  - Content.Text", theme_custom_table_content_text_, 1, 9, false, nullptr},
            {"  - \xe5\x86\x85\xe5\xae\xb9.\xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Content.Background", theme_custom_table_content_bg_, 1, 9, false, nullptr},
            // Independent: Slider group
            {"\xe6\xbb\x91\xe5\x8a\xa8\xe6\x9d\xa1",  "Slider",       nullptr,              1, -1, true,  theme_custom_slider_},
            {"  - \xe8\xbd\xa8\xe9\x81\x93.\xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Track.Background",  theme_custom_slider_track_, 1, 14, false, nullptr},
            {"  - \xe8\xbd\xa8\xe9\x81\x93.\xe6\x82\xac\xe5\x81\x9c","  - Track.Hover",   theme_custom_slider_track_hover_, 1, 14, false, nullptr},
            {"  - \xe6\xbb\x91\xe5\x9d\x97.\xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Grab.Background",  theme_custom_slider_, 1, 14, false, nullptr},
            {"  - \xe6\xbb\x91\xe5\x9d\x97.\xe6\x8a\x93\xe5\x8f\x96","  - Grab.Active",  theme_custom_slider_active_, 1, 14, false, nullptr},
            // Independent: Input group
            {"\xe8\xbe\x93\xe5\x85\xa5\xe6\xa1\x86",  "Input Box",    nullptr,              1, -1, true,  theme_custom_frame_},
            {"  - \xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Background", theme_custom_frame_, 1, 19, false, nullptr},
            {"  - \xe7\x82\xb9\xe5\x87\xbb","- Clicked",   theme_custom_frame_active_, 1, 19, false, nullptr},
            // Independent: Dropdown group
            {"\xe4\xb8\x8b\xe6\x8b\x89\xe5\x88\x97\xe8\xa1\xa8", "Dropdown", nullptr, 1, -1, true, theme_custom_popup_},
            {"  - \xe6\x98\xbe\xe7\xa4\xba\xe6\xa1\x86.\xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Display.Background", theme_custom_dropdown_frame_, 1, 22, false, nullptr},
            {"  - \xe5\x88\x97\xe8\xa1\xa8.\xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - List.Background",   theme_custom_popup_, 1, 22, false, nullptr},
            {"  - \xe5\x88\x97\xe8\xa1\xa8.\xe8\xbe\xb9\xe6\xa1\x86","  - List.Border",   theme_custom_dropdown_border_, 1, 22, false, nullptr},
            {"  - \xe5\x88\x97\xe8\xa1\xa8.\xe7\x82\xb9\xe5\x87\xbb","  - List.Clicked",  theme_custom_dropdown_active_, 1, 22, false, nullptr},
            // Independent: Checkbox group
            {"\xe5\x8d\x95\xe9\x80\x89\xe6\xa1\x86",  "Checkbox",     nullptr,              1, -1, true,  theme_custom_check_},
            {"  - \xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Background", theme_custom_check_, 1, 27, false, nullptr},
            {"  - \xe5\x8b\xbe\xe9\x80\x89","- Check",  theme_custom_check_, 1, 27, false, nullptr},
            // Independent: Scrollbar group
            {"\xe6\xbb\x9a\xe5\x8a\xa8\xe6\x9d\xa1",  "Scrollbar",    nullptr,              1, -1, true,  theme_custom_scrollbar_bg_},
            {"  - \xe8\xbd\xa8\xe9\x81\x93.\xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Track.Background", theme_custom_scrollbar_bg_, 1, 30, false, nullptr},
            {"  - \xe6\xbb\x91\xe5\x9d\x97.\xe8\x83\x8c\xe6\x99\xaf\xe9\xa2\x9c\xe8\x89\xb2","  - Grab", theme_custom_scrollbar_grab_, 1, 30, false, nullptr},
            {"  - \xe6\xbb\x91\xe5\x9d\x97.\xe6\x82\xac\xe5\x81\x9c","  - Grab.Hover", theme_custom_scrollbar_grab_hover_, 1, 30, false, nullptr},
            {"  - \xe6\xbb\x91\xe5\x9d\x97.\xe6\x8a\x93\xe5\x8f\x96","  - Grab.Active", theme_custom_scrollbar_grab_active_, 1, 30, false, nullptr},
        };
        constexpr int item_cnt = sizeof(items) / sizeof(items[0]);

        // Brightness helper
        auto brightness = [](float c[3]) { return c ? c[0]+c[1]+c[2] : 0.0f; };

        // Sort indices by column
        int indices[30];
        for (int i = 0; i < item_cnt; i++) indices[i] = i;
        if (color_sort_col_ >= 0) {
            std::sort(indices, indices + item_cnt, [&](int a, int b) {
                if (items[a].group != items[b].group) return items[a].group < items[b].group;
                if (items[a].parent_idx != items[b].parent_idx) return items[a].parent_idx < items[b].parent_idx;
                bool ca = color_sort_col_ == 0;
                if (ca) {
                    const char* na = g_language == Language::CN ? items[a].name_cn : items[a].name_en;
                    const char* nb = g_language == Language::CN ? items[b].name_cn : items[b].name_en;
                    int cmp = _stricmp(na, nb);
                    return color_sort_asc_ ? (cmp < 0) : (cmp > 0);
                } else {
                    float ba = items[a].color ? brightness(items[a].color) : -1;
                    float bb = items[b].color ? brightness(items[b].color) : -1;
                    if (ba < 0) return false; if (bb < 0) return true;
                    return color_sort_asc_ ? (ba < bb) : (ba > bb);
                }
            });
        }

        // Collapsible state (per parent index)
        static bool s_collapsed[40] = {};

        // Table with scroll Y to limit height (prevents pushing keybinding off-screen)
        float table_max_h = ImGui::GetContentRegionAvail().y - 20.0f;
        if (table_max_h > 400.0f) table_max_h = 400.0f;
        if (table_max_h < 100.0f) table_max_h = 100.0f;
        if (ImGui::BeginTable("##ctable", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg,
            ImVec2(0.0f, table_max_h))) {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.45f);

            // Header row: push header text color locally
            float ht_r = 0.90f, ht_g = 0.90f, ht_b = 0.92f;
            if (theme_preset_ >= 0 && theme_preset_ < 5) {
                const auto& cp3 = kPresets[theme_preset_];
                ht_r = cp3.table_header_text[0]; ht_g = cp3.table_header_text[1]; ht_b = cp3.table_header_text[2];
            } else if (theme_preset_ < 0) {
                ht_r = theme_custom_table_header_text_[0]; ht_g = theme_custom_table_header_text_[1]; ht_b = theme_custom_table_header_text_[2];
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ht_r, ht_g, ht_b, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
            ImGui::TableNextRow();
            for (int col = 0; col < 2; col++) {
                ImGui::TableSetColumnIndex(col);
                ImGui::SetCursorPosY(0.0f);
                const char* hdr = (col == 0)
                    ? (g_language == Language::CN ? "\xe6\x8e\xa7\xe4\xbb\xb6\xe5\x90\x8d\xe7\xa7\xb0" : "Control")
                    : (g_language == Language::CN ? "\xe9\xa2\x9c\xe8\x89\xb2" : "Color");
                std::string ht = hdr;
                if (col == color_sort_col_)
                    ht += color_sort_asc_ ? " [A-Z]" : " [Z-A]";
                ImGui::PushID(col);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                float avail_x = ImGui::GetContentRegionAvail().x;
                if (ImGui::Button(ht.c_str(), ImVec2(avail_x, 0.0f))) {
                    if (color_sort_col_ == col) color_sort_asc_ = !color_sort_asc_;
                    else { color_sort_col_ = col; color_sort_asc_ = true; }
                }
                ImGui::PopStyleVar(2);
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(); // header text color

            // Render rows: push content text color locally
            float ct_r = 0.90f, ct_g = 0.90f, ct_b = 0.92f;
            if (theme_preset_ >= 0 && theme_preset_ < 5) {
                const auto& cp3 = kPresets[theme_preset_];
                ct_r = cp3.table_content_text[0]; ct_g = cp3.table_content_text[1]; ct_b = cp3.table_content_text[2];
            } else if (theme_preset_ < 0) {
                ct_r = theme_custom_table_content_text_[0]; ct_g = theme_custom_table_content_text_[1]; ct_b = theme_custom_table_content_text_[2];
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ct_r, ct_g, ct_b, 1.0f));
            // Render rows (skip children of collapsed parents and fix text)
            int last_group = -1;
            for (int ri = 0; ri < item_cnt; ri++) {
                int idx = indices[ri];
                auto& it = items[idx];

                // Skip children of collapsed parents entirely (don't create empty rows)
                if (it.parent_idx >= 0 && s_collapsed[it.parent_idx])
                    continue;

                // Group header
                if (it.group != last_group) {
                    last_group = it.group;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                        IM_COL32(40, 40, 55, 180));
                    ImGui::TextColored(ImVec4(0.7f,0.7f,1.0f,1.0f), "  %s",
                        it.group == 0
                            ? (g_language == Language::CN ? "\xe5\x85\xa8\xe5\xb1\x80\xe6\x8e\xa7\xe4\xbb\xb6" : "Global Controls")
                            : (g_language == Language::CN ? "\xe7\x8b\xac\xe7\xab\x8b\xe6\x8e\xa7\xe4\xbb\xb6" : "Indep. Controls"));
                    ImGui::TableSetColumnIndex(1);
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (it.has_sub) {
                    // Collapsible parent
                    ImGui::PushID(idx * 2 + 100);
                    bool tn_open = ImGui::TreeNodeEx("##tn", ImGuiTreeNodeFlags_SpanFullWidth,
                            "%s", g_language == Language::CN ? it.name_cn : it.name_en);
                    if (tn_open) {
                        ImGui::TreePop();
                    }
                    s_collapsed[idx] = !tn_open;
                    ImGui::PopID();
                } else {
                    // Leaf item - use simple > prefix instead of unicode arrow
                    float px = ImGui::GetCursorPosX() + 12.0f;
                    ImGui::SetCursorPosX(px);
                    ImGui::Text("%s", g_language == Language::CN ? it.name_cn : it.name_en);
                }

                // Color column
                ImGui::TableSetColumnIndex(1);
                if (it.color) {
                    char cid[32];
                    snprintf(cid, sizeof(cid), "##c%d", idx);
                    if (ImGui::ColorEdit3(cid, it.color,
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                        SaveUISettings();
                }
            }
            ImGui::PopStyleColor(); // content text color
            ImGui::EndTable();
        }

        ImGui::Dummy(ImVec2(0, 4));
    }
    ImGui::Dummy(ImVec2(0, 6));

    // Font display name with Chinese aliases for common TTC/TTF files
    auto font_display_name = [&](const std::string& filename) -> std::string {
        if (g_language != Language::CN) return filename;
        // Extract base name (lowercase, no extension)
        std::string base = filename;
        std::transform(base.begin(), base.end(), base.begin(), ::tolower);
        if (base.size() > 4) {
            std::string ext = base.substr(base.size() - 4);
            if (ext == ".ttf" || ext == ".ttc") base = base.substr(0, base.size() - 4);
        }
        static const std::pair<const char*, const char*> kFontMap[] = {
            {"msyh",     "\xe5\xbe\xae\xe8\xbd\xaf\xe9\x9b\x85\xe9\xbb\x91"},  // msyh → 微软雅黑
            {"msyhbd",   "\xe5\xbe\xae\xe8\xbd\xaf\xe9\x9b\x85\xe9\xbb\x91 \xe7\xb2\x97\xe4\xbd\x93"},  // msyhbd → 微软雅黑 粗体
            {"msyhl",    "\xe5\xbe\xae\xe8\xbd\xaf\xe9\x9b\x85\xe9\xbb\x91 \xe7\xbb\x86\xe4\xbd\x93"},  // msyhl → 微软雅黑 细体
            {"simhei",   "\xe9\xbb\x91\xe4\xbd\x93"},  // simhei → 黑体
            {"simsun",   "\xe5\xae\x8b\xe4\xbd\x93"},  // simsun → 宋体
            {"simkai",   "\xe6\xa5\xb7\xe4\xbd\x93"},  // simkai → 楷体
            {"simfang",  "\xe4\xbb\xbf\xe5\xae\x8b"},  // simfang → 仿宋
            {"deng",     "\xe7\xad\x89\xe7\xba\xbf"},  // deng → 等线
            {"fzsbd",    "\xe6\x96\xb9\xe6\xad\xa3\xe7\xb2\x97\xe9\xbb\x91\xe5\xae\x8b\xe7\xae\x80\xe4\xbd\x93"},  // fzsbd → 方正粗黑宋简体
            {"yahei",    "\xe5\xbe\xae\xe8\xbd\xaf\xe9\x9b\x85\xe9\xbb\x91"},  // yahei → 微软雅黑
            {"default",  "\xe9\xbb\x98\xe8\xae\xa4"},  // Default → 默认
        };
        for (auto& entry : kFontMap) {
            if (base == entry.first)
                return entry.second;
        }
        return filename;
    };

    // UI Font
    ImGui::Text("%s", GetText(StringID::UIFont));
    ImGui::SameLine();
    int cur_idx = current_font_index_ < 0 ? 0 : current_font_index_;
    std::string cur_display = font_display_name(available_fonts_[cur_idx]);
    // Local override: dropdown display box frame + border
    float df_r = 0.18f, df_g = 0.18f, df_b = 0.22f;
    float db_r = 0.25f, db_g = 0.25f, db_b = 0.30f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& cp = kPresets[theme_preset_];
        df_r = cp.dropdown_frame[0]; df_g = cp.dropdown_frame[1]; df_b = cp.dropdown_frame[2];
        db_r = cp.dropdown_border[0]; db_g = cp.dropdown_border[1]; db_b = cp.dropdown_border[2];
    } else if (theme_preset_ < 0) {
        df_r = theme_custom_dropdown_frame_[0]; df_g = theme_custom_dropdown_frame_[1]; df_b = theme_custom_dropdown_frame_[2];
        db_r = theme_custom_dropdown_border_[0]; db_g = theme_custom_dropdown_border_[1]; db_b = theme_custom_dropdown_border_[2];
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(df_r, df_g, df_b, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(db_r, db_g, db_b, 1.0f));
    if (ImGui::BeginCombo("##font", cur_display.c_str())) {
        for (int i = 0; i < (int)available_fonts_.size(); i++) {
            bool is_selected = (i == current_font_index_ || (i == 0 && current_font_index_ < 0));
            std::string item_display = font_display_name(available_fonts_[i]);
            if (ImGui::Selectable(item_display.c_str(), is_selected)) {
                if (i != current_font_index_) {
                    if (i == 0) {
                        if (g_language == Language::CN) {
                            // Language is Chinese — cannot switch to Default font
                            toast_message_ = g_language == Language::CN ? "\xe5\xbd\x93\xe5\x89\x8d\xe8\xaf\xad\xe8\xa8\x80\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe8\xaf\xa5\xe5\xad\x97\xe4\xbd\x93" : "Current language does not support this font";
                            toast_timestamp_ = XPLMGetElapsedTime();
                        } else {
                            current_font_index_ = -1;
                            ImGui::GetIO().Fonts->Clear();
                            ImGui::GetIO().Fonts->AddFontDefault();
                            font_dirty_ = true;
                            SaveUISettings();
                        }
                    } else {
                        ApplyFont(i);
                        SaveUISettings();
                    }
                }
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor(2);
    ImGui::Dummy(ImVec2(0, 5));

    // UI Scale
    ImGui::Text("%s", GetText(StringID::UIScale));
    ImGui::SameLine();
    const char* scale_items[] = { "75%", "100%", "125%", "150%" };
    const float scale_values[] = { 0.75f, 1.0f, 1.25f, 1.5f };
    int current_scale_idx = 1;
    for (int i = 0; i < 4; i++) {
        if (fabs(ui_scale_ - scale_values[i]) < 0.01f) {
            current_scale_idx = i;
            break;
        }
    }
    // Local override: dropdown display box frame + border
    float df2_r = 0.18f, df2_g = 0.18f, df2_b = 0.22f;
    float db2_r = 0.25f, db2_g = 0.25f, db2_b = 0.30f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& cp2 = kPresets[theme_preset_];
        df2_r = cp2.dropdown_frame[0]; df2_g = cp2.dropdown_frame[1]; df2_b = cp2.dropdown_frame[2];
        db2_r = cp2.dropdown_border[0]; db2_g = cp2.dropdown_border[1]; db2_b = cp2.dropdown_border[2];
    } else if (theme_preset_ < 0) {
        df2_r = theme_custom_dropdown_frame_[0]; df2_g = theme_custom_dropdown_frame_[1]; df2_b = theme_custom_dropdown_frame_[2];
        db2_r = theme_custom_dropdown_border_[0]; db2_g = theme_custom_dropdown_border_[1]; db2_b = theme_custom_dropdown_border_[2];
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(df2_r, df2_g, df2_b, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(db2_r, db2_g, db2_b, 1.0f));
    if (ImGui::BeginCombo("##scale", scale_items[current_scale_idx])) {
        for (int i = 0; i < 4; i++) {
            bool is_selected = (i == current_scale_idx);
            if (ImGui::Selectable(scale_items[i], is_selected)) {
                if (i != current_scale_idx) {
                    ui_scale_ = scale_values[i];
                    ApplyUIScale();
                    SaveUISettings();
                }
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor(2);
    ImGui::Dummy(ImVec2(0, 10));

    // ========== UI 背景 / Background ==========
    ImGui::Separator();
    ImGui::Text("%s", g_language == Language::CN ? "\xe8\x83\x8c\xe6\x99\xaf" : "Background");
    ImGui::Dummy(ImVec2(0, 5));

    // File picker row (separate from opacity slider for better scaling)
    if (ImGui::Button(g_language == Language::CN ? "\xe9\x80\x89\xe6\x8b\xa9\xe5\x9b\xbe\xe7\x89\x87..." : "Select Image...")) {
        char filebuf[MAX_PATH] = {0};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
        ofn.lpstrFile = filebuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn)) {
            CopyAndLoadBGImage(std::string(filebuf));
        }
    }
    ImGui::SameLine();
    if (bg_texture_id_) {
        if (ImGui::SmallButton(g_language == Language::CN ? "\xe7\xa7\xbb\xe9\x99\xa4" : "Remove")) {
            UnloadBGTexture();
            char xp_root[512] = {0};
            XPLMGetSystemPath(xp_root);
            std::string tex_path = std::string(xp_root) + "Resources\\plugins\\ISFP_xLink\\Data\\Texture\\ui_background.png";
            std::filesystem::remove(tex_path);
            SaveUISettings();
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "  %s (%dx%d)",
            g_language == Language::CN ? "\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd" : "Loaded", bg_tex_w_, bg_tex_h_);
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  %s",
            g_language == Language::CN ? "\xe6\x9c\xaa\xe8\xae\xbe\xe7\xbd\xae\xe8\x83\x8c\xe6\x99\xaf" : "No background set");
    }

    // Opacity slider on its own line
    ImGui::Dummy(ImVec2(0, 4));
    // Override FrameBg/FrameBgHovered with slider track colors for the slider control
    float st_r2 = 0.15f, st_g2 = 0.15f, st_b2 = 0.20f;
    float sth_r2 = 0.20f, sth_g2 = 0.20f, sth_b2 = 0.28f;
    if (theme_preset_ >= 0 && theme_preset_ < 5) {
        const auto& cp2 = kPresets[theme_preset_];
        st_r2 = cp2.slider_track[0]; st_g2 = cp2.slider_track[1]; st_b2 = cp2.slider_track[2];
        sth_r2 = cp2.slider_track_hover[0]; sth_g2 = cp2.slider_track_hover[1]; sth_b2 = cp2.slider_track_hover[2];
    } else if (theme_preset_ < 0) {
        st_r2 = theme_custom_slider_track_[0]; st_g2 = theme_custom_slider_track_[1]; st_b2 = theme_custom_slider_track_[2];
        sth_r2 = theme_custom_slider_track_hover_[0]; sth_g2 = theme_custom_slider_track_hover_[1]; sth_b2 = theme_custom_slider_track_hover_[2];
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(st_r2, st_g2, st_b2, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(sth_r2, sth_g2, sth_b2, 1.0f));
    ImGui::Text("%s", g_language == Language::CN ? "\xe9\x80\x8f\xe6\x98\x8e\xe5\xba\xa6" : "Opacity");
    ImGui::SameLine();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 50.0f);
    if (ImGui::SliderFloat("##bg_alpha", &bg_alpha_, 0.0f, 1.0f, "", ImGuiSliderFlags_NoInput)) {
        SaveUISettings();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    char alpha_label[16];
    snprintf(alpha_label, sizeof(alpha_label), "%d%%", (int)(bg_alpha_ * 100.0f + 0.5f));
    ImGui::Text("%s", alpha_label);
    ImGui::Dummy(ImVec2(0, 10));

    // ========== 功能 / Features ========== (中间)
    ImGui::Separator();
    ImGui::Text("%s", g_language == Language::CN ? "\xe5\x8a\x9f\xe8\x83\xbd" : "Features");
    ImGui::Dummy(ImVec2(0, 5));

    if (ImGui::Checkbox(GetText(StringID::HideUIOnStartup), &hide_on_startup_)) {
        SaveUISettings();
    }
    ImGui::Dummy(ImVec2(0, 5));

    bool fsd = g_fsd_enabled.load();
    if (ImGui::Checkbox(GetText(StringID::EnableFSD), &fsd)) {
        g_fsd_enabled = fsd;
        if (!fsd && g_network) g_network->StopServer();
        else if (fsd && g_network) g_network->StartServer(g_port);
        if (g_config) { g_config->SetBool("plugin.fsd.enabled", fsd); g_config->Save(); }
        SyncMenuCheckmarks();
    }
    ImGui::Dummy(ImVec2(0, 5));

    bool csl = g_csl_enabled.load();
    if (ImGui::Checkbox(GetText(StringID::EnableCSL), &csl)) {
        g_csl_enabled = csl;
        if (!csl && g_csl) g_csl->Stop();
        else if (csl && g_csl) g_csl->Start();
        if (g_config) { g_config->SetBool("plugin.csl.enabled", csl); g_config->Save(); }
        SyncMenuCheckmarks();
    }
    ImGui::Dummy(ImVec2(0, 5));

    bool log = g_csl_log_enabled.load();
    if (ImGui::Checkbox(GetText(StringID::EnableLog), &log)) {
        g_csl_log_enabled = log;
        Logger::SetEnabled(log);
        if (g_config) { g_config->SetBool("plugin.csl.log_enabled", log); g_config->Save(); }
        SyncMenuCheckmarks();
    }
    ImGui::Dummy(ImVec2(0, 10));

    // ========== 快捷键 / Shortcut ========== (最下面)
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 5));

    // Bordered box for hotkey settings
    float child_h = (std::max)(ImGui::GetContentRegionAvail().y, 120.0f);
    bool shortcut_box_open = ImGui::BeginChild("##shortcut_box", ImVec2(0, child_h), ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysUseWindowPadding);
    if (shortcut_box_open) {
        ImGui::Dummy(ImVec2(0, 4));

        // Hotkey entries (sortable array)
        struct HotkeyEntry {
            const char*     name;
            HotkeyBinding*  hk;
            BindTarget      target;
        };

        HotkeyEntry entries[] = {
            { g_language == Language::CN ? "\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f UI" : "Show/Hide UI",
              &g_hotkey, BindTarget::TOGGLE_UI },
            { g_language == Language::CN ? "\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f \xe8\x88\xb5\xe9\x9d\xa2\xe8\x81\x94\xe5\x8a\xa8\xe6\xa1\x86" : "Show/Hide Mouse Roller",
              &g_mouseyoke_hotkey, BindTarget::TOGGLE_MOUSE_ROLLER },
        };
        constexpr int entry_count = sizeof(entries) / sizeof(entries[0]);

        // Sort helper: locale-aware string comparison (pinyin for Chinese)
        auto locale_compare = [](const char* sa, const char* sb) -> int {
            auto to_utf16 = [](const char* s) -> std::wstring {
                int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
                if (len <= 0) return {};
                std::wstring ws(static_cast<size_t>(len) - 1, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, s, -1, &ws[0], len);
                return ws;
            };
            std::wstring wa = to_utf16(sa);
            std::wstring wb = to_utf16(sb);
            if (CompareStringEx(LOCALE_NAME_SYSTEM_DEFAULT, NORM_IGNORECASE,
                    wa.c_str(), -1, wb.c_str(), -1,
                    nullptr, nullptr, 0) == CSTR_LESS_THAN)
                return -1;
            if (CompareStringEx(LOCALE_NAME_SYSTEM_DEFAULT, NORM_IGNORECASE,
                    wa.c_str(), -1, wb.c_str(), -1,
                    nullptr, nullptr, 0) == CSTR_GREATER_THAN)
                return 1;
            return 0;
        };

        // Sort entries by current sort column and direction
        auto sort_entries = [&]() {
            std::sort(std::begin(entries), std::end(entries),
                [&](const HotkeyEntry& a, const HotkeyEntry& b) -> bool {
                    int cmp = locale_compare(a.name, b.name);
                    return sort_ascending_ ? (cmp < 0) : (cmp > 0);
                });
        };
        sort_entries();

        // 3-column table: 快捷键 | 名称 | 点击绑定
        bool table_open = ImGui::BeginTable("##shortcut_table", 3,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV);
        if (table_open) {
            // Custom clickable column headers with sort toggle
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.30f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.40f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.30f);

            // Draw custom header row
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            for (int col = 0; col < 3; col++) {
                ImGui::TableSetColumnIndex(col);
                const char* label = nullptr;
                switch (col) {
                    case 0: label = g_language == Language::CN
                        ? "\xe5\xbf\xab\xe6\x8d\xb7\xe9\x94\xae" : "Shortcut"; break;
                    case 1: label = g_language == Language::CN
                        ? "\xe5\x8a\x9f\xe8\x83\xbd\xe4\xbf\xa1\xe6\x81\xaf" : "Function"; break;
                    case 2: label = g_language == Language::CN
                        ? "\xe7\x82\xb9\xe5\x87\xbb\xe7\xbb\x91\xe5\xae\x9a" : "Bind"; break;
                }
                ImGui::PushID(col);
                // Sort indicator text (avoid Unicode glyphs that font may lack)
                std::string header_text = label;
                if (col == 1 && col == sort_column_) {
                    header_text += sort_ascending_
                        ? " [A-Z]" : " [Z-A]";
                }
                if (col == 1) {
                    // Full-width button spanning the column to the border
                    ImVec2 btn_size(ImGui::GetContentRegionAvail().x, 0.0f);
                    if (ImGui::Button(header_text.c_str(), btn_size)) {
                        sort_ascending_ = !sort_ascending_;
                        sort_entries();
                    }
                } else {
                    // Left-aligned with small padding
                    float px = ImGui::GetCursorPosX() + 8.0f;
                    ImGui::SetCursorPosX(px);
                    ImGui::Text("%s", header_text.c_str());
                }
                ImGui::PopID();
            }

            for (int i = 0; i < entry_count; i++) {
                ImGui::TableNextRow();
                HotkeyBinding& hk = *entries[i].hk;

                // Col 0: shortcut key (left-aligned with padding)
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                {
                    float px = ImGui::GetCursorPosX() + 8.0f;
                    ImGui::SetCursorPosX(px);
                    if (hk.vkey == 0)
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s",
                            GetText(StringID::HotkeyUnbound));
                    else
                        ImGui::Text("%s", GetHotkeyDisplayName(hk).c_str());
                }

                // Col 1: action name
                ImGui::TableSetColumnIndex(1);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", entries[i].name);

                // Col 2: bind button
                ImGui::TableSetColumnIndex(2);
                ImGui::PushID(entries[i].name);
                if (ImGui::Button(GetText(StringID::HotkeyBind))) {
                    pending_hotkey_ = HotkeyBinding();
                    pending_hotkey_.vkey = 0;
                    bind_captured_ = false;
                    bind_cleared_ = false;
                    bind_just_entered_ = true;
                    bind_prev_key_ = 0;
                    bind_peak_shift_ = false;
                    bind_peak_ctrl_ = false;
                    bind_peak_alt_ = false;
                    binding_mode_ = true;
                    binding_target_ = entries[i].target;
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, 10));
}


} // namespace ISFP
