#include "mouse_yoke.h"
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include "logger.h"

namespace ISFP {

/*
 * The clickable yoke control box (crosshair + surface indicators) is
 * rendered by X-Plane when no physical PFC yoke is detected.  Setting
 * sim/joystick/eq_pfc_yoke to 1 tells X-Plane that a PFC (Precision
 * Flight Controls) yoke is present, which suppresses the on-screen
 * yoke indicator entirely.
 *
 * This approach is based on the reference implementation:
 *   https://github.com/equdevel/MouseYoke
 */

static XPLMDataRef s_eq_pfc = nullptr;

MouseYokeManager::MouseYokeManager() = default;
MouseYokeManager::~MouseYokeManager() { Shutdown(); }

bool MouseYokeManager::Initialize() {
    if (initialized_) return true;

    s_eq_pfc = XPLMFindDataRef("sim/joystick/eq_pfc_yoke");
    if (!s_eq_pfc)
        Logger::Main("ISFP-xLink:MouseYoke:eq_pfc_yoke not found\n");

    initialized_ = true;
    Logger::Main("ISFP-xLink:MouseYoke:Manager initialized\n");
    return true;
}

void MouseYokeManager::Shutdown() {
    if (!initialized_) return;
    if (hidden_) SetHidden(false);
    s_eq_pfc = nullptr;
    initialized_ = false;
    Logger::Main("ISFP-xLink:MouseYoke:Manager shutdown\n");
}

void MouseYokeManager::SetHidden(bool hidden) {
    hidden_ = hidden;

    if (s_eq_pfc)
        XPLMSetDatai(s_eq_pfc, hidden ? 1 : 0);

    Logger::Main(hidden_
        ? "ISFP-xLink:MouseYoke:Crosshair hidden (eq_pfc_yoke=1)\n"
        : "ISFP-xLink:MouseYoke:Crosshair restored (eq_pfc_yoke=0)\n");
}

} // namespace ISFP
