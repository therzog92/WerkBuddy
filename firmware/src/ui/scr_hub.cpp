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
  lv_obj_set_style_pad_hor(body, 18, 0);
  lv_obj_set_style_pad_bottom(body, 16, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 10, 0);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  char cb[24], db[40];
  format_clock(cb, sizeof(cb), db, sizeof(db));

  /* Clock block — open type on the theme bg (no card/pill chrome) */
  lv_obj_t * hero = lv_obj_create(body);
  lv_obj_remove_style_all(hero);
  lv_obj_set_width(hero, lv_pct(100));
  lv_obj_set_height(hero, 118);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(hero, 2, 0);
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
  lv_obj_set_style_text_font(g_clock, font_display(64), 0);
  lv_obj_set_style_text_letter_space(g_clock, 1, 0);

  g_date = lv_label_create(hero);
  lv_label_set_text(g_date, db);
  lv_obj_set_style_text_color(g_date, theme::gold(), 0);
  lv_obj_set_style_text_font(g_date, &lv_font_montserrat_14, 0);

  g_timer = lv_timer_create(tick, 1000, nullptr);

  /* App grid */
  lv_obj_t * grid = lv_obj_create(body);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, lv_pct(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_style_pad_hor(grid, 8, 0);
  lv_obj_set_style_pad_top(grid, 4, 0);
  lv_obj_set_style_pad_row(grid, 10, 0);
  lv_obj_set_style_pad_column(grid, 4, 0);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(grid, cols, rows);

  struct Item {
    AppIcon icon;
    const char * label;
    lv_event_cb_t cb;
  };
  const Item items[] = {
      {AppIcon::Werk, "WerkPager", [](lv_event_t * /*e*/) { go_werk(); }},
      {AppIcon::Games, "Games", [](lv_event_t * /*e*/) { go_games_folder(); }},
      {AppIcon::Doodle, "Doodle", [](lv_event_t * /*e*/) { go_doodle(); }},
      {AppIcon::Settings, "Settings", [](lv_event_t * /*e*/) { go_settings(); }},
  };
  for (int i = 0; i < 4; ++i) {
    lv_obj_t * icon = make_app_icon(grid, items[i].icon, items[i].label, items[i].cb);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, i % 2, 1, LV_GRID_ALIGN_CENTER, i / 2, 1);
  }

  /* Footer status */
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
