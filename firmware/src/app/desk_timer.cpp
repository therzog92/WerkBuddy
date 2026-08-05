#include "app/desk_timer.h"

#include "ui/chrome.h"
#include "ui/nav.h"

#include "lvgl/lvgl.h"

namespace wp {
namespace desk_timer {
namespace {

constexpr uint32_t kDefaultMs = 25 * 60 * 1000;
constexpr uint32_t kMinMs = 1000; /* 1 second */
constexpr uint32_t kMaxMs = 99 * 60 * 1000 + 59 * 1000;

State g_state = State::Idle;
uint32_t g_duration_ms = kDefaultMs;
uint32_t g_remaining_ms = kDefaultMs;
uint32_t g_ends_at_ms = 0;
ListenFn g_listen = nullptr;
lv_timer_t * g_tick = nullptr;
bool g_firing = false;

void notify() {
  if (g_listen) g_listen();
}

void fire_done() {
  if (g_firing) return;
  g_firing = true;
  g_state = State::Finished;
  g_remaining_ms = 0;
  g_ends_at_ms = 0;
  /* Drop UI listener so a mid-rebuild notify can't re-enter go_timer. */
  g_listen = nullptr;
  if (ui::current_screen() == ui::Screen::Idle) ui::wake_from_idle();
  /* Always rebuild — finished uses a full-screen alarm UI. */
  ui::go_timer();
  g_firing = false;
}

void on_tick(lv_timer_t * /*t*/) {
  if (g_state != State::Running) return;
  const int32_t left = (int32_t)(g_ends_at_ms - lv_tick_get());
  if (left <= 0) {
    fire_done();
    return;
  }
  const uint32_t next = (uint32_t)left;
  const bool sec_changed = (next / 1000) != (g_remaining_ms / 1000);
  g_remaining_ms = next;
  if (sec_changed) notify();
}

}  // namespace

void init() {
  if (g_tick) return;
  g_tick = lv_timer_create(on_tick, 200, nullptr);
}

State state() { return g_state; }
bool is_active() { return g_state == State::Running || g_state == State::Paused; }
bool is_finished() { return g_state == State::Finished; }

uint32_t duration_ms() { return g_duration_ms; }

void set_duration_ms(uint32_t ms) {
  if (ms < kMinMs) ms = kMinMs;
  if (ms > kMaxMs) ms = kMaxMs;
  g_duration_ms = ms;
  if (g_state == State::Idle) {
    g_remaining_ms = ms;
    notify();
  }
}

uint32_t remaining_ms() {
  if (g_state == State::Running) {
    const int32_t left = (int32_t)(g_ends_at_ms - lv_tick_get());
    return left > 0 ? (uint32_t)left : 0;
  }
  if (g_state == State::Finished) return 0;
  return g_remaining_ms;
}

void start() {
  if (g_state == State::Finished) dismiss();
  if (g_state == State::Paused) {
    resume();
    return;
  }
  if (g_remaining_ms < 1000) g_remaining_ms = g_duration_ms;
  g_ends_at_ms = lv_tick_get() + g_remaining_ms;
  g_state = State::Running;
  notify();
}

void pause() {
  if (g_state != State::Running) return;
  g_remaining_ms = remaining_ms();
  g_ends_at_ms = 0;
  g_state = State::Paused;
  notify();
}

void resume() {
  if (g_state != State::Paused) return;
  if (g_remaining_ms < 1000) {
    fire_done();
    return;
  }
  g_ends_at_ms = lv_tick_get() + g_remaining_ms;
  g_state = State::Running;
  notify();
}

void reset() {
  g_state = State::Idle;
  g_ends_at_ms = 0;
  g_remaining_ms = g_duration_ms;
  notify();
}

void dismiss() {
  g_state = State::Idle;
  g_ends_at_ms = 0;
  g_remaining_ms = g_duration_ms;
  notify();
}

void format_remaining(char * buf, uint32_t n) {
  uint32_t ms = remaining_ms();
  if (g_state == State::Idle) ms = g_duration_ms;
  const uint32_t sec = ms / 1000;
  const uint32_t m = sec / 60;
  const uint32_t s = sec % 60;
  lv_snprintf(buf, n, "%u:%02u", (unsigned)m, (unsigned)s);
}

void set_listener(ListenFn fn) { g_listen = fn; }

}  // namespace desk_timer
}  // namespace wp
