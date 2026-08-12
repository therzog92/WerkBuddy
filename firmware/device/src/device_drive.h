#pragma once
#include <lvgl.h>
#include <stdint.h>

#ifndef WERKPAGER_DEVICE_DRIVE
#define WERKPAGER_DEVICE_DRIVE 0
#endif

namespace wp {
namespace drive {

#if WERKPAGER_DEVICE_DRIVE
void init(lv_display_t * disp);
void poll();
/** If true, `data` is filled from the drive queue (skip real touch). */
bool take_pointer(lv_indev_data_t * data);
/** Copy flushed strip into the shot buffer when a capture is armed. */
void on_flush(const lv_area_t * area, const uint8_t * px_map);
#else
inline void init(lv_display_t * /*disp*/) {}
inline void poll() {}
inline bool take_pointer(lv_indev_data_t * /*data*/) { return false; }
inline void on_flush(const lv_area_t * /*area*/, const uint8_t * /*px*/) {}
#endif

}  // namespace drive
}  // namespace wp
