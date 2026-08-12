#include "desk.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace shell {
namespace {

Desk g_desk;
constexpr uint32_t kTimeoutMs[] = {
    60UL * 1000, 3UL * 60 * 1000, 5UL * 60 * 1000, 10UL * 60 * 1000, 0};
constexpr const char * kTimeoutLbl[] = {"1 min", "3 min", "5 min", "10 min", "Off"};
constexpr const char * kThemeName[] = {"Eleganza", "Ice", "Matcha", "Coral"};
constexpr uint32_t kThemeBg[] = {0x101018, 0x0e1a22, 0x101810, 0x1a1010};
constexpr uint32_t kThemeFg[] = {0xf5f5f5, 0xe8f4f8, 0xf0f5f0, 0xfff0ee};
constexpr uint32_t kThemeAccent[] = {0xf0c040, 0x7ec8e3, 0x8fbc8f, 0xe07060};

}  // namespace

Desk & desk() { return g_desk; }

void desk_defaults() {
  g_desk = Desk{};
  std::snprintf(g_desk.name, sizeof(g_desk.name), "Desk");
}

bool peer_saved(const char * id) {
  if (!id || !id[0]) return false;
  for (int i = 0; i < g_desk.peer_count; ++i) {
    if (std::strcmp(g_desk.peers[i].id, id) == 0) return true;
  }
  return false;
}

void add_peer(const char * id, const char * name) {
  if (!id || !id[0] || !name) return;
  if (peer_saved(id)) {
    for (int i = 0; i < g_desk.peer_count; ++i) {
      if (std::strcmp(g_desk.peers[i].id, id) == 0) {
        std::snprintf(g_desk.peers[i].name, sizeof(g_desk.peers[i].name), "%s", name);
        return;
      }
    }
  }
  if (g_desk.peer_count >= kMaxPeers) return;
  Peer & p = g_desk.peers[g_desk.peer_count++];
  std::snprintf(p.id, sizeof(p.id), "%s", id);
  std::snprintf(p.name, sizeof(p.name), "%s", name);
}

void remove_peer(const char * id) {
  if (!id) return;
  for (int i = 0; i < g_desk.peer_count; ++i) {
    if (std::strcmp(g_desk.peers[i].id, id) != 0) continue;
    for (int j = i + 1; j < g_desk.peer_count; ++j) g_desk.peers[j - 1] = g_desk.peers[j];
    --g_desk.peer_count;
    return;
  }
}

void clear_nearby() { g_desk.nearby_count = 0; }

void add_nearby(const char * id, const char * name) {
  if (!id || !id[0] || !name) return;
  for (int i = 0; i < g_desk.nearby_count; ++i) {
    if (std::strcmp(g_desk.nearby[i].id, id) == 0) {
      std::snprintf(g_desk.nearby[i].name, sizeof(g_desk.nearby[i].name), "%s", name);
      return;
    }
  }
  if (g_desk.nearby_count >= kMaxPeers) return;
  Peer & p = g_desk.nearby[g_desk.nearby_count++];
  std::snprintf(p.id, sizeof(p.id), "%s", id);
  std::snprintf(p.name, sizeof(p.name), "%s", name);
}

uint32_t idle_timeout_ms() {
  if (g_desk.timeout_id >= 5) return 0;
  return kTimeoutMs[g_desk.timeout_id];
}

void local_time(std::tm * out) {
  const time_t now = time(nullptr) + (time_t)(g_desk.clock_offset_ms / 1000);
  *out = *localtime(&now);
}

void set_clock_from_parts(int year, int mon, int day, int hour, int min) {
  std::tm want{};
  want.tm_year = year - 1900;
  want.tm_mon = mon - 1;
  want.tm_mday = day;
  want.tm_hour = hour;
  want.tm_min = min;
  want.tm_sec = 0;
  want.tm_isdst = -1;
  const time_t target = mktime(&want);
  const time_t now = time(nullptr);
  g_desk.clock_offset_ms = (int64_t)(target - now) * 1000;
}

const char * theme_name(uint8_t id) {
  if (id >= 4) return "?";
  return kThemeName[id];
}
uint32_t theme_bg(uint8_t id) { return kThemeBg[id % 4]; }
uint32_t theme_fg(uint8_t id) { return kThemeFg[id % 4]; }
uint32_t theme_accent(uint8_t id) { return kThemeAccent[id % 4]; }

const char * timeout_label(uint8_t id) {
  if (id >= 5) return "?";
  return kTimeoutLbl[id];
}

}  // namespace shell
}  // namespace wp
