#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace sim {

/** Start TCP drive server + pointer indev when WERKPAGER_DRIVE=1. */
void driver_init(lv_display_t * disp);

/** True if drive mode is active. */
bool driver_enabled();

}  // namespace sim
}  // namespace wp
