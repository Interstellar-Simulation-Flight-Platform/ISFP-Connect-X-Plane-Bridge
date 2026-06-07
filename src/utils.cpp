#define _USE_MATH_DEFINES
#include "utils.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <cctype>

#include "imgui.h"
#include "logger.h"

namespace ISFP {

// ==================== Hotkey Display ====================
std::string GetHotkeyDisplayName(const HotkeyBinding& hk) {
    // No key captured yet
    if (hk.vkey == 0)
        return "...";

    std::string name;
    if (hk.ctrl)  name += "Ctrl+";
    if (hk.shift) name += "Shift+";
    if (hk.alt)   name += "Alt+";

    // Convert virtual key to readable name via Windows API
    char key_name[128] = {};
    UINT scan_code = MapVirtualKeyA((UINT)hk.vkey, MAPVK_VK_TO_VSC);
    LONG lParam = (scan_code & 0xFF) << 16;
    if (GetKeyNameTextA(lParam, key_name, sizeof(key_name)) > 0) {
        name += key_name;
    } else {
        // Fallback: use common VK mappings
        switch (hk.vkey) {
            case VK_SPACE:  name += "Space"; break;
            case VK_RETURN: name += "Enter"; break;
            case VK_TAB:    name += "Tab";   break;
            case VK_BACK:   name += "Backspace"; break;
            case VK_DELETE: name += "Delete"; break;
            case VK_INSERT: name += "Insert"; break;
            case VK_HOME:   name += "Home";   break;
            case VK_END:    name += "End";    break;
            case VK_PRIOR:  name += "PageUp"; break;
            case VK_NEXT:   name += "PageDown"; break;
            case VK_LEFT:   name += "Left";   break;
            case VK_RIGHT:  name += "Right";  break;
            case VK_UP:     name += "Up";     break;
            case VK_DOWN:   name += "Down";   break;
            case VK_F1: case VK_F2: case VK_F3: case VK_F4:
            case VK_F5: case VK_F6: case VK_F7: case VK_F8:
            case VK_F9: case VK_F10: case VK_F11: case VK_F12:
                name += "F" + std::to_string(hk.vkey - VK_F1 + 1);
                break;
            default:
                if (hk.vkey >= 'A' && hk.vkey <= 'Z')
                    name += (char)hk.vkey;
                else if (hk.vkey >= '0' && hk.vkey <= '9')
                    name += (char)hk.vkey;
                else
                    name += "Key(" + std::to_string(hk.vkey) + ")";
                break;
        }
    }
    return name;
}

// ==================== Haversine Distance ====================
double HaversineDistance(double lat1, double lon1, double lat2, double lon2) {
    const double EARTH_RADIUS = 6371.0;
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon1 - lon2) * M_PI / 180.0;

    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}

// ==================== Airline Code Extraction ====================
std::string ExtractAirlineCode(const std::string& callsign) {
    std::string code;
    for (char ch : callsign) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            code += ch;
        } else {
            break;
        }
    }
    return code;
}

// ==================== Aircraft Position Prediction ====================
void PredictAircraftPosition(double& lat, double& lon,
                             float heading, float groundspeed,
                             double delta_time) {
    const double KNOTS_TO_MPS = 0.514444;
    double speed_mps = groundspeed * KNOTS_TO_MPS;

    const double DEG_TO_METER = 111319.9;
    double heading_rad = heading * M_PI / 180.0;

    double distance = speed_mps * delta_time;
    double d_lat = (distance * cos(heading_rad)) / DEG_TO_METER;
    double d_lon = (distance * sin(heading_rad)) / (DEG_TO_METER * cos(lat * M_PI / 180.0));

    lat += d_lat;
    lon += d_lon;
}

// ==================== Game Input Blocking ====================
bool IsGameInputBlocked() {
    // Check if ImGui wants keyboard capture (text input fields active)
    if (ImGui::GetIO().WantCaptureKeyboard)
        return true;
    return false;
}

} // namespace ISFP
