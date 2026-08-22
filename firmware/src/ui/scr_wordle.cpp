#include "ui/scr_wordle.h"

#include "app/app.h"
#include "app/active_games.h"
#include "app/presence.h"
#include "app/score_log.h"
#include "games/wordle.h"
#include "protocol/messages.h"
#include "ui/chrome.h"
#include "ui/fonts.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace ui {
namespace {

using games::wordle::kMaxAttempts;
using games::wordle::kWordLen;
using games::wordle::TileState;

constexpr uint32_t kColorCorrect = 0x538d4e;  /* Classic Wordle Green */
constexpr uint32_t kColorPresent = 0xb59f3b;  /* Classic Wordle Yellow */
constexpr uint32_t kColorAbsent  = 0x3a3a3c;  /* Dark Gray */
constexpr uint32_t kColorKeyDef  = 0x4a3a5a;  /* Keyboard default panel */
constexpr uint32_t kColorTileDef = 0x1e1628;  /* Empty tile fill */
constexpr uint32_t kColorBorder  = 0x3d2c52;  /* Empty tile border */

void fill_msg_ids(proto::Msg & m, const char * to_id) {
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", app::desk().id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", app::desk().name);
  if (to_id) std::snprintf(m.to_id, sizeof(m.to_id), "%s", to_id);
}

void copy_str(char * dst, size_t cap, const char * src) {
  std::snprintf(dst, cap, "%s", src ? src : "");
}

lv_color_t tile_bg_color(TileState s) {
  switch (s) {
    case TileState::Correct: return lv_color_hex(kColorCorrect);
    case TileState::Present: return lv_color_hex(kColorPresent);
    case TileState::Absent:  return lv_color_hex(kColorAbsent);
    case TileState::Tbd:
    case TileState::Empty:
    default:
      return lv_color_hex(kColorTileDef);
  }
}

lv_color_t key_bg_color(TileState s) {
  switch (s) {
    case TileState::Correct: return lv_color_hex(kColorCorrect);
    case TileState::Present: return lv_color_hex(kColorPresent);
    case TileState::Absent:  return lv_color_hex(kColorAbsent);
    case TileState::Tbd:
    case TileState::Empty:
    default:
      return lv_color_hex(kColorKeyDef);
  }
}
void on_wordle_forfeit_yes(lv_event_t * /*e*/) {
  app::WordleGame & g2 = app::wordle();
  if (g2.active) {
    score_log::note("Wordle", g2.opp_name, score_log::Outcome::ForfeitSelf);
    proto::Msg m;
    m.type = proto::MsgType::WordleForfeit;
    fill_msg_ids(m, g2.opp_id);
    app::send(m);
  }
  app::end_focused();
  go_game_back();
}
void submit_picked_word() {
  app::WordleGame & g = app::wordle();
  if (!g.active || g.my_word_picked) return;
  if (g.current_len < kWordLen) {
    toast("5 letters required");
    return;
  }
  g.current_input[kWordLen] = '\0';
  if (!games::wordle::is_valid_word(g.current_input)) {
    toast("Not in word list");
    return;
  }
  copy_str(g.word_for_opp, sizeof(g.word_for_opp), g.current_input);
  g.my_word_picked = true;
  g.current_len = 0;
  g.current_input[0] = '\0';

  proto::Msg m;
  m.type = proto::MsgType::WordleWord;
  fill_msg_ids(m, g.opp_id);
  copy_str(m.word, sizeof(m.word), g.word_for_opp);
  app::send(m);
  app::games_mark_dirty();
  go_wordle();
}

