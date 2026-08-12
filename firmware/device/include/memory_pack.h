#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace memory_pack {

/** Sorted stem count — same order both desks use for seed picks. */
int count();
const char * stem_at(int i);
const lv_image_dsc_t * dsc_at(int i);
const lv_image_dsc_t * find_dsc(const char * stem);

}  // namespace memory_pack
}  // namespace wp
