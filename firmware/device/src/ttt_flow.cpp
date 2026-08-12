#include "ttt_flow.h"

#include "desk.h"
#include "espnow_link.h"
#include "protocol_pack.h"
#include "shell.h"
#include "ui_chrome.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstdio>
#include <cstring>

namespace wp {
namespace shell {
namespace {

void ttt_reset_board() {
  for (int i = 0; i < 9; ++i) desk().ttt_board[i] = 0;
  desk().ttt_turn = 'X';
  desk().ttt_over = false;
  desk().ttt_result_dismissed = false;
}

char ttt_winner() {
  static const int lines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                                  {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
  const char * b = desk().ttt_board;
  for (auto & ln : lines) {
    if (b[ln[0]] && b[ln[0]] == b[ln[1]] && b[ln[0]] == b[ln[2]]) return b[ln[0]];
  }
  return 0;
}

bool ttt_full() {
  for (int i = 0; i < 9; ++i)
    if (!desk().ttt_board[i]) return false;
  return true;
}

void end_ttt_to_hub(const char * toast) {
  desk().ttt_active = false;
  desk().ttt_waiting = false;
  desk().ttt_incoming = false;
  desk().ttt_over = false;
  desk().ttt_result_dismissed = false;
  go_hub();
  if (toast) show_toast(toast);
}

static void on_challenge(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= desk().peer_count) return;
  const Peer & p = desk().peers[idx];
  desk().ttt_waiting = true;
  desk().ttt_incoming = false;
  desk().ttt_active = true;
  desk().ttt_over = false;
  desk().ttt_result_dismissed = false;
  std::snprintf(desk().ttt_peer_id, sizeof(desk().ttt_peer_id), "%s", p.id);
  std::snprintf(desk().ttt_peer_name, sizeof(desk().ttt_peer_name), "%s", p.name);
  desk().ttt_mark = 'X';
  ttt_reset_board();
  net::link_send_ttt(p.id, (int)pack::Type::TttInvite);
  go_ttt();
}

static void on_cancel_invite(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (desk().ttt_waiting) {
    net::link_send_ttt(desk().ttt_peer_id, (int)pack::Type::TttDecline);
  }
  end_ttt_to_hub("Invite cancelled");
}

static void on_accept(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  desk().ttt_incoming = false;
  desk().ttt_waiting = false;
  desk().ttt_active = true;
  desk().ttt_mark = 'O';
  ttt_reset_board();
  net::link_send_ttt(desk().ttt_peer_id, (int)pack::Type::TttAccept);
  go_ttt();
}

static void on_decline(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  net::link_send_ttt(desk().ttt_peer_id, (int)pack::Type::TttDecline);
  end_ttt_to_hub("Declined");
}

static void on_cell(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (!desk().ttt_active || desk().ttt_waiting || desk().ttt_over) return;
  if (desk().ttt_turn != desk().ttt_mark) return;
  const int cell = (int)(intptr_t)lv_event_get_user_data(e);
  if (cell < 0 || cell > 8 || desk().ttt_board[cell]) return;
  desk().ttt_board[cell] = desk().ttt_mark;
  net::link_send_ttt_move(desk().ttt_peer_id, (int8_t)cell, desk().ttt_mark);
  if (ttt_winner() || ttt_full()) {
    desk().ttt_over = true;
    desk().ttt_result_dismissed = false;
  } else {
    desk().ttt_turn = (desk().ttt_mark == 'X') ? 'O' : 'X';
  }
  Serial.printf("ttt move cell=%d mark=%c\n", cell, desk().ttt_mark);
  go_ttt();
}

static void on_forfeit_confirmed(lv_event_t * /*e*/) {
  net::link_send_ttt(desk().ttt_peer_id, (int)pack::Type::TttForfeit);
  end_ttt_to_hub("You forfeited");
}

static void on_forfeit(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  dui::show_forfeit_confirm(on_forfeit_confirmed);
}

static void on_home(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (desk().ttt_active && !desk().ttt_over && !desk().ttt_waiting && !desk().ttt_incoming) {
    /* Mid-game Home = forfeit confirm (same as sim intent to not abandon silently). */
    dui::show_forfeit_confirm(on_forfeit_confirmed);
    return;
  }
  if (desk().ttt_waiting) {
    /* Leave challenge pending — reopen Games to resume wait (thin Active Games). */
    go_hub();
    return;
  }
  end_ttt_to_hub(nullptr);
}

static void on_play_again(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  char id[16];
  char name[13];
  std::snprintf(id, sizeof(id), "%s", desk().ttt_peer_id);
  std::snprintf(name, sizeof(name), "%s", desk().ttt_peer_name);
  desk().ttt_waiting = true;
  desk().ttt_incoming = false;
  desk().ttt_active = true;
  desk().ttt_mark = 'X';
  ttt_reset_board();
  std::snprintf(desk().ttt_peer_id, sizeof(desk().ttt_peer_id), "%s", id);
  std::snprintf(desk().ttt_peer_name, sizeof(desk().ttt_peer_name), "%s", name);
  net::link_send_ttt(id, (int)pack::Type::TttInvite);
  go_ttt();
}

static void on_dismiss_result(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  desk().ttt_result_dismissed = true;
  go_ttt();
}

static void on_back_hub(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  go_hub();
}

void fill_ttt_play(lv_obj_t * body) {
  const bool my_turn = !desk().ttt_over && !desk().ttt_waiting && desk().ttt_turn == desk().ttt_mark;
  const char win = ttt_winner();

  if (desk().ttt_over && !desk().ttt_result_dismissed) {
    dui::make_status(body, "Game over");
  } else if (my_turn) {
    dui::make_status(body, "Your turn");
  } else if (desk().ttt_over) {
    dui::make_status(body, "Play again?");
  } else {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Waiting on %s...", desk().ttt_peer_name);
    dui::make_status(body, buf);
  }

  lv_obj_t * row = lv_obj_create(body);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, 28);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * you = lv_label_create(row);
  lv_label_set_text(you, "you are");
  lv_obj_set_style_text_color(you, dui::muted(), 0);
  lv_obj_set_style_text_font(you, &lv_font_montserrat_12, 0);
  lv_obj_t * badge = lv_obj_create(row);
  lv_obj_remove_style_all(badge);
  lv_obj_set_size(badge, 28, 28);
  lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(badge, dui::panel(), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(badge, 1, 0);
  lv_obj_set_style_border_color(badge, dui::border(), 0);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  dui::ttt_draw_mark(badge, desk().ttt_mark, 18);

  lv_obj_t * board = lv_obj_create(body);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 280, 280);
  lv_obj_set_style_radius(board, 20, 0);
  lv_obj_set_style_bg_color(board, dui::panel(), 0);
  lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(board, 1, 0);
  lv_obj_set_style_border_color(board, dui::border(), 0);
  lv_obj_set_layout(board, LV_LAYOUT_GRID);
  static lv_coord_t c[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t r[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(board, c, r);
  lv_obj_set_style_pad_all(board, 8, 0);
  lv_obj_set_style_pad_row(board, 8, 0);
  lv_obj_set_style_pad_column(board, 8, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  const lv_color_t cell_bg = lv_color_mix(dui::bg0(), dui::panel(), 140);
  for (int i = 0; i < 9; ++i) {
    lv_obj_t * cell = lv_obj_create(board);
    lv_obj_remove_style_all(cell);
    lv_obj_set_style_radius(cell, 16, 0);
    lv_obj_set_style_bg_color(cell, cell_bg, 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, dui::border(), 0);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    if (desk().ttt_board[i]) {
      dui::ttt_draw_mark(cell, desk().ttt_board[i], 52);
    } else if (my_turn) {
      lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(cell, on_cell, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
  }

  if (desk().ttt_over && !desk().ttt_result_dismissed) {
    const int outcome = !win ? -1 : (win == desk().ttt_mark ? 1 : 0);
    dui::attach_result_overlay(body, outcome, on_dismiss_result);
  }
}

}  // namespace

void go_ttt() {
  lv_obj_t * scr = dui::make_screen();
  char sub[40] = {};
  if (desk().ttt_active && desk().ttt_peer_name[0] && !desk().ttt_incoming) {
    std::snprintf(sub, sizeof(sub), "vs %s", desk().ttt_peer_name);
  } else if (desk().ttt_incoming) {
    std::snprintf(sub, sizeof(sub), "from %s", desk().ttt_peer_name);
  }
  dui::make_topbar(scr, "TIC TAC TOE", desk().name, sub[0] ? sub : nullptr);
  lv_obj_t * body = dui::make_body(scr, true);
  lv_obj_t * dock = dui::make_dock(scr);

  if (desk().ttt_incoming) {
    dui::make_wait_block(body, "GAME INVITE", desk().ttt_peer_name, "wants to play Tic Tac Toe");
    dui::dock_btn(dock, "Decline", false, true, on_decline);
    dui::dock_btn(dock, "Accept", true, false, on_accept);
  } else if (desk().ttt_active && desk().ttt_waiting) {
    dui::make_wait_block(body, "CHALLENGE SENT", desk().ttt_peer_name, "Waiting for them to accept...");
    dui::dock_btn(dock, "Cancel", false, true, on_cancel_invite);
    dui::dock_btn(dock, "Home", false, false, on_home);
  } else if (desk().ttt_active) {
    fill_ttt_play(body);
    if (desk().ttt_over) {
      dui::dock_btn(dock, "Play again", true, false, on_play_again);
      dui::dock_btn(dock, "Home", false, false, [](lv_event_t * e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        end_ttt_to_hub(nullptr);
      });
    } else {
      dui::dock_btn(dock, "Forfeit", false, true, on_forfeit);
      dui::dock_btn(dock, "Home", false, false, on_home);
    }
  } else {
    dui::make_tagline(body, "Challenge a peer");
    if (desk().peer_count == 0) {
      dui::make_tagline(body, "No saved desks - add one in Settings.");
    }
    for (int i = 0; i < desk().peer_count; ++i) {
      dui::make_peer_btn(body, desk().peers[i].name, on_challenge, (void *)(intptr_t)i);
    }
    dui::dock_btn(dock, "Home", false, false, on_back_hub);
  }

  load_screen(scr);
}

void go_ttt_peer_pick() { go_ttt(); }

bool ttt_busy() {
  return desk().ttt_active || desk().ttt_waiting || desk().ttt_incoming;
}

void ttt_on_msg(const net::RxMsg & m) {
  switch (m.kind) {
    case net::RxMsg::Kind::TttInvite:
      desk().ttt_incoming = true;
      desk().ttt_waiting = false;
      desk().ttt_active = false;
      desk().ttt_over = false;
      std::snprintf(desk().ttt_peer_id, sizeof(desk().ttt_peer_id), "%s", m.from_id);
      std::snprintf(desk().ttt_peer_name, sizeof(desk().ttt_peer_name), "%s", m.from_name);
      go_ttt();
      show_toast("TTT challenge");
      break;
    case net::RxMsg::Kind::TttAccept:
      if (desk().ttt_waiting && std::strcmp(desk().ttt_peer_id, m.from_id) == 0) {
        desk().ttt_waiting = false;
        desk().ttt_active = true;
        desk().ttt_mark = 'X';
        ttt_reset_board();
        go_ttt();
        show_toast("Challenge accepted");
      }
      break;
    case net::RxMsg::Kind::TttDecline:
      if (desk().ttt_waiting && std::strcmp(desk().ttt_peer_id, m.from_id) == 0) {
        end_ttt_to_hub("Invite declined");
      } else if (desk().ttt_incoming && std::strcmp(desk().ttt_peer_id, m.from_id) == 0) {
        end_ttt_to_hub("Invite cancelled");
      }
      break;
    case net::RxMsg::Kind::TttMove:
      if (!desk().ttt_active || desk().ttt_over) break;
      if (std::strcmp(desk().ttt_peer_id, m.from_id) != 0) break;
      if (m.cell < 0 || m.cell > 8 || desk().ttt_board[m.cell]) break;
      desk().ttt_board[m.cell] = m.mark ? m.mark : ((desk().ttt_mark == 'X') ? 'O' : 'X');
      if (ttt_winner() || ttt_full()) {
        desk().ttt_over = true;
        desk().ttt_result_dismissed = false;
      } else {
        desk().ttt_turn = desk().ttt_mark;
      }
      go_ttt();
      break;
    case net::RxMsg::Kind::TttForfeit:
      if (std::strcmp(desk().ttt_peer_id, m.from_id) == 0) {
        char line[48];
        std::snprintf(line, sizeof(line), "%s forfeited", m.from_name);
        end_ttt_to_hub(line);
      }
      break;
    default:
      break;
  }
}

}  // namespace shell
}  // namespace wp
