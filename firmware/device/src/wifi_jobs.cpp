/*
 * Ephemeral STA jobs (SNTP). Never stay associated — ESP-NOW SoftAP restored after.
 */

#include "wifi_jobs.h"

#include "app/app.h"

#include <WiFi.h>
#include <time.h>

#include <cstdio>
#include <cstring>

namespace wp {
namespace wifi_jobs {
namespace {

constexpr uint8_t kChannel = 1;
constexpr int kJoinTries = 40;   /* *250ms = 10s */
constexpr int kNtpTries = 40;

void restore_espnow_ap() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("WerkBuddy", nullptr, kChannel);
}

}  // namespace

int scan(ScanAp * out, int max_out) {
  if (!out || max_out <= 0) return 0;
  WiFi.mode(WIFI_AP_STA);
  const int n = WiFi.scanNetworks(false, false);
  int count = 0;
  for (int i = 0; i < n && count < max_out; ++i) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    std::snprintf(out[count].ssid, sizeof(out[count].ssid), "%s", ssid.c_str());
    const int rssi = WiFi.RSSI(i);
    int bars = 1;
    if (rssi > -55) bars = 4;
    else if (rssi > -65) bars = 3;
    else if (rssi > -75) bars = 2;
    out[count].bars = bars;
    out[count].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    ++count;
  }
  WiFi.scanDelete();
  restore_espnow_ap();
  return count;
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
  WiFi.disconnect(true, false);
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
    WiFi.disconnect(true, false);
    restore_espnow_ap();
    if (err && err_n) std::snprintf(err, err_n, "Could not join %s", d.wifi_ssid);
    Serial.println("WiFi sync: join FAILED");
    return false;
  }

  Serial.printf("WiFi sync: joined IP=%s\n", WiFi.localIP().toString().c_str());
  /* US Central with DST. configTzTime so localtime() is not stuck on UTC. */
  configTzTime("CST6CDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");

  bool got = false;
  for (int i = 0; i < kNtpTries; ++i) {
    time_t now = time(nullptr);
    if (now > 1700000000) { /* ~2023+ */
      struct tm ti {};
      localtime_r(&now, &ti);
      /* Sanity: Central wall hour should not match UTC when offset is hours. */
      Serial.printf("WiFi sync: local %04d-%02d-%02d %02d:%02d\n", ti.tm_year + 1900,
                    ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min);
      got = true;
      break;
    }
    delay(250);
  }

  WiFi.disconnect(true, false);
  delay(50);
  restore_espnow_ap();
  d.wifi_connected = false;

  if (!got) {
    if (err && err_n) std::snprintf(err, err_n, "NTP timed out");
    Serial.println("WiFi sync: NTP FAILED");
    return false;
  }

  /* TZ via configTzTime; caller bumps sync_gen + ESP-NOW TimeSync. */
  d.clock_offset_ms = 0;
  Serial.println("WiFi sync: OK");
  return true;
}

}  // namespace wifi_jobs
}  // namespace wp
