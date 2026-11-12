#include "ui/radar_colors.h"

#include <Arduino.h>
#include <Preferences.h>
#include "services/prefs_util.h"

#include <cstdio>
#include <cstring>
#include <utility>

namespace ui::radar::colors {

const SlotInfo kSlots[kSlotCount] = {
    {"bg", "Background"},
    {"grid", "Rings & grid"},
    {"plane", "Planes"},
    {"heading", "Heading line"},
    {"text", "Text"},
    {"type", "Aircraft type"},
    {"alt", "Altitude"},
    {"runway", "Runways"},
    {"airport", "Airport codes"},
    {"centre", "Centre dot"},
};

// Order of each row: background, grid, plane, heading, text, type, altitude,
// runway, airport code, centre dot.
//
// The frame buffer is RGB332 and this panel swaps red and blue, so on the
// glass red has 4 levels (00 55 AA FF) and green/blue have 8
// (00 24 49 6D 92 B6 DB FF). Preset values sit exactly on those levels so the
// browser preview matches the screen.
const Theme kPresets[] = {
    // The original look, exactly as the original firmware showed it on this
    // panel (its type/altitude/runway colours appeared red/blue-swapped).
    {"classic", "Classic green",
     {0x040A1C, 0x106410, 0xFF0000, 0xFF00FF, 0xFFFFFF, 0x00C8FF, 0xFFC85A,
      0xAA9638, 0xE6D26E, 0xFFFFFF}},
    // Deep purple background, violet rings, gold planes, pink/lilac labels.
    {"purple", "Purple",
     {0x55006D, 0xAA6DFF, 0xFFDB00, 0xFF6DDB, 0xFFDBFF, 0xAA92FF, 0xFF92DB,
      0xAA49DB, 0xAAB6FF, 0xFFDB00}},
    // Old-style amber scope: everything in warm orange/yellow tones.
    {"amber", "Amber",
     {0x000000, 0xB66D00, 0xFFFFFF, 0xFFDB00, 0xFFB600, 0xFFDB55, 0xDB9200,
      0x924900, 0xDB9255, 0xFFB600}},
    // Navy with cyan rings, orange planes, aqua labels.
    {"ocean", "Ocean",
     {0x000055, 0x00B6FF, 0xFF6D00, 0xFFDB00, 0xFFFFFF, 0x6DDBFF, 0x92FFAA,
      0x0092AA, 0xB6FFFF, 0xFFFFFF}},
    // Red-on-black night mode (easy on dark-adapted eyes).
    {"night", "Night red",
     {0x000000, 0x6D0000, 0xFF9200, 0xDB2400, 0xFF6D55, 0xDB4900, 0xFF9255,
      0x6D2400, 0xB64900, 0xFF6D00}},
    // Greys and white.
    {"mono", "Monochrome",
     {0x000000, 0x556D6D, 0xFFFFFF, 0xAAB6B6, 0xFFFFFF, 0xAADBDB, 0xAAB6B6,
      0x554949, 0xAAB6B6, 0xFFFFFF}},
    // Black, grey, gold, pink planes.
    {"seahawks", "Seahawks",
     {0x000000, 0x556D6D, 0xFF6DB6, 0xFFDB00, 0xFFFFFF, 0xFFDB49, 0xAAB6B6,
      0x554949, 0xFFDB00, 0xFF6DB6}},
    // Black, gold, red, blue.
    {"panthers", "Panthers",
     {0x000000, 0xAA6D00, 0xFFDB00, 0xFF2400, 0xFFFFFF, 0xFFDB49, 0x55B6FF,
      0x0049B6, 0xFF4949, 0xFF2400}},
    // Blue, yellow, white.
    {"knights", "Knights",
     {0x000049, 0x006DFF, 0xFFDB00, 0xFFFFFF, 0xFFFFFF, 0xFFDB49, 0xAADBFF,
      0x0049B6, 0xFFDB00, 0xFFDB00}},
};
const size_t kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

namespace {

constexpr char kPrefsNamespace[] = "radarcolor";
constexpr char kKeyPreset[] = "preset";
constexpr char kKeyColours[] = "cols";  // uint32_t[kSlotCount]
// Earlier 3-colour version of this theme (migrated on load).
constexpr char kKeyOldBg[] = "bg";
constexpr char kKeyOldGrid[] = "grid";
constexpr char kKeyOldPlane[] = "plane";
constexpr char kKeySwap[] = "swapRB";
constexpr char kCustomId[] = "custom";

char s_preset[12] = "classic";
uint32_t s_rgb[kSlotCount];
bool s_swap = true;

const Theme* findPreset(const char* id) {
  if (id != nullptr) {
    for (size_t i = 0; i < kPresetCount; ++i) {
      if (strcmp(id, kPresets[i].id) == 0) {
        return &kPresets[i];
      }
    }
  }
  return nullptr;
}

const char* canonicalId(const char* id) {
  const Theme* t = findPreset(id);
  return t != nullptr ? t->id : kCustomId;
}

void loadPreset(const Theme& t) { memcpy(s_rgb, t.rgb, sizeof(s_rgb)); }

}  // namespace

void init() {
  loadPreset(kPresets[0]);
  Preferences prefs;
  if (!services::prefs::openForRead(prefs, kPrefsNamespace)) {
    return;  // nothing saved yet: classic
  }
  char id[sizeof(s_preset)] = "";
  if (prefs.isKey(kKeyPreset)) {
    prefs.getString(kKeyPreset, id, sizeof(id));
  }
  strncpy(s_preset, canonicalId(id), sizeof(s_preset) - 1);
  s_preset[sizeof(s_preset) - 1] = '\0';

  uint32_t saved[kSlotCount];
  if (services::prefs::blobLength(prefs, kKeyColours) == sizeof(saved) &&
      prefs.getBytes(kKeyColours, saved, sizeof(saved)) == sizeof(saved)) {
    for (size_t i = 0; i < kSlotCount; ++i) {
      s_rgb[i] = saved[i] & 0xFFFFFF;
    }
  } else if (prefs.isKey(kKeyOldBg)) {
    // Theme saved by the 3-colour version: new colours come from its preset.
    const Theme* t = findPreset(s_preset);
    loadPreset(t != nullptr ? *t : kPresets[0]);
    s_rgb[kBackground] = prefs.getUInt(kKeyOldBg, s_rgb[kBackground]) & 0xFFFFFF;
    s_rgb[kGrid] = prefs.getUInt(kKeyOldGrid, s_rgb[kGrid]) & 0xFFFFFF;
    s_rgb[kPlane] = prefs.getUInt(kKeyOldPlane, s_rgb[kPlane]) & 0xFFFFFF;
  }
  s_swap = prefs.getBool(kKeySwap, true);
  prefs.end();
}

uint32_t get(Slot slot) { return slot < kSlotCount ? s_rgb[slot] : 0; }
const char* presetId() { return s_preset; }
bool swapRedBlue() { return s_swap; }

void save(const char* preset_id, const uint32_t* rgb, bool swap_red_blue) {
  for (size_t i = 0; i < kSlotCount; ++i) {
    s_rgb[i] = rgb[i] & 0xFFFFFF;
  }
  s_swap = swap_red_blue;
  strncpy(s_preset, canonicalId(preset_id), sizeof(s_preset) - 1);
  s_preset[sizeof(s_preset) - 1] = '\0';

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  prefs.putBytes(kKeyColours, s_rgb, sizeof(s_rgb));
  prefs.putString(kKeyPreset, s_preset);
  prefs.putBool(kKeySwap, s_swap);
  services::prefs::removeIfPresent(prefs, kKeyOldBg);
  services::prefs::removeIfPresent(prefs, kKeyOldGrid);
  services::prefs::removeIfPresent(prefs, kKeyOldPlane);
  prefs.end();
  Serial.printf("Theme: %s\n", s_preset);
}

uint16_t toPanel565(uint32_t rgb) {
  uint8_t r = static_cast<uint8_t>((rgb >> 16) & 0xFF);
  const uint8_t g = static_cast<uint8_t>((rgb >> 8) & 0xFF);
  uint8_t b = static_cast<uint8_t>(rgb & 0xFF);
  if (s_swap) {
    std::swap(r, b);
  }
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint32_t blend(uint32_t a, uint32_t b, uint8_t amount) {
  uint32_t out = 0;
  for (int shift = 0; shift <= 16; shift += 8) {
    const uint32_t ca = (a >> shift) & 0xFF;
    const uint32_t cb = (b >> shift) & 0xFF;
    const uint32_t c = (ca * (255 - amount) + cb * amount + 127) / 255;
    out |= c << shift;
  }
  return out;
}

bool parseHex(const char* text, uint32_t* out) {
  if (text == nullptr) {
    return false;
  }
  if (text[0] == '#') {
    ++text;
  }
  if (strlen(text) != 6) {
    return false;
  }
  uint32_t v = 0;
  for (int i = 0; i < 6; ++i) {
    const char c = text[i];
    v <<= 4;
    if (c >= '0' && c <= '9') {
      v |= static_cast<uint32_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      v |= static_cast<uint32_t>(c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      v |= static_cast<uint32_t>(c - 'A' + 10);
    } else {
      return false;
    }
  }
  *out = v;
  return true;
}

void formatHex(uint32_t rgb, char* buf, size_t len) {
  snprintf(buf, len, "#%06lx", static_cast<unsigned long>(rgb & 0xFFFFFF));
}

}  // namespace ui::radar::colors
