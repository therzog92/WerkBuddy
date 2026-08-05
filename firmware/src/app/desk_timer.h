#pragma once

#include <cstdint>

namespace wp {
namespace desk_timer {

enum class State : uint8_t {
  Idle = 0,     /* no active countdown */
  Running,
  Paused,
  Finished,     /* rang; waiting for dismiss */
};

void init(); /* start background ticker once at boot */

State state();
bool is_active(); /* running or paused */
bool is_finished();

/** Selected / last duration (ms). Default 25 min. */
uint32_t duration_ms();
void set_duration_ms(uint32_t ms);

/** Time left while running/paused/finished(0). */
uint32_t remaining_ms();

void start();
void pause();
void resume();
void reset();     /* back to Idle, keeps duration */
void dismiss();   /* clear Finished → Idle */

/** "25:00" / "4:59" into buf. */
void format_remaining(char * buf, uint32_t n);

/** UI refresh hook — scr_timer registers while visible. */
using ListenFn = void (*)();
void set_listener(ListenFn fn);

}  // namespace desk_timer
}  // namespace wp
