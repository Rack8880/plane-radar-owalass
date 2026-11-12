#pragma once

/** True when the next boot should show the setup screen first (after credential reset). */
bool wifiShowsSetupScreenOnBoot();
/** Full reset: Wi-Fi, radar position and units, then reboot into setup.
 *  Only reachable from the Device page (BOOT hold is Wi-Fi only). */
void wifiResetCredentialsAndReboot();
/** Forget only the Wi-Fi network (keeps location/settings), then reboot into setup. */
void wifiEraseCredentialsOnlyAndReboot();
/** Boot flow: connect with UI, open portal only if saved creds fail. */
bool wifiSetupConnect();
/** Reconnect using saved creds; never opens the captive portal. */
bool wifiReconnect();
/** Keeps the LAN config portal alive; call every loop() iteration. */
void wifiLoop();
bool wifiBootButtonPressed();
/** GPIO + interrupt setup; call once early in setup(). */
void bootButtonInit();
/** Latched short tap (survives blocking HTTP/display work). */
bool bootButtonConsumeTap();
/** Call each loop iteration; triggers WiFi reset on long hold. */
void bootButtonPollLongPress();
