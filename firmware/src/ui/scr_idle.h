#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

/** Idle screen — black or clock per settings; tap to wake. */
lv_obj_t * idle_screen();

}  // namespace ui
}  // namespace wp
