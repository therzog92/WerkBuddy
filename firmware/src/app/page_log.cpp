#include "app/page_log.h"

#include "app/app.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace wp {
namespace page_log {
namespace {

const char * kFile = "werkpager_page_log.ini";

Entry g_entries[kMaxEntries] = {};
int g_count = 0;
int g_head = 0;
bool g_loaded = false;

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

}  // namespace

void init() {
  if (g_loaded) return;
  g_loaded = true;
  FILE * f = std::fopen(kFile, "rb");
  if (!f) return;
  char line[256];
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
    char * f4 = std::strchr(f3, '|');
    if (!f4) continue;
    *f4++ = '\0';
    Entry & e = g_entries[g_count];
    e.dir = (p[0] == 'o' || p[0] == 'O') ? Dir::Out : Dir::In;
    copy_field(e.peer_name, sizeof(e.peer_name), f1);
    copy_field(e.emoji, sizeof(e.emoji), f2);
    copy_field(e.message, sizeof(e.message), f3);
    e.epoch_ms = std::atoll(f4);
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
    std::fprintf(f, "%c|%s|%s|%s|%lld\n", e.dir == Dir::Out ? 'o' : 'i', e.peer_name, e.emoji,
                 e.message, (long long)e.epoch_ms);
  }
  std::fclose(f);
}

void add(Dir dir, const char * peer_name, const char * emoji, const char * message) {
  init();
  Entry & e = g_entries[g_head];
  e.dir = dir;
  copy_field(e.peer_name, sizeof(e.peer_name), peer_name);
  copy_field(e.emoji, sizeof(e.emoji), emoji);
  copy_field(e.message, sizeof(e.message), message);
  e.epoch_ms = wall_stamp();
  g_head = (g_head + 1) % kMaxEntries;
  if (g_count < kMaxEntries) ++g_count;
  save();
}

void clear() {
  init();
  g_count = 0;
  g_head = 0;
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

}  // namespace page_log
}  // namespace wp
