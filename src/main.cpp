/**
 * Plane Radar — WiFi setup, then radar UI on the round GC9A01 display.
 */

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "hardware/display.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "services/wifi_setup.h"
#include "ui/radar_colors.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"
#include "ui/status_screens.h"

namespace {

bool g_radar_visible = false;
unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;
unsigned long g_last_redraw_ms = 0;

// "Settings page: <ip>" countdown screen shown after each Wi-Fi connect.
bool g_ip_screen = false;
unsigned long g_ip_screen_start_ms = 0;
int g_ip_screen_shown = -1;

void startIpScreen() {
  if (config::kIpScreenSeconds <= 0 || WiFi.status() != WL_CONNECTED) {
    return;
  }
  const String ip = WiFi.localIP().toString();
  Serial.printf("Showing IP address for %d s\n", config::kIpScreenSeconds);
  statusScreenIpBegin(ip.c_str());
  g_ip_screen = true;
  g_ip_screen_start_ms = millis();
  g_ip_screen_shown = -1;
  g_radar_visible = false;
}

/** Update the countdown; returns false once it has finished. */
bool tickIpScreen() {
  const unsigned long elapsed_s = (millis() - g_ip_screen_start_ms) / 1000;
  const int left = config::kIpScreenSeconds - static_cast<int>(elapsed_s);
  if (left <= 0) {
    g_ip_screen = false;
    Serial.println("IP screen done");
    return false;
  }
  if (left != g_ip_screen_shown) {
    statusScreenIpCountdown(left);
    g_ip_screen_shown = left;
  }
  return true;
}

void showRadarIfConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    g_radar_visible = false;
    return;
  }
  ui::radarDisplayDraw();
  g_radar_visible = true;
}

void onRangeTap() {
  ui::radar::rangeNext();
  char range_label[12];
  ui::radar::formatCurrentRing3Label(range_label, sizeof(range_label));
  Serial.printf("Range: %s (outer ~%.0f km)\n", range_label,
                ui::radar::rangeCurrent().outer_km);

  if (g_radar_visible && WiFi.status() == WL_CONNECTED) {
    ui::radarDisplayDraw();
  }
}

void handleBootButton() {
  bootButtonPollLongPress();
  if (bootButtonConsumeTap()) {
    if (g_ip_screen) {
      g_ip_screen = false;  // a tap skips the IP countdown
      Serial.println("IP screen skipped");
    } else {
      onRangeTap();
    }
  }
}

// ADS-B fetch runs on its own task: the HTTPS request blocks for ~1-2 s, and
// keeping it off the main loop lets the radar keep redrawing (dead-reckoned) at
// 4 Hz throughout. The task publishes into the shared aircraft buffer under a
// lock; the render loop reads a snapshot.
void adsbFetchTask(void*) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      services::adsb::fetchUpdate(services::location::lat(),
                                  services::location::lon(),
                                  ui::radar::fetchRadiusKm());
    }
    vTaskDelay(pdMS_TO_TICKS(config::kAdsbFetchIntervalMs));
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Plane Radar");

  bootButtonInit();
  ui::radar::colors::init();  // before any screen: setup screens use it
  displayInit();
  if (wifiShowsSetupScreenOnBoot()) {
    statusScreenPortal();
  }
  services::location::init();
  ui::radar::rangeInit();
  services::adsb::init();

  if (wifiSetupConnect()) {
    startIpScreen();
  }

  // Start the background ADS-B fetch (pinned to core 0, away from the render
  // loop on core 1). It checks Wi-Fi state each cycle.
  xTaskCreatePinnedToCore(adsbFetchTask, "adsb", 16384, nullptr, 1, nullptr, 0);
}

void loop() {
  handleBootButton();
  wifiLoop();

  if (WiFi.status() != WL_CONNECTED) {
    if (g_radar_visible || g_ip_screen) {
      Serial.println("WiFi lost — will reconnect");
      g_radar_visible = false;
      g_ip_screen = false;
    }

    if (g_wifi_down_since == 0) {
      g_wifi_down_since = millis();
    }

    const unsigned long down_ms = millis() - g_wifi_down_since;
    if (down_ms >= config::kWifiDownGraceMs &&
        millis() - g_last_reconnect_ms >= config::kWifiReconnectIntervalMs) {
      g_last_reconnect_ms = millis();
      if (wifiReconnect()) {
        g_wifi_down_since = 0;
        startIpScreen();
      }
    }
  } else {
    g_wifi_down_since = 0;
    if (g_ip_screen) {
      if (!tickIpScreen()) {
        showRadarIfConnected();
      }
    } else if (!g_radar_visible) {
      // Connected without going through startIpScreen() (e.g. the Wi-Fi
      // reconnected by itself): show the IP first, then the radar.
      startIpScreen();
      if (!g_ip_screen) {
        showRadarIfConnected();
      }
    } else if (millis() - g_last_redraw_ms >= config::kRadarRedrawIntervalMs) {
      // Redraw at 4 Hz with dead-reckoned positions; the ADS-B fetch runs on
      // its own task (adsbFetchTask).
      g_last_redraw_ms = millis();
      ui::radarDisplayRefreshAircraft();
    }
  }

  delay(10);
}
