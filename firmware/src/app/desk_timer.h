#pragma once

#include <cstdint>

namespace wp {
namespace desk_timer {

constexpr int kSlots = 2;

enum class State : uint8_t {
  Idle = 0, /* no active countdown */
  Running,
  Paused,
  Finished, /* rang; waiting for dismiss */
};

void init(); /* start background ticker once at boot */

/** UI focuses this slot for rollers / Start / Reset. */
int selected();
void select(int slot);

State state(int slot);
State state(); /* selected slot */
bool is_active(int slot);
bool is_active(); /* any slot running or paused */
bool is_finished(); /* any slot finished */
int finished_slot(); /* first Finished slot, or -1 */

/** Selected / last duration (ms). Default 25 min. */
uint32_t duration_ms(int slot);
uint32_t duration_ms();
void set_duration_ms(int slot, uint32_t ms);
void set_duration_ms(uint32_t ms); /* selected */

/** Time left while running/paused/finished(0). */
uint32_t remaining_ms(int slot);
uint32_t remaining_ms();

void start(int slot);
void start();
void pause(int slot);
void pause();
void resume(int slot);
void resume();
void reset(int slot); /* back to Idle, keeps duration */
void reset();
void dismiss(int slot); /* clear Finished → Idle */
void dismiss();         /* first finished slot */

/** "4:25:00" / "25:00" / "4:59" into buf. */
void format_remaining(int slot, char * buf, uint32_t n);
void format_remaining(char * buf, uint32_t n);

/** Wall clock changed (Settings / peer sync) — keep remaining time, don't fire. */
void rebase_wall();

/** UI refresh hook — scr_timer registers while visible. */
using ListenFn = void (*)();
void set_listener(ListenFn fn);

}  // namespace desk_timer
}  // namespace wp
