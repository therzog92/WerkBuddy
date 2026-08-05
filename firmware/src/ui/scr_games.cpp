#include "ui/scr_games.h"

#include "app/app.h"
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
#include "ui/theme.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

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

lv_obj_t * make_status(lv_obj_t * parent, const char * text) {
  lv_obj_t * s = lv_label_create(parent);
  lv_label_set_text(s, text);
  lv_obj_set_style_text_color(s, theme::mint(), 0);
  lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(s, lv_pct(100));
  return s;
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

void peer_list(lv_obj_t * parent, lv_event_cb_t on_peer) {
  make_tagline(parent, "Challenge a peer");
  const app::Desk & d = app::desk();
  if (d.peer_count == 0) make_tagline(parent, "No saved desks - add one in Settings.");
  for (int i = 0; i < d.peer_count; ++i) {
    make_peer_btn(parent, d.peers[i].name, "challenge", on_peer, (void *)(intptr_t)i);
  }
}

/* ================= TIC TAC TOE ================= */

void ttt_challenge(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count || app::busy() || d.ttt.active || d.ttt_invite.active) return;
  const app::Peer & p = d.peers[idx];
  d.ttt = {};
  d.ttt.active = true;
  d.ttt.waiting = true;
  d.ttt.mark = 'X';
  d.ttt.turn = 'X';
  std::snprintf(d.ttt.opp_id, sizeof(d.ttt.opp_id), "%s", p.id);
  std::snprintf(d.ttt.opp_name, sizeof(d.ttt.opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::TttInvite;
  fill_msg_ids(m, p.id);
  app::send(m);
  go_ttt();
}

void ttt_play_cell(lv_event_t * e) {
  const int cell = (int)(intptr_t)lv_event_get_user_data(e);
  app::TttGame & g = app::desk().ttt;
  if (!g.active || g.waiting || g.over || g.turn != g.mark) return;
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

void fill_ttt_play(lv_obj_t * parent) {
  app::TttGame & g = app::desk().ttt;
  const bool my_turn = !g.over && !g.waiting && g.turn == g.mark;
  const char win = games::ttt::winner(g.board);
  if (g.over && !g.result_dismissed) {
    make_status(parent, "Game over");
  } else if (my_turn) {
    make_status(parent, "Your turn - serve!");
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
  lv_obj_set_style_bg_color(badge, theme::hot(), 0);
  lv_obj_set_style_bg_grad_color(badge, theme::gold(), 0);
  lv_obj_set_style_bg_grad_dir(badge, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * bl = lv_label_create(badge);
  char mk[2] = {g.mark, 0};
  lv_label_set_text(bl, mk);
  lv_obj_set_style_text_color(bl, lv_color_hex(0x1a0610), 0);
  lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
  lv_obj_center(bl);

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
      lv_obj_t * l = lv_label_create(cell);
      char t[2] = {g.board[i], 0};
      lv_label_set_text(l, t);
      lv_obj_set_style_text_font(l, font_display(52), 0);
      lv_obj_set_style_text_color(l, g.board[i] == 'X' ? theme::hot() : theme::mint(), 0);
      lv_obj_center(l);
    } else if (my_turn) {
      lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(cell, ttt_play_cell, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
  }

  if (g.over && !g.result_dismissed) {
    const int outcome = !win ? -1 : (win == g.mark ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::desk().ttt.result_dismissed = true;
      go_ttt();
    }, "Tic Tac Toe", g.opp_name);
  }
}

lv_obj_t * game_ttt_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  char sub[40];
  make_topbar(scr, "TIC TAC TOE", d.name,
              game_vs_sub(sub, sizeof(sub), d.ttt.active, d.ttt.opp_name, d.ttt_invite.active,
                          d.ttt_invite.from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (d.ttt_invite.active) {
    make_wait_block(body, "GAME INVITE", d.ttt_invite.from_name, "wants to play Tic Tac Toe");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::TttDecline;
      fill_msg_ids(m, desk.ttt_invite.from_id);
      app::send(m);
      desk.ttt_invite.active = false;
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = desk.ttt_invite;
      desk.ttt_invite.active = false;
      desk.ttt = {};
      desk.ttt.active = true;
      desk.ttt.mark = 'O';
      desk.ttt.turn = 'X';
      std::snprintf(desk.ttt.opp_id, sizeof(desk.ttt.opp_id), "%s", inv.from_id);
      std::snprintf(desk.ttt.opp_name, sizeof(desk.ttt.opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::TttAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_ttt();
    });
  } else if (d.ttt.active && d.ttt.waiting) {
    make_wait_block(body, "CHALLENGE SENT", d.ttt.opp_name, "Waiting for them to accept...");
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::TttForfeit;
      fill_msg_ids(m, desk.ttt.opp_id);
      app::send(m);
      desk.ttt.active = false;
      go_ttt();
    });
  } else if (d.ttt.active) {
    fill_ttt_play(body);
    if (d.ttt.over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            app::Peer p;
            std::snprintf(p.id, sizeof(p.id), "%s", desk.ttt.opp_id);
            std::snprintf(p.name, sizeof(p.name), "%s", desk.ttt.opp_name);
            desk.ttt = {};
            desk.ttt.active = true;
            desk.ttt.waiting = true;
            desk.ttt.mark = 'X';
            desk.ttt.turn = 'X';
            std::snprintf(desk.ttt.opp_id, sizeof(desk.ttt.opp_id), "%s", p.id);
            std::snprintf(desk.ttt.opp_name, sizeof(desk.ttt.opp_name), "%s", p.name);
            proto::Msg m;
            m.type = proto::MsgType::TttInvite;
            fill_msg_ids(m, p.id);
            app::send(m);
            go_ttt();
          },
          [](lv_event_t * /*e*/) {
            app::desk().ttt.active = false;
            go_games_folder();
          });
    } else {
      dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
        show_forfeit_confirm([](lv_event_t * /*ev*/) {
          app::Desk & desk = app::desk();
          score_log::note("Tic Tac Toe", desk.ttt.opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::TttForfeit;
          fill_msg_ids(m, desk.ttt.opp_id);
          app::send(m);
          desk.ttt.active = false;
          go_games_folder();
        });
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
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count || app::busy() || d.sttt.active || d.sttt_invite.active) return;
  const app::Peer & p = d.peers[idx];
  d.sttt = {};
  d.sttt.active = true;
  d.sttt.waiting = true;
  d.sttt.mark = 'X';
  d.sttt.turn = 'X';
  games::sttt::init(d.sttt.boards, d.sttt.meta, d.sttt.next_board);
  std::snprintf(d.sttt.opp_id, sizeof(d.sttt.opp_id), "%s", p.id);
  std::snprintf(d.sttt.opp_name, sizeof(d.sttt.opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::StttInvite;
  fill_msg_ids(m, p.id);
  app::send(m);
  go_sttt();
}

void sttt_play_cell(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  const int board = packed / 9;
  const int cell = packed % 9;
  app::StttGame & g = app::desk().sttt;
  if (!g.active || g.waiting || g.over || g.turn != g.mark) return;
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
  app::StttGame & g = app::desk().sttt;
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
        lv_obj_t * l = lv_label_create(cell);
        char t[2] = {g.boards[b][c], 0};
        lv_label_set_text(l, t);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, g.boards[b][c] == 'X' ? theme::hot() : theme::mint(), 0);
        lv_obj_center(l);
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
      lv_obj_t * big = lv_label_create(ov);
      char t[2] = {g.meta[b], 0};
      lv_label_set_text(big, t);
      lv_obj_set_style_text_font(big, font_display(40), 0);
      lv_obj_set_style_text_color(big, g.meta[b] == 'X' ? theme::hot() : theme::mint(), 0);
      lv_obj_center(big);
      lv_obj_move_foreground(ov);
    } else if (g.meta[b] == 'D') {
      lv_obj_set_style_border_opa(shell, LV_OPA_40, 0);
    }
  }

  if (g.over && !g.result_dismissed) {
    const int outcome = !win ? -1 : (win == g.mark ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::desk().sttt.result_dismissed = true;
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

lv_obj_t * game_sttt_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();

  const bool playing = d.sttt.active && !d.sttt.waiting && !d.sttt_invite.active;
  lv_obj_t * top = nullptr;
  if (playing) {
    const bool my_turn = !d.sttt.over && d.sttt.turn == d.sttt.mark;
    const bool forced = games::sttt::forced(d.sttt.meta, d.sttt.next_board);
    const char * status;
    if (d.sttt.over && !d.sttt.result_dismissed) status = "Game over";
    else if (d.sttt.over) status = "Play again?";
    else if (my_turn && forced) status = "Your turn: lit board";
    else if (my_turn) status = "Your turn: any board";
    else {
      static char wait[40];
      lv_snprintf(wait, sizeof(wait), "Waiting on %s", d.sttt.opp_name);
      status = wait;
    }
    top = make_sttt_play_topbar(scr, d.name, d.sttt.opp_name, d.sttt.mark, status);
  } else {
    char sub[40];
    top = make_topbar(scr, "SUPER TIC TAC TOE", d.name,
                      game_vs_sub(sub, sizeof(sub), d.sttt.active, d.sttt.opp_name,
                                  d.sttt_invite.active, d.sttt_invite.from_name));
  }
  (void)top;

  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (d.sttt_invite.active) {
    make_wait_block(body, "GAME INVITE", d.sttt_invite.from_name, "wants to play Super Tic Tac Toe");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::StttDecline;
      fill_msg_ids(m, desk.sttt_invite.from_id);
      app::send(m);
      desk.sttt_invite.active = false;
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = desk.sttt_invite;
      desk.sttt_invite.active = false;
      desk.sttt = {};
      desk.sttt.active = true;
      desk.sttt.mark = 'O';
      desk.sttt.turn = 'X';
      games::sttt::init(desk.sttt.boards, desk.sttt.meta, desk.sttt.next_board);
      std::snprintf(desk.sttt.opp_id, sizeof(desk.sttt.opp_id), "%s", inv.from_id);
      std::snprintf(desk.sttt.opp_name, sizeof(desk.sttt.opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::StttAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_sttt();
    });
  } else if (d.sttt.active && d.sttt.waiting) {
    make_wait_block(body, "CHALLENGE SENT", d.sttt.opp_name, "Waiting for them to accept...");
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::StttForfeit;
      fill_msg_ids(m, desk.sttt.opp_id);
      app::send(m);
      desk.sttt.active = false;
      go_sttt();
    });
  } else if (d.sttt.active) {
    /* Compact dock so the board can use more vertical space. */
    lv_obj_set_height(dock, kDockCompactH);
    lv_obj_set_style_pad_ver(dock, 6, 0);
    lv_obj_set_height(body, WP_VER_RES - kTopbarH - kDockCompactH);

    fill_sttt_play(body);
    if (d.sttt.over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            app::Peer p;
            std::snprintf(p.id, sizeof(p.id), "%s", desk.sttt.opp_id);
            std::snprintf(p.name, sizeof(p.name), "%s", desk.sttt.opp_name);
            desk.sttt = {};
            desk.sttt.active = true;
            desk.sttt.waiting = true;
            desk.sttt.mark = 'X';
            desk.sttt.turn = 'X';
            games::sttt::init(desk.sttt.boards, desk.sttt.meta, desk.sttt.next_board);
            std::snprintf(desk.sttt.opp_id, sizeof(desk.sttt.opp_id), "%s", p.id);
            std::snprintf(desk.sttt.opp_name, sizeof(desk.sttt.opp_name), "%s", p.name);
            proto::Msg m;
            m.type = proto::MsgType::StttInvite;
            fill_msg_ids(m, p.id);
            app::send(m);
            go_sttt();
          },
          [](lv_event_t * /*e*/) {
            app::desk().sttt.active = false;
            go_games_folder();
          });
      /* Shrink play-again dock buttons too. */
      const uint32_t n = lv_obj_get_child_count(dock);
      for (uint32_t i = 0; i < n; ++i) lv_obj_set_height(lv_obj_get_child(dock, i), 36);
    } else {
      lv_obj_t * btn = dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
        show_forfeit_confirm([](lv_event_t * /*ev*/) {
          app::Desk & desk = app::desk();
          score_log::note("Super TTT", desk.sttt.opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::StttForfeit;
          fill_msg_ids(m, desk.sttt.opp_id);
          app::send(m);
          desk.sttt.active = false;
          go_games_folder();
        });
      });
      lv_obj_set_height(btn, 36);
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
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count || app::busy() || d.c4.active || d.c4_invite.active) return;
  const app::Peer & p = d.peers[idx];
  d.c4 = {};
  d.c4.active = true;
  d.c4.waiting = true;
  d.c4.my_color = (int8_t)g_c4_pick_color;
  d.c4.turn = d.c4.my_color;
  games::c4::init(d.c4.board);
  std::snprintf(d.c4.opp_id, sizeof(d.c4.opp_id), "%s", p.id);
  std::snprintf(d.c4.opp_name, sizeof(d.c4.opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::C4Invite;
  fill_msg_ids(m, p.id);
  m.color = d.c4.my_color;
  app::send(m);
  go_c4();
}

void c4_drop(lv_event_t * e) {
  const int col = (int)(intptr_t)lv_event_get_user_data(e);
  app::C4Game & g = app::desk().c4;
  if (!g.active || g.waiting || g.over || g.turn != g.my_color) return;
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
  app::C4Game & g = app::desk().c4;
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
      lv_obj_set_style_bg_grad(disc, disc_grad(g.board[r][c]), 0);
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
          app::desk().c4.last_r = -1;
          app::desk().c4.last_c = -1;
        });
        lv_anim_start(&a);
      }
    }
  }

  if (!g_c4_frame_buf) {
    g_c4_frame_buf = static_cast<uint8_t *>(lv_malloc(kC4BoardW * kC4BoardH * 4));
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
      app::desk().c4.result_dismissed = true;
      go_c4();
    }, "Connect Four", g.opp_name);
  }
}

