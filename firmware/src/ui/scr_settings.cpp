#include "ui/scr_settings.h"

#include "app/app.h"
#include "protocol/messages.h"
#include "ui/brightness.h"
#include "ui/chrome.h"
#include "ui/emoji_badge.h"
#include "ui/emoji_palette.h"
#include "ui/nav.h"
#include "ui/scr_pager.h"
#include "ui/theme.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace wp {
namespace ui {
namespace {

lv_obj_t * g_osk_value = nullptr;
char g_osk_buf[64] = "";
bool g_caps = true;
lv_obj_t * g_osk_letters[32];
int g_osk_letter_n = 0;
lv_obj_t * g_caps_btn = nullptr;
/* -1 name, >=0 canned, -2 compose, -3 wifi ssid, -4 wifi password */
int g_osk_canned_index = -1;
char g_wifi_ssid_draft[33] = "";

void add_section(lv_obj_t * body, const char * title) {
  lv_obj_t * t = lv_label_create(body);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_color(t, theme::muted(), 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
  lv_obj_set_width(t, lv_pct(100));
}

void chip_set_selected(lv_obj_t * b, bool selected) {
  if (!b) return;
  lv_obj_set_style_bg_color(b, selected ? theme::gold() : lv_color_hex(0x4a4558), 0);
  lv_obj_t * l = lv_obj_get_child(b, 0);
  if (l) lv_obj_set_style_text_color(l, selected ? lv_color_hex(0x1a1224) : theme::ink(), 0);
}

void chip_row_select(lv_obj_t * row, int selected_idx) {
  if (!row) return;
  const uint32_t n = lv_obj_get_child_count(row);
  for (uint32_t i = 0; i < n; ++i) {
    chip_set_selected(lv_obj_get_child(row, i), (int)i == selected_idx);
  }
}

lv_obj_t * chip(lv_obj_t * row, const char * label, bool selected, lv_event_cb_t cb, void * ud) {
  lv_obj_t * b = lv_button_create(row);
  lv_obj_set_height(b, 34);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_pad_hor(b, 14, 0);
  lv_obj_t * l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_center(l);
  chip_set_selected(b, selected);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

void osk_refresh() {
  if (!g_osk_value) return;
  char shown[72];
  if (g_osk_canned_index == -4) {
    /* Mask password while typing */
    const size_t n = std::strlen(g_osk_buf);
    size_t i = 0;
    for (; i < n && i + 1 < sizeof(shown); ++i) shown[i] = '*';
    shown[i++] = '|';
    shown[i] = '\0';
  } else {
    lv_snprintf(shown, sizeof(shown), "%s|", g_osk_buf);
  }
  lv_label_set_text(g_osk_value, shown);
}

void osk_type(const char * ch) {
  int max = 22;
  if (g_osk_canned_index == -1) max = 12;
  else if (g_osk_canned_index == -3) max = 32;
  else if (g_osk_canned_index == -4) max = 63;
  if ((int)std::strlen(g_osk_buf) >= max) return;
  char c = ch[0];
  if (g_caps && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  const size_t n = std::strlen(g_osk_buf);
  g_osk_buf[n] = c;
  g_osk_buf[n + 1] = '\0';
  osk_refresh();
}

void osk_apply_caps() {
  for (int i = 0; i < g_osk_letter_n; ++i) {
    lv_obj_t * l = g_osk_letters[i];
    const char * t = lv_label_get_text(l);
    if (!t || !t[0]) continue;
    char c = t[0];
    if (g_caps && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (!g_caps && c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    char buf[2] = {c, 0};
    lv_label_set_text(l, buf);
  }
  if (g_caps_btn) {
    lv_obj_set_style_bg_color(g_caps_btn, g_caps ? theme::gold() : theme::panel(), 0);
    lv_obj_t * cl = lv_obj_get_child(g_caps_btn, 0);
    if (cl) lv_obj_set_style_text_color(cl, g_caps ? lv_color_hex(0x1a1200) : theme::ink(), 0);
  }
}

void add_osk_row(lv_obj_t * parent, const char * keys, int inset, bool letters) {
  lv_obj_t * row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, 40);
  lv_obj_set_style_pad_left(row, inset, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 4, 0);
  for (const char * p = keys; *p; ++p) {
    char key[2] = {*p, 0};
    lv_obj_t * b = lv_button_create(row);
    lv_obj_set_size(b, 38, 38);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, key);
    lv_obj_set_style_text_color(l, theme::ink(), 0);
    lv_obj_center(l);
    if (letters && g_osk_letter_n < 32) g_osk_letters[g_osk_letter_n++] = l;
    lv_obj_add_event_cb(
        b,
        [](lv_event_t * e) {
          osk_type(static_cast<const char *>(lv_event_get_user_data(e)));
        },
        LV_EVENT_CLICKED, (void *)p);
  }
}

lv_obj_t * build_keyboard(const char * title, const char * initial, int canned_index) {
  g_osk_canned_index = canned_index;
  g_caps = canned_index == -1; /* name starts CAPS; others start lower */
  std::snprintf(g_osk_buf, sizeof(g_osk_buf), "%s", initial ? initial : "");

  lv_obj_t * scr = make_screen();
  make_topbar(scr, canned_index == -2 ? "WERK ROOM" : "SETTINGS", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  make_tagline(body, title);

  g_osk_value = lv_label_create(body);
  lv_obj_set_style_text_color(g_osk_value, theme::ink(), 0);
  lv_obj_set_style_text_font(g_osk_value, &lv_font_montserrat_20, 0);
  lv_obj_set_style_bg_color(g_osk_value, theme::panel(), 0);
  lv_obj_set_style_bg_opa(g_osk_value, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(g_osk_value, 10, 0);
  lv_obj_set_style_radius(g_osk_value, 10, 0);
  lv_obj_set_width(g_osk_value, lv_pct(100));
  osk_refresh();

  g_osk_letter_n = 0;
  g_caps_btn = nullptr;
  add_osk_row(body, "1234567890", 0, false);
  add_osk_row(body, "!?&#@/+-='", 0, false);
  add_osk_row(body, "qwertyuiop", 0, true);
  add_osk_row(body, "asdfghjkl", 16, true);
  add_osk_row(body, "zxcvbnm", 40, true);

  lv_obj_t * actions = lv_obj_create(body);
  lv_obj_remove_style_all(actions);
  lv_obj_set_width(actions, lv_pct(100));
  lv_obj_set_height(actions, 44);
  lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(actions, 6, 0);

  g_caps_btn = lv_button_create(actions);
  lv_obj_set_flex_grow(g_caps_btn, 1);
  lv_obj_set_style_bg_color(g_caps_btn, theme::panel(), 0);
  lv_obj_set_style_shadow_width(g_caps_btn, 0, 0);
  lv_obj_t * cl = lv_label_create(g_caps_btn);
  lv_label_set_text(cl, "CAPS");
  lv_obj_center(cl);
  lv_obj_add_event_cb(
      g_caps_btn,
      [](lv_event_t * /*e*/) {
        g_caps = !g_caps;
        osk_apply_caps();
      },
      LV_EVENT_CLICKED, nullptr);
  osk_apply_caps();

  lv_obj_t * space = lv_button_create(actions);
  lv_obj_set_flex_grow(space, 3);
  lv_obj_set_style_bg_color(space, theme::panel(), 0);
  lv_obj_set_style_shadow_width(space, 0, 0);
  lv_obj_t * sl = lv_label_create(space);
  lv_label_set_text(sl, "space");
  lv_obj_center(sl);
  lv_obj_add_event_cb(space, [](lv_event_t * /*e*/) { osk_type(" "); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t * bk = lv_button_create(actions);
  lv_obj_set_flex_grow(bk, 1);
  lv_obj_set_style_bg_color(bk, theme::panel(), 0);
  lv_obj_set_style_shadow_width(bk, 0, 0);
  lv_obj_t * bl = lv_label_create(bk);
  lv_label_set_text(bl, LV_SYMBOL_BACKSPACE);
  lv_obj_set_style_text_color(bl, theme::ink(), 0);
  lv_obj_center(bl);
  lv_obj_add_event_cb(
      bk,
      [](lv_event_t * /*e*/) {
        const size_t n = std::strlen(g_osk_buf);
        if (n > 0) g_osk_buf[n - 1] = '\0';
        osk_refresh();
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Cancel", false, false, [](lv_event_t * /*e*/) {
    if (g_osk_canned_index == -2) go_compose_refresh();
    else go_settings();
  });
  dock_btn(dock, "Done", true, false, [](lv_event_t * /*e*/) {
    if (g_osk_canned_index == -2) {
      compose_set_message(g_osk_buf);
      go_compose_refresh();
      return;
    }
    if (g_osk_canned_index == -4) {
      /* PC sim: accept any password; device will join STA for real. */
      app::Desk & d = app::desk();
      std::snprintf(d.wifi_ssid, sizeof(d.wifi_ssid), "%s", g_wifi_ssid_draft);
      d.wifi_connected = true;
      g_wifi_ssid_draft[0] = '\0';
      g_osk_buf[0] = '\0'; /* don't keep password */
      app::save();
      toast_fmt("Connected to %s", d.wifi_ssid);
      go_settings();
      return;
    }
    app::Desk & d = app::desk();
    if (g_osk_canned_index >= 0 && g_osk_canned_index < app::kCannedCount) {
      if (!g_osk_buf[0]) std::snprintf(g_osk_buf, sizeof(g_osk_buf), "Message %d", g_osk_canned_index + 1);
      std::snprintf(d.canned[g_osk_canned_index], sizeof(d.canned[0]), "%s", g_osk_buf);
    } else {
      if (!g_osk_buf[0]) std::snprintf(g_osk_buf, sizeof(g_osk_buf), "Queen");
      std::snprintf(d.name, sizeof(d.name), "%s", g_osk_buf);
    }
    app::save();
    go_settings();
  });
  return scr;
}

void on_scan(lv_event_t * /*e*/) {
  app::desk().nearby_count = 0;
  proto::Msg m;
  m.type = proto::MsgType::Discover;
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", app::desk().id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", app::desk().name);
  app::send(m);
  toast("Scanning desks...");
}

/* Draft clock values while editing (web dateInput / clockInput). */
int g_y = 2026, g_mo = 8, g_d = 4, g_hh = 12, g_mm = 0;
/* Snapshot at picker open — restored if user hits X (cancel). */
int g_snap_y = 2026, g_snap_mo = 8, g_snap_d = 4, g_snap_hh = 12, g_snap_mm = 0;

void sync_draft_from_desk() {
  std::tm tm{};
  app::local_time(&tm);
  g_y = tm.tm_year + 1900;
  g_mo = tm.tm_mon + 1;
  g_d = tm.tm_mday;
  g_hh = tm.tm_hour;
  g_mm = tm.tm_min;
}

void snapshot_draft() {
  g_snap_y = g_y;
  g_snap_mo = g_mo;
  g_snap_d = g_d;
  g_snap_hh = g_hh;
  g_snap_mm = g_mm;
}

void restore_draft() {
  g_y = g_snap_y;
  g_mo = g_snap_mo;
  g_d = g_snap_d;
  g_hh = g_snap_hh;
  g_mm = g_snap_mm;
}

void apply_draft_clock() {
  std::tm target{};
  target.tm_year = g_y - 1900;
  target.tm_mon = g_mo - 1;
  target.tm_mday = g_d;
  target.tm_hour = g_hh;
  target.tm_min = g_mm;
  target.tm_sec = 0;
  target.tm_isdst = -1;
  const std::time_t want = std::mktime(&target);
  const std::time_t now = std::time(nullptr);
  if (want != (std::time_t)-1) {
    app::desk().clock_offset_ms = (int64_t)(want - now) * 1000;
    app::save();
    toast("Date & time set");
  }
  go_settings();
}

lv_obj_t * make_picker_close_btn(lv_obj_t * parent, lv_event_cb_t on_cancel) {
  lv_obj_t * x = lv_button_create(parent);
  lv_obj_set_size(x, 36, 32);
  lv_obj_set_style_bg_color(x, theme::bg0(), 0);
  lv_obj_set_style_shadow_width(x, 0, 0);
  lv_obj_set_style_radius(x, 8, 0);
  lv_obj_t * xl = lv_label_create(x);
  lv_label_set_text(xl, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_color(xl, theme::ink(), 0);
  lv_obj_center(xl);
  if (on_cancel) lv_obj_add_event_cb(x, on_cancel, LV_EVENT_CLICKED, nullptr);
  return x;
}

int days_in_month(int y, int mo) {
  static const int kDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (mo == 2) {
    const bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    return leap ? 29 : 28;
  }
  if (mo < 1 || mo > 12) return 31;
  return kDays[mo];
}

lv_obj_t * g_date_grid = nullptr;
lv_obj_t * g_date_title = nullptr;
lv_obj_t * g_date_ov = nullptr;
int g_hour12 = 12;
int g_ampm = 0; /* 0=AM 1=PM */
lv_obj_t * g_time_hour_lbl = nullptr;
lv_obj_t * g_time_min_lbl = nullptr;
lv_obj_t * g_time_ampm_lbl = nullptr;

void refresh_date_title() {
  if (!g_date_title) return;
  static const char * kMo[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char tb[24];
  lv_snprintf(tb, sizeof(tb), "%s %d", kMo[(g_mo - 1 + 12) % 12], g_y);
  lv_label_set_text(g_date_title, tb);
}

void rebuild_date_grid() {
  if (!g_date_grid) return;
  while (lv_obj_get_child_count(g_date_grid) > 0) lv_obj_delete(lv_obj_get_child(g_date_grid, 0));
  static const char * kWd[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  for (int i = 0; i < 7; ++i) {
    lv_obj_t * h = lv_label_create(g_date_grid);
    lv_label_set_text(h, kWd[i]);
    lv_obj_set_style_text_color(h, theme::muted(), 0);
    lv_obj_set_style_text_font(h, &lv_font_montserrat_12, 0);
    lv_obj_set_grid_cell(h, LV_GRID_ALIGN_CENTER, i, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  }
  std::tm t{};
  t.tm_year = g_y - 1900;
  t.tm_mon = g_mo - 1;
  t.tm_mday = 1;
  t.tm_isdst = -1;
  std::mktime(&t);
  const int start = t.tm_wday;
  const int dim = days_in_month(g_y, g_mo);
  for (int day = 1; day <= dim; ++day) {
    const int slot = start + day - 1;
    lv_obj_t * b = lv_button_create(g_date_grid);
    lv_obj_set_size(b, 36, 28);
    const bool sel = day == g_d;
    lv_obj_set_style_bg_color(b, sel ? theme::gold() : theme::bg0(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_t * l = lv_label_create(b);
    char buf[4];
    lv_snprintf(buf, sizeof(buf), "%d", day);
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_color(l, sel ? lv_color_hex(0x1a1200) : theme::ink(), 0);
    lv_obj_center(l);
    lv_obj_set_grid_cell(b, LV_GRID_ALIGN_CENTER, slot % 7, 1, LV_GRID_ALIGN_CENTER, 1 + slot / 7, 1);
    lv_obj_add_event_cb(
        b,
        [](lv_event_t * e) {
          g_d = (int)(intptr_t)lv_event_get_user_data(e);
          rebuild_date_grid(); /* select only — Done commits */
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)day);
  }
}

void close_date_picker() {
  if (!g_date_ov) return;
  lv_obj_delete(g_date_ov);
  g_date_ov = nullptr;
  g_date_grid = nullptr;
  g_date_title = nullptr;
}

void open_date_picker() {
  sync_draft_from_desk();
  snapshot_draft();

  g_date_ov = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g_date_ov);
  lv_obj_set_size(g_date_ov, WP_HOR_RES, WP_VER_RES);
  lv_obj_set_style_bg_color(g_date_ov, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(g_date_ov, 180, 0);

  lv_obj_t * card = lv_obj_create(g_date_ov);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, 360, 360);
  lv_obj_center(card);
  lv_obj_set_style_bg_color(card, theme::panel(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 16, 0);
  lv_obj_set_style_pad_all(card, 12, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(card, 8, 0);

  lv_obj_t * header = lv_obj_create(card);
  lv_obj_remove_style_all(header);
  lv_obj_set_width(header, lv_pct(100));
  lv_obj_set_height(header, 32);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t * hdr = lv_label_create(header);
  lv_label_set_text(hdr, "Set date");
  lv_obj_set_style_text_color(hdr, theme::muted(), 0);
  make_picker_close_btn(header, [](lv_event_t * /*e*/) {
    restore_draft();
    close_date_picker();
  });

  lv_obj_t * nav = lv_obj_create(card);
  lv_obj_remove_style_all(nav);
  lv_obj_set_width(nav, lv_pct(100));
  lv_obj_set_height(nav, 36);
  lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t * prev = lv_button_create(nav);
  lv_obj_set_size(prev, 40, 32);
  lv_obj_set_style_bg_color(prev, theme::bg0(), 0);
  lv_obj_set_style_shadow_width(prev, 0, 0);
  lv_obj_t * pl = lv_label_create(prev);
  lv_label_set_text(pl, "<");
  lv_obj_center(pl);
  lv_obj_add_event_cb(
      prev,
      [](lv_event_t * /*e*/) {
        g_mo--;
        if (g_mo < 1) {
          g_mo = 12;
          g_y--;
        }
        if (g_d > days_in_month(g_y, g_mo)) g_d = days_in_month(g_y, g_mo);
        refresh_date_title();
        rebuild_date_grid();
      },
      LV_EVENT_CLICKED, nullptr);

  g_date_title = lv_label_create(nav);
  lv_obj_set_style_text_color(g_date_title, theme::ink(), 0);
  refresh_date_title();

  lv_obj_t * next = lv_button_create(nav);
  lv_obj_set_size(next, 40, 32);
  lv_obj_set_style_bg_color(next, theme::bg0(), 0);
  lv_obj_set_style_shadow_width(next, 0, 0);
  lv_obj_t * nl = lv_label_create(next);
  lv_label_set_text(nl, ">");
  lv_obj_center(nl);
  lv_obj_add_event_cb(
      next,
      [](lv_event_t * /*e*/) {
        g_mo++;
        if (g_mo > 12) {
          g_mo = 1;
          g_y++;
        }
        if (g_d > days_in_month(g_y, g_mo)) g_d = days_in_month(g_y, g_mo);
        refresh_date_title();
        rebuild_date_grid();
      },
      LV_EVENT_CLICKED, nullptr);

  g_date_grid = lv_obj_create(card);
  lv_obj_remove_style_all(g_date_grid);
  lv_obj_set_width(g_date_grid, lv_pct(100));
  lv_obj_set_flex_grow(g_date_grid, 1);
  lv_obj_set_layout(g_date_grid, LV_LAYOUT_GRID);
  static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                              LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(g_date_grid, cols, rows);
  lv_obj_set_style_pad_row(g_date_grid, 4, 0);
  lv_obj_set_style_pad_column(g_date_grid, 4, 0);
  rebuild_date_grid();

  lv_obj_t * done = lv_button_create(card);
  lv_obj_set_width(done, lv_pct(100));
  lv_obj_set_height(done, 40);
  lv_obj_set_style_bg_color(done, theme::gold(), 0);
  lv_obj_set_style_shadow_width(done, 0, 0);
  lv_obj_t * dl = lv_label_create(done);
  lv_label_set_text(dl, "Done");
  lv_obj_set_style_text_color(dl, lv_color_hex(0x1a1200), 0);
  lv_obj_center(dl);
  lv_obj_add_event_cb(
      done,
      [](lv_event_t * /*e*/) {
        close_date_picker();
        apply_draft_clock();
      },
      LV_EVENT_CLICKED, nullptr);
}

void refresh_time_labels() {
  if (g_time_hour_lbl) {
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", g_hour12);
    lv_label_set_text(g_time_hour_lbl, buf);
  }
  if (g_time_min_lbl) {
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%02d", g_mm);
    lv_label_set_text(g_time_min_lbl, buf);
  }
  if (g_time_ampm_lbl) lv_label_set_text(g_time_ampm_lbl, g_ampm ? "PM" : "AM");
}

lv_obj_t * g_time_ov = nullptr;

void close_time_picker() {
  if (!g_time_ov) return;
  lv_obj_delete(g_time_ov);
  g_time_ov = nullptr;
  g_time_hour_lbl = g_time_min_lbl = g_time_ampm_lbl = nullptr;
}

void open_time_picker() {
  sync_draft_from_desk();
  snapshot_draft();
  g_hour12 = g_hh % 12;
  if (g_hour12 == 0) g_hour12 = 12;
  g_ampm = g_hh >= 12 ? 1 : 0;

  g_time_ov = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g_time_ov);
  lv_obj_set_size(g_time_ov, WP_HOR_RES, WP_VER_RES);
  lv_obj_set_style_bg_color(g_time_ov, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(g_time_ov, 180, 0);

  lv_obj_t * card = lv_obj_create(g_time_ov);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, 320, 280);
  lv_obj_center(card);
  lv_obj_set_style_bg_color(card, theme::panel(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 16, 0);
  lv_obj_set_style_pad_all(card, 14, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, 10, 0);

  lv_obj_t * header = lv_obj_create(card);
  lv_obj_remove_style_all(header);
  lv_obj_set_width(header, lv_pct(100));
  lv_obj_set_height(header, 32);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t * title = lv_label_create(header);
  lv_label_set_text(title, "Set time");
  lv_obj_set_style_text_color(title, theme::muted(), 0);
  make_picker_close_btn(header, [](lv_event_t * /*e*/) {
    restore_draft();
    close_time_picker();
  });

  lv_obj_t * row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, lv_pct(100), 120);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  /* Hour column */
  lv_obj_t * hcol = lv_obj_create(row);
  lv_obj_remove_style_all(hcol);
  lv_obj_set_size(hcol, 70, 110);
  lv_obj_set_flex_flow(hcol, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(hcol, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t * hup = lv_button_create(hcol);
  lv_obj_set_size(hup, 56, 28);
  lv_obj_set_style_bg_color(hup, theme::bg0(), 0);
  lv_obj_set_style_shadow_width(hup, 0, 0);
  lv_label_set_text(lv_label_create(hup), "+");
  lv_obj_center(lv_obj_get_child(hup, 0));
  g_time_hour_lbl = lv_label_create(hcol);
  lv_obj_set_style_text_font(g_time_hour_lbl, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(g_time_hour_lbl, theme::ink(), 0);
  lv_obj_t * hdn = lv_button_create(hcol);
  lv_obj_set_size(hdn, 56, 28);
  lv_obj_set_style_bg_color(hdn, theme::bg0(), 0);
  lv_obj_set_style_shadow_width(hdn, 0, 0);
  lv_label_set_text(lv_label_create(hdn), "-");
  lv_obj_center(lv_obj_get_child(hdn, 0));
  lv_obj_add_event_cb(hup, [](lv_event_t * /*e*/) {
    g_hour12 = g_hour12 >= 12 ? 1 : g_hour12 + 1;
    refresh_time_labels();
  }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(hdn, [](lv_event_t * /*e*/) {
    g_hour12 = g_hour12 <= 1 ? 12 : g_hour12 - 1;
    refresh_time_labels();
  }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t * colon = lv_label_create(row);
  lv_label_set_text(colon, ":");
  lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(colon, theme::ink(), 0);

  /* Minute column */
  lv_obj_t * mcol = lv_obj_create(row);
  lv_obj_remove_style_all(mcol);
  lv_obj_set_size(mcol, 70, 110);
  lv_obj_set_flex_flow(mcol, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mcol, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t * mup = lv_button_create(mcol);
  lv_obj_set_size(mup, 56, 28);
  lv_obj_set_style_bg_color(mup, theme::bg0(), 0);
  lv_obj_set_style_shadow_width(mup, 0, 0);
  lv_label_set_text(lv_label_create(mup), "+");
  lv_obj_center(lv_obj_get_child(mup, 0));
  g_time_min_lbl = lv_label_create(mcol);
  lv_obj_set_style_text_font(g_time_min_lbl, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(g_time_min_lbl, theme::ink(), 0);
  lv_obj_t * mdn = lv_button_create(mcol);
  lv_obj_set_size(mdn, 56, 28);
  lv_obj_set_style_bg_color(mdn, theme::bg0(), 0);
  lv_obj_set_style_shadow_width(mdn, 0, 0);
  lv_label_set_text(lv_label_create(mdn), "-");
  lv_obj_center(lv_obj_get_child(mdn, 0));
  lv_obj_add_event_cb(mup, [](lv_event_t * /*e*/) {
    g_mm = (g_mm + 1) % 60;
    refresh_time_labels();
  }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(mdn, [](lv_event_t * /*e*/) {
    g_mm = (g_mm + 59) % 60;
    refresh_time_labels();
  }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t * ampm = lv_button_create(row);
  lv_obj_set_size(ampm, 56, 40);
  lv_obj_set_style_bg_color(ampm, theme::gold(), 0);
  lv_obj_set_style_shadow_width(ampm, 0, 0);
  g_time_ampm_lbl = lv_label_create(ampm);
  lv_obj_set_style_text_color(g_time_ampm_lbl, lv_color_hex(0x1a1200), 0);
  lv_obj_center(g_time_ampm_lbl);
  lv_obj_add_event_cb(ampm, [](lv_event_t * /*e*/) {
    g_ampm = 1 - g_ampm;
    refresh_time_labels();
  }, LV_EVENT_CLICKED, nullptr);

  refresh_time_labels();

  lv_obj_t * done = lv_button_create(card);
  lv_obj_set_width(done, lv_pct(100));
  lv_obj_set_height(done, 40);
  lv_obj_set_style_bg_color(done, theme::gold(), 0);
  lv_obj_set_style_shadow_width(done, 0, 0);
  lv_obj_t * dl = lv_label_create(done);
  lv_label_set_text(dl, "Done");
  lv_obj_set_style_text_color(dl, lv_color_hex(0x1a1200), 0);
  lv_obj_center(dl);
  lv_obj_add_event_cb(
      done,
      [](lv_event_t * /*e*/) {
        int h = g_hour12 % 12;
        if (g_ampm) h += 12;
        g_hh = h;
        close_time_picker();
        apply_draft_clock();
      },
      LV_EVENT_CLICKED, nullptr);
}

}  // namespace

lv_obj_t * settings_screen() {
  app::Desk & d = app::desk();
  theme::set(static_cast<theme::Id>(d.theme));

  lv_obj_t * scr = make_screen();
  make_topbar(scr, "SETTINGS", d.name);
  lv_obj_t * body = make_body(scr, true);
  make_tagline(body, "Settings");

  add_section(body, "My name");
  lv_obj_t * name = lv_button_create(body);
  lv_obj_set_width(name, lv_pct(100));
  lv_obj_set_height(name, 44);
  style_peer_like(name);
  lv_obj_t * nl = lv_label_create(name);
  lv_label_set_text(nl, d.name);
  lv_obj_set_style_text_color(nl, lv_color_hex(0x1a0a12), 0);
  lv_obj_center(nl);
  lv_obj_add_event_cb(name, [](lv_event_t * /*e*/) { go_keyboard_name(); }, LV_EVENT_CLICKED, nullptr);

  add_section(body, "Theme");
  lv_obj_t * themes = lv_obj_create(body);
  lv_obj_remove_style_all(themes);
  lv_obj_set_width(themes, lv_pct(100));
  lv_obj_set_height(themes, 40);
  lv_obj_set_flex_flow(themes, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(themes, 8, 0);
  for (int i = 0; i < (int)theme::Id::Count; ++i) {
    const auto id = static_cast<theme::Id>(i);
    lv_obj_t * s = lv_obj_create(themes);
    lv_obj_remove_style_all(s);
    lv_obj_set_size(s, 48, 32);
    lv_obj_set_style_radius(s, 10, 0);
    theme::Id prev = theme::current();
    theme::set(id);
    lv_obj_set_style_bg_color(s, theme::hot(), 0);
    lv_obj_set_style_bg_grad_color(s, theme::gold(), 0);
    theme::set(prev);
    lv_obj_set_style_bg_grad_dir(s, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    const bool sel = (d.theme == (uint8_t)i);
    lv_obj_set_style_border_width(s, sel ? 2 : 0, 0);
    lv_obj_set_style_border_color(s, theme::ink(), 0);
    lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        s,
        [](lv_event_t * e) {
          const int id = (int)(intptr_t)lv_event_get_user_data(e);
          app::desk().theme = (uint8_t)id;
          theme::set(static_cast<theme::Id>(id));
          app::save();
          go_settings();
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  add_section(body, "Brightness");
  {
    lv_obj_t * row = lv_obj_create(body);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 44);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lo = lv_label_create(row);
    lv_label_set_text(lo, "Dim");
    lv_obj_set_style_text_color(lo, theme::muted(), 0);
    lv_obj_set_style_text_font(lo, &lv_font_montserrat_12, 0);

    lv_obj_t * slider = lv_slider_create(row);
    lv_obj_set_flex_grow(slider, 1);
    lv_obj_set_height(slider, 12);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, brightness::percent(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, theme::panel(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, theme::gold(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, theme::ink(), LV_PART_KNOB);
    lv_obj_add_event_cb(
        slider,
        [](lv_event_t * e) {
          auto * s = static_cast<lv_obj_t *>(lv_event_get_target(e));
          brightness::set_percent((uint8_t)lv_slider_get_value(s));
        },
        LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t * hi = lv_label_create(row);
    lv_label_set_text(hi, "Bright");
    lv_obj_set_style_text_color(hi, theme::muted(), 0);
    lv_obj_set_style_text_font(hi, &lv_font_montserrat_12, 0);
  }

  add_section(body, "WiFi (optional) - clock sync & OTA updates");
  {
    lv_obj_t * status = lv_label_create(body);
    if (d.wifi_connected && d.wifi_ssid[0]) {
      char st[48];
      lv_snprintf(st, sizeof(st), "Connected - %s", d.wifi_ssid);
      lv_label_set_text(status, st);
      lv_obj_set_style_text_color(status, theme::mint(), 0);
    } else {
      lv_label_set_text(status, "Not connected");
      lv_obj_set_style_text_color(status, theme::muted(), 0);
    }
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_width(status, lv_pct(100));

    lv_obj_t * wifi_row = lv_obj_create(body);
    lv_obj_remove_style_all(wifi_row);
    lv_obj_set_width(wifi_row, lv_pct(100));
    lv_obj_set_height(wifi_row, 40);
    lv_obj_set_flex_flow(wifi_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(wifi_row, 6, 0);
    lv_obj_remove_flag(wifi_row, LV_OBJ_FLAG_SCROLLABLE);

    chip(wifi_row, d.wifi_connected ? "Change" : "Scan Wi-Fi", false,
         [](lv_event_t * /*e*/) { go_wifi_scan(); }, nullptr);
    if (d.wifi_connected) {
      chip(wifi_row, "Disconnect", false,
           [](lv_event_t * /*e*/) {
             app::desk().wifi_connected = false;
             app::desk().wifi_ssid[0] = '\0';
             app::save();
             toast("Wi-Fi disconnected");
             go_settings();
           },
           nullptr);
    }

    lv_obj_t * wifi_acts = lv_obj_create(body);
    lv_obj_remove_style_all(wifi_acts);
    lv_obj_set_width(wifi_acts, lv_pct(100));
    lv_obj_set_height(wifi_acts, 40);
    lv_obj_set_flex_flow(wifi_acts, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(wifi_acts, 6, 0);
    lv_obj_remove_flag(wifi_acts, LV_OBJ_FLAG_SCROLLABLE);

    chip(wifi_acts, "Sync time", false,
         [](lv_event_t * /*e*/) {
           if (!app::desk().wifi_connected) {
             toast("Connect to Wi-Fi first");
             return;
           }
           app::desk().clock_offset_ms = 0;
           app::save();
           toast("Time synced");
           /* stay put — go_settings() would jump scroll to top */
         },
         nullptr);
    chip(wifi_acts, "Updates", false,
         [](lv_event_t * /*e*/) {
           if (!app::desk().wifi_connected) {
             toast("Connect to Wi-Fi first");
             return;
           }
           go_ota_releases();
         },
         nullptr);
  }

  add_section(body, "Screen timeout");
  lv_obj_t * to = lv_obj_create(body);
  lv_obj_remove_style_all(to);
  lv_obj_set_width(to, lv_pct(100));
  lv_obj_set_height(to, 40);
  lv_obj_set_flex_flow(to, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(to, 6, 0);
  const app::TimeoutSpec * specs = app::timeout_specs();
  for (int i = 0; i < 4; ++i) {
    chip(to, specs[i].label, d.timeout_id == (uint8_t)i,
         [](lv_event_t * e) {
           const auto id = (uint8_t)(intptr_t)lv_event_get_user_data(e);
           app::desk().timeout_id = id;
           app::save();
           /* stay put — go_settings() would jump scroll to top */
           chip_row_select(lv_obj_get_parent(static_cast<lv_obj_t *>(lv_event_get_target(e))),
                           (int)id);
         },
         (void *)(intptr_t)i);
  }

  add_section(body, "When idle");
  lv_obj_t * idle = lv_obj_create(body);
  lv_obj_remove_style_all(idle);
  lv_obj_set_width(idle, lv_pct(100));
  lv_obj_set_height(idle, 40);
  lv_obj_set_flex_flow(idle, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(idle, 6, 0);
  chip(idle, "Black", d.idle_mode == 0,
       [](lv_event_t * e) {
         app::desk().idle_mode = 0;
         app::save();
         chip_row_select(lv_obj_get_parent(static_cast<lv_obj_t *>(lv_event_get_target(e))), 0);
       },
       nullptr);
  chip(idle, "Clock", d.idle_mode == 1,
       [](lv_event_t * e) {
         app::desk().idle_mode = 1;
         app::save();
         chip_row_select(lv_obj_get_parent(static_cast<lv_obj_t *>(lv_event_get_target(e))), 1);
       },
       nullptr);

  add_section(body, "Date & time (saved; keeps ticking vs wall clock)");
  sync_draft_from_desk();
  {
    std::tm tm{};
    app::local_time(&tm);
    char date_lbl[40], time_lbl[24];
    static const char * kMo[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int h12 = tm.tm_hour % 12;
    if (h12 == 0) h12 = 12;
    lv_snprintf(date_lbl, sizeof(date_lbl), "%s %d, %d", kMo[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900);
    lv_snprintf(time_lbl, sizeof(time_lbl), "%d:%02d %s", h12, tm.tm_min, tm.tm_hour >= 12 ? "PM" : "AM");

    lv_obj_t * dt = lv_obj_create(body);
    lv_obj_remove_style_all(dt);
    lv_obj_set_width(dt, lv_pct(100));
    lv_obj_set_height(dt, 40);
    lv_obj_set_flex_flow(dt, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dt, 6, 0);
    chip(dt, date_lbl, false, [](lv_event_t * /*e*/) { open_date_picker(); }, nullptr);
    chip(dt, time_lbl, false, [](lv_event_t * /*e*/) { open_time_picker(); }, nullptr);
  }

  add_section(body, "Werk emojis (tap to change)");
  lv_obj_t * emos = lv_obj_create(body);
  lv_obj_remove_style_all(emos);
  lv_obj_set_width(emos, lv_pct(100));
  lv_obj_set_height(emos, 48);
  lv_obj_set_flex_flow(emos, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(emos, 4, 0);
  for (int i = 0; i < app::kEmojiSlots; ++i) {
    lv_obj_t * b = lv_button_create(emos);
    lv_obj_set_size(b, 42, 42);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 2, 0);
    lv_obj_t * img = make_emoji_image(b, d.emojis[i], 32);
    lv_obj_center(img);
    lv_obj_add_event_cb(
        b, [](lv_event_t * e) { go_emoji_picker((int)(intptr_t)lv_event_get_user_data(e)); },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  add_section(body, "Canned messages (tap to edit)");
  for (int i = 0; i < app::kCannedCount; ++i) {
    lv_obj_t * b = lv_button_create(body);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_height(b, 38);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, d.canned[i]);
    lv_obj_set_style_text_color(l, theme::ink(), 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_add_event_cb(
        b, [](lv_event_t * e) { go_keyboard_canned((int)(intptr_t)lv_event_get_user_data(e)); },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  add_section(body, "Saved desks");
  if (d.peer_count == 0) make_tagline(body, "None yet - scan nearby.");
  for (int i = 0; i < d.peer_count; ++i) {
    lv_obj_t * row = lv_obj_create(body);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 40);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t * n = lv_label_create(row);
    lv_label_set_text(n, d.peers[i].name);
    lv_obj_set_style_text_color(n, theme::ink(), 0);
    lv_obj_t * rm = lv_button_create(row);
    lv_obj_set_size(rm, 80, 32);
    lv_obj_set_style_bg_color(rm, theme::danger(), 0);
    lv_obj_set_style_shadow_width(rm, 0, 0);
    lv_obj_t * rl = lv_label_create(rm);
    lv_label_set_text(rl, "Remove");
    lv_obj_center(rl);
    lv_obj_add_event_cb(
        rm,
        [](lv_event_t * e) {
          const int idx = (int)(intptr_t)lv_event_get_user_data(e);
          if (idx >= 0 && idx < app::desk().peer_count) app::remove_peer(app::desk().peers[idx].id);
          go_settings();
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  if (d.nearby_count > 0) {
    add_section(body, "Nearby (this scan)");
    for (int i = 0; i < d.nearby_count; ++i) {
      const bool saved = app::peer_saved(d.nearby[i].id);
      lv_obj_t * row = lv_obj_create(body);
      lv_obj_remove_style_all(row);
      lv_obj_set_width(row, lv_pct(100));
      lv_obj_set_height(row, 40);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_t * n = lv_label_create(row);
      lv_label_set_text(n, d.nearby[i].name);
      lv_obj_set_style_text_color(n, theme::ink(), 0);
      if (!saved) {
        lv_obj_t * add = lv_button_create(row);
        lv_obj_set_size(add, 64, 32);
        lv_obj_set_style_bg_color(add, theme::gold(), 0);
        lv_obj_set_style_shadow_width(add, 0, 0);
        lv_obj_t * al = lv_label_create(add);
        lv_label_set_text(al, "Add");
        lv_obj_set_style_text_color(al, lv_color_hex(0x1a1200), 0);
        lv_obj_center(al);
        lv_obj_add_event_cb(
            add,
            [](lv_event_t * e) {
              const int idx = (int)(intptr_t)lv_event_get_user_data(e);
              app::Desk & desk = app::desk();
              if (idx >= 0 && idx < desk.nearby_count)
                app::add_peer(desk.nearby[idx].id, desk.nearby[idx].name);
              go_settings();
            },
            LV_EVENT_CLICKED, (void *)(intptr_t)i);
      } else {
        lv_obj_t * ok = lv_label_create(row);
        lv_label_set_text(ok, "saved");
        lv_obj_set_style_text_color(ok, theme::mint(), 0);
      }
    }
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
  dock_btn(dock, "Scan desks", true, false, on_scan);
  return scr;
}

lv_obj_t * keyboard_screen_name() {
  return build_keyboard("Enter name", app::desk().name, -1);
}

lv_obj_t * keyboard_screen_canned(int index) {
  if (index < 0 || index >= app::kCannedCount) index = 0;
  char title[24];
  lv_snprintf(title, sizeof(title), "Canned #%d", index + 1);
  return build_keyboard(title, app::desk().canned[index], index);
}

lv_obj_t * keyboard_screen_compose() {
  return build_keyboard("Custom message", compose_message(), -2);
}

lv_obj_t * keyboard_screen_wifi_pass() {
  char title[48];
  if (g_wifi_ssid_draft[0])
    lv_snprintf(title, sizeof(title), "Password - %s", g_wifi_ssid_draft);
  else
    lv_snprintf(title, sizeof(title), "WiFi password");
  return build_keyboard(title, "", -4);
}

namespace {

struct ScanAp {
  const char * ssid;
  int bars; /* 1..4 */
  bool open;
};

/* PC sim mock scan — device will fill from WiFi.scanNetworks(). */
const ScanAp kScanAps[] = {
    {"WerkOffice", 4, false},
    {"Studio-5G", 3, false},
    {"Guest", 3, true},
    {"ATT-WiFi-8821", 2, false},
    {"xfinitywifi", 1, true},
    {"PIXELATE", 2, false},
};
constexpr int kScanApCount = (int)(sizeof(kScanAps) / sizeof(kScanAps[0]));

void on_pick_ap(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= kScanApCount) return;
  const ScanAp & ap = kScanAps[idx];
  std::snprintf(g_wifi_ssid_draft, sizeof(g_wifi_ssid_draft), "%s", ap.ssid);
  if (ap.open) {
    app::Desk & d = app::desk();
    std::snprintf(d.wifi_ssid, sizeof(d.wifi_ssid), "%s", ap.ssid);
    d.wifi_connected = true;
    g_wifi_ssid_draft[0] = '\0';
    app::save();
    toast_fmt("Connected to %s", d.wifi_ssid);
    go_settings();
    return;
  }
  go_keyboard_wifi_pass();
}

}  // namespace

lv_obj_t * wifi_scan_screen() {
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "SETTINGS", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  make_tagline(body, "Choose a network");

  lv_obj_t * scanning = lv_label_create(body);
  lv_label_set_text(scanning, "Networks nearby");
  lv_obj_set_style_text_color(scanning, theme::muted(), 0);
  lv_obj_set_style_text_font(scanning, &lv_font_montserrat_12, 0);

  for (int i = 0; i < kScanApCount; ++i) {
    const ScanAp & ap = kScanAps[i];
    lv_obj_t * b = lv_button_create(body);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_height(b, 48);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, theme::border(), 0);
    lv_obj_set_style_pad_hor(b, 12, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * left = lv_obj_create(b);
    lv_obj_remove_style_all(left);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(left, 2, 0);
    lv_obj_t * name = lv_label_create(left);
    lv_label_set_text(name, ap.ssid);
    lv_obj_set_style_text_color(name, theme::ink(), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
    lv_obj_t * meta = lv_label_create(left);
    lv_label_set_text(meta, ap.open ? "Open" : "Secured");
    lv_obj_set_style_text_color(meta, theme::muted(), 0);
    lv_obj_set_style_text_font(meta, &lv_font_montserrat_12, 0);

    char bars[8];
    int n = ap.bars;
    if (n < 1) n = 1;
    if (n > 4) n = 4;
    for (int k = 0; k < n; ++k) bars[k] = '|';
    bars[n] = '\0';
    lv_obj_t * sig = lv_label_create(b);
    lv_label_set_text(sig, bars);
    lv_obj_set_style_text_color(sig, theme::gold(), 0);
    lv_obj_set_style_text_font(sig, &lv_font_montserrat_16, 0);

    lv_obj_add_event_cb(b, on_pick_ap, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Cancel", false, false, [](lv_event_t * /*e*/) { go_settings(); });
  dock_btn(dock, "Rescan", true, false, [](lv_event_t * /*e*/) {
    toast("Scanning…");
    go_wifi_scan();
  });
  return scr;
}

namespace {

struct OtaRelease {
  const char * tag;  /* e.g. v0.1.0 */
  const char * note; /* short label */
};

/* PC sim mock list — device fetches GitHub Releases API. */
const OtaRelease kOtaReleases[] = {
    {"v0.2.0", "latest"},
    {"v0.1.0-sim", "this build"},
    {"v0.1.0", "stable"},
    {"v0.0.9", "older"},
};
constexpr int kOtaReleaseCount = (int)(sizeof(kOtaReleases) / sizeof(kOtaReleases[0]));

void on_pick_release(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= kOtaReleaseCount) return;
  const OtaRelease & r = kOtaReleases[idx];
  /* Match "0.1.0-sim" against tag "v0.1.0-sim" */
  const char * cur = app::kFirmwareVersion;
  const char * tag_body = r.tag[0] == 'v' ? r.tag + 1 : r.tag;
  if (std::strcmp(tag_body, cur) == 0) {
    toast("Already on this version");
    return;
  }
  /* Real desks: download asset + flash. Sim: pretend. */
  toast_fmt("Would install %s", r.tag);
  go_settings();
}

}  // namespace

lv_obj_t * ota_releases_screen() {
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "SETTINGS", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  make_tagline(body, "Install a release");

  lv_obj_t * cur = lv_label_create(body);
  char cur_lbl[48];
  lv_snprintf(cur_lbl, sizeof(cur_lbl), "Running %s", app::kFirmwareVersion);
  lv_label_set_text(cur, cur_lbl);
  lv_obj_set_style_text_color(cur, theme::muted(), 0);
  lv_obj_set_style_text_font(cur, &lv_font_montserrat_12, 0);
  lv_obj_set_width(cur, lv_pct(100));

  lv_obj_t * hint = lv_label_create(body);
  lv_label_set_text(hint, "Pick any version to upgrade or downgrade.");
  lv_obj_set_style_text_color(hint, theme::muted(), 0);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
  lv_obj_set_width(hint, lv_pct(100));

  for (int i = 0; i < kOtaReleaseCount; ++i) {
    const OtaRelease & r = kOtaReleases[i];
    const char * tag_body = r.tag[0] == 'v' ? r.tag + 1 : r.tag;
    const bool is_current = std::strcmp(tag_body, app::kFirmwareVersion) == 0;

    lv_obj_t * b = lv_button_create(body);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_height(b, 52);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, is_current ? 2 : 1, 0);
    lv_obj_set_style_border_color(b, is_current ? theme::gold() : theme::border(), 0);
    lv_obj_set_style_pad_hor(b, 12, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * left = lv_obj_create(b);
    lv_obj_remove_style_all(left);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(left, 2, 0);
    lv_obj_t * name = lv_label_create(left);
    lv_label_set_text(name, r.tag);
    lv_obj_set_style_text_color(name, theme::ink(), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_t * meta = lv_label_create(left);
    lv_label_set_text(meta, is_current ? "current" : r.note);
    lv_obj_set_style_text_color(meta, is_current ? theme::gold() : theme::muted(), 0);
    lv_obj_set_style_text_font(meta, &lv_font_montserrat_12, 0);

    lv_obj_t * action = lv_label_create(b);
    lv_label_set_text(action, is_current ? "OK" : "Install");
    lv_obj_set_style_text_color(action, is_current ? theme::muted() : theme::mint(), 0);
    lv_obj_set_style_text_font(action, &lv_font_montserrat_14, 0);

    lv_obj_add_event_cb(b, on_pick_release, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_settings(); });
  dock_btn(dock, "Refresh", true, false, [](lv_event_t * /*e*/) {
    toast("Fetching releases…");
    go_ota_releases();
  });
  return scr;
}

int g_emoji_picker_slot = 0; /* settings 0..n-1, or kEmojiPickerCompose */
int g_emoji_picker_cat = 0;
lv_obj_t * g_emoji_grid = nullptr;
lv_obj_t * g_emoji_cat_btns[kEmojiCategoryCount] = {};

void emoji_picker_fill_grid() {
  if (!g_emoji_grid) return;
  lv_obj_clean(g_emoji_grid);
  if (g_emoji_picker_cat < 0 || g_emoji_picker_cat >= kEmojiCategoryCount) g_emoji_picker_cat = 0;
  const EmojiCategory & cat = kEmojiCategories[g_emoji_picker_cat];
  for (int i = 0; i < cat.count; ++i) {
    lv_obj_t * b = lv_button_create(g_emoji_grid);
    lv_obj_set_size(b, 58, 48);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 2, 0);
    lv_obj_t * img = make_emoji_image(b, cat.emojis[i], 34);
    lv_obj_center(img);
    /* pack category in high bits, index in low */
    const intptr_t ud = ((intptr_t)g_emoji_picker_cat << 16) | i;
    lv_obj_add_event_cb(
        b,
        [](lv_event_t * e) {
          const intptr_t ud = (intptr_t)lv_event_get_user_data(e);
          const int ci = (int)(ud >> 16);
          const int ei = (int)(ud & 0xffff);
          if (ci < 0 || ci >= kEmojiCategoryCount) return;
          const EmojiCategory & cat = kEmojiCategories[ci];
          if (ei < 0 || ei >= cat.count) return;
          const char * emo = cat.emojis[ei];
          if (g_emoji_picker_slot == kEmojiPickerCompose) {
            compose_set_emoji(emo);
            go_compose_refresh();
            return;
          }
          if (g_emoji_picker_slot >= 0 && g_emoji_picker_slot < app::kEmojiSlots) {
            std::snprintf(app::desk().emojis[g_emoji_picker_slot],
                          sizeof(app::desk().emojis[0]), "%s", emo);
            app::save();
          }
          go_settings();
        },
        LV_EVENT_CLICKED, (void *)ud);
  }
  lv_obj_scroll_to_y(g_emoji_grid, 0, LV_ANIM_OFF);
}

void emoji_picker_style_cats() {
  for (int i = 0; i < kEmojiCategoryCount; ++i) {
    lv_obj_t * b = g_emoji_cat_btns[i];
    if (!b) continue;
    const bool on = i == g_emoji_picker_cat;
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_width(b, 0, 0);
    /* Gold underline marker for the active category tab. */
    lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(b, on ? 2 : 0, 0);
    lv_obj_set_style_border_color(b, theme::gold(), 0);
    lv_obj_set_style_pad_bottom(b, on ? 2 : 4, 0);
    if (lv_obj_get_child_count(b) >= 2) {
      lv_obj_t * img = lv_obj_get_child(b, 0);
      lv_obj_t * lbl = lv_obj_get_child(b, 1);
      lv_obj_set_style_opa(img, on ? LV_OPA_COVER : LV_OPA_60, 0);
      lv_obj_set_style_text_color(lbl, on ? theme::gold() : theme::muted(), 0);
      lv_obj_set_style_text_opa(lbl, on ? LV_OPA_COVER : LV_OPA_70, 0);
    }
  }
}

void on_emoji_cat(lv_event_t * e) {
  const int ci = (int)(intptr_t)lv_event_get_user_data(e);
  if (ci < 0 || ci >= kEmojiCategoryCount) return;
  if (ci == g_emoji_picker_cat) return;
  g_emoji_picker_cat = ci;
  emoji_picker_style_cats();
  emoji_picker_fill_grid();
}

lv_obj_t * emoji_picker_screen(int slot) {
  const bool compose_mode = slot == kEmojiPickerCompose;
  if (!compose_mode && (slot < 0 || slot >= app::kEmojiSlots)) slot = 0;
  g_emoji_picker_slot = compose_mode ? kEmojiPickerCompose : slot;
  g_emoji_picker_cat = 0;
  g_emoji_grid = nullptr;
  for (int i = 0; i < kEmojiCategoryCount; ++i) g_emoji_cat_btns[i] = nullptr;

  lv_obj_t * scr = make_screen();
  make_topbar(scr, compose_mode ? "WERK ROOM" : "SETTINGS", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  make_tagline(body, compose_mode ? "Pick an emoji for this ping" : "Pick an emoji");

  /* Category tab bar — muted icon + label, not emoji tiles. */
  lv_obj_t * cats = lv_obj_create(body);
  lv_obj_remove_style_all(cats);
  lv_obj_set_width(cats, lv_pct(100));
  lv_obj_set_height(cats, 54);
  lv_obj_set_flex_flow(cats, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cats, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(cats, 2, 0);
  lv_obj_set_style_pad_bottom(cats, 2, 0);
  lv_obj_set_style_border_side(cats, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(cats, 1, 0);
  lv_obj_set_style_border_color(cats, theme::border(), 0);
  lv_obj_add_flag(cats, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(cats, LV_DIR_HOR);
  lv_obj_remove_flag(cats, LV_OBJ_FLAG_SCROLL_ELASTIC);

  for (int i = 0; i < kEmojiCategoryCount; ++i) {
    lv_obj_t * b = lv_button_create(cats);
    lv_obj_set_size(b, 56, 50);
    lv_obj_set_style_radius(b, 0, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_style_pad_row(b, 1, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * img = make_emoji_image(b, kEmojiCategories[i].icon, 22);
    lv_obj_set_style_opa(img, LV_OPA_60, 0);

    lv_obj_t * lbl = lv_label_create(b);
    lv_label_set_text(lbl, kEmojiCategories[i].id);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, theme::muted(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl, 54);

    lv_obj_add_event_cb(b, on_emoji_cat, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    g_emoji_cat_btns[i] = b;
  }
  emoji_picker_style_cats();

  g_emoji_grid = lv_obj_create(body);
  lv_obj_remove_style_all(g_emoji_grid);
  lv_obj_set_width(g_emoji_grid, lv_pct(100));
  lv_obj_set_flex_grow(g_emoji_grid, 1);
  lv_obj_set_flex_flow(g_emoji_grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(g_emoji_grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(g_emoji_grid, 6, 0);
  lv_obj_set_style_pad_column(g_emoji_grid, 6, 0);
  lv_obj_set_style_pad_top(g_emoji_grid, 8, 0);
  lv_obj_add_flag(g_emoji_grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g_emoji_grid, LV_DIR_VER);
  emoji_picker_fill_grid();

  lv_obj_t * dock = make_dock(scr);
  if (compose_mode) {
    dock_btn(dock, "Cancel", false, false, [](lv_event_t * /*e*/) { go_compose_refresh(); });
  } else {
    dock_btn(dock, "Cancel", false, false, [](lv_event_t * /*e*/) { go_settings(); });
  }
  return scr;
}

}  // namespace ui
}  // namespace wp

