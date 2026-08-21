/*
 * Ephemeral STA jobs (SNTP). Never stay associated — ESP-NOW SoftAP restored after.
 */

#include "wifi_jobs.h"

#include "app/app.h"
#include "device_net.h"

#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

#include <cstdio>
#include <cstring>

namespace wp {
namespace wifi_jobs {
namespace {

constexpr int kJoinTries = 40; /* *250ms = 10s */
constexpr int kNtpTries = 40;

}  // namespace

int bars_for(int rssi) {
  if (rssi > -55) return 4;
  if (rssi > -65) return 3;
  if (rssi > -75) return 2;
  return 1;
}

int scan(ScanAp * out, int max_out) {
  if (!out || max_out <= 0) return 0;

  /* Full STA-mode scan sees all 2.4 GHz channels — anchoring SoftAP on ch 1
   * while scanning skips the phone-hotspot channels. Re-home radio afterward. */
  WiFi.scanDelete();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(50);

  constexpr int kCap = 48;
  char ssid[kCap][33];
  int rssi[kCap];
  bool open[kCap];
  int n = 0;

  for (int pass = 0; pass < 2; ++pass) {
    const int found = WiFi.scanNetworks(false /*async*/, false /*show_hidden*/);
    for (int i = 0; i < found && n < kCap; ++i) {
      const String s = WiFi.SSID(i);
      if (!s.length()) continue;
      const int r = WiFi.RSSI(i);
      const bool o = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;

      int slot = -1;
      for (int k = 0; k < n; ++k)
        if (s == ssid[k]) { slot = k; break; }
      if (slot >= 0) {
        if (r > rssi[slot]) { rssi[slot] = r; open[slot] = o; }
        continue;
      }
      std::snprintf(ssid[n], 33, "%s", s.c_str());
      rssi[n] = r;
      open[n] = o;
      ++n;
    }
    WiFi.scanDelete();
    if (pass == 0) delay(120);
  }

  /* Strongest signal first, then truncate to the requested count. */
  for (int a = 0; a < n; ++a)
    for (int b = a + 1; b < n; ++b)
      if (rssi[b] > rssi[a]) {
        const int tr = rssi[a]; rssi[a] = rssi[b]; rssi[b] = tr;
        const bool to = open[a]; open[a] = open[b]; open[b] = to;
        char ts[33]; std::snprintf(ts, sizeof(ts), "%s", ssid[a]);
        std::snprintf(ssid[a], sizeof(ssid[a]), "%s", ssid[b]);
        std::snprintf(ssid[b], sizeof(ssid[b]), "%s", ts);
      }

  const int count = n < max_out ? n : max_out;
  for (int i = 0; i < count; ++i) {
    std::snprintf(out[i].ssid, sizeof(out[i].ssid), "%s", ssid[i]);
    out[i].bars = bars_for(rssi[i]);
    out[i].open = open[i];
  }

  net::restore_espnow_radio();
  return count;
}

bool test_join(const char * ssid, const char * pass, char * err, size_t err_n) {
  if (err && err_n) err[0] = 0;
  if (!ssid || !ssid[0]) {
    if (err && err_n) std::snprintf(err, err_n, "No network");
    return false;
  }

  WiFi.scanDelete();
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(ssid, (pass && pass[0]) ? pass : nullptr);

  wl_status_t st = WL_IDLE_STATUS;
  for (int i = 0; i < kJoinTries; ++i) {
    st = WiFi.status();
    if (st == WL_CONNECTED) break;
    if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL ||
        st == WL_CONNECTION_LOST)
      break;
    delay(250);
  }

  WiFi.disconnect(false, false);
  net::restore_espnow_radio();

  if (st == WL_CONNECTED) {
    Serial.printf("WiFi verify: joined %s OK\n", ssid);
    return true;
  }
  if (st == WL_NO_SSID_AVAIL) {
    if (err && err_n) std::snprintf(err, err_n, "Could not find network");
  } else if (st == WL_CONNECT_FAILED || st == WL_CONNECTION_LOST) {
    if (err && err_n) std::snprintf(err, err_n, "Wrong password");
  } else {
    if (err && err_n) std::snprintf(err, err_n, "Timed out");
  }
  Serial.printf("WiFi verify: FAILED (status %d)\n", (int)st);
  return false;
}

bool sync_time(char * err, size_t err_n) {
  if (err && err_n) err[0] = 0;
  app::Desk & d = app::desk();
  if (!d.wifi_ssid[0]) {
    if (err && err_n) std::snprintf(err, err_n, "No Wi-Fi saved");
    return false;
  }

  Serial.printf("WiFi sync: joining %s\n", d.wifi_ssid);
  d.wifi_connected = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(d.wifi_ssid, d.wifi_pass[0] ? d.wifi_pass : nullptr);

  bool joined = false;
  for (int i = 0; i < kJoinTries; ++i) {
    if (WiFi.status() == WL_CONNECTED) {
      joined = true;
      break;
    }
    delay(250);
  }
  if (!joined) {
    d.wifi_connected = false;
    WiFi.disconnect(false, false);
    net::restore_espnow_radio();
    if (err && err_n) std::snprintf(err, err_n, "Could not join %s", d.wifi_ssid);
    Serial.println("WiFi sync: join FAILED");
    return false;
  }

  Serial.printf("WiFi sync: joined IP=%s\n", WiFi.localIP().toString().c_str());
  configTzTime("CST6CDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");

  /* Wait for an actual NTP sync — the boot clock already holds a sane default
   * epoch, so a naive time() threshold would fire immediately with no sync. */
  bool got = false;
  for (int i = 0; i < kNtpTries; ++i) {
    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      time_t now = time(nullptr);
      struct tm ti {};
      localtime_r(&now, &ti);
      Serial.printf("WiFi sync: local %04d-%02d-%02d %02d:%02d\n", ti.tm_year + 1900,
                    ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min);
      got = true;
      break;
    }
    delay(250);
  }

  WiFi.disconnect(false, false);
  delay(50);
  net::restore_espnow_radio();
  d.wifi_connected = false;

  if (!got) {
    if (err && err_n) std::snprintf(err, err_n, "NTP timed out");
    Serial.println("WiFi sync: NTP FAILED");
    return false;
  }

  d.clock_offset_ms = 0;
  Serial.println("WiFi sync: OK");
  return true;
}

bool join_sta(char * err, size_t err_n) {
  if (err && err_n) err[0] = 0;
  app::Desk & d = app::desk();
  if (!d.wifi_ssid[0]) {
    if (err && err_n) std::snprintf(err, err_n, "No Wi-Fi saved");
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    d.wifi_connected = true;
    return true;
  }

  Serial.printf("WiFi join: %s\n", d.wifi_ssid);
  d.wifi_connected = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(d.wifi_ssid, d.wifi_pass[0] ? d.wifi_pass : nullptr);

  for (int i = 0; i < kJoinTries; ++i) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("WiFi join: IP=%s\n", WiFi.localIP().toString().c_str());
      return true;
    }
    delay(250);
  }

  d.wifi_connected = false;
  WiFi.disconnect(false, false);
  net::restore_espnow_radio();
  if (err && err_n) std::snprintf(err, err_n, "Could not join %s", d.wifi_ssid);
  Serial.println("WiFi join: FAILED");
  return false;
}

void leave_sta() {
  app::desk().wifi_connected = false;
  WiFi.disconnect(false, false);
  delay(50);
  net::restore_espnow_radio();
  Serial.println("WiFi leave: SoftAP restored");
}

}  // namespace wifi_jobs
}  // namespace wp