void on_pick_key_click(lv_event_t * e) {
  const char * key = (const char *)lv_event_get_user_data(e);
  if (!key || !key[0]) return;
  app::WordleGame & g = app::wordle();
  if (!g.active || g.my_word_picked) return;

  if (std::strcmp(key, "ENTER") == 0) {
    submit_picked_word();
    return;
  } else if (std::strcmp(key, "DEL") == 0 || std::strcmp(key, "BACK") == 0) {
    if (g.current_len > 0) {
      g.current_len--;
      g.current_input[g.current_len] = '\0';
    }
  } else {
    if (g.current_len < kWordLen) {
      char ch = key[0];
      if (ch >= 'a' && ch <= 'z') ch -= ('a' - 'A');
      g.current_input[g.current_len++] = ch;
      g.current_input[g.current_len] = '\0';
    }
  }
  go_wordle();
}

void on_guess_key_click(lv_event_t * e) {
  const char * key = (const char *)lv_event_get_user_data(e);
  if (!key || !key[0]) return;
  app::WordleGame & g = app::wordle();
  if (!g.active || !g.my_word_picked || !g.opp_word_ready || g.over) return;

  if (std::strcmp(key, "ENTER") == 0) {
    if (g.current_len < kWordLen) {
      toast("5 letters required");
      return;
    }
    g.current_input[kWordLen] = '\0';
    if (!games::wordle::is_valid_word(g.current_input)) {
      toast("Not in word list");
      return;
    }
    if (g.attempt_count < kMaxAttempts) {
      std::memcpy(g.attempts[g.attempt_count].word, g.current_input, kWordLen);
      g.attempts[g.attempt_count].word[kWordLen] = '\0';
      games::wordle::evaluate_guess(g.current_input, g.my_target, g.attempts[g.attempt_count].states);
      for (int i = 0; i < kWordLen; ++i) {
        char ch = g.attempts[g.attempt_count].word[i];
        if (ch >= 'A' && ch <= 'Z') {
          int k_idx = ch - 'A';
          TileState s = g.attempts[g.attempt_count].states[i];
          if (s == TileState::Correct) {
            g.key_states[k_idx] = TileState::Correct;
          } else if (s == TileState::Present && g.key_states[k_idx] != TileState::Correct) {
            g.key_states[k_idx] = TileState::Present;
          } else if (s == TileState::Absent && g.key_states[k_idx] == TileState::Empty) {
            g.key_states[k_idx] = TileState::Absent;
          }
        }
      }
      g.attempt_count++;
      char target_u[6] = {};
      for (int i = 0; i < kWordLen; ++i) {
        char c = g.my_target[i];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        target_u[i] = c;
      }
      bool win = std::strncmp(g.current_input, target_u, kWordLen) == 0;
      if (win) {
        g.i_won = true;
        g.over = true;
        g.result_dismissed = false;
        score_log::note("Wordle", g.opp_name, score_log::Outcome::Win);
        proto::Msg m;
        m.type = proto::MsgType::WordleResult;
        fill_msg_ids(m, g.opp_id);
        m.won = true;
        m.attempts = (uint8_t)g.attempt_count;
        app::send(m);
      } else if (g.attempt_count >= kMaxAttempts) {
        g.i_lost = true;
        g.over = true;
        g.result_dismissed = false;
        score_log::note("Wordle", g.opp_name, score_log::Outcome::Lose);
        proto::Msg m;
        m.type = proto::MsgType::WordleResult;
        fill_msg_ids(m, g.opp_id);
        m.won = false;
        m.attempts = (uint8_t)kMaxAttempts;
        app::send(m);
      }
      g.current_len = 0;
      g.current_input[0] = '\0';
      app::games_mark_dirty();
    }
  } else if (std::strcmp(key, "DEL") == 0 || std::strcmp(key, "BACK") == 0) {
    if (g.current_len > 0) {
      g.current_len--;
      g.current_input[g.current_len] = '\0';
      app::games_mark_dirty();
    }
  } else {
    if (g.current_len < kWordLen) {
      char ch = key[0];
      if (ch >= 'a' && ch <= 'z') ch -= ('a' - 'A');
      g.current_input[g.current_len++] = ch;
      g.current_input[g.current_len] = '\0';
      app::games_mark_dirty();
    }
  }
  go_wordle();
}

