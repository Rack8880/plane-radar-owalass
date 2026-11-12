#pragma once

#include <cstddef>
#include <cstdint>

namespace ui::radar {

/**
 * Range presets (label on ring 3 = ¾ of outer radius).
 *
 * Recommended for ADS-B on a 1.28″ display:
 *   5 km  — pattern / very local (airfield vicinity)
 *  10 km  — default; neighborhood spotting
 *  15 km  — wider local area
 *  25 km  — metro / regional picture
 *
 * Outer radius (for aircraft math) is ring-3 distance ÷ 0.75.
 *
 * The four distances can be changed from the web settings page; these are the
 * factory defaults used until then.
 */
struct RangePreset {
  /** Distance shown on ring 3 (¾ of outer radius), always stored in km. */
  float ring3_km;
  float outer_km;
};

constexpr float kRing3ToOuterKm = 4.0f / 3.0f;

constexpr RangePreset kDefaultRangePresets[] = {
    {5.0f, 5.0f * kRing3ToOuterKm},
    {10.0f, 10.0f * kRing3ToOuterKm},
    {15.0f, 15.0f * kRing3ToOuterKm},
    {25.0f, 25.0f * kRing3ToOuterKm},
};

constexpr size_t kRangePresetCount =
    sizeof(kDefaultRangePresets) / sizeof(kDefaultRangePresets[0]);

/** Allowed ring-3 distance for a user preset (km). */
constexpr float kMinPresetKm = 0.5f;
constexpr float kMaxPresetKm = 500.0f;

/** Longest watch-filter string (comma-separated callsign prefixes). */
constexpr size_t kWatchMaxLen = 48;

/** Load saved range, units and display options from flash. Call once after boot. */
void rangeInit();
/** Cycle preset and save to flash. */
void rangeNext();
/** Select a preset by index and save to flash (ignored if out of range). */
void rangeSet(uint8_t index);
const RangePreset& rangeCurrent();
uint8_t rangeIndex();
/** The four active presets (user values, or the defaults). */
const RangePreset* rangePresets();
/**
 * Replace the four ring-3 distances (km). Rejected (returns false, nothing
 * saved) unless every value is within kMinPresetKm..kMaxPresetKm and they
 * strictly increase.
 */
bool rangeSavePresets(const float* ring3_km, size_t count);
/** ADSB fetch radius (km): scaled to screen edge so beyond-ring dots have data. */
float fetchRadiusKm();

bool useMiles();
bool showRunways();
bool showAircraftCount();
bool showTagCallsign();
bool showTagType();
bool showTagAlt();
/** Altitude tag in metres instead of feet. */
bool useMeters();
/** Upper-case, comma-separated callsign prefixes; "" = filter off. */
const char* watchList();

/** Settings-page setters (each saves to flash). */
void setUseMiles(bool on);
void setShowRunways(bool on);
void setShowAircraftCount(bool on);
void setShowTagCallsign(bool on);
void setShowTagType(bool on);
void setShowTagAlt(bool on);
void setUseMeters(bool on);
/** Normalises (upper-case, no spaces, max kWatchMaxLen) and saves. */
void setWatchList(const char* value);

/** True when the callsign starts with one of the watch prefixes. */
bool matchesWatchList(const char* callsign);

/** WiFi setup-portal checkbox: "T" = miles, otherwise km. */
void saveMilesFromPortal(const char* checkbox_value);
void saveRunwaysFromPortal(const char* checkbox_value);
void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles);
void formatCurrentRing3Label(char* buf, size_t len);
/** Reset distance units to km (e.g. with WiFi credential wipe). */
void unitsReset();

}  // namespace ui::radar
