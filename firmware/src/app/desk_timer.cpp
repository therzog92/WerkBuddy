#include "app/desk_timer.h"

#include "app/app.h"
#include "app/storage.h"
#include "ui/chrome.h"
#include "ui/nav.h"

#include "lvgl/lvgl.h"

#include <cstdio>
#include <cstring>

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
  uint32_t ends_at_ms = 0;       /* lv_tick deadline while running */
  uint32_t ends_at_wall = 0;     /* wall unix when it should fire (survives reboot) */
};

Slot g_slots[kSlots];
int g_selected = 0;
ListenFn g_listen = nullptr;
lv_timer_t * g_tick = nullptr;
bool g_firing = false;
bool g_persist_armed = false;

Slot & slot_ref(int i) {
  if (i < 0 || i >= kSlots) i = 0;
  return g_slots[i];
}

void notify() {
  if (g_listen) g_listen();
}

void persist_now();

void fire_done(int i) {
  if (g_firing) return;
  g_firing = true;
  Slot & s = slot_ref(i);
  s.state = State::Finished;
  s.remaining_ms = 0;
  s.ends_at_ms = 0;
  s.ends_at_wall = 0;
  g_listen = nullptr;
  persist_now();
  if (ui::current_screen() == ui::Screen::Idle) ui::wake_from_idle();
  ui::go_timer();
  g_firing = false;
}

void on_tick(lv_timer_t * /*t*/) {
  bool sec_changed = false;
  for (int i = 0; i < kSlots; ++i) {
    Slot & s = g_slots[i];
    if (s.state != State::Running) continue;
    /* Monotonic lv_tick while powered on — a wall-clock jump must not fire the timer. */
    const int32_t left = (int32_t)(s.ends_at_ms - lv_tick_get());
    if (left <= 0) {
      fire_done(i);
      return;
    }
    const uint32_t next = (uint32_t)left;
    if ((next / 1000) != (s.remaining_ms / 1000)) sec_changed = true;
    s.remaining_ms = next;
  }
  if (sec_changed) {
    persist_now();
    notify();
  }
}

uint32_t clamp_ms(uint32_t ms) {
  if (ms < kMinMs) return kMinMs;
  if (ms > kMaxMs) return kMaxMs;
  return ms;
}

#pragma pack(push, 1)
struct PersistBlob {
  uint32_t magic;
  uint8_t state[kSlots];
  uint32_t duration_ms[kSlots];
  uint32_t remaining_ms[kSlots];
  uint32_t ends_at_wall[kSlots];
};
#pragma pack(pop)

constexpr uint32_t kPersistMagic = 0x544D5231u; /* TMR1 */

void persist_now() {
#if defined(WP_DEVICE)
  PersistBlob b{};
  b.magic = kPersistMagic;
  for (int i = 0; i < kSlots; ++i) {
    b.state[i] = (uint8_t)g_slots[i].state;
    b.duration_ms[i] = g_slots[i].duration_ms;
    b.remaining_ms[i] = g_slots[i].remaining_ms;
    b.ends_at_wall[i] = g_slots[i].ends_at_wall;
  }
  storage::save_timer_blob(&b, sizeof(b));
#else
  (void)0;
#endif
}

void restore_from_blob() {
#if defined(WP_DEVICE)
  PersistBlob b{};
  size_t n = sizeof(b);
  if (!storage::load_timer_blob(&b, &n) || n != sizeof(b) || b.magic != kPersistMagic) return;
  const uint32_t wall = app::wall_unix();
  for (int i = 0; i < kSlots; ++i) {
    Slot & s = g_slots[i];
    s.duration_ms = clamp_ms(b.duration_ms[i] ? b.duration_ms[i] : kDefaultMs);
    s.remaining_ms = b.remaining_ms[i] ? b.remaining_ms[i] : s.duration_ms;
    s.ends_at_wall = b.ends_at_wall[i];
    const auto st = static_cast<State>(b.state[i]);
    if (st == State::Running && s.ends_at_wall && wall >= 1700000000u) {
      if (wall >= s.ends_at_wall) {
        s.state = State::Finished;
        s.remaining_ms = 0;
        s.ends_at_wall = 0;
        s.ends_at_ms = 0;
      } else {
        s.state = State::Running;
        s.remaining_ms = (s.ends_at_wall - wall) * 1000u;
        s.ends_at_ms = lv_tick_get() + s.remaining_ms;
      }
    } else if (st == State::Running) {
      /* No valid wall yet — resume from last persisted remaining. */
      s.state = State::Running;
      if (s.remaining_ms < 1000) s.remaining_ms = s.duration_ms;
      s.ends_at_ms = lv_tick_get() + s.remaining_ms;
    } else if (st == State::Paused || st == State::Finished) {
      s.state = st;
      s.ends_at_ms = 0;
      if (st != State::Finished) s.ends_at_wall = 0;
    } else {
      s.state = State::Idle;
      s.ends_at_wall = 0;
      s.ends_at_ms = 0;
      s.remaining_ms = s.duration_ms;
    }
  }
  /* If any finished while powered off, open the timer UI once UI is up. */
  for (int i = 0; i < kSlots; ++i) {
    if (g_slots[i].state == State::Finished) {
      app::schedule(400, [](void * /*ud*/) { ui::go_timer(); }, nullptr);
      break;
    }
  }
#endif
}

}  // namespace

