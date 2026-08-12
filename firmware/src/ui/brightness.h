#pragma once

#include <cstdint>

namespace wp {
namespace ui {
namespace brightness {

/** Create the dim overlay (call once after LVGL display exists). */
void init();

/** Re-read desk().brightness and page-boost; update overlay. */
void apply();

/**
 * While true (incoming/outgoing page), force full brightness.
 * Backlight / dim overlay ignored until cleared.
 */
void set_page_boost(bool on);

/** Clamp to 10..100, store on desk, save, apply. */
void set_percent(uint8_t percent);

uint8_t percent();

/**
 * Hardware TFT backlight (device pin). Sim: no-op.
 * Black idle turns this off so desks don't cook the battery in a bag.
 */
void set_panel_on(bool on);

}  // namespace brightness
}  // namespace ui
}  // namespace wp
