#include "storage_nvs.h"

#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace wp {
namespace storage {
namespace {

Preferences prefs;

void load_peers(shell::Desk & desk) {
  desk.peer_count = 0;
  char key[8];
  for (int i = 0; i < shell::kMaxPeers; ++i) {
    std::snprintf(key, sizeof(key), "p%d", i);
    String v = prefs.getString(key, "");
    if (v.length() == 0) break;
    const int bar = v.indexOf('|');
    if (bar <= 0) continue;
    shell::Peer & p = desk.peers[desk.peer_count++];
    std::snprintf(p.id, sizeof(p.id), "%s", v.substring(0, bar).c_str());
    std::snprintf(p.name, sizeof(p.name), "%s", v.substring(bar + 1).c_str());
  }
}

void save_peers(const shell::Desk & desk) {
  char key[8];
  for (int i = 0; i < shell::kMaxPeers; ++i) {
    std::snprintf(key, sizeof(key), "p%d", i);
    if (i < desk.peer_count) {
      char buf[48];
      std::snprintf(buf, sizeof(buf), "%s|%s", desk.peers[i].id, desk.peers[i].name);
      prefs.putString(key, buf);
    } else {
      prefs.remove(key);
    }
  }
}

}  // namespace

bool load(shell::Desk & desk) {
  if (!prefs.begin("werkpager", true)) return false;
  const bool has = prefs.isKey("name");
  if (has) {
    String n = prefs.getString("name", "Desk");
    std::snprintf(desk.name, sizeof(desk.name), "%s", n.c_str());
    desk.theme = (uint8_t)prefs.getUChar("theme", 0);
    desk.timeout_id = (uint8_t)prefs.getUChar("timeout", 2);
    desk.idle_mode = (uint8_t)prefs.getUChar("idle", 1);
    desk.setup_done = prefs.getBool("setup", false);
    desk.clock_offset_ms = prefs.getLong64("clk_off", 0);
    load_peers(desk);
  }
  prefs.end();
  return has;
}

void save(const shell::Desk & desk) {
  if (!prefs.begin("werkpager", false)) return;
  prefs.putString("name", desk.name);
  prefs.putUChar("theme", desk.theme);
  prefs.putUChar("timeout", desk.timeout_id);
  prefs.putUChar("idle", desk.idle_mode);
  prefs.putBool("setup", desk.setup_done);
  prefs.putLong64("clk_off", desk.clock_offset_ms);
  save_peers(desk);
  prefs.end();
}

}  // namespace storage
}  // namespace wp
