#include "ui/scr_hub.h"

#include "app/active_games.h"
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
  if (app::desk().clock_24h) {
    lv_snprintf(buf, (uint32_t)n, "%02d:%02d", tm.tm_hour, tm.tm_min);
  } else {
    int hour12 = tm.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    const char * ampm = tm.tm_hour >= 12 ? "PM" : "AM";
    lv_snprintf(buf, (uint32_t)n, "%d:%02d %s", hour12, tm.tm_min, ampm);
  }
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
  lv_obj_remove_flag(hero, LV_OBJ_FLAG_SCROLLABLE);

  g_desk = lv_label_create(hero);
  char desk[48];
  lv_snprintf(desk, sizeof(desk), "%s's Desk", app::desk().name);
  lv_label_set_text(g_desk, desk);
  lv_obj_set_style_text_color(g_desk, theme::muted(), 0);
  lv_obj_set_style_text_font(g_desk, &lv_font_montserrat_28, 0);

  g_clock = lv_label_create(hero);
  lv_label_set_text(g_clock, cb);
  lv_obj_set_style_text_color(g_clock, theme::gold(), 0);
  lv_obj_set_style_text_font(g_clock, font_display(60), 0);
  lv_obj_set_style_text_letter_space(g_clock, 2, 0);
  lv_obj_add_flag(g_clock, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      g_clock,
      [](lv_event_t * /*e*/) {
        app::desk().clock_24h = app::desk().clock_24h ? 0 : 1;
        app::save();
        char cb2[24], db2[40];
        format_clock(cb2, sizeof(cb2), db2, sizeof(db2));
        if (g_clock) lv_label_set_text(g_clock, cb2);
        if (g_date) lv_label_set_text(g_date, db2);
        toast(app::desk().clock_24h ? "24-hour clock" : "12-hour clock");
      },
      LV_EVENT_CLICKED, nullptr);

  g_date = lv_label_create(hero);
  lv_label_set_text(g_date, db);
  lv_obj_set_style_text_color(g_date, theme::gold(), 0);
  lv_obj_set_style_text_font(g_date, &lv_font_montserrat_20, 0);

  g_timer_lbl = lv_label_create(hero);
  lv_label_set_text(g_timer_lbl, "");
  lv_obj_set_style_text_color(g_timer_lbl, theme::mint(), 0);
  lv_obj_set_style_text_font(g_timer_lbl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_pad_top(g_timer_lbl, 2, 0);
  lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_timer_lbl, [](lv_event_t * /*e*/) { go_timer(); }, LV_EVENT_CLICKED,
                      nullptr);
  /* Desk name also opens timer (easy target). */
  lv_obj_add_flag(g_desk, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_desk, [](lv_event_t * /*e*/) { go_timer(); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(g_date, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_date, [](lv_event_t * /*e*/) { go_timer(); }, LV_EVENT_CLICKED, nullptr);

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

  {
    const int n_active = app::active_count();
    const int n_turn = app::your_turn_count();
    if (n_active > 0) {
      lv_obj_t * strip = lv_button_create(body);
      lv_obj_remove_style_all(strip);
      lv_obj_set_width(strip, lv_pct(100));
      lv_obj_set_height(strip, 48);
      lv_obj_set_style_pad_ver(strip, 10, 0);
      lv_obj_set_style_pad_hor(strip, 12, 0);
      lv_obj_set_style_radius(strip, 12, 0);
      lv_obj_set_style_bg_color(strip, theme::panel(), 0);
      lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
      lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(strip, 10, 0);
      lv_obj_add_flag(strip, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(strip, [](lv_event_t * /*e*/) { go_active_games(); }, LV_EVENT_CLICKED,
                          nullptr);

      lv_obj_t * count_lbl = lv_label_create(strip);
      char abuf[40];
      lv_snprintf(abuf, sizeof(abuf), "Active Games: %d", n_active);
      lv_label_set_text(count_lbl, abuf);
      lv_obj_set_style_text_color(count_lbl, theme::ink(), 0);
      lv_obj_set_style_text_font(count_lbl, &lv_font_montserrat_16, 0);

      if (n_turn > 0) {
        /* Fixed bright green pill — readable on every wallpaper/theme. */
        constexpr uint32_t kTurnGreen = 0x3ddc84;
        lv_obj_t * pill = lv_obj_create(strip);
        lv_obj_remove_style_all(pill);
        lv_obj_set_height(pill, 32);
        lv_obj_set_style_pad_hor(pill, 14, 0);
        lv_obj_set_style_pad_ver(pill, 6, 0);
        lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(pill, lv_color_hex(kTurnGreen), 0);
        lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
        lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(pill, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t * turn_lbl = lv_label_create(pill);
        lv_label_set_text(turn_lbl, "Your Turn");
        lv_obj_set_style_text_color(turn_lbl, lv_color_hex(0x062012), 0);
        lv_obj_set_style_text_font(turn_lbl, &lv_font_montserrat_14, 0);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, pill);
        lv_anim_set_values(&a, 170, 255);
        lv_anim_set_duration(&a, 700);
        lv_anim_set_playback_duration(&a, 700);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&a, [](void * obj, int32_t v) {
          lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(obj), (lv_opa_t)v, 0);
        });
        lv_anim_start(&a);
      }
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
  lv_obj_t * doodle = make_app_icon_sized(corners, AppIcon::Doodle, "Doodle",
                                          [](lv_event_t * /*e*/) { go_doodle(); }, kGlyph, kColW,
                                          kColH, &lv_font_montserrat_12);
  /* Pulse like WerkPager when remote strokes arrived while we were elsewhere. */
  if (app::desk().doodle_unread && lv_obj_get_child_count(doodle) > 0) {
    lv_obj_t * wrap = lv_obj_get_child(doodle, 0);
    if (wrap && lv_obj_get_child_count(wrap) > 0) {
      lv_obj_t * glyph = lv_obj_get_child(wrap, 0);
      lv_obj_set_style_transform_pivot_x(glyph, 36, 0);
      lv_obj_set_style_transform_pivot_y(glyph, 36, 0);
      constexpr int32_t kBase = (48 * 256) / 72;
      constexpr int32_t kLo = (kBase * 90) / 100;
      constexpr int32_t kHi = (kBase * 110) / 100;
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
        constexpr int32_t kBase = (48 * 256) / 72;
        constexpr int32_t kLo = (kBase * 90) / 100;
        constexpr int32_t kHi = (kBase * 110) / 100;
        const lv_opa_t opa = (lv_opa_t)(150 + (v * 105) / 1000);
        lv_obj_set_style_bg_opa(g, opa, 0);
        lv_obj_set_style_transform_scale(g, kLo + ((kHi - kLo) * v) / 1000, 0);
      });
      lv_anim_start(&a);
    }
  }
  make_app_icon_sized(corners, AppIcon::Games, "Games", [](lv_event_t * /*e*/) { go_games_folder(); },
                      kGlyph, kColW, kColH, &lv_font_montserrat_12);
  make_app_icon_sized(corners, AppIcon::Utilities, "Utilities",
                      [](lv_event_t * /*e*/) { go_utils_folder(); }, kGlyph, kColW, kColH,
                      &lv_font_montserrat_12);
  make_app_icon_sized(corners, AppIcon::Settings, "Settings",
                      [](lv_event_t * /*e*/) { go_settings(); }, kGlyph, kColW, kColH,
                      &lv_font_montserrat_12);

  return scr;
}

}  // namespace ui
}  // namespace wp
