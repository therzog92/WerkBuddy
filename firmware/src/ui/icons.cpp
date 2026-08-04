#include "ui/icons.h"

#include "ui/theme.h"

#include <cmath>

namespace wp {
namespace ui {
namespace {

/** Decorative children must not steal clicks from the app-icon wrapper. */
void make_click_through(lv_obj_t * obj) {
  if (!obj) return;
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  const uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < n; ++i) {
    make_click_through(lv_obj_get_child(obj, i));
  }
}

struct IconColors {
  uint32_t top;
  uint32_t bot;
};

IconColors colors_for(AppIcon icon) {
  switch (icon) {
    case AppIcon::Werk: return {0xff4fa3, 0xf0c24b};
    case AppIcon::Games: return {0x5a6a88, 0x3d4a66};
    case AppIcon::Doodle: return {0xe87858, 0x9a7ad4};
    case AppIcon::Settings: return {0x6a7280, 0x4a5568};
    case AppIcon::Ttt: return {0x5b8cff, 0x7c5cff};
    case AppIcon::C4: return {0x1d4ed8, 0x38bdf8};
    case AppIcon::Battleship: return {0x0f766e, 0x115e59};
    case AppIcon::Checkers: return {0x8b3a3a, 0x2a2438};
    case AppIcon::Memory: return {0xc45a9a, 0xe8c46a};
  }
  return {0x666666, 0x333333};
}

lv_obj_t * line(lv_obj_t * parent, lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2,
                lv_coord_t w, lv_color_t col) {
  /* Points are relative to the line object — size it to the full glyph. */
  auto * p = static_cast<lv_point_precise_t *>(lv_malloc(sizeof(lv_point_precise_t) * 2));
  p[0].x = x1;
  p[0].y = y1;
  p[1].x = x2;
  p[1].y = y2;
  lv_obj_t * l = lv_line_create(parent);
  lv_obj_set_size(l, 72, 72);
  lv_obj_set_pos(l, 0, 0);
  lv_line_set_points(l, p, 2);
  lv_obj_set_style_line_width(l, w, 0);
  lv_obj_set_style_line_color(l, col, 0);
  lv_obj_set_style_line_rounded(l, true, 0);
  lv_obj_set_user_data(l, p);
  return l;
}

lv_obj_t * rect_bar(lv_obj_t * parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                    lv_color_t col) {
  lv_obj_t * r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, w, h);
  lv_obj_set_pos(r, x, y);
  lv_obj_set_style_radius(r, 1, 0);
  lv_obj_set_style_bg_color(r, col, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  return r;
}

void draw_werk(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  /* Wi-Fi arcs via arcs */
  lv_obj_t * a1 = lv_arc_create(g);
  lv_obj_set_size(a1, 40, 40);
  lv_obj_center(a1);
  lv_arc_set_bg_angles(a1, 200, 340);
  lv_arc_set_value(a1, 0);
  lv_obj_remove_style(a1, nullptr, LV_PART_KNOB);
  lv_obj_set_style_arc_width(a1, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a1, c, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a1, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a1, LV_OPA_TRANSP, LV_PART_INDICATOR);

  lv_obj_t * a2 = lv_arc_create(g);
  lv_obj_set_size(a2, 26, 26);
  lv_obj_align(a2, LV_ALIGN_CENTER, 0, 2);
  lv_arc_set_bg_angles(a2, 210, 330);
  lv_arc_set_value(a2, 0);
  lv_obj_remove_style(a2, nullptr, LV_PART_KNOB);
  lv_obj_set_style_arc_width(a2, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a2, c, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a2, LV_OPA_TRANSP, LV_PART_INDICATOR);

  lv_obj_t * dot = lv_obj_create(g);
  lv_obj_remove_style_all(dot);
  lv_obj_set_size(dot, 6, 6);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, c, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_align(dot, LV_ALIGN_CENTER, 0, 12);

  line(g, 36, 14, 36, 20, 3, c);
  line(g, 22, 18, 26, 22, 3, c);
  line(g, 50, 18, 46, 22, 3, c);
}

void draw_games(lv_obj_t * g) {
  const uint32_t tiles[4] = {0x5b8cff, 0xe8899c, 0xc45a5a, 0xe8c46a};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t * t = lv_obj_create(g);
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, 20, 20);
    lv_obj_set_style_radius(t, 6, 0);
    lv_obj_set_style_bg_color(t, lv_color_hex(tiles[i]), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_pos(t, 14 + (i % 2) * 24, 14 + (i / 2) * 24);
  }
}

void draw_doodle(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  /* Pencil body (no transforms — keeps layout/hit targets clean) */
  lv_obj_t * body = lv_obj_create(g);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, 34, 12);
  lv_obj_set_style_radius(body, 3, 0);
  lv_obj_set_style_bg_color(body, c, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_pos(body, 22, 28);

  lv_obj_t * eraser = lv_obj_create(g);
  lv_obj_remove_style_all(eraser);
  lv_obj_set_size(eraser, 8, 12);
  lv_obj_set_style_radius(eraser, 2, 0);
  lv_obj_set_style_bg_color(eraser, c, 0);
  lv_obj_set_style_bg_opa(eraser, LV_OPA_50, 0);
  lv_obj_set_pos(eraser, 16, 28);

  /* Tip triangle approximated with a small diamond */
  lv_obj_t * tip = lv_obj_create(g);
  lv_obj_remove_style_all(tip);
  lv_obj_set_size(tip, 10, 10);
  lv_obj_set_style_radius(tip, 2, 0);
  lv_obj_set_style_bg_color(tip, c, 0);
  lv_obj_set_style_bg_opa(tip, LV_OPA_COVER, 0);
  lv_obj_set_pos(tip, 52, 29);

  /* Ink trail under the tip */
  line(g, 18, 48, 30, 54, 3, c);
  line(g, 30, 54, 48, 50, 3, c);
  line(g, 48, 50, 58, 56, 3, c);
}

void draw_settings(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  constexpr lv_coord_t kCx = 36, kCy = 36;
  /* Six stubby teeth tucked into the rim — reads as a cog, not a sun. */
  for (int i = 0; i < 6; ++i) {
    const float ang = i * 60.0f * 3.14159f / 180.0f;
    const lv_coord_t tx = kCx + (lv_coord_t)(16 * cosf(ang)) - 4;
    const lv_coord_t ty = kCy + (lv_coord_t)(16 * sinf(ang)) - 4;
    lv_obj_t * tooth = lv_obj_create(g);
    lv_obj_remove_style_all(tooth);
    lv_obj_set_size(tooth, 8, 8);
    lv_obj_set_style_radius(tooth, 2, 0);
    lv_obj_set_style_bg_color(tooth, c, 0);
    lv_obj_set_style_bg_opa(tooth, LV_OPA_COVER, 0);
    lv_obj_set_pos(tooth, tx, ty);
  }
  lv_obj_t * disc = lv_obj_create(g);
  lv_obj_remove_style_all(disc);
  lv_obj_set_size(disc, 30, 30);
  lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(disc, c, 0);
  lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
  lv_obj_center(disc);
  lv_obj_t * hole = lv_obj_create(g);
  lv_obj_remove_style_all(hole);
  lv_obj_set_size(hole, 12, 12);
  lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, 0);
  /* Punch the hub with the glyph's average mid-tone via transparent + parent show-through:
   * use a dark inset so it reads as a hole on any accent. */
  lv_obj_set_style_bg_color(hole, lv_color_hex(0x2a2438), 0);
  lv_obj_set_style_bg_opa(hole, LV_OPA_COVER, 0);
  lv_obj_center(hole);
}

