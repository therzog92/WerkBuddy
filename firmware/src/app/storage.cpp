#include "app/storage.h"

#include "app/app.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wp {
namespace storage {
namespace {

const char * kFile = "werkpager_settings.ini";
constexpr int kTimeoutScheme = 2; /* 1m/3m/5m/10m/Off */

void put(FILE * f, const char * key, const char * val) { std::fprintf(f, "%s=%s\n", key, val); }
void put_int(FILE * f, const char * key, long long val) { std::fprintf(f, "%s=%lld\n", key, val); }

/** Old scheme: 0=30s 1=1m 2=5m 3=off → new indices. */
uint8_t remap_legacy_timeout(uint8_t old_id) {
  switch (old_id) {
    case 0: return 0; /* 30s → 1m */
    case 1: return 0; /* 1m → 1m */
    case 2: return 2; /* 5m → 5m */
    case 3: return 4; /* off → off */
    default: return 2; /* default 5m */
  }
}

}  // namespace

bool load(app::Desk & d) {
  FILE * f = std::fopen(kFile, "rb");
  if (!f) return false;

  char line[192];
  int peer_i = 0;
  bool saw_setup = false;
  int timeout_scheme = 0;
  uint8_t raw_timeout = d.timeout_id;

  while (std::fgets(line, sizeof(line), f)) {
    char * eq = std::strchr(line, '=');
    if (!eq) continue;
    *eq = '\0';
    char * key = line;
    char * val = eq + 1;
    val[std::strcspn(val, "\r\n")] = '\0';

    if (!std::strcmp(key, "name")) {
      std::snprintf(d.name, sizeof(d.name), "%s", val);
    } else if (!std::strcmp(key, "theme")) {
      int t = std::atoi(val);
      if (t < 0) t = 0;
      if (t > 7) t = 7;
      d.theme = (uint8_t)t;
    } else if (!std::strcmp(key, "bg_preset")) {
      int p = std::atoi(val);
      if (p < 0) p = 0;
      if (p > 5) p = 5;
      d.bg_preset = (uint8_t)p;
    } else if (!std::strcmp(key, "timeout")) {
      raw_timeout = (uint8_t)std::atoi(val);
    } else if (!std::strcmp(key, "timeout_v")) {
      timeout_scheme = std::atoi(val);
    } else if (!std::strcmp(key, "idle_mode")) {
      d.idle_mode = (uint8_t)std::atoi(val) ? 1 : 0;
    } else if (!std::strcmp(key, "brightness")) {
      int b = std::atoi(val);
      if (b < 10) b = 10;
      if (b > 100) b = 100;
      d.brightness = (uint8_t)b;
    } else if (!std::strcmp(key, "setup_done")) {
      d.setup_done = std::atoi(val) != 0;
      saw_setup = true;
    } else if (!std::strcmp(key, "clock_offset_ms")) {
      d.clock_offset_ms = std::atoll(val);
    } else if (!std::strcmp(key, "wifi_ssid")) {
      std::snprintf(d.wifi_ssid, sizeof(d.wifi_ssid), "%s", val);
    } else if (!std::strcmp(key, "wifi_connected")) {
      /* Legacy key ignored — association is never persisted (ephemeral STA). */
      (void)val;
    } else if (!std::strcmp(key, "hs_2048")) {
      d.high_score_2048 = std::atoi(val);
      if (d.high_score_2048 < 0) d.high_score_2048 = 0;
    } else if (!std::strcmp(key, "fw_version")) {
      std::snprintf(d.fw_version, sizeof(d.fw_version), "%s", val);
    } else if (!std::strncmp(key, "emoji", 5)) {
      const int i = std::atoi(key + 5);
      if (i >= 0 && i < app::kEmojiSlots) std::snprintf(d.emojis[i], sizeof(d.emojis[i]), "%s", val);
    } else if (!std::strncmp(key, "canned", 6)) {
      const int i = std::atoi(key + 6);
      if (i >= 0 && i < app::kCannedCount) std::snprintf(d.canned[i], sizeof(d.canned[i]), "%s", val);
    } else if (!std::strcmp(key, "peer") && peer_i < app::kMaxPeers) {
      char * bar = std::strchr(val, '|');
      if (bar) {
        *bar = '\0';
        std::snprintf(d.peers[peer_i].id, sizeof(d.peers[peer_i].id), "%s", val);
        std::snprintf(d.peers[peer_i].name, sizeof(d.peers[peer_i].name), "%s", bar + 1);
        ++peer_i;
      }
    }
  }
  d.peer_count = peer_i > 0 ? peer_i : d.peer_count;
  if (!saw_setup) d.setup_done = true; /* existing installs */

  if (timeout_scheme >= kTimeoutScheme) {
    d.timeout_id = raw_timeout;
  } else {
    d.timeout_id = remap_legacy_timeout(raw_timeout);
  }
  if (d.timeout_id >= app::kTimeoutCount) d.timeout_id = 2;

  std::fclose(f);
  return true;
}

void save(const app::Desk & d) {
  FILE * f = std::fopen(kFile, "wb");
  if (!f) return;
  put(f, "name", d.name);
  put_int(f, "theme", d.theme);
  put_int(f, "bg_preset", d.bg_preset);
  put_int(f, "timeout", d.timeout_id);
  put_int(f, "timeout_v", kTimeoutScheme);
  put_int(f, "idle_mode", d.idle_mode);
  put_int(f, "brightness", d.brightness);
  put_int(f, "setup_done", d.setup_done ? 1 : 0);
  put_int(f, "clock_offset_ms", d.clock_offset_ms);
  put(f, "wifi_ssid", d.wifi_ssid);
  /* Do not persist wifi_connected — STA is join-for-job only. */
  put_int(f, "hs_2048", d.high_score_2048);
  if (d.fw_version[0]) put(f, "fw_version", d.fw_version);
  for (int i = 0; i < app::kEmojiSlots; ++i) {
    char key[16];
    std::snprintf(key, sizeof(key), "emoji%d", i);
    put(f, key, d.emojis[i]);
  }
  for (int i = 0; i < app::kCannedCount; ++i) {
    char key[16];
    std::snprintf(key, sizeof(key), "canned%d", i);
    put(f, key, d.canned[i]);
  }
  for (int i = 0; i < d.peer_count; ++i) {
    char val[48];
    std::snprintf(val, sizeof(val), "%s|%s", d.peers[i].id, d.peers[i].name);
    put(f, "peer", val);
  }
  std::fclose(f);
}

}  // namespace storage
}  // namespace wp
