#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

/** Build a fresh hub (home) screen from app state. */
lv_obj_t * hub_screen();

/** Rebuild Active Games / Your Turn strip in place (no full hub tear-down). */
void hub_refresh_games_chrome();

}  // namespace ui
}  // namespace wp
