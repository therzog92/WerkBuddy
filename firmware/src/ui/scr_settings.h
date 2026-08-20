#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

lv_obj_t * settings_screen();
/** Rebuild Settings while keeping the current scroll position (scan / add peer). */
void refresh_settings_keep_scroll();
lv_obj_t * keyboard_screen_name();
lv_obj_t * keyboard_screen_setup_name();
lv_obj_t * keyboard_screen_factory_reset();
lv_obj_t * keyboard_screen_canned(int index);
lv_obj_t * keyboard_screen_compose();
lv_obj_t * wifi_scan_screen();
lv_obj_t * keyboard_screen_wifi_pass();
lv_obj_t * keyboard_screen_checklist();
lv_obj_t * ota_releases_screen();
/** Settings: slot 0..kEmojiSlots-1. Compose pick-from-all: pass kEmojiPickerCompose. */
constexpr int kEmojiPickerCompose = -1;
lv_obj_t * emoji_picker_screen(int slot);
lv_obj_t * bg_upload_screen();
lv_obj_t * fw_upload_screen();

}  // namespace ui
}  // namespace wp
