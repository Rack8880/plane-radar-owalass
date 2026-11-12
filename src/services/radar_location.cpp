#include "services/radar_location.h"

#include <Arduino.h>
#include <Preferences.h>
#include "services/prefs_util.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"

namespace services::location {

namespace {

constexpr char kPrefsNamespace[] = "radar";
constexpr char kKeyLat[] = "lat";
constexpr char kKeyLon[] = "lon";
constexpr char kKeyActive[] = "active";

double s_lat = config::kDefaultRadarLat;
double s_lon = config::kDefaultRadarLon;
SavedLocation s_slots[kMaxSavedLocations] = {};
uint8_t s_active = kNoActiveSlot;

/** Slot keys: "s0"/"s1"/"s2" hold the whole SavedLocation record. */
void slotKey(uint8_t slot, char* buf, size_t len) {
  snprintf(buf, len, "s%u", static_cast<unsigned>(slot));
}

bool parseCoord(const char* text, double* out) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const double v = strtod(text, &end);
  if (end == text || (end != nullptr && *end != '\0')) {
    return false;
  }
  *out = v;
  return true;
}

bool validLatLon(double lat, double lon) {
  return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

void persist(double lat, double lon, uint8_t active) {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.putDouble(kKeyLat, lat);
  prefs.putDouble(kKeyLon, lon);
  prefs.putUChar(kKeyActive, active);
  prefs.end();
  s_lat = lat;
  s_lon = lon;
  s_active = active;
}

void persistSlot(uint8_t slot) {
  char key[4];
  slotKey(slot, key, sizeof(key));
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  if (s_slots[slot].valid) {
    prefs.putBytes(key, &s_slots[slot], sizeof(SavedLocation));
  } else {
    services::prefs::removeIfPresent(prefs, key);
  }
  prefs.end();
}

}  // namespace

void init() {
  Preferences prefs;
  if (!services::prefs::openForRead(prefs, kPrefsNamespace)) {
    return;  // nothing saved yet: config defaults
  }
  if (prefs.isKey(kKeyLat) && prefs.isKey(kKeyLon)) {
    const double lat = prefs.getDouble(kKeyLat, config::kDefaultRadarLat);
    const double lon = prefs.getDouble(kKeyLon, config::kDefaultRadarLon);
    if (validLatLon(lat, lon)) {
      s_lat = lat;
      s_lon = lon;
    }
  }
  for (uint8_t i = 0; i < kMaxSavedLocations; ++i) {
    char key[4];
    slotKey(i, key, sizeof(key));
    SavedLocation loc = {};
    if (services::prefs::blobLength(prefs, key) == sizeof(loc) &&
        prefs.getBytes(key, &loc, sizeof(loc)) == sizeof(loc) && loc.valid &&
        validLatLon(loc.lat, loc.lon)) {
      loc.name[kLocationNameMax] = '\0';
      s_slots[i] = loc;
    }
  }
  const uint8_t active = prefs.getUChar(kKeyActive, kNoActiveSlot);
  s_active = (active < kMaxSavedLocations && s_slots[active].valid)
                 ? active
                 : kNoActiveSlot;
  prefs.end();
}

double lat() { return s_lat; }

double lon() { return s_lon; }

bool saveLatLon(double lat, double lon) {
  if (!validLatLon(lat, lon)) {
    return false;
  }
  // Unchanged coordinates (e.g. the settings page re-saving the form) keep
  // the active saved location highlighted.
  const bool unchanged = fabs(lat - s_lat) < 5e-7 && fabs(lon - s_lon) < 5e-7;
  const uint8_t active = unchanged ? s_active : kNoActiveSlot;
  persist(lat, lon, active);
  Serial.printf("Radar location saved: %.6f, %.6f\n", lat, lon);
  return true;
}

bool saveFromStrings(const char* lat_str, const char* lon_str) {
  double lat = 0.0;
  double lon = 0.0;
  if (!parseCoord(lat_str, &lat) || !parseCoord(lon_str, &lon)) {
    return false;
  }
  return saveLatLon(lat, lon);
}

void clear() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  services::prefs::removeIfPresent(prefs, kKeyLat);
  services::prefs::removeIfPresent(prefs, kKeyLon);
  services::prefs::removeIfPresent(prefs, kKeyActive);
  prefs.end();
  s_lat = config::kDefaultRadarLat;
  s_lon = config::kDefaultRadarLon;
  s_active = kNoActiveSlot;
}

const SavedLocation* savedLocations() { return s_slots; }

uint8_t activeLocationIndex() { return s_active; }

bool saveLocation(uint8_t slot, const char* name, double lat, double lon) {
  if (slot >= kMaxSavedLocations || !validLatLon(lat, lon)) {
    return false;
  }
  SavedLocation loc = {};
  strncpy(loc.name, name != nullptr ? name : "", kLocationNameMax);
  loc.name[kLocationNameMax] = '\0';
  loc.lat = lat;
  loc.lon = lon;
  loc.valid = true;
  s_slots[slot] = loc;
  persistSlot(slot);
  if (s_active == slot) {
    persist(lat, lon, slot);  // editing the slot in use moves the radar too
  }
  return true;
}

bool useLocation(uint8_t slot) {
  if (slot >= kMaxSavedLocations || !s_slots[slot].valid) {
    return false;
  }
  persist(s_slots[slot].lat, s_slots[slot].lon, slot);
  Serial.printf("Radar location: saved slot %u\n", static_cast<unsigned>(slot));
  return true;
}

void deleteLocation(uint8_t slot) {
  if (slot >= kMaxSavedLocations) {
    return;
  }
  s_slots[slot] = SavedLocation{};
  persistSlot(slot);
  if (s_active == slot) {
    // Keep the radar where it is; it just isn't a saved slot any more.
    persist(s_lat, s_lon, kNoActiveSlot);
  }
}

}  // namespace services::location