void init() {
  if (g_tick) return;
  g_tick = lv_timer_create(on_tick, 200, nullptr);
  restore_from_blob();
  g_persist_armed = true;
}

int selected() { return g_selected; }
void select(int slot) {
  if (slot < 0 || slot >= kSlots) return;
  g_selected = slot;
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
    if (state(i) == State::Finished) return true;
  return false;
}
int finished_slot() {
  for (int i = 0; i < kSlots; ++i)
    if (state(i) == State::Finished) return i;
  return -1;
}

uint32_t duration_ms(int slot) { return slot_ref(slot).duration_ms; }
uint32_t duration_ms() { return duration_ms(g_selected); }
void set_duration_ms(int slot, uint32_t ms) {
  Slot & s = slot_ref(slot);
  s.duration_ms = clamp_ms(ms);
  if (s.state == State::Idle) s.remaining_ms = s.duration_ms;
  persist_now();
}
void set_duration_ms(uint32_t ms) { set_duration_ms(g_selected, ms); }

uint32_t remaining_ms(int slot) {
  Slot & s = slot_ref(slot);
  if (s.state == State::Running) {
    const int32_t left = (int32_t)(s.ends_at_ms - lv_tick_get());
    return left > 0 ? (uint32_t)left : 0;
  }
  return s.remaining_ms;
}
uint32_t remaining_ms() { return remaining_ms(g_selected); }

void start(int slot) {
  Slot & s = slot_ref(slot);
  if (s.remaining_ms < 1000) s.remaining_ms = s.duration_ms;
  s.state = State::Running;
  s.ends_at_ms = lv_tick_get() + s.remaining_ms;
  const uint32_t wall = app::wall_unix();
  if (wall >= 1700000000u)
    s.ends_at_wall = wall + (s.remaining_ms + 999) / 1000;
  else
    s.ends_at_wall = 0;
  persist_now();
  notify();
}
void start() { start(g_selected); }

void pause(int slot) {
  Slot & s = slot_ref(slot);
  if (s.state != State::Running) return;
  s.remaining_ms = remaining_ms(slot);
  s.state = State::Paused;
  s.ends_at_ms = 0;
  s.ends_at_wall = 0;
  persist_now();
  notify();
}
void pause() { pause(g_selected); }

void resume(int slot) {
  Slot & s = slot_ref(slot);
  if (s.state != State::Paused) return;
  if (s.remaining_ms < 1000) {
    start(slot);
    return;
  }
  s.state = State::Running;
  s.ends_at_ms = lv_tick_get() + s.remaining_ms;
  const uint32_t wall = app::wall_unix();
  if (wall >= 1700000000u)
    s.ends_at_wall = wall + (s.remaining_ms + 999) / 1000;
  else
    s.ends_at_wall = 0;
  persist_now();
  notify();
}
void resume() { resume(g_selected); }

void reset(int slot) {
  Slot & s = slot_ref(slot);
  s.state = State::Idle;
  s.remaining_ms = s.duration_ms;
  s.ends_at_ms = 0;
  s.ends_at_wall = 0;
  persist_now();
  notify();
}
void reset() { reset(g_selected); }

void dismiss(int slot) {
  Slot & s = slot_ref(slot);
  if (s.state != State::Finished) return;
  s.state = State::Idle;
  s.remaining_ms = s.duration_ms;
  s.ends_at_wall = 0;
  persist_now();
  notify();
}
void dismiss() {
  const int i = finished_slot();
  if (i >= 0) dismiss(i);
}

void format_remaining(int slot, char * buf, uint32_t n) {
  uint32_t ms = remaining_ms(slot);
  const uint32_t total_s = (ms + 999) / 1000;
  const uint32_t h = total_s / 3600;
  const uint32_t m = (total_s % 3600) / 60;
  const uint32_t s = total_s % 60;
  if (h > 0)
    lv_snprintf(buf, n, "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  else
    lv_snprintf(buf, n, "%u:%02u", (unsigned)m, (unsigned)s);
}
void format_remaining(char * buf, uint32_t n) { format_remaining(g_selected, buf, n); }

void set_listener(ListenFn fn) { g_listen = fn; }

void rebase_wall() {
  const uint32_t wall = app::wall_unix();
  for (int i = 0; i < kSlots; ++i) {
    Slot & s = g_slots[i];
    if (s.state != State::Running) continue;
    s.remaining_ms = remaining_ms(i);
    s.ends_at_ms = lv_tick_get() + s.remaining_ms;
    if (wall >= 1700000000u)
      s.ends_at_wall = wall + (s.remaining_ms + 999) / 1000;
    else
      s.ends_at_wall = 0;
  }
  persist_now();
}

}  // namespace desk_timer
}  // namespace wp
