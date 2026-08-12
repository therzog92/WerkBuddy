#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {
namespace emoji_pack {

const lv_image_dsc_t * find_dsc(const char * emoji_utf8);
int count();
const char * at(int i);

}  // namespace emoji_pack
}  // namespace ui
}  // namespace wp