void make_wordle_keyboard(lv_obj_t * parent, lv_event_cb_t on_key, const TileState * key_states) {
  static const char * r1[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
  static const char * r2[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
  static const char * r3[] = {"ENTER", "Z", "X", "C", "V", "B", "N", "M", "DEL"};

  auto make_row = [&](const char ** keys, int count) {
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 38);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < count; ++i) {
      const char * k = keys[i];
      const bool is_action = std::strcmp(k, "ENTER") == 0 || std::strcmp(k, "DEL") == 0;
      lv_obj_t * btn = lv_button_create(row);
      lv_obj_remove_style_all(btn);
      lv_obj_set_height(btn, 36);
      lv_obj_set_width(btn, is_action ? 58 : 38);
      lv_obj_set_style_radius(btn, 6, 0);

      TileState ks = TileState::Empty;
      if (!is_action && k[0] >= 'A' && k[0] <= 'Z' && key_states) {
        ks = key_states[k[0] - 'A'];
      }
      
      if (std::strcmp(k, "ENTER") == 0 && key_states) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2ecc71), 0); /* Vibrant Green */
        lv_obj_set_style_border_width(btn, 2, 0);
      } else {
        lv_obj_set_style_bg_color(btn, key_bg_color(ks), 0);
      }
      
      lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
      lv_obj_add_event_cb(btn, on_key, LV_EVENT_CLICKED, (void *)k);

      lv_obj_t * lbl = lv_label_create(btn);
      lv_label_set_text(lbl, k);
      
      lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
      
      lv_obj_set_style_text_font(lbl, is_action ? &lv_font_montserrat_12 : &lv_font_montserrat_16, 0);
      lv_obj_center(lbl);
    }
  };
  lv_obj_t * kb = lv_obj_create(parent);
  lv_obj_remove_style_all(kb);
  lv_obj_set_width(kb, lv_pct(100));
  lv_obj_set_height(kb, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(kb, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(kb, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(kb, 4, 0);
  lv_obj_remove_flag(kb, LV_OBJ_FLAG_SCROLLABLE);

  make_row(r1, 10);
  make_row(r2, 9);
  make_row(r3, 9);
}

void wordle_challenge(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  const app::Peer & p = d.peers[idx];

  char why[48];
  if (!app::peer_contact_ok(idx, why, sizeof(why))) {
    toast(why);
    return;
  }
  if (!app::begin_match(app::GameKind::Wordle, p.id)) {
    if (app::find_slot(app::GameKind::Wordle, p.id) >= 0) {
      char buf[72];
      lv_snprintf(buf, sizeof(buf), "Already playing Wordle with %s", p.name);
      toast(buf);
    } else {
      toast("Can't start Wordle");
    }
    return;
  }

  app::WordleGame & g = app::wordle();
  g = {};
  g.active = true;
  g.waiting = true;
  copy_str(g.opp_id, proto::kMaxId, p.id);
  copy_str(g.opp_name, proto::kMaxName, p.name);

  proto::Msg m;
  m.type = proto::MsgType::WordleInvite;
  fill_msg_ids(m, p.id);
  app::send(m);
  go_wordle();
}

}  // namespace

lv_obj_t * game_wordle_screen() {
  const bool has_slot = app::has_focused(app::GameKind::Wordle);
  const bool is_inv = app::invite_active(app::GameKind::Wordle);
  app::WordleGame & g = app::wordle();

  /* —— State A: Peer Picker (no active game) —— */
  if (!has_slot || (!g.active && !is_inv)) {
    lv_obj_t * scr = make_screen();
    make_topbar(scr, "WORDLE", app::desk().name, "Pick an opponent");
    lv_obj_t * body = make_body(scr, true);
    make_tagline(body, "Challenge a peer to 2-Player Wordle");
    const app::Desk & d = app::desk();
    if (d.peer_count == 0) make_tagline(body, "No saved desks - add one in Settings.");
    for (int i = 0; i < d.peer_count; ++i) {
      char sub[24];
      const char * st = app::peer_presence_subtitle(i, sub, sizeof(sub));
      lv_obj_t * btn = make_peer_btn(body, d.peers[i].name, st, wordle_challenge, (void *)(intptr_t)i);
      if (!app::peer_present_idx(i)) lv_obj_set_style_opa(btn, LV_OPA_50, 0);
    }
    lv_obj_t * dock = make_dock(scr);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_game_back(); });
    return scr;
  }

  /* —— State B: Incoming Invite —— */
  if (is_inv) {
    app::Invite & inv = app::invite_ref(app::GameKind::Wordle);
    lv_obj_t * scr = make_screen();
    char sub[48];
    lv_snprintf(sub, sizeof(sub), "vs. %s", inv.from_name);
    make_topbar(scr, "WORDLE", app::desk().name, sub);
    lv_obj_t * body = make_body(scr, false);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * msg = lv_label_create(body);
    char prompt[96];
    lv_snprintf(prompt, sizeof(prompt), "%s challenged you to 2P Wordle!", inv.from_name);
    lv_label_set_text(msg, prompt);
    lv_obj_set_style_text_color(msg, theme::gold(), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t * dock = make_dock(scr);
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Invite & inv2 = app::invite_ref(app::GameKind::Wordle);
      app::WordleGame & g2 = app::wordle();
      g2 = {};
      g2.active = true;
      g2.waiting = false;
      copy_str(g2.opp_id, proto::kMaxId, inv2.from_id);
      copy_str(g2.opp_name, proto::kMaxName, inv2.from_name);
      proto::Msg m;
      m.type = proto::MsgType::WordleAccept;
      fill_msg_ids(m, inv2.from_id);
      app::send(m);
      app::accept_invite(app::GameKind::Wordle);
      go_wordle();
    });
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Invite & inv2 = app::invite_ref(app::GameKind::Wordle);
      proto::Msg m;
      m.type = proto::MsgType::WordleDecline;
      fill_msg_ids(m, inv2.from_id);
      app::send(m);
      app::end_focused();
      go_game_back();
    });
    return scr;
  }

  /* —— State C: Outgoing Challenge Waiting —— */
  if (g.active && g.waiting) {
    lv_obj_t * scr = make_screen();
    char sub[48];
    lv_snprintf(sub, sizeof(sub), "vs. %s", g.opp_name);
    make_topbar(scr, "WORDLE", app::desk().name, sub);
    lv_obj_t * body = make_body(scr, false);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * msg = lv_label_create(body);
    char prompt[96];
    lv_snprintf(prompt, sizeof(prompt), "Waiting for %s to accept...", g.opp_name);
    lv_label_set_text(msg, prompt);
    lv_obj_set_style_text_color(msg, theme::mint(), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);

    lv_obj_t * dock = make_dock(scr);
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::cancel_slot(app::focus_index());
    });
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_game_back(); });
    return scr;
  }

  /* —— State D: Phase 1 — Secret Word Picking for Opponent —— */
  if (g.active && !g.my_word_picked) {
    lv_obj_t * scr = make_screen();
    char sub[48];
    lv_snprintf(sub, sizeof(sub), "Pick word for %s", g.opp_name);
    make_topbar(scr, "WORDLE", app::desk().name, sub);

    lv_obj_t * body = make_body(scr, false);
    lv_obj_set_style_pad_all(body, 8, 0);
    lv_obj_set_style_pad_row(body, 12, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * hint = lv_label_create(body);
    char prompt[96];
    lv_snprintf(prompt, sizeof(prompt), "Choose a 5-letter secret word for %s:", g.opp_name);
    lv_label_set_text(hint, prompt);
    lv_obj_set_style_text_color(hint, theme::gold(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);

    /* 5 letter boxes */
    lv_obj_t * boxes = lv_obj_create(body);
    lv_obj_remove_style_all(boxes);
    lv_obj_set_size(boxes, lv_pct(100), 44);
    lv_obj_set_flex_flow(boxes, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(boxes, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(boxes, 6, 0);
    lv_obj_remove_flag(boxes, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < kWordLen; ++i) {
      lv_obj_t * tile = lv_obj_create(boxes);
      lv_obj_remove_style_all(tile);
      lv_obj_set_size(tile, 40, 42);
      lv_obj_set_style_radius(tile, 6, 0);
      lv_obj_set_style_bg_color(tile, lv_color_hex(kColorTileDef), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(tile, 2, 0);
      lv_obj_set_style_border_color(tile, i < g.current_len ? theme::gold() : lv_color_hex(kColorBorder), 0);

      lv_obj_t * tlbl = lv_label_create(tile);
      char ch_str[2] = {i < g.current_len ? g.current_input[i] : ' ', '\0'};
      lv_label_set_text(tlbl, ch_str);
      lv_obj_set_style_text_color(tlbl, lv_color_hex(0xffffff), 0);
      lv_obj_set_style_text_font(tlbl, &lv_font_montserrat_16, 0);
      lv_obj_center(tlbl);
    }

    make_wordle_keyboard(body, on_pick_key_click, nullptr);

    lv_obj_t * dock = make_dock(scr);
    dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) { show_forfeit_confirm(on_wordle_forfeit_yes); });
    
    dock_btn(dock, "Submit", true, false, [](lv_event_t * /*e*/) { submit_picked_word(); });
    
    return scr;
  }

  /* —— State E: Word Picked, Waiting for Opponent's Word —— */
  if (g.active && g.my_word_picked && !g.opp_word_ready) {
    lv_obj_t * scr = make_screen();
    char sub[48];
    lv_snprintf(sub, sizeof(sub), "vs. %s", g.opp_name);
    make_topbar(scr, "WORDLE", app::desk().name, sub);

    lv_obj_t * body = make_body(scr, false);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(body, 10, 0);

    lv_obj_t * card = lv_obj_create(body);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 300, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, theme::panel(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 8, 0);

    lv_obj_t * t1 = lv_label_create(card);
    lv_label_set_text(t1, "Word Selected!");
    lv_obj_set_style_text_color(t1, theme::mint(), 0);
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_20, 0);

    lv_obj_t * t2 = lv_label_create(card);
    char prompt[96];
    lv_snprintf(prompt, sizeof(prompt), "Waiting for %s to pick your word...", g.opp_name);
    lv_label_set_text(t2, prompt);
    lv_obj_set_style_text_color(t2, theme::ink(), 0);
    lv_obj_set_style_text_font(t2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(t2, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t * t3 = lv_label_create(card);
    char pick_buf[64];
    lv_snprintf(pick_buf, sizeof(pick_buf), "Your word for %s: %s", g.opp_name, g.word_for_opp);
    lv_label_set_text(t3, pick_buf);
    lv_obj_set_style_text_color(t3, theme::muted(), 0);
    lv_obj_set_style_text_font(t3, &lv_font_montserrat_14, 0);

    lv_obj_t * dock = make_dock(scr);
    dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) { show_forfeit_confirm(on_wordle_forfeit_yes); });
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_game_back(); });
    return scr;
  }

  /* —— State F: Phase 2 — Guessing Opponent's Word —— */
  lv_obj_t * scr = make_screen();
  char sub[48];
  lv_snprintf(sub, sizeof(sub), "vs. %s", g.opp_name);
  make_topbar(scr, "WORDLE", app::desk().name, sub);

  lv_obj_t * body = make_body(scr, false);
  lv_obj_set_style_pad_all(body, 2, 0);
  lv_obj_set_style_pad_row(body, 3, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  /* 6×5 Board Grid */
  lv_obj_t * board = lv_obj_create(body);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, 180, 206);
  lv_obj_set_flex_flow(board, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(board, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(board, 2, 0);
  lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  for (int r = 0; r < kMaxAttempts; ++r) {
    lv_obj_t * row = lv_obj_create(board);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), 32);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 2, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    for (int c = 0; c < kWordLen; ++c) {
      lv_obj_t * tile = lv_obj_create(row);
      lv_obj_remove_style_all(tile);
      lv_obj_set_size(tile, 32, 32);
      lv_obj_set_style_radius(tile, 4, 0);

      char letter = ' ';
      TileState tstate = TileState::Empty;

      if (r < g.attempt_count) {
        letter = g.attempts[r].word[c];
        tstate = g.attempts[r].states[c];
      } else if (r == g.attempt_count && c < g.current_len) {
        letter = g.current_input[c];
        tstate = TileState::Tbd;
      }

      lv_obj_set_style_bg_color(tile, tile_bg_color(tstate), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(tile, 1, 0);
      lv_obj_set_style_border_color(
          tile, tstate == TileState::Tbd ? theme::gold() : lv_color_hex(kColorBorder), 0);

      lv_obj_t * tlbl = lv_label_create(tile);
      char ch_str[2] = {letter, '\0'};
      lv_label_set_text(tlbl, ch_str);
      lv_obj_set_style_text_color(tlbl, lv_color_hex(0xffffff), 0);
      lv_obj_set_style_text_font(tlbl, &lv_font_montserrat_14, 0);
      lv_obj_center(tlbl);
    }
  }

  make_wordle_keyboard(body, on_guess_key_click, g.key_states);

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) { show_forfeit_confirm(on_wordle_forfeit_yes); });
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_game_back(); });

  /* Result overlay */
  if (g.over && !g.result_dismissed) {
    lv_obj_t * ov = lv_obj_create(scr);
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(ov, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ov, 180, 0);
    lv_obj_align(ov, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * card = lv_obj_create(ov);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 280, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, theme::panel(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 18, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, g.i_won ? theme::gold() : theme::hot(), 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_center(card);

    lv_obj_t * head = lv_label_create(card);
    lv_label_set_text(head, g.i_won ? "You Win! 🎉" : "Game Over 😢");
    lv_obj_set_style_text_color(head, g.i_won ? theme::gold() : theme::hot(), 0);
    lv_obj_set_style_text_font(head, &lv_font_montserrat_20, 0);

    lv_obj_t * word_lbl = lv_label_create(card);
    char wbuf[64];
    lv_snprintf(wbuf, sizeof(wbuf), "Target Word: %s", g.my_target);
    lv_label_set_text(word_lbl, wbuf);
    lv_obj_set_style_text_color(word_lbl, theme::ink(), 0);
    lv_obj_set_style_text_font(word_lbl, &lv_font_montserrat_16, 0);

    lv_obj_t * sub_lbl = lv_label_create(card);
    char sbuf[96];
    if (g.i_won) {
      lv_snprintf(sbuf, sizeof(sbuf), "Guessed in %d tries!", g.attempt_count);
    } else {
      lv_snprintf(sbuf, sizeof(sbuf), "%s wins this round!", g.opp_name);
    }
    lv_label_set_text(sub_lbl, sbuf);
    lv_obj_set_style_text_color(sub_lbl, theme::muted(), 0);
    lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_14, 0);

    lv_obj_t * btn_done = lv_button_create(card);
    lv_obj_remove_style_all(btn_done);
    lv_obj_set_size(btn_done, 120, 36);
    lv_obj_set_style_radius(btn_done, 10, 0);
    lv_obj_set_style_bg_color(btn_done, theme::gold(), 0);
    lv_obj_set_style_bg_opa(btn_done, LV_OPA_COVER, 0);
    lv_obj_t * d_lbl = lv_label_create(btn_done);
    lv_label_set_text(d_lbl, "Done");
    lv_obj_set_style_text_color(d_lbl, lv_color_hex(0x1a1220), 0);
    lv_obj_center(d_lbl);

    lv_obj_add_event_cb(btn_done, [](lv_event_t * /*e*/) {
      app::WordleGame & g2 = app::wordle();
      g2.result_dismissed = true;
      app::end_focused();
      go_game_back();
    }, LV_EVENT_CLICKED, nullptr);
  }

  return scr;
}

}  // namespace ui
}  // namespace wp