void draw_ttt(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  constexpr lv_coord_t kIn = 13;
  constexpr lv_coord_t kCell = 14;
  constexpr lv_coord_t kGap = 2;
  constexpr lv_coord_t kSpan = 3 * kCell + 2 * kGap;
  /* Vertical / horizontal grid bars */
  rect_bar(g, kIn + kCell, kIn, kGap, kSpan, c);
  rect_bar(g, kIn + 2 * kCell + kGap, kIn, kGap, kSpan, c);
  rect_bar(g, kIn, kIn + kCell, kSpan, kGap, c);
  rect_bar(g, kIn, kIn + 2 * kCell + kGap, kSpan, kGap, c);

  auto mark = [&](const char * t, int cx, int cy) {
    lv_obj_t * cell = lv_obj_create(g);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, kCell, kCell);
    lv_obj_set_pos(cell, kIn + cx * (kCell + kGap), kIn + cy * (kCell + kGap));
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    lv_obj_t * lbl = lv_label_create(cell);
    lv_label_set_text(lbl, t);
    lv_obj_set_style_text_color(lbl, c, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);
  };
  mark("X", 0, 0);
  mark("O", 2, 2);
}

void draw_c4(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  constexpr lv_coord_t kCols = 4, kRows = 3;
  constexpr lv_coord_t kDot = 8;
  constexpr lv_coord_t kPitch = 11;
  constexpr lv_coord_t kBoardW = kCols * kPitch - 3;
  constexpr lv_coord_t kBoardH = kRows * kPitch - 3;
  const lv_coord_t ox = (72 - kBoardW) / 2;
  const lv_coord_t oy = (72 - kBoardH) / 2 + 2;

  lv_obj_t * frame = lv_obj_create(g);
  lv_obj_remove_style_all(frame);
  lv_obj_set_size(frame, kBoardW + 8, kBoardH + 8);
  lv_obj_set_pos(frame, ox - 4, oy - 4);
  lv_obj_set_style_radius(frame, 6, 0);
  lv_obj_set_style_border_width(frame, 2, 0);
  lv_obj_set_style_border_color(frame, c, 0);
  lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);

  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      /* Filled stack in col 1 (2 discs) and col 2 (1 disc) — reads as C4 */
      const bool filled = (col == 1 && row >= 1) || (col == 2 && row == 2);
      lv_obj_t * d = lv_obj_create(g);
      lv_obj_remove_style_all(d);
      lv_obj_set_size(d, kDot, kDot);
      lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_pos(d, ox + col * kPitch, oy + row * kPitch);
      if (filled) {
        lv_obj_set_style_bg_color(d, c, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
      } else {
        lv_obj_set_style_border_width(d, 2, 0);
        lv_obj_set_style_border_color(d, c, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_TRANSP, 0);
      }
    }
  }
}

