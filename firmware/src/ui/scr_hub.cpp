#include "ui/scr_hub.h"

#include "app/app.h"
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
}

void on_deleted(lv_event_t * /*e*/) {
  g_clock = nullptr;
  g_date = nullptr;
  g_desk = nullptr;
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
}

}  // namespace

lv_obj_t * hub_screen() {
  lv_obj_t * scr = make_screen();
  lv_obj_add_event_cb(scr, on_deleted, LV_EVENT_DELETE, nullptr);

  /* Slim brand bar — product name only; desk identity lives with the clock. */
  lv_obj_t * top = lv_obj_create(scr);
  lv_obj_remove_style_all(top);
  lv_obj_set_size(top, WP_HOR_RES, 44);
  lv_obj_set_style_pad_hor(top, 20, 0);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t * brand = lv_label_create(top);
  lv_label_set_text(brand, "WERKPAGER");
  lv_obj_set_style_text_color(brand, theme::gold(), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_letter_space(brand, 3, 0);

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
  lv_obj_set_style_pad_bottom(body, 10, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 6, 0);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  char cb[24], db[40];
  format_clock(cb, sizeof(cb), db, sizeof(db));

  /* Clock block — slightly tighter to leave room for the game row. */
  lv_obj_t * hero = lv_obj_create(body);
  lv_obj_remove_style_all(hero);
  lv_obj_set_width(hero, lv_pct(100));
  lv_obj_set_height(hero, 96);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(hero, 1, 0);
  lv_obj_remove_flag(hero, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(hero, LV_OBJ_FLAG_SCROLLABLE);

  g_desk = lv_label_create(hero);
  char desk[48];
  lv_snprintf(desk, sizeof(desk), "%s's Desk", app::desk().name);
  lv_label_set_text(g_desk, desk);
  lv_obj_set_style_text_color(g_desk, theme::muted(), 0);
  lv_obj_set_style_text_font(g_desk, &lv_font_montserrat_14, 0);

  g_clock = lv_label_create(hero);
  lv_label_set_text(g_clock, cb);
  lv_obj_set_style_text_color(g_clock, theme::ink(), 0);
  lv_obj_set_style_text_font(g_clock, font_display(56), 0);
  lv_obj_set_style_text_letter_space(g_clock, 1, 0);

  g_date = lv_label_create(hero);
  lv_label_set_text(g_date, db);
  lv_obj_set_style_text_color(g_date, theme::gold(), 0);
  lv_obj_set_style_text_font(g_date, &lv_font_montserrat_14, 0);

  g_timer = lv_timer_create(tick, 1000, nullptr);

  /* Center stage: WerkPager sits mid-low above the dock. */
  lv_obj_t * stage = lv_obj_create(body);
  lv_obj_remove_style_all(stage);
  lv_obj_set_width(stage, lv_pct(100));
  lv_obj_set_flex_grow(stage, 1);
  lv_obj_set_flex_flow(stage, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(stage, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_bottom(stage, 12, 0);
  lv_obj_remove_flag(stage, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * werk = make_app_icon_sized(stage, AppIcon::Werk, "WerkPager",
                                        [](lv_event_t * /*e*/) { go_werk(); }, 96, 150, 128,
                                        &lv_font_montserrat_14);
  /* Slow, visible pulse on the WerkPager glyph (opacity + slight scale). */
  if (lv_obj_get_child_count(werk) > 0) {
    lv_obj_t * wrap = lv_obj_get_child(werk, 0);
    if (wrap && lv_obj_get_child_count(wrap) > 0) {
      lv_obj_t * glyph = lv_obj_get_child(wrap, 0);
      lv_obj_set_style_transform_pivot_x(glyph, 36, 0);
      lv_obj_set_style_transform_pivot_y(glyph, 36, 0);
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
        /* v 0..1000 → opa 130..255, scale ~92%..100% of the 96px hub size (256*96/72). */
        const lv_opa_t opa = (lv_opa_t)(130 + (v * 125) / 1000);
        const int32_t scale = 314 + (v * 28) / 1000;
        lv_obj_set_style_bg_opa(g, opa, 0);
        lv_obj_set_style_transform_scale(g, scale, 0);
      });
      lv_anim_start(&a);
    }
  }

  /* Bottom dock: equal columns so Games shares WerkPager's center axis;
   * Doodle / Settings match in size, pinned to the outer corners. */
  lv_obj_t * corners = lv_obj_create(body);
  lv_obj_remove_style_all(corners);
  lv_obj_set_width(corners, lv_pct(100));
  lv_obj_set_height(corners, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(corners, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(corners, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(corners, LV_OBJ_FLAG_SCROLLABLE);

  auto make_slot = [](lv_obj_t * parent, lv_flex_align_t h_align) {
    lv_obj_t * slot = lv_obj_create(parent);
    lv_obj_remove_style_all(slot);
    lv_obj_set_flex_grow(slot, 1);
    lv_obj_set_height(slot, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(slot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(slot, LV_FLEX_ALIGN_CENTER, h_align, h_align);
    lv_obj_remove_flag(slot, LV_OBJ_FLAG_SCROLLABLE);
    return slot;
  };

  constexpr int kCornerGlyph = 52;
  constexpr int kCornerColW = 84;
  constexpr int kCornerColH = 84;
  constexpr int kGamesGlyph = 64;
  constexpr int kGamesColW = 96;
  constexpr int kGamesColH = 96;

  lv_obj_t * left = make_slot(corners, LV_FLEX_ALIGN_START);
  make_app_icon_sized(left, AppIcon::Doodle, "Doodle", [](lv_event_t * /*e*/) { go_doodle(); },
                      kCornerGlyph, kCornerColW, kCornerColH, &lv_font_montserrat_12);

  lv_obj_t * mid = make_slot(corners, LV_FLEX_ALIGN_CENTER);
  make_app_icon_sized(mid, AppIcon::Games, "Games", [](lv_event_t * /*e*/) { go_games_folder(); },
                      kGamesGlyph, kGamesColW, kGamesColH, &lv_font_montserrat_12);

  lv_obj_t * right = make_slot(corners, LV_FLEX_ALIGN_END);
  make_app_icon_sized(right, AppIcon::Settings, "Settings",
                      [](lv_event_t * /*e*/) { go_settings(); }, kCornerGlyph, kCornerColW,
                      kCornerColH, &lv_font_montserrat_12);

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
