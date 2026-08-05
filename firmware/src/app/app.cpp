#include "app/app.h"

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
const char * kDefaultCanned[kCannedCount] = {"Got a sec?", "Come here", "Lunch?", "Urgent"};

void copy_str(char * dst, size_t cap, const char * src) {
  std::snprintf(dst, cap, "%s", src ? src : "");
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
};

void mem_resolve_cb(void * ud) {
  auto * r = static_cast<MemResolve *>(ud);
  MemGame & g = g_desk.mem;
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
    if (ui::current_screen() == ui::Screen::Mem) ui::go_memory();
  }
  delete r;
}

void schedule_mem_resolve(int8_t a, int8_t b, bool i_played) {
  schedule(700, mem_resolve_cb, new MemResolve{a, b, i_played});
}

/* Web maybeStartBsBattle */
void maybe_start_bs_battle() {
  BsGame & g = g_desk.bs;
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

}  // namespace

Desk & desk() { return g_desk; }

void schedule(uint32_t delay_ms, void (*fn)(void *), void * user_data) {
  lv_timer_t * t = lv_timer_create(scheduled_cb, delay_ms, new Scheduled{fn, user_data});
  lv_timer_set_repeat_count(t, 1);
}

void init() {
  for (int i = 0; i < kEmojiSlots; ++i) copy_str(g_desk.emojis[i], proto::kMaxEmoji, kDefaultEmojis[i]);
  for (int i = 0; i < kCannedCount; ++i)
    copy_str(g_desk.canned[i], proto::kMaxMessage, kDefaultCanned[i]);

  /* Default roster mirrors web: Will starts saved. */
  copy_str(g_desk.peers[0].id, proto::kMaxId, "mac-will");
  copy_str(g_desk.peers[0].name, proto::kMaxName, "Will");
  g_desk.peer_count = 1;

  storage::load(g_desk);
  net::link_init();
}

void save() { storage::save(g_desk); }

