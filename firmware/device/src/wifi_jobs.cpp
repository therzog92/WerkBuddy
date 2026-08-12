/*
 * Ephemeral STA jobs (SNTP). Never stay associated — ESP-NOW SoftAP restored after.
 */

#include "wifi_jobs.h"

#include "app/app.h"
#include "device_net.h"

#include <WiFi.h>
#include <time.h>

#include <cstdio>
#include <cstring>

namespace wp {
namespace wifi_jobs {
namespace {

constexpr int kJoinTries = 40; /* *250ms = 10s */
constexpr int kNtpTries = 40;

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
  net::restore_espnow_radio();
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

  bool got = false;
  for (int i = 0; i < kNtpTries; ++i) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
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
