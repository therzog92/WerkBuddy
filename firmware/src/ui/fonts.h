#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

/** Bebas Neue — web display face for pager names (lazy-loaded). */
const lv_font_t * font_display(int32_t px);

/** DM Sans italic — web body italic (hints). */
const lv_font_t * font_body_italic(int32_t px);

}  // namespace ui
}  // namespace wp
