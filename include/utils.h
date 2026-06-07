#ifndef UTILS_H
#define UTILS_H

#include <string>
#include "isfp_plugin.h"

namespace ISFP {

// Convert HotkeyBinding to human-readable display string (e.g. "Shift+I")
std::string GetHotkeyDisplayName(const HotkeyBinding& hk);

// Haversine formula: great-circle distance between two lat/lon points (km)
double HaversineDistance(double lat1, double lon1, double lat2, double lon2);

// Extract airline code from callsign (e.g. "CCA1234" -> "CCA")
std::string ExtractAirlineCode(const std::string& callsign);

// Predict aircraft position from groundspeed and heading (delta_time in seconds)
void PredictAircraftPosition(double& lat, double& lon,
                             float heading, float groundspeed,
                             double delta_time);

// Check whether game keyboard input should be blocked (e.g. during text input or key binding)
bool IsGameInputBlocked();

} // namespace ISFP

#endif // UTILS_H
