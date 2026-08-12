#include "ui/orient.h"

#include "app/app.h"

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {
namespace orient {

bool flip_180() { return app::desk().rotate_180 != 0; }

void apply() {
#if !defined(WP_DEVICE)
  lv_display_t * disp = lv_display_get_default();
  if (disp) {
    lv_display_set_rotation(disp, flip_180() ? LV_DISPLAY_ROTATION_180 : LV_DISPLAY_ROTATION_0);
  }
#endif
  lv_obj_t * scr = lv_screen_active();
  if (scr) lv_obj_invalidate(scr);
}

void set_flip_180(bool on) {
  app::desk().rotate_180 = on ? 1 : 0;
  app::save();
  apply();
}

}  // namespace orient
}  // namespace ui
}  // namespace wp
