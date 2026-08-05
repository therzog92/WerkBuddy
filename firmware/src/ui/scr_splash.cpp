#include "ui/scr_splash.h"

#include "app/app.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {
namespace {

lv_timer_t * g_splash_timer = nullptr;

void clear_splash_timer() {
  if (!g_splash_timer) return;
  lv_timer_delete(g_splash_timer);
  g_splash_timer = nullptr;
}

void finish_splash(lv_timer_t * /*t*/) {
  g_splash_timer = nullptr;
  if (current_screen() != Screen::Splash) return;
  if (!app::desk().setup_done) go_setup();
  else go_hub();
}

void on_tap(lv_event_t * /*e*/) {
  clear_splash_timer();
  if (current_screen() != Screen::Splash) return;
  if (!app::desk().setup_done) go_setup();
  else go_hub();
}

}  // namespace

lv_obj_t * splash_screen() {
  clear_splash_timer();

  lv_obj_t * scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_size(scr, WP_HOR_RES, WP_VER_RES);
  theme::apply_screen_bg(scr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, on_tap, LV_EVENT_CLICKED, nullptr);

  lv_obj_t * brand = lv_label_create(scr);
  lv_label_set_text(brand, "WERKBUDDY");
  lv_obj_set_style_text_color(brand, theme::gold(), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_letter_space(brand, 2, 0);
  lv_obj_center(brand);

  lv_obj_t * sub = lv_label_create(scr);
  lv_label_set_text(sub, app::desk().name);
  lv_obj_set_style_text_color(sub, theme::muted(), 0);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 36);

  /* ~1.4s then hub (tap skips) */
  g_splash_timer = lv_timer_create(finish_splash, 1400, nullptr);
  lv_timer_set_repeat_count(g_splash_timer, 1);
  return scr;
}

}  // namespace ui
}  // namespace wp
