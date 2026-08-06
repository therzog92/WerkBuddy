#include "app/app.h"

#include "app/active_games.h"
#include "app/background.h"
#include "app/checklist.h"
#include "app/page_log.h"
#include "app/score_log.h"
#include "app/storage.h"
#include "games/sttt.h"
#include "games/ttt.h"
#include "net/link.h"
#include "ui/chrome.h"
#include "ui/nav.h"
#include "ui/scr_doodle.h"

#include "lvgl/lvgl.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace wp {
namespace app {
namespace {

Desk g_desk;

const char * kDefaultEmojis[kEmojiSlots] = {"💅", "👑", "📢", "👀", "✨", "☕", "🆘"};
const char * kDefaultCanned[kCannedCount] = {"You busy?", "Come here!", "Downstairs?",
                                             "OMG WTF AHHHH!!!"};

void copy_str(char * dst, size_t cap, const char * src) {
  std::snprintf(dst, cap, "%s", src ? src : "");
}

void apply_runtime_defaults(Desk & d) {
  for (int i = 0; i < kEmojiSlots; ++i) copy_str(d.emojis[i], proto::kMaxEmoji, kDefaultEmojis[i]);
  for (int i = 0; i < kCannedCount; ++i)
    copy_str(d.canned[i], proto::kMaxMessage, kDefaultCanned[i]);
  copy_str(d.peers[0].id, proto::kMaxId, "mac-will");
  copy_str(d.peers[0].name, proto::kMaxName, "Will");
  copy_str(d.peers[1].id, proto::kMaxId, "mac-alex");
  copy_str(d.peers[1].name, proto::kMaxName, "Alex");
  d.peer_count = 2;
}

bool same(const char * a, const char * b) { return std::strcmp(a, b) == 0; }

/* —— schedule helper —— */
struct Scheduled {
  void (*fn)(void *);
  void * user_data;
};

void scheduled_cb(lv_timer_t * t) {
  auto * s = static_cast<Scheduled *>(lv_timer_get_user_data(t));
  s->fn(s->user_data);
  delete s;
  /* repeat_count=1 → LVGL deletes the timer after this callback */
}

/* —— memory flip resolution (web resolveMemFlip, 700ms) —— */
struct MemResolve {
  int8_t a, b;
  bool i_played;
  char opp_id[proto::kMaxId];
};

void mem_resolve_cb(void * ud) {
  auto * r = static_cast<MemResolve *>(ud);
  const int idx = find_slot(GameKind::Mem, r->opp_id);
  GameSlot * slot = slot_at(idx);
  if (!slot || !slot_is_live(*slot) || slot->invite_pending) {
    delete r;
    return;
  }
  MemGame & g = slot->g.mem;
  if (g.active) {
    const bool match = g.deck[r->a] == g.deck[r->b];
    if (match) {
      g.matched[r->a] = true;
      g.matched[r->b] = true;
      if (r->i_played) g.my_score++;
      else g.opp_score++;
      g.my_turn = r->i_played;
    } else {
      g.my_turn = !r->i_played;
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
    if (g.my_turn && !g.over) {
      note_turn_start(idx);
      if (is_viewing(GameKind::Mem, r->opp_id))
        refresh_viewing(GameKind::Mem, r->opp_id);
      else
        notify_your_turn(GameKind::Mem, g.opp_name, r->opp_id);
    } else {
      refresh_viewing(GameKind::Mem, r->opp_id);
    }
  }
  delete r;
}

void schedule_mem_resolve(int8_t a, int8_t b, bool i_played, const char * opp_id) {
  auto * r = new MemResolve{a, b, i_played, {}};
  copy_str(r->opp_id, sizeof(r->opp_id), opp_id);
  schedule(700, mem_resolve_cb, r);
}

/* Web maybeStartBsBattle */
void maybe_start_bs_battle(BsGame & g) {
  if (g.me_ready && g.opp_ready) {
    g.setup = false;
    g.waiting = false;
    g.my_turn = g.i_am_first;
    copy_str(g.last_msg, sizeof(g.last_msg),
             g.my_turn ? "Your turn - tap to fire!" : "Enemy turn...");
    g.mode = g.my_turn ? 0 : 1;
  } else if (g.me_ready) {
    copy_str(g.last_msg, sizeof(g.last_msg), "Waiting for opponent fleet...");
  }
}

void go_game(GameKind kind) {
  switch (kind) {
    case GameKind::Ttt: ui::go_ttt(); break;
    case GameKind::Sttt: ui::go_sttt(); break;
    case GameKind::C4: ui::go_c4(); break;
    case GameKind::Bs: ui::go_battleship(); break;
    case GameKind::Ck: ui::go_checkers(); break;
    case GameKind::Mem: ui::go_memory(); break;
    case GameKind::Rv: ui::go_reversi(); break;
    case GameKind::Db: ui::go_dots(); break;
    default: break;
  }
}

GameSlot * receive_invite(GameKind kind, const proto::Msg & m, int & idx) {
  if (!can_start(kind, m.from_id)) return nullptr;
  idx = alloc_slot(kind);
  GameSlot * slot = slot_at(idx);
  if (!slot) return nullptr;
  slot->invite_pending = true;
  slot->invite.active = true;
  copy_str(slot->invite.from_id, sizeof(slot->invite.from_id), m.from_id);
  copy_str(slot->invite.from_name, sizeof(slot->invite.from_name), m.from_name);
  slot->invite.color =
      (m.color >= 0 && m.color < games::c4::kColorCount) ? m.color : 0;
  slot->invite.seed = m.seed;
  set_focus(idx);
  go_game(kind);
  return slot;
}

void finish_remote_move(int idx, GameKind kind, const char * peer_id, const char * peer_name,
                        bool local_turn) {
  if (local_turn) {
    note_turn_start(idx);
    if (is_viewing(kind, peer_id))
      refresh_viewing(kind, peer_id);
    else
      notify_your_turn(kind, peer_name, peer_id);
  } else {
    refresh_viewing(kind, peer_id);
  }
}

void remove_remote_game(GameKind kind, const proto::Msg & m, const char * score_name,
                        const char * toast_name) {
  const int idx = find_slot(kind, m.from_id);
  GameSlot * slot = slot_at(idx);
  if (!slot) return;
  if (!slot->invite_pending && slot_is_live(*slot)) {
    bool waiting = false;
    switch (kind) {
      case GameKind::Ttt: waiting = slot->g.ttt.waiting; break;
      case GameKind::Sttt: waiting = slot->g.sttt.waiting; break;
      case GameKind::C4: waiting = slot->g.c4.waiting; break;
      case GameKind::Bs: waiting = slot->g.bs.waiting; break;
      case GameKind::Ck: waiting = slot->g.ck.waiting; break;
      case GameKind::Mem: waiting = slot->g.mem.waiting; break;
      case GameKind::Rv: waiting = slot->g.rv.waiting; break;
      case GameKind::Db: waiting = slot->g.db.waiting; break;
      default: break;
    }
    if (!waiting) score_log::note(score_name, m.from_name, score_log::Outcome::ForfeitOpp);
  }
  const bool was_focus = focus_index() == idx;
  const bool was_viewing = is_viewing(kind, m.from_id);
  free_slot(idx);
  char toast[proto::kMaxName + 40];
  std::snprintf(toast, sizeof(toast), "%s left %s", m.from_name, toast_name);
  ui::toast_fmt("%s", toast);
  if (was_focus || was_viewing) ui::go_hub();
}


void drop_slot_if_viewing(GameKind kind, const char * peer_id, int idx) {
  const bool leave = focus_index() == idx || is_viewing(kind, peer_id);
  free_slot(idx);
  if (leave) ui::go_hub();
}

}  // namespace

Desk & desk() { return g_desk; }

void schedule(uint32_t delay_ms, void (*fn)(void *), void * user_data) {
  lv_timer_t * t = lv_timer_create(scheduled_cb, delay_ms, new Scheduled{fn, user_data});
  lv_timer_set_repeat_count(t, 1);
}

void init() {
  apply_runtime_defaults(g_desk);

  storage::load(g_desk);
  if (g_desk.timeout_id >= kTimeoutCount) g_desk.timeout_id = 2;
  net::link_init();
  games_init();
}

void save() { storage::save(g_desk); }

void factory_reset() {
  char id[proto::kMaxId];
  copy_str(id, sizeof(id), g_desk.id);

  page_log::clear();
  score_log::clear();
  checklist::clear_all();
  background::clear();
  clear_all_games();

  g_desk = Desk{};
  copy_str(g_desk.id, sizeof(g_desk.id), id);
  g_desk.name[0] = '\0';
  g_desk.setup_done = false;
  apply_runtime_defaults(g_desk);
  storage::save(g_desk);
}

const TimeoutSpec * timeout_specs() {
  static const TimeoutSpec specs[kTimeoutCount] = {
      {"1m", 60000}, {"3m", 180000}, {"5m", 300000}, {"10m", 600000}, {"Off", 0},
  };
  return specs;
}

bool busy() {
  const Desk & d = g_desk;
  return d.incoming.active || d.outgoing.active;
}

bool peer_saved(const char * id) {
  for (int i = 0; i < g_desk.peer_count; ++i)
    if (same(g_desk.peers[i].id, id)) return true;
  return false;
}

void add_peer(const char * id, const char * name) {
  if (peer_saved(id) || g_desk.peer_count >= kMaxPeers) return;
  copy_str(g_desk.peers[g_desk.peer_count].id, proto::kMaxId, id);
  copy_str(g_desk.peers[g_desk.peer_count].name, proto::kMaxName, name);
  g_desk.peer_count++;
  save();
}

void remove_peer(const char * id) {
  for (int i = 0; i < g_desk.peer_count; ++i) {
    if (same(g_desk.peers[i].id, id)) {
      for (int j = i; j < g_desk.peer_count - 1; ++j) g_desk.peers[j] = g_desk.peers[j + 1];
      g_desk.peer_count--;
      save();
      return;
    }
  }
}

void local_time(std::tm * out) {
  const std::time_t t =
      std::time(nullptr) + (std::time_t)(g_desk.clock_offset_ms / 1000);
#if defined(_WIN32)
  localtime_s(out, &t);
#else
  localtime_r(&t, out);
#endif
}

void adjust_clock_minutes(int minutes) {
  g_desk.clock_offset_ms += (int64_t)minutes * 60 * 1000;
  save();
}

void adjust_clock_days(int days) {
  g_desk.clock_offset_ms += (int64_t)days * 24 * 60 * 60 * 1000;
  save();
}

void send(const proto::Msg & msg) { net::link_send(msg); }

/* ============ incoming radio (mirrors web bus handler) ============ */

void handle_msg(const proto::Msg & m) {
  Desk & d = g_desk;

  switch (m.type) {
    case proto::MsgType::DiscoverReply: {
      for (int i = 0; i < d.nearby_count; ++i)
        if (same(d.nearby[i].id, m.from_id)) return;
      if (d.nearby_count < kMaxPeers) {
        copy_str(d.nearby[d.nearby_count].id, proto::kMaxId, m.from_id);
        copy_str(d.nearby[d.nearby_count].name, proto::kMaxName, m.from_name);
        d.nearby_count++;
      }
      ui::toast_fmt("%s is nearby", m.from_name);
      if (ui::current_screen() == ui::Screen::Settings) ui::go_settings();
      return;
    }

    case proto::MsgType::Call: {
      d.incoming.active = true;
      copy_str(d.incoming.from_id, proto::kMaxId, m.from_id);
      copy_str(d.incoming.from_name, proto::kMaxName, m.from_name);
      copy_str(d.incoming.emoji, proto::kMaxEmoji, m.emoji);
      copy_str(d.incoming.message, proto::kMaxMessage, m.message);
      page_log::add(page_log::Dir::In, m.from_name, m.emoji, m.message);
      ui::sync_ui();
      return;
    }

    case proto::MsgType::Ack: {
      if (d.outgoing.active) {
        d.outgoing.active = false;
        ui::sync_ui();
        ui::toast_fmt("%s shantayed", m.from_name);
      }
      return;
    }

    case proto::MsgType::Clear: {
      if (d.incoming.active && same(d.incoming.from_id, m.from_id)) {
        d.incoming.active = false;
        ui::sync_ui();
        ui::toast_fmt("%s cancelled", m.from_name);
      }
      return;
    }

    /* —— Tic Tac Toe —— */
    case proto::MsgType::TttInvite: {
      int idx = -1;
      if (!receive_invite(GameKind::Ttt, m, idx)) return;
      ui::toast_fmt("%s challenged you", m.from_name);
      return;
    }
    case proto::MsgType::TttAccept: {
      const int idx = find_slot(GameKind::Ttt, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.ttt.active || !slot->g.ttt.waiting) return;
      slot->g.ttt.waiting = false;
      copy_str(slot->g.ttt.opp_name, proto::kMaxName, m.from_name);
      if (slot->g.ttt.turn == slot->g.ttt.mark) note_turn_start(idx);
      refresh_viewing(GameKind::Ttt, m.from_id);
      ui::toast_fmt("%s accepted", m.from_name);
      return;
    }
    case proto::MsgType::TttDecline: {
      const int idx = find_slot(GameKind::Ttt, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.ttt.active || !slot->g.ttt.waiting) return;
      drop_slot_if_viewing(GameKind::Ttt, m.from_id, idx);
      ui::toast_fmt("%s declined", m.from_name);
      return;
    }
    case proto::MsgType::TttMove: {
      const int idx = find_slot(GameKind::Ttt, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      TttGame & g = slot->g.ttt;
      if (m.cell < 0 || m.cell >= 9 || g.board[m.cell]) return;
      g.board[m.cell] = m.mark;
      if (games::ttt::winner(g.board) || games::ttt::full(g.board)) {
        g.over = true;
        g.result_dismissed = false;
      } else {
        g.turn = m.mark == 'X' ? 'O' : 'X';
      }
      finish_remote_move(idx, GameKind::Ttt, m.from_id, g.opp_name,
                         !g.over && g.turn == g.mark);
      return;
    }
    case proto::MsgType::TttForfeit: {
      remove_remote_game(GameKind::Ttt, m, "Tic Tac Toe", "the game");
      return;
    }

    /* —— Super Tic Tac Toe —— */
    case proto::MsgType::StttInvite: {
      int idx = -1;
      if (!receive_invite(GameKind::Sttt, m, idx)) return;
      ui::toast_fmt("%s challenged you", m.from_name);
      return;
    }
    case proto::MsgType::StttAccept: {
      const int idx = find_slot(GameKind::Sttt, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.sttt.active || !slot->g.sttt.waiting) return;
      slot->g.sttt.waiting = false;
      copy_str(slot->g.sttt.opp_name, proto::kMaxName, m.from_name);
      if (slot->g.sttt.turn == slot->g.sttt.mark) note_turn_start(idx);
      refresh_viewing(GameKind::Sttt, m.from_id);
      ui::toast_fmt("%s accepted", m.from_name);
      return;
    }
    case proto::MsgType::StttDecline: {
      const int idx = find_slot(GameKind::Sttt, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.sttt.active || !slot->g.sttt.waiting) return;
      drop_slot_if_viewing(GameKind::Sttt, m.from_id, idx);
      ui::toast_fmt("%s declined", m.from_name);
      return;
    }
    case proto::MsgType::StttMove: {
      const int idx = find_slot(GameKind::Sttt, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      StttGame & g = slot->g.sttt;
      if (!games::sttt::play(g.boards, g.meta, g.next_board, m.col, m.cell, m.mark)) return;
      if (games::sttt::over(g.meta)) {
        g.over = true;
        g.result_dismissed = false;
      } else {
        g.turn = m.mark == 'X' ? 'O' : 'X';
      }
      finish_remote_move(idx, GameKind::Sttt, m.from_id, g.opp_name,
                         !g.over && g.turn == g.mark);
      return;
    }
    case proto::MsgType::StttForfeit: {
      remove_remote_game(GameKind::Sttt, m, "Super TTT", "the game");
      return;
    }

    /* —— Connect Four —— */
    case proto::MsgType::C4Invite: {
      int idx = -1;
      if (!receive_invite(GameKind::C4, m, idx)) return;
      ui::toast_fmt("%s challenged you - Connect Four", m.from_name);
      return;
    }
    case proto::MsgType::C4Accept: {
      const int idx = find_slot(GameKind::C4, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      C4Game & g = slot->g.c4;
      if (!g.active || !g.waiting) return;
      g.waiting = false;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      g.opp_color = (m.color >= 0 && m.color < games::c4::kColorCount) ? m.color : 1;
      if (g.turn == g.my_color) note_turn_start(idx);
      refresh_viewing(GameKind::C4, m.from_id);
      return;
    }
    case proto::MsgType::C4Decline: {
      const int idx = find_slot(GameKind::C4, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.c4.active || !slot->g.c4.waiting) return;
      drop_slot_if_viewing(GameKind::C4, m.from_id, idx);
      ui::toast_fmt("%s declined", m.from_name);
      return;
    }
    case proto::MsgType::C4Drop: {
      const int idx = find_slot(GameKind::C4, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      C4Game & g = slot->g.c4;
      const int8_t color = m.color >= 0 ? m.color : g.opp_color;
      const int row = games::c4::drop(g.board, m.col, color);
      if (row < 0) return;
      g.last_r = (int8_t)row;
      g.last_c = m.col;
      const int w = games::c4::winner(g.board);
      if (w >= 0) {
        g.over = true;
        g.result_dismissed = false;
      } else {
        g.turn = g.my_color;
      }
      finish_remote_move(idx, GameKind::C4, m.from_id, g.opp_name,
                         !g.over && g.turn == g.my_color);
      return;
    }
    case proto::MsgType::C4Forfeit: {
      remove_remote_game(GameKind::C4, m, "Connect Four", "Connect Four");
      return;
    }

    /* —— Battleship —— */
    case proto::MsgType::BsInvite: {
      int idx = -1;
      if (!receive_invite(GameKind::Bs, m, idx)) return;
      ui::toast_fmt("%s challenged you - Battleship", m.from_name);
      return;
    }
    case proto::MsgType::BsAccept: {
      const int idx = find_slot(GameKind::Bs, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      BsGame & g = slot->g.bs;
      if (!g.active || !g.waiting) return;
      g.waiting = false;
      g.setup = true;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      copy_str(g.last_msg, sizeof(g.last_msg), "Place your fleet");
      games::bs::clear_fleet(g.fleet);
      g.me_ready = false;
      g.anchor_x = g.anchor_y = -1;
      g.selected_ship = -1;
      note_turn_start(idx);
      refresh_viewing(GameKind::Bs, m.from_id);
      return;
    }
    case proto::MsgType::BsDecline: {
      const int idx = find_slot(GameKind::Bs, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.bs.active || !slot->g.bs.waiting) return;
      drop_slot_if_viewing(GameKind::Bs, m.from_id, idx);
      ui::toast_fmt("%s declined", m.from_name);
      return;
    }
    case proto::MsgType::BsReady: {
      const int idx = find_slot(GameKind::Bs, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      BsGame & g = slot->g.bs;
      g.opp_ready = true;
      maybe_start_bs_battle(g);
      finish_remote_move(idx, GameKind::Bs, m.from_id, g.opp_name,
                         !g.setup && g.my_turn);
      return;
    }
    case proto::MsgType::BsFire: {
      /* web handleIncomingFire */
      const int idx = find_slot(GameKind::Bs, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      BsGame & g = slot->g.bs;
      if (!g.active || g.setup || m.x < 0 || m.x >= games::bs::kGrid || m.y < 0 ||
          m.y >= games::bs::kGrid)
        return;
      const auto res = games::bs::resolve_fire(g.fleet, m.x, m.y);
      if (!res.hit) g.fleet_miss[m.y][m.x] = true;

      proto::Msg r;
      r.type = proto::MsgType::BsResult;
      copy_str(r.from_id, proto::kMaxId, d.id);
      copy_str(r.to_id, proto::kMaxId, m.from_id);
      r.x = m.x;
      r.y = m.y;
      r.hit = res.hit;
      r.sunk = res.sunk;
      r.game_over = res.game_over;
      send(r);

      if (res.game_over) {
        g.over = true;
        g.i_won = false;
        g.result_dismissed = false;
        copy_str(g.last_msg, sizeof(g.last_msg), "Fleet destroyed");
      } else {
        g.my_turn = true;
        copy_str(g.last_msg, sizeof(g.last_msg),
                 res.hit ? "They hit you! Your turn." : "Missed you. Your turn.");
        g.mode = 0;
      }
      finish_remote_move(idx, GameKind::Bs, m.from_id, g.opp_name,
                         !g.over && g.my_turn);
      return;
    }
    case proto::MsgType::BsResult: {
      const int idx = find_slot(GameKind::Bs, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      BsGame & g = slot->g.bs;
      if (!g.active || m.x < 0 || m.x >= games::bs::kGrid || m.y < 0 ||
          m.y >= games::bs::kGrid)
        return;
      g.tracking[m.y][m.x] = m.hit ? 1 : 2;
      if (m.game_over) {
        g.over = true;
        g.i_won = true;
        g.result_dismissed = false;
        copy_str(g.last_msg, sizeof(g.last_msg), "You sank their fleet!");
      } else {
        copy_str(g.last_msg, sizeof(g.last_msg), m.hit ? (m.sunk ? "Hit - sunk!" : "Hit!") : "Miss");
        g.my_turn = false;
        g.mode = 1;
      }
      finish_remote_move(idx, GameKind::Bs, m.from_id, g.opp_name, false);
      return;
    }
    case proto::MsgType::BsForfeit: {
      remove_remote_game(GameKind::Bs, m, "Battleship", "Battleship");
      return;
    }

    /* —— Checkers —— */
    case proto::MsgType::CkInvite: {
      int idx = -1;
      if (!receive_invite(GameKind::Ck, m, idx)) return;
      ui::toast_fmt("%s challenged you - Checkers", m.from_name);
      return;
    }
    case proto::MsgType::CkAccept: {
      const int idx = find_slot(GameKind::Ck, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      CkGame & g = slot->g.ck;
      if (!g.active || !g.waiting) return;
      g.waiting = false;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      if (g.turn == g.side) note_turn_start(idx);
      refresh_viewing(GameKind::Ck, m.from_id);
      return;
    }
    case proto::MsgType::CkDecline: {
      const int idx = find_slot(GameKind::Ck, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.ck.active || !slot->g.ck.waiting) return;
      drop_slot_if_viewing(GameKind::Ck, m.from_id, idx);
      ui::toast_fmt("%s declined", m.from_name);
      return;
    }
    case proto::MsgType::CkMove: {
      /* web applyIncomingCkMove */
      const int idx = find_slot(GameKind::Ck, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      CkGame & g = slot->g.ck;
      games::ck::Move mv{m.from_x, m.from_y, m.to_x, m.to_y,
                         (int8_t)(m.to_x - m.from_x) == 2 || (int8_t)(m.from_x - m.to_x) == 2};
      games::ck::apply_move(g.board, mv);
      if (mv.jump) {
        const char opp_side = g.side == 'r' ? 'b' : 'r';
        games::ck::Move more[16];
        const int n = games::ck::legal_moves(g.board, opp_side, mv.tx, mv.ty, more, 16);
        bool has_jump = false;
        for (int i = 0; i < n; ++i)
          if (more[i].jump) has_jump = true;
        if (has_jump) {
          /* opponent still jumping — keep their turn */
          refresh_viewing(GameKind::Ck, m.from_id);
          return;
        }
      }
      g.turn = g.side;
      games::ck::Move any[64];
      if (games::ck::count_side(g.board, 'r') == 0 || games::ck::count_side(g.board, 'b') == 0 ||
          games::ck::legal_moves(g.board, g.turn, -1, -1, any, 64) == 0) {
        g.over = true;
        g.result_dismissed = false;
      }
      finish_remote_move(idx, GameKind::Ck, m.from_id, g.opp_name,
                         !g.over && g.turn == g.side);
      return;
    }
    case proto::MsgType::CkForfeit: {
      remove_remote_game(GameKind::Ck, m, "Checkers", "Checkers");
      return;
    }

    /* —— Memory —— */
    case proto::MsgType::MemInvite: {
      int idx = -1;
      if (!receive_invite(GameKind::Mem, m, idx)) return;
      ui::toast_fmt("%s challenged you - Memory", m.from_name);
      return;
    }
    case proto::MsgType::MemAccept: {
      const int idx = find_slot(GameKind::Mem, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      MemGame & g = slot->g.mem;
      if (!g.active || !g.waiting) return;
      g.waiting = false;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      if (g.my_turn) note_turn_start(idx);
      refresh_viewing(GameKind::Mem, m.from_id);
      return;
    }
    case proto::MsgType::MemDecline: {
      const int idx = find_slot(GameKind::Mem, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.mem.active || !slot->g.mem.waiting) return;
      drop_slot_if_viewing(GameKind::Mem, m.from_id, idx);
      ui::toast_fmt("%s declined", m.from_name);
      return;
    }
    case proto::MsgType::MemFlip: {
      /* web applyIncomingMemFlip */
      const int idx = find_slot(GameKind::Mem, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      MemGame & g = slot->g.mem;
      if (m.card_a < 0 || m.card_a >= games::mem::kCards || m.card_b < 0 ||
          m.card_b >= games::mem::kCards)
        return;
      g.flip_a = m.card_a;
      g.flip_b = m.card_b;
      g.local_flip = -1;
      g.lock = true;
      g.my_turn = false;
      refresh_viewing(GameKind::Mem, m.from_id);
      schedule_mem_resolve(m.card_a, m.card_b, false, m.from_id);
      return;
    }
    case proto::MsgType::MemForfeit: {
      remove_remote_game(GameKind::Mem, m, "Memory", "Memory");
      return;
    }

    /* —— Reversi —— */
    case proto::MsgType::RvInvite: {
      int idx = -1;
      if (!receive_invite(GameKind::Rv, m, idx)) return;
      ui::toast_fmt("%s challenged you - Reversi", m.from_name);
      return;
    }
    case proto::MsgType::RvAccept: {
      const int idx = find_slot(GameKind::Rv, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      RvGame & g = slot->g.rv;
      if (!g.active || !g.waiting) return;
      g.waiting = false;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      if (g.turn == g.my_color) note_turn_start(idx);
      refresh_viewing(GameKind::Rv, m.from_id);
      return;
    }
    case proto::MsgType::RvDecline: {
      const int idx = find_slot(GameKind::Rv, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.rv.active || !slot->g.rv.waiting) return;
      drop_slot_if_viewing(GameKind::Rv, m.from_id, idx);
      ui::toast_fmt("%s declined", m.from_name);
      return;
    }
    case proto::MsgType::RvMove: {
      const int idx = find_slot(GameKind::Rv, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      RvGame & g = slot->g.rv;
      const int8_t color = (g.my_color == games::rv::kBlack) ? games::rv::kWhite : games::rv::kBlack;
      if (m.x < 0 && m.y < 0) {
        /* Pass — board unchanged */
      } else if (!games::rv::apply(g.board, m.x, m.y, color)) {
        return;
      }
      const int8_t w = games::rv::check_over(g.board);
      if (w != 0) {
        g.over = true;
        g.result_dismissed = false;
      } else if (games::rv::any_move(g.board, g.my_color)) {
        g.turn = g.my_color;
      } else {
        /* I must pass; opponent plays again after we send pass from UI */
        g.turn = g.my_color; /* UI auto-passes when no legal move */
      }
      finish_remote_move(idx, GameKind::Rv, m.from_id, g.opp_name,
                         !g.over && g.turn == g.my_color);
      return;
    }
    case proto::MsgType::RvForfeit: {
      remove_remote_game(GameKind::Rv, m, "Reversi", "Reversi");
      return;
    }

    /* —— Dots & Boxes —— */
    case proto::MsgType::DbInvite: {
      int idx = -1;
      if (!receive_invite(GameKind::Db, m, idx)) return;
      ui::toast_fmt("%s challenged you - Dots & Boxes", m.from_name);
      return;
    }
    case proto::MsgType::DbAccept: {
      const int idx = find_slot(GameKind::Db, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      DbGame & g = slot->g.db;
      if (!g.active || !g.waiting) return;
      g.waiting = false;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      if (g.turn == g.my_side) note_turn_start(idx);
      refresh_viewing(GameKind::Db, m.from_id);
      return;
    }
    case proto::MsgType::DbDecline: {
      const int idx = find_slot(GameKind::Db, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending || !slot->g.db.active || !slot->g.db.waiting) return;
      drop_slot_if_viewing(GameKind::Db, m.from_id, idx);
      ui::toast_fmt("%s declined", m.from_name);
      return;
    }
    case proto::MsgType::DbLine: {
      const int idx = find_slot(GameKind::Db, m.from_id);
      GameSlot * slot = slot_at(idx);
      if (!slot || slot->invite_pending) return;
      DbGame & g = slot->g.db;
      const int8_t side = (g.my_side == games::db::kP1) ? games::db::kP2 : games::db::kP1;
      const int claimed = games::db::claim(g.state, m.y /*is_vert*/, m.x /*r*/, m.col /*c*/, side);
      if (claimed < 0) return;
      if (games::db::over(g.state)) {
        g.over = true;
        g.result_dismissed = false;
      } else if (claimed == 0) {
        g.turn = g.my_side;
      } else {
        g.turn = side;
      }
      finish_remote_move(idx, GameKind::Db, m.from_id, g.opp_name,
                         !g.over && g.turn == g.my_side);
      return;
    }
    case proto::MsgType::DbForfeit: {
      remove_remote_game(GameKind::Db, m, "Dots & Boxes", "Dots & Boxes");
      return;
    }

    /* —— Doodle —— */
    case proto::MsgType::DoodleStroke: {
      if (!d.doodle_peer_id[0]) {
        copy_str(d.doodle_peer_id, proto::kMaxId, m.from_id);
        copy_str(d.doodle_peer_name, proto::kMaxName, m.from_name);
      }
      if (ui::current_screen() != ui::Screen::Doodle) {
        ui::toast_fmt("Doodle from %s", m.from_name);
        ui::go_doodle();
      }
      ui::doodle_apply_remote_stroke(m);
      return;
    }
    case proto::MsgType::DoodleClear: {
      /* Always wipe session — even if not currently on the doodle screen. */
      ui::doodle_remote_clear();
      return;
    }

    default:
      return;
  }
}

}  // namespace app
}  // namespace wp