lv_obj_t * game_c4_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  char sub[40];
  make_topbar(scr, "CONNECT FOUR", d.name,
              game_vs_sub(sub, sizeof(sub), d.c4.active, d.c4.opp_name, d.c4_invite.active,
                          d.c4_invite.from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (d.c4_invite.active) {
    make_wait_block(body, "GAME INVITE", d.c4_invite.from_name, "wants to play Connect Four");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::C4Decline;
      fill_msg_ids(m, desk.c4_invite.from_id);
      app::send(m);
      desk.c4_invite.active = false;
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = desk.c4_invite;
      desk.c4_invite.active = false;
      desk.c4 = {};
      desk.c4.active = true;
      desk.c4.opp_color = inv.color >= 0 ? inv.color : 0;
      desk.c4.my_color = desk.c4.opp_color == 0 ? 1 : 0;
      desk.c4.turn = desk.c4.opp_color; /* challenger goes first with their color */
      games::c4::init(desk.c4.board);
      std::snprintf(desk.c4.opp_id, sizeof(desk.c4.opp_id), "%s", inv.from_id);
      std::snprintf(desk.c4.opp_name, sizeof(desk.c4.opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::C4Accept;
      fill_msg_ids(m, inv.from_id);
      m.color = desk.c4.my_color;
      app::send(m);
      go_c4();
    });
  } else if (d.c4.active && d.c4.waiting) {
    make_wait_block(body, "CHALLENGE SENT", d.c4.opp_name, "Waiting for them to accept...");
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::C4Forfeit;
      fill_msg_ids(m, desk.c4.opp_id);
      app::send(m);
      desk.c4.active = false;
      go_c4();
    });
  } else if (d.c4.active) {
    fill_c4_play(body);
    if (d.c4.over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            const int8_t color = desk.c4.my_color;
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", desk.c4.opp_id);
            std::snprintf(oname, sizeof(oname), "%s", desk.c4.opp_name);
            desk.c4 = {};
            desk.c4.active = true;
            desk.c4.waiting = true;
            desk.c4.my_color = color;
            desk.c4.turn = color;
            games::c4::init(desk.c4.board);
            std::snprintf(desk.c4.opp_id, sizeof(desk.c4.opp_id), "%s", oid);
            std::snprintf(desk.c4.opp_name, sizeof(desk.c4.opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::C4Invite;
            fill_msg_ids(m, oid);
            m.color = color;
            app::send(m);
            go_c4();
          },
          [](lv_event_t * /*e*/) {
            app::desk().c4.active = false;
            go_games_folder();
          });
    } else {
      dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
        show_forfeit_confirm([](lv_event_t * /*ev*/) {
          app::Desk & desk = app::desk();
          score_log::note("Connect Four", desk.c4.opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::C4Forfeit;
          fill_msg_ids(m, desk.c4.opp_id);
          app::send(m);
          desk.c4.active = false;
          go_games_folder();
        });
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
      lv_obj_set_style_bg_grad(sw, disc_grad(i), 0);
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
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count || app::busy() || d.bs.active || d.bs_invite.active) return;
  const app::Peer & p = d.peers[idx];
  d.bs = {};
  d.bs.active = true;
  d.bs.waiting = true;
  d.bs.i_am_first = true;
  std::snprintf(d.bs.opp_id, sizeof(d.bs.opp_id), "%s", p.id);
  std::snprintf(d.bs.opp_name, sizeof(d.bs.opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::BsInvite;
  fill_msg_ids(m, p.id);
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
  app::BsGame & g = app::desk().bs;
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
  app::BsGame & g = app::desk().bs;
  if (g.setup || !g.my_turn || g.over || g.tracking[y][x]) return;
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
  app::BsGame & g = app::desk().bs;
  constexpr int kCell = 28, kGap = 2;
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

lv_obj_t * game_bs_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  char sub[40];
  make_topbar(scr, "BATTLESHIP", d.name,
              game_vs_sub(sub, sizeof(sub), d.bs.active, d.bs.opp_name, d.bs_invite.active,
                          d.bs_invite.from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (d.bs_invite.active) {
    make_wait_block(body, "GAME INVITE", d.bs_invite.from_name, "wants to play Battleship");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::BsDecline;
      fill_msg_ids(m, desk.bs_invite.from_id);
      app::send(m);
      desk.bs_invite.active = false;
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = desk.bs_invite;
      desk.bs_invite.active = false;
      desk.bs = {};
      desk.bs.active = true;
      desk.bs.setup = true;
      desk.bs.i_am_first = false;
      games::bs::clear_fleet(desk.bs.fleet);
      std::snprintf(desk.bs.opp_id, sizeof(desk.bs.opp_id), "%s", inv.from_id);
      std::snprintf(desk.bs.opp_name, sizeof(desk.bs.opp_name), "%s", inv.from_name);
      std::snprintf(desk.bs.last_msg, sizeof(desk.bs.last_msg), "Place your fleet");
      proto::Msg m;
      m.type = proto::MsgType::BsAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_battleship();
    });
  } else if (d.bs.active && d.bs.waiting) {
    make_wait_block(body, "CHALLENGE SENT", d.bs.opp_name, "Waiting for them to accept...");
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::BsForfeit;
      fill_msg_ids(m, desk.bs.opp_id);
      app::send(m);
      desk.bs.active = false;
      go_battleship();
    });
  } else if (d.bs.active && d.bs.setup) {
    const int next = games::bs::next_ship_index(d.bs.fleet);
    if (d.bs.me_ready) {
      make_status(body, d.bs.last_msg[0] ? d.bs.last_msg : "Waiting for opponent fleet...");
    } else if (next < games::bs::kShipCount) {
      char buf[72];
      if (d.bs.anchor_x >= 0) {
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
    if (!d.bs.me_ready) {
      if (d.bs.selected_ship >= 0) {
        dock_btn(dock, "Remove", false, true, [](lv_event_t * /*e*/) {
          app::BsGame & g = app::desk().bs;
          if (g.selected_ship >= 0) {
            games::bs::remove_ship(g.fleet, g.selected_ship);
            g.selected_ship = -1;
          }
          go_battleship();
        });
      }
      dock_btn(dock, "Random", false, false, [](lv_event_t * /*e*/) {
        app::BsGame & g = app::desk().bs;
        games::bs::random_fleet(g.fleet);
        g.anchor_x = g.anchor_y = -1;
        g.selected_ship = -1;
        go_battleship();
      });
      if (games::bs::placed_count(d.bs.fleet) == games::bs::kShipCount) {
        dock_btn(dock, "Ready", true, false, [](lv_event_t * /*e*/) {
          app::BsGame & g = app::desk().bs;
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
    dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
      show_forfeit_confirm([](lv_event_t * /*ev*/) {
        app::Desk & desk = app::desk();
        score_log::note("Battleship", desk.bs.opp_name, score_log::Outcome::ForfeitSelf);
        proto::Msg m;
        m.type = proto::MsgType::BsForfeit;
        fill_msg_ids(m, desk.bs.opp_id);
        app::send(m);
        desk.bs.active = false;
        go_games_folder();
      });
    });
  } else if (d.bs.active) {
    make_status(body, d.bs.over ? (d.bs.result_dismissed ? "Play again?" : "Game over")
                                : (d.bs.last_msg[0] ? d.bs.last_msg
                                                    : (d.bs.my_turn ? "Your turn" : "Enemy turn")));
    lv_obj_t * tabs = lv_obj_create(body);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_width(tabs, lv_pct(100));
    lv_obj_set_height(tabs, 36);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tabs, 8, 0);
    auto tab = [&](const char * label, int mode) {
      lv_obj_t * b = lv_button_create(tabs);
      lv_obj_set_flex_grow(b, 1);
      lv_obj_set_style_bg_color(b, d.bs.mode == mode ? theme::gold() : theme::panel(), 0);
      lv_obj_set_style_shadow_width(b, 0, 0);
      lv_obj_t * l = lv_label_create(b);
      lv_label_set_text(l, label);
      lv_obj_set_style_text_color(l, d.bs.mode == mode ? lv_color_hex(0x1a1200) : theme::ink(), 0);
      lv_obj_center(l);
      lv_obj_add_event_cb(
          b,
          [](lv_event_t * e) {
            app::desk().bs.mode = (uint8_t)(intptr_t)lv_event_get_user_data(e);
            go_battleship();
          },
          LV_EVENT_CLICKED, (void *)(intptr_t)mode);
    };
    tab("Offense", 0);
    tab("Defense", 1);
    fill_bs_grid(body, d.bs.mode == 0);
    if (d.bs.over && !d.bs.result_dismissed) {
      attach_result_overlay(body, d.bs.i_won ? 1 : 0, [](lv_event_t * /*e*/) {
        app::desk().bs.result_dismissed = true;
        go_battleship();
      }, "Battleship", d.bs.opp_name);
    }
    if (d.bs.over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", desk.bs.opp_id);
            std::snprintf(oname, sizeof(oname), "%s", desk.bs.opp_name);
            desk.bs = {};
            desk.bs.active = true;
            desk.bs.waiting = true;
            desk.bs.i_am_first = true;
            std::snprintf(desk.bs.opp_id, sizeof(desk.bs.opp_id), "%s", oid);
            std::snprintf(desk.bs.opp_name, sizeof(desk.bs.opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::BsInvite;
            fill_msg_ids(m, oid);
            app::send(m);
            go_battleship();
          },
          [](lv_event_t * /*e*/) {
            app::desk().bs.active = false;
            go_games_folder();
          });
    } else {
      dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
        show_forfeit_confirm([](lv_event_t * /*ev*/) {
          app::Desk & desk = app::desk();
          score_log::note("Battleship", desk.bs.opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::BsForfeit;
          fill_msg_ids(m, desk.bs.opp_id);
          app::send(m);
          desk.bs.active = false;
          go_games_folder();
        });
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
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count || app::busy() || d.ck.active || d.ck_invite.active) return;
  const app::Peer & p = d.peers[idx];
  d.ck = {};
  d.ck.active = true;
  d.ck.waiting = true;
  d.ck.side = 'r';
  d.ck.turn = 'r';
  games::ck::init(d.ck.board);
  std::snprintf(d.ck.opp_id, sizeof(d.ck.opp_id), "%s", p.id);
  std::snprintf(d.ck.opp_name, sizeof(d.ck.opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::CkInvite;
  fill_msg_ids(m, p.id);
  app::send(m);
  go_checkers();
}

void ck_tap(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  int vx = packed % 8, vy = packed / 8;
  app::CkGame & g = app::desk().ck;
  if (!g.active || g.waiting || g.over || g.turn != g.side) return;

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
  app::CkGame & g = app::desk().ck;
  const bool my_turn = !g.over && !g.waiting && g.turn == g.side;
  if (g.over && !g.result_dismissed) make_status(parent, "Game over");
  else if (g.over) make_status(parent, "Play again?");
  else if (my_turn) make_status(parent, g.must_x >= 0 ? "Continue jump!" : "Your turn");
  else {
    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Waiting on %s...", g.opp_name);
    make_status(parent, buf);
  }

  constexpr int kCell = 40;
  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 8 * kCell, 8 * kCell);
  lv_obj_set_style_radius(board, 12, 0);
  lv_obj_set_style_clip_corner(board, true, 0);
  lv_obj_set_style_border_width(board, 6, 0);
  lv_obj_set_style_border_color(board, lv_color_hex(0x4a1828), 0);
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
      lv_obj_set_style_bg_color(cell, dark ? lv_color_hex(0x6b3a55) : lv_color_hex(0xf0e0c8), 0);
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
        lv_obj_set_size(piece, 28, 28);
        lv_obj_set_style_radius(piece, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(piece, LV_OPA_COVER, 0);
        if (games::ck::is_red(p)) {
          lv_obj_set_style_bg_color(piece, lv_color_hex(0xc41e3a), 0);
        } else {
          lv_obj_set_style_bg_color(piece, lv_color_hex(0x1a1220), 0);
          lv_obj_set_style_border_width(piece, 2, 0);
          lv_obj_set_style_border_color(piece, theme::gold(), 0);
        }
        if (games::ck::is_king(p)) {
          lv_obj_t * k = lv_label_create(piece);
          lv_label_set_text(k, "K");
          lv_obj_set_style_text_color(k, theme::gold(), 0);
          lv_obj_set_style_text_font(k, &lv_font_montserrat_12, 0);
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
      app::desk().ck.result_dismissed = true;
      go_checkers();
    }, "Checkers", g.opp_name);
  }
}

lv_obj_t * game_ck_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  char sub[40];
  make_topbar(scr, "CHECKERS", d.name,
              game_vs_sub(sub, sizeof(sub), d.ck.active, d.ck.opp_name, d.ck_invite.active,
                          d.ck_invite.from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (d.ck_invite.active) {
    make_wait_block(body, "GAME INVITE", d.ck_invite.from_name, "wants to play Checkers");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::CkDecline;
      fill_msg_ids(m, desk.ck_invite.from_id);
      app::send(m);
      desk.ck_invite.active = false;
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = desk.ck_invite;
      desk.ck_invite.active = false;
      desk.ck = {};
      desk.ck.active = true;
      desk.ck.side = 'b';
      desk.ck.turn = 'r';
      games::ck::init(desk.ck.board);
      std::snprintf(desk.ck.opp_id, sizeof(desk.ck.opp_id), "%s", inv.from_id);
      std::snprintf(desk.ck.opp_name, sizeof(desk.ck.opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::CkAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_checkers();
    });
  } else if (d.ck.active && d.ck.waiting) {
    make_wait_block(body, "CHALLENGE SENT", d.ck.opp_name, "Waiting for them to accept...");
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::CkForfeit;
      fill_msg_ids(m, desk.ck.opp_id);
      app::send(m);
      desk.ck.active = false;
      go_checkers();
    });
  } else if (d.ck.active) {
    fill_ck_play(body);
    if (d.ck.over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", desk.ck.opp_id);
            std::snprintf(oname, sizeof(oname), "%s", desk.ck.opp_name);
            desk.ck = {};
            desk.ck.active = true;
            desk.ck.waiting = true;
            desk.ck.side = 'r';
            desk.ck.turn = 'r';
            games::ck::init(desk.ck.board);
            std::snprintf(desk.ck.opp_id, sizeof(desk.ck.opp_id), "%s", oid);
            std::snprintf(desk.ck.opp_name, sizeof(desk.ck.opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::CkInvite;
            fill_msg_ids(m, oid);
            app::send(m);
            go_checkers();
          },
          [](lv_event_t * /*e*/) {
            app::desk().ck.active = false;
            go_games_folder();
          });
    } else {
      dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
        show_forfeit_confirm([](lv_event_t * /*ev*/) {
          app::Desk & desk = app::desk();
          score_log::note("Checkers", desk.ck.opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::CkForfeit;
          fill_msg_ids(m, desk.ck.opp_id);
          app::send(m);
          desk.ck.active = false;
          go_games_folder();
        });
      });
    }
  } else {
    peer_list(body, ck_challenge);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  }
  return scr;
}

/* ================= MEMORY ================= */

std::string mem_face_path(uint8_t pair) {
  namespace fs = std::filesystem;
  pair = (uint8_t)(pair % games::mem::kPairs);
  static std::string cache[games::mem::kPairs];
  static bool ready[games::mem::kPairs] = {};
  if (ready[pair]) return cache[pair];

  const char * name = games::mem::face_names()[pair];
  const fs::path candidates[] = {
      fs::current_path() / "assets" / "memory",
      fs::current_path() / ".." / "assets" / "memory",
      fs::path("C:/Users/Tommy/Projects/WerkPager/firmware/assets/memory"),
  };
  for (const auto & dir : candidates) {
    /* Prefer PNG (LV_USE_LODEPNG); fall back to jpg copies if present. */
    const char * exts[] = {".png", ".jpg", ".jpeg"};
    for (const char * ext : exts) {
      fs::path p = dir / (std::string(name) + ext);
      std::error_code ec;
      if (!fs::exists(p, ec)) continue;
      fs::path canon = fs::weakly_canonical(p, ec);
      if (ec) canon = fs::absolute(p, ec);
      if (ec) continue;
      std::string s = "S:" + canon.string();
      for (char & c : s)
        if (c == '\\') c = '/';
      /* Confirm LVGL can decode before claiming success. */
      lv_image_header_t hdr{};
      if (lv_image_decoder_get_info(s.c_str(), &hdr) != LV_RESULT_OK || hdr.w == 0) continue;
      cache[pair] = std::move(s);
      ready[pair] = true;
      return cache[pair];
    }
  }
  ready[pair] = true; /* negative-cache empty */
  return cache[pair];
}

struct MemResolveLocal {
  int8_t a, b;
};

void mem_resolve_local(void * ud) {
  auto * r = static_cast<MemResolveLocal *>(ud);
  app::MemGame & g = app::desk().mem;
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
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count || app::busy() || d.mem.active || d.mem_invite.active) return;
  const app::Peer & p = d.peers[idx];
  d.mem = {};
  d.mem.active = true;
  d.mem.waiting = true;
  d.mem.seed = (uint32_t)std::rand();
  d.mem.my_turn = true;
  games::mem::build_deck(d.mem.seed, d.mem.deck);
  std::snprintf(d.mem.opp_id, sizeof(d.mem.opp_id), "%s", p.id);
  std::snprintf(d.mem.opp_name, sizeof(d.mem.opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::MemInvite;
  fill_msg_ids(m, p.id);
  m.seed = d.mem.seed;
  app::send(m);
  go_memory();
}

void mem_flip(lv_event_t * e) {
  const int card = (int)(intptr_t)lv_event_get_user_data(e);
  app::MemGame & g = app::desk().mem;
  if (!g.active || g.waiting || g.over || !g.my_turn || g.lock) return;
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
  app::MemGame & g = app::desk().mem;
  char status[64];
  lv_snprintf(status, sizeof(status), "You %d - %s %d%s", g.my_score, g.opp_name, g.opp_score,
              g.my_turn && !g.lock ? " - your turn" : "");
  make_status(parent, status);

  constexpr int kCard = 72, kGap = 8;
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
        lv_image_set_src(img, path.c_str());
        /* Faces are authored at 96px; scale to card inset. Avoid set_size — it
         * can clip/blank scaled file images in LVGL 9. */
        const int32_t scale = ((kCard - 8) * 256) / 96;
        lv_image_set_scale(img, scale);
        lv_obj_center(img);
        lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
        shown = true;
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
        lv_obj_set_style_text_font(l, font_display(28), 0);
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
      app::desk().mem.result_dismissed = true;
      go_memory();
    }, "Memory", g.opp_name);
  }
}

lv_obj_t * game_mem_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  char sub[40];
  make_topbar(scr, "MEMORY", d.name,
              game_vs_sub(sub, sizeof(sub), d.mem.active, d.mem.opp_name, d.mem_invite.active,
                          d.mem_invite.from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (d.mem_invite.active) {
    make_wait_block(body, "GAME INVITE", d.mem_invite.from_name, "wants to match pairs");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::MemDecline;
      fill_msg_ids(m, desk.mem_invite.from_id);
      app::send(m);
      desk.mem_invite.active = false;
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = desk.mem_invite;
      desk.mem_invite.active = false;
      desk.mem = {};
      desk.mem.active = true;
      desk.mem.seed = inv.seed;
      desk.mem.my_turn = false;
      games::mem::build_deck(desk.mem.seed, desk.mem.deck);
      std::snprintf(desk.mem.opp_id, sizeof(desk.mem.opp_id), "%s", inv.from_id);
      std::snprintf(desk.mem.opp_name, sizeof(desk.mem.opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::MemAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_memory();
    });
  } else if (d.mem.active && d.mem.waiting) {
    make_wait_block(body, "CHALLENGE SENT", d.mem.opp_name, "Waiting for them to accept...");
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::MemForfeit;
      fill_msg_ids(m, desk.mem.opp_id);
      app::send(m);
      desk.mem.active = false;
      go_memory();
    });
  } else if (d.mem.active) {
    fill_mem_play(body);
    if (d.mem.over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", desk.mem.opp_id);
            std::snprintf(oname, sizeof(oname), "%s", desk.mem.opp_name);
            desk.mem = {};
            desk.mem.active = true;
            desk.mem.waiting = true;
            desk.mem.seed = (uint32_t)std::rand();
            desk.mem.my_turn = true;
            games::mem::build_deck(desk.mem.seed, desk.mem.deck);
            std::snprintf(desk.mem.opp_id, sizeof(desk.mem.opp_id), "%s", oid);
            std::snprintf(desk.mem.opp_name, sizeof(desk.mem.opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::MemInvite;
            fill_msg_ids(m, oid);
            m.seed = desk.mem.seed;
            app::send(m);
            go_memory();
          },
          [](lv_event_t * /*e*/) {
            app::desk().mem.active = false;
            go_games_folder();
          });
    } else {
      dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
        show_forfeit_confirm([](lv_event_t * /*ev*/) {
          app::Desk & desk = app::desk();
          score_log::note("Memory", desk.mem.opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::MemForfeit;
          fill_msg_ids(m, desk.mem.opp_id);
          app::send(m);
          desk.mem.active = false;
          go_games_folder();
        });
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
  app::RvGame & g = app::desk().rv;
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
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count || app::busy() || d.rv.active || d.rv_invite.active) return;
  const app::Peer & p = d.peers[idx];
  d.rv = {};
  d.rv.active = true;
  d.rv.waiting = true;
  d.rv.my_color = games::rv::kBlack;
  d.rv.turn = games::rv::kBlack;
  games::rv::init(d.rv.board);
  std::snprintf(d.rv.opp_id, sizeof(d.rv.opp_id), "%s", p.id);
  std::snprintf(d.rv.opp_name, sizeof(d.rv.opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::RvInvite;
  fill_msg_ids(m, p.id);
  app::send(m);
  go_reversi();
}

void rv_play_cell(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  const int r = packed / games::rv::kN;
  const int c = packed % games::rv::kN;
  app::RvGame & g = app::desk().rv;
  if (!g.active || g.waiting || g.over || g.turn != g.my_color) return;
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
  app::RvGame & g = app::desk().rv;
  const bool my_turn = !g.over && !g.waiting && g.turn == g.my_color;
  const bool can_move = games::rv::any_move(g.board, g.my_color);

  if (my_turn && !can_move && !g.over) {
    if (games::rv::check_over(g.board) != 0) {
      g.over = true;
      g.result_dismissed = false;
    } else {
      rv_send_pass();
    }
    go_reversi();
    return;
  }

  int bl = 0, wh = 0;
  games::rv::count_pieces(g.board, &bl, &wh);
  char score[48];
  lv_snprintf(score, sizeof(score), "● %d   ○ %d", bl, wh);
  make_status(parent, score);

  if (g.over && !g.result_dismissed) make_status(parent, "Game over");
  else if (g.over) make_status(parent, "Play again?");
  else if (my_turn) make_status(parent, "Your turn");
  else {
    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Waiting on %s...", g.opp_name);
    make_status(parent, buf);
  }

  constexpr int kCell = 40;
  constexpr int kGap = 3;
  constexpr int kPad = 8;
  constexpr int kBoard = kPad * 2 + games::rv::kN * kCell + (games::rv::kN - 1) * kGap;

  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, kBoard, kBoard);
  lv_obj_set_style_radius(board, 12, 0);
  lv_obj_set_style_bg_color(board, lv_color_hex(0x0f3d26), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(board, kPad, 0);
  lv_obj_set_layout(board, LV_LAYOUT_GRID);
  static lv_coord_t cols[] = {kCell, kCell, kCell, kCell, kCell, kCell, kCell, kCell, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {kCell, kCell, kCell, kCell, kCell, kCell, kCell, kCell, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(board, cols, rows);
  lv_obj_set_style_pad_row(board, kGap, 0);
  lv_obj_set_style_pad_column(board, kGap, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  for (int r = 0; r < games::rv::kN; ++r) {
    for (int c = 0; c < games::rv::kN; ++c) {
      lv_obj_t * cell = lv_obj_create(board);
      lv_obj_remove_style_all(cell);
      lv_obj_set_style_radius(cell, 4, 0);
      lv_obj_set_style_bg_color(cell, lv_color_hex(0x1a5c3a), 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, c, 1, LV_GRID_ALIGN_STRETCH, r, 1);
      lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

      const int8_t piece = g.board[r][c];
      if (piece == games::rv::kBlack || piece == games::rv::kWhite) {
        lv_obj_t * disc = lv_obj_create(cell);
        lv_obj_remove_style_all(disc);
        lv_obj_set_size(disc, kCell - 8, kCell - 8);
        lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(disc, piece == games::rv::kBlack ? lv_color_hex(0x1a1a1a) : lv_color_hex(0xf2f2f2),
                                  0);
        lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
        lv_obj_center(disc);
        lv_obj_remove_flag(disc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
      } else if (my_turn && games::rv::would_flip(g.board, r, c, g.my_color) > 0) {
        lv_obj_set_style_border_width(cell, 2, 0);
        lv_obj_set_style_border_color(cell, theme::gold(), 0);
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cell, rv_play_cell, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(r * games::rv::kN + c));
      }
    }
  }

  if (g.over && !g.result_dismissed) {
    const int8_t w = games::rv::check_over(g.board);
    const int outcome = (w < 0) ? -1 : (w == g.my_color ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::desk().rv.result_dismissed = true;
      go_reversi();
    }, "Reversi", g.opp_name);
  }
}

lv_obj_t * game_rv_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  char sub[40];
  make_topbar(scr, "REVERSI", d.name,
              game_vs_sub(sub, sizeof(sub), d.rv.active, d.rv.opp_name, d.rv_invite.active,
                          d.rv_invite.from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (d.rv_invite.active) {
    make_wait_block(body, "GAME INVITE", d.rv_invite.from_name, "wants to play Reversi");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::RvDecline;
      fill_msg_ids(m, desk.rv_invite.from_id);
      app::send(m);
      desk.rv_invite.active = false;
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = desk.rv_invite;
      desk.rv_invite.active = false;
      desk.rv = {};
      desk.rv.active = true;
      desk.rv.my_color = games::rv::kWhite;
      desk.rv.turn = games::rv::kBlack;
      games::rv::init(desk.rv.board);
      std::snprintf(desk.rv.opp_id, sizeof(desk.rv.opp_id), "%s", inv.from_id);
      std::snprintf(desk.rv.opp_name, sizeof(desk.rv.opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::RvAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_reversi();
    });
  } else if (d.rv.active && d.rv.waiting) {
    make_wait_block(body, "CHALLENGE SENT", d.rv.opp_name, "Waiting for them to accept...");
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::RvForfeit;
      fill_msg_ids(m, desk.rv.opp_id);
      app::send(m);
      desk.rv.active = false;
      go_reversi();
    });
  } else if (d.rv.active) {
    fill_rv_play(body);
    if (d.rv.over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            const int8_t color = desk.rv.my_color;
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", desk.rv.opp_id);
            std::snprintf(oname, sizeof(oname), "%s", desk.rv.opp_name);
            desk.rv = {};
            desk.rv.active = true;
            desk.rv.waiting = true;
            desk.rv.my_color = color;
            desk.rv.turn = games::rv::kBlack;
            games::rv::init(desk.rv.board);
            std::snprintf(desk.rv.opp_id, sizeof(desk.rv.opp_id), "%s", oid);
            std::snprintf(desk.rv.opp_name, sizeof(desk.rv.opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::RvInvite;
            fill_msg_ids(m, oid);
            app::send(m);
            go_reversi();
          },
          [](lv_event_t * /*e*/) {
            app::desk().rv.active = false;
            go_hub();
          });
    } else {
      dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
        show_forfeit_confirm([](lv_event_t * /*ev*/) {
          app::Desk & desk = app::desk();
          score_log::note("Reversi", desk.rv.opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::RvForfeit;
          fill_msg_ids(m, desk.rv.opp_id);
          app::send(m);
          desk.rv.active = false;
          go_games_folder();
        });
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
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count || app::busy() || d.db.active || d.db_invite.active) return;
  const app::Peer & p = d.peers[idx];
  d.db = {};
  d.db.active = true;
  d.db.waiting = true;
  d.db.my_side = games::db::kP1;
  d.db.turn = games::db::kP1;
  games::db::init(d.db.state);
  std::snprintf(d.db.opp_id, sizeof(d.db.opp_id), "%s", p.id);
  std::snprintf(d.db.opp_name, sizeof(d.db.opp_name), "%s", p.name);
  proto::Msg m;
  m.type = proto::MsgType::DbInvite;
  fill_msg_ids(m, p.id);
  app::send(m);
  go_dots();
}

void db_play_edge(lv_event_t * e) {
  const int packed = (int)(intptr_t)lv_event_get_user_data(e);
  const int is_vert = (packed >> 16) & 1;
  const int r = (packed >> 8) & 0xff;
  const int c = packed & 0xff;
  app::DbGame & g = app::desk().db;
  if (!g.active || g.waiting || g.over || g.turn != g.my_side) return;
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
  app::DbGame & g = app::desk().db;
  const bool my_turn = !g.over && !g.waiting && g.turn == g.my_side;

  char score[48];
  lv_snprintf(score, sizeof(score), "You %d  ·  %s %d",
              g.my_side == games::db::kP1 ? g.state.score1 : g.state.score2, g.opp_name,
              g.my_side == games::db::kP1 ? g.state.score2 : g.state.score1);
  make_status(parent, score);

  if (g.over && !g.result_dismissed) make_status(parent, "Game over");
  else if (g.over) make_status(parent, "Play again?");
  else if (my_turn) make_status(parent, "Your turn - tap a line");
  else {
    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Waiting on %s...", g.opp_name);
    make_status(parent, buf);
  }

  constexpr int kStep = 64;
  constexpr int kDot = 10;
  constexpr int kThick = 10;
  constexpr int kBoard = kStep * games::db::kBox + kDot + 8;
  const int origin = 4;

  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, kBoard, kBoard);
  lv_obj_set_style_radius(board, 12, 0);
  lv_obj_set_style_bg_color(board, theme::panel(), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  auto owner_color = [&](int8_t owner) -> lv_color_t {
    if (owner == g.my_side) return theme::mint();
    if (owner) return theme::hot();
    return theme::border();
  };

  for (int br = 0; br < games::db::kBox; ++br) {
    for (int bc = 0; bc < games::db::kBox; ++bc) {
      if (!g.state.box[br][bc]) continue;
      lv_obj_t * box = lv_obj_create(board);
      lv_obj_remove_style_all(box);
      lv_obj_set_pos(box, origin + bc * kStep + kDot / 2, origin + br * kStep + kDot / 2);
      lv_obj_set_size(box, kStep - kDot / 2, kStep - kDot / 2);
      lv_obj_set_style_bg_color(box, owner_color(g.state.box[br][bc]), 0);
      lv_obj_set_style_bg_opa(box, LV_OPA_50, 0);
      lv_obj_set_style_radius(box, 6, 0);
      lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    }
  }

  auto edge_hit = [&](int is_vert, int r, int c, int x, int y, int w, int h, bool taken) {
    lv_obj_t * hit = lv_obj_create(board);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, x, y);
    lv_obj_set_size(hit, w, h);
    lv_obj_set_style_radius(hit, 4, 0);
    if (taken) {
      lv_obj_set_style_bg_color(hit, theme::ink(), 0);
      lv_obj_set_style_bg_opa(hit, LV_OPA_COVER, 0);
    } else if (my_turn) {
      lv_obj_set_style_bg_color(hit, theme::gold(), 0);
      lv_obj_set_style_bg_opa(hit, LV_OPA_40, 0);
      lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(hit, db_play_edge, LV_EVENT_CLICKED,
                          (void *)(intptr_t)((is_vert << 16) | (r << 8) | c));
    } else {
      lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
    }
    lv_obj_remove_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
  };

  for (int r = 0; r < games::db::kDots; ++r) {
    for (int c = 0; c < games::db::kBox; ++c) {
      const bool taken = games::db::h_taken(g.state, r, c);
      edge_hit(0, r, c, origin + c * kStep + kDot / 2, origin + r * kStep + (kDot - kThick) / 2,
               kStep - kDot / 2, kThick, taken);
    }
  }
  for (int r = 0; r < games::db::kBox; ++r) {
    for (int c = 0; c < games::db::kDots; ++c) {
      const bool taken = games::db::v_taken(g.state, r, c);
      edge_hit(1, r, c, origin + c * kStep + (kDot - kThick) / 2, origin + r * kStep + kDot / 2, kThick,
               kStep - kDot / 2, taken);
    }
  }

  for (int r = 0; r < games::db::kDots; ++r) {
    for (int c = 0; c < games::db::kDots; ++c) {
      lv_obj_t * dot = lv_obj_create(board);
      lv_obj_remove_style_all(dot);
      lv_obj_set_pos(dot, origin + c * kStep, origin + r * kStep);
      lv_obj_set_size(dot, kDot, kDot);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, theme::gold(), 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }
  }

  if (g.over && !g.result_dismissed) {
    const int8_t w = games::db::winner(g.state);
    const int outcome = (w < 0) ? -1 : (w == g.my_side ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::desk().db.result_dismissed = true;
      go_dots();
    }, "Dots & Boxes", g.opp_name);
  }
}

lv_obj_t * game_db_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  char sub[40];
  make_topbar(scr, "DOTS & BOXES", d.name,
              game_vs_sub(sub, sizeof(sub), d.db.active, d.db.opp_name, d.db_invite.active,
                          d.db_invite.from_name));
  lv_obj_t * body = make_body(scr, true);
  lv_obj_t * dock = make_dock(scr);

  if (d.db_invite.active) {
    make_wait_block(body, "GAME INVITE", d.db_invite.from_name, "wants to play Dots & Boxes");
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::DbDecline;
      fill_msg_ids(m, desk.db_invite.from_id);
      app::send(m);
      desk.db_invite.active = false;
      go_games_folder();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      const auto inv = desk.db_invite;
      desk.db_invite.active = false;
      desk.db = {};
      desk.db.active = true;
      desk.db.my_side = games::db::kP2;
      desk.db.turn = games::db::kP1;
      games::db::init(desk.db.state);
      std::snprintf(desk.db.opp_id, sizeof(desk.db.opp_id), "%s", inv.from_id);
      std::snprintf(desk.db.opp_name, sizeof(desk.db.opp_name), "%s", inv.from_name);
      proto::Msg m;
      m.type = proto::MsgType::DbAccept;
      fill_msg_ids(m, inv.from_id);
      app::send(m);
      go_dots();
    });
  } else if (d.db.active && d.db.waiting) {
    make_wait_block(body, "CHALLENGE SENT", d.db.opp_name, "Waiting for them to accept...");
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::Desk & desk = app::desk();
      proto::Msg m;
      m.type = proto::MsgType::DbForfeit;
      fill_msg_ids(m, desk.db.opp_id);
      app::send(m);
      desk.db.active = false;
      go_dots();
    });
  } else if (d.db.active) {
    fill_db_play(body);
    if (d.db.over) {
      dock_play_again_home(
          dock,
          [](lv_event_t * /*e*/) {
            app::Desk & desk = app::desk();
            const int8_t side = desk.db.my_side;
            char oid[proto::kMaxId], oname[proto::kMaxName];
            std::snprintf(oid, sizeof(oid), "%s", desk.db.opp_id);
            std::snprintf(oname, sizeof(oname), "%s", desk.db.opp_name);
            desk.db = {};
            desk.db.active = true;
            desk.db.waiting = true;
            desk.db.my_side = side;
            desk.db.turn = games::db::kP1;
            games::db::init(desk.db.state);
            std::snprintf(desk.db.opp_id, sizeof(desk.db.opp_id), "%s", oid);
            std::snprintf(desk.db.opp_name, sizeof(desk.db.opp_name), "%s", oname);
            proto::Msg m;
            m.type = proto::MsgType::DbInvite;
            fill_msg_ids(m, oid);
            app::send(m);
            go_dots();
          },
          [](lv_event_t * /*e*/) {
            app::desk().db.active = false;
            go_hub();
          });
    } else {
      dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) {
        show_forfeit_confirm([](lv_event_t * /*ev*/) {
          app::Desk & desk = app::desk();
          score_log::note("Dots & Boxes", desk.db.opp_name, score_log::Outcome::ForfeitSelf);
          proto::Msg m;
          m.type = proto::MsgType::DbForfeit;
          fill_msg_ids(m, desk.db.opp_id);
          app::send(m);
          desk.db.active = false;
          go_games_folder();
        });
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
  static lv_coord_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(grid, cols, rows);
  lv_obj_set_style_pad_row(grid, 12, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  struct Item {
    AppIcon icon;
    const char * label;
    lv_event_cb_t cb;
  };
  const Item items[] = {
      {AppIcon::Ttt, "Tic Tac Toe", [](lv_event_t * /*e*/) { go_ttt(); }},
      {AppIcon::Sttt, "Super TTT", [](lv_event_t * /*e*/) { go_sttt(); }},
      {AppIcon::C4, "Connect Four", [](lv_event_t * /*e*/) { go_c4(); }},
      {AppIcon::Battleship, "Battleship", [](lv_event_t * /*e*/) { go_battleship(); }},
      {AppIcon::Checkers, "Checkers", [](lv_event_t * /*e*/) { go_checkers(); }},
      {AppIcon::Memory, "Memory", [](lv_event_t * /*e*/) { go_memory(); }},
      {AppIcon::Reversi, "Reversi", [](lv_event_t * /*e*/) { go_reversi(); }},
      {AppIcon::Dots, "Dots & Boxes", [](lv_event_t * /*e*/) { go_dots(); }},
      {AppIcon::Scoreboard, "Scoreboard", [](lv_event_t * /*e*/) { go_scoreboard(); }},
  };
  for (int i = 0; i < 9; ++i) {
    lv_obj_t * icon = make_app_icon(grid, items[i].icon, items[i].label, items[i].cb);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, i % 3, 1, LV_GRID_ALIGN_START, i / 3, 1);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
  return scr;
}

lv_obj_t * game_ttt_screen() { return game_ttt_build(); }
lv_obj_t * game_sttt_screen() { return game_sttt_build(); }
lv_obj_t * game_c4_screen() { return game_c4_build(); }
lv_obj_t * game_bs_screen() { return game_bs_build(); }
lv_obj_t * game_ck_screen() { return game_ck_build(); }
lv_obj_t * game_mem_screen() { return game_mem_build(); }
lv_obj_t * game_rv_screen() { return game_rv_build(); }
lv_obj_t * game_db_screen() { return game_db_build(); }

void games_debug_show(const char * game, const char * panel) {
  app::Desk & d = app::desk();
  auto seed_peer = [&](auto & g) {
    std::snprintf(g.opp_id, sizeof(g.opp_id), "mac-will");
    std::snprintf(g.opp_name, sizeof(g.opp_name), "Will");
  };

  if (game && !std::strcmp(game, "ttt")) {
    d.ttt = {};
    d.ttt.active = true;
    d.ttt.waiting = panel && !std::strcmp(panel, "wait");
    d.ttt.mark = 'X';
    d.ttt.turn = 'X';
    seed_peer(d.ttt);
    if (panel && !std::strcmp(panel, "play")) {
      d.ttt.board[0] = 'X';
      d.ttt.board[4] = 'O';
    } else if (panel && !std::strcmp(panel, "win")) {
      d.ttt.board[0] = d.ttt.board[1] = d.ttt.board[2] = 'X';
      d.ttt.board[3] = d.ttt.board[4] = 'O';
      d.ttt.over = true;
      d.ttt.result_dismissed = false;
    } else if (panel && !std::strcmp(panel, "lose")) {
      d.ttt.board[0] = d.ttt.board[1] = d.ttt.board[2] = 'O';
      d.ttt.board[3] = d.ttt.board[4] = 'X';
      d.ttt.over = true;
      d.ttt.result_dismissed = false;
    }
    go_ttt();
  } else if (game && !std::strcmp(game, "c4")) {
    d.c4 = {};
    d.c4.active = true;
    d.c4.waiting = panel && !std::strcmp(panel, "wait");
    d.c4.my_color = 0;
    d.c4.opp_color = 1;
    d.c4.turn = 0;
    games::c4::init(d.c4.board);
    seed_peer(d.c4);
    if (panel && !std::strcmp(panel, "win")) {
      for (int r = 5; r >= 2; --r) d.c4.board[r][0] = 0;
      d.c4.last_r = 2;
      d.c4.last_c = 0;
      d.c4.over = true;
      d.c4.result_dismissed = false;
    }
    go_c4();
  } else if (game && !std::strcmp(game, "bs")) {
    d.bs = {};
    d.bs.active = true;
    d.bs.setup = !(panel && (!std::strcmp(panel, "play") || !std::strcmp(panel, "defense") ||
                             !std::strcmp(panel, "win")));
    d.bs.my_turn = true;
    d.bs.mode = (panel && !std::strcmp(panel, "defense")) ? 1 : 0;
    if (d.bs.setup) {
      std::snprintf(d.bs.last_msg, sizeof(d.bs.last_msg), "Place your fleet");
    } else {
      games::bs::random_fleet(d.bs.fleet);
      std::snprintf(d.bs.last_msg, sizeof(d.bs.last_msg), "Your turn - tap to fire!");
    }
    if (panel && !std::strcmp(panel, "win")) {
      d.bs.over = true;
      d.bs.i_won = true;
      d.bs.result_dismissed = false;
      d.bs.setup = false;
    }
    seed_peer(d.bs);
    go_battleship();
  } else if (game && !std::strcmp(game, "ck")) {
    d.ck = {};
    d.ck.active = true;
    d.ck.side = 'r';
    d.ck.turn = 'r';
    games::ck::init(d.ck.board);
    seed_peer(d.ck);
    if (panel && !std::strcmp(panel, "win")) {
      /* Clear opponent pieces so red wins. */
      for (int y = 0; y < games::ck::kSize; ++y)
        for (int x = 0; x < games::ck::kSize; ++x)
          if (d.ck.board[y][x] == 'b' || d.ck.board[y][x] == 'B') d.ck.board[y][x] = 0;
      d.ck.over = true;
      d.ck.result_dismissed = false;
    }
    go_checkers();
  } else if (game && !std::strcmp(game, "mem")) {
    d.mem = {};
    d.mem.active = true;
    d.mem.seed = 42;
    d.mem.my_turn = true;
    games::mem::build_deck(d.mem.seed, d.mem.deck);
    seed_peer(d.mem);
    if (panel && !std::strcmp(panel, "play")) {
      /* Face a full row so asset decode is visible in screenshots. */
      for (int i = 0; i < 4; ++i) d.mem.matched[i] = true;
    }
    go_memory();
  }
}

}  // namespace ui
}  // namespace wp
