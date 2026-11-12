#pragma once

void statusScreenPortal();
void statusScreenConnectFailed();
void statusScreenWifiReset();

/** Saved-network connect animation (call Tick until connect finishes). */
void statusScreenConnectingBegin(const char* ssid);
void statusScreenConnectingTick();

/**
 * "Settings page: <ip>" screen with a countdown, shown after Wi-Fi connects.
 * Begin draws the static part; Countdown redraws only the number.
 */
void statusScreenIpBegin(const char* ip);
void statusScreenIpCountdown(int seconds_left);
