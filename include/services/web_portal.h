#pragma once

/**
 * Settings web site served while the radar is on your Wi-Fi
 * (http://plane-radar.local or the device IP). First-time Wi-Fi setup still
 * uses WiFiManager's access-point portal; this replaces WiFiManager's pages
 * once connected.
 *
 * Pages are compiled into the firmware (include/services/web_pages.h), so no
 * separate filesystem upload is needed.
 */
namespace services::web {

void start();
void stop();
bool active();
/** Serve pending requests; call every loop() iteration while active. */
void handleClient();

}  // namespace services::web
