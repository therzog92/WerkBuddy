#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

lv_obj_t * game_wordle_screen();
/** Clear Classic/Race picker so the next open starts at mode select. */
void wordle_reset_setup();

}  // namespace ui
}  // namespace wp

