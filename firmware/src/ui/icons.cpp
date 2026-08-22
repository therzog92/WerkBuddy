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

uint32_t u32(lv_color_t c) { return lv_color_to_u32(c); }

IconColors colors_for(AppIcon icon) {
  /* Match web .app-glyph-* linear-gradient(145deg, …) endpoints. */
  switch (icon) {
    case AppIcon::Werk: return {u32(theme::hot()), u32(theme::gold())};
    case AppIcon::Games: return {0x5b8cff, 0x7c5cff}; /* blue → violet */
    case AppIcon::Doodle: return {0xe87858, 0x9a7ad4};
    case AppIcon::Timer: return {u32(theme::mint()), u32(theme::gold())};
    case AppIcon::Settings: return {0xa78bfa, 0x4f46e5}; /* lilac → indigo */
    case AppIcon::Utilities: return {u32(theme::mint()), u32(theme::hot())};
    case AppIcon::Checklist: return {u32(theme::gold()), u32(theme::mint())};
    case AppIcon::Calculator: return {0x38bdf8, 0x2563eb}; /* sky → blue */
    case AppIcon::Ttt: return {0x5b8cff, 0x7c5cff};
    case AppIcon::Sttt:
      return {u32(theme::hot()), u32(lv_color_mix(theme::hot(), theme::mint(), 128))};
    case AppIcon::C4: return {0x1d4ed8, 0x38bdf8};
    case AppIcon::Battleship: return {0x0f766e, 0x115e59};
    case AppIcon::Checkers: return {0x8b3a3a, 0x2a2438};
    case AppIcon::Memory: return {0xc45a9a, 0xe8c46a};
    case AppIcon::Reversi: return {0x1a5c3a, 0x0f3d26};
    case AppIcon::Dots: return {0x7a5cff, 0x4a2fbf};
    case AppIcon::Scoreboard: return {u32(theme::gold()), 0x8a6a1a};
    case AppIcon::G2048: return {0xf2b179, 0xedc22e};
    case AppIcon::Wordle: return {0xffffff, 0xe0e0e0};
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
  const lv_color_t c = lv_color_hex(0xffffff);
  const lv_color_t ink = lv_color_hex(0x3d4a66);
  /* Gamepad body — controls are children so they stay inside */
  lv_obj_t * body = lv_obj_create(g);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, 48, 26);
  lv_obj_set_style_radius(body, 11, 0);
  lv_obj_set_style_bg_color(body, c, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(body, true, 0);
  lv_obj_align(body, LV_ALIGN_CENTER, 0, 1);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  auto pad_btn = [&](lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, bool round) {
    lv_obj_t * t = lv_obj_create(body);
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, w, h);
    lv_obj_set_pos(t, x, y);
    lv_obj_set_style_radius(t, round ? LV_RADIUS_CIRCLE : 2, 0);
    lv_obj_set_style_bg_color(t, ink, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    return t;
  };
  /* D-pad */
  pad_btn(8, 11, 12, 4, false);
  pad_btn(12, 7, 4, 12, false);
  /* Face buttons */
  pad_btn(30, 7, 6, 6, true);
  pad_btn(37, 12, 6, 6, true);
}

void draw_doodle(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  /*
   * Diagonal paintbrush (tip bottom-left → handle top-right), drawn with
   * primitives so it survives hub rebuilds (file-based lv_image was dropping
   * when the previous screen deleted async and flushed the decoder cache).
   */
  auto blob = [&](lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t * o = lv_obj_create(g);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  };
  /* Brush head */
  blob(10, 40, 22, 18);
  blob(8, 48, 12, 10);
  /* Ferrule (short mid segment, slight gap from tip) */
  line(g, 28, 40, 38, 30, 9, c);
  /* Handle tapering to a point */
  line(g, 40, 28, 54, 16, 7, c);
  line(g, 52, 17, 60, 11, 4, c);
}

void draw_timer(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  /* Clock face */
  lv_obj_t * ring = lv_obj_create(g);
  lv_obj_remove_style_all(ring);
  lv_obj_set_size(ring, 40, 40);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(ring, 4, 0);
  lv_obj_set_style_border_color(ring, c, 0);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
  lv_obj_set_pos(ring, 16, 18);
  /* Crown */
  rect_bar(g, 32, 10, 8, 6, c);
  /* Hands */
  line(g, 36, 38, 36, 26, 3, c);
  line(g, 36, 38, 48, 38, 3, c);
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

void draw_utilities(lv_obj_t * g) {
  /* Folder-ish: like games but with wrench-ish bars */
  const lv_color_t c = lv_color_hex(0xffffff);
  lv_obj_t * body = lv_obj_create(g);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, 44, 30);
  lv_obj_set_style_radius(body, 8, 0);
  lv_obj_set_style_bg_color(body, c, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_pos(body, 14, 24);
  lv_obj_t * tab = lv_obj_create(g);
  lv_obj_remove_style_all(tab);
  lv_obj_set_size(tab, 18, 10);
  lv_obj_set_style_radius(tab, 4, 0);
  lv_obj_set_style_bg_color(tab, c, 0);
  lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
  lv_obj_set_pos(tab, 14, 16);
  rect_bar(g, 22, 32, 28, 3, lv_color_hex(0x3d4a66));
  rect_bar(g, 22, 40, 20, 3, lv_color_hex(0x3d4a66));
}

void draw_checklist(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  for (int i = 0; i < 3; ++i) {
    const lv_coord_t y = 18 + i * 14;
    lv_obj_t * box = lv_obj_create(g);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 12, 12);
    lv_obj_set_style_radius(box, 3, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, c, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(box, 16, y);
    if (i < 2) {
      lv_obj_set_style_bg_color(box, c, 0);
      lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    }
    rect_bar(g, 34, y + 4, 22, 3, c);
  }
}

void draw_calculator(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  lv_obj_t * frame = lv_obj_create(g);
  lv_obj_remove_style_all(frame);
  lv_obj_set_size(frame, 40, 48);
  lv_obj_set_style_radius(frame, 6, 0);
  lv_obj_set_style_border_width(frame, 2, 0);
  lv_obj_set_style_border_color(frame, c, 0);
  lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
  lv_obj_center(frame);
  rect_bar(g, 22, 18, 28, 8, c);
  for (int r = 0; r < 3; ++r)
    for (int col = 0; col < 3; ++col)
      rect_bar(g, 22 + col * 10, 30 + r * 8, 7, 5, c);
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

void draw_sttt(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  constexpr lv_coord_t kIn = 12;
  constexpr lv_coord_t kMini = 14;
  constexpr lv_coord_t kGap = 3;
  constexpr lv_coord_t kSpan = 3 * kMini + 2 * kGap;
  rect_bar(g, kIn + kMini, kIn, kGap, kSpan, c);
  rect_bar(g, kIn + 2 * kMini + kGap, kIn, kGap, kSpan, c);
  rect_bar(g, kIn, kIn + kMini, kSpan, kGap, c);
  rect_bar(g, kIn, kIn + 2 * kMini + kGap, kSpan, kGap, c);
  for (int i = 0; i < 9; ++i) {
    const int bx = i % 3, by = i / 3;
    const lv_coord_t ox = kIn + bx * (kMini + kGap) + 2;
    const lv_coord_t oy = kIn + by * (kMini + kGap) + 2;
    rect_bar(g, ox + 3, oy, 1, 9, c);
    rect_bar(g, ox + 7, oy, 1, 9, c);
    rect_bar(g, ox, oy + 3, 9, 1, c);
    rect_bar(g, ox, oy + 7, 9, 1, c);
  }
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

void draw_rv(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  lv_obj_t * board = lv_obj_create(g);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 44, 44);
  lv_obj_set_style_radius(board, 4, 0);
  lv_obj_set_style_border_width(board, 2, 0);
  lv_obj_set_style_border_color(board, c, 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_TRANSP, 0);
  lv_obj_center(board);
  auto disc = [&](lv_coord_t x, lv_coord_t y, bool filled) {
    lv_obj_t * d = lv_obj_create(g);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, 12, 12);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(d, x, y);
    if (filled) {
      lv_obj_set_style_bg_color(d, c, 0);
      lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    } else {
      lv_obj_set_style_border_width(d, 2, 0);
      lv_obj_set_style_border_color(d, c, 0);
      lv_obj_set_style_bg_opa(d, LV_OPA_TRANSP, 0);
    }
  };
  disc(23, 23, true);
  disc(37, 23, false);
  disc(23, 37, false);
  disc(37, 37, true);
}

void draw_db(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  constexpr int kN = 3;
  constexpr lv_coord_t kPitch = 16;
  constexpr lv_coord_t kOx = (72 - (kN - 1) * kPitch) / 2;
  constexpr lv_coord_t kOy = kOx;
  for (int r = 0; r < kN; ++r) {
    for (int col = 0; col < kN; ++col) {
      lv_obj_t * dot = lv_obj_create(g);
      lv_obj_remove_style_all(dot);
      lv_obj_set_size(dot, 6, 6);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, c, 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      lv_obj_set_pos(dot, kOx + col * kPitch - 3, kOy + r * kPitch - 3);
    }
  }
  /* A couple of claimed edges to read as "in progress" */
  line(g, kOx, kOy, kOx + kPitch, kOy, 3, c);
  line(g, kOx, kOy, kOx, kOy + kPitch, 3, c);
  line(g, kOx + kPitch, kOy, kOx + kPitch, kOy + kPitch, 3, c);
  lv_obj_t * box = lv_obj_create(g);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, kPitch - 6, kPitch - 6);
  lv_obj_set_pos(box, kOx + 3, kOy + 3);
  lv_obj_set_style_bg_color(box, c, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_30, 0);
  lv_obj_set_style_radius(box, 2, 0);
}

void draw_scoreboard(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  constexpr lv_coord_t kW = 10;
  const lv_coord_t heights[3] = {18, 30, 22};
  for (int i = 0; i < 3; ++i) {
    const lv_coord_t x = 16 + i * 15;
    const lv_coord_t h = heights[i];
    lv_obj_t * bar = lv_obj_create(g);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, kW, h);
    lv_obj_set_pos(bar, x, 50 - h);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_bg_color(bar, c, 0);
    lv_obj_set_style_bg_opa(bar, i == 1 ? LV_OPA_COVER : LV_OPA_60, 0);
  }
  line(g, 12, 50, 60, 50, 2, c);
}

