#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

enum class AppIcon {
  Werk,
  Games,
  Doodle,
  Timer,
  Settings,
  Utilities,
  Checklist,
  Calculator,
  Ttt,
  Sttt,
  C4,
  Battleship,
  Checkers,
  Memory,
  Reversi,
  Dots,
  Scoreboard,
  G2048,
  Wordle,
};

/** 72×72 rounded glyph with drawn icon (matches web app-glyph look). */
lv_obj_t * make_app_glyph(lv_obj_t * parent, AppIcon icon);

/** Full app icon column: glyph + label. Clickable wrapper. */
lv_obj_t * make_app_icon(lv_obj_t * parent, AppIcon icon, const char * label,
                         lv_event_cb_t cb, void * user_data = nullptr);

/**
 * Sized hub icon. `glyph_vis` is the on-screen diameter (drawing stays 72px, scaled).
 * `col_w` / `col_h` size the clickable column; `label_font` may be null for default 14.
 */
lv_obj_t * make_app_icon_sized(lv_obj_t * parent, AppIcon icon, const char * label,
                               lv_event_cb_t cb, int glyph_vis, int col_w, int col_h,
                               const lv_font_t * label_font = nullptr,
                               void * user_data = nullptr);

}  // namespace ui
}  // namespace wp
