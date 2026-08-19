#include "ui/scr_idle.h"

#include "app/app.h"
#include "app/background.h"
#include "app/desk_timer.h"
#include "ui/brightness.h"
#include "ui/chrome.h"
#include "ui/fonts.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <ctime>

namespace wp {
namespace ui {
namespace {

lv_obj_t * g_time = nullptr;
lv_obj_t * g_date = nullptr;
lv_obj_t * g_timer_lbl = nullptr;
lv_timer_t * g_timer = nullptr;

void format_now(char * time_buf, size_t tn, char * date_buf, size_t dn) {
  std::tm tm{};
  app::local_time(&tm);
  if (app::desk().clock_24h) {
    lv_snprintf(time_buf, (uint32_t)tn, "%02d:%02d", tm.tm_hour, tm.tm_min);
  } else {
    int hour = tm.tm_hour % 12;
    if (hour == 0) hour = 12;
    const char * ampm = tm.tm_hour >= 12 ? "PM" : "AM";
    lv_snprintf(time_buf, (uint32_t)tn, "%d:%02d %s", hour, tm.tm_min, ampm);
  }
  static const char * kWd[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday"};
  static const char * kMo[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  lv_snprintf(date_buf, (uint32_t)dn, "%s, %s %d", kWd[tm.tm_wday], kMo[tm.tm_mon], tm.tm_mday);
}

void tick(lv_timer_t * /*t*/) {
  char tb[16], db[32];
  format_now(tb, sizeof(tb), db, sizeof(db));
  if (g_time) lv_label_set_text(g_time, tb);
  if (g_date) lv_label_set_text(g_date, db);
  if (g_timer_lbl) {
    if (desk_timer::is_finished()) {
      lv_label_set_text(g_timer_lbl, "Time's up");
      lv_obj_remove_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
    } else if (desk_timer::is_active()) {
      char parts[2][16];
      int n = 0;
      for (int i = 0; i < desk_timer::kSlots; ++i) {
        if (!desk_timer::is_active(i)) continue;
        desk_timer::format_remaining(i, parts[n], sizeof(parts[n]));
        ++n;
      }
      char line[48];
      if (n >= 2)
        lv_snprintf(line, sizeof(line), "T1 %s / T2 %s", parts[0], parts[1]);
      else if (n == 1)
        lv_snprintf(line, sizeof(line), "Timer %s", parts[0]);
      else
        line[0] = '\0';
      if (line[0]) {
        lv_label_set_text(g_timer_lbl, line);
        lv_obj_remove_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void on_deleted(lv_event_t * /*e*/) {
  g_time = nullptr;
  g_date = nullptr;
  g_timer_lbl = nullptr;
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
}

}  // namespace

lv_obj_t * idle_screen() {
  lv_obj_t * scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_size(scr, WP_HOR_RES, WP_VER_RES);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  /* PRESSED wakes even if CLICKED is eaten by a toast / short tap glitch. */
  auto wake = [](lv_event_t * /*e*/) { wake_from_idle(); };
  lv_obj_add_event_cb(scr, wake, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(scr, wake, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(scr, on_deleted, LV_EVENT_DELETE, nullptr);

  /* Pre-setup always blacks out (and kills backlight) — no clock drain in a bag. */
  const bool clock_mode = app::desk().setup_done && app::desk().idle_mode == 1;
  theme::apply_idle_bg(scr, !clock_mode);
  brightness::set_panel_on(clock_mode);

  if (clock_mode) {
    /* Accent pool only when there is no photo — wallpaper idle is a picture frame. */
    if (!background::has()) {
      lv_obj_t * glow = lv_obj_create(scr);
      lv_obj_remove_style_all(glow);
      lv_obj_set_size(glow, 280, 280);
      lv_obj_center(glow);
      lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(glow, theme::hot(), 0);
      lv_obj_set_style_bg_opa(glow, 22, 0);
      lv_obj_remove_flag(glow, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t * col = lv_obj_create(scr);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 6, 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);

    g_time = nullptr;
    g_date = nullptr;
    if ((app::desk().chrome_hide & app::Desk::kHideIdleName) == 0) {
      lv_obj_t * desk = lv_label_create(col);
      char desk_buf[48];
      lv_snprintf(desk_buf, sizeof(desk_buf), "%s's Desk", app::desk().name);
      lv_label_set_text(desk, desk_buf);
      lv_obj_set_style_text_color(desk, theme::gold(), 0);
      lv_obj_set_style_text_font(desk, &lv_font_montserrat_28, 0);
      lv_obj_set_style_text_letter_space(desk, 1, 0);
    }

    char tb[16], db[32];
    format_now(tb, sizeof(tb), db, sizeof(db));
    if ((app::desk().chrome_hide & app::Desk::kHideIdleClock) == 0) {
      g_time = lv_label_create(col);
      lv_label_set_text(g_time, tb);
      lv_obj_set_style_text_color(g_time, theme::ink(), 0);
      lv_obj_set_style_text_font(g_time, font_display(96), 0);
      lv_obj_set_style_text_letter_space(g_time, 2, 0);

      g_date = lv_label_create(col);
      lv_label_set_text(g_date, db);
      lv_obj_set_style_text_color(g_date, theme::muted(), 0);
      lv_obj_set_style_text_font(g_date, &lv_font_montserrat_28, 0);
    }

    g_timer_lbl = lv_label_create(col);
    lv_label_set_text(g_timer_lbl, "");
    lv_obj_set_style_text_color(g_timer_lbl, theme::mint(), 0);
    lv_obj_set_style_text_font(g_timer_lbl, &lv_font_montserrat_20, 0);
    lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * hint = lv_label_create(col);
    lv_label_set_text(hint, "tap to wake");
    lv_obj_set_style_text_color(hint, theme::muted(), 0);
    lv_obj_set_style_text_opa(hint, LV_OPA_50, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_top(hint, 20, 0);

    g_timer = lv_timer_create(tick, 1000, nullptr);
    tick(nullptr);
  }

  return scr;
}

}  // namespace ui
}  // namespace wp
