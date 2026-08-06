#pragma once

namespace wp {
namespace sim {

/** Start async playthrough of one game ("ttt") or "all" vs Will bot. */
bool playthrough_start(const char * which);

/** Advance state machine; call from drive poll. Returns true when a reply is ready. */
bool playthrough_tick();

/** Reply line (no trailing newline) when tick returns true. */
const char * playthrough_reply();

bool playthrough_busy();

}  // namespace sim
}  // namespace wp
