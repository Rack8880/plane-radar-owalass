#pragma once

#include <cstddef>
#include <cstdint>

namespace services::location {

constexpr size_t kMaxSavedLocations = 3;
constexpr uint8_t kNoActiveSlot = 0xFF;
constexpr size_t kLocationNameMax = 16;

struct SavedLocation {
  char name[kLocationNameMax + 1];
  double lat;
  double lon;
  bool valid;
};

/** Load saved lat/lon and location slots from NVS, or use config defaults.
 *  Call once before WiFi setup. */
void init();

/** Active radar centre (a saved slot's coordinates, or typed-in ones). */
double lat();
double lon();

/** Parse portal strings, validate, persist to NVS, update runtime values.
 *  Clears the active saved slot (custom coordinates now in use). */
bool saveFromStrings(const char* lat_str, const char* lon_str);
/** Same as saveFromStrings with numbers. */
bool saveLatLon(double lat, double lon);

/** Clear stored coordinates (e.g. with WiFi credential reset). */
void clear();

// --- Saved location slots (up to kMaxSavedLocations) ---

const SavedLocation* savedLocations();
/** Index of the slot in use, or kNoActiveSlot for typed-in coordinates. */
uint8_t activeLocationIndex();
/** Save/overwrite a slot. Validates lat/lon; name is trimmed to fit. */
bool saveLocation(uint8_t slot, const char* name, double lat, double lon);
/** Centre the radar on a saved slot. */
bool useLocation(uint8_t slot);
/** Remove a slot (others keep their positions). */
void deleteLocation(uint8_t slot);

}  // namespace services::location
