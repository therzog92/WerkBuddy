#pragma once

/*
 * Screen orientation — Normal or 180° flip for stand cases that mount either way.
 * Device: pixel + touch remap in the flush/read callbacks.
 * Sim: LVGL display rotation so the SDL window matches.
 */

namespace wp {
namespace ui {
namespace orient {

bool flip_180();
/** Persist + apply immediately. */
void set_flip_180(bool on);
/** Apply desk().rotate_180 to the live display (call after load). */
void apply();

}  // namespace orient
}  // namespace ui
}  // namespace wp
