#include "ui/status_screens.h"

#include <lgfx/v1/lgfx_fonts.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "ui/radar_colors.h"

namespace {

constexpr int kLineGap = 6;
const int kCenterX = config::kDisplayWidth / 2;
const int kCenterY = config::kDisplayHeight / 2;

constexpr int kSpinnerDotCount = 10;
constexpr int kSpinnerRadius = 113;
constexpr int kSpinnerDotRadius = 2;
constexpr int kSpinnerEraseRadius = 4;
constexpr float kSpinnerStepDeg = 6.0f;

struct SpinnerDot {
  int x = 0;
  int y = 0;
  bool drawn = false;
};

char s_connecting_ssid[33];
char s_ssid_line[33];
constexpr int kConnectingTextMaxWidthPx = 220;
float s_spinner_angle_deg = -90.0f;
SpinnerDot s_spinner_dots[kSpinnerDotCount];
bool s_connecting_text_drawn = false;

constexpr auto& kGfxTitle = fonts::FreeSans18pt7b;
constexpr auto& kGfxBody = fonts::FreeSans12pt7b;
constexpr auto& kGfxDetail = fonts::Font2;
constexpr auto& kPortalGfxTitle = fonts::FreeSansBold18pt7b;
constexpr auto& kPortalGfxBody = fonts::FreeSansBold12pt7b;
constexpr auto& kPortalGfxEmphasis = fonts::FreeSansBold18pt7b;
constexpr auto& kConnectingGfxDetail = fonts::FreeSans9pt7b;

/** Yellow as this panel shows it: the module swaps red and blue, so ask for
 *  cyan when the "Swap red and blue" setting is on (same switch as the radar). */
uint16_t yellowForPanel() {
  return ui::radar::colors::swapRedBlue() ? tft.color565(0, 255, 255)
                                          : config::kColorYellow;
}

// The "Connecting to..." screen follows the Colours page theme: background,
// text, and a spinner in the rings colour (brightened) fading into the
// background. The yellow setup / error / reset screens stay yellow on purpose
// so instructions are always readable.
uint16_t connectingBg() {
  return ui::radar::colors::panel565(ui::radar::colors::kBackground);
}

uint16_t connectingText() {
  return ui::radar::colors::panel565(ui::radar::colors::kText);
}

/** Rings colour scaled up so its brightest channel is full (dim green -> bright). */
uint32_t spinnerHeadRgb() {
  const uint32_t rgb = ui::radar::colors::grid();
  const uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
  const uint32_t peak = std::max(r, std::max(g, b));
  if (peak == 0) {
    return 0x00FF00;
  }
  auto up = [peak](uint32_t c) { return std::min<uint32_t>(255, c * 255 / peak); };
  return (up(r) << 16) | (up(g) << 8) | up(b);
}

struct TextLine {
  const char* text;
  float vlw_size;
  const lgfx::GFXfont* gfx_font;
};

int lineHeightGfx(const lgfx::GFXfont* font) {
  displayFontSetBitmap(tft, font);
  return tft.fontHeight();
}

int lineHeightVlw(float size) {
  displayFontSetSmoothSize(tft, size);
  return tft.fontHeight();
}

void applyLineStyle(const TextLine& line) {
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, line.vlw_size);
  } else {
    displayFontSetBitmap(tft, line.gfx_font);
  }
}

void drawTextBlock(uint16_t bg, uint16_t fg, const TextLine* lines, size_t count) {
  tft.fillScreen(bg);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(textdatum_t::middle_center);

  int total_h = 0;
  for (size_t i = 0; i < count; ++i) {
    if (displayFontIsSmooth()) {
      total_h += lineHeightVlw(lines[i].vlw_size);
    } else {
      total_h += lineHeightGfx(lines[i].gfx_font);
    }
    if (i + 1 < count) {
      total_h += kLineGap;
    }
  }

  int y = (config::kDisplayHeight - total_h) / 2;
  for (size_t i = 0; i < count; ++i) {
    applyLineStyle(lines[i]);
    const int h =
        displayFontIsSmooth() ? lineHeightVlw(lines[i].vlw_size)
                              : lineHeightGfx(lines[i].gfx_font);
    tft.drawString(lines[i].text, kCenterX, y + h / 2);
    y += h + kLineGap;
  }
}