void draw_g2048(lv_obj_t * g) {
  const lv_color_t c = lv_color_hex(0xffffff);
  constexpr lv_coord_t kIn = 14;
  constexpr lv_coord_t kCell = 10;
  constexpr lv_coord_t kGap = 3;
  for (int r = 0; r < 4; ++r) {
    for (int col = 0; col < 4; ++col) {
      rect_bar(g, kIn + col * (kCell + kGap), kIn + r * (kCell + kGap), kCell, kCell, c);
    }
  }
  lv_obj_t * lab = lv_label_create(g);
  lv_label_set_text(lab, "2048");
  lv_obj_set_style_text_color(lab, c, 0);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);
  lv_obj_align(lab, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void draw_wordle(lv_obj_t * g) {
  const lv_color_t green = lv_color_hex(0x538d4e);
  const lv_color_t gold = lv_color_hex(0xb59f3b);
  const lv_color_t gray = lv_color_hex(0x3a3a3c);
  constexpr lv_coord_t kInX = 14;
  constexpr lv_coord_t kInY = 12;
  constexpr lv_coord_t kCell = 12;
  constexpr lv_coord_t kGap = 3;

  /* 3x3 tile grid mimicking Wordle guesses */
  const lv_color_t cols[3][3] = {
    {gray, green, gray},
    {gold, gray, green},
    {green, green, green}
  };

  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      rect_bar(g, kInX + c * (kCell + kGap), kInY + r * (kCell + kGap), kCell, kCell, cols[r][c]);
    }
  }

}

