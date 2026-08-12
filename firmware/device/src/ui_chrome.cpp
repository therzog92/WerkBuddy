#include "ui_chrome.h"

#include "desk.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace dui {
namespace {

struct Palette {
  uint32_t bg0, panel, ink, muted, gold, hot, mint, danger, border, grad_bot;
};

/* First four sim palettes (Eleganza / Runway / Ice / Lemon) — desk.theme 0..3. */
constexpr Palette kPal[] = {
    {0x0c0a0f, 0x22182f, 0xf7f2ea, 0xb9a8c9, 0xf0c24b, 0xff4fa3, 0x5dffc2, 0xff5c7a, 0x3d2a55,
     0x120c18},
    {0x0a0a0a, 0x241616, 0xfff5f0, 0xc9a8a8, 0xf0c24b, 0xff3b3b, 0xffb4a2, 0xff6b6b, 0x5a2a2a,
     0x100808},
    {0x061018, 0x122536, 0xe8f6ff, 0x8eb4c9, 0x7ad7ff, 0x3d8bfd, 0x5dffc2, 0xff6b8a, 0x1e3d55,
     0x081018},
    {0x12100a, 0x2a2418, 0xfff8e8, 0xc9b898, 0xffe566, 0xffb703, 0xb8f248, 0xff5c7a, 0x5a4a20,
     0x100c08},
};

const Palette & pal() { return kPal[shell::desk().theme % 4]; }

lv_obj_t * g_confirm = nullptr;

}  // namespace

lv_color_t bg0() { return lv_color_hex(pal().bg0); }
lv_color_t panel() { return lv_color_hex(pal().panel); }
lv_color_t ink() { return lv_color_hex(pal().ink); }
lv_color_t muted() { return lv_color_hex(pal().muted); }
lv_color_t gold() { return lv_color_hex(pal().gold); }
lv_color_t hot() { return lv_color_hex(pal().hot); }
lv_color_t mint() { return lv_color_hex(pal().mint); }
lv_color_t danger() { return lv_color_hex(pal().danger); }
lv_color_t border() { return lv_color_hex(pal().border); }
lv_color_t grad_bot() { return lv_color_hex(pal().grad_bot); }

