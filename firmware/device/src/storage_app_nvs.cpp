#include "app/storage.h"

#include "app/app.h"

#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace wp {
namespace storage {
namespace {

Preferences prefs;

}  // namespace

bool load(app::Desk & d) {
  if (!prefs.begin("werkpager", true)) return false;

  /* Always apply — empty string must clear any in-RAM default name. */
  {
    String name = prefs.getString("name", "");
    std::snprintf(d.name, sizeof(d.name), "%s", name.c_str());
  }
  d.theme = (uint8_t)prefs.getUChar("theme", d.theme);
  if (d.theme > 7) d.theme = 0;
  d.bg_preset = (uint8_t)prefs.getUChar("bg_preset", d.bg_preset);
  d.timeout_id = (uint8_t)prefs.getUChar("timeout", d.timeout_id);
  d.idle_mode = (uint8_t)prefs.getUChar("idle_mode", d.idle_mode) ? 1 : 0;
  d.brightness = (uint8_t)prefs.getUChar("brightness", d.brightness);
  if (d.brightness < 10) d.brightness = 10;
  d.rotate_180 = prefs.getUChar("rot180", d.rotate_180) ? 1 : 0;
  d.clock_24h = prefs.getUChar("clk24", d.clock_24h) ? 1 : 0;
  if (prefs.isKey("setup_done")) {
    d.setup_done = prefs.getBool("setup_done", false);
  } else if (prefs.isKey("setup")) {
    d.setup_done = prefs.getBool("setup", false); /* thin-shell key */
  } else {
    /* Truly legacy NVS with a name but no setup flag → already configured. */
    d.setup_done = d.name[0] != '\0';
  }
  d.clock_offset_ms = prefs.getLong64("clock_off", d.clock_offset_ms);
  if (!prefs.isKey("clock_off")) d.clock_offset_ms = prefs.getLong64("clk_off", d.clock_offset_ms);
  d.wall_epoch = prefs.getUInt("wall_ep", d.wall_epoch);
  d.clock_sync_gen = prefs.getUInt("clock_gen", d.clock_sync_gen);
  d.high_score_2048 = prefs.getInt("hs_2048", d.high_score_2048);

  String ssid = prefs.getString("wifi_ssid", "");
  if (ssid.length()) std::snprintf(d.wifi_ssid, sizeof(d.wifi_ssid), "%s", ssid.c_str());
  String wpass = prefs.getString("wifi_pass", "");
  if (wpass.length()) std::snprintf(d.wifi_pass, sizeof(d.wifi_pass), "%s", wpass.c_str());

  for (int i = 0; i < app::kEmojiSlots; ++i) {
    char key[12];
    std::snprintf(key, sizeof(key), "e%d", i);
    String v = prefs.getString(key, "");
    if (v.length()) std::snprintf(d.emojis[i], sizeof(d.emojis[i]), "%s", v.c_str());
  }
  for (int i = 0; i < app::kCannedCount; ++i) {
    char key[12];
    std::snprintf(key, sizeof(key), "c%d", i);
    String v = prefs.getString(key, "");
    if (v.length()) std::snprintf(d.canned[i], sizeof(d.canned[i]), "%s", v.c_str());
  }

  d.peer_count = 0;
  const bool has_peers_key = prefs.isKey("peers");
  const int n = prefs.getInt("peers", 0);
  for (int i = 0; i < n && i < app::kMaxPeers; ++i) {
    char ik[12], nk[12];
    std::snprintf(ik, sizeof(ik), "p%di", i);
    std::snprintf(nk, sizeof(nk), "p%dn", i);
    String id = prefs.getString(ik, "");
    String nm = prefs.getString(nk, "");
    if (!id.length()) continue;
    std::snprintf(d.peers[d.peer_count].id, sizeof(d.peers[0].id), "%s", id.c_str());
    std::snprintf(d.peers[d.peer_count].name, sizeof(d.peers[0].name), "%s", nm.c_str());
    ++d.peer_count;
  }
  /* Thin-shell peers: p0 = "macid|Name" — only when never migrated (no peers key).
   * If peers=0 after factory reset, must NOT resurrect leftover p0/p1 keys. */
  if (!has_peers_key && d.peer_count == 0) {
    for (int i = 0; i < app::kMaxPeers; ++i) {
      char key[8];
      std::snprintf(key, sizeof(key), "p%d", i);
      String v = prefs.getString(key, "");
      if (!v.length()) break;
      const int bar = v.indexOf('|');
      if (bar <= 0) continue;
      std::snprintf(d.peers[d.peer_count].id, sizeof(d.peers[0].id), "%s",
                    v.substring(0, bar).c_str());
      std::snprintf(d.peers[d.peer_count].name, sizeof(d.peers[0].name), "%s",
                    v.substring(bar + 1).c_str());
      ++d.peer_count;
    }
  }

  prefs.end();
  return true;
}

void save(const app::Desk & d) {
  if (!prefs.begin("werkpager", false)) return;
  prefs.putString("name", d.name);
  prefs.putUChar("theme", d.theme);
  prefs.putUChar("bg_preset", d.bg_preset);
  prefs.putUChar("timeout", d.timeout_id);
  prefs.putUChar("idle_mode", d.idle_mode);
  prefs.putUChar("brightness", d.brightness);
  prefs.putUChar("rot180", d.rotate_180 ? 1 : 0);
  prefs.putUChar("clk24", d.clock_24h ? 1 : 0);
  prefs.putBool("setup_done", d.setup_done);
  prefs.remove("setup"); /* drop thin-shell alias so load won't resurrect it */
  prefs.putLong64("clock_off", d.clock_offset_ms);
  prefs.putUInt("wall_ep", d.wall_epoch);
  prefs.putUInt("clock_gen", d.clock_sync_gen);
  prefs.putInt("hs_2048", d.high_score_2048);
  prefs.putString("wifi_ssid", d.wifi_ssid);
  prefs.putString("wifi_pass", d.wifi_pass);

  for (int i = 0; i < app::kEmojiSlots; ++i) {
    char key[12];
    std::snprintf(key, sizeof(key), "e%d", i);
    prefs.putString(key, d.emojis[i]);
  }
  for (int i = 0; i < app::kCannedCount; ++i) {
    char key[12];
    std::snprintf(key, sizeof(key), "c%d", i);
    prefs.putString(key, d.canned[i]);
  }

  prefs.putInt("peers", d.peer_count);
  for (int i = 0; i < app::kMaxPeers; ++i) {
    char ik[12], nk[12], legacy[8];
    std::snprintf(ik, sizeof(ik), "p%di", i);
    std::snprintf(nk, sizeof(nk), "p%dn", i);
    std::snprintf(legacy, sizeof(legacy), "p%d", i);
    if (i < d.peer_count) {
      prefs.putString(ik, d.peers[i].id);
      prefs.putString(nk, d.peers[i].name);
    } else {
      prefs.remove(ik);
      prefs.remove(nk);
    }
    prefs.remove(legacy); /* thin-shell "mac|Name" slots */
  }
  prefs.end();
}

void wipe() {
  if (!prefs.begin("werkpager", false)) return;
  prefs.clear();
  prefs.end();
}

/* Preferences putBytes is capped ~4000B — chunk large game blobs. */
constexpr size_t kChunk = 3500;
constexpr int kMaxChunks = 8;

bool load_games_blob(void * dst, size_t * len_io) {
  if (!dst || !len_io || !*len_io) return false;
  if (!prefs.begin("werkpager", true)) return false;
  const size_t total = (size_t)prefs.getUInt("glen", 0);
  if (total == 0 || total > *len_io) {
    prefs.end();
    return false;
  }
  auto * out = static_cast<uint8_t *>(dst);
  size_t got = 0;
  for (int i = 0; i < kMaxChunks && got < total; ++i) {
    char key[8];
    std::snprintf(key, sizeof(key), "g%d", i);
    const size_t want = total - got > kChunk ? kChunk : total - got;
    const size_t n = prefs.getBytes(key, out + got, want);
    if (n != want) {
      prefs.end();
      return false;
    }
    got += n;
  }
  prefs.end();
  *len_io = got;
  return got == total;
}

bool save_games_blob(const void * src, size_t len) {
  if (!prefs.begin("werkpager", false)) return false;
  if (!src || !len) {
    prefs.putUInt("glen", 0);
    for (int i = 0; i < kMaxChunks; ++i) {
      char key[8];
      std::snprintf(key, sizeof(key), "g%d", i);
      prefs.remove(key);
    }
    prefs.end();
    return true;
  }
  if (len > kChunk * kMaxChunks) {
    prefs.end();
    return false;
  }
  const auto * in = static_cast<const uint8_t *>(src);
  size_t off = 0;
  int i = 0;
  for (; i < kMaxChunks && off < len; ++i) {
    char key[8];
    std::snprintf(key, sizeof(key), "g%d", i);
    const size_t n = len - off > kChunk ? kChunk : len - off;
    if (prefs.putBytes(key, in + off, n) != n) {
      prefs.end();
      return false;
    }
    off += n;
  }
  for (; i < kMaxChunks; ++i) {
    char key[8];
    std::snprintf(key, sizeof(key), "g%d", i);
    prefs.remove(key);
  }
  prefs.putUInt("glen", (uint32_t)len);
  prefs.end();
  return true;
}

bool load_timer_blob(void * dst, size_t * len_io) {
  if (!dst || !len_io || !*len_io) return false;
  if (!prefs.begin("werkpager", true)) return false;
  const size_t n = prefs.getBytesLength("tmr");
  if (n == 0 || n > *len_io) {
    prefs.end();
    return false;
  }
  const size_t got = prefs.getBytes("tmr", dst, n);
  prefs.end();
  if (got != n) return false;
  *len_io = got;
  return true;
}

bool save_timer_blob(const void * src, size_t len) {
  if (!prefs.begin("werkpager", false)) return false;
  if (!src || !len) {
    prefs.remove("tmr");
    prefs.end();
    return true;
  }
  const bool ok = prefs.putBytes("tmr", src, len) == len;
  prefs.end();
  return ok;
}

}  // namespace storage
}  // namespace wp