bool busy() {
  const Desk & d = g_desk;
  return d.incoming.active || d.outgoing.active || d.ttt_invite.active || d.ttt.active ||
         d.sttt_invite.active || d.sttt.active || d.c4_invite.active || d.c4.active ||
         d.bs_invite.active || d.bs.active || d.ck_invite.active || d.ck.active ||
         d.mem_invite.active || d.mem.active;
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

const TimeoutSpec * timeout_specs() {
  static const TimeoutSpec specs[4] = {
      {"30s", 30000}, {"1m", 60000}, {"5m", 300000}, {"Off", 0},
  };
  return specs;
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
      if (d.ttt.active || d.ttt_invite.active) return; /* busy */
      d.ttt_invite.active = true;
      copy_str(d.ttt_invite.from_id, proto::kMaxId, m.from_id);
      copy_str(d.ttt_invite.from_name, proto::kMaxName, m.from_name);
      ui::go_ttt();
      ui::toast_fmt("%s challenged you", m.from_name);
      return;
    }
    case proto::MsgType::TttAccept: {
      if (!d.ttt.active || !d.ttt.waiting || !same(d.ttt.opp_id, m.from_id)) return;
      d.ttt.waiting = false;
      copy_str(d.ttt.opp_name, proto::kMaxName, m.from_name);
      ui::go_ttt();
      ui::toast_fmt("%s accepted", m.from_name);
      return;
    }
    case proto::MsgType::TttDecline: {
      if (!d.ttt.active || !d.ttt.waiting || !same(d.ttt.opp_id, m.from_id)) return;
      d.ttt.active = false;
      ui::toast_fmt("%s declined", m.from_name);
      ui::sync_ui();
      return;
    }
    case proto::MsgType::TttMove: {
      TttGame & g = d.ttt;
      if (!g.active || !same(g.opp_id, m.from_id)) return;
      if (m.cell < 0 || m.cell >= 9 || g.board[m.cell]) return;
      g.board[m.cell] = m.mark;
      if (games::ttt::winner(g.board) || games::ttt::full(g.board)) {
        g.over = true;
        g.result_dismissed = false;
      } else {
        g.turn = m.mark == 'X' ? 'O' : 'X';
      }
      ui::go_ttt();
      return;
    }
    case proto::MsgType::TttForfeit: {
      bool changed = false;
      if (d.ttt_invite.active && same(d.ttt_invite.from_id, m.from_id)) {
        d.ttt_invite.active = false;
        changed = true;
      }
      if (d.ttt.active && same(d.ttt.opp_id, m.from_id)) {
        d.ttt.active = false;
        changed = true;
      }
      if (changed) {
        ui::toast_fmt("%s left the game", m.from_name);
        ui::sync_ui();
      }
      return;
    }

    /* —— Super Tic Tac Toe —— */
    case proto::MsgType::StttInvite: {
      if (d.sttt.active || d.sttt_invite.active) return;
      d.sttt_invite.active = true;
      copy_str(d.sttt_invite.from_id, proto::kMaxId, m.from_id);
      copy_str(d.sttt_invite.from_name, proto::kMaxName, m.from_name);
      ui::go_sttt();
      ui::toast_fmt("%s challenged you", m.from_name);
      return;
    }
    case proto::MsgType::StttAccept: {
      if (!d.sttt.active || !d.sttt.waiting || !same(d.sttt.opp_id, m.from_id)) return;
      d.sttt.waiting = false;
      copy_str(d.sttt.opp_name, proto::kMaxName, m.from_name);
      ui::go_sttt();
      ui::toast_fmt("%s accepted", m.from_name);
      return;
    }
    case proto::MsgType::StttDecline: {
      if (!d.sttt.active || !d.sttt.waiting || !same(d.sttt.opp_id, m.from_id)) return;
      d.sttt.active = false;
      ui::toast_fmt("%s declined", m.from_name);
      ui::sync_ui();
      return;
    }
    case proto::MsgType::StttMove: {
      StttGame & g = d.sttt;
      if (!g.active || !same(g.opp_id, m.from_id)) return;
      if (!games::sttt::play(g.boards, g.meta, g.next_board, m.col, m.cell, m.mark)) return;
      if (games::sttt::over(g.meta)) {
        g.over = true;
        g.result_dismissed = false;
      } else {
        g.turn = m.mark == 'X' ? 'O' : 'X';
      }
      ui::go_sttt();
      return;
    }
    case proto::MsgType::StttForfeit: {
      bool changed = false;
      if (d.sttt_invite.active && same(d.sttt_invite.from_id, m.from_id)) {
        d.sttt_invite.active = false;
        changed = true;
      }
      if (d.sttt.active && same(d.sttt.opp_id, m.from_id)) {
        d.sttt.active = false;
        changed = true;
      }
      if (changed) {
        ui::toast_fmt("%s left the game", m.from_name);
        ui::sync_ui();
      }
      return;
    }

    /* —— Connect Four —— */
    case proto::MsgType::C4Invite: {
      if (d.c4.active || d.c4_invite.active) return;
      d.c4_invite.active = true;
      copy_str(d.c4_invite.from_id, proto::kMaxId, m.from_id);
      copy_str(d.c4_invite.from_name, proto::kMaxName, m.from_name);
      d.c4_invite.color = (m.color >= 0 && m.color < games::c4::kColorCount) ? m.color : 0;
      ui::go_c4();
      ui::toast_fmt("%s challenged you - Connect Four", m.from_name);
      return;
    }
    case proto::MsgType::C4Accept: {
      C4Game & g = d.c4;
      if (!g.active || !g.waiting || !same(g.opp_id, m.from_id)) return;
      g.waiting = false;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      g.opp_color = (m.color >= 0 && m.color < games::c4::kColorCount) ? m.color : 1;
      ui::go_c4();
      return;
    }
    case proto::MsgType::C4Decline: {
      if (d.c4.active && d.c4.waiting && same(d.c4.opp_id, m.from_id)) {
        d.c4.active = false;
        ui::toast_fmt("%s declined", m.from_name);
        ui::sync_ui();
      }
      return;
    }
    case proto::MsgType::C4Drop: {
      C4Game & g = d.c4;
      if (!g.active || !same(g.opp_id, m.from_id)) return;
      const int8_t color = m.color >= 0 ? m.color : g.opp_color;
      const int row = games::c4::drop(g.board, m.col, color);
      if (row >= 0) {
        g.last_r = (int8_t)row;
        g.last_c = m.col;
      }
      const int w = games::c4::winner(g.board);
      if (w >= 0) {
        g.over = true;
        g.result_dismissed = false;
      } else {
        g.turn = g.my_color;
      }
      ui::go_c4();
      return;
    }
    case proto::MsgType::C4Forfeit: {
      bool changed = false;
      if (d.c4_invite.active && same(d.c4_invite.from_id, m.from_id)) {
        d.c4_invite.active = false;
        changed = true;
      }
      if (d.c4.active && same(d.c4.opp_id, m.from_id)) {
        d.c4.active = false;
        changed = true;
      }
      if (changed) {
        ui::toast_fmt("%s left Connect Four", m.from_name);
        ui::sync_ui();
      }
      return;
    }

    /* —— Battleship —— */
    case proto::MsgType::BsInvite: {
      if (d.bs.active || d.bs_invite.active) return;
      d.bs_invite.active = true;
      copy_str(d.bs_invite.from_id, proto::kMaxId, m.from_id);
      copy_str(d.bs_invite.from_name, proto::kMaxName, m.from_name);
      ui::go_battleship();
      ui::toast_fmt("%s challenged you - Battleship", m.from_name);
      return;
    }
    case proto::MsgType::BsAccept: {
      BsGame & g = d.bs;
      if (!g.active || !g.waiting || !same(g.opp_id, m.from_id)) return;
      g.waiting = false;
      g.setup = true;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      copy_str(g.last_msg, sizeof(g.last_msg), "Place your fleet");
      games::bs::clear_fleet(g.fleet);
      g.me_ready = false;
      g.anchor_x = g.anchor_y = -1;
      g.selected_ship = -1;
      ui::go_battleship();
      return;
    }
    case proto::MsgType::BsDecline: {
      if (d.bs.active && d.bs.waiting && same(d.bs.opp_id, m.from_id)) {
        d.bs.active = false;
        ui::toast_fmt("%s declined", m.from_name);
        ui::sync_ui();
      }
      return;
    }
    case proto::MsgType::BsReady: {
      BsGame & g = d.bs;
      if (!g.active || !same(g.opp_id, m.from_id)) return;
      g.opp_ready = true;
      maybe_start_bs_battle();
      ui::go_battleship();
      return;
    }
    case proto::MsgType::BsFire: {
      /* web handleIncomingFire */
      BsGame & g = d.bs;
      if (!g.active || g.setup) return;
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
      ui::go_battleship();
      return;
    }
    case proto::MsgType::BsResult: {
      BsGame & g = d.bs;
      if (!g.active) return;
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
      ui::go_battleship();
      return;
    }
    case proto::MsgType::BsForfeit: {
      bool changed = false;
      if (d.bs_invite.active && same(d.bs_invite.from_id, m.from_id)) {
        d.bs_invite.active = false;
        changed = true;
      }
      if (d.bs.active && same(d.bs.opp_id, m.from_id)) {
        d.bs.active = false;
        changed = true;
      }
      if (changed) {
        ui::toast_fmt("%s left Battleship", m.from_name);
        ui::sync_ui();
      }
      return;
    }

    /* —— Checkers —— */
    case proto::MsgType::CkInvite: {
      if (d.ck.active || d.ck_invite.active) return;
      d.ck_invite.active = true;
      copy_str(d.ck_invite.from_id, proto::kMaxId, m.from_id);
      copy_str(d.ck_invite.from_name, proto::kMaxName, m.from_name);
      ui::go_checkers();
      ui::toast_fmt("%s challenged you - Checkers", m.from_name);
      return;
    }
    case proto::MsgType::CkAccept: {
      CkGame & g = d.ck;
      if (!g.active || !g.waiting || !same(g.opp_id, m.from_id)) return;
      g.waiting = false;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      ui::go_checkers();
      return;
    }
    case proto::MsgType::CkDecline: {
      if (d.ck.active && d.ck.waiting && same(d.ck.opp_id, m.from_id)) {
        d.ck.active = false;
        ui::toast_fmt("%s declined", m.from_name);
        ui::sync_ui();
      }
      return;
    }
    case proto::MsgType::CkMove: {
      /* web applyIncomingCkMove */
      CkGame & g = d.ck;
      if (!g.active || !same(g.opp_id, m.from_id)) return;
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
          if (ui::current_screen() == ui::Screen::Ck) ui::go_checkers();
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
      ui::go_checkers();
      return;
    }
    case proto::MsgType::CkForfeit: {
      bool changed = false;
      if (d.ck_invite.active && same(d.ck_invite.from_id, m.from_id)) {
        d.ck_invite.active = false;
        changed = true;
      }
      if (d.ck.active && same(d.ck.opp_id, m.from_id)) {
        d.ck.active = false;
        changed = true;
      }
      if (changed) {
        ui::toast_fmt("%s left Checkers", m.from_name);
        ui::sync_ui();
      }
      return;
    }

    /* —— Memory —— */
    case proto::MsgType::MemInvite: {
      if (d.mem.active || d.mem_invite.active) return;
      d.mem_invite.active = true;
      copy_str(d.mem_invite.from_id, proto::kMaxId, m.from_id);
      copy_str(d.mem_invite.from_name, proto::kMaxName, m.from_name);
      d.mem_invite.seed = m.seed;
      ui::go_memory();
      ui::toast_fmt("%s challenged you - Memory", m.from_name);
      return;
    }
    case proto::MsgType::MemAccept: {
      MemGame & g = d.mem;
      if (!g.active || !g.waiting || !same(g.opp_id, m.from_id)) return;
      g.waiting = false;
      copy_str(g.opp_name, proto::kMaxName, m.from_name);
      ui::go_memory();
      return;
    }
    case proto::MsgType::MemDecline: {
      if (d.mem.active && d.mem.waiting && same(d.mem.opp_id, m.from_id)) {
        d.mem.active = false;
        ui::toast_fmt("%s declined", m.from_name);
        ui::sync_ui();
      }
      return;
    }
    case proto::MsgType::MemFlip: {
      /* web applyIncomingMemFlip */
      MemGame & g = d.mem;
      if (!g.active || !same(g.opp_id, m.from_id)) return;
      g.flip_a = m.card_a;
      g.flip_b = m.card_b;
      g.local_flip = -1;
      g.lock = true;
      g.my_turn = false;
      ui::go_memory();
      schedule_mem_resolve(m.card_a, m.card_b, false);
      return;
    }
    case proto::MsgType::MemForfeit: {
      bool changed = false;
      if (d.mem_invite.active && same(d.mem_invite.from_id, m.from_id)) {
        d.mem_invite.active = false;
        changed = true;
      }
      if (d.mem.active && same(d.mem.opp_id, m.from_id)) {
        d.mem.active = false;
        changed = true;
      }
      if (changed) {
        ui::toast_fmt("%s left Memory", m.from_name);
        ui::sync_ui();
      }
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
