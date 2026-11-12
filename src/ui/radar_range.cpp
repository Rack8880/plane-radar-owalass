#include "ui/radar_range.h"

#include "ui/radar_theme.h"

#include <Arduino.h>
#include <Preferences.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ui::radar {

namespace {

constexpr char kPrefsNamespace[] = "planeradar";
constexpr char kPrefsRangeKey[] = "rangeIdx";
constexpr char kPrefsMilesKey[] = "useMiles";
constexpr char kPrefsRunwaysKey[] = "showRwys";
constexpr char kPrefsPresetsKey[] = "presets";
constexpr char kPrefsCountKey[] = "showCount";
constexpr char kPrefsTagCsKey[] = "tagCs";
constexpr char kPrefsTagTypeKey[] = "tagType";
constexpr char kPrefsTagAltKey[] = "tagAlt";
constexpr char kPrefsMetersKey[] = "useMeters";
constexpr char kPrefsWatchKey[] = "watch";
constexpr uint8_t kDefaultRangeIndex = 1;  // 10 km ring
constexpr float kKmPerMile = 1.609344f;

Preferences s_prefs;
uint8_t s_range_index = kDefaultRangeIndex;
RangePreset s_presets[kRangePresetCount] = {
    kDefaultRangePresets[0], kDefaultRangePresets[1], kDefaultRangePresets[2],
    kDefaultRangePresets[3]};
bool s_use_miles = false;
bool s_show_runways = true;
bool s_show_count = false;
bool s_tag_callsign = true;
bool s_tag_type = true;
bool s_tag_alt = true;
bool s_use_meters = false;
char s_watch[kWatchMaxLen + 1] = "";

void saveRangeIndex() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putUChar(kPrefsRangeKey, s_range_index);
  s_prefs.end();
}

void saveBool(const char* key, bool value) {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(key, value);
  s_prefs.end();
}

bool portalCheckboxChecked(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  // WiFiManager checkbox submits its value= attribute ("T", or "F" if we prefilled F).
  if ((value[0] == 'T' || value[0] == 't' || value[0] == 'F' || value[0] == 'f') &&
      value[1] == '\0') {
    return true;
  }
  return strcmp(value, "on") == 0;
}

bool presetsValid(const float* km, size_t count) {
  if (count != kRangePresetCount) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!std::isfinite(km[i]) || km[i] < kMinPresetKm || km[i] > kMaxPresetKm) {
      return false;
    }
    if (i > 0 && km[i] <= km[i - 1]) {
      return false;
    }
  }
  return true;
}

void applyPresets(const float* km) {
  for (size_t i = 0; i < kRangePresetCount; ++i) {
    s_presets[i].ring3_km = km[i];
    s_presets[i].outer_km = km[i] * kRing3ToOuterKm;
  }
}

/** Upper-case, drop spaces, collapse empty items, cap length. */
void normaliseWatch(const char* in, char* out, size_t out_len) {
  size_t n = 0;
  bool last_comma = true;  // suppress leading commas
  for (const char* p = in; p != nullptr && *p != '\0' && n + 1 < out_len; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == ',') {
      if (!last_comma) {
        out[n++] = ',';
        last_comma = true;
      }
      continue;
    }
    if (!isalnum(c)) {
      continue;
    }
    out[n++] = static_cast<char>(toupper(c));
    last_comma = false;
  }
  while (n > 0 && out[n - 1] == ',') {
    --n;
  }
  out[n] = '\0';
}

}  // namespace

void rangeInit() {
  if (!s_prefs.begin(kPrefsNamespace, true)) {
    return;
  }
  float km[kRangePresetCount];
  if (s_prefs.getBytesLength(kPrefsPresetsKey) == sizeof(km) &&
      s_prefs.getBytes(kPrefsPresetsKey, km, sizeof(km)) == sizeof(km) &&
      presetsValid(km, kRangePresetCount)) {
    applyPresets(km);
  }
  const uint8_t saved = s_prefs.getUChar(kPrefsRangeKey, kDefaultRangeIndex);
  s_range_index =
      (saved < kRangePresetCount) ? saved : kDefaultRangeIndex;
  s_use_miles = s_prefs.getBool(kPrefsMilesKey, false);
  s_show_runways = s_prefs.getBool(kPrefsRunwaysKey, true);
  s_show_count = s_prefs.getBool(kPrefsCountKey, false);
  s_tag_callsign = s_prefs.getBool(kPrefsTagCsKey, true);
  s_tag_type = s_prefs.getBool(kPrefsTagTypeKey, true);
  s_tag_alt = s_prefs.getBool(kPrefsTagAltKey, true);
  s_use_meters = s_prefs.getBool(kPrefsMetersKey, false);
  if (s_prefs.isKey(kPrefsWatchKey)) {
    char raw[kWatchMaxLen + 1] = "";
    s_prefs.getString(kPrefsWatchKey, raw, sizeof(raw));
    normaliseWatch(raw, s_watch, sizeof(s_watch));
  }
  s_prefs.end();
}

