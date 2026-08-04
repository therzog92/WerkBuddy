#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

enum class AppIcon {
  Werk,
  Games,
  Doodle,
  Settings,
  Ttt,
  C4,
  Battleship,
  Checkers,
  Memory,
};

/** 72×72 rounded glyph with drawn icon (matches web app-glyph look). */
lv_obj_t * make_app_glyph(lv_obj_t * parent, AppIcon icon);

/** Full app icon column: glyph + label. Clickable wrapper. */
lv_obj_t * make_app_icon(lv_obj_t * parent, AppIcon icon, const char * label,
                         lv_event_cb_t cb, void * user_data = nullptr);

}  // namespace ui
}  // namespace wp
