#include "app/storage.h"

#include "app/app.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace wp {
namespace storage {
namespace {

const char * kFile = "werkpager_settings.ini";

void put(FILE * f, const char * key, const char * val) { std::fprintf(f, "%s=%s\n", key, val); }
void put_int(FILE * f, const char * key, long long val) { std::fprintf(f, "%s=%lld\n", key, val); }

}  // namespace

bool load(app::Desk & d) {
  FILE * f = std::fopen(kFile, "rb");
  if (!f) return false;

  char line[192];
  int peer_i = 0;
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
      d.theme = (uint8_t)std::atoi(val);
    } else if (!std::strcmp(key, "timeout")) {
      d.timeout_id = (uint8_t)std::atoi(val);
    } else if (!std::strcmp(key, "idle_mode")) {
      d.idle_mode = (uint8_t)std::atoi(val);
    } else if (!std::strcmp(key, "brightness")) {
      int b = std::atoi(val);
      if (b < 10) b = 10;
      if (b > 100) b = 100;
      d.brightness = (uint8_t)b;
    } else if (!std::strcmp(key, "clock_offset_ms")) {
      d.clock_offset_ms = std::atoll(val);
    } else if (!std::strncmp(key, "emoji", 5)) {
      const int i = std::atoi(key + 5);
      if (i >= 0 && i < app::kEmojiSlots) std::snprintf(d.emojis[i], sizeof(d.emojis[i]), "%s", val);
    } else if (!std::strncmp(key, "canned", 6)) {
      const int i = std::atoi(key + 6);
      if (i >= 0 && i < app::kCannedCount) std::snprintf(d.canned[i], sizeof(d.canned[i]), "%s", val);
    } else if (!std::strcmp(key, "peer") && peer_i < app::kMaxPeers) {
      /* "peer=<id>|<name>" */
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
  std::fclose(f);
  return true;
}

void save(const app::Desk & d) {
  FILE * f = std::fopen(kFile, "wb");
  if (!f) return;
  put(f, "name", d.name);
  put_int(f, "theme", d.theme);
  put_int(f, "timeout", d.timeout_id);
  put_int(f, "idle_mode", d.idle_mode);
  put_int(f, "brightness", d.brightness);
  put_int(f, "clock_offset_ms", d.clock_offset_ms);
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
