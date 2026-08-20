#include "app/presence.h"

#include "app/app.h"
#include "net/link.h"

#include "lvgl/lvgl.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace app {
namespace {

uint32_t g_peer_last_ms[kMaxPeers] = {};
bool g_peer_remote_dnd[kMaxPeers] = {};

bool same_id(const char * a, const char * b) {
  if (!a || !b) return false;
  return std::strcmp(a, b) == 0;
}

void presence_beacon_cb(lv_timer_t * /*t*/) { broadcast_presence(); }

}  // namespace

bool dnd() { return desk().dnd != 0; }

void set_dnd(bool on) {
  Desk & d = desk();
  const bool was = d.dnd != 0;
  d.dnd = on ? 1 : 0;
  if (was != on) {
    save();
    broadcast_presence();
  }
}

int peer_index(const char * id) {
  if (!id || !id[0]) return -1;
  const Desk & d = desk();
  for (int i = 0; i < d.peer_count; ++i) {
    if (same_id(d.peers[i].id, id)) return i;
  }
  return -1;
}

bool peer_present_idx(int idx) {
  if (idx < 0 || idx >= desk().peer_count) return false;
  if (!g_peer_last_ms[idx]) return false;
  return lv_tick_elaps(g_peer_last_ms[idx]) < kPresenceMs;
}

bool peer_present(const char * id) { return peer_present_idx(peer_index(id)); }

bool peer_remote_dnd(const char * id) {
  const int i = peer_index(id);
  if (i < 0) return false;
  return g_peer_remote_dnd[i];
}

const char * peer_presence_subtitle(int idx, char * buf, size_t buf_n) {
  if (!buf || buf_n == 0) return nullptr;
  buf[0] = '\0';
  if (idx < 0 || idx >= desk().peer_count) return nullptr;
  if (!peer_present_idx(idx)) {
    lv_snprintf(buf, (uint32_t)buf_n, "Not nearby");
    return buf;
  }
  if (g_peer_remote_dnd[idx]) {
    lv_snprintf(buf, (uint32_t)buf_n, "Do not disturb");
    return buf;
  }
  return nullptr;
}

bool peer_contact_ok(int idx, char * why, size_t why_n) {
  if (idx < 0 || idx >= desk().peer_count) {
    if (why && why_n) lv_snprintf(why, (uint32_t)why_n, "Unknown desk");
    return false;
  }
  return peer_contact_ok_id(desk().peers[idx].id, why, why_n);
}

bool peer_contact_ok_id(const char * id, char * why, size_t why_n) {
  const int idx = peer_index(id);
  if (idx < 0) {
    if (why && why_n) lv_snprintf(why, (uint32_t)why_n, "Unknown desk");
    return false;
  }
  const Desk & d = desk();
  if (!peer_present_idx(idx)) {
    if (why && why_n) lv_snprintf(why, (uint32_t)why_n, "%s is not nearby", d.peers[idx].name);
    return false;
  }
  if (g_peer_remote_dnd[idx]) {
    if (why && why_n) lv_snprintf(why, (uint32_t)why_n, "%s is busy", d.peers[idx].name);
    return false;
  }
  return true;
}

void note_peer_presence(const char * id, const char * name, bool remote_dnd) {
  if (!id || !id[0]) return;
  touch_peer_name(id, name);
  const int i = peer_index(id);
  if (i < 0) return;
  g_peer_last_ms[i] = lv_tick_get();
  g_peer_remote_dnd[i] = remote_dnd;
}

void broadcast_presence() {
  Desk & d = desk();
  proto::Msg m{};
  m.type = proto::MsgType::Status;
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", d.id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", d.name[0] ? d.name : "Desk");
  m.to_id[0] = '\0';
  m.hit = d.dnd != 0;
  send(m);
}

void presence_tick() { broadcast_presence(); }

void presence_init() {
  lv_timer_create(presence_beacon_cb, kPresenceBeaconMs, nullptr);
  schedule(2500, [](void * /*ud*/) { broadcast_presence(); }, nullptr);
}

void presence_clear_peer(int idx) {
  if (idx < 0 || idx >= kMaxPeers) return;
  g_peer_last_ms[idx] = 0;
  g_peer_remote_dnd[idx] = false;
}

void presence_on_remove_peer(int removed_idx) {
  for (int i = removed_idx; i < kMaxPeers - 1; ++i) {
    g_peer_last_ms[i] = g_peer_last_ms[i + 1];
    g_peer_remote_dnd[i] = g_peer_remote_dnd[i + 1];
  }
  presence_clear_peer(kMaxPeers - 1);
}

}  // namespace app
}  // namespace wp
