#ifndef MOUSEYOKE_H
#define MOUSEYOKE_H

#include "isfp_plugin.h"
#include <atomic>

namespace ISFP {

// MouseYoke Manager
// Hides the X-Plane on-screen yoke/crosshair indicator by overriding
// sim/operation/override/override_yoke_visibility and writing 0 to
// sim/cockpit2/controls/yoke_visible_ratio.
class MouseYokeManager {
public:
    MouseYokeManager();
    ~MouseYokeManager();

    bool Initialize();
    void Shutdown();

    // Enable / disable hiding
    void SetHidden(bool hidden);
    bool IsHidden() const { return hidden_; }

private:
    bool initialized_ = false;
    bool hidden_ = false;
};

extern MouseYokeManager* g_mouseyoke;
extern std::atomic<bool> g_mouseyoke_enabled;

} // namespace ISFP

#endif // MOUSEYOKE_H
