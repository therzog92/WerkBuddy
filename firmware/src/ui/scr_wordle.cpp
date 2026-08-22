#include "ui/scr_wordle.h"

#include "app/app.h"
#include "app/active_games.h"
#include "app/presence.h"
#include "app/score_log.h"
#include "games/wordle.h"
#include "protocol/messages.h"
#include "ui/chrome.h"
#include "ui/emoji_badge.h"
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
constexpr uint32_t kColorAbsent = 0x3a3a3c;   /* Dark Gray */
constexpr uint32_t kColorKeyDef = 0x4a3a5a;   /* Keyboard default panel */
constexpr uint32_t kColorTileDef = 0x1e1628;  /* Empty tile fill */
constexpr uint32_t kColorBorder = 0x3d2c52;   /* Empty tile border */

lv_obj_t * g_wordle_scr = nullptr;
lv_obj_t * g_pick_tiles[kWordLen] = {};
lv_obj_t * g_pick_lbls[kWordLen] = {};
lv_obj_t * g_guess_tiles[kMaxAttempts][kWordLen] = {};
lv_obj_t * g_guess_lbls[kMaxAttempts][kWordLen] = {};
lv_obj_t * g_key_btns[26] = {};
int g_pending_mode = -1; /* -1 pick mode, 0 classic, 1 race */

bool my_done(const app::WordleGame & g) { return g.i_won || g.i_lost; }
bool opp_done(const app::WordleGame & g) { return g.opp_won || g.opp_lost; }
bool match_done(const app::WordleGame & g) {
  if (g.race) return g.over;
  return my_done(g) && opp_done(g);
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
    case TileState::Absent: return lv_color_hex(kColorAbsent);
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
    case TileState::Absent: return lv_color_hex(kColorAbsent);
    case TileState::Tbd:
    case TileState::Empty:
    default:
      return lv_color_hex(kColorKeyDef);
  }
}

void clear_live_if_scr(lv_obj_t * scr) {
  if (g_wordle_scr != scr) return;
  g_wordle_scr = nullptr;
  std::memset(g_pick_tiles, 0, sizeof(g_pick_tiles));
  std::memset(g_pick_lbls, 0, sizeof(g_pick_lbls));
  std::memset(g_guess_tiles, 0, sizeof(g_guess_tiles));
  std::memset(g_guess_lbls, 0, sizeof(g_guess_lbls));
  std::memset(g_key_btns, 0, sizeof(g_key_btns));
}

void on_wordle_scr_deleted(lv_event_t * e) {
  clear_live_if_scr(static_cast<lv_obj_t *>(lv_event_get_target(e)));
}

void bind_wordle_scr(lv_obj_t * scr) {
  g_wordle_scr = scr;
  lv_obj_add_event_cb(scr, on_wordle_scr_deleted, LV_EVENT_DELETE, nullptr);
}

void paint_tile(lv_obj_t * tile, lv_obj_t * lbl, char letter, TileState s) {
  if (!tile || !lbl) return;
  lv_obj_set_style_bg_color(tile, tile_bg_color(s), 0);
  lv_obj_set_style_border_color(
      tile, s == TileState::Tbd ? theme::gold() : lv_color_hex(kColorBorder), 0);
  char ch[2] = {letter ? letter : ' ', 0};
  lv_label_set_text(lbl, ch);
}

void refresh_pick_tiles() {
  app::WordleGame & g = app::wordle();
  for (int i = 0; i < kWordLen; ++i) {
    const bool filled = i < g.current_len;
    paint_tile(g_pick_tiles[i], g_pick_lbls[i], filled ? g.current_input[i] : ' ',
               filled ? TileState::Tbd : TileState::Empty);
  }
}

void refresh_current_guess_row() {
  app::WordleGame & g = app::wordle();
  const int r = g.attempt_count;
  if (r < 0 || r >= kMaxAttempts) return;
  for (int c = 0; c < kWordLen; ++c) {
    const bool filled = c < g.current_len;
    paint_tile(g_guess_tiles[r][c], g_guess_lbls[r][c], filled ? g.current_input[c] : ' ',
               filled ? TileState::Tbd : TileState::Empty);
  }
}