void draw_bs(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  line(g, 14, 44, 58, 44, 3, c);
  line(g, 14, 44, 20, 56, 3, c);
  line(g, 58, 44, 52, 56, 3, c);
  line(g, 20, 56, 52, 56, 3, c);
  line(g, 22, 44, 22, 28, 3, c);
  line(g, 22, 28, 36, 18, 3, c);
  line(g, 36, 18, 50, 28, 3, c);
  line(g, 50, 28, 50, 44, 3, c);
}

void draw_ck(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  lv_obj_t * board = lv_obj_create(g);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 44, 44);
  lv_obj_set_style_radius(board, 4, 0);
  lv_obj_set_style_border_width(board, 2, 0);
  lv_obj_set_style_border_color(board, c, 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_TRANSP, 0);
  lv_obj_center(board);
  for (int i = 1; i <= 3; ++i) {
    line(g, 14, 14 + i * 11, 58, 14 + i * 11, 1, c);
    line(g, 14 + i * 11, 14, 14 + i * 11, 58, 1, c);
  }
  lv_obj_t * p1 = lv_obj_create(g);
  lv_obj_remove_style_all(p1);
  lv_obj_set_size(p1, 10, 10);
  lv_obj_set_style_radius(p1, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(p1, c, 0);
  lv_obj_set_style_bg_opa(p1, LV_OPA_COVER, 0);
  lv_obj_set_pos(p1, 20, 20);
  lv_obj_t * p2 = lv_obj_create(g);
  lv_obj_remove_style_all(p2);
  lv_obj_set_size(p2, 10, 10);
  lv_obj_set_style_radius(p2, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(p2, c, 0);
  lv_obj_set_style_bg_opa(p2, LV_OPA_COVER, 0);
  lv_obj_set_pos(p2, 42, 42);
}

void draw_mem(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  lv_obj_t * a = lv_obj_create(g);
  lv_obj_remove_style_all(a);
  lv_obj_set_size(a, 22, 28);
  lv_obj_set_style_radius(a, 4, 0);
  lv_obj_set_style_border_width(a, 2, 0);
  lv_obj_set_style_border_color(a, c, 0);
  lv_obj_set_style_bg_opa(a, LV_OPA_30, 0);
  lv_obj_set_style_bg_color(a, c, 0);
  lv_obj_set_pos(a, 18, 16);
  lv_obj_t * b = lv_obj_create(g);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, 22, 28);
  lv_obj_set_style_radius(b, 4, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, c, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_20, 0);
  lv_obj_set_style_bg_color(b, c, 0);
  lv_obj_set_pos(b, 32, 22);
}