constexpr float kConnectingDetailVlw = 0.92f;

void applyConnectingDetailStyle() {
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, kConnectingDetailVlw);
  } else {
    displayFontSetBitmap(tft, &kConnectingGfxDetail);
  }
}

/** SSID on one line; truncate with … if wider than kConnectingTextMaxWidthPx. */
void fitSsidLine() {
  strncpy(s_ssid_line, s_connecting_ssid, sizeof(s_ssid_line) - 1);
  s_ssid_line[sizeof(s_ssid_line) - 1] = '\0';
  applyConnectingDetailStyle();
  if (tft.textWidth(s_ssid_line) <= kConnectingTextMaxWidthPx) {
    return;
  }
  const size_t len = strlen(s_connecting_ssid);
  for (size_t n = len; n > 0; --n) {
    snprintf(s_ssid_line, sizeof(s_ssid_line), "%.*s…", static_cast<int>(n),
             s_connecting_ssid);
    if (tft.textWidth(s_ssid_line) <= kConnectingTextMaxWidthPx) {
      return;
    }
  }
  strncpy(s_ssid_line, "…", sizeof(s_ssid_line) - 1);
  s_ssid_line[sizeof(s_ssid_line) - 1] = '\0';
}

void drawConnectingText() {
  tft.fillScreen(connectingBg());

  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(connectingText(), connectingBg());

  applyConnectingDetailStyle();
  const int detail_h = tft.fontHeight();
  const int total_h = detail_h * 2 + kLineGap;
  const int block_top = (config::kDisplayHeight - total_h) / 2;
  constexpr int kPanelPadY = 8;
  tft.fillRect(kCenterX - kConnectingTextMaxWidthPx / 2, block_top - kPanelPadY,
               kConnectingTextMaxWidthPx, total_h + kPanelPadY * 2, connectingBg());

  int y = block_top;
  tft.drawString("Connecting to", kCenterX, y + detail_h / 2);
  y += detail_h + kLineGap;
  tft.drawString(s_ssid_line, kCenterX, y + detail_h / 2);

  s_connecting_text_drawn = true;
}

void eraseSpinnerDots() {
  for (int i = 0; i < kSpinnerDotCount; ++i) {
    if (!s_spinner_dots[i].drawn) {
      continue;
    }
    tft.fillCircle(s_spinner_dots[i].x, s_spinner_dots[i].y, kSpinnerEraseRadius,
                   connectingBg());
    s_spinner_dots[i].drawn = false;
  }
}

void drawSpinnerDots() {
  constexpr float kDegToRad = 0.01745329252f;
  const float head_rad = s_spinner_angle_deg * kDegToRad;

  for (int i = 0; i < kSpinnerDotCount; ++i) {
    const float a = head_rad - static_cast<float>(i) * (6.283185307f / kSpinnerDotCount);
    const int x = kCenterX + static_cast<int>(std::lround(std::cos(a) * kSpinnerRadius));
    const int y = kCenterY + static_cast<int>(std::lround(std::sin(a) * kSpinnerRadius));

    // Head dot full colour, tail fading toward the background.
    const uint8_t fade = static_cast<uint8_t>(i * 22);
    const uint16_t color = ui::radar::colors::toPanel565(ui::radar::colors::blend(
        spinnerHeadRgb(), ui::radar::colors::background(), fade));
    tft.fillSmoothCircle(x, y, kSpinnerDotRadius, color);

    s_spinner_dots[i].x = x;
    s_spinner_dots[i].y = y;
    s_spinner_dots[i].drawn = true;
  }
}

}  // namespace

void statusScreenConnectingBegin(const char* ssid) {
  const char* name = (ssid != nullptr && ssid[0] != '\0') ? ssid : "network";
  strncpy(s_connecting_ssid, name, sizeof(s_connecting_ssid) - 1);
  s_connecting_ssid[sizeof(s_connecting_ssid) - 1] = '\0';
  fitSsidLine();
  s_spinner_angle_deg = -90.0f;
  for (auto& dot : s_spinner_dots) {
    dot.drawn = false;
  }
  s_connecting_text_drawn = false;
  drawConnectingText();
  drawSpinnerDots();
}

