#include "app/score_log.h"

#include "app/app.h"

#include "lvgl/lvgl.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace wp {
namespace score_log {
namespace {

#ifdef WP_DEVICE
const char * kFile = "/littlefs/werkpager_score_log.ini";
#else
const char * kFile = "werkpager_score_log.ini";
#endif

Entry g_entries[kMaxEntries] = {};
int g_count = 0;
int g_head = 0;
bool g_loaded = false;

char g_dedupe_game[kMaxGame] = {};
char g_dedupe_peer[kMaxPeer] = {};
Outcome g_dedupe_outcome = Outcome::Win;
uint32_t g_dedupe_tick = 0;
bool g_has_dedupe = false;

int64_t wall_stamp() {
  std::tm tm{};
  app::local_time(&tm);
  return (int64_t)(tm.tm_year + 1900) * 10000000000LL + (int64_t)(tm.tm_mon + 1) * 100000000LL +
         (int64_t)tm.tm_mday * 1000000LL + (int64_t)tm.tm_hour * 10000LL +
         (int64_t)tm.tm_min * 100LL + (int64_t)tm.tm_sec;
}

void copy_field(char * dst, size_t n, const char * src) {
  if (!src) src = "";
  std::snprintf(dst, n, "%s", src);
}

Outcome parse_outcome(const char * s) {
  if (!s) return Outcome::Win;
  if (s[0] == 'l' || s[0] == 'L') return Outcome::Lose;
  if (s[0] == 't' || s[0] == 'T') return Outcome::Tie;
  if (s[0] == 'f' || s[0] == 'F') return Outcome::ForfeitSelf;
  if (s[0] == 'o' || s[0] == 'O') return Outcome::ForfeitOpp;
  return Outcome::Win;
}

char outcome_char(Outcome o) {
  switch (o) {
    case Outcome::Lose: return 'l';
    case Outcome::Tie: return 't';
    case Outcome::ForfeitSelf: return 'f';
    case Outcome::ForfeitOpp: return 'o';
    default: return 'w';
  }
}

}  // namespace

void init() {
  if (g_loaded) return;
  g_loaded = true;
  FILE * f = std::fopen(kFile, "rb");
  if (!f) return;
  char line[160];
  while (std::fgets(line, sizeof(line), f) && g_count < kMaxEntries) {
    char * p = line;
    p[std::strcspn(p, "\r\n")] = '\0';
    char * f1 = std::strchr(p, '|');
    if (!f1) continue;
    *f1++ = '\0';
    char * f2 = std::strchr(f1, '|');
    if (!f2) continue;
    *f2++ = '\0';
    char * f3 = std::strchr(f2, '|');
    if (!f3) continue;
    *f3++ = '\0';
    Entry & e = g_entries[g_count];
    copy_field(e.game, sizeof(e.game), p);
    copy_field(e.peer_name, sizeof(e.peer_name), f1);
    e.outcome = parse_outcome(f2);
    e.stamp = std::atoll(f3);
    ++g_count;
  }
  g_head = g_count % kMaxEntries;
  std::fclose(f);
}

void save() {
  FILE * f = std::fopen(kFile, "wb");
  if (!f) return;
  const int n = g_count;
  for (int i = 0; i < n; ++i) {
    const int idx = (g_head - n + i + kMaxEntries * 2) % kMaxEntries;
    const Entry & e = g_entries[idx];
    std::fprintf(f, "%s|%s|%c|%lld\n", e.game, e.peer_name, outcome_char(e.outcome),
                 (long long)e.stamp);
  }
  std::fclose(f);
}

void note(const char * game, const char * peer_name, Outcome outcome) {
  init();
  if (!game || !game[0]) game = "Game";
  if (!peer_name || !peer_name[0]) peer_name = "—";
  const uint32_t now = lv_tick_get();
  /* Skip UI rebuild spam for a few seconds; allow another identical result later. */
  if (g_has_dedupe && g_dedupe_outcome == outcome && (now - g_dedupe_tick) < 2500 &&
      !std::strcmp(g_dedupe_game, game) && !std::strcmp(g_dedupe_peer, peer_name)) {
    return;
  }
  copy_field(g_dedupe_game, sizeof(g_dedupe_game), game);
  copy_field(g_dedupe_peer, sizeof(g_dedupe_peer), peer_name);
  g_dedupe_outcome = outcome;
  g_dedupe_tick = now;
  g_has_dedupe = true;

  Entry & e = g_entries[g_head];
  copy_field(e.game, sizeof(e.game), game);
  copy_field(e.peer_name, sizeof(e.peer_name), peer_name);
  e.outcome = outcome;
  e.stamp = wall_stamp();
  g_head = (g_head + 1) % kMaxEntries;
  if (g_count < kMaxEntries) ++g_count;
  save();
}

void clear() {
  init();
  g_count = 0;
  g_head = 0;
  g_has_dedupe = false;
  save();
}

int count() {
  init();
  return g_count;
}

const Entry * at(int newest_index) {
  init();
  if (newest_index < 0 || newest_index >= g_count) return nullptr;
  const int idx = (g_head - 1 - newest_index + kMaxEntries * 2) % kMaxEntries;
  return &g_entries[idx];
}

const char * outcome_label(Outcome o) {
  switch (o) {
    case Outcome::Win: return "Won";
    case Outcome::Lose: return "Lost";
    case Outcome::Tie: return "Tie";
    case Outcome::ForfeitSelf: return "You Forfeited";
    case Outcome::ForfeitOpp: return "Opp. Forfeited";
    default: return "?";
  }
}

}  // namespace score_log
}  // namespace wp
