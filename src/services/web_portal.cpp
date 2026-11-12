#include "services/web_portal.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include <cmath>
#include <cstring>

#include "config.h"
#include "services/radar_location.h"
#include "services/wifi_setup.h"
#include "services/web_pages.h"
#include "ui/radar_colors.h"
#include "ui/radar_range.h"

namespace services::web {

namespace {

WebServer s_server(80);
bool s_active = false;
bool s_routes_registered = false;

void sendPage(const char* body, const char* type) {
  s_server.sendHeader("Cache-Control", "no-cache");
  s_server.send_P(200, type, body);
}

void sendJson(const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  s_server.sendHeader("Cache-Control", "no-store");
  s_server.send(200, "application/json", out);
}

void sendOk() { s_server.send(200, "application/json", "{\"ok\":true}"); }

void sendError(int code, const char* message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  String out;
  serializeJson(doc, out);
  s_server.send(code, "application/json", out);
}

bool readBody(JsonDocument* doc) {
  if (deserializeJson(*doc, s_server.arg("plain"))) {
    sendError(400, "Bad request");
    return false;
  }
  return true;
}

bool readFinite(JsonVariantConst v, double* out) {
  if (!v.is<double>() && !v.is<long>()) {
    return false;
  }
  *out = v.as<double>();
  return std::isfinite(*out);
}

// --- Radar settings ------------------------------------------------------

void handleGetRadar() {
  JsonDocument doc;
  doc["lat"] = services::location::lat();
  doc["lon"] = services::location::lon();
  doc["range"] = ui::radar::rangeIndex();
  doc["useMiles"] = ui::radar::useMiles();
  doc["showRunways"] = ui::radar::showRunways();
  doc["showCount"] = ui::radar::showAircraftCount();
  doc["showFlight"] = ui::radar::showTagCallsign();
  doc["showType"] = ui::radar::showTagType();
  doc["showAlt"] = ui::radar::showTagAlt();
  doc["useMeters"] = ui::radar::useMeters();
  doc["watch"] = ui::radar::watchList();
  doc["minKm"] = ui::radar::kMinPresetKm;
  doc["maxKm"] = ui::radar::kMaxPresetKm;
  JsonArray presets = doc["presets"].to<JsonArray>();
  for (size_t i = 0; i < ui::radar::kRangePresetCount; ++i) {
    presets.add(ui::radar::rangePresets()[i].ring3_km);
  }
  sendJson(doc);
}

void handlePostRadar() {
  JsonDocument doc;
  if (!readBody(&doc)) {
    return;
  }

  // Validate everything first, so a bad field saves nothing.
  double lat = 0.0;
  double lon = 0.0;
  if (!readFinite(doc["lat"], &lat) || !readFinite(doc["lon"], &lon) ||
      lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    sendError(400, "Latitude must be -90..90 and longitude -180..180.");
    return;
  }
  float km[ui::radar::kRangePresetCount];
  JsonArrayConst presets = doc["presets"];
  if (presets.size() != ui::radar::kRangePresetCount) {
    sendError(400, "Four range presets are needed.");
    return;
  }
  for (size_t i = 0; i < ui::radar::kRangePresetCount; ++i) {
    double v = 0.0;
    if (!readFinite(presets[i], &v)) {
      sendError(400, "Range presets must be numbers.");
      return;
    }
    km[i] = static_cast<float>(v);
  }
  if (!ui::radar::rangeSavePresets(km, ui::radar::kRangePresetCount)) {
    sendError(400, "Range presets must be 0.5-500 km and each bigger than the last.");
    return;
  }

  services::location::saveLatLon(lat, lon);
  ui::radar::rangeSet(doc["range"] | ui::radar::rangeIndex());
  ui::radar::setUseMiles(doc["useMiles"] | false);
  ui::radar::setShowRunways(doc["showRunways"] | false);
  ui::radar::setShowAircraftCount(doc["showCount"] | false);
  ui::radar::setShowTagCallsign(doc["showFlight"] | false);
  ui::radar::setShowTagType(doc["showType"] | false);
  ui::radar::setShowTagAlt(doc["showAlt"] | false);
  ui::radar::setUseMeters(doc["useMeters"] | false);
  ui::radar::setWatchList(doc["watch"] | "");
  Serial.println("Radar settings saved from web page");
  sendOk();
}

// --- Colours -------------------------------------------------------------

void handleGetTheme() {
  namespace colors = ui::radar::colors;
  JsonDocument doc;
  char hex[8];
  doc["preset"] = colors::presetId();
  doc["swap"] = colors::swapRedBlue();
  JsonArray slots = doc["slots"].to<JsonArray>();
  JsonObject current = doc["colors"].to<JsonObject>();
  for (size_t i = 0; i < colors::kSlotCount; ++i) {
    JsonObject o = slots.add<JsonObject>();
    o["key"] = colors::kSlots[i].key;
    o["label"] = colors::kSlots[i].label;
    colors::formatHex(colors::get(static_cast<colors::Slot>(i)), hex, sizeof(hex));
    current[colors::kSlots[i].key] = hex;
  }
  JsonArray list = doc["presets"].to<JsonArray>();
  for (size_t p = 0; p < colors::kPresetCount; ++p) {
    const colors::Theme& t = colors::kPresets[p];
    JsonObject o = list.add<JsonObject>();
    o["id"] = t.id;
    o["name"] = t.name;
    JsonObject c = o["colors"].to<JsonObject>();
    for (size_t i = 0; i < colors::kSlotCount; ++i) {
      colors::formatHex(t.rgb[i], hex, sizeof(hex));
      c[colors::kSlots[i].key] = hex;
    }
  }
  sendJson(doc);
}

void handlePostTheme() {
  namespace colors = ui::radar::colors;
  JsonDocument doc;
  if (!readBody(&doc)) {
    return;
  }
  uint32_t rgb[colors::kSlotCount];
  JsonObjectConst in = doc["colors"];
  for (size_t i = 0; i < colors::kSlotCount; ++i) {
    if (!colors::parseHex(in[colors::kSlots[i].key] | "", &rgb[i])) {
      sendError(400, "Colours must look like #rrggbb.");
      return;
    }
  }
  colors::save(doc["preset"] | "custom", rgb, doc["swap"] | colors::swapRedBlue());
  sendOk();
}

// --- Saved locations -----------------------------------------------------

void handleGetLocations() {
  JsonDocument doc;
  doc["max"] = static_cast<int>(services::location::kMaxSavedLocations);
  doc["nameMax"] = static_cast<int>(services::location::kLocationNameMax);
  doc["lat"] = services::location::lat();
  doc["lon"] = services::location::lon();
  const uint8_t active = services::location::activeLocationIndex();
  JsonArray arr = doc["locations"].to<JsonArray>();
  const services::location::SavedLocation* locs =
      services::location::savedLocations();
  for (uint8_t i = 0; i < services::location::kMaxSavedLocations; ++i) {
    JsonObject o = arr.add<JsonObject>();
    o["slot"] = i;
    o["valid"] = locs[i].valid;
    o["name"] = locs[i].name;
    o["lat"] = locs[i].lat;
    o["lon"] = locs[i].lon;
    o["active"] = (active == i);
  }
  sendJson(doc);
}

void handlePostLocations() {
  JsonDocument doc;
  if (!readBody(&doc)) {
    return;
  }
  const char* action = doc["action"] | "";
  const int slot = doc["slot"] | -1;
  if (slot < 0 || slot >= static_cast<int>(services::location::kMaxSavedLocations)) {
    sendError(400, "Unknown location slot.");
    return;
  }
  const uint8_t s = static_cast<uint8_t>(slot);

  if (strcmp(action, "use") == 0) {
    if (!services::location::useLocation(s)) {
      sendError(400, "That location is empty.");
      return;
    }
  } else if (strcmp(action, "delete") == 0) {
    services::location::deleteLocation(s);
  } else if (strcmp(action, "save") == 0) {
    double lat = 0.0;
    double lon = 0.0;
    if (!readFinite(doc["lat"], &lat) || !readFinite(doc["lon"], &lon) ||
        !services::location::saveLocation(s, doc["name"] | "", lat, lon)) {
      sendError(400, "Latitude must be -90..90 and longitude -180..180.");
      return;
    }
  } else {
    sendError(400, "Unknown action.");
    return;
  }
  sendOk();
}

// --- Device --------------------------------------------------------------

void handleGetDevice() {
  JsonDocument doc;
  doc["version"] = config::kFirmwareVersion;
  doc["built"] = __DATE__;
  doc["mdns"] = String(config::kPortalHostname) + ".local";
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  sendJson(doc);
}

void replyThenRestartSoon() {
  sendOk();
  delay(400);  // give the reply time to leave before the radio goes down
}

void handleRestart() {
  replyThenRestartSoon();
  esp_restart();
}

void handleWifiReset() {
  replyThenRestartSoon();
  wifiEraseCredentialsOnlyAndReboot();
}

void registerRoutes() {
  s_server.on("/", HTTP_GET, [] { sendPage(pages::kIndexHtml, "text/html"); });
  s_server.on("/radar", HTTP_GET, [] { sendPage(pages::kRadarHtml, "text/html"); });
  s_server.on("/colours", HTTP_GET,
              [] { sendPage(pages::kColoursHtml, "text/html"); });
  s_server.on("/locs", HTTP_GET, [] { sendPage(pages::kLocsHtml, "text/html"); });
  s_server.on("/device", HTTP_GET,
              [] { sendPage(pages::kDeviceHtml, "text/html"); });
  s_server.on("/style.css", HTTP_GET, [] { sendPage(pages::kStyleCss, "text/css"); });
  s_server.on("/app.js", HTTP_GET,
              [] { sendPage(pages::kAppJs, "application/javascript"); });

  s_server.on("/api/radar", HTTP_GET, handleGetRadar);
  s_server.on("/api/radar", HTTP_POST, handlePostRadar);
  s_server.on("/api/theme", HTTP_GET, handleGetTheme);
  s_server.on("/api/theme", HTTP_POST, handlePostTheme);
  s_server.on("/api/locations", HTTP_GET, handleGetLocations);
  s_server.on("/api/locations", HTTP_POST, handlePostLocations);
  s_server.on("/api/device", HTTP_GET, handleGetDevice);
  s_server.on("/api/restart", HTTP_POST, handleRestart);
  s_server.on("/api/wifi/reset", HTTP_POST, handleWifiReset);
  s_server.onNotFound([] {
    // Old WiFiManager bookmarks (e.g. /param) land on the menu.
    s_server.sendHeader("Location", "/");
    s_server.send(302, "text/plain", "");
  });
}

}  // namespace

void start() {
  if (s_active) {
    return;
  }
  if (!s_routes_registered) {
    registerRoutes();
    s_routes_registered = true;
  }
  s_server.begin();
  s_active = true;
}

void stop() {
  if (!s_active) {
    return;
  }
  s_server.stop();
  s_active = false;
}

bool active() { return s_active; }

void handleClient() {
  if (s_active) {
    s_server.handleClient();
  }
}

}  // namespace services::web
