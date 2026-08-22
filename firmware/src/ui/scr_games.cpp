#include "ui/scr_games.h"

#include "app/app.h"
#include "app/active_games.h"
#include "app/presence.h"
#include "app/score_log.h"
#include "games/battleship.h"
#include "games/c4.h"
#include "games/checkers.h"
#include "games/dots.h"
#include "games/memory.h"
#include "games/reversi.h"
#include "games/sttt.h"
#include "games/ttt.h"
#include "protocol/messages.h"
#include "ui/chrome.h"
#include "ui/emoji_badge.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/nav.h"
#include "ui/scr_wordle.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifndef WP_DEVICE
#include <filesystem>
#else
#include <esp_heap_caps.h>
#include "memory_pack.h"
#endif

namespace wp {
namespace ui {
namespace {

void fill_msg_ids(proto::Msg & m, const char * to_id) {
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", app::desk().id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", app::desk().name);
  if (to_id) std::snprintf(m.to_id, sizeof(m.to_id), "%s", to_id);
}

/** Topbar subtitle under the game title — matches web `vs <strong>Name</strong>`. */
const char * game_vs_sub(char * buf, size_t n, bool active, const char * opp_name,
                         bool invite_active, const char * invite_from) {
  buf[0] = '\0';
  if (active && opp_name && opp_name[0]) {
    lv_snprintf(buf, (uint32_t)n, "vs. %s", opp_name);
  } else if (invite_active && invite_from && invite_from[0]) {
    lv_snprintf(buf, (uint32_t)n, "vs. %s", invite_from);
  }
  return buf[0] ? buf : nullptr;
}

/** Win / lose / draw celebration card. Logs to scoreboard when game/peer given. */
void attach_result_overlay(lv_obj_t * parent, int outcome /*1 win, 0 lose, -1 draw*/,
                           lv_event_cb_t on_dismiss, const char * game = nullptr,
                           const char * peer = nullptr) {
  if (game && game[0]) {
    const score_log::Outcome o =
        outcome > 0 ? score_log::Outcome::Win
                    : (outcome < 0 ? score_log::Outcome::Tie : score_log::Outcome::Lose);
    score_log::note(game, peer, o);
  }

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
  lv_obj_set_style_bg_color(card, theme::panel(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 20, 0);
  lv_obj_set_style_pad_all(card, 18, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_border_color(card, outcome > 0 ? theme::gold() : (outcome < 0 ? theme::mint() : theme::hot()), 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, 8, 0);
  lv_obj_center(card);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);

  const char * emo = outcome > 0 ? "🎉" : (outcome < 0 ? "🤝" : "😢");
  lv_obj_t * img = make_emoji_image(card, emo, 56);
  lv_obj_center(img);

  lv_obj_t * res = lv_label_create(card);
  lv_label_set_text(res, outcome > 0 ? "Condragulations!" : (outcome < 0 ? "It's a draw" : "Sashay away..."));
  lv_obj_set_style_text_color(res, theme::gold(), 0);
  lv_obj_set_style_text_font(res, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(res, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * tap = lv_label_create(card);
  lv_label_set_text(tap, "tap to dismiss");
  lv_obj_set_style_text_color(tap, theme::muted(), 0);
  lv_obj_set_style_text_font(tap, &lv_font_montserrat_12, 0);

  if (on_dismiss) lv_obj_add_event_cb(ov, on_dismiss, LV_EVENT_CLICKED, nullptr);
}

void dock_play_again_home(lv_obj_t * dock, lv_event_cb_t on_again, lv_event_cb_t on_home) {
  dock_btn(dock, "Play again", true, false, on_again);
  dock_btn(dock, "Home", false, false, on_home);
}

/** Outgoing invite: Cancel frees the slot; Home leaves it in Active Games. */
void dock_cancel_home(lv_obj_t * dock) {
  dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
    app::cancel_slot(app::focus_index());
  });
  dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
}

/** Slim centered Forfeit control — not a full-width slab. */
lv_obj_t * dock_forfeit_btn(lv_obj_t * dock, lv_event_cb_t on_confirm) {
  lv_obj_t * btn = dock_btn(dock, "Forfeit", false, true, [](lv_event_t * e) {
    auto * cb = reinterpret_cast<lv_event_cb_t>(lv_event_get_user_data(e));
    show_forfeit_confirm(cb);
  }, (void *)on_confirm);
  lv_obj_set_flex_grow(btn, 0);
  lv_obj_set_width(btn, 120);
  lv_obj_set_height(btn, 28);
  return btn;
}

lv_obj_t * make_status(lv_obj_t * parent, const char * text) {
  lv_obj_t * s = lv_label_create(parent);
  lv_label_set_text(s, text);
  lv_obj_set_style_text_color(s, theme::mint(), 0);
  lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(s, lv_pct(100));
  return s;
}

bool guard_peer_challenge(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  char why[48];
  if (!app::peer_contact_ok(idx, why, sizeof(why))) {
    toast(why);
    return false;
  }
  return true;
}

bool guard_active_move(const char * opp_id) {
  if (!opp_id || !opp_id[0]) return true;
  if (!app::peer_present(opp_id)) {
    toast("Opponent is away");
    return false;
  }
  return true;
}

/** Light score pill: Name · disc · n   Name · disc · n (shared by scored games). */
lv_obj_t * make_score_pill(lv_obj_t * parent, const char * name_a, int score_a, lv_color_t accent_a,
                           const char * name_b, int score_b, lv_color_t accent_b) {
  lv_obj_t * pill = lv_obj_create(parent);
  lv_obj_remove_style_all(pill);
  lv_obj_set_height(pill, 26);
  lv_obj_set_width(pill, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(pill, 12, 0);
  lv_obj_set_style_pad_ver(pill, 3, 0);
  lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(pill, lv_color_hex(0xd6cfc4), 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(pill, 12, 0);
  lv_obj_remove_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(pill, LV_OBJ_FLAG_CLICKABLE);

  const lv_color_t pill_ink = lv_color_hex(0x1a1220);
  auto add_side = [&](const char * name, int n, lv_color_t accent) {
    lv_obj_t * side = lv_obj_create(pill);
    lv_obj_remove_style_all(side);
    lv_obj_set_size(side, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(side, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(side, 4, 0);
    lv_obj_remove_flag(side, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(side, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * lab = lv_label_create(side);
    lv_label_set_text(lab, name);
    lv_obj_set_style_text_color(lab, pill_ink, 0);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);

    lv_obj_t * disc = lv_obj_create(side);
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, 12, 12);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, accent, 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(disc, 1, 0);
    lv_obj_set_style_border_color(disc, lv_color_hex(0x5a5048), 0);
    lv_obj_remove_flag(disc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * num = lv_label_create(side);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", n);
    lv_label_set_text(num, buf);
    lv_obj_set_style_text_color(num, pill_ink, 0);
    lv_obj_set_style_text_font(num, &lv_font_montserrat_14, 0);
  };
  add_side(name_a, score_a, accent_a);
  add_side(name_b, score_b, accent_b);
  return pill;
}

lv_obj_t * make_wait_block(lv_obj_t * parent, const char * eye, const char * name, const char * sub) {
  lv_obj_t * box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_width(box, lv_pct(100));
  lv_obj_set_flex_grow(box, 1);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(box, 8, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * e = lv_label_create(box);
  lv_label_set_text(e, eye);
  lv_obj_set_style_text_color(e, theme::mint(), 0);
  lv_obj_set_style_text_font(e, &lv_font_montserrat_12, 0);

  lv_obj_t * n = lv_label_create(box);
  lv_label_set_text(n, name);
  lv_obj_set_style_text_color(n, theme::gold(), 0);
  lv_obj_set_style_text_font(n, font_display(52), 0);

  lv_obj_t * s = lv_label_create(box);
  lv_label_set_text(s, sub);
  lv_obj_set_style_text_color(s, theme::muted(), 0);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  return box;
}


bool begin_kind(app::GameKind kind, const app::Peer & p) {
  if (app::begin_match(kind, p.id)) return true;
  if (app::find_slot(kind, p.id) >= 0) {
    char buf[72];
    lv_snprintf(buf, sizeof(buf), "Already playing %s with %s", app::kind_name(kind), p.name);
    toast(buf);
  } else if (app::active_count() >= app::kMaxActiveGames) {
    char buf[56];
    lv_snprintf(buf, sizeof(buf), "Active Games full (%d max)", app::kMaxActiveGames);
    toast(buf);
  } else {
    toast("Can't start that game");
  }
  return false;
}

void dock_forfeit_home(lv_obj_t * dock, lv_event_cb_t on_confirm) {
  dock_forfeit_btn(dock, on_confirm);
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_game_back(); });
}

void peer_list(lv_obj_t * parent, lv_event_cb_t on_peer) {
  make_tagline(parent, "Challenge a peer");
  const app::Desk & d = app::desk();
  if (d.peer_count == 0) make_tagline(parent, "No saved desks - add one in Settings.");
  for (int i = 0; i < d.peer_count; ++i) {
    char sub[24];
    const char * st = app::peer_presence_subtitle(i, sub, sizeof(sub));
    lv_obj_t * btn = make_peer_btn(parent, d.peers[i].name, st, on_peer, (void *)(intptr_t)i);
    if (!app::peer_present_idx(i)) lv_obj_set_style_opa(btn, LV_OPA_50, 0);
  }
}

/* ================= TIC TAC TOE ================= */

void ttt_challenge(lv_event_t * e) {
  if (!guard_peer_challenge(e)) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];
  if (!begin_kind(app::GameKind::Ttt, p)) return;
  app::ttt() = {};
  app::ttt().active = true;
  app::ttt().waiting = true;
  const bool first = app::roll_first();
  app::ttt().mark = first ? 'X' : 'O';
  app::ttt().turn = 'X';
  std::snprintf(app::ttt().opp_id, sizeof(app::ttt().opp_id), "%s", p.id);
  std::snprintf(app::ttt().opp_name, sizeof(app::ttt().opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::TttInvite;
  fill_msg_ids(m, p.id);
  m.first = first;
  app::send(m);
  go_ttt();
}

void ttt_play_cell(lv_event_t * e) {
  const int cell = (int)(intptr_t)lv_event_get_user_data(e);
  app::TttGame & g = app::ttt();
  if (!g.active || g.waiting || g.over || g.turn != g.mark) return;
  if (!guard_active_move(g.opp_id)) return;
  if (cell < 0 || cell >= 9 || g.board[cell]) return;
  g.board[cell] = g.mark;
  proto::Msg m;
  m.type = proto::MsgType::TttMove;
  fill_msg_ids(m, g.opp_id);
  m.cell = (int8_t)cell;
  m.mark = g.mark;
  app::send(m);
  if (games::ttt::winner(g.board) || games::ttt::full(g.board)) {
    g.over = true;
    g.result_dismissed = false;
  } else {
    g.turn = g.mark == 'X' ? 'O' : 'X';
  }
  go_ttt();
}

/** Geometric X / thick ring O — not font glyphs (O looked like "0"). X=hot, O=gold. */
void ttt_draw_mark(lv_obj_t * parent, char mark, int size) {
  const lv_color_t col = (mark == 'X') ? theme::hot() : theme::gold();
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
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    return;
  }
  /* X: two rounded bars crossed via lines */
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

void fill_ttt_play(lv_obj_t * parent) {
  app::TttGame & g = app::ttt();
  const bool my_turn = !g.over && !g.waiting && g.turn == g.mark;
  const char win = games::ttt::winner(g.board);
  if (g.over && !g.result_dismissed) {
    make_status(parent, "Game over");
  } else if (my_turn) {
    make_status(parent, "Your turn");
  } else if (g.over) {
    make_status(parent, "Play again?");
  } else {
    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Waiting on %s...", g.opp_name);
    make_status(parent, buf);
  }

  lv_obj_t * row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, 28);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * you = lv_label_create(row);
  lv_label_set_text(you, "you are");
  lv_obj_set_style_text_color(you, theme::muted(), 0);
  lv_obj_set_style_text_font(you, &lv_font_montserrat_12, 0);
  lv_obj_t * badge = lv_obj_create(row);
  lv_obj_remove_style_all(badge);
  lv_obj_set_size(badge, 28, 28);
  lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(badge, theme::panel(), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(badge, 1, 0);
  lv_obj_set_style_border_color(badge, theme::border(), 0);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  ttt_draw_mark(badge, g.mark, 18);

  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 280, 280);
  lv_obj_set_style_radius(board, 20, 0);
  lv_obj_set_style_bg_color(board, theme::panel(), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(board, 1, 0);
  lv_obj_set_style_border_color(board, theme::border(), 0);
  lv_obj_set_layout(board, LV_LAYOUT_GRID);
  static lv_coord_t c[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t r[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(board, c, r);
  lv_obj_set_style_pad_all(board, 8, 0);
  lv_obj_set_style_pad_row(board, 8, 0);
  lv_obj_set_style_pad_column(board, 8, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  const lv_color_t cell_bg = lv_color_mix(theme::bg0(), theme::panel(), 140);
  for (int i = 0; i < 9; ++i) {
    lv_obj_t * cell = lv_obj_create(board);
    lv_obj_remove_style_all(cell);
    lv_obj_set_style_radius(cell, 16, 0);
    lv_obj_set_style_bg_color(cell, cell_bg, 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, theme::border(), 0);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    if (g.board[i]) {
      ttt_draw_mark(cell, g.board[i], 52);
    } else if (my_turn) {
      lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(cell, ttt_play_cell, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
  }

  if (g.over && !g.result_dismissed) {
    const int outcome = !win ? -1 : (win == g.mark ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::ttt().result_dismissed = true;
      go_ttt();
    }, "Tic Tac Toe", g.opp_name);
  }
}

lv_obj_t * game_ttt_build(lv_obj_t * into) {
  app::Desk & d = app::desk();
  lv_obj_t * scr = into;
  if (scr) lv_obj_clean(scr);
  else scr = make_screen();
  char sub[40];
  make_topbar(scr, "TIC TAC TOE", d.name,
              game_vs_sub(sub, sizeof(sub), app::ttt().active, app::ttt().opp_name, app::invite_active(app::GameKind::Ttt),
                          app::invite_ref(app::GameKind::Ttt).from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (app::invite_active(app::GameKind::Ttt)) {
    make_wait_block(body, "GAME INVITE", app::invite_ref(app::GameKind::Ttt).from_name, "wants to play Tic Tac Toe");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::TttDecline;
      fill_msg_ids(m, app::invite_ref(app::GameKind::Ttt).from_id);
      app::send(m);
      app::end_focused();
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
  const auto inv = app::invite_ref(app::GameKind::Ttt);
  app::accept_invite(app::GameKind::Ttt);
  app::ttt() = {};
  app::ttt().active = true;
  app::ttt().mark = inv.first ? 'O' : 'X';
  app::ttt().turn = 'X';
      std::snprintf(app::ttt().opp_id, sizeof(app::ttt().opp_id), "%s", inv.from_id);
      std::snprintf(app::ttt().opp_name, sizeof(app::ttt().opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::TttAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_ttt();
    });
  } else if (app::ttt().active && app::ttt().waiting) {
    make_wait_block(body, "CHALLENGE SENT", app::ttt().opp_name, "Waiting for them to accept...");
    dock_cancel_home(dock);
  } else if (app::ttt().active) {
    fill_ttt_play(body);
    if (app::ttt().over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            app::Peer p;
            std::snprintf(p.id, sizeof(p.id), "%s", app::ttt().opp_id);
            std::snprintf(p.name, sizeof(p.name), "%s", app::ttt().opp_name);
            app::ttt() = {};
            app::ttt().active = true;
            app::ttt().waiting = true;
            const bool first = app::roll_first();
            app::ttt().mark = first ? 'X' : 'O';
            app::ttt().turn = 'X';
            std::snprintf(app::ttt().opp_id, sizeof(app::ttt().opp_id), "%s", p.id);
            std::snprintf(app::ttt().opp_name, sizeof(app::ttt().opp_name), "%s", p.name);
            proto::Msg m;
            m.type = proto::MsgType::TttInvite;
            fill_msg_ids(m, p.id);
            m.first = first;
            app::send(m);
            go_ttt();
          },
          [](lv_event_t * /*e*/) {
            app::end_focused();
            go_hub();
          });
    } else {
      dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
          score_log::note("Tic Tac Toe", app::ttt().opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::TttForfeit;
          fill_msg_ids(m, app::ttt().opp_id);
          app::send(m);
          app::end_focused();
          go_hub();
      });
    }
  } else {
    peer_list(body, ttt_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

/* ================= SUPER TIC TAC TOE ================= */

void sttt_challenge(lv_event_t * e) {
  if (!guard_peer_challenge(e)) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];
  if (!begin_kind(app::GameKind::Sttt, p)) return;
  app::sttt() = {};
  app::sttt().active = true;
  app::sttt().waiting = true;
  const bool first = app::roll_first();
  app::sttt().mark = first ? 'X' : 'O';
  app::sttt().turn = 'X';
  games::sttt::init(app::sttt().boards, app::sttt().meta, app::sttt().next_board);
  std::snprintf(app::sttt().opp_id, sizeof(app::sttt().opp_id), "%s", p.id);
  std::snprintf(app::sttt().opp_name, sizeof(app::sttt().opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::StttInvite;
  fill_msg_ids(m, p.id);
  m.first = first;
  app::send(m);
  go_sttt();
}

void sttt_play_cell(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  const int board = packed / 9;
  const int cell = packed % 9;
  app::StttGame & g = app::sttt();
  if (!g.active || g.waiting || g.over || g.turn != g.mark) return;
  if (!guard_active_move(g.opp_id)) return;
  if (!games::sttt::play(g.boards, g.meta, g.next_board, board, cell, g.mark)) return;
  proto::Msg m;
  m.type = proto::MsgType::StttMove;
  fill_msg_ids(m, g.opp_id);
  m.col = (int8_t)board;
  m.cell = (int8_t)cell;
  m.mark = g.mark;
  app::send(m);
  if (games::sttt::over(g.meta)) {
    g.over = true;
    g.result_dismissed = false;
  } else {
    g.turn = g.mark == 'X' ? 'O' : 'X';
  }
  go_sttt();
}

void fill_sttt_play(lv_obj_t * parent) {
  app::StttGame & g = app::sttt();
  const bool my_turn = !g.over && !g.waiting && g.turn == g.mark;
  const char win = games::sttt::winner(g.meta);
  const bool forced = games::sttt::forced(g.meta, g.next_board);

  /* Board fills body — status / mark live in the topbar. */
  lv_obj_set_style_pad_ver(parent, 2, 0);
  lv_obj_set_style_pad_row(parent, 0, 0);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t * center = lv_obj_create(parent);
  lv_obj_remove_style_all(center);
  lv_obj_set_width(center, lv_pct(100));
  lv_obj_set_height(center, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(center, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(center, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int kOuter = 368;
  lv_obj_t * board = lv_obj_create(center);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, kOuter, kOuter);
  lv_obj_set_style_radius(board, 18, 0);
  lv_obj_set_style_bg_color(board, theme::panel(), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(board, 1, 0);
  lv_obj_set_style_border_color(board, theme::muted(), 0);
  lv_obj_set_layout(board, LV_LAYOUT_GRID);
  static lv_coord_t bc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t br[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(board, bc, br);
  lv_obj_set_style_pad_all(board, 5, 0);
  lv_obj_set_style_pad_row(board, 5, 0);
  lv_obj_set_style_pad_column(board, 5, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  const lv_color_t grid_line = theme::muted();
  const lv_color_t cell_bg = lv_color_mix(theme::bg0(), theme::panel(), 180);

  for (int b = 0; b < 9; ++b) {
    const bool forced_here = my_turn && forced && g.next_board == b;
    /* Shell = distinct outline for each mini-board; inner = purple cell gaps. */
    lv_obj_t * shell = lv_obj_create(board);
    lv_obj_remove_style_all(shell);
    lv_obj_set_style_radius(shell, 10, 0);
    lv_obj_set_style_bg_color(shell, theme::panel(), 0);
    lv_obj_set_style_bg_opa(shell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(shell, forced_here ? 2 : 2, 0);
    lv_obj_set_style_border_color(shell, forced_here ? theme::mint() : grid_line, 0);
    lv_obj_set_style_border_opa(shell, forced_here ? LV_OPA_COVER : LV_OPA_80, 0);
    lv_obj_set_grid_cell(shell, LV_GRID_ALIGN_STRETCH, b % 3, 1, LV_GRID_ALIGN_STRETCH, b / 3, 1);
    lv_obj_set_style_pad_all(shell, 2, 0);
    lv_obj_remove_flag(shell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * mini = lv_obj_create(shell);
    lv_obj_remove_style_all(mini);
    lv_obj_set_size(mini, lv_pct(100), lv_pct(100));
    lv_obj_set_style_radius(mini, 6, 0);
    lv_obj_set_style_bg_color(mini, grid_line, 0);
    lv_obj_set_style_bg_opa(mini, LV_OPA_40, 0);
    lv_obj_set_layout(mini, LV_LAYOUT_GRID);
    static lv_coord_t mc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t mr[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(mini, mc, mr);
    lv_obj_set_style_pad_all(mini, 2, 0);
    lv_obj_set_style_pad_row(mini, 2, 0);
    lv_obj_set_style_pad_column(mini, 2, 0);
    lv_obj_remove_flag(mini, LV_OBJ_FLAG_SCROLLABLE);

    for (int c = 0; c < 9; ++c) {
      lv_obj_t * cell = lv_obj_create(mini);
      lv_obj_remove_style_all(cell);
      lv_obj_set_style_radius(cell, 4, 0);
      lv_obj_set_style_bg_color(cell, cell_bg, 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, c % 3, 1, LV_GRID_ALIGN_STRETCH, c / 3, 1);
      lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
      if (g.boards[b][c]) {
        ttt_draw_mark(cell, g.boards[b][c], 26);
      } else if (my_turn && games::sttt::legal(g.boards, g.meta, g.next_board, b, c)) {
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cell, sttt_play_cell, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(b * 9 + c));
      }
    }

    if (g.meta[b] == 'X' || g.meta[b] == 'O') {
      lv_obj_t * ov = lv_obj_create(mini);
      lv_obj_remove_style_all(ov);
      lv_obj_set_size(ov, lv_pct(100), lv_pct(100));
      lv_obj_set_style_radius(ov, 6, 0);
      lv_obj_set_style_bg_color(ov, g.meta[b] == 'X' ? theme::hot() : theme::mint(), 0);
      lv_obj_set_style_bg_opa(ov, 70, 0);
      lv_obj_add_flag(ov, LV_OBJ_FLAG_FLOATING);
      lv_obj_remove_flag(ov, LV_OBJ_FLAG_CLICKABLE);
      ttt_draw_mark(ov, g.meta[b], 56);
      lv_obj_move_foreground(ov);
    } else if (g.meta[b] == 'D') {
      lv_obj_set_style_border_opa(shell, LV_OPA_40, 0);
    }
  }

  if (g.over && !g.result_dismissed) {
    const int outcome = !win ? -1 : (win == g.mark ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::sttt().result_dismissed = true;
      go_sttt();
    }, "Super TTT", g.opp_name);
  }
}

lv_obj_t * make_sttt_play_topbar(lv_obj_t * scr, const char * me, const char * opp, char mark,
                                 const char * status) {
  lv_obj_t * top = lv_obj_create(scr);
  lv_obj_remove_style_all(top);
  lv_obj_set_size(top, WP_HOR_RES, kTopbarH);
  lv_obj_set_pos(top, 0, 0);
  lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(top, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  /* Left: title + vs/status. Fixed max width so it cannot cover the right column. */
  lv_obj_t * left = lv_obj_create(top);
  lv_obj_remove_style_all(left);
  lv_obj_set_pos(left, 14, 6);
  lv_obj_set_size(left, 280, kTopbarH - 8);
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(left, 1, 0);
  lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * brand = lv_label_create(left);
  lv_label_set_text(brand, "SUPER TIC TAC TOE");
  lv_obj_set_style_text_color(brand, theme::gold(), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);

  lv_obj_t * sub_row = lv_obj_create(left);
  lv_obj_remove_style_all(sub_row);
  lv_obj_set_size(sub_row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(sub_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(sub_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(sub_row, 8, 0);
  lv_obj_remove_flag(sub_row, LV_OBJ_FLAG_SCROLLABLE);

  char vs[40];
  lv_snprintf(vs, sizeof(vs), "vs. %s", opp ? opp : "?");
  lv_obj_t * vs_lbl = lv_label_create(sub_row);
  lv_label_set_text(vs_lbl, vs);
  lv_obj_set_style_text_color(vs_lbl, theme::muted(), 0);
  lv_obj_set_style_text_font(vs_lbl, &lv_font_montserrat_12, 0);

  if (status && status[0]) {
    lv_obj_t * st = lv_label_create(sub_row);
    lv_label_set_text(st, status);
    lv_obj_set_style_text_color(st, theme::mint(), 0);
    lv_obj_set_style_text_font(st, &lv_font_montserrat_12, 0);
  }

  /* Right chrome: pin labels directly on the topbar (no nested clip boxes). */
  lv_obj_t * me_lbl = lv_label_create(top);
  lv_label_set_text(me_lbl, me && me[0] ? me : "Tommy");
  lv_obj_set_style_text_color(me_lbl, theme::muted(), 0);
  lv_obj_set_style_text_font(me_lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(me_lbl, LV_ALIGN_TOP_RIGHT, -14, 4);

  lv_obj_t * mark_row = lv_obj_create(top);
  lv_obj_remove_style_all(mark_row);
  lv_obj_set_size(mark_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mark_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(mark_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(mark_row, 5, 0);
  lv_obj_remove_flag(mark_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(mark_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t * you = lv_label_create(mark_row);
  lv_label_set_text(you, "you are");
  lv_obj_set_style_text_color(you, theme::muted(), 0);
  lv_obj_set_style_text_font(you, &lv_font_montserrat_12, 0);

  lv_obj_t * badge = lv_obj_create(mark_row);
  lv_obj_remove_style_all(badge);
  lv_obj_set_size(badge, 22, 22);
  lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(badge, theme::hot(), 0);
  lv_obj_set_style_bg_grad_color(badge, theme::gold(), 0);
  lv_obj_set_style_bg_grad_dir(badge, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * bl = lv_label_create(badge);
  char mk[2] = {mark ? mark : '?', 0};
  lv_label_set_text(bl, mk);
  lv_obj_set_style_text_color(bl, lv_color_hex(0x1a0610), 0);
  lv_obj_set_style_text_font(bl, &lv_font_montserrat_12, 0);
  lv_obj_center(bl);

  lv_obj_align_to(mark_row, me_lbl, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 2);
  lv_obj_move_foreground(me_lbl);
  lv_obj_move_foreground(mark_row);

  return top;
}

lv_obj_t * game_sttt_build(lv_obj_t * into) {
  app::Desk & d = app::desk();
  lv_obj_t * scr = into;
  if (scr) lv_obj_clean(scr);
  else scr = make_screen();

  const bool playing = app::sttt().active && !app::sttt().waiting && !app::invite_active(app::GameKind::Sttt);
  lv_obj_t * top = nullptr;
  if (playing) {
    const bool my_turn = !app::sttt().over && app::sttt().turn == app::sttt().mark;
    const bool forced = games::sttt::forced(app::sttt().meta, app::sttt().next_board);
    const char * status;
    if (app::sttt().over && !app::sttt().result_dismissed) status = "Game over";
    else if (app::sttt().over) status = "Play again?";
    else if (my_turn && forced) status = "Your turn: lit board";
    else if (my_turn) status = "Your turn: any board";
    else {
      static char wait[40];
      lv_snprintf(wait, sizeof(wait), "Waiting on %s", app::sttt().opp_name);
      status = wait;
    }
    top = make_sttt_play_topbar(scr, d.name, app::sttt().opp_name, app::sttt().mark, status);
  } else {
    char sub[40];
    top = make_topbar(scr, "SUPER TIC TAC TOE", d.name,
                      game_vs_sub(sub, sizeof(sub), app::sttt().active, app::sttt().opp_name,
                                  app::invite_active(app::GameKind::Sttt), app::invite_ref(app::GameKind::Sttt).from_name));
  }
  (void)top;

  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (app::invite_active(app::GameKind::Sttt)) {
    make_wait_block(body, "GAME INVITE", app::invite_ref(app::GameKind::Sttt).from_name, "wants to play Super Tic Tac Toe");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::StttDecline;
      fill_msg_ids(m, app::invite_ref(app::GameKind::Sttt).from_id);
      app::send(m);
      app::end_focused();
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = app::invite_ref(app::GameKind::Sttt);
      app::accept_invite(app::GameKind::Sttt);
      app::sttt() = {};
      app::sttt().active = true;
      app::sttt().mark = inv.first ? 'O' : 'X';
      app::sttt().turn = 'X';
      games::sttt::init(app::sttt().boards, app::sttt().meta, app::sttt().next_board);
      std::snprintf(app::sttt().opp_id, sizeof(app::sttt().opp_id), "%s", inv.from_id);
      std::snprintf(app::sttt().opp_name, sizeof(app::sttt().opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::StttAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_sttt();
    });
  } else if (app::sttt().active && app::sttt().waiting) {
    make_wait_block(body, "CHALLENGE SENT", app::sttt().opp_name, "Waiting for them to accept...");
    dock_cancel_home(dock);
  } else if (app::sttt().active) {
    /* Compact dock so the board can use more vertical space. */
    lv_obj_set_height(dock, kDockCompactH);
    lv_obj_set_style_pad_ver(dock, 6, 0);
    lv_obj_set_height(body, WP_VER_RES - kTopbarH - kDockCompactH);

    fill_sttt_play(body);
    if (app::sttt().over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            app::Peer p;
            std::snprintf(p.id, sizeof(p.id), "%s", app::sttt().opp_id);
            std::snprintf(p.name, sizeof(p.name), "%s", app::sttt().opp_name);
            app::sttt() = {};
            app::sttt().active = true;
            app::sttt().waiting = true;
            const bool first = app::roll_first();
            app::sttt().mark = first ? 'X' : 'O';
            app::sttt().turn = 'X';
            games::sttt::init(app::sttt().boards, app::sttt().meta, app::sttt().next_board);
            std::snprintf(app::sttt().opp_id, sizeof(app::sttt().opp_id), "%s", p.id);
            std::snprintf(app::sttt().opp_name, sizeof(app::sttt().opp_name), "%s", p.name);
            proto::Msg m;
            m.type = proto::MsgType::StttInvite;
            fill_msg_ids(m, p.id);
            m.first = first;
            app::send(m);
            go_sttt();
          },
          [](lv_event_t * /*e*/) {
            app::end_focused();
            go_hub();
          });
      /* Shrink play-again dock buttons too. */
      const uint32_t n = lv_obj_get_child_count(dock);
      for (uint32_t i = 0; i < n; ++i) lv_obj_set_height(lv_obj_get_child(dock, i), 36);
    } else {
      dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
          score_log::note("Super TTT", app::sttt().opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::StttForfeit;
          fill_msg_ids(m, app::sttt().opp_id);
          app::send(m);
          app::end_focused();
          go_hub();
      });
    }
  } else {
    peer_list(body, sttt_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

/* ================= CONNECT FOUR ================= */

int g_c4_pick_color = 0;

struct DiscColors { uint32_t light, main, dark; };
const DiscColors kDiscs[6] = {
    {0xffb0d4, 0xff4fa3, 0xd62878}, {0xffe566, 0xf0c24b, 0xc49214},
    {0xb8ffe0, 0x5dffc2, 0x1aa879}, {0xb8e4ff, 0x4fa3ff, 0x1d6fd0},
    {0xe0b8ff, 0xa85dff, 0x6b21a8}, {0xffc4b0, 0xff7a3c, 0xd9480f},
};

lv_grad_dsc_t * disc_grad(int idx) {
  static lv_grad_dsc_t grads[6];
  static bool init = false;
  if (!init) {
    init = true;
    for (int i = 0; i < 6; ++i) {
      const lv_color_t colors[3] = {lv_color_hex(kDiscs[i].light), lv_color_hex(kDiscs[i].main),
                                    lv_color_hex(kDiscs[i].dark)};
      const lv_opa_t opas[3] = {LV_OPA_COVER, LV_OPA_COVER, LV_OPA_COVER};
      const uint8_t fracs[3] = {0, 140, 255};
      lv_grad_init_stops(&grads[i], colors, opas, fracs, 3);
      lv_grad_radial_init(&grads[i], lv_pct(30), lv_pct(28), lv_pct(100), lv_pct(110),
                          LV_GRAD_EXTEND_PAD);
    }
  }
  return &grads[idx % 6];
}

lv_grad_dsc_t * c4_rainbow() {
  static lv_grad_dsc_t g;
  static bool init = false;
  if (!init) {
    init = true;
    const lv_color_t colors[7] = {
        lv_color_hex(0xe8899c), lv_color_hex(0xe8b56a), lv_color_hex(0xd4d06a),
        lv_color_hex(0x7dcca8), lv_color_hex(0x6aa8d8), lv_color_hex(0xa88ad8),
        lv_color_hex(0xe8899c),
    };
    const lv_opa_t opas[7] = {LV_OPA_COVER, LV_OPA_COVER, LV_OPA_COVER, LV_OPA_COVER,
                              LV_OPA_COVER, LV_OPA_COVER, LV_OPA_COVER};
    const uint8_t fracs[7] = {0, 46, 92, 138, 184, 224, 255};
    lv_grad_init_stops(&g, colors, opas, fracs, 7);
    lv_grad_linear_init(&g, lv_pct(0), lv_pct(0), lv_pct(100), lv_pct(100), LV_GRAD_EXTEND_PAD);
  }
  return &g;
}

void c4_challenge(lv_event_t * e) {
  if (!guard_peer_challenge(e)) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];
  if (!begin_kind(app::GameKind::C4, p)) return;
  app::c4() = {};
  app::c4().active = true;
  app::c4().waiting = true;
  app::c4().my_color = (int8_t)g_c4_pick_color;
  const bool first = app::roll_first();
  app::c4().first = first;
  games::c4::init(app::c4().board);
  std::snprintf(app::c4().opp_id, sizeof(app::c4().opp_id), "%s", p.id);
  std::snprintf(app::c4().opp_name, sizeof(app::c4().opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::C4Invite;
  fill_msg_ids(m, p.id);
  m.color = app::c4().my_color;
  m.first = first;
  app::send(m);
  /* Defer rebuild — color-picker radials + sync go_c4 froze touch on both desks. */
  app::schedule(1, [](void * /*p*/) {
    /* Don't yank back if the user already hit Home. */
    const Screen s = current_screen();
    if (s == Screen::Hub || s == Screen::ActiveGames || s == Screen::Idle ||
        s == Screen::GamesFolder)
      return;
    go_c4();
  }, nullptr);
}

void c4_drop(lv_event_t * e) {
  const int col = (int)(intptr_t)lv_event_get_user_data(e);
  app::C4Game & g = app::c4();
  if (!g.active || g.waiting || g.over || g.turn != g.my_color) return;
  if (!guard_active_move(g.opp_id)) return;
  const int row = games::c4::drop(g.board, col, g.my_color);
  if (row < 0) return;
  g.last_r = (int8_t)row;
  g.last_c = (int8_t)col;
  proto::Msg m;
  m.type = proto::MsgType::C4Drop;
  fill_msg_ids(m, g.opp_id);
  m.col = (int8_t)col;
  m.color = g.my_color;
  app::send(m);
  const int w = games::c4::winner(g.board);
  if (w >= 0) {
    g.over = true;
    g.result_dismissed = false;
  } else {
    g.turn = g.opp_color;
  }
  go_c4();
}

/* Cached ARGB frame with circular cutouts so discs can sit behind it. */
constexpr int kC4Cell = 44, kC4Gap = 4, kC4Pad = 10;
constexpr int kC4BoardW = 7 * kC4Cell + 6 * kC4Gap + 2 * kC4Pad;
constexpr int kC4BoardH = 6 * kC4Cell + 5 * kC4Gap + 2 * kC4Pad;
constexpr int kC4FrameVer = 2; /* bump when paint algorithm changes */
uint8_t * g_c4_frame_buf = nullptr;
int g_c4_frame_painted_ver = 0;

void c4_punch_hole_aa(lv_draw_buf_t * db, float cx, float cy, float rad) {
  uint8_t * data = static_cast<uint8_t *>(db->data);
  const uint32_t stride = db->header.stride;
  constexpr float kAa = 1.25f;
  const int x0 = (int)std::floor(cx - rad - kAa);
  const int x1 = (int)std::ceil(cx + rad + kAa);
  const int y0 = (int)std::floor(cy - rad - kAa);
  const int y1 = (int)std::ceil(cy + rad + kAa);
  for (int y = y0; y <= y1; ++y) {
    if (y < 0 || y >= kC4BoardH) continue;
    for (int x = x0; x <= x1; ++x) {
      if (x < 0 || x >= kC4BoardW) continue;
      const float dx = (float)x + 0.5f - cx;
      const float dy = (float)y + 0.5f - cy;
      const float d = std::sqrt(dx * dx + dy * dy);
      float punch;
      if (d <= rad - kAa) punch = 1.f;
      else if (d >= rad + kAa) punch = 0.f;
      else punch = 1.f - (d - (rad - kAa)) / (2.f * kAa);
      if (punch <= 0.f) continue;
      auto * px = reinterpret_cast<lv_color32_t *>(data + (uint32_t)y * stride + (uint32_t)x * 4u);
      px->alpha = (uint8_t)((float)px->alpha * (1.f - punch) + 0.5f);
    }
  }
}

void c4_paint_frame(lv_obj_t * canvas) {
  lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  lv_draw_rect_dsc_t rd;
  lv_draw_rect_dsc_init(&rd);
  rd.bg_opa = LV_OPA_COVER;
  rd.radius = 16;
  rd.bg_grad = *c4_rainbow();
  rd.border_width = 3;
  rd.border_color = lv_color_hex(0x8d7fa0);
  rd.border_opa = LV_OPA_COVER;
  const lv_area_t full = {0, 0, kC4BoardW - 1, kC4BoardH - 1};
  lv_draw_rect(&layer, &rd, &full);
  lv_canvas_finish_layer(canvas, &layer);

  lv_draw_buf_t * db = lv_canvas_get_draw_buf(canvas);
  if (!db || !db->data) return;
  const float rad = (float)kC4Cell * 0.5f - 0.5f;
  for (int row = 0; row < 6; ++row) {
    for (int col = 0; col < 7; ++col) {
      const float cx = (float)kC4Pad + (float)col * (float)(kC4Cell + kC4Gap) + (float)kC4Cell * 0.5f;
      const float cy = (float)kC4Pad + (float)row * (float)(kC4Cell + kC4Gap) + (float)kC4Cell * 0.5f;
      c4_punch_hole_aa(db, cx, cy, rad);
    }
  }
  lv_obj_invalidate(canvas);
}

void fill_c4_play(lv_obj_t * parent) {
  app::C4Game & g = app::c4();
  const bool my_turn = !g.over && !g.waiting && g.turn == g.my_color;
  const int w = games::c4::winner(g.board);
  if (g.over && !g.result_dismissed) make_status(parent, "Game over");
  else if (g.over) make_status(parent, "Play again?");
  else if (my_turn) make_status(parent, "Your turn - tap a column");
  else {
    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Waiting on %s...", g.opp_name);
    make_status(parent, buf);
  }

  lv_obj_t * yourow = lv_obj_create(parent);
  lv_obj_remove_style_all(yourow);
  lv_obj_set_size(yourow, LV_SIZE_CONTENT, 28);
  lv_obj_set_flex_flow(yourow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(yourow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(yourow, 6, 0);
  lv_obj_remove_flag(yourow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * you = lv_label_create(yourow);
  lv_label_set_text(you, "you are");
  lv_obj_set_style_text_color(you, theme::muted(), 0);
  lv_obj_set_style_text_font(you, &lv_font_montserrat_12, 0);
  lv_obj_t * badge = lv_obj_create(yourow);
  lv_obj_remove_style_all(badge);
  lv_obj_set_size(badge, 22, 22);
  lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(badge, lv_color_hex(kDiscs[g.my_color >= 0 ? g.my_color % 6 : 0].main), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(badge, 2, 0);
  lv_obj_set_style_border_color(badge, lv_color_hex(0xf7f2ea), 0);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE);

  /* Discs behind a solid rainbow frame with punched circular holes. */
  lv_obj_t * wrap = lv_obj_create(parent);
  lv_obj_remove_style_all(wrap);
  lv_obj_set_size(wrap, kC4BoardW, kC4BoardH);
  lv_obj_set_style_radius(wrap, 16, 0);
  lv_obj_set_style_bg_color(wrap, lv_color_hex(0x1a1228), 0);
  lv_obj_set_style_bg_opa(wrap, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(wrap, false, 0);
  lv_obj_add_flag(wrap, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_remove_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * pieces = lv_obj_create(wrap);
  lv_obj_remove_style_all(pieces);
  lv_obj_set_pos(pieces, 0, 0);
  lv_obj_set_size(pieces, kC4BoardW, kC4BoardH);
  lv_obj_set_style_bg_opa(pieces, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(pieces, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_remove_flag(pieces, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(pieces, LV_OBJ_FLAG_SCROLLABLE);

  for (int r = 0; r < 6; ++r) {
    for (int c = 0; c < 7; ++c) {
      if (g.board[r][c] < 0) continue;
      const int x = kC4Pad + c * (kC4Cell + kC4Gap);
      const int y = kC4Pad + r * (kC4Cell + kC4Gap);
      lv_obj_t * disc = lv_obj_create(pieces);
      lv_obj_remove_style_all(disc);
      lv_obj_set_pos(disc, x, y);
      lv_obj_set_size(disc, kC4Cell, kC4Cell);
      lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
#ifdef WP_DEVICE
      lv_obj_set_style_bg_color(disc, lv_color_hex(kDiscs[g.board[r][c] % 6].main), 0);
#else
      lv_obj_set_style_bg_grad(disc, disc_grad(g.board[r][c]), 0);
#endif
      lv_obj_remove_flag(disc, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
      if (g.last_r == r && g.last_c == c) {
        const int drop_px = y + kC4Cell;
        lv_obj_set_style_translate_y(disc, -drop_px, 0);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, disc);
        lv_anim_set_values(&a, -drop_px, 0);
        lv_anim_set_duration(&a, 480 + (uint32_t)r * 55);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a, [](void * obj, int32_t v) {
          lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(obj), v, 0);
        });
        lv_anim_set_completed_cb(&a, [](lv_anim_t * /*a*/) {
          app::c4().last_r = -1;
          app::c4().last_c = -1;
        });
        lv_anim_start(&a);
      }
    }
  }

  if (!g_c4_frame_buf) {
    const size_t n = (size_t)kC4BoardW * (size_t)kC4BoardH * 4;
#ifdef WP_DEVICE
    g_c4_frame_buf = static_cast<uint8_t *>(
        heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_c4_frame_buf) g_c4_frame_buf = static_cast<uint8_t *>(malloc(n));
#else
    g_c4_frame_buf = static_cast<uint8_t *>(lv_malloc(n));
#endif
  }
  if (!g_c4_frame_buf) {
    make_status(parent, "Board OOM");
    return;
  }
  lv_obj_t * frame = lv_canvas_create(wrap);
  lv_obj_set_pos(frame, 0, 0);
  lv_obj_set_size(frame, kC4BoardW, kC4BoardH);
  lv_canvas_set_buffer(frame, g_c4_frame_buf, kC4BoardW, kC4BoardH, LV_COLOR_FORMAT_ARGB8888);
  lv_obj_remove_flag(frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
  if (g_c4_frame_buf && g_c4_frame_painted_ver != kC4FrameVer) {
    c4_paint_frame(frame);
    g_c4_frame_painted_ver = kC4FrameVer;
  }

  if (my_turn) {
    for (int c = 0; c < 7; ++c) {
      if (g.board[0][c] >= 0) continue;
      lv_obj_t * hit = lv_obj_create(wrap);
      lv_obj_remove_style_all(hit);
      lv_obj_set_pos(hit, kC4Pad + c * (kC4Cell + kC4Gap), 0);
      lv_obj_set_size(hit, kC4Cell, kC4BoardH);
      lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
      lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_event_cb(hit, c4_drop, LV_EVENT_CLICKED, (void *)(intptr_t)c);
    }
  }

  if (g.over && !g.result_dismissed) {
    const int outcome = (w == games::c4::kColorCount) ? -1 : (w == g.my_color ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::c4().result_dismissed = true;
      go_c4();
    }, "Connect Four", g.opp_name);
  }
}

lv_obj_t * game_c4_build(lv_obj_t * into) {
  app::Desk & d = app::desk();
  lv_obj_t * scr = into;
  if (scr) lv_obj_clean(scr);
  else scr = make_screen();
  char sub[40];
  make_topbar(scr, "CONNECT FOUR", d.name,
              game_vs_sub(sub, sizeof(sub), app::c4().active, app::c4().opp_name, app::invite_active(app::GameKind::C4),
                          app::invite_ref(app::GameKind::C4).from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (app::invite_active(app::GameKind::C4)) {
    make_wait_block(body, "GAME INVITE", app::invite_ref(app::GameKind::C4).from_name, "wants to play Connect Four");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::C4Decline;
      fill_msg_ids(m, app::invite_ref(app::GameKind::C4).from_id);
      app::send(m);
      app::end_focused();
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = app::invite_ref(app::GameKind::C4);
      app::accept_invite(app::GameKind::C4);
      app::c4() = {};
      app::c4().active = true;
      app::c4().opp_color = inv.color >= 0 ? inv.color : 0;
      app::c4().my_color = app::c4().opp_color == 0 ? 1 : 0;
      app::c4().turn = inv.first ? app::c4().opp_color : app::c4().my_color;
      games::c4::init(app::c4().board);
      std::snprintf(app::c4().opp_id, sizeof(app::c4().opp_id), "%s", inv.from_id);
      std::snprintf(app::c4().opp_name, sizeof(app::c4().opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::C4Accept;
      fill_msg_ids(m, inv.from_id);
      m.color = app::c4().my_color;
      app::send(m);
      app::schedule(1, [](void * /*p*/) {
        const Screen s = current_screen();
        if (s == Screen::Hub || s == Screen::ActiveGames || s == Screen::Idle ||
            s == Screen::GamesFolder)
          return;
        go_c4();
      }, nullptr);
    });
  } else if (app::c4().active && app::c4().waiting) {
    make_wait_block(body, "CHALLENGE SENT", app::c4().opp_name, "Waiting for them to accept...");
    dock_cancel_home(dock);
  } else if (app::c4().active) {
    fill_c4_play(body);
    if (app::c4().over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            const int8_t color = app::c4().my_color;
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", app::c4().opp_id);
            std::snprintf(oname, sizeof(oname), "%s", app::c4().opp_name);
            app::c4() = {};
            app::c4().active = true;
            app::c4().waiting = true;
            const bool first = app::roll_first();
            app::c4().first = first;
            app::c4().my_color = color;
            games::c4::init(app::c4().board);
            std::snprintf(app::c4().opp_id, sizeof(app::c4().opp_id), "%s", oid);
            std::snprintf(app::c4().opp_name, sizeof(app::c4().opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::C4Invite;
            fill_msg_ids(m, oid);
            m.color = color;
            m.first = first;
            app::send(m);
            go_c4();
          },
          [](lv_event_t * /*e*/) {
            app::end_focused();
            go_hub();
          });
    } else {
      dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
          score_log::note("Connect Four", app::c4().opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::C4Forfeit;
          fill_msg_ids(m, app::c4().opp_id);
          app::send(m);
          app::end_focused();
          go_hub();
      });
    }
  } else {
    make_tagline(body, "Your disc color");
    lv_obj_t * colors = lv_obj_create(body);
    lv_obj_remove_style_all(colors);
    lv_obj_set_width(colors, lv_pct(100));
    lv_obj_set_height(colors, 44);
    lv_obj_set_flex_flow(colors, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(colors, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (int i = 0; i < 6; ++i) {
      lv_obj_t * sw = lv_obj_create(colors);
      lv_obj_remove_style_all(sw);
      lv_obj_set_size(sw, 36, 36);
      lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
#ifdef WP_DEVICE
      /* Solid discs — radial grads on six swatches froze glass after invite. */
      lv_obj_set_style_bg_color(sw, lv_color_hex(kDiscs[i].main), 0);
#else
      lv_obj_set_style_bg_grad(sw, disc_grad(i), 0);
#endif
      lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(sw, g_c4_pick_color == i ? 3 : 0, 0);
      lv_obj_set_style_border_color(sw, theme::ink(), 0);
      lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(
          sw,
          [](lv_event_t * e) {
            g_c4_pick_color = (int)(intptr_t)lv_event_get_user_data(e);
            go_c4();
          },
          LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    peer_list(body, c4_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

/* ================= BATTLESHIP ================= */

void bs_challenge(lv_event_t * e) {
  if (!guard_peer_challenge(e)) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];
  if (!begin_kind(app::GameKind::Bs, p)) return;
  app::bs() = {};
  app::bs().active = true;
  app::bs().waiting = true;
  const bool first = app::roll_first();
  app::bs().i_am_first = first;
  std::snprintf(app::bs().opp_id, sizeof(app::bs().opp_id), "%s", p.id);
  std::snprintf(app::bs().opp_name, sizeof(app::bs().opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::BsInvite;
  fill_msg_ids(m, p.id);
  m.first = first;
  app::send(m);
  go_battleship();
}

void style_ship_segment(lv_obj_t * cell, const games::bs::Ship & ship, int seg) {
  const bool bow = seg == 0;
  const bool stern = seg == (int)ship.len - 1;
  const int bridge = ((int)ship.len - 1) / 2;
  lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(cell, 0, 0);
  /* Hull body — lighter top-left like web .bs-cell.ship */
  lv_obj_set_style_bg_color(cell, lv_color_hex(0xb0b8c4), 0);
  lv_obj_set_style_bg_grad_color(cell, lv_color_hex(0x5a6472), 0);
  lv_obj_set_style_bg_grad_dir(cell, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_radius(cell, 3, 0);
  if (bow) {
    if (ship.horiz) {
      lv_obj_set_style_radius(cell, 14, 0); /* tip on the right visually via gradient */
      lv_obj_set_style_bg_color(cell, lv_color_hex(0x7a8494), 0);
      lv_obj_set_style_bg_grad_color(cell, lv_color_hex(0xc0c8d4), 0);
      lv_obj_set_style_bg_grad_dir(cell, LV_GRAD_DIR_HOR, 0);
    } else {
      lv_obj_set_style_radius(cell, 14, 0);
      lv_obj_set_style_bg_color(cell, lv_color_hex(0xc0c8d4), 0);
      lv_obj_set_style_bg_grad_color(cell, lv_color_hex(0x7a8494), 0);
      lv_obj_set_style_bg_grad_dir(cell, LV_GRAD_DIR_VER, 0);
    }
  } else if (stern) {
    lv_obj_set_style_radius(cell, ship.horiz ? 6 : 6, 0);
  }
  if (seg == bridge) {
    lv_obj_t * br = lv_obj_create(cell);
    lv_obj_remove_style_all(br);
    lv_obj_set_size(br, ship.horiz ? 12 : 10, ship.horiz ? 10 : 12);
    lv_obj_set_style_bg_color(br, lv_color_hex(0x3a4250), 0);
    lv_obj_set_style_bg_grad_color(br, lv_color_hex(0x1e2430), 0);
    lv_obj_set_style_bg_grad_dir(br, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(br, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(br, 2, 0);
    lv_obj_center(br);
    lv_obj_remove_flag(br, LV_OBJ_FLAG_CLICKABLE);
  } else if (!bow && !stern) {
    lv_obj_t * deck = lv_obj_create(cell);
    lv_obj_remove_style_all(deck);
    lv_obj_set_size(deck, ship.horiz ? 14 : 3, ship.horiz ? 3 : 14);
    lv_obj_set_style_bg_color(deck, lv_color_hex(0x283040), 0);
    lv_obj_set_style_bg_opa(deck, LV_OPA_70, 0);
    lv_obj_center(deck);
    lv_obj_remove_flag(deck, LV_OBJ_FLAG_CLICKABLE);
  }
}

/** Find ship index covering (x,y), and segment along that ship. */
bool ship_seg_at(const games::bs::Fleet & f, int x, int y, int * ship_out, int * seg_out) {
  const int idx = games::bs::ship_at(f, x, y);
  if (idx < 0) return false;
  const games::bs::Ship & s = f.ships[idx];
  for (int i = 0; i < s.len; ++i) {
    const int xx = s.horiz ? s.x + i : s.x;
    const int yy = s.horiz ? s.y : s.y + i;
    if (xx == x && yy == y) {
      if (ship_out) *ship_out = idx;
      if (seg_out) *seg_out = i;
      return true;
    }
  }
  return false;
}

void bs_setup_tap(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  const int x = packed % 10, y = packed / 10;
  app::BsGame & g = app::bs();
  if (!g.setup || g.me_ready) return;

  const int existing = games::bs::ship_at(g.fleet, x, y);
  if (existing >= 0) {
    g.anchor_x = g.anchor_y = -1;
    g.selected_ship = (g.selected_ship == existing) ? (int8_t)-1 : (int8_t)existing;
    go_battleship();
    return;
  }
  g.selected_ship = -1;

  const int si = games::bs::next_ship_index(g.fleet);
  const bool all = si >= games::bs::kShipCount;
  if (all) {
    toast("Tap a ship to remove, or Ready");
    return;
  }
  const int len = games::bs::ship_specs()[si].len;

  if (g.anchor_x >= 0) {
    games::bs::Placement opts[4];
    const int n = games::bs::placement_options_from_anchor(g.fleet, g.anchor_x, g.anchor_y, len, opts);
    /* Which options cover this cell? */
    games::bs::Placement chosen{};
    int matches = 0;
    for (int i = 0; i < n; ++i) {
      for (int k = 0; k < len; ++k) {
        const int xx = opts[i].horiz ? opts[i].x + k : opts[i].x;
        const int yy = opts[i].horiz ? opts[i].y : opts[i].y + k;
        if (xx == x && yy == y) {
          chosen = opts[i];
          matches++;
          break;
        }
      }
    }
    if (matches) {
      const bool is_anchor = g.anchor_x == x && g.anchor_y == y;
      if (is_anchor && matches > 1) {
        g.anchor_x = g.anchor_y = -1;
        go_battleship();
        return;
      }
      /* Prefer placement where tap is tip/start when ambiguous */
      if (matches > 1) {
        for (int i = 0; i < n; ++i) {
          const int tipx = opts[i].horiz ? opts[i].x + len - 1 : opts[i].x;
          const int tipy = opts[i].horiz ? opts[i].y : opts[i].y + len - 1;
          if ((tipx == x && tipy == y) || (opts[i].x == x && opts[i].y == y)) {
            chosen = opts[i];
            break;
          }
        }
      }
      games::bs::place_ship(g.fleet, si, chosen.x, chosen.y, chosen.horiz);
      g.anchor_x = g.anchor_y = -1;
      go_battleship();
      return;
    }
    if (g.anchor_x == x && g.anchor_y == y) {
      g.anchor_x = g.anchor_y = -1;
    } else {
      g.anchor_x = (int8_t)x;
      g.anchor_y = (int8_t)y;
    }
    go_battleship();
    return;
  }

  g.anchor_x = (int8_t)x;
  g.anchor_y = (int8_t)y;
  go_battleship();
}

void bs_fire_cell(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  const int x = packed % 10, y = packed / 10;
  app::BsGame & g = app::bs();
  if (g.setup || !g.my_turn || g.over || g.tracking[y][x]) return;
  if (!guard_active_move(g.opp_id)) return;
  proto::Msg m;
  m.type = proto::MsgType::BsFire;
  fill_msg_ids(m, g.opp_id);
  m.x = (int8_t)x;
  m.y = (int8_t)y;
  g.my_turn = false;
  std::snprintf(g.last_msg, sizeof(g.last_msg), "Firing...");
  app::send(m);
  go_battleship();
}

void fill_bs_grid(lv_obj_t * parent, bool offense) {
  app::BsGame & g = app::bs();
  /* Grow cells into leftover body space (status / Offense·Defense sit above). */
  constexpr int kGap = 1;
  lv_obj_update_layout(parent);
  const int avail_w = (int)lv_obj_get_content_width(parent);
  const int content_h = (int)lv_obj_get_content_height(parent);
  const int pad_row = (int)lv_obj_get_style_pad_row(parent, LV_PART_MAIN);
  int used = 0;
  const uint32_t n = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < n; ++i) used += (int)lv_obj_get_height(lv_obj_get_child(parent, i));
  if (n > 0) used += pad_row * (int)n; /* gaps between siblings + before board */
  int avail_h = content_h - used - 2;
  if (avail_h < 200) avail_h = 200;
  int kCell = (avail_w - 9 * kGap) / 10;
  const int cell_h = (avail_h - 9 * kGap) / 10;
  if (cell_h < kCell) kCell = cell_h;
  if (kCell < 28) kCell = 28;
  if (kCell > 40) kCell = 40;

  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 10 * kCell + 9 * kGap, 10 * kCell + 9 * kGap);
  lv_obj_set_style_bg_color(board, lv_color_hex(0x0a1628), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(board, 8, 0);
  lv_obj_set_style_pad_all(board, 0, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  /* Ghost preview cells for setup placement */
  bool ghost[10][10] = {};
  if (!offense && g.setup && !g.me_ready && g.anchor_x >= 0) {
    const int si = games::bs::next_ship_index(g.fleet);
    if (si < games::bs::kShipCount) {
      games::bs::Placement opts[4];
      const int n = games::bs::placement_options_from_anchor(g.fleet, g.anchor_x, g.anchor_y,
                                                            games::bs::ship_specs()[si].len, opts);
      for (int i = 0; i < n; ++i) {
        const int len = games::bs::ship_specs()[si].len;
        for (int k = 0; k < len; ++k) {
          const int xx = opts[i].horiz ? opts[i].x + k : opts[i].x;
          const int yy = opts[i].horiz ? opts[i].y : opts[i].y + k;
          if (xx >= 0 && xx < 10 && yy >= 0 && yy < 10) ghost[yy][xx] = true;
        }
      }
    }
  }

  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 10; ++x) {
      lv_obj_t * cell = lv_obj_create(board);
      lv_obj_remove_style_all(cell);
      lv_obj_set_size(cell, kCell, kCell);
      lv_obj_set_pos(cell, x * (kCell + kGap), y * (kCell + kGap));
      lv_obj_set_style_radius(cell, 3, 0);
      lv_obj_set_style_bg_color(cell, lv_color_hex(0x143048), 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
      const int packed = y * 10 + x;

      if (offense) {
        if (g.tracking[y][x] == 1) {
          lv_obj_set_style_bg_color(cell, theme::hot(), 0);
        } else if (g.tracking[y][x] == 2) {
          lv_obj_set_style_bg_color(cell, theme::muted(), 0);
        } else if (g.my_turn && !g.over) {
          lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_add_event_cb(cell, bs_fire_cell, LV_EVENT_CLICKED, (void *)(intptr_t)packed);
        }
      } else {
        int ship_i = -1, seg = 0;
        if (ship_seg_at(g.fleet, x, y, &ship_i, &seg)) {
          style_ship_segment(cell, g.fleet.ships[ship_i], seg);
          if (g.selected_ship == ship_i) {
            lv_obj_set_style_border_width(cell, 2, 0);
            lv_obj_set_style_border_color(cell, theme::gold(), 0);
          }
        } else if (g.fleet.grid[y][x] == games::bs::kHitCell) {
          lv_obj_set_style_bg_color(cell, theme::hot(), 0);
        }
        if (g.fleet_miss[y][x]) lv_obj_set_style_bg_color(cell, theme::muted(), 0);
        if (ghost[y][x]) {
          lv_obj_set_style_bg_color(cell, theme::mint(), 0);
          lv_obj_set_style_bg_opa(cell, LV_OPA_50, 0);
        }
        if (g.anchor_x == x && g.anchor_y == y) {
          lv_obj_set_style_border_width(cell, 2, 0);
          lv_obj_set_style_border_color(cell, theme::gold(), 0);
          lv_obj_set_style_bg_color(cell, theme::gold(), 0);
          lv_obj_set_style_bg_opa(cell, LV_OPA_40, 0);
        }
        if (g.setup && !g.me_ready) {
          lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_add_event_cb(cell, bs_setup_tap, LV_EVENT_CLICKED, (void *)(intptr_t)packed);
        }
      }
    }
  }
}

lv_obj_t * game_bs_build(lv_obj_t * into) {
  app::Desk & d = app::desk();
  lv_obj_t * scr = into;
  if (scr) lv_obj_clean(scr);
  else scr = make_screen();
  char sub[40];
  make_topbar(scr, "BATTLESHIP", d.name,
              game_vs_sub(sub, sizeof(sub), app::bs().active, app::bs().opp_name, app::invite_active(app::GameKind::Bs),
                          app::invite_ref(app::GameKind::Bs).from_name));
  lv_obj_t * body = make_body(scr, true);
  /* Pull status + tabs up so the 10×10 board can grow. */
  lv_obj_set_style_pad_top(body, 0, 0);
  lv_obj_set_style_pad_row(body, 2, 0);
  lv_obj_set_style_pad_bottom(body, 2, 0);
  lv_obj_t * dock = make_dock(scr);

  if (app::invite_active(app::GameKind::Bs)) {
    make_wait_block(body, "GAME INVITE", app::invite_ref(app::GameKind::Bs).from_name, "wants to play Battleship");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::BsDecline;
      fill_msg_ids(m, app::invite_ref(app::GameKind::Bs).from_id);
      app::send(m);
      app::end_focused();
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = app::invite_ref(app::GameKind::Bs);
      app::accept_invite(app::GameKind::Bs);
      app::bs() = {};
      app::bs().active = true;
      app::bs().setup = true;
      app::bs().i_am_first = !inv.first;
      games::bs::clear_fleet(app::bs().fleet);
      std::snprintf(app::bs().opp_id, sizeof(app::bs().opp_id), "%s", inv.from_id);
      std::snprintf(app::bs().opp_name, sizeof(app::bs().opp_name), "%s", inv.from_name);
      std::snprintf(app::bs().last_msg, sizeof(app::bs().last_msg), "Place your fleet");
      proto::Msg m;
      m.type = proto::MsgType::BsAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_battleship();
    });
  } else if (app::bs().active && app::bs().waiting) {
    make_wait_block(body, "CHALLENGE SENT", app::bs().opp_name, "Waiting for them to accept...");
    dock_cancel_home(dock);
  } else if (app::bs().active && app::bs().setup) {
    const int next = games::bs::next_ship_index(app::bs().fleet);
    if (app::bs().me_ready) {
      make_status(body, app::bs().last_msg[0] ? app::bs().last_msg : "Waiting for opponent fleet...");
    } else if (next < games::bs::kShipCount) {
      char buf[72];
      if (app::bs().anchor_x >= 0) {
        lv_snprintf(buf, sizeof(buf), "Tap a highlighted square to place %s (%d)",
                    games::bs::ship_specs()[next].name, games::bs::ship_specs()[next].len);
      } else {
        lv_snprintf(buf, sizeof(buf), "Tap a square to start %s (%d)",
                    games::bs::ship_specs()[next].name, games::bs::ship_specs()[next].len);
      }
      make_status(body, buf);
    } else {
      make_status(body, "Fleet set - Ready, or tap a ship to remove");
    }
    fill_bs_grid(body, false);
    if (!app::bs().me_ready) {
      if (app::bs().selected_ship >= 0) {
        dock_btn(dock, "Remove", false, true, [](lv_event_t * /*e*/) {
          app::BsGame & g = app::bs();
          if (g.selected_ship >= 0) {
            games::bs::remove_ship(g.fleet, g.selected_ship);
            g.selected_ship = -1;
          }
          go_battleship();
        });
      }
      dock_btn(dock, "Random", false, false, [](lv_event_t * /*e*/) {
        app::BsGame & g = app::bs();
        games::bs::random_fleet(g.fleet);
        g.anchor_x = g.anchor_y = -1;
        g.selected_ship = -1;
        go_battleship();
      });
      if (games::bs::placed_count(app::bs().fleet) == games::bs::kShipCount) {
        dock_btn(dock, "Ready", true, false, [](lv_event_t * /*e*/) {
          app::BsGame & g = app::bs();
          g.me_ready = true;
          std::snprintf(g.last_msg, sizeof(g.last_msg), "Waiting for opponent fleet...");
          proto::Msg m;
          m.type = proto::MsgType::BsReady;
          fill_msg_ids(m, g.opp_id);
          app::send(m);
          if (g.opp_ready) {
            g.setup = false;
            g.my_turn = g.i_am_first;
            std::snprintf(g.last_msg, sizeof(g.last_msg),
                          g.my_turn ? "Your turn - tap to fire!" : "Enemy turn...");
            g.mode = g.my_turn ? 0 : 1;
          }
          go_battleship();
        });
      }
    }
    dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
app::Desk & desk = app::desk();
        score_log::note("Battleship", app::bs().opp_name, score_log::Outcome::ForfeitSelf);
        proto::Msg m;
        m.type = proto::MsgType::BsForfeit;
        fill_msg_ids(m, app::bs().opp_id);
        app::send(m);
        app::end_focused();
        go_hub();
      });
  } else if (app::bs().active) {
    make_status(body, app::bs().over ? (app::bs().result_dismissed ? "Play again?" : "Game over")
                                : (app::bs().last_msg[0] ? app::bs().last_msg
                                                    : (app::bs().my_turn ? "Your turn" : "Enemy turn")));
    lv_obj_t * tabs = lv_obj_create(body);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_width(tabs, lv_pct(100));
    lv_obj_set_height(tabs, 24);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tabs, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(tabs, 8, 0);
    lv_obj_remove_flag(tabs, LV_OBJ_FLAG_SCROLLABLE);
    auto tab = [&](const char * label, int mode) {
      lv_obj_t * b = lv_button_create(tabs);
      lv_obj_set_flex_grow(b, 1);
      lv_obj_set_height(b, 24);
      lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_pad_all(b, 0, 0);
      lv_obj_set_style_pad_ver(b, 0, 0);
      lv_obj_set_style_bg_color(b, app::bs().mode == mode ? theme::gold() : theme::panel(), 0);
      lv_obj_set_style_shadow_width(b, 0, 0);
      lv_obj_t * l = lv_label_create(b);
      lv_label_set_text(l, label);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, app::bs().mode == mode ? lv_color_hex(0x1a1200) : theme::ink(), 0);
      lv_obj_center(l);
      lv_obj_add_event_cb(
          b,
          [](lv_event_t * e) {
            app::bs().mode = (uint8_t)(intptr_t)lv_event_get_user_data(e);
            go_battleship();
          },
          LV_EVENT_CLICKED, (void *)(intptr_t)mode);
    };
    tab("Offense", 0);
    tab("Defense", 1);
    fill_bs_grid(body, app::bs().mode == 0);
    if (app::bs().over && !app::bs().result_dismissed) {
      attach_result_overlay(body, app::bs().i_won ? 1 : 0, [](lv_event_t * /*e*/) {
        app::bs().result_dismissed = true;
        go_battleship();
      }, "Battleship", app::bs().opp_name);
    }
    if (app::bs().over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", app::bs().opp_id);
            std::snprintf(oname, sizeof(oname), "%s", app::bs().opp_name);
            app::bs() = {};
            app::bs().active = true;
            app::bs().waiting = true;
            const bool first = app::roll_first();
            app::bs().i_am_first = first;
            std::snprintf(app::bs().opp_id, sizeof(app::bs().opp_id), "%s", oid);
            std::snprintf(app::bs().opp_name, sizeof(app::bs().opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::BsInvite;
            fill_msg_ids(m, oid);
            m.first = first;
            app::send(m);
            go_battleship();
          },
          [](lv_event_t * /*e*/) {
            app::end_focused();
            go_hub();
          });
    } else {
      dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
          score_log::note("Battleship", app::bs().opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::BsForfeit;
          fill_msg_ids(m, app::bs().opp_id);
          app::send(m);
          app::end_focused();
          go_hub();
      });
    }
  } else {
    peer_list(body, bs_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

/* ================= CHECKERS ================= */

void ck_challenge(lv_event_t * e) {
  if (!guard_peer_challenge(e)) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];
  if (!begin_kind(app::GameKind::Ck, p)) return;
  app::ck() = {};
  app::ck().active = true;
  app::ck().waiting = true;
  const bool first = app::roll_first();
  app::ck().side = first ? 'r' : 'b';
  app::ck().turn = 'r';
  games::ck::init(app::ck().board);
  std::snprintf(app::ck().opp_id, sizeof(app::ck().opp_id), "%s", p.id);
  std::snprintf(app::ck().opp_name, sizeof(app::ck().opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::CkInvite;
  fill_msg_ids(m, p.id);
  m.first = first;
  app::send(m);
  go_checkers();
}

void ck_tap(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  int vx = packed % 8, vy = packed / 8;
  app::CkGame & g = app::ck();
  if (!g.active || g.waiting || g.over || g.turn != g.side) return;
  if (!guard_active_move(g.opp_id)) return;

  /* view transform: black sees board flipped so own pieces at bottom */
  int x = vx, y = vy;
  if (g.side == 'b') {
    x = 7 - vx;
    y = 7 - vy;
  }

  games::ck::Move moves[64];
  const int from_x = g.must_x >= 0 ? g.must_x : g.sel_x;
  const int from_y = g.must_y >= 0 ? g.must_y : g.sel_y;
  const int n = games::ck::legal_moves(g.board, g.side, from_x, from_y, moves, 64);

  /* If selecting destination */
  if (g.sel_x >= 0 || g.must_x >= 0) {
    for (int i = 0; i < n; ++i) {
      if (moves[i].tx == x && moves[i].ty == y) {
        games::ck::apply_move(g.board, moves[i]);
        proto::Msg m;
        m.type = proto::MsgType::CkMove;
        fill_msg_ids(m, g.opp_id);
        m.from_x = moves[i].fx;
        m.from_y = moves[i].fy;
        m.to_x = moves[i].tx;
        m.to_y = moves[i].ty;
        app::send(m);

        if (moves[i].jump) {
          games::ck::Move more[16];
          const int nm = games::ck::legal_moves(g.board, g.side, moves[i].tx, moves[i].ty, more, 16);
          bool has_jump = false;
          for (int j = 0; j < nm; ++j)
            if (more[j].jump) has_jump = true;
          if (has_jump) {
            g.must_x = moves[i].tx;
            g.must_y = moves[i].ty;
            g.sel_x = moves[i].tx;
            g.sel_y = moves[i].ty;
            go_checkers();
            return;
          }
        }
        g.must_x = g.must_y = g.sel_x = g.sel_y = -1;
        g.turn = g.side == 'r' ? 'b' : 'r';
        games::ck::Move any[64];
        if (games::ck::count_side(g.board, 'r') == 0 || games::ck::count_side(g.board, 'b') == 0 ||
            games::ck::legal_moves(g.board, g.turn, -1, -1, any, 64) == 0) {
          g.over = true;
          g.result_dismissed = false;
        }
        go_checkers();
        return;
      }
    }
    /* reselect if own piece */
    if (g.must_x < 0 && games::ck::side_of(g.board[y][x]) == g.side) {
      g.sel_x = (int8_t)x;
      g.sel_y = (int8_t)y;
      go_checkers();
    }
    return;
  }

  if (games::ck::side_of(g.board[y][x]) == g.side) {
    g.sel_x = (int8_t)x;
    g.sel_y = (int8_t)y;
    go_checkers();
  }
}

void fill_ck_play(lv_obj_t * parent) {
  app::CkGame & g = app::ck();
  const bool my_turn = !g.over && !g.waiting && g.turn == g.side;
  if (g.over && !g.result_dismissed) make_status(parent, "Game over");
  else if (g.over) make_status(parent, "Play again?");
  else if (my_turn) make_status(parent, g.must_x >= 0 ? "Continue jump!" : "Your turn");
  else {
    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Waiting on %s...", g.opp_name);
    make_status(parent, buf);
  }

  /* Grow squares into leftover body space under the status line. */
  lv_obj_update_layout(parent);
  const int avail_w = (int)lv_obj_get_content_width(parent);
  const int content_h = (int)lv_obj_get_content_height(parent);
  const int pad_row = (int)lv_obj_get_style_pad_row(parent, LV_PART_MAIN);
  int used = 0;
  const uint32_t n = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < n; ++i) used += (int)lv_obj_get_height(lv_obj_get_child(parent, i));
  if (n > 0) used += pad_row * (int)n;
  int avail_h = content_h - used - 2;
  if (avail_h < 240) avail_h = 240;
  int kCell = avail_w / 8;
  const int cell_h = avail_h / 8;
  if (cell_h < kCell) kCell = cell_h;
  if (kCell < 40) kCell = 40;
  if (kCell > 52) kCell = 52;
  const int piece_sz = (kCell * 28) / 40;
  const lv_font_t * king_font =
      piece_sz >= 34 ? &lv_font_montserrat_14 : &lv_font_montserrat_12;

  /* High-contrast mauve board; mint + soft rose pieces (easy to read). */
  const lv_color_t sq_light = lv_color_mix(theme::ink(), theme::panel(), LV_OPA_30);
  const lv_color_t sq_dark = theme::bg0();
  const lv_color_t piece_rose = lv_color_mix(theme::hot(), theme::ink(), LV_OPA_70);
  const lv_color_t piece_mint = theme::mint();
  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 8 * kCell, 8 * kCell);
  lv_obj_set_style_radius(board, 14, 0);
  lv_obj_set_style_clip_corner(board, true, 0);
  lv_obj_set_style_border_width(board, 2, 0);
  lv_obj_set_style_border_color(board, theme::border(), 0);
  lv_obj_set_style_bg_color(board, theme::bg1(), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  for (int vy = 0; vy < 8; ++vy) {
    for (int vx = 0; vx < 8; ++vx) {
      int x = vx, y = vy;
      if (g.side == 'b') {
        x = 7 - vx;
        y = 7 - vy;
      }
      lv_obj_t * cell = lv_obj_create(board);
      lv_obj_remove_style_all(cell);
      lv_obj_set_size(cell, kCell, kCell);
      lv_obj_set_pos(cell, vx * kCell, vy * kCell);
      const bool dark = (x + y) % 2 == 1;
      lv_obj_set_style_bg_color(cell, dark ? sq_dark : sq_light, 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
      if (g.sel_x == x && g.sel_y == y) {
        lv_obj_set_style_border_width(cell, 2, 0);
        lv_obj_set_style_border_color(cell, theme::gold(), 0);
      }
      const char p = g.board[y][x];
      if (p) {
        lv_obj_t * piece = lv_obj_create(cell);
        lv_obj_remove_style_all(piece);
        lv_obj_set_size(piece, piece_sz, piece_sz);
        lv_obj_set_style_radius(piece, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(piece, LV_OPA_COVER, 0);
        const bool red = games::ck::is_red(p);
        lv_obj_set_style_bg_color(piece, red ? piece_rose : piece_mint, 0);
        if (games::ck::is_king(p)) {
          lv_obj_t * k = lv_label_create(piece);
          lv_label_set_text(k, "K");
          lv_obj_set_style_text_color(k, theme::bg0(), 0);
          lv_obj_set_style_text_font(k, king_font, 0);
          lv_obj_center(k);
        }
        lv_obj_center(piece);
        lv_obj_remove_flag(piece, LV_OBJ_FLAG_CLICKABLE);
      }
      if (my_turn && dark) {
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cell, ck_tap, LV_EVENT_CLICKED, (void *)(intptr_t)(vy * 8 + vx));
      }
    }
  }

  if (g.over && !g.result_dismissed) {
    const char opp = g.side == 'r' ? 'b' : 'r';
    bool i_won = games::ck::count_side(g.board, opp) == 0;
    if (!i_won && games::ck::count_side(g.board, g.side) > 0) {
      games::ck::Move any[64];
      if (games::ck::legal_moves(g.board, g.turn, -1, -1, any, 64) == 0) i_won = (g.turn != g.side);
    }
    attach_result_overlay(parent, i_won ? 1 : 0, [](lv_event_t * /*e*/) {
      app::ck().result_dismissed = true;
      go_checkers();
    }, "Checkers", g.opp_name);
  }
}

lv_obj_t * game_ck_build(lv_obj_t * into) {
  app::Desk & d = app::desk();
  lv_obj_t * scr = into;
  if (scr) lv_obj_clean(scr);
  else scr = make_screen();
  char sub[40];
  make_topbar(scr, "CHECKERS", d.name,
              game_vs_sub(sub, sizeof(sub), app::ck().active, app::ck().opp_name, app::invite_active(app::GameKind::Ck),
                          app::invite_ref(app::GameKind::Ck).from_name));
  lv_obj_t * body = make_body(scr, true);
  /* Pull status up so the 8×8 board can grow. */
  lv_obj_set_style_pad_top(body, 0, 0);
  lv_obj_set_style_pad_row(body, 2, 0);
  lv_obj_set_style_pad_bottom(body, 2, 0);
  lv_obj_t * dock = make_dock(scr);

  if (app::invite_active(app::GameKind::Ck)) {
    make_wait_block(body, "GAME INVITE", app::invite_ref(app::GameKind::Ck).from_name, "wants to play Checkers");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::CkDecline;
      fill_msg_ids(m, app::invite_ref(app::GameKind::Ck).from_id);
      app::send(m);
      app::end_focused();
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = app::invite_ref(app::GameKind::Ck);
      app::accept_invite(app::GameKind::Ck);
      app::ck() = {};
      app::ck().active = true;
      app::ck().side = inv.first ? 'b' : 'r';
      app::ck().turn = 'r';
      games::ck::init(app::ck().board);
      std::snprintf(app::ck().opp_id, sizeof(app::ck().opp_id), "%s", inv.from_id);
      std::snprintf(app::ck().opp_name, sizeof(app::ck().opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::CkAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_checkers();
    });
  } else if (app::ck().active && app::ck().waiting) {
    make_wait_block(body, "CHALLENGE SENT", app::ck().opp_name, "Waiting for them to accept...");
    dock_cancel_home(dock);
  } else if (app::ck().active) {
    fill_ck_play(body);
    if (app::ck().over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", app::ck().opp_id);
            std::snprintf(oname, sizeof(oname), "%s", app::ck().opp_name);
            app::ck() = {};
            app::ck().active = true;
            app::ck().waiting = true;
            const bool first = app::roll_first();
            app::ck().side = first ? 'r' : 'b';
            app::ck().turn = 'r';
            games::ck::init(app::ck().board);
            std::snprintf(app::ck().opp_id, sizeof(app::ck().opp_id), "%s", oid);
            std::snprintf(app::ck().opp_name, sizeof(app::ck().opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::CkInvite;
            fill_msg_ids(m, oid);
            m.first = first;
            app::send(m);
            go_checkers();
          },
          [](lv_event_t * /*e*/) {
            app::end_focused();
            go_hub();
          });
    } else {
      dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
          score_log::note("Checkers", app::ck().opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::CkForfeit;
          fill_msg_ids(m, app::ck().opp_id);
          app::send(m);
          app::end_focused();
          go_hub();
      });
    }
  } else {
    peer_list(body, ck_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

/* ================= MEMORY ================= */

/** LVGL "S:..." paths for the 8 pairs of the active match (seed-picked). */
std::string g_mem_faces[games::mem::kPairs];
uint32_t g_mem_faces_seed = 0;
bool g_mem_faces_bound = false;

#ifdef WP_DEVICE
/** Stems from the baked RGB565 pack — no FS decode on flip. */
std::vector<std::string> mem_scan_pool() {
  std::vector<std::string> out;
  const int n = memory_pack::count();
  out.reserve((size_t)n);
  for (int i = 0; i < n; ++i) {
    const char * stem = memory_pack::stem_at(i);
    if (stem && stem[0]) out.push_back(stem);
  }
  return out;
}
#else
std::filesystem::path mem_assets_dir() {
  namespace fs = std::filesystem;
  const fs::path candidates[] = {
      fs::current_path() / "assets" / "memory",
      fs::current_path() / ".." / "assets" / "memory",
      fs::path("C:/Users/Tommy/Projects/WerkPager/firmware/assets/memory"),
  };
  for (const auto & dir : candidates) {
    std::error_code ec;
    if (fs::is_directory(dir, ec)) return dir;
  }
  return {};
}

std::string mem_lvgl_path(const std::filesystem::path & p) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path canon = fs::weakly_canonical(p, ec);
  if (ec) canon = fs::absolute(p, ec);
  if (ec) return {};
  std::string s = "S:" + canon.string();
  for (char & c : s)
    if (c == '\\') c = '/';
  lv_image_header_t hdr{};
  if (lv_image_decoder_get_info(s.c_str(), &hdr) != LV_RESULT_OK || hdr.w == 0) return {};
  return s;
}

/** Sorted unique stems → best path (png preferred). Rescans folder each call. */
std::vector<std::string> mem_scan_pool() {
  namespace fs = std::filesystem;
  std::vector<std::string> out;
  const fs::path dir = mem_assets_dir();
  if (dir.empty()) return out;

  struct Cand {
    std::string stem;
    fs::path path;
    int rank; /* lower = better; png=0 jpg=1 jpeg=2 */
  };
  std::vector<Cand> cands;
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    const fs::path p = it->path();
    std::string ext = p.extension().string();
    for (char & c : ext)
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    int rank = -1;
    if (ext == ".png") rank = 0;
    else if (ext == ".jpg") rank = 1;
    else if (ext == ".jpeg") rank = 2;
    if (rank < 0) continue;
    std::string stem = p.stem().string();
    if (stem.empty() || stem[0] == '_' || stem[0] == '.') continue;
    Cand c;
    c.stem = stem;
    c.path = p;
    c.rank = rank;
    cands.push_back(c);
  }
  std::sort(cands.begin(), cands.end(), [](const Cand & a, const Cand & b) {
    if (a.stem != b.stem) return a.stem < b.stem;
    return a.rank < b.rank;
  });
  std::string last;
  for (const Cand & c : cands) {
    if (c.stem == last) continue;
    last = c.stem;
    std::string lvgl = mem_lvgl_path(c.path);
    if (!lvgl.empty()) out.push_back(std::move(lvgl));
  }
  return out;
}
#endif

/** Pick kPairs faces from the folder using seed (deterministic for MP sync). */
void mem_bind_faces(uint32_t seed) {
  for (int i = 0; i < games::mem::kPairs; ++i) g_mem_faces[i].clear();
  g_mem_faces_seed = seed;
  g_mem_faces_bound = true;

  std::vector<std::string> pool = mem_scan_pool();
  const int n = (int)pool.size();
  if (n <= 0) return;

  if (n <= games::mem::kPairs) {
    for (int i = 0; i < games::mem::kPairs; ++i) g_mem_faces[i] = pool[i % n];
    return;
  }

  /* Shuffle pool indices with a salt distinct from build_deck's shuffle. */
  std::vector<int> idx((size_t)n);
  for (int i = 0; i < n; ++i) idx[(size_t)i] = i;
  games::mem::Rng rng(seed ^ 0xA5C3F1EDu);
  for (int i = n - 1; i > 0; --i) {
    const int j = (int)(rng.next() * (i + 1));
    const int tmp = idx[(size_t)i];
    idx[(size_t)i] = idx[(size_t)j];
    idx[(size_t)j] = tmp;
  }
  for (int i = 0; i < games::mem::kPairs; ++i) g_mem_faces[i] = pool[(size_t)idx[(size_t)i]];
}

void mem_prepare_deck(uint32_t seed, uint8_t deck[games::mem::kCards]) {
  games::mem::build_deck(seed, deck);
  mem_bind_faces(seed);
}

const std::string & mem_face_path(uint8_t pair) {
  pair = (uint8_t)(pair % games::mem::kPairs);
  if (!g_mem_faces_bound) mem_bind_faces(app::mem().seed);
  return g_mem_faces[pair];
}

struct MemResolveLocal {
  int8_t a, b;
};

void mem_resolve_local(void * ud) {
  auto * r = static_cast<MemResolveLocal *>(ud);
  app::MemGame & g = app::mem();
  if (g.active) {
    const bool match = g.deck[r->a] == g.deck[r->b];
    if (match) {
      g.matched[r->a] = true;
      g.matched[r->b] = true;
      g.my_score++;
      g.my_turn = true;
    } else {
      g.my_turn = false;
    }
    g.flip_a = g.flip_b = -1;
    g.lock = false;
    bool all = true;
    for (bool m : g.matched)
      if (!m) all = false;
    if (all) {
      g.over = true;
      g.result_dismissed = false;
    }
    if (ui::current_screen() == ui::Screen::Mem) go_memory();
  }
  delete r;
}

void mem_challenge(lv_event_t * e) {
  if (!guard_peer_challenge(e)) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];
  if (!begin_kind(app::GameKind::Mem, p)) return;
  app::mem() = {};
  app::mem().active = true;
  app::mem().waiting = true;
  app::mem().seed = (uint32_t)std::rand();
  const bool first = app::roll_first();
  app::mem().my_turn = first;
  mem_prepare_deck(app::mem().seed, app::mem().deck);
  std::snprintf(app::mem().opp_id, sizeof(app::mem().opp_id), "%s", p.id);
  std::snprintf(app::mem().opp_name, sizeof(app::mem().opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::MemInvite;
  fill_msg_ids(m, p.id);
  m.seed = app::mem().seed;
  m.first = first;
  app::send(m);
  go_memory();
}

void mem_flip(lv_event_t * e) {
  const int card = (int)(intptr_t)lv_event_get_user_data(e);
  app::MemGame & g = app::mem();
  if (!g.active || g.waiting || g.over || !g.my_turn || g.lock) return;
  if (!guard_active_move(g.opp_id)) return;
  if (card < 0 || card >= 16 || g.matched[card]) return;
  if (g.local_flip == card) return;

  if (g.local_flip < 0) {
    g.local_flip = (int8_t)card;
    go_memory();
    return;
  }

  g.flip_a = g.local_flip;
  g.flip_b = (int8_t)card;
  g.local_flip = -1;
  g.lock = true;
  proto::Msg m;
  m.type = proto::MsgType::MemFlip;
  fill_msg_ids(m, g.opp_id);
  m.card_a = g.flip_a;
  m.card_b = g.flip_b;
  app::send(m);
  app::schedule(700, mem_resolve_local, new MemResolveLocal{g.flip_a, g.flip_b});
  go_memory();
}

void fill_mem_play(lv_obj_t * parent) {
  app::MemGame & g = app::mem();

  lv_obj_set_style_pad_row(parent, 2, 0);
  lv_obj_set_style_pad_ver(parent, 0, 0);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  make_score_pill(parent, app::desk().name, g.my_score, theme::mint(), g.opp_name, g.opp_score,
                  theme::hot());

  const char * status = "Your turn";
  char wait_buf[48];
  if (g.over && !g.result_dismissed) status = "Game over";
  else if (g.over) status = "Play again?";
  else if (g.lock) status = "Matching...";
  else if (!g.my_turn) {
    lv_snprintf(wait_buf, sizeof(wait_buf), "Waiting on %s...", g.opp_name);
    status = wait_buf;
  }
  make_status(parent, status);

  /* Grow cards into leftover space under score + status. */
  constexpr int kGap = 6;
  lv_obj_update_layout(parent);
  const int avail_w = (int)lv_obj_get_content_width(parent);
  const int content_h = (int)lv_obj_get_content_height(parent);
  const int pad_row = (int)lv_obj_get_style_pad_row(parent, LV_PART_MAIN);
  int used = 0;
  const uint32_t n = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < n; ++i) used += (int)lv_obj_get_height(lv_obj_get_child(parent, i));
  if (n > 0) used += pad_row * (int)n;
  int avail_h = content_h - used - 2;
  if (avail_h < 280) avail_h = 280;
  int kCard = (avail_w - 3 * kGap) / 4;
  const int card_h = (avail_h - 3 * kGap) / 4;
  if (card_h < kCard) kCard = card_h;
  if (kCard < 72) kCard = 72;
  if (kCard > 100) kCard = 100;

  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 4 * kCard + 3 * kGap, 4 * kCard + 3 * kGap);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < 16; ++i) {
    lv_obj_t * card = lv_obj_create(board);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, kCard, kCard);
    lv_obj_set_pos(card, (i % 4) * (kCard + kGap), (i / 4) * (kCard + kGap));
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    const bool up = g.matched[i] || g.flip_a == i || g.flip_b == i || g.local_flip == i;
    if (up) {
      lv_obj_set_style_bg_color(card, lv_color_hex(0x1a1224), 0);
      lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
      const std::string & path = mem_face_path(g.deck[i]);
      bool shown = false;
      if (!path.empty()) {
        lv_obj_t * img = lv_image_create(card);
        int32_t src_w = 96;
#ifdef WP_DEVICE
        /* Baked RGB565 descriptors — zero decode cost on flip. */
        const lv_image_dsc_t * dsc = memory_pack::find_dsc(path.c_str());
        if (dsc) {
          lv_image_set_src(img, dsc);
          if (dsc->header.w > 0) src_w = (int32_t)dsc->header.w;
          shown = true;
        }
#else
        lv_image_set_src(img, path.c_str());
        /* Scale whatever source size to the card inset (authoring target 96px
         * via tools/memory-crop). Avoid set_size — can clip/blank in LVGL 9. */
        lv_image_header_t hdr{};
        if (lv_image_decoder_get_info(path.c_str(), &hdr) == LV_RESULT_OK && hdr.w > 0)
          src_w = (int32_t)hdr.w;
        shown = true;
#endif
        if (shown) {
          const int32_t scale = ((kCard - 8) * 256) / src_w;
          lv_image_set_scale(img, scale);
          lv_obj_center(img);
          lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
        } else {
          lv_obj_delete(img);
        }
      }
      if (!shown) {
        static const uint32_t kPairTint[] = {0xff4fa3, 0xf0c24b, 0x5dffc2, 0x4fa3ff,
                                            0xa85dff, 0xff7a3c, 0xe07090, 0x5cb88a};
        lv_obj_set_style_bg_color(card, lv_color_hex(kPairTint[g.deck[i] % 8]), 0);
        lv_obj_t * l = lv_label_create(card);
        char t[4];
        lv_snprintf(t, sizeof(t), "%d", g.deck[i] + 1);
        lv_label_set_text(l, t);
        lv_obj_set_style_text_color(l, lv_color_hex(0x1a0610), 0);
        lv_obj_set_style_text_font(l, font_display(kCard >= 88 ? 36 : 28), 0);
        lv_obj_center(l);
      }
      /* Matched: slight fade, but keep faces readable (was too dark at 140). */
      if (g.matched[i]) lv_obj_set_style_opa(card, 200, 0);
    } else {
      lv_obj_set_style_bg_color(card, lv_color_hex(0x6a3a78), 0);
      lv_obj_set_style_bg_grad_color(card, lv_color_hex(0x3a2450), 0);
      lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
      lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
      if (g.my_turn && !g.lock && !g.over) {
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, mem_flip, LV_EVENT_CLICKED, (void *)(intptr_t)i);
      }
    }
  }

  if (g.over && !g.result_dismissed) {
    const int outcome = g.my_score == g.opp_score ? -1 : (g.my_score > g.opp_score ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::mem().result_dismissed = true;
      go_memory();
    }, "Memory", g.opp_name);
  }
}

lv_obj_t * game_mem_build(lv_obj_t * into) {
  app::Desk & d = app::desk();
  lv_obj_t * scr = into;
  if (scr) lv_obj_clean(scr);
  else scr = make_screen();
  char sub[40];
  make_topbar(scr, "MEMORY", d.name,
              game_vs_sub(sub, sizeof(sub), app::mem().active, app::mem().opp_name, app::invite_active(app::GameKind::Mem),
                          app::invite_ref(app::GameKind::Mem).from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (app::invite_active(app::GameKind::Mem)) {
    make_wait_block(body, "GAME INVITE", app::invite_ref(app::GameKind::Mem).from_name, "wants to match pairs");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::MemDecline;
      fill_msg_ids(m, app::invite_ref(app::GameKind::Mem).from_id);
      app::send(m);
      app::end_focused();
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = app::invite_ref(app::GameKind::Mem);
      app::accept_invite(app::GameKind::Mem);
      app::mem() = {};
      app::mem().active = true;
      app::mem().seed = inv.seed;
      app::mem().my_turn = !inv.first;
      mem_prepare_deck(app::mem().seed, app::mem().deck);
      std::snprintf(app::mem().opp_id, sizeof(app::mem().opp_id), "%s", inv.from_id);
      std::snprintf(app::mem().opp_name, sizeof(app::mem().opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::MemAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_memory();
    });
  } else if (app::mem().active && app::mem().waiting) {
    make_wait_block(body, "CHALLENGE SENT", app::mem().opp_name, "Waiting for them to accept...");
    dock_cancel_home(dock);
  } else if (app::mem().active) {
    lv_obj_set_height(dock, kDockCompactH);
    lv_obj_set_style_pad_ver(dock, 4, 0);
    lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_height(body, WP_VER_RES - kTopbarH - kDockCompactH);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    fill_mem_play(body);
    if (app::mem().over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", app::mem().opp_id);
            std::snprintf(oname, sizeof(oname), "%s", app::mem().opp_name);
            app::mem() = {};
            app::mem().active = true;
            app::mem().waiting = true;
            app::mem().seed = (uint32_t)std::rand();
            const bool first = app::roll_first();
            app::mem().my_turn = first;
            mem_prepare_deck(app::mem().seed, app::mem().deck);
            std::snprintf(app::mem().opp_id, sizeof(app::mem().opp_id), "%s", oid);
            std::snprintf(app::mem().opp_name, sizeof(app::mem().opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::MemInvite;
            fill_msg_ids(m, oid);
            m.seed = app::mem().seed;
            m.first = first;
            app::send(m);
            go_memory();
          },
          [](lv_event_t * /*e*/) {
            app::end_focused();
            go_hub();
          });
    } else {
      dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
        score_log::note("Memory", app::mem().opp_name, score_log::Outcome::ForfeitSelf);
        proto::Msg m;
        m.type = proto::MsgType::MemForfeit;
        fill_msg_ids(m, app::mem().opp_id);
        app::send(m);
        app::end_focused();
        go_hub();
      });
    }
  } else {
    peer_list(body, mem_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

/* ================= REVERSI ================= */

void rv_send_pass() {
  app::RvGame & g = app::rv();
  proto::Msg m;
  m.type = proto::MsgType::RvMove;
  fill_msg_ids(m, g.opp_id);
  m.x = -1;
  m.y = -1;
  app::send(m);
  const int8_t opp = (g.my_color == games::rv::kBlack) ? games::rv::kWhite : games::rv::kBlack;
  if (games::rv::check_over(g.board) != 0) {
    g.over = true;
    g.result_dismissed = false;
  } else {
    g.turn = opp;
  }
}

void rv_challenge(lv_event_t * e) {
  if (!guard_peer_challenge(e)) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];
  if (!begin_kind(app::GameKind::Rv, p)) return;
  app::rv() = {};
  app::rv().active = true;
  app::rv().waiting = true;
  const bool first = app::roll_first();
  app::rv().my_color = first ? games::rv::kBlack : games::rv::kWhite;
  app::rv().turn = games::rv::kBlack;
  games::rv::init(app::rv().board);
  std::snprintf(app::rv().opp_id, sizeof(app::rv().opp_id), "%s", p.id);
  std::snprintf(app::rv().opp_name, sizeof(app::rv().opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::RvInvite;
  fill_msg_ids(m, p.id);
  m.first = first;
  app::send(m);
  go_reversi();
}

void rv_play_cell(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  const int r = packed / games::rv::kN;
  const int c = packed % games::rv::kN;
  app::RvGame & g = app::rv();
  if (!g.active || g.waiting || g.over || g.turn != g.my_color) return;
  if (!guard_active_move(g.opp_id)) return;
  if (!games::rv::apply(g.board, r, c, g.my_color)) return;
  proto::Msg m;
  m.type = proto::MsgType::RvMove;
  fill_msg_ids(m, g.opp_id);
  m.x = (int8_t)r;
  m.y = (int8_t)c;
  app::send(m);
  const int8_t w = games::rv::check_over(g.board);
  const int8_t opp = (g.my_color == games::rv::kBlack) ? games::rv::kWhite : games::rv::kBlack;
  if (w != 0) {
    g.over = true;
    g.result_dismissed = false;
  } else if (games::rv::any_move(g.board, opp)) {
    g.turn = opp;
  } else if (games::rv::any_move(g.board, g.my_color)) {
    g.turn = g.my_color; /* opponent must pass — keep playing */
  } else {
    g.over = true;
    g.result_dismissed = false;
  }
  go_reversi();
}

void fill_rv_play(lv_obj_t * parent) {
  app::RvGame & g = app::rv();
  const bool my_turn = !g.over && !g.waiting && g.turn == g.my_color;
  const bool can_move = games::rv::any_move(g.board, g.my_color);

  /* Passes are handled in app::handle_msg (RvMove) to avoid a second full rebuild. */
  if (my_turn && !can_move && !g.over) {
    if (games::rv::check_over(g.board) != 0) {
      g.over = true;
      g.result_dismissed = false;
    } else {
      rv_send_pass();
      /* Defer rebuild — sync go_reversi here stacked with the incoming-move refresh. */
      app::schedule(1, [](void * /*p*/) {
        const Screen s = current_screen();
        if (s == Screen::Hub || s == Screen::ActiveGames || s == Screen::Idle ||
            s == Screen::GamesFolder)
          return;
        go_reversi();
      }, nullptr);
    }
    return;
  }

  int bl = 0, wh = 0;
  games::rv::count_pieces(g.board, &bl, &wh);

  lv_obj_set_style_pad_row(parent, 2, 0);
  lv_obj_set_style_pad_ver(parent, 0, 0);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  const char * black_name = (g.my_color == games::rv::kBlack) ? app::desk().name : g.opp_name;
  const char * white_name = (g.my_color == games::rv::kWhite) ? app::desk().name : g.opp_name;
  make_score_pill(parent, black_name, bl, lv_color_hex(0x1a1a1a), white_name, wh,
                  lv_color_hex(0xffffff));

  const char * status = "Your turn";
  char wait_buf[48];
  if (g.over && !g.result_dismissed) status = "Game over";
  else if (g.over) status = "Play again?";
  else if (!my_turn) {
    lv_snprintf(wait_buf, sizeof(wait_buf), "Waiting on %s...", g.opp_name);
    status = wait_buf;
  }
  make_status(parent, status);

  /* Grow cells into leftover space under score + status. */
  constexpr int kGap = 2;
  constexpr int kPad = 2;
  constexpr int kN = games::rv::kN;
  lv_obj_update_layout(parent);
  const int avail_w = (int)lv_obj_get_content_width(parent);
  const int content_h = (int)lv_obj_get_content_height(parent);
  const int pad_row = (int)lv_obj_get_style_pad_row(parent, LV_PART_MAIN);
  int used = 0;
  const uint32_t nch = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < nch; ++i) used += (int)lv_obj_get_height(lv_obj_get_child(parent, i));
  if (nch > 0) used += pad_row * (int)nch;
  int avail_h = content_h - used;
  if (avail_h < 280) avail_h = 280;
  int kCell = (avail_w - kPad * 2 - (kN - 1) * kGap) / kN;
  const int cell_h = (avail_h - kPad * 2 - (kN - 1) * kGap) / kN;
  if (cell_h < kCell) kCell = cell_h;
  if (kCell < 37) kCell = 37;
  if (kCell > 52) kCell = 52;
  const int kBoard = kPad * 2 + kN * kCell + (kN - 1) * kGap;
  const int disc = kCell > 10 ? kCell - 8 : kCell - 4;

  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, kBoard, kBoard);
  lv_obj_set_style_radius(board, 12, 0);
  lv_obj_set_style_bg_color(board, lv_color_hex(0x0f3d26), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(board, kPad, 0);
  lv_obj_set_layout(board, LV_LAYOUT_GRID);
  static lv_coord_t cols[9];
  static lv_coord_t rows[9];
  for (int i = 0; i < kN; ++i) {
    cols[i] = (lv_coord_t)kCell;
    rows[i] = (lv_coord_t)kCell;
  }
  cols[kN] = LV_GRID_TEMPLATE_LAST;
  rows[kN] = LV_GRID_TEMPLATE_LAST;
  lv_obj_set_grid_dsc_array(board, cols, rows);
  lv_obj_set_style_pad_row(board, kGap, 0);
  lv_obj_set_style_pad_column(board, kGap, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  for (int r = 0; r < kN; ++r) {
    for (int c = 0; c < kN; ++c) {
      lv_obj_t * cell = lv_obj_create(board);
      lv_obj_remove_style_all(cell);
      lv_obj_set_style_radius(cell, 4, 0);
      lv_obj_set_style_bg_color(cell, lv_color_hex(0x1a5c3a), 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, c, 1, LV_GRID_ALIGN_STRETCH, r, 1);
      lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

      const int8_t piece = g.board[r][c];
      if (piece == games::rv::kBlack || piece == games::rv::kWhite) {
        lv_obj_t * d = lv_obj_create(cell);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, disc, disc);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(d, piece == games::rv::kBlack ? lv_color_hex(0x1a1a1a) : lv_color_hex(0xf2f2f2),
                                  0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_center(d);
        lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(d, LV_OBJ_FLAG_SCROLLABLE);
      } else if (my_turn && games::rv::would_flip(g.board, r, c, g.my_color) > 0) {
        lv_obj_set_style_border_width(cell, 2, 0);
        lv_obj_set_style_border_color(cell, theme::gold(), 0);
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cell, rv_play_cell, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(r * kN + c));
      }
    }
  }

  if (g.over && !g.result_dismissed) {
    const int8_t w = games::rv::check_over(g.board);
    const int outcome = (w < 0) ? -1 : (w == g.my_color ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::rv().result_dismissed = true;
      go_reversi();
    }, "Reversi", g.opp_name);
  }
}

lv_obj_t * game_rv_build(lv_obj_t * into) {
  app::Desk & d = app::desk();
  lv_obj_t * scr = into;
  if (scr) lv_obj_clean(scr);
  else scr = make_screen();
  char sub[40];
  make_topbar(scr, "REVERSI", d.name,
              game_vs_sub(sub, sizeof(sub), app::rv().active, app::rv().opp_name, app::invite_active(app::GameKind::Rv),
                          app::invite_ref(app::GameKind::Rv).from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (app::invite_active(app::GameKind::Rv)) {
    make_wait_block(body, "GAME INVITE", app::invite_ref(app::GameKind::Rv).from_name, "wants to play Reversi");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::RvDecline;
      fill_msg_ids(m, app::invite_ref(app::GameKind::Rv).from_id);
      app::send(m);
      app::end_focused();
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = app::invite_ref(app::GameKind::Rv);
      app::accept_invite(app::GameKind::Rv);
      app::rv() = {};
      app::rv().active = true;
      app::rv().my_color = inv.first ? games::rv::kWhite : games::rv::kBlack;
      app::rv().turn = games::rv::kBlack;
      games::rv::init(app::rv().board);
      std::snprintf(app::rv().opp_id, sizeof(app::rv().opp_id), "%s", inv.from_id);
      std::snprintf(app::rv().opp_name, sizeof(app::rv().opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::RvAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_reversi();
    });
  } else if (app::rv().active && app::rv().waiting) {
    make_wait_block(body, "CHALLENGE SENT", app::rv().opp_name, "Waiting for them to accept...");
    dock_cancel_home(dock);
  } else if (app::rv().active) {
    lv_obj_set_height(dock, kDockCompactH);
    lv_obj_set_style_pad_ver(dock, 4, 0);
    lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_height(body, WP_VER_RES - kTopbarH - kDockCompactH);
    lv_obj_set_style_pad_top(body, 0, 0);
    lv_obj_set_style_pad_bottom(body, 2, 0);
    lv_obj_set_style_pad_row(body, 2, 0);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    fill_rv_play(body);
    if (app::rv().over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            const int8_t color = app::rv().my_color;
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", app::rv().opp_id);
            std::snprintf(oname, sizeof(oname), "%s", app::rv().opp_name);
            app::rv() = {};
            app::rv().active = true;
            app::rv().waiting = true;
            const bool first = app::roll_first();
            app::rv().my_color = first ? games::rv::kBlack : games::rv::kWhite;
            app::rv().turn = games::rv::kBlack;
            games::rv::init(app::rv().board);
            std::snprintf(app::rv().opp_id, sizeof(app::rv().opp_id), "%s", oid);
            std::snprintf(app::rv().opp_name, sizeof(app::rv().opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::RvInvite;
            fill_msg_ids(m, oid);
            m.first = first;
            app::send(m);
            go_reversi();
          },
          [](lv_event_t * /*e*/) {
            app::end_focused();
            go_hub();
          });
    } else {
      dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
        score_log::note("Reversi", app::rv().opp_name, score_log::Outcome::ForfeitSelf);
        proto::Msg m;
        m.type = proto::MsgType::RvForfeit;
        fill_msg_ids(m, app::rv().opp_id);
        app::send(m);
        app::end_focused();
        go_hub();
      });
    }
  } else {
    peer_list(body, rv_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

/* ================= DOTS & BOXES ================= */

void db_challenge(lv_event_t * e) {
  if (!guard_peer_challenge(e)) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];
  if (!begin_kind(app::GameKind::Db, p)) return;
  app::db() = {};
  app::db().active = true;
  app::db().waiting = true;
  const bool first = app::roll_first();
  app::db().my_side = first ? games::db::kP1 : games::db::kP2;
  app::db().turn = games::db::kP1;
  games::db::init(app::db().state);
  std::snprintf(app::db().opp_id, sizeof(app::db().opp_id), "%s", p.id);
  std::snprintf(app::db().opp_name, sizeof(app::db().opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::DbInvite;
  fill_msg_ids(m, p.id);
  m.first = first;
  app::send(m);
  go_dots();
}

void db_play_edge(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  const int is_vert = (packed >> 16) & 1;
  const int r = (packed >> 8) & 0xff;
  const int c = packed & 0xff;
  app::DbGame & g = app::db();
  if (!g.active || g.waiting || g.over || g.turn != g.my_side) return;
  if (!guard_active_move(g.opp_id)) return;
  const int claimed = games::db::claim(g.state, is_vert, r, c, g.my_side);
  if (claimed < 0) return;
  proto::Msg m;
  m.type = proto::MsgType::DbLine;
  fill_msg_ids(m, g.opp_id);
  m.y = (int8_t)is_vert;
  m.x = (int8_t)r;
  m.col = (int8_t)c;
  app::send(m);
  if (games::db::over(g.state)) {
    g.over = true;
    g.result_dismissed = false;
  } else if (claimed == 0) {
    g.turn = (g.my_side == games::db::kP1) ? games::db::kP2 : games::db::kP1;
  } /* else keep turn after claiming a box */
  go_dots();
}

void fill_db_play(lv_obj_t * parent) {
  app::DbGame & g = app::db();
  const bool my_turn = !g.over && !g.waiting && g.turn == g.my_side;
  const int my_score = g.my_side == games::db::kP1 ? g.state.score1 : g.state.score2;
  const int opp_score = g.my_side == games::db::kP1 ? g.state.score2 : g.state.score1;

  lv_obj_set_style_pad_row(parent, 2, 0);
  lv_obj_set_style_pad_ver(parent, 0, 0);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  make_score_pill(parent, app::desk().name, my_score, theme::mint(), g.opp_name, opp_score,
                  theme::hot());

  const char * status = "Your turn - tap a line";
  char wait_buf[48];
  if (g.over && !g.result_dismissed) status = "Game over";
  else if (g.over) status = "Play again?";
  else if (!my_turn) {
    lv_snprintf(wait_buf, sizeof(wait_buf), "Waiting on %s...", g.opp_name);
    status = wait_buf;
  }
  make_status(parent, status);

  auto owner_color = [&](int8_t owner) -> lv_color_t {
    if (owner == g.my_side) return theme::mint();
    if (owner) return theme::hot();
    return theme::border();
  };

  /* Soft continuous lattice — thin + low-contrast so it doesn’t Hermann-flicker
   * against bright dots. Claimed edges stay thick + player-colored. */
  constexpr int kDot = 6;
  constexpr int kThickClaim = 8;
  constexpr int kThickEmpty = 3;
  constexpr int kOrigin = 4;
  lv_obj_update_layout(parent);
  const int avail_w = (int)lv_obj_get_content_width(parent);
  const int content_h = (int)lv_obj_get_content_height(parent);
  const int pad_row = (int)lv_obj_get_style_pad_row(parent, LV_PART_MAIN);
  int used = 0;
  const uint32_t nch = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < nch; ++i) used += (int)lv_obj_get_height(lv_obj_get_child(parent, i));
  if (nch > 0) used += pad_row * (int)nch;
  int avail_h = content_h - used;
  if (avail_h < 300) avail_h = 300;
  int side = avail_w < avail_h ? avail_w : avail_h;
  if (side < 318) side = 318; /* prior board size */
  if (side > 420) side = 420;
  const int kStep = (side - kDot - kOrigin * 2) / games::db::kBox;
  const int kBoard = kStep * games::db::kBox + kDot + kOrigin * 2;

  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, kBoard, kBoard);
  lv_obj_set_style_radius(board, 12, 0);
  lv_obj_set_style_bg_color(board, theme::panel(), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  for (int br = 0; br < games::db::kBox; ++br) {
    for (int bc = 0; bc < games::db::kBox; ++bc) {
      if (!g.state.box[br][bc]) continue;
      lv_obj_t * box = lv_obj_create(board);
      lv_obj_remove_style_all(box);
      lv_obj_set_pos(box, kOrigin + bc * kStep + kDot / 2, kOrigin + br * kStep + kDot / 2);
      lv_obj_set_size(box, kStep - kDot / 2, kStep - kDot / 2);
      lv_obj_set_style_bg_color(box, owner_color(g.state.box[br][bc]), 0);
      lv_obj_set_style_bg_opa(box, LV_OPA_40, 0);
      lv_obj_set_style_radius(box, 4, 0);
      lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    }
  }

  auto edge_hit = [&](int is_vert, int r, int c, int x, int y, int w, int h, int8_t owner) {
    /* Hit target is always the fat corridor; visual line is a centered child. */
    lv_obj_t * hit = lv_obj_create(board);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, x, y);
    lv_obj_set_size(hit, w, h);
    lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(hit, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * line = lv_obj_create(hit);
    lv_obj_remove_style_all(line);
    lv_obj_set_style_radius(line, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    if (owner) {
      if (is_vert) lv_obj_set_size(line, kThickClaim, h);
      else lv_obj_set_size(line, w, kThickClaim);
      lv_obj_set_style_bg_color(line, owner_color(owner), 0);
      lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    } else {
      if (is_vert) lv_obj_set_size(line, kThickEmpty, h);
      else lv_obj_set_size(line, w, kThickEmpty);
      lv_obj_set_style_bg_color(line, theme::border(), 0);
      lv_obj_set_style_bg_opa(line, LV_OPA_70, 0);
      if (my_turn) {
        lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(hit, db_play_edge, LV_EVENT_CLICKED,
                            (void *)(intptr_t)((is_vert << 16) | (r << 8) | c));
      }
    }
    lv_obj_center(line);
  };

  const int hit_pad = (kThickClaim - kDot) / 2;
  for (int r = 0; r < games::db::kDots; ++r) {
    for (int c = 0; c < games::db::kBox; ++c) {
      edge_hit(0, r, c, kOrigin + c * kStep + kDot / 2,
               kOrigin + r * kStep - hit_pad, kStep - kDot / 2, kThickClaim,
               games::db::h_owner(g.state, r, c));
    }
  }
  for (int r = 0; r < games::db::kBox; ++r) {
    for (int c = 0; c < games::db::kDots; ++c) {
      edge_hit(1, r, c, kOrigin + c * kStep - hit_pad, kOrigin + r * kStep + kDot / 2, kThickClaim,
               kStep - kDot / 2, games::db::v_owner(g.state, r, c));
    }
  }

  for (int r = 0; r < games::db::kDots; ++r) {
    for (int c = 0; c < games::db::kDots; ++c) {
      lv_obj_t * dot = lv_obj_create(board);
      lv_obj_remove_style_all(dot);
      lv_obj_set_pos(dot, kOrigin + c * kStep, kOrigin + r * kStep);
      lv_obj_set_size(dot, kDot, kDot);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      /* Soft dots (not bright gold) — bright hubs are what trigger Hermann flicker. */
      lv_obj_set_style_bg_color(dot, theme::muted(), 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_70, 0);
      lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }
  }

  if (g.over && !g.result_dismissed) {
    const int8_t w = games::db::winner(g.state);
    const int outcome = (w < 0) ? -1 : (w == g.my_side ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::db().result_dismissed = true;
      go_dots();
    }, "Dots & Boxes", g.opp_name);
  }
}

lv_obj_t * game_db_build(lv_obj_t * into) {
  app::Desk & d = app::desk();
  lv_obj_t * scr = into;
  if (scr) lv_obj_clean(scr);
  else scr = make_screen();
  char sub[40];
  make_topbar(scr, "DOTS & BOXES", d.name,
              game_vs_sub(sub, sizeof(sub), app::db().active, app::db().opp_name, app::invite_active(app::GameKind::Db),
                          app::invite_ref(app::GameKind::Db).from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (app::invite_active(app::GameKind::Db)) {
    make_wait_block(body, "GAME INVITE", app::invite_ref(app::GameKind::Db).from_name, "wants to play Dots & Boxes");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::DbDecline;
      fill_msg_ids(m, app::invite_ref(app::GameKind::Db).from_id);
      app::send(m);
      app::end_focused();
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = app::invite_ref(app::GameKind::Db);
      app::accept_invite(app::GameKind::Db);
      app::db() = {};
      app::db().active = true;
      app::db().my_side = inv.first ? games::db::kP2 : games::db::kP1;
      app::db().turn = games::db::kP1;
      games::db::init(app::db().state);
      std::snprintf(app::db().opp_id, sizeof(app::db().opp_id), "%s", inv.from_id);
      std::snprintf(app::db().opp_name, sizeof(app::db().opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::DbAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_dots();
    });
  } else if (app::db().active && app::db().waiting) {
    make_wait_block(body, "CHALLENGE SENT", app::db().opp_name, "Waiting for them to accept...");
    dock_cancel_home(dock);
  } else if (app::db().active) {
    lv_obj_set_height(dock, kDockCompactH);
    lv_obj_set_style_pad_ver(dock, 4, 0);
    lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_height(body, WP_VER_RES - kTopbarH - kDockCompactH);
    lv_obj_set_style_pad_top(body, 0, 0);
    lv_obj_set_style_pad_bottom(body, 2, 0);
    lv_obj_set_style_pad_row(body, 2, 0);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    fill_db_play(body);
    if (app::db().over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            const int8_t side = app::db().my_side;
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", app::db().opp_id);
            std::snprintf(oname, sizeof(oname), "%s", app::db().opp_name);
            app::db() = {};
            app::db().active = true;
            app::db().waiting = true;
            const bool first = app::roll_first();
            app::db().my_side = first ? games::db::kP1 : games::db::kP2;
            app::db().turn = games::db::kP1;
            games::db::init(app::db().state);
            std::snprintf(app::db().opp_id, sizeof(app::db().opp_id), "%s", oid);
            std::snprintf(app::db().opp_name, sizeof(app::db().opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::DbInvite;
            fill_msg_ids(m, oid);
            m.first = first;
            app::send(m);
            go_dots();
          },
          [](lv_event_t * /*e*/) {
            app::end_focused();
            go_hub();
          });
    } else {
      dock_forfeit_home(dock, [](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
        score_log::note("Dots & Boxes", app::db().opp_name, score_log::Outcome::ForfeitSelf);
        proto::Msg m;
        m.type = proto::MsgType::DbForfeit;
        fill_msg_ids(m, app::db().opp_id);
        app::send(m);
        app::end_focused();
        go_hub();
      });
    }
  } else {
    peer_list(body, db_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

}  // namespace

lv_obj_t * games_folder_screen() {
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "GAMES", app::desk().name);
  lv_obj_t * body = make_body(scr, true);

  lv_obj_t * grid = lv_obj_create(body);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, lv_pct(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                              LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(grid, cols, rows);
  lv_obj_set_style_pad_row(grid, 12, 0);
  lv_obj_set_style_pad_right(grid, 6, 0);
  lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_ON);
  lv_obj_set_style_bg_color(grid, theme::gold(), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(grid, 180, LV_PART_SCROLLBAR);
  lv_obj_set_style_width(grid, 4, LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(grid, 2, LV_PART_SCROLLBAR);

  struct Item {
    AppIcon icon;
    const char * label;
    lv_event_cb_t cb;
  };
  const Item items[] = {
      {AppIcon::Ttt, "Tic Tac Toe",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         go_ttt();
       }},
      {AppIcon::Sttt, "Super TTT",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         go_sttt();
       }},
      {AppIcon::C4, "Connect Four",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         go_c4();
       }},
      {AppIcon::Battleship, "Battleship",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         go_battleship();
       }},
      {AppIcon::Checkers, "Checkers",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         go_checkers();
       }},
      {AppIcon::Memory, "Memory",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         go_memory();
       }},
      {AppIcon::Reversi, "Reversi",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         go_reversi();
       }},
      {AppIcon::Dots, "Dots & Boxes",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         go_dots();
       }},
      {AppIcon::Wordle, "Wordle",
       [](lv_event_t * /*e*/) {
         app::set_focus(-1);
         wordle_reset_setup();
         go_wordle();
       }},
      {AppIcon::G2048, "2048 (1P)", [](lv_event_t * /*e*/) { go_g2048(); }},
      {AppIcon::Scoreboard, "Scoreboard", [](lv_event_t * /*e*/) { go_scoreboard(); }},
  };
  constexpr int kGameCount = (int)(sizeof(items) / sizeof(items[0]));
  for (int i = 0; i < kGameCount; ++i) {
    lv_obj_t * icon = make_app_icon(grid, items[i].icon, items[i].label, items[i].cb);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, i % 3, 1, LV_GRID_ALIGN_START, i / 3, 1);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
  return scr;
}

lv_obj_t * game_ttt_screen() { return game_ttt_build(nullptr); }
lv_obj_t * game_sttt_screen() { return game_sttt_build(nullptr); }
lv_obj_t * game_c4_screen() { return game_c4_build(nullptr); }
lv_obj_t * game_bs_screen() { return game_bs_build(nullptr); }
lv_obj_t * game_ck_screen() { return game_ck_build(nullptr); }
lv_obj_t * game_mem_screen() { return game_mem_build(nullptr); }
lv_obj_t * game_rv_screen() { return game_rv_build(nullptr); }
lv_obj_t * game_db_screen() { return game_db_build(nullptr); }

void game_reload_inplace() {
  lv_obj_t * cur = lv_screen_active();
  if (!cur) return;
  hide_forfeit_confirm();
  switch (current_screen()) {
    case Screen::Ttt: game_ttt_build(cur); break;
    case Screen::Sttt: game_sttt_build(cur); break;
    case Screen::C4: game_c4_build(cur); break;
    case Screen::Bs: game_bs_build(cur); break;
    case Screen::Ck: game_ck_build(cur); break;
    case Screen::Mem: game_mem_build(cur); break;
    case Screen::Rv: game_rv_build(cur); break;
    case Screen::Db: game_db_build(cur); break;
    default: break;
  }
}

void games_debug_show(const char * game, const char * panel) {
  auto seed_peer = [&](auto & g) {
    std::snprintf(g.opp_id, sizeof(g.opp_id), "mac-will");
    std::snprintf(g.opp_name, sizeof(g.opp_name), "Will");
  };
  auto prep = [&](app::GameKind kind) -> bool {
    app::clear_all_games();
    if (!app::begin_match(kind, "mac-will")) return false;
    return true;
  };

  if (game && !std::strcmp(game, "ttt")) {
    if (!prep(app::GameKind::Ttt)) return;
    app::ttt() = {};
    app::ttt().active = true;
    app::ttt().waiting = panel && !std::strcmp(panel, "wait");
    app::ttt().mark = 'X';
    app::ttt().turn = 'X';
    seed_peer(app::ttt());
    if (panel && !std::strcmp(panel, "play")) {
      app::ttt().board[0] = 'X';
      app::ttt().board[4] = 'O';
    } else if (panel && !std::strcmp(panel, "win")) {
      app::ttt().board[0] = app::ttt().board[1] = app::ttt().board[2] = 'X';
      app::ttt().board[3] = app::ttt().board[4] = 'O';
      app::ttt().over = true;
      app::ttt().result_dismissed = false;
    } else if (panel && !std::strcmp(panel, "lose")) {
      app::ttt().board[0] = app::ttt().board[1] = app::ttt().board[2] = 'O';
      app::ttt().board[3] = app::ttt().board[4] = 'X';
      app::ttt().over = true;
      app::ttt().result_dismissed = false;
    }
    go_ttt();
  } else if (game && !std::strcmp(game, "sttt")) {
    if (!prep(app::GameKind::Sttt)) return;
    app::sttt() = {};
    app::sttt().active = true;
    app::sttt().waiting = panel && !std::strcmp(panel, "wait");
    app::sttt().mark = 'X';
    app::sttt().turn = 'X';
    games::sttt::init(app::sttt().boards, app::sttt().meta, app::sttt().next_board);
    seed_peer(app::sttt());
    if (panel && !std::strcmp(panel, "play")) {
      app::sttt().boards[0][0] = 'X';
      app::sttt().boards[0][4] = 'O';
      app::sttt().boards[4][4] = 'X';
      app::sttt().next_board = 4;
    }
    go_sttt();
  } else if (game && !std::strcmp(game, "c4")) {
    if (!prep(app::GameKind::C4)) return;
    app::c4() = {};
    app::c4().active = true;
    app::c4().waiting = panel && !std::strcmp(panel, "wait");
    app::c4().my_color = 0;
    app::c4().opp_color = 1;
    app::c4().turn = 0;
    games::c4::init(app::c4().board);
    seed_peer(app::c4());
    if (panel && !std::strcmp(panel, "win")) {
      for (int r = 5; r >= 2; --r) app::c4().board[r][0] = 0;
      app::c4().last_r = 2;
      app::c4().last_c = 0;
      app::c4().over = true;
      app::c4().result_dismissed = false;
    }
    go_c4();
  } else if (game && !std::strcmp(game, "bs")) {
    if (!prep(app::GameKind::Bs)) return;
    app::bs() = {};
    app::bs().active = true;
    app::bs().setup = !(panel && (!std::strcmp(panel, "play") || !std::strcmp(panel, "defense") ||
                             !std::strcmp(panel, "win")));
    app::bs().my_turn = true;
    app::bs().mode = (panel && !std::strcmp(panel, "defense")) ? 1 : 0;
    if (app::bs().setup) {
      games::bs::clear_fleet(app::bs().fleet);
      /* Two ships down; anchor set for next (Cruiser) so ghost placements show. */
      games::bs::place_ship(app::bs().fleet, 0, 0, 0, true);  /* Carrier */
      games::bs::place_ship(app::bs().fleet, 1, 0, 2, true);  /* Battleship */
      app::bs().anchor_x = 4;
      app::bs().anchor_y = 5;
      std::snprintf(app::bs().last_msg, sizeof(app::bs().last_msg), "Tap a highlighted square to place Cruiser (3)");
    } else {
      games::bs::random_fleet(app::bs().fleet);
      std::snprintf(app::bs().last_msg, sizeof(app::bs().last_msg), "Your turn - tap to fire!");
    }
    if (panel && !std::strcmp(panel, "win")) {
      app::bs().over = true;
      app::bs().i_won = true;
      app::bs().result_dismissed = false;
      app::bs().setup = false;
    }
    seed_peer(app::bs());
    go_battleship();
  } else if (game && !std::strcmp(game, "ck")) {
    if (!prep(app::GameKind::Ck)) return;
    app::ck() = {};
    app::ck().active = true;
    app::ck().side = 'r';
    app::ck().turn = 'r';
    games::ck::init(app::ck().board);
    seed_peer(app::ck());
    if (panel && !std::strcmp(panel, "play")) {
      /* Mid-game: a few men advanced / traded. */
      app::ck().board[5][2] = 0;
      app::ck().board[4][3] = 'r';
      app::ck().board[5][4] = 0;
      app::ck().board[3][4] = 'r';
      app::ck().board[2][1] = 0;
      app::ck().board[3][2] = 'b';
      app::ck().board[2][5] = 0;
      app::ck().board[1][2] = 0; /* traded */
      app::ck().sel_x = 3;
      app::ck().sel_y = 4;
    }
    if (panel && !std::strcmp(panel, "win")) {
      /* Clear opponent pieces so red wins. */
      for (int y = 0; y < games::ck::kSize; ++y)
        for (int x = 0; x < games::ck::kSize; ++x)
          if (app::ck().board[y][x] == 'b' || app::ck().board[y][x] == 'B') app::ck().board[y][x] = 0;
      app::ck().over = true;
      app::ck().result_dismissed = false;
    }
    go_checkers();
  } else if (game && !std::strcmp(game, "mem")) {
    if (!prep(app::GameKind::Mem)) return;
    app::mem() = {};
    app::mem().active = true;
    app::mem().seed = 42;
    app::mem().my_turn = true;
    mem_prepare_deck(app::mem().seed, app::mem().deck);
    seed_peer(app::mem());
    if (panel && !std::strcmp(panel, "play")) {
      /* Face a full row so asset decode is visible in screenshots. */
      for (int i = 0; i < 4; ++i) app::mem().matched[i] = true;
    }
    go_memory();
  } else if (game && !std::strcmp(game, "rv")) {
    if (!prep(app::GameKind::Rv)) return;
    app::rv() = {};
    app::rv().active = true;
    app::rv().my_color = games::rv::kBlack;
    app::rv().turn = games::rv::kBlack;
    games::rv::init(app::rv().board);
    seed_peer(app::rv());
    if (panel && !std::strcmp(panel, "win")) {
      app::rv().over = true;
      app::rv().result_dismissed = false;
    }
    go_reversi();
  } else if (game && !std::strcmp(game, "db")) {
    if (!prep(app::GameKind::Db)) return;
    app::db() = {};
    app::db().active = true;
    app::db().my_side = games::db::kP1;
    app::db().turn = games::db::kP1;
    games::db::init(app::db().state);
    seed_peer(app::db());
    /* Seed a few edges so player-colored lines show in screenshots. */
    games::db::claim(app::db().state, 0, 0, 0, games::db::kP1);
    games::db::claim(app::db().state, 1, 0, 0, games::db::kP2);
    games::db::claim(app::db().state, 0, 1, 0, games::db::kP2);
    games::db::claim(app::db().state, 0, 0, 1, games::db::kP1);
    games::db::claim(app::db().state, 1, 1, 1, games::db::kP1);
    if (panel && !std::strcmp(panel, "win")) {
      app::db().over = true;
      app::db().result_dismissed = false;
    }
    go_dots();
  }
}

bool accept_incoming_slot(int idx) {
  app::GameSlot * s = app::slot_at(idx);
  if (!s || !s->invite_pending || !s->invite.active) return false;
  app::set_focus(idx);
  const app::GameKind kind = s->kind;
  const app::Invite inv = s->invite; /* copy before accept_invite clears it */

  auto fill = [](proto::Msg & m, const char * to) {
    std::snprintf(m.from_id, sizeof(m.from_id), "%s", app::desk().id);
    std::snprintf(m.from_name, sizeof(m.from_name), "%s", app::desk().name);
    if (to) std::snprintf(m.to_id, sizeof(m.to_id), "%s", to);
  };

  app::accept_invite(kind);
  proto::Msg m{};

  switch (kind) {
    case app::GameKind::Ttt:
      app::ttt() = {};
      app::ttt().active = true;
      app::ttt().mark = inv.first ? 'O' : 'X';
      app::ttt().turn = 'X';
      std::snprintf(app::ttt().opp_id, sizeof(app::ttt().opp_id), "%s", inv.from_id);
      std::snprintf(app::ttt().opp_name, sizeof(app::ttt().opp_name), "%s", inv.from_name);
      m.type = proto::MsgType::TttAccept;
      fill(m, inv.from_id);
      app::send(m);
      go_ttt();
      return true;
    case app::GameKind::Sttt:
      app::sttt() = {};
      app::sttt().active = true;
      app::sttt().mark = inv.first ? 'O' : 'X';
      app::sttt().turn = 'X';
      games::sttt::init(app::sttt().boards, app::sttt().meta, app::sttt().next_board);
      std::snprintf(app::sttt().opp_id, sizeof(app::sttt().opp_id), "%s", inv.from_id);
      std::snprintf(app::sttt().opp_name, sizeof(app::sttt().opp_name), "%s", inv.from_name);
      m.type = proto::MsgType::StttAccept;
      fill(m, inv.from_id);
      app::send(m);
      go_sttt();
      return true;
    case app::GameKind::C4:
      app::c4() = {};
      app::c4().active = true;
      app::c4().opp_color = inv.color >= 0 ? inv.color : 0;
      app::c4().my_color = app::c4().opp_color == 0 ? 1 : 0;
      app::c4().turn = inv.first ? app::c4().opp_color : app::c4().my_color;
      games::c4::init(app::c4().board);
      std::snprintf(app::c4().opp_id, sizeof(app::c4().opp_id), "%s", inv.from_id);
      std::snprintf(app::c4().opp_name, sizeof(app::c4().opp_name), "%s", inv.from_name);
      m.type = proto::MsgType::C4Accept;
      fill(m, inv.from_id);
      m.color = app::c4().my_color;
      app::send(m);
      go_c4();
      return true;
    case app::GameKind::Bs:
      app::bs() = {};
      app::bs().active = true;
      app::bs().setup = true;
      app::bs().i_am_first = !inv.first;
      games::bs::clear_fleet(app::bs().fleet);
      std::snprintf(app::bs().opp_id, sizeof(app::bs().opp_id), "%s", inv.from_id);
      std::snprintf(app::bs().opp_name, sizeof(app::bs().opp_name), "%s", inv.from_name);
      std::snprintf(app::bs().last_msg, sizeof(app::bs().last_msg), "Place your fleet");
      m.type = proto::MsgType::BsAccept;
      fill(m, inv.from_id);
      app::send(m);
      go_battleship();
      return true;
    case app::GameKind::Ck:
      app::ck() = {};
      app::ck().active = true;
      app::ck().side = inv.first ? 'b' : 'r';
      app::ck().turn = 'r';
      games::ck::init(app::ck().board);
      std::snprintf(app::ck().opp_id, sizeof(app::ck().opp_id), "%s", inv.from_id);
      std::snprintf(app::ck().opp_name, sizeof(app::ck().opp_name), "%s", inv.from_name);
      m.type = proto::MsgType::CkAccept;
      fill(m, inv.from_id);
      app::send(m);
      go_checkers();
      return true;
    case app::GameKind::Mem:
      app::mem() = {};
      app::mem().active = true;
      app::mem().seed = inv.seed;
      app::mem().my_turn = !inv.first;
      mem_prepare_deck(app::mem().seed, app::mem().deck);
      std::snprintf(app::mem().opp_id, sizeof(app::mem().opp_id), "%s", inv.from_id);
      std::snprintf(app::mem().opp_name, sizeof(app::mem().opp_name), "%s", inv.from_name);
      m.type = proto::MsgType::MemAccept;
      fill(m, inv.from_id);
      app::send(m);
      go_memory();
      return true;
    case app::GameKind::Rv:
      app::rv() = {};
      app::rv().active = true;
      app::rv().my_color = inv.first ? games::rv::kWhite : games::rv::kBlack;
      app::rv().turn = games::rv::kBlack;
      games::rv::init(app::rv().board);
      std::snprintf(app::rv().opp_id, sizeof(app::rv().opp_id), "%s", inv.from_id);
      std::snprintf(app::rv().opp_name, sizeof(app::rv().opp_name), "%s", inv.from_name);
      m.type = proto::MsgType::RvAccept;
      fill(m, inv.from_id);
      app::send(m);
      go_reversi();
      return true;
    case app::GameKind::Db:
      app::db() = {};
      app::db().active = true;
      app::db().my_side = inv.first ? games::db::kP2 : games::db::kP1;
      app::db().turn = games::db::kP1;
      games::db::init(app::db().state);
      std::snprintf(app::db().opp_id, sizeof(app::db().opp_id), "%s", inv.from_id);
      std::snprintf(app::db().opp_name, sizeof(app::db().opp_name), "%s", inv.from_name);
      m.type = proto::MsgType::DbAccept;
      fill(m, inv.from_id);
      app::send(m);
      go_dots();
      return true;
    case app::GameKind::Wordle:
      app::wordle() = {};
      app::wordle().active = true;
      app::wordle().waiting = false;
      app::wordle().race = inv.wordle_mode != 0;
      std::snprintf(app::wordle().opp_id, sizeof(app::wordle().opp_id), "%s", inv.from_id);
      std::snprintf(app::wordle().opp_name, sizeof(app::wordle().opp_name), "%s", inv.from_name);
      m.type = proto::MsgType::WordleAccept;
      fill(m, inv.from_id);
      app::send(m);
      go_wordle();
      return true;
    default: return false;
  }
}

}  // namespace ui
}  // namespace wp
