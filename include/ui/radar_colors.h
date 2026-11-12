#pragma once

#include <cstddef>
#include <cstdint>

/**
 * User colour theme for the radar screen, chosen on the web settings page and
 * saved to flash.
 *
 * Colours are plain 0xRRGGBB values as a browser colour picker produces them,
 * describing what should appear on the glass. The radar frame buffer is 8-bit
 * (RGB332), so the panel shows the nearest of 256 colours.
 */
namespace ui::radar::colors {

/** Every themeable colour. Order matches kSlots[]. */
enum Slot : uint8_t {
  kBackground,  // screen background
  kGrid,        // range rings and crosshairs
  kPlane,       // plane triangles and edge-of-screen dots
  kHeading,     // the line ahead of each plane (direction / speed)
  kText,        // callsigns, N/E/S/W, distance label, plane count
  kTagType,     // aircraft-type label (e.g. B738)
  kTagAlt,      // altitude label
  kRunway,      // airport runway lines
  kAirport,     // airport codes (e.g. EHAM)
  kCentre,      // the dot marking your position
  kSlotCount,
};

struct SlotInfo {
  const char* key;    // JSON key used by the web page
  const char* label;  // shown on the web page
};
extern const SlotInfo kSlots[kSlotCount];

struct Theme {
  const char* id;    // short key used by the web page, e.g. "purple"
  const char* name;  // label shown on the web page
  uint32_t rgb[kSlotCount];
};

/** Built-in presets. Index 0 ("classic") is the original look. */
extern const Theme kPresets[];
extern const size_t kPresetCount;

/** Load the saved theme from flash (defaults to "classic"). */
void init();

/** Active colour for a slot (0xRRGGBB). */
uint32_t get(Slot slot);
inline uint32_t background() { return get(kBackground); }
inline uint32_t grid() { return get(kGrid); }
inline uint32_t plane() { return get(kPlane); }

/** Preset id the colours came from, or "custom". */
const char* presetId();

/**
 * This GC9A01 module swaps red and blue on the glass; with this on (the
 * default) colours are pre-swapped so they appear as chosen.
 */
bool swapRedBlue();

/** Save all colours (preset id may be "custom") and the swap setting. */
void save(const char* preset_id, const uint32_t* rgb, bool swap_red_blue);

/** 0xRRGGBB -> RGB565 for this panel (applies the red/blue swap setting). */
uint16_t toPanel565(uint32_t rgb);
/** Slot colour as RGB565 for this panel. */
inline uint16_t panel565(Slot slot) { return toPanel565(get(slot)); }
/** Mix two 0xRRGGBB colours: amount 0 = a, 255 = b. */
uint32_t blend(uint32_t a, uint32_t b, uint8_t amount);

/** "#rrggbb" -> 0xRRGGBB. Returns false if malformed. */
bool parseHex(const char* text, uint32_t* out);
/** 0xRRGGBB -> "#rrggbb" (buf >= 8 bytes). */
void formatHex(uint32_t rgb, char* buf, size_t len);

}  // namespace ui::radar::colors