void draw_icon(lv_obj_t * g, AppIcon icon) {
  switch (icon) {
    case AppIcon::Werk: draw_werk(g); break;
    case AppIcon::Games: draw_games(g); break;
    case AppIcon::Doodle: draw_doodle(g); break;
    case AppIcon::Settings: draw_settings(g); break;
    case AppIcon::Ttt: draw_ttt(g); break;
    case AppIcon::C4: draw_c4(g); break;
    case AppIcon::Battleship: draw_bs(g); break;
    case AppIcon::Checkers: draw_ck(g); break;
    case AppIcon::Memory: draw_mem(g); break;
  }
}

}  // namespace

lv_obj_t * make_app_glyph(lv_obj_t * parent, AppIcon icon) {
  const IconColors col = colors_for(icon);
  lv_obj_t * glyph = lv_obj_create(parent);
  lv_obj_remove_style_all(glyph);
  lv_obj_set_size(glyph, 72, 72);
  lv_obj_set_style_radius(glyph, 20, 0);
  lv_obj_set_style_bg_color(glyph, lv_color_hex(col.top), 0);
  lv_obj_set_style_bg_grad_color(glyph, lv_color_hex(col.bot), 0);
  lv_obj_set_style_bg_grad_dir(glyph, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(glyph, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(glyph, 18, 0);
  lv_obj_set_style_shadow_color(glyph, lv_color_hex(col.bot), 0);
  lv_obj_set_style_shadow_opa(glyph, LV_OPA_50, 0);
  lv_obj_set_style_shadow_offset_y(glyph, 6, 0);
  lv_obj_remove_flag(glyph, LV_OBJ_FLAG_SCROLLABLE);
  draw_icon(glyph, icon);
  make_click_through(glyph);
  return glyph;
}

lv_obj_t * make_app_icon(lv_obj_t * parent, AppIcon icon, const char * label, lv_event_cb_t cb,
                         void * user_data) {
  lv_obj_t * col = lv_obj_create(parent);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, 130, 104);
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 8, 0);
  lv_obj_add_flag(col, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  if (cb) lv_obj_add_event_cb(col, cb, LV_EVENT_CLICKED, user_data);

  make_app_glyph(col, icon);

  lv_obj_t * lbl = lv_label_create(col);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_color(lbl, theme::ink(), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_opa(lbl, LV_OPA_90, 0);
  lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
  return col;
}

}  // namespace ui
}  // namespace wp
