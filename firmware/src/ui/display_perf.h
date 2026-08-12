#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {
namespace display_perf {

/** Device: remember the LVGL display for render-mode switches. No-op on sim. */
void bind(lv_display_t * disp);

/**
 * Device: FULL mode for full-screen color washes (incoming call / timer alarm).
 * PARTIAL otherwise so OSK / taps only dirty small regions (much less lag).
 */
void prefer_full_frame(bool on);

}  // namespace display_perf
}  // namespace ui
}  // namespace wp
