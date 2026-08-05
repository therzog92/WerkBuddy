#include "app/checklist.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace checklist {
namespace {

const char * kFile = "werkpager_checklist.ini";

Item g_items[kMaxItems] = {};
int g_count = 0;
bool g_loaded = false;

}  // namespace

void init() {
  if (g_loaded) return;
  g_loaded = true;
  FILE * f = std::fopen(kFile, "rb");
  if (!f) return;
  char line[96];
  while (std::fgets(line, sizeof(line), f) && g_count < kMaxItems) {
    line[std::strcspn(line, "\r\n")] = '\0';
    /* 0|text or 1|text */
    char * bar = std::strchr(line, '|');
    if (!bar || bar == line) continue;
    *bar = '\0';
    Item & it = g_items[g_count];
    it.done = line[0] == '1';
    std::snprintf(it.text, sizeof(it.text), "%s", bar + 1);
    if (it.text[0]) ++g_count;
  }
  std::fclose(f);
}

void save() {
  FILE * f = std::fopen(kFile, "wb");
  if (!f) return;
  for (int i = 0; i < g_count; ++i) {
    std::fprintf(f, "%d|%s\n", g_items[i].done ? 1 : 0, g_items[i].text);
  }
  std::fclose(f);
}

int count() {
  init();
  return g_count;
}

const Item * at(int i) {
  init();
  if (i < 0 || i >= g_count) return nullptr;
  return &g_items[i];
}

bool add(const char * text) {
  init();
  if (g_count >= kMaxItems) return false;
  if (!text || !text[0]) return false;
  std::snprintf(g_items[g_count].text, sizeof(g_items[g_count].text), "%s", text);
  g_items[g_count].done = false;
  ++g_count;
  save();
  return true;
}

void toggle(int i) {
  init();
  if (i < 0 || i >= g_count) return;
  g_items[i].done = !g_items[i].done;
  save();
}

void remove(int i) {
  init();
  if (i < 0 || i >= g_count) return;
  for (int j = i; j < g_count - 1; ++j) g_items[j] = g_items[j + 1];
  --g_count;
  save();
}

void clear_done() {
  init();
  int w = 0;
  for (int i = 0; i < g_count; ++i) {
    if (!g_items[i].done) g_items[w++] = g_items[i];
  }
  g_count = w;
  save();
}

void clear_all() {
  init();
  g_count = 0;
  save();
}

}  // namespace checklist
}  // namespace wp