void paint_guess_row(int r) {
  app::WordleGame & g = app::wordle();
  if (r < 0 || r >= kMaxAttempts) return;
  for (int c = 0; c < kWordLen; ++c) {
    paint_tile(g_guess_tiles[r][c], g_guess_lbls[r][c], g.attempts[r].word[c],
               g.attempts[r].states[c]);
  }
}

void paint_keyboard_keys() {
  app::WordleGame & g = app::wordle();
  for (int i = 0; i < 26; ++i) {
    if (!g_key_btns[i]) continue;
    lv_obj_set_style_bg_color(g_key_btns[i], key_bg_color(g.key_states[i]), 0);
  }
}

bool live_pick() { return g_wordle_scr && g_pick_tiles[0]; }
bool live_guess() { return g_wordle_scr && g_guess_tiles[0][0]; }

void show_wordle_result(lv_obj_t * scr) {
  app::WordleGame & g = app::wordle();
  if (!scr || g.result_dismissed) return;
  if (!g.over && !match_done(g)) return;

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
  const bool you_ok = g.i_won;
  lv_obj_set_style_border_color(card, you_ok ? theme::gold() : theme::hot(), 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, 10, 0);
  lv_obj_center(card);

  make_emoji_image(card, you_ok ? "🎉" : "😢", 48);

  lv_obj_t * head = lv_label_create(card);
  const char * head_txt = "Game Over";
  if (g.race) {
    head_txt = you_ok ? "You Win!" : "Game Over";
  } else if (match_done(g)) {
    if (you_ok && g.opp_won) head_txt = "Both got it!";
    else if (you_ok) head_txt = "You Win!";
    else if (g.opp_lost) head_txt = "Both missed";
    else head_txt = "Game Over";
  } else {
    head_txt = you_ok ? "You got it!" : "Out of tries";
  }
  lv_label_set_text(head, head_txt);
  lv_obj_set_style_text_color(head, you_ok ? theme::gold() : theme::hot(), 0);
  lv_obj_set_style_text_font(head, &lv_font_montserrat_20, 0);

  lv_obj_t * word_lbl = lv_label_create(card);
  char wbuf[64];
  lv_snprintf(wbuf, sizeof(wbuf), "Target Word: %s", g.my_target);
  lv_label_set_text(word_lbl, wbuf);
  lv_obj_set_style_text_color(word_lbl, theme::ink(), 0);
  lv_obj_set_style_text_font(word_lbl, &lv_font_montserrat_16, 0);

  lv_obj_t * sub_lbl = lv_label_create(card);
  char sbuf[96];
  if (g.race) {
    if (you_ok) lv_snprintf(sbuf, sizeof(sbuf), "Guessed in %d tries!", g.attempt_count);
    else if (g.opp_won) lv_snprintf(sbuf, sizeof(sbuf), "%s wins this round!", g.opp_name);
    else lv_snprintf(sbuf, sizeof(sbuf), "Out of tries");
  } else if (match_done(g)) {
    char me[24], them[32];
    if (you_ok) lv_snprintf(me, sizeof(me), "You: %d tries", g.attempt_count);
    else lv_snprintf(me, sizeof(me), "You: missed");
    if (g.opp_won && g.opp_attempts)
      lv_snprintf(them, sizeof(them), "%s: %u tries", g.opp_name, (unsigned)g.opp_attempts);
    else if (g.opp_won)
      lv_snprintf(them, sizeof(them), "%s: got it", g.opp_name);
    else
      lv_snprintf(them, sizeof(them), "%s: missed", g.opp_name);
    lv_snprintf(sbuf, sizeof(sbuf), "%s  ·  %s", me, them);
  } else {
    lv_snprintf(sbuf, sizeof(sbuf), "Waiting for %s...", g.opp_name);
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

  lv_obj_add_event_cb(
      btn_done,
      [](lv_event_t * /*e*/) {
        app::WordleGame & g2 = app::wordle();
        g2.result_dismissed = true;
        if (match_done(g2)) {
          app::end_focused();
          go_game_back();
        } else {
          go_wordle();
        }
      },
      LV_EVENT_CLICKED, nullptr);
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

void apply_letter(app::WordleGame & g, char ch) {
  if (g.current_len >= kWordLen) return;
  if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
  g.current_input[g.current_len++] = ch;
  g.current_input[g.current_len] = '\0';
}

void apply_del(app::WordleGame & g) {
  if (g.current_len <= 0) return;
  g.current_len--;
  g.current_input[g.current_len] = '\0';
}

void on_pick_key(lv_event_t * e) {
  const char * key = (const char *)lv_event_get_user_data(e);
  if (!key || !key[0]) return;
  app::WordleGame & g = app::wordle();
  if (!g.active || g.my_word_picked) return;

  if (std::strcmp(key, "ENTER") == 0) {
    submit_picked_word();
    return;
  }
  if (std::strcmp(key, "DEL") == 0 || std::strcmp(key, "BACK") == 0) {
    apply_del(g);
  } else {
    apply_letter(g, key[0]);
  }
  if (live_pick())
    refresh_pick_tiles();
  else
    go_wordle();
}

void submit_guess() {
  app::WordleGame & g = app::wordle();
  if (g.current_len < kWordLen) {
    toast("5 letters required");
    return;
  }
  g.current_input[kWordLen] = '\0';
  if (!games::wordle::is_valid_word(g.current_input)) {
    toast("Not in word list");
    return;
  }
  if (g.attempt_count >= kMaxAttempts) return;

  const int row = g.attempt_count;
  std::memcpy(g.attempts[row].word, g.current_input, kWordLen);
  g.attempts[row].word[kWordLen] = '\0';
  games::wordle::evaluate_guess(g.current_input, g.my_target, g.attempts[row].states);
  for (int i = 0; i < kWordLen; ++i) {
    char ch = g.attempts[row].word[i];
    if (ch >= 'A' && ch <= 'Z') {
      int k_idx = ch - 'A';
      TileState s = g.attempts[row].states[i];
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
  const bool win = std::strncmp(g.current_input, target_u, kWordLen) == 0;
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

  if (live_guess()) {
    paint_guess_row(row);
    paint_keyboard_keys();
    if (g.over) show_wordle_result(g_wordle_scr);
  } else {
    go_wordle();
  }
}

void on_guess_key(lv_event_t * e) {
  const char * key = (const char *)lv_event_get_user_data(e);
  if (!key || !key[0]) return;
  app::WordleGame & g = app::wordle();
  if (!g.active || !g.my_word_picked || !g.opp_word_ready || g.over) return;

  if (std::strcmp(key, "ENTER") == 0) {
    submit_guess();
    return;
  }
  if (std::strcmp(key, "DEL") == 0 || std::strcmp(key, "BACK") == 0) {
    apply_del(g);
  } else {
    apply_letter(g, key[0]);
  }
  if (live_guess())
    refresh_current_guess_row();
  else
    go_wordle();
}

void make_wordle_keyboard(lv_obj_t * parent, lv_event_cb_t on_key, const TileState * key_states) {
  static const char * r1[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
  static const char * r2[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
  static const char * r3[] = {"ENTER", "Z", "X", "C", "V", "B", "N", "M", "DEL"};
  std::memset(g_key_btns, 0, sizeof(g_key_btns));

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
        g_key_btns[k[0] - 'A'] = btn;
      }

      if (std::strcmp(k, "ENTER") == 0 && key_states) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2ecc71), 0);
        lv_obj_set_style_border_width(btn, 2, 0);
      } else {
        lv_obj_set_style_bg_color(btn, key_bg_color(ks), 0);
      }

      lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
      /* PRESSED so the letter lands on finger-down, not wait-for-release. */
      lv_obj_add_event_cb(btn, on_key, LV_EVENT_PRESSED, (void *)k);

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
  g.race = g_pending_mode != 0;
  copy_str(g.opp_id, proto::kMaxId, p.id);
  copy_str(g.opp_name, proto::kMaxName, p.name);

  proto::Msg m;
  m.type = proto::MsgType::WordleInvite;
  fill_msg_ids(m, p.id);
  m.wordle_mode = g.race ? 1 : 0;
  app::send(m);
  go_wordle();
}

lv_obj_t * make_letter_tile(lv_obj_t * row, int size, lv_obj_t ** out_lbl) {
  lv_obj_t * tile = lv_obj_create(row);
  lv_obj_remove_style_all(tile);
  lv_obj_set_size(tile, size, size);
  lv_obj_set_style_radius(tile, 4, 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tile, 1, 0);
  lv_obj_t * lbl = lv_label_create(tile);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(lbl, size >= 40 ? &lv_font_montserrat_16 : &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);
  if (out_lbl) *out_lbl = lbl;
  return tile;
}

}  // namespace

void wordle_reset_setup() { g_pending_mode = -1; }

lv_obj_t * game_wordle_screen() {
  const bool has_slot = app::has_focused(app::GameKind::Wordle);
  const bool is_inv = app::invite_active(app::GameKind::Wordle);
  app::WordleGame & g = app::wordle();

  /* —— State A: Mode picker, then peer list —— */
  if (!has_slot || (!g.active && !is_inv)) {
    lv_obj_t * scr = make_screen();
    lv_obj_t * body = make_body(scr, true);
    lv_obj_t * dock = make_dock(scr);
    if (g_pending_mode < 0) {
      make_topbar(scr, "WORDLE", app::desk().name, "Pick a mode");
      make_tagline(body, "How do you want to play?");
      make_peer_btn(body, "Classic", "Two games at once - each guesses the other's word",
                    [](lv_event_t * /*e*/) {
                      g_pending_mode = 0;
                      go_wordle();
                    }, nullptr);
      make_peer_btn(body, "Race", "First to guess the secret wins the match",
                    [](lv_event_t * /*e*/) {
                      g_pending_mode = 1;
                      go_wordle();
                    }, nullptr);
      dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_game_back(); });
    } else {
      make_topbar(scr, "WORDLE", app::desk().name, g_pending_mode ? "Race" : "Classic");
      make_tagline(body, g_pending_mode ? "Challenge a peer to a Wordle race"
                                       : "Challenge a peer to Classic Wordle");
      const app::Desk & d = app::desk();
      if (d.peer_count == 0) make_tagline(body, "No saved desks - add one in Settings.");
      for (int i = 0; i < d.peer_count; ++i) {
        char sub[24];
        const char * st = app::peer_presence_subtitle(i, sub, sizeof(sub));
        lv_obj_t * btn = make_peer_btn(body, d.peers[i].name, st, wordle_challenge, (void *)(intptr_t)i);
        if (!app::peer_present_idx(i)) lv_obj_set_style_opa(btn, LV_OPA_50, 0);
      }
      dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) {
        g_pending_mode = -1;
        go_wordle();
      });
    }
    return scr;
  }

  /* —— State B: Incoming Invite —— */
  if (is_inv) {
    app::Invite & inv = app::invite_ref(app::GameKind::Wordle);
    lv_obj_t * scr = make_screen();
    make_topbar(scr, "WORDLE", app::desk().name);
    lv_obj_t * body = make_body(scr, true);
    make_wait_block(body, "GAME INVITE", inv.from_name,
                    inv.wordle_mode ? "wants a Wordle race" : "wants to play Wordle");
    lv_obj_t * dock = make_dock(scr);
    dock_btn(dock, "Decline", false, true, [](lv_event_t * /*e*/) {
      app::Invite & inv2 = app::invite_ref(app::GameKind::Wordle);
      proto::Msg m;
      m.type = proto::MsgType::WordleDecline;
      fill_msg_ids(m, inv2.from_id);
      app::send(m);
      app::end_focused();
      go_game_back();
    });
    dock_btn(dock, "Accept", true, false, [](lv_event_t * /*e*/) {
      app::Invite & inv2 = app::invite_ref(app::GameKind::Wordle);
      app::WordleGame & g2 = app::wordle();
      g2 = {};
      g2.active = true;
      g2.waiting = false;
      g2.race = inv2.wordle_mode != 0;
      copy_str(g2.opp_id, proto::kMaxId, inv2.from_id);
      copy_str(g2.opp_name, proto::kMaxName, inv2.from_name);
      proto::Msg m;
      m.type = proto::MsgType::WordleAccept;
      fill_msg_ids(m, inv2.from_id);
      app::send(m);
      app::accept_invite(app::GameKind::Wordle);
      go_wordle();
    });
    return scr;
  }

  /* —— State C: Outgoing Challenge Waiting —— */
  if (g.active && g.waiting) {
    lv_obj_t * scr = make_screen();
    make_topbar(scr, "WORDLE", app::desk().name, g.race ? "Race" : "Classic");
    lv_obj_t * body = make_body(scr, true);
    make_wait_block(body, "CHALLENGE SENT", g.opp_name, "Waiting for them to accept...");
    lv_obj_t * dock = make_dock(scr);
    dock_btn(dock, "Cancel", false, true, [](lv_event_t * /*e*/) {
      app::cancel_slot(app::focus_index());
    });
    dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
    return scr;
  }

  /* —— State D: Phase 1 — Secret Word Picking for Opponent —— */
  if (g.active && !g.my_word_picked) {
    lv_obj_t * scr = make_screen();
    bind_wordle_scr(scr);
    char sub[48];
    lv_snprintf(sub, sizeof(sub), "%s · pick for %s", g.race ? "Race" : "Classic", g.opp_name);
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

    lv_obj_t * boxes = lv_obj_create(body);
    lv_obj_remove_style_all(boxes);
    lv_obj_set_size(boxes, lv_pct(100), 44);
    lv_obj_set_flex_flow(boxes, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(boxes, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(boxes, 6, 0);
    lv_obj_remove_flag(boxes, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < kWordLen; ++i) {
      g_pick_tiles[i] = make_letter_tile(boxes, 42, &g_pick_lbls[i]);
      lv_obj_set_style_border_width(g_pick_tiles[i], 2, 0);
    }
    refresh_pick_tiles();

    make_wordle_keyboard(body, on_pick_key, nullptr);

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
  bind_wordle_scr(scr);
  char sub[48];
  lv_snprintf(sub, sizeof(sub), "%s vs %s", g.race ? "Race" : "Classic", g.opp_name);
  make_topbar(scr, "WORDLE", app::desk().name, sub);

  lv_obj_t * body = make_body(scr, false);
  lv_obj_set_style_pad_all(body, 2, 0);
  lv_obj_set_style_pad_row(body, 3, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

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
      g_guess_tiles[r][c] = make_letter_tile(row, 32, &g_guess_lbls[r][c]);
      char letter = ' ';
      TileState tstate = TileState::Empty;
      if (r < g.attempt_count) {
        letter = g.attempts[r].word[c];
        tstate = g.attempts[r].states[c];
      } else if (r == g.attempt_count && c < g.current_len) {
        letter = g.current_input[c];
        tstate = TileState::Tbd;
      }
      paint_tile(g_guess_tiles[r][c], g_guess_lbls[r][c], letter, tstate);
    }
  }

  make_wordle_keyboard(body, on_guess_key, g.key_states);

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Forfeit", false, true, [](lv_event_t * /*e*/) { show_forfeit_confirm(on_wordle_forfeit_yes); });
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_game_back(); });

  if (g.over && g.result_dismissed && !match_done(g)) {
    lv_obj_t * wait = lv_label_create(body);
    char wbuf[64];
    lv_snprintf(wbuf, sizeof(wbuf), "Waiting for %s to finish...", g.opp_name);
    lv_label_set_text(wait, wbuf);
    lv_obj_set_style_text_color(wait, theme::mint(), 0);
    lv_obj_set_style_text_font(wait, &lv_font_montserrat_14, 0);
  }

  if ((g.over || match_done(g)) && !g.result_dismissed) show_wordle_result(scr);
  return scr;
}

}  // namespace ui
}  // namespace wp
