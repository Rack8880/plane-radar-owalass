#pragma once

// Quiet wrappers around Arduino Preferences. Preferences logs an [E] line for
// every read of a namespace or key that was never saved (normal on a fresh
// device), so these check first and stay silent when nothing is stored.

#include <Preferences.h>
#include <nvs.h>

#include <cstddef>

namespace services::prefs {

/** Open `ns` read-only if it exists. Returns false (silently) if never saved. */
inline bool openForRead(Preferences& prefs, const char* ns) {
  nvs_handle_t handle = 0;
  const esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return false;  // namespace never created: nothing saved yet
  }
  if (err == ESP_OK) {
    nvs_close(handle);
  }
  return prefs.begin(ns, true);
}

/** Stored blob size, or 0 if the key is missing. */
inline size_t blobLength(Preferences& prefs, const char* key) {
  return prefs.isKey(key) ? prefs.getBytesLength(key) : 0;
}

/** Remove `key` only if it exists. */
inline void removeIfPresent(Preferences& prefs, const char* key) {
  if (prefs.isKey(key)) {
    prefs.remove(key);
  }
}

}  // namespace services::prefs