void statusScreenConnectingTick() {
  if (!s_connecting_text_drawn) {
    drawConnectingText();
  }
  eraseSpinnerDots();
  s_spinner_angle_deg += kSpinnerStepDeg;
  if (s_spinner_angle_deg >= 270.0f) {
    s_spinner_angle_deg -= 360.0f;
  }
  drawSpinnerDots();
}

void statusScreenPortal() {
  const TextLine lines[] = {
      {"Wi-Fi setup", 1.15f, &kPortalGfxTitle},
      {"1. Join network:", 1.05f, &kPortalGfxBody},
      {config::kPortalApName, 1.12f, &kPortalGfxEmphasis},
      {"2. Open in browser:", 1.05f, &kPortalGfxBody},
      {config::kPortalHostUrl, 1.12f, &kPortalGfxEmphasis},
      {"or 192.168.4.1", 1.0f, &kPortalGfxBody},
  };
  drawTextBlock(yellowForPanel(), config::kTextOnYellow, lines,
                sizeof(lines) / sizeof(lines[0]));
}

void statusScreenConnectFailed() {
  const TextLine lines[] = {
      {"Could not connect", 1.15f, &kGfxTitle},
      {"Unplug and plug", 1.0f, &kGfxBody},
      {"back in to retry,", 1.0f, &kGfxBody},
      {"or hold BOOT 3 sec", 1.0f, &kGfxBody},
      {"to set up Wi-Fi again", 1.0f, &kGfxBody},
  };
  drawTextBlock(yellowForPanel(), config::kTextOnYellow, lines,
                sizeof(lines) / sizeof(lines[0]));
}

void statusScreenWifiReset() {
  const TextLine lines[] = {
      {"Wi-Fi reset", 1.15f, &kPortalGfxTitle},
      {"Restarting...", 1.05f, &kPortalGfxBody},
  };
  drawTextBlock(yellowForPanel(), config::kTextOnYellow, lines,
                sizeof(lines) / sizeof(lines[0]));
}

namespace {

constexpr int kIpTextMaxWidthPx = 200;  // fits the round screen at mid height
constexpr int kIpCountdownY = 182;
constexpr float kIpCountdownVlw = 1.6f;

void applyIpCountdownStyle() {
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, kIpCountdownVlw);
  } else {
    displayFontSetBitmap(tft, &kPortalGfxEmphasis);
  }
}

}  // namespace

void statusScreenIpBegin(const char* ip) {
  const uint16_t bg = connectingBg();
  tft.fillScreen(bg);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(connectingText(), bg);

  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, 1.0f);
  } else {
    displayFontSetBitmap(tft, &kPortalGfxBody);
  }
  tft.drawString("Settings page:", kCenterX, 68);

  // The IP as large as fits across the screen.
  if (displayFontIsSmooth()) {
    float size = 1.5f;
    displayFontSetSmoothSize(tft, size);
    while (size > 0.8f && tft.textWidth(ip) > kIpTextMaxWidthPx) {
      size -= 0.05f;
      displayFontSetSmoothSize(tft, size);
    }
  } else {
    displayFontSetBitmap(tft, &kPortalGfxEmphasis);
    if (tft.textWidth(ip) > kIpTextMaxWidthPx) {
      displayFontSetBitmap(tft, &kPortalGfxBody);
    }
  }
  tft.drawString(ip, kCenterX, 104);

  applyConnectingDetailStyle();
  char host_line[48];
  snprintf(host_line, sizeof(host_line), "or %s", config::kPortalHostUrl);
  tft.drawString(host_line, kCenterX, 140);
}

void statusScreenIpCountdown(int seconds_left) {
  const uint16_t bg = connectingBg();
  applyIpCountdownStyle();
  const int h = tft.fontHeight();
  // Clear just the number's band, so the rest of the screen doesn't flicker.
  tft.fillRect(kCenterX - 70, kIpCountdownY - h / 2 - 3, 140, h + 6, bg);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(connectingText(), bg);
  char buf[12];
  snprintf(buf, sizeof(buf), "%d...", seconds_left);
  tft.drawString(buf, kCenterX, kIpCountdownY);
}