void draw_icon(lv_obj_t * g, AppIcon icon) {
  switch (icon) {
    case AppIcon::Werk: draw_werk(g); break;
    case AppIcon::Games: draw_games(g); break;
    case AppIcon::Doodle: draw_doodle(g); break;
    case AppIcon::Timer: draw_timer(g); break;
    case AppIcon::Settings: draw_settings(g); break;
    case AppIcon::Utilities: draw_utilities(g); break;
    case AppIcon::Checklist: draw_checklist(g); break;
    case AppIcon::Calculator: draw_calculator(g); break;
    case AppIcon::Ttt: draw_ttt(g); break;
    case AppIcon::Sttt: draw_sttt(g); break;
    case AppIcon::C4: draw_c4(g); break;
    case AppIcon::Battleship: draw_bs(g); break;
    case AppIcon::Checkers: draw_ck(g); break;
    case AppIcon::Memory: draw_mem(g); break;
    case AppIcon::Reversi: draw_rv(g); break;
    case AppIcon::Dots: draw_db(g); break;
    case AppIcon::Scoreboard: draw_scoreboard(g); break;
    case AppIcon::G2048: draw_g2048(g); break;
    case AppIcon::Wordle: draw_wordle(g); break;
  }
}

}  // namespace

lv_obj_t * make_app_glyph(lv_obj_t * parent, AppIcon icon) {
  const IconColors col = colors_for(icon);
  lv_obj_t * glyph = lv_obj_create(parent);
  lv_obj_remove_style_all(glyph);
  lv_obj_set_size(glyph, 72, 72);
  /* Squircle tile + gradient + soft shadow — matches web .app-glyph */
  lv_obj_set_style_radius(glyph, 18, 0);
  lv_obj_set_style_bg_color(glyph, lv_color_hex(col.top), 0);
  lv_obj_set_style_bg_grad_color(glyph, lv_color_hex(col.bot), 0);
  lv_obj_set_style_bg_grad_dir(glyph, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(glyph, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(glyph, 16, 0);
  lv_obj_set_style_shadow_offset_y(glyph, 6, 0);
  lv_obj_set_style_shadow_color(glyph, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(glyph, LV_OPA_40, 0);
  lv_obj_set_style_border_width(glyph, 0, 0);
  lv_obj_set_style_clip_corner(glyph, true, 0);
  lv_obj_remove_flag(glyph, LV_OBJ_FLAG_SCROLLABLE);

  /* Soft top highlight (web inset 0 1px white) */
  lv_obj_t * sheen = lv_obj_create(glyph);
  lv_obj_remove_style_all(sheen);
  lv_obj_set_size(sheen, 72, 14);
  lv_obj_set_pos(sheen, 0, 0);
  lv_obj_set_style_radius(sheen, 18, 0);
  lv_obj_set_style_bg_color(sheen, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(sheen, LV_OPA_20, 0);
  lv_obj_set_style_bg_grad_color(sheen, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_grad_dir(sheen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_main_opa(sheen, LV_OPA_30, 0);
  lv_obj_set_style_bg_grad_opa(sheen, LV_OPA_TRANSP, 0);

  draw_icon(glyph, icon);
  make_click_through(glyph);
  return glyph;
}

lv_obj_t * make_app_icon(lv_obj_t * parent, AppIcon icon, const char * label, lv_event_cb_t cb,
                         void * user_data) {
  return make_app_icon_sized(parent, icon, label, cb, 72, 130, 104, &lv_font_montserrat_14,
                             user_data);
}

lv_obj_t * make_app_icon_sized(lv_obj_t * parent, AppIcon icon, const char * label,
                               lv_event_cb_t cb, int glyph_vis, int col_w, int col_h,
                               const lv_font_t * label_font, void * user_data) {
  if (glyph_vis < 24) glyph_vis = 24;
  if (col_w < glyph_vis) col_w = glyph_vis;
  if (col_h < glyph_vis + 16) col_h = glyph_vis + 20;

  lv_obj_t * col = lv_obj_create(parent);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, col_w, col_h);
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, glyph_vis >= 72 ? 8 : 4, 0);
  lv_obj_add_flag(col, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(col, 14);
  lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  if (cb) lv_obj_add_event_cb(col, cb, LV_EVENT_CLICKED, user_data);

  lv_obj_t * wrap = lv_obj_create(col);
  lv_obj_remove_style_all(wrap);
  lv_obj_set_size(wrap, glyph_vis, glyph_vis);
  lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(wrap, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_remove_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(wrap, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * glyph = make_app_glyph(wrap, icon);
  const int32_t scale = (glyph_vis * 256) / 72;
  lv_obj_set_style_transform_pivot_x(glyph, 36, 0);
  lv_obj_set_style_transform_pivot_y(glyph, 36, 0);
  lv_obj_set_style_transform_scale(glyph, scale, 0);
  lv_obj_center(glyph);
  lv_obj_remove_flag(glyph, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * lbl = lv_label_create(col);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_color(lbl, theme::ink(), 0);
  lv_obj_set_style_text_font(lbl, label_font ? label_font : &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_opa(lbl, LV_OPA_90, 0);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lbl, col_w);
  lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
  return col;
}

}  // namespace ui
}  // namespace wp
