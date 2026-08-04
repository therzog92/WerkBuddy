#include "ui/scr_games.h"

#include "app/app.h"
#include "games/battleship.h"
#include "games/c4.h"
#include "games/checkers.h"
#include "games/memory.h"
#include "games/ttt.h"
#include "protocol/messages.h"
#include "ui/chrome.h"
#include "ui/emoji_badge.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/nav.h"
#include "ui/theme.h"

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

/** Win / lose / draw celebration card (web .ttt-result). Floating over the board. */
void attach_result_overlay(lv_obj_t * parent, int outcome /*1 win, 0 lose, -1 draw*/,
                           lv_event_cb_t on_dismiss) {
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
  if (idx < 0 || idx >= d.peer_count || d.ttt.active || d.ttt_invite.active) return;
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
    });
  }
}

lv_obj_t * game_ttt_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "TIC TAC TOE", d.name);
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
  if (idx < 0 || idx >= d.peer_count || d.c4.active || d.c4_invite.active) return;
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

  constexpr int kCell = 44, kGap = 4, kPad = 10;
  constexpr int kBoardW = 7 * kCell + 6 * kGap + 2 * kPad;
  constexpr int kBoardH = 6 * kCell + 5 * kGap + 2 * kPad;
  lv_obj_t * board = lv_obj_create(parent);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, kBoardW, kBoardH);
  lv_obj_set_style_radius(board, 16, 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_grad(board, c4_rainbow(), 0);
  lv_obj_set_style_border_width(board, 3, 0);
  lv_obj_set_style_border_color(board, lv_color_hex(0x8d7fa0), 0);
  lv_obj_set_style_pad_all(board, kPad, 0);
  /* Allow drop animation to travel from above the board (web .c4-dropping). */
  lv_obj_set_style_clip_corner(board, false, 0);
  lv_obj_add_flag(board, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_layout(board, LV_LAYOUT_GRID);
  static lv_coord_t cols[8], rows[7];
  for (int i = 0; i < 7; ++i) cols[i] = kCell;
  cols[7] = LV_GRID_TEMPLATE_LAST;
  for (int i = 0; i < 6; ++i) rows[i] = kCell;
  rows[6] = LV_GRID_TEMPLATE_LAST;
  lv_obj_set_grid_dsc_array(board, cols, rows);
  lv_obj_set_style_pad_row(board, kGap, 0);
  lv_obj_set_style_pad_column(board, kGap, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  for (int r = 0; r < 6; ++r) {
    for (int c = 0; c < 7; ++c) {
      lv_obj_t * hole = lv_obj_create(board);
      lv_obj_remove_style_all(hole);
      lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(hole, lv_color_hex(0x1a1228), 0);
      lv_obj_set_style_bg_opa(hole, LV_OPA_COVER, 0);
      lv_obj_set_grid_cell(hole, LV_GRID_ALIGN_CENTER, c, 1, LV_GRID_ALIGN_CENTER, r, 1);
      lv_obj_set_size(hole, kCell, kCell);
      lv_obj_remove_flag(hole, LV_OBJ_FLAG_SCROLLABLE);
      if (g.board[r][c] >= 0) {
        lv_obj_set_style_bg_grad(hole, disc_grad(g.board[r][c]), 0);
        /* Drop animation for the most recent disc (web .c4-dropping). */
        if (g.last_r == r && g.last_c == c) {
          const int drop_px = (r + 1) * (kCell + kGap) + kPad;
          lv_obj_set_style_translate_y(hole, -drop_px, 0);
          lv_anim_t a;
          lv_anim_init(&a);
          lv_anim_set_var(&a, hole);
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
      } else if (my_turn) {
        lv_obj_add_flag(hole, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(hole, c4_drop, LV_EVENT_CLICKED, (void *)(intptr_t)c);
      }
    }
  }

  if (g.over && !g.result_dismissed) {
    const int outcome = (w == games::c4::kColorCount) ? -1 : (w == g.my_color ? 1 : 0);
    attach_result_overlay(parent, outcome, [](lv_event_t * /*e*/) {
      app::desk().c4.result_dismissed = true;
      go_c4();
    });
  }
}

lv_obj_t * game_c4_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "CONNECT FOUR", d.name);
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
  if (idx < 0 || idx >= d.peer_count || d.bs.active || d.bs_invite.active) return;
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
  make_topbar(scr, "BATTLESHIP", d.name);
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
      });
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
  if (idx < 0 || idx >= d.peer_count || d.ck.active || d.ck_invite.active) return;
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
    });
  }
}

lv_obj_t * game_ck_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "CHECKERS", d.name);
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
  const char * name = games::mem::face_names()[pair % games::mem::kPairs];
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
      return s;
    }
  }
  return {};
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
  if (idx < 0 || idx >= d.peer_count || d.mem.active || d.mem_invite.active) return;
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
      const std::string path = mem_face_path(g.deck[i]);
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
    });
  }
}

lv_obj_t * game_mem_build() {
  app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "MEMORY", d.name);
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
  static lv_coord_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(grid, cols, rows);
  lv_obj_set_style_pad_row(grid, 16, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  struct Item {
    AppIcon icon;
    const char * label;
    lv_event_cb_t cb;
  };
  const Item items[] = {
      {AppIcon::Ttt, "Tic Tac Toe", [](lv_event_t * /*e*/) { go_ttt(); }},
      {AppIcon::C4, "Connect Four", [](lv_event_t * /*e*/) { go_c4(); }},
      {AppIcon::Battleship, "Battleship", [](lv_event_t * /*e*/) { go_battleship(); }},
      {AppIcon::Checkers, "Checkers", [](lv_event_t * /*e*/) { go_checkers(); }},
      {AppIcon::Memory, "Memory", [](lv_event_t * /*e*/) { go_memory(); }},
  };
  for (int i = 0; i < 5; ++i) {
    lv_obj_t * icon = make_app_icon(grid, items[i].icon, items[i].label, items[i].cb);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, i % 3, 1, LV_GRID_ALIGN_START, i / 3, 1);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
  return scr;
}

lv_obj_t * game_ttt_screen() { return game_ttt_build(); }
lv_obj_t * game_c4_screen() { return game_c4_build(); }
lv_obj_t * game_bs_screen() { return game_bs_build(); }
lv_obj_t * game_ck_screen() { return game_ck_build(); }
lv_obj_t * game_mem_screen() { return game_mem_build(); }

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
