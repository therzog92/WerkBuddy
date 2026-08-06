#include "ui/scr_hub.h"

#include "app/app.h"
#include "app/desk_timer.h"
#include "ui/chrome.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <ctime>

namespace wp {
namespace ui {
namespace {

lv_obj_t * g_clock = nullptr;
lv_obj_t * g_date = nullptr;
lv_obj_t * g_desk = nullptr;
lv_obj_t * g_timer_lbl = nullptr;
lv_timer_t * g_timer = nullptr;

void format_clock(char * buf, size_t n, char * date_buf, size_t dn) {
  std::tm tm{};
  app::local_time(&tm);
  int hour12 = tm.tm_hour % 12;
  if (hour12 == 0) hour12 = 12;
  const char * ampm = tm.tm_hour >= 12 ? "PM" : "AM";
  lv_snprintf(buf, (uint32_t)n, "%d:%02d %s", hour12, tm.tm_min, ampm);
  static const char * kWd[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday"};
  static const char * kMo[] = {"January", "February", "March", "April", "May", "June",
                               "July", "August", "September", "October", "November", "December"};
  lv_snprintf(date_buf, (uint32_t)dn, "%s, %s %d", kWd[tm.tm_wday], kMo[tm.tm_mon], tm.tm_mday);
}

void tick(lv_timer_t * /*t*/) {
  if (!g_clock) return;
  char cb[24], db[40];
  format_clock(cb, sizeof(cb), db, sizeof(db));
  lv_label_set_text(g_clock, cb);
  if (g_date) lv_label_set_text(g_date, db);
  if (g_timer_lbl) {
    if (desk_timer::is_finished()) {
      lv_label_set_text(g_timer_lbl, "Time's up - tap");
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
  g_clock = nullptr;
  g_date = nullptr;
  g_desk = nullptr;
  g_timer_lbl = nullptr;
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
}

}  // namespace

lv_obj_t * hub_screen() {
  lv_obj_t * scr = make_screen(theme::BgWash::Hub);
  lv_obj_add_event_cb(scr, on_deleted, LV_EVENT_DELETE, nullptr);

  lv_obj_t * top = lv_obj_create(scr);
  lv_obj_remove_style_all(top);
  lv_obj_set_size(top, WP_HOR_RES, 44);
  lv_obj_set_style_pad_hor(top, 20, 0);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t * brand = lv_label_create(top);
  lv_label_set_text(brand, "WERKBUDDY");
  lv_obj_set_style_text_color(brand, theme::gold(), 0);
  lv_obj_set_style_text_font(brand, font_display(18), 0);
  lv_obj_set_style_text_letter_space(brand, 2, 0);

  lv_obj_t * live = lv_obj_create(top);
  lv_obj_remove_style_all(live);
  lv_obj_set_size(live, LV_SIZE_CONTENT, 22);
  lv_obj_set_flex_flow(live, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(live, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(live, 6, 0);
  lv_obj_set_style_pad_hor(live, 10, 0);
  lv_obj_set_style_bg_color(live, theme::panel(), 0);
  lv_obj_set_style_bg_opa(live, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(live, 11, 0);
  lv_obj_t * dot = lv_obj_create(live);
  lv_obj_remove_style_all(dot);
  lv_obj_set_size(dot, 6, 6);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, theme::mint(), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_t * live_lbl = lv_label_create(live);
  lv_label_set_text(live_lbl, "live");
  lv_obj_set_style_text_color(live_lbl, theme::muted(), 0);
  lv_obj_set_style_text_font(live_lbl, &lv_font_montserrat_12, 0);

  lv_obj_t * body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, WP_HOR_RES, WP_VER_RES - 44);
  lv_obj_set_pos(body, 0, 44);
  lv_obj_set_style_pad_hor(body, 12, 0);
  lv_obj_set_style_pad_top(body, 6, 0);
  lv_obj_set_style_pad_bottom(body, 10, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 4, 0);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  char cb[24], db[40];
  format_clock(cb, sizeof(cb), db, sizeof(db));

  /* Clock block — size to content so desk name + timer line never clip. */
  lv_obj_t * hero = lv_obj_create(body);
  lv_obj_remove_style_all(hero);
  lv_obj_set_width(hero, lv_pct(100));
  lv_obj_set_height(hero, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(hero, 3, 0);
  lv_obj_set_style_pad_ver(hero, 4, 0);
  lv_obj_add_flag(hero, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(hero, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(hero, [](lv_event_t * /*e*/) { go_timer(); }, LV_EVENT_CLICKED, nullptr);

  g_desk = lv_label_create(hero);
  char desk[48];
  lv_snprintf(desk, sizeof(desk), "%s's Desk", app::desk().name);
  lv_label_set_text(g_desk, desk);
  lv_obj_set_style_text_color(g_desk, theme::muted(), 0);
  lv_obj_set_style_text_font(g_desk, &lv_font_montserrat_14, 0);

  g_clock = lv_label_create(hero);
  lv_label_set_text(g_clock, cb);
  lv_obj_set_style_text_color(g_clock, theme::gold(), 0);
  lv_obj_set_style_text_font(g_clock, font_display(52), 0);
  lv_obj_set_style_text_letter_space(g_clock, 2, 0);

  g_date = lv_label_create(hero);
  lv_label_set_text(g_date, db);
  lv_obj_set_style_text_color(g_date, theme::gold(), 0);
  lv_obj_set_style_text_font(g_date, &lv_font_montserrat_14, 0);

  g_timer_lbl = lv_label_create(hero);
  lv_label_set_text(g_timer_lbl, "");
  lv_obj_set_style_text_color(g_timer_lbl, theme::mint(), 0);
  lv_obj_set_style_text_font(g_timer_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_pad_top(g_timer_lbl, 2, 0);
  lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);

  g_timer = lv_timer_create(tick, 1000, nullptr);
  tick(nullptr);

  lv_obj_t * stage = lv_obj_create(body);
  lv_obj_remove_style_all(stage);
  lv_obj_set_width(stage, lv_pct(100));
  lv_obj_set_flex_grow(stage, 1);
  lv_obj_set_flex_flow(stage, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(stage, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_bottom(stage, 8, 0);
  lv_obj_remove_flag(stage, LV_OBJ_FLAG_SCROLLABLE);

  /*
   * Same cheap pulse as the old grid icon (style scale + opa only).
   * Grow into the 88px wrap rather than past it — avoids crop and keeps the
   * dirty region small (scaling past the wrap was what made overflow fixes laggy).
   */
  lv_obj_t * werk = make_app_icon_sized(stage, AppIcon::Werk, "WerkPager",
                                        [](lv_event_t * /*e*/) { go_werk(); }, 88, 140, 118,
                                        &lv_font_montserrat_14);
  if (lv_obj_get_child_count(werk) > 0) {
    lv_obj_t * wrap = lv_obj_get_child(werk, 0);
    if (wrap && lv_obj_get_child_count(wrap) > 0) {
      lv_obj_t * glyph = lv_obj_get_child(wrap, 0);
      lv_obj_set_style_transform_pivot_x(glyph, 36, 0);
      lv_obj_set_style_transform_pivot_y(glyph, 36, 0);
      constexpr int32_t kLo = (80 * 256) / 72;
      constexpr int32_t kHi = (88 * 256) / 72;
      lv_obj_set_style_transform_scale(glyph, kLo, 0);
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, glyph);
      lv_anim_set_values(&a, 0, 1000);
      lv_anim_set_duration(&a, 1400);
      lv_anim_set_playback_duration(&a, 1400);
      lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
      lv_anim_set_exec_cb(&a, [](void * obj, int32_t v) {
        auto * g = static_cast<lv_obj_t *>(obj);
        constexpr int32_t kLo = (80 * 256) / 72;
        constexpr int32_t kHi = (88 * 256) / 72;
        const lv_opa_t opa = (lv_opa_t)(150 + (v * 105) / 1000);
        lv_obj_set_style_bg_opa(g, opa, 0);
        lv_obj_set_style_transform_scale(g, kLo + ((kHi - kLo) * v) / 1000, 0);
      });
      lv_anim_start(&a);
    }
  }

  lv_obj_t * corners = lv_obj_create(body);
  lv_obj_remove_style_all(corners);
  lv_obj_set_width(corners, lv_pct(100));
  lv_obj_set_height(corners, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(corners, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(corners, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(corners, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int kGlyph = 48;
  constexpr int kColW = 72;
  constexpr int kColH = 76;
  make_app_icon_sized(corners, AppIcon::Doodle, "Doodle", [](lv_event_t * /*e*/) { go_doodle(); },
                      kGlyph, kColW, kColH, &lv_font_montserrat_12);
  make_app_icon_sized(corners, AppIcon::Games, "Games", [](lv_event_t * /*e*/) { go_games_folder(); },
                      kGlyph, kColW, kColH, &lv_font_montserrat_12);
  make_app_icon_sized(corners, AppIcon::Utilities, "Utilities",
                      [](lv_event_t * /*e*/) { go_utils_folder(); }, kGlyph, kColW, kColH,
                      &lv_font_montserrat_12);
  make_app_icon_sized(corners, AppIcon::Settings, "Settings",
                      [](lv_event_t * /*e*/) { go_settings(); }, kGlyph, kColW, kColH,
                      &lv_font_montserrat_12);

  lv_obj_t * status = lv_label_create(body);
  char sb[56];
  const int n = app::desk().peer_count;
  lv_snprintf(sb, sizeof(sb), "%d desk%s nearby", n, n == 1 ? "" : "s");
  lv_label_set_text(status, sb);
  lv_obj_set_style_text_color(status, theme::muted(), 0);
  lv_obj_set_style_text_font(status, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_opa(status, LV_OPA_70, 0);

  return scr;
}

}  // namespace ui
}  // namespace wp
