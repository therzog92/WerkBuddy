#include "ui/scr_g2048.h"

#include "app/app.h"
#include "games/g2048.h"
#include "ui/chrome.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace ui {
namespace {

using games::g2048::Dir;
using games::g2048::kCells;
using games::g2048::kSize;

/* Classic 2048 palette (gabrielecirulli/2048). */
constexpr uint32_t kCream = 0xfaf8ef;
constexpr uint32_t kInk = 0x776e65;
constexpr uint32_t kInkLight = 0xbbada0;
constexpr uint32_t kBoardBg = 0xbbada0;
constexpr uint32_t kScoreBg = 0x3c3a32;
constexpr uint32_t kAccent = 0xf65e3b;
constexpr uint32_t kTitleGold = 0xedc22e;

struct Session {
  uint16_t board[kCells] = {};
  int score = 0;
  bool active = false;
  bool over = false;
  bool won_shown = false;
};

Session g_sess;

lv_color_t tile_color(uint16_t v) {
  switch (v) {
    case 0: return lv_color_hex(0xcdc1b4);
    case 2: return lv_color_hex(0xeee4da);
    case 4: return lv_color_hex(0xede0c8);
    case 8: return lv_color_hex(0xf2b179);
    case 16: return lv_color_hex(0xf59563);
    case 32: return lv_color_hex(0xf67c5f);
    case 64: return lv_color_hex(0xf65e3b);
    case 128: return lv_color_hex(0xedcf72);
    case 256: return lv_color_hex(0xedcc61);
    case 512: return lv_color_hex(0xedc850);
    case 1024: return lv_color_hex(0xedc53f);
    default: return lv_color_hex(0xedc22e);
  }
}

lv_color_t tile_ink(uint16_t v) {
  return (v <= 4) ? lv_color_hex(kInk) : lv_color_hex(0xf9f6f2);
}

void ensure_started() {
  if (g_sess.active) return;
  games::g2048::new_game(g_sess.board);
  g_sess.score = 0;
  g_sess.active = true;
  g_sess.over = false;
  g_sess.won_shown = false;
}

void note_high_score() {
  app::Desk & d = app::desk();
  if (g_sess.score > d.high_score_2048) {
    d.high_score_2048 = g_sess.score;
    app::save();
  }
}

void apply_dir(Dir dir) {
  ensure_started();
  if (g_sess.over) return;
  int delta = 0;
  if (!games::g2048::move(g_sess.board, dir, delta)) return;
  g_sess.score += delta;
  note_high_score();
  games::g2048::spawn(g_sess.board);
  if (games::g2048::has_tile(g_sess.board, 2048) && !g_sess.won_shown) {
    g_sess.won_shown = true;
    toast("You made 2048 — keep going!");
  }
  if (!games::g2048::can_move(g_sess.board)) {
    g_sess.over = true;
    note_high_score();
  }
  go_g2048();
}

void reset_session() {
  games::g2048::new_game(g_sess.board);
  g_sess.score = 0;
  g_sess.active = true;
  g_sess.over = false;
  g_sess.won_shown = false;
}

void on_forfeit_yes(lv_event_t * /*e*/) {
  note_high_score();
  g_sess = Session{};
  go_hub();
}

void on_dir(lv_event_t * e) {
  apply_dir(static_cast<Dir>((int)(intptr_t)lv_event_get_user_data(e)));
}

void on_gesture(lv_event_t * /*e*/) {
  lv_indev_t * indev = lv_indev_active();
  if (!indev) return;
  const lv_dir_t g = lv_indev_get_gesture_dir(indev);
  lv_indev_wait_release(indev);
  if (g & LV_DIR_LEFT) apply_dir(Dir::Left);
  else if (g & LV_DIR_RIGHT) apply_dir(Dir::Right);
  else if (g & LV_DIR_TOP) apply_dir(Dir::Up);
  else if (g & LV_DIR_BOTTOM) apply_dir(Dir::Down);
}

void format_score(char * buf, size_t n, int value) {
  if (value >= 100000) lv_snprintf(buf, (uint32_t)n, "%.1fk", value / 1000.0);
  else if (value >= 10000) lv_snprintf(buf, (uint32_t)n, "%.1fk", value / 1000.0);
  else lv_snprintf(buf, (uint32_t)n, "%d", value);
}

lv_obj_t * make_score_chip(lv_obj_t * parent, const char * title, int value) {
  lv_obj_t * chip = lv_obj_create(parent);
  lv_obj_remove_style_all(chip);
  lv_obj_set_size(chip, 80, 56);
  lv_obj_set_style_radius(chip, 6, 0);
  lv_obj_set_style_bg_color(chip, lv_color_hex(kScoreBg), 0);
  lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(chip, 0, 0);
  lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * t = lv_label_create(chip);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_color(t, lv_color_hex(0xeee4da), 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);

  lv_obj_t * v = lv_label_create(chip);
  char buf[16];
  format_score(buf, sizeof(buf), value);
  lv_label_set_text(v, buf);
  lv_obj_set_style_text_color(v, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_16, 0);
  return chip;
}

lv_obj_t * accent_btn(lv_obj_t * parent, const char * label, lv_event_cb_t cb, int w, int h) {
  lv_obj_t * b = lv_button_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_style_min_width(b, 0, 0);
  lv_obj_set_style_min_height(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, 8, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(kAccent), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_t * lab = lv_label_create(b);
  lv_label_set_text(lab, label);
  lv_obj_set_style_text_color(lab, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
  lv_obj_center(lab);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

lv_obj_t * dir_btn(lv_obj_t * parent, const char * label, Dir d) {
  lv_obj_t * b = lv_button_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, 64, 36);
  lv_obj_set_style_radius(b, 8, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(kInkLight), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_t * lab = lv_label_create(b);
  lv_label_set_text(lab, label);
  lv_obj_set_style_text_color(lab, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
  lv_obj_center(lab);
  lv_obj_add_event_cb(b, on_dir, LV_EVENT_CLICKED, (void *)(intptr_t)d);
  return b;
}

void fill_board(lv_obj_t * parent) {
  ensure_started();

  /* Single header row: [2048][NEW] …… [SCORE][BEST] */
  lv_obj_t * head = lv_obj_create(parent);
  lv_obj_remove_style_all(head);
  lv_obj_set_size(head, 360, 56);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * left = lv_obj_create(head);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, LV_SIZE_CONTENT, 56);
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(left, 8, 0);

  lv_obj_t * title = lv_obj_create(left);
  lv_obj_remove_style_all(title);
  lv_obj_set_size(title, 72, 56);
  lv_obj_set_style_radius(title, 8, 0);
  lv_obj_set_style_bg_color(title, lv_color_hex(kTitleGold), 0);
  lv_obj_set_style_bg_opa(title, LV_OPA_COVER, 0);
  lv_obj_t * title_lab = lv_label_create(title);
  lv_label_set_text(title_lab, "2048");
  lv_obj_set_style_text_color(title_lab, lv_color_hex(0xf9f6f2), 0);
  lv_obj_set_style_text_font(title_lab, &lv_font_montserrat_16, 0);
  lv_obj_center(title_lab);

  accent_btn(left, "NEW", [](lv_event_t * /*e*/) {
    show_confirm("Start a new game?", "New game", true, [](lv_event_t * /*e*/) {
      note_high_score();
      reset_session();
      go_g2048();
    });
  }, 72, 56);

  lv_obj_t * chips = lv_obj_create(head);
  lv_obj_remove_style_all(chips);
  lv_obj_set_size(chips, LV_SIZE_CONTENT, 56);
  lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(chips, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(chips, 8, 0);
  make_score_chip(chips, "SCORE", g_sess.score);
  make_score_chip(chips, "BEST", app::desk().high_score_2048);

  /* Board + D-pad fit above dock */
  constexpr int kBoard = 292;
  constexpr int kGap = 8;
  constexpr int kPad = 10;
  constexpr int kCell = (kBoard - 2 * kPad - 3 * kGap) / 4;
  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, kBoard, kBoard);
  lv_obj_set_style_radius(board, 8, 0);
  lv_obj_set_style_bg_color(board, lv_color_hex(kBoardBg), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(board, kPad, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(board, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(board, on_gesture, LV_EVENT_GESTURE, nullptr);

  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      const uint16_t v = g_sess.board[r * kSize + c];
      lv_obj_t * cell = lv_obj_create(board);
      lv_obj_remove_style_all(cell);
      lv_obj_set_size(cell, kCell, kCell);
      lv_obj_set_pos(cell, c * (kCell + kGap), r * (kCell + kGap));
      lv_obj_set_style_radius(cell, 4, 0);
      lv_obj_set_style_bg_color(cell, tile_color(v), 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE);
      if (v) {
        lv_obj_t * lab = lv_label_create(cell);
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)v);
        lv_label_set_text(lab, buf);
        lv_obj_set_style_text_color(lab, tile_ink(v), 0);
        const lv_font_t * font =
            v >= 1024 ? &lv_font_montserrat_14 : (v >= 128 ? &lv_font_montserrat_16 : &lv_font_montserrat_20);
        lv_obj_set_style_text_font(lab, font, 0);
        lv_obj_center(lab);
      }
    }
  }

  /* Single-row D-pad — always visible above dock */
  lv_obj_t * pad = lv_obj_create(parent);
  lv_obj_remove_style_all(pad);
  lv_obj_set_width(pad, 280);
  lv_obj_set_height(pad, 40);
  lv_obj_set_flex_flow(pad, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pad, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  dir_btn(pad, "<", Dir::Left);
  dir_btn(pad, "^", Dir::Up);
  dir_btn(pad, "v", Dir::Down);
  dir_btn(pad, ">", Dir::Right);

  if (g_sess.over) {
    lv_obj_t * ov = lv_obj_create(parent);
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(ov, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(ov, lv_color_hex(kCream), 0);
    lv_obj_set_style_bg_opa(ov, 180, 0);
    lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * card = lv_obj_create(ov);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 260, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_center(card);

    lv_obj_t * ttl = lv_label_create(card);
    lv_label_set_text(ttl, "Game over!");
    lv_obj_set_style_text_color(ttl, lv_color_hex(kInk), 0);
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_20, 0);

    lv_obj_t * sub = lv_label_create(card);
    char sb[48];
    lv_snprintf(sb, sizeof(sb), "Score %d", g_sess.score);
    lv_label_set_text(sub, sb);
    lv_obj_set_style_text_color(sub, lv_color_hex(kInkLight), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);

    accent_btn(card, "Try again", [](lv_event_t * /*e*/) {
      reset_session();
      go_g2048();
    }, 140, 40);
  }
}

}  // namespace

lv_obj_t * game_g2048_screen() {
  ensure_started();
  lv_obj_t * scr = make_screen();
  lv_obj_set_style_bg_color(scr, lv_color_hex(kCream), 0);

  /* No WerkBuddy topbar — classic 2048 is full-bleed cream to the dock. */
  lv_obj_t * body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, WP_HOR_RES, WP_VER_RES - kDockH);
  lv_obj_set_pos(body, 0, 0);
  lv_obj_set_style_bg_color(body, lv_color_hex(kCream), 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(body, 14, 0);
  lv_obj_set_style_pad_top(body, 10, 0);
  lv_obj_set_style_pad_bottom(body, 4, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 6, 0);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  fill_board(body);

  lv_obj_t * dock = make_dock(scr);
  lv_obj_set_style_bg_color(dock, lv_color_hex(kCream), 0);
  lv_obj_set_style_bg_opa(dock, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dock, 0, 0);
  lv_obj_set_style_pad_top(dock, 4, 0);

  lv_obj_t * forfeit = dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
    show_forfeit_confirm(on_forfeit_yes);
  });
  lv_obj_set_flex_grow(forfeit, 0);
  lv_obj_set_width(forfeit, 120);
  lv_obj_set_height(forfeit, 32);
  lv_obj_set_style_bg_color(forfeit, lv_color_hex(kAccent), 0);

  lv_obj_t * home = dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
  lv_obj_set_height(home, 32);
  lv_obj_set_style_bg_color(home, lv_color_hex(kInkLight), 0);
  if (lv_obj_get_child_count(home) > 0) {
    lv_obj_set_style_text_color(lv_obj_get_child(home, 0), lv_color_hex(0xffffff), 0);
  }
  return scr;
}

}  // namespace ui
}  // namespace wp