lv_obj_t * make_screen() {
  lv_obj_t * scr = lv_obj_create(nullptr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, bg0(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  return scr;
}

lv_obj_t * make_topbar(lv_obj_t * scr, const char * title, const char * me, const char * sub) {
  lv_obj_t * top = lv_obj_create(scr);
  lv_obj_remove_style_all(top);
  lv_obj_set_size(top, kHor, kTopbarH);
  lv_obj_set_style_pad_hor(top, 14, 0);
  lv_obj_set_style_pad_top(top, 10, 0);
  lv_obj_set_style_pad_bottom(top, 6, 0);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t * left = lv_obj_create(top);
  lv_obj_remove_style_all(left);
  lv_obj_set_height(left, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(left, 1);
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * brand = lv_label_create(left);
  lv_label_set_text(brand, title);
  lv_obj_set_style_text_color(brand, gold(), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);

  if (sub && sub[0]) {
    lv_obj_t * sub_lbl = lv_label_create(left);
    lv_label_set_text(sub_lbl, sub);
    lv_obj_set_style_text_color(sub_lbl, muted(), 0);
    lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_12, 0);
  }

  lv_obj_t * me_lbl = lv_label_create(top);
  lv_label_set_text(me_lbl, me ? me : "");
  lv_obj_set_style_text_color(me_lbl, muted(), 0);
  lv_obj_set_style_text_font(me_lbl, &lv_font_montserrat_14, 0);
  return top;
}

lv_obj_t * make_body(lv_obj_t * scr, bool with_dock) {
  lv_obj_t * body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  const int h = kVer - kTopbarH - (with_dock ? kDockH : 0);
  lv_obj_set_size(body, kHor, h);
  lv_obj_set_pos(body, 0, kTopbarH);
  lv_obj_set_style_pad_hor(body, 14, 0);
  lv_obj_set_style_pad_top(body, 6, 0);
  lv_obj_set_style_pad_bottom(body, with_dock ? 16 : 6, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(body, 8, 0);
  lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(body, LV_DIR_VER);
  return body;
}

lv_obj_t * make_dock(lv_obj_t * scr) {
  lv_obj_t * dock = lv_obj_create(scr);
  lv_obj_remove_style_all(dock);
  lv_obj_set_size(dock, kHor, kDockH);
  lv_obj_align(dock, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_pad_hor(dock, 12, 0);
  lv_obj_set_style_pad_ver(dock, 6, 0);
  lv_obj_set_style_bg_color(dock, grad_bot(), 0);
  lv_obj_set_style_bg_opa(dock, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(dock, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(dock, 8, 0);
  lv_obj_remove_flag(dock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(dock);
  return dock;
}

lv_obj_t * dock_btn(lv_obj_t * dock, const char * label, bool primary, bool danger, lv_event_cb_t cb,
                    void * user_data) {
  lv_obj_t * btn = lv_button_create(dock);
  lv_obj_set_height(btn, 36);
  lv_obj_set_flex_grow(btn, 1);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  if (danger) lv_obj_set_style_bg_color(btn, dui::danger(), 0);
  else if (primary) lv_obj_set_style_bg_color(btn, gold(), 0);
  else lv_obj_set_style_bg_color(btn, lv_color_hex(0x4a4558), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

  lv_obj_t * lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_color(lbl, primary && !danger ? lv_color_hex(0x1a1224) : ink(), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);
  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  return btn;
}

lv_obj_t * make_status(lv_obj_t * parent, const char * text) {
  lv_obj_t * s = lv_label_create(parent);
  lv_label_set_text(s, text);
  lv_obj_set_style_text_color(s, mint(), 0);
  lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(s, lv_pct(100));
  return s;
}

lv_obj_t * make_tagline(lv_obj_t * parent, const char * text) {
  lv_obj_t * t = lv_label_create(parent);
  lv_label_set_text(t, text);
  lv_obj_set_style_text_color(t, muted(), 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
  lv_obj_set_width(t, lv_pct(100));
  return t;
}

lv_obj_t * make_wait_block(lv_obj_t * parent, const char * eye, const char * name, const char * sub) {
  lv_obj_t * box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_width(box, lv_pct(100));
  lv_obj_set_flex_grow(box, 1);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(box, 8, 0);

  lv_obj_t * e = lv_label_create(box);
  lv_label_set_text(e, eye);
  lv_obj_set_style_text_color(e, mint(), 0);
  lv_obj_set_style_text_font(e, &lv_font_montserrat_12, 0);

  lv_obj_t * n = lv_label_create(box);
  lv_label_set_text(n, name);
  lv_obj_set_style_text_color(n, gold(), 0);
  lv_obj_set_style_text_font(n, &lv_font_montserrat_28, 0);

  lv_obj_t * s = lv_label_create(box);
  lv_label_set_text(s, sub);
  lv_obj_set_style_text_color(s, muted(), 0);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  return box;
}

lv_obj_t * make_peer_btn(lv_obj_t * parent, const char * name, lv_event_cb_t cb, void * user_data) {
  lv_obj_t * btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_width(btn, lv_pct(100));
  lv_obj_set_height(btn, 56);
  lv_obj_set_style_radius(btn, 16, 0);
  lv_obj_set_style_bg_color(btn, gold(), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t * n = lv_label_create(btn);
  lv_label_set_text(n, name);
  lv_obj_set_style_text_color(n, lv_color_hex(0x1a0a12), 0);
  lv_obj_set_style_text_font(n, &lv_font_montserrat_20, 0);
  lv_obj_center(n);
  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  return btn;
}

void ttt_draw_mark(lv_obj_t * parent, char mark, int size) {
  const lv_color_t col = (mark == 'X') ? hot() : gold();
  lv_obj_t * wrap = lv_obj_create(parent);
  lv_obj_remove_style_all(wrap);
  lv_obj_set_size(wrap, size, size);
  lv_obj_center(wrap);
  lv_obj_remove_flag(wrap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);
  if (mark == 'O') {
    const int stroke = size >= 40 ? 7 : (size >= 24 ? 5 : 4);
    lv_obj_t * ring = lv_obj_create(wrap);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, size, size);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, stroke, 0);
    lv_obj_set_style_border_color(ring, col, 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
    lv_obj_center(ring);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    return;
  }
  const int stroke = size >= 40 ? 8 : (size >= 24 ? 5 : 4);
  const int inset = size / 6;
  auto add_diag = [&](lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2) {
    auto * pts = static_cast<lv_point_precise_t *>(lv_malloc(sizeof(lv_point_precise_t) * 2));
    pts[0].x = x1;
    pts[0].y = y1;
    pts[1].x = x2;
    pts[1].y = y2;
    lv_obj_t * ln = lv_line_create(wrap);
    lv_obj_set_size(ln, size, size);
    lv_obj_set_pos(ln, 0, 0);
    lv_line_set_points(ln, pts, 2);
    lv_obj_set_style_line_width(ln, stroke, 0);
    lv_obj_set_style_line_color(ln, col, 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
    lv_obj_set_user_data(ln, pts);
    lv_obj_remove_flag(ln, LV_OBJ_FLAG_CLICKABLE);
  };
  add_diag(inset, inset, size - inset, size - inset);
  add_diag(size - inset, inset, inset, size - inset);
}

void attach_result_overlay(lv_obj_t * parent, int outcome, lv_event_cb_t on_dismiss) {
  lv_obj_t * ov = lv_obj_create(parent);
  lv_obj_remove_style_all(ov);
  lv_obj_set_size(ov, lv_pct(100), lv_pct(100));
  lv_obj_add_flag(ov, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(ov, 170, 0);
  lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(ov, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t * card = lv_obj_create(ov);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, 280, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card, panel(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 20, 0);
  lv_obj_set_style_pad_all(card, 18, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_border_color(card, outcome > 0 ? gold() : (outcome < 0 ? mint() : hot()), 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, 8, 0);
  lv_obj_center(card);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * big = lv_label_create(card);
  lv_label_set_text(big, outcome > 0 ? "YOU WIN" : (outcome < 0 ? "DRAW" : "YOU LOSE"));
  lv_obj_set_style_text_color(big, gold(), 0);
  lv_obj_set_style_text_font(big, &lv_font_montserrat_28, 0);

  lv_obj_t * res = lv_label_create(card);
  lv_label_set_text(res, outcome > 0 ? "Condragulations!" : (outcome < 0 ? "It's a draw" : "Sashay away..."));
  lv_obj_set_style_text_color(res, ink(), 0);
  lv_obj_set_style_text_font(res, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_align(res, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * tap = lv_label_create(card);
  lv_label_set_text(tap, "tap to dismiss");
  lv_obj_set_style_text_color(tap, muted(), 0);
  lv_obj_set_style_text_font(tap, &lv_font_montserrat_12, 0);

  if (on_dismiss) lv_obj_add_event_cb(ov, on_dismiss, LV_EVENT_CLICKED, nullptr);
}

void show_forfeit_confirm(lv_event_cb_t on_yes) {
  if (g_confirm) {
    lv_obj_delete(g_confirm);
    g_confirm = nullptr;
  }
  g_confirm = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g_confirm);
  lv_obj_set_size(g_confirm, kHor, kVer);
  lv_obj_set_style_bg_color(g_confirm, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(g_confirm, 180, 0);
  lv_obj_add_flag(g_confirm, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * card = lv_obj_create(g_confirm);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, 320, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card, panel(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 20, 0);
  lv_obj_set_style_pad_all(card, 20, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, 14, 0);
  lv_obj_center(card);

  lv_obj_t * t = lv_label_create(card);
  lv_label_set_text(t, "Are you sure you\nwant to forfeit?");
  lv_obj_set_style_text_color(t, ink(), 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, lv_pct(100), 48);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t * no = lv_button_create(row);
  lv_obj_set_size(no, 120, 40);
  lv_obj_set_style_radius(no, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(no, lv_color_hex(0x4a4558), 0);
  lv_obj_add_event_cb(no, [](lv_event_t * /*e*/) {
    if (g_confirm) {
      lv_obj_delete(g_confirm);
      g_confirm = nullptr;
    }
  }, LV_EVENT_CLICKED, nullptr);
  lv_obj_t * nl = lv_label_create(no);
  lv_label_set_text(nl, "Keep playing");
  lv_obj_set_style_text_color(nl, ink(), 0);
  lv_obj_center(nl);

  lv_obj_t * yes = lv_button_create(row);
  lv_obj_set_size(yes, 120, 40);
  lv_obj_set_style_radius(yes, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(yes, danger(), 0);
  lv_obj_add_event_cb(yes, [](lv_event_t * e) {
    auto * cb = reinterpret_cast<lv_event_cb_t>(lv_event_get_user_data(e));
    if (g_confirm) {
      lv_obj_delete(g_confirm);
      g_confirm = nullptr;
    }
    if (cb) cb(e);
  }, LV_EVENT_CLICKED, (void *)on_yes);
  lv_obj_t * yl = lv_label_create(yes);
  lv_label_set_text(yl, "Forfeit");
  lv_obj_set_style_text_color(yl, ink(), 0);
  lv_obj_center(yl);
}

}  // namespace dui
}  // namespace wp