void rangeNext() {
  s_range_index = static_cast<uint8_t>((s_range_index + 1) % kRangePresetCount);
  saveRangeIndex();
}

void rangeSet(uint8_t index) {
  if (index >= kRangePresetCount || index == s_range_index) {
    return;
  }
  s_range_index = index;
  saveRangeIndex();
}

const RangePreset& rangeCurrent() { return s_presets[s_range_index]; }

uint8_t rangeIndex() { return s_range_index; }

const RangePreset* rangePresets() { return s_presets; }

bool rangeSavePresets(const float* ring3_km, size_t count) {
  if (!presetsValid(ring3_km, count)) {
    return false;
  }
  applyPresets(ring3_km);
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putBytes(kPrefsPresetsKey, ring3_km, sizeof(float) * kRangePresetCount);
    s_prefs.end();
  }
  return true;
}

float fetchRadiusKm() {
  const float outer_km = rangeCurrent().outer_km;
  const float screen_r_px =
      static_cast<float>(kCenterX - kBeyondRingScreenMarginPx);
  return outer_km * (screen_r_px / static_cast<float>(kGridOuterRadius));
}

bool useMiles() { return s_use_miles; }
bool showRunways() { return s_show_runways; }
bool showAircraftCount() { return s_show_count; }
bool showTagCallsign() { return s_tag_callsign; }
bool showTagType() { return s_tag_type; }
bool showTagAlt() { return s_tag_alt; }
bool useMeters() { return s_use_meters; }
const char* watchList() { return s_watch; }

void setUseMiles(bool on) {
  s_use_miles = on;
  saveBool(kPrefsMilesKey, on);
}
void setShowRunways(bool on) {
  s_show_runways = on;
  saveBool(kPrefsRunwaysKey, on);
}
void setShowAircraftCount(bool on) {
  s_show_count = on;
  saveBool(kPrefsCountKey, on);
}
void setShowTagCallsign(bool on) {
  s_tag_callsign = on;
  saveBool(kPrefsTagCsKey, on);
}
void setShowTagType(bool on) {
  s_tag_type = on;
  saveBool(kPrefsTagTypeKey, on);
}
void setShowTagAlt(bool on) {
  s_tag_alt = on;
  saveBool(kPrefsTagAltKey, on);
}
void setUseMeters(bool on) {
  s_use_meters = on;
  saveBool(kPrefsMetersKey, on);
}

void setWatchList(const char* value) {
  normaliseWatch(value, s_watch, sizeof(s_watch));
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putString(kPrefsWatchKey, s_watch);
    s_prefs.end();
  }
}

bool matchesWatchList(const char* callsign) {
  if (s_watch[0] == '\0') {
    return true;  // filter off: everything matches
  }
  if (callsign == nullptr || callsign[0] == '\0') {
    return false;
  }
  const char* p = s_watch;
  while (*p != '\0') {
    const char* end = p;
    while (*end != '\0' && *end != ',') {
      ++end;
    }
    const size_t n = static_cast<size_t>(end - p);
    if (n > 0 && strncasecmp(callsign, p, n) == 0) {
      return true;
    }
    p = (*end == ',') ? end + 1 : end;
  }
  return false;
}

void saveMilesFromPortal(const char* checkbox_value) {
  setUseMiles(portalCheckboxChecked(checkbox_value));
  Serial.printf("Distance units: %s\n", s_use_miles ? "miles" : "km");
}

void saveRunwaysFromPortal(const char* checkbox_value) {
  setShowRunways(portalCheckboxChecked(checkbox_value));
  Serial.printf("Runway overlay: %s\n", s_show_runways ? "on" : "off");
}

void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles) {
  // Whole numbers as before; one decimal only for small fractional custom
  // presets (e.g. 2.5km), or when miles would otherwise round down to 0.
  const float v = use_miles ? ring3_km / kKmPerMile : ring3_km;
  const char* unit = use_miles ? "mi" : "km";
  const bool fractional_km =
      !use_miles && v < 9.95f && fabsf(v - roundf(v)) >= 0.05f;
  if (fractional_km || v < 0.5f) {
    snprintf(buf, len, "%.1f%s", v, unit);
  } else {
    snprintf(buf, len, "%d%s", static_cast<int>(lroundf(v)), unit);
  }
}

void formatCurrentRing3Label(char* buf, size_t len) {
  formatRing3Label(buf, len, rangeCurrent().ring3_km, s_use_miles);
}

void unitsReset() {
  s_use_miles = false;
  s_show_runways = true;
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.remove(kPrefsMilesKey);
    s_prefs.remove(kPrefsRunwaysKey);
    s_prefs.end();
  }
}

}  // namespace ui::radar
