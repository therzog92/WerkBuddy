#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace theme {

enum class Id : uint8_t {
  Eleganza = 0,
  Runway = 1,
  Ice = 2,
  Lemon = 3,
  Matcha = 4,
  Count = 5,
};

void set(Id id);
Id current();
const char * name(Id id);

lv_color_t bg0();
lv_color_t bg1();
lv_color_t panel();
lv_color_t ink();
lv_color_t muted();
lv_color_t gold();
lv_color_t hot();
lv_color_t mint();
lv_color_t danger();
lv_color_t border();
lv_color_t call_a();
lv_color_t call_b();
lv_color_t grad_top();
lv_color_t grad_bot();

void apply_screen_bg(lv_obj_t * scr);

/** Wallpaper dim level when a custom background is set. */
enum class BgWash : uint8_t {
  Hub = 0,  /* photo readable behind home chrome */
  Page,     /* settings / games / etc — considerably dimmed for text */
  Idle,     /* clock lock screen — photo present, dimmed */
};

void apply_screen_bg(lv_obj_t * scr, BgWash wash);

/** Rebuild gradient stops for incoming burst (call after set()). */
void refresh_incoming_grads(lv_grad_dsc_t * hot_g, lv_grad_dsc_t * gold_g, lv_grad_dsc_t * mint_g);

}  // namespace theme
}  // namespace wp
