#include "ui/brightness.h"

#include "app/app.h"

#include "lvgl/lvgl.h"

#if defined(WP_DEVICE)
/* Provided by device main — drives TFT_BL. */
extern "C" void wp_device_set_backlight(bool on);
#endif

namespace wp {
namespace ui {
namespace brightness {
namespace {

lv_obj_t * g_dim = nullptr;
bool g_page_boost = false;

uint8_t clamp_pct(int v) {
  if (v < 10) return 10;
  if (v > 100) return 100;
  return (uint8_t)v;
}

}  // namespace

void init() {
  if (g_dim) return;
  g_dim = lv_obj_create(lv_layer_sys());
  lv_obj_remove_style_all(g_dim);
  lv_obj_set_size(g_dim, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(g_dim, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(g_dim, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(g_dim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(g_dim, LV_OBJ_FLAG_SCROLLABLE);
  apply();
}

void apply() {
  if (!g_dim) return;
  if (g_page_boost) {
    lv_obj_set_style_bg_opa(g_dim, LV_OPA_TRANSP, 0);
    return;
  }
  const int pct = (int)clamp_pct(app::desk().brightness);
  /* 100% → transparent; 10% → ~90% black veil (simulates backlight) */
  const lv_opa_t opa = (lv_opa_t)((100 - pct) * 255 / 100);
  lv_obj_set_style_bg_opa(g_dim, opa, 0);
}

void set_page_boost(bool on) {
  g_page_boost = on;
  apply();
}

void set_percent(uint8_t percent) {
  app::desk().brightness = clamp_pct(percent);
  app::save();
  apply();
}

uint8_t percent() { return clamp_pct(app::desk().brightness); }

void set_panel_on(bool on) {
#if defined(WP_DEVICE)
  wp_device_set_backlight(on);
#else
  (void)on;
#endif
}

}  // namespace brightness
}  // namespace ui
}  // namespace wp
