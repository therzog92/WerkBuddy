#pragma once

#include <cstdint>

namespace wp {
namespace checklist {

constexpr int kMaxItems = 7;
constexpr int kMaxText = 40;

struct Item {
  char text[kMaxText] = {};
  bool done = false;
};

void init();
int count();
const Item * at(int i);
bool add(const char * text);
void toggle(int i);
void remove(int i);
void clear_done();
/** Wipe all checklist items (factory reset). */
void clear_all();
void save();

}  // namespace checklist
}  // namespace wp
