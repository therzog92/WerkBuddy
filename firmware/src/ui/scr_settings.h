#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

lv_obj_t * settings_screen();
lv_obj_t * keyboard_screen_name();
lv_obj_t * keyboard_screen_canned(int index);
lv_obj_t * keyboard_screen_compose();
lv_obj_t * emoji_picker_screen(int slot);

}  // namespace ui
}  // namespace wp
