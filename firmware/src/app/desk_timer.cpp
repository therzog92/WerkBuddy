#include "app/desk_timer.h"

#include "ui/chrome.h"
#include "ui/nav.h"

#include "lvgl/lvgl.h"

namespace wp {
namespace desk_timer {
namespace {

constexpr uint32_t kDefaultMs = 30 * 60 * 1000;
constexpr uint32_t kMinMs = 1000; /* 1 second */
constexpr uint32_t kMaxMs = 23 * 3600 * 1000 + 59 * 60 * 1000 + 59 * 1000; /* 23:59:59 */

struct Slot {
  State state = State::Idle;
  uint32_t duration_ms = kDefaultMs;
  uint32_t remaining_ms = kDefaultMs;
  uint32_t ends_at_ms = 0;
};

Slot g_slots[kSlots];
int g_selected = 0;
ListenFn g_listen = nullptr;
lv_timer_t * g_tick = nullptr;
bool g_firing = false;

Slot & slot_ref(int i) {
  if (i < 0 || i >= kSlots) i = 0;
  return g_slots[i];
}

void notify() {
  if (g_listen) g_listen();
}

void fire_done(int i) {
  if (g_firing) return;
  g_firing = true;
  Slot & s = slot_ref(i);
  s.state = State::Finished;
  s.remaining_ms = 0;
  s.ends_at_ms = 0;
  g_listen = nullptr;
  if (ui::current_screen() == ui::Screen::Idle) ui::wake_from_idle();
  ui::go_timer();
  g_firing = false;
}

void on_tick(lv_timer_t * /*t*/) {
  bool sec_changed = false;
  for (int i = 0; i < kSlots; ++i) {
    Slot & s = g_slots[i];
    if (s.state != State::Running) continue;
    const int32_t left = (int32_t)(s.ends_at_ms - lv_tick_get());
    if (left <= 0) {
      fire_done(i);
      return;
    }
    const uint32_t next = (uint32_t)left;
    if ((next / 1000) != (s.remaining_ms / 1000)) sec_changed = true;
    s.remaining_ms = next;
  }
  if (sec_changed) notify();
}

uint32_t clamp_ms(uint32_t ms) {
  if (ms < kMinMs) return kMinMs;
  if (ms > kMaxMs) return kMaxMs;
  return ms;
}

}  // namespace

void init() {
  if (g_tick) return;
  g_tick = lv_timer_create(on_tick, 200, nullptr);
}

int selected() { return g_selected; }

void select(int slot) {
  if (slot < 0 || slot >= kSlots) return;
  g_selected = slot;
  notify();
}

State state(int slot) { return slot_ref(slot).state; }
State state() { return state(g_selected); }

bool is_active(int slot) {
  const State st = state(slot);
  return st == State::Running || st == State::Paused;
}

bool is_active() {
  for (int i = 0; i < kSlots; ++i)
    if (is_active(i)) return true;
  return false;
}

bool is_finished() {
  for (int i = 0; i < kSlots; ++i)
    if (g_slots[i].state == State::Finished) return true;
  return false;
}

int finished_slot() {
  for (int i = 0; i < kSlots; ++i)
    if (g_slots[i].state == State::Finished) return i;
  return -1;
}

uint32_t duration_ms(int slot) { return slot_ref(slot).duration_ms; }
uint32_t duration_ms() { return duration_ms(g_selected); }

void set_duration_ms(int slot, uint32_t ms) {
  ms = clamp_ms(ms);
  Slot & s = slot_ref(slot);
  s.duration_ms = ms;
  if (s.state == State::Idle) {
    s.remaining_ms = ms;
    notify();
  }
}

void set_duration_ms(uint32_t ms) { set_duration_ms(g_selected, ms); }

uint32_t remaining_ms(int slot) {
  Slot & s = slot_ref(slot);
  if (s.state == State::Running) {
    const int32_t left = (int32_t)(s.ends_at_ms - lv_tick_get());
    return left > 0 ? (uint32_t)left : 0;
  }
  if (s.state == State::Finished) return 0;
  return s.remaining_ms;
}

uint32_t remaining_ms() { return remaining_ms(g_selected); }

void start(int slot) {
  Slot & s = slot_ref(slot);
  if (s.state == State::Finished) dismiss(slot);
  if (s.state == State::Paused) {
    resume(slot);
    return;
  }
  if (s.remaining_ms < 1000) s.remaining_ms = s.duration_ms;
  s.ends_at_ms = lv_tick_get() + s.remaining_ms;
  s.state = State::Running;
  notify();
}

void start() { start(g_selected); }

void pause(int slot) {
  Slot & s = slot_ref(slot);
  if (s.state != State::Running) return;
  s.remaining_ms = remaining_ms(slot);
  s.ends_at_ms = 0;
  s.state = State::Paused;
  notify();
}

void pause() { pause(g_selected); }

void resume(int slot) {
  Slot & s = slot_ref(slot);
  if (s.state != State::Paused) return;
  if (s.remaining_ms < 1000) {
    fire_done(slot);
    return;
  }
  s.ends_at_ms = lv_tick_get() + s.remaining_ms;
  s.state = State::Running;
  notify();
}

void resume() { resume(g_selected); }

void reset(int slot) {
  Slot & s = slot_ref(slot);
  s.state = State::Idle;
  s.ends_at_ms = 0;
  s.remaining_ms = s.duration_ms;
  notify();
}

void reset() { reset(g_selected); }

void dismiss(int slot) {
  Slot & s = slot_ref(slot);
  s.state = State::Idle;
  s.ends_at_ms = 0;
  s.remaining_ms = s.duration_ms;
  notify();
}

void dismiss() {
  const int i = finished_slot();
  if (i >= 0) dismiss(i);
  else reset(g_selected);
}

void format_remaining(int slot, char * buf, uint32_t n) {
  uint32_t ms = remaining_ms(slot);
  if (state(slot) == State::Idle) ms = duration_ms(slot);
  const uint32_t sec = ms / 1000;
  const uint32_t h = sec / 3600;
  const uint32_t m = (sec / 60) % 60;
  const uint32_t s = sec % 60;
  if (h > 0) lv_snprintf(buf, n, "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  else lv_snprintf(buf, n, "%u:%02u", (unsigned)m, (unsigned)s);
}

void format_remaining(char * buf, uint32_t n) { format_remaining(g_selected, buf, n); }

void set_listener(ListenFn fn) { g_listen = fn; }

}  // namespace desk_timer
}  // namespace wp
