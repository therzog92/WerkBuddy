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

}  // namespace brightness
}  // namespace ui
}  // namespace wp
