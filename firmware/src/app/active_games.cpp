#include "app/active_games.h"

#include "app/score_log.h"
#include "net/link.h"
#include "protocol/messages.h"
#include "ui/chrome.h"
#include "ui/nav.h"

#include "lvgl/lvgl.h"

#include <cstdio>

namespace wp {
namespace app {
namespace {

GameSlot g_slots[kMaxActiveGames];
int g_focus = -1;
lv_timer_t * g_forfeit_timer = nullptr;

bool same(const char * a, const char * b) { return a && b && std::strcmp(a, b) == 0; }

void copy_str(char * dst, size_t cap, const char * src) {
  std::snprintf(dst, cap, "%s", src ? src : "");
}

const char * opp_id_of(const GameSlot & s) {
  if (s.invite_pending) return s.invite.from_id;
  switch (s.kind) {
    case GameKind::Ttt: return s.g.ttt.opp_id;
    case GameKind::Sttt: return s.g.sttt.opp_id;
    case GameKind::C4: return s.g.c4.opp_id;
    case GameKind::Bs: return s.g.bs.opp_id;
    case GameKind::Ck: return s.g.ck.opp_id;
    case GameKind::Mem: return s.g.mem.opp_id;
    case GameKind::Rv: return s.g.rv.opp_id;
    case GameKind::Db: return s.g.db.opp_id;
    default: return "";
  }
}

const char * opp_name_of(const GameSlot & s) {
  if (s.invite_pending) return s.invite.from_name;
  switch (s.kind) {
    case GameKind::Ttt: return s.g.ttt.opp_name;
    case GameKind::Sttt: return s.g.sttt.opp_name;
    case GameKind::C4: return s.g.c4.opp_name;
    case GameKind::Bs: return s.g.bs.opp_name;
    case GameKind::Ck: return s.g.ck.opp_name;
    case GameKind::Mem: return s.g.mem.opp_name;
    case GameKind::Rv: return s.g.rv.opp_name;
    case GameKind::Db: return s.g.db.opp_name;
    default: return "";
  }
}

bool slot_live(const GameSlot & s) {
  if (!s.used) return false;
  if (s.invite_pending) return s.invite.active;
  switch (s.kind) {
    case GameKind::Ttt: return s.g.ttt.active;
    case GameKind::Sttt: return s.g.sttt.active;
    case GameKind::C4: return s.g.c4.active;
    case GameKind::Bs: return s.g.bs.active;
    case GameKind::Ck: return s.g.ck.active;
    case GameKind::Mem: return s.g.mem.active;
    case GameKind::Rv: return s.g.rv.active;
    case GameKind::Db: return s.g.db.active;
    default: return false;
  }
}

ui::Screen screen_for(GameKind k) {
  switch (k) {
    case GameKind::Ttt: return ui::Screen::Ttt;
    case GameKind::Sttt: return ui::Screen::Sttt;
    case GameKind::C4: return ui::Screen::C4;
    case GameKind::Bs: return ui::Screen::Bs;
    case GameKind::Ck: return ui::Screen::Ck;
    case GameKind::Mem: return ui::Screen::Mem;
    case GameKind::Rv: return ui::Screen::Rv;
    case GameKind::Db: return ui::Screen::Db;
    default: return ui::Screen::Hub;
  }
}

void go_kind(GameKind k) {
  switch (k) {
    case GameKind::Ttt: ui::go_ttt(); break;
    case GameKind::Sttt: ui::go_sttt(); break;
    case GameKind::C4: ui::go_c4(); break;
    case GameKind::Bs: ui::go_battleship(); break;
    case GameKind::Ck: ui::go_checkers(); break;
    case GameKind::Mem: ui::go_memory(); break;
    case GameKind::Rv: ui::go_reversi(); break;
    case GameKind::Db: ui::go_dots(); break;
    default: ui::go_hub(); break;
  }
}

void fill_from(proto::Msg & m) {
  copy_str(m.from_id, sizeof(m.from_id), desk().id);
  copy_str(m.from_name, sizeof(m.from_name), desk().name);
}

void send_forfeit(GameKind k, const char * to_id) {
  proto::Msg m;
  switch (k) {
    case GameKind::Ttt: m.type = proto::MsgType::TttForfeit; break;
    case GameKind::Sttt: m.type = proto::MsgType::StttForfeit; break;
    case GameKind::C4: m.type = proto::MsgType::C4Forfeit; break;
    case GameKind::Bs: m.type = proto::MsgType::BsForfeit; break;
    case GameKind::Ck: m.type = proto::MsgType::CkForfeit; break;
    case GameKind::Mem: m.type = proto::MsgType::MemForfeit; break;
    case GameKind::Rv: m.type = proto::MsgType::RvForfeit; break;
    case GameKind::Db: m.type = proto::MsgType::DbForfeit; break;
    default: return;
  }
  fill_from(m);
  copy_str(m.to_id, sizeof(m.to_id), to_id);
  send(m);
}

void send_decline(GameKind k, const char * to_id) {
  proto::Msg m;
  switch (k) {
    case GameKind::Ttt: m.type = proto::MsgType::TttDecline; break;
    case GameKind::Sttt: m.type = proto::MsgType::StttDecline; break;
    case GameKind::C4: m.type = proto::MsgType::C4Decline; break;
    case GameKind::Bs: m.type = proto::MsgType::BsDecline; break;
    case GameKind::Ck: m.type = proto::MsgType::CkDecline; break;
    case GameKind::Mem: m.type = proto::MsgType::MemDecline; break;
    case GameKind::Rv: m.type = proto::MsgType::RvDecline; break;
    case GameKind::Db: m.type = proto::MsgType::DbDecline; break;
    default: return;
  }
  fill_from(m);
  copy_str(m.to_id, sizeof(m.to_id), to_id);
  send(m);
}

bool outgoing_wait(const GameSlot & s) {
  if (!s.used || s.invite_pending) return false;
  switch (s.kind) {
    case GameKind::Ttt: return s.g.ttt.active && s.g.ttt.waiting;
    case GameKind::Sttt: return s.g.sttt.active && s.g.sttt.waiting;
    case GameKind::C4: return s.g.c4.active && s.g.c4.waiting;
    case GameKind::Bs: return s.g.bs.active && s.g.bs.waiting;
    case GameKind::Ck: return s.g.ck.active && s.g.ck.waiting;
    case GameKind::Mem: return s.g.mem.active && s.g.mem.waiting;
    case GameKind::Rv: return s.g.rv.active && s.g.rv.waiting;
    case GameKind::Db: return s.g.db.active && s.g.db.waiting;
    default: return false;
  }
}

void auto_forfeit_slot(int idx) {
  GameSlot & s = g_slots[idx];
  if (!slot_live(s) || s.invite_pending) return;
  if (!is_my_turn(s)) return;
  const GameKind k = s.kind;
  const char * opp = opp_name_of(s);
  const char * oid = opp_id_of(s);
  char opp_copy[proto::kMaxName];
  copy_str(opp_copy, sizeof(opp_copy), opp);
  char oid_copy[proto::kMaxId];
  copy_str(oid_copy, sizeof(oid_copy), oid);
  score_log::note(kind_name(k), opp_copy, score_log::Outcome::ForfeitSelf);
  send_forfeit(k, oid_copy);
  char toast[96];
  std::snprintf(toast, sizeof(toast), "%s vs %s: auto-forfeit (24h)", kind_name(k), opp_copy);
  ui::toast_fmt("%s", toast);
  const bool was_focus = g_focus == idx;
  const bool on_screen = ui::current_screen() == screen_for(k);
  free_slot(idx);
  if (was_focus || on_screen) ui::go_hub();
}

void forfeit_tick(lv_timer_t * /*t*/) {
  const uint32_t now = mono_ms();
  for (int i = 0; i < kMaxActiveGames; ++i) {
    GameSlot & s = g_slots[i];
    if (!slot_live(s) || s.invite_pending) continue;
    if (!is_my_turn(s)) continue;
    if (s.turn_started_ms == 0) {
      s.turn_started_ms = now;
      continue;
    }
    if (now - s.turn_started_ms >= kTurnForfeitMs) auto_forfeit_slot(i);
  }
}

GameSlot & require_focus(GameKind k) {
  GameSlot * s = focused_kind(k);
  if (!s) {
    static GameSlot dummy;
    dummy = GameSlot{};
    dummy.kind = k;
    return dummy;
  }
  return *s;
}

}  // namespace

const char * slot_peer_id(const GameSlot & s) { return opp_id_of(s); }
const char * slot_peer_name(const GameSlot & s) { return opp_name_of(s); }
bool slot_is_live(const GameSlot & s) { return slot_live(s); }

uint32_t mono_ms() { return lv_tick_get(); }

void games_init() {
  if (!g_forfeit_timer) g_forfeit_timer = lv_timer_create(forfeit_tick, 5000, nullptr);
}

int active_count() {
  int n = 0;
  for (int i = 0; i < kMaxActiveGames; ++i)
    if (slot_live(g_slots[i])) ++n;
  return n;
}

int your_turn_count() {
  int n = 0;
  for (int i = 0; i < kMaxActiveGames; ++i) {
    if (!slot_live(g_slots[i]) || g_slots[i].invite_pending) continue;
    if (is_my_turn(g_slots[i])) ++n;
  }
  return n;
}

bool can_start(GameKind kind, const char * peer_id) {
  if (!peer_id || !peer_id[0]) return false;
  if (find_slot(kind, peer_id) >= 0) return false;
  return active_count() < kMaxActiveGames;
}

int find_slot(GameKind kind, const char * peer_id) {
  for (int i = 0; i < kMaxActiveGames; ++i) {
    if (!g_slots[i].used || g_slots[i].kind != kind) continue;
    if (!slot_live(g_slots[i])) continue;
    if (same(opp_id_of(g_slots[i]), peer_id)) return i;
  }
  return -1;
}

int alloc_slot(GameKind kind) {
  if (active_count() >= kMaxActiveGames) return -1;
  for (int i = 0; i < kMaxActiveGames; ++i) {
    if (g_slots[i].used && slot_live(g_slots[i])) continue;
    g_slots[i] = GameSlot{};
    g_slots[i].used = true;
    g_slots[i].kind = kind;
    g_slots[i].turn_started_ms = mono_ms();
    return i;
  }
  return -1;
}

void free_slot(int idx) {
  if (idx < 0 || idx >= kMaxActiveGames) return;
  g_slots[idx] = GameSlot{};
  if (g_focus == idx) g_focus = -1;
}

void clear_all_games() {
  for (int i = 0; i < kMaxActiveGames; ++i) g_slots[i] = GameSlot{};
  g_focus = -1;
}

void set_focus(int idx) { g_focus = idx; }
int focus_index() { return g_focus; }

GameSlot * slot_at(int idx) {
  if (idx < 0 || idx >= kMaxActiveGames) return nullptr;
  return &g_slots[idx];
}

GameSlot * focused() {
  if (g_focus < 0 || g_focus >= kMaxActiveGames) return nullptr;
  if (!g_slots[g_focus].used) return nullptr;
  return &g_slots[g_focus];
}

GameSlot * focused_kind(GameKind kind) {
  GameSlot * s = focused();
  if (s && s->kind == kind) return s;
  return nullptr;
}

const char * kind_name(GameKind kind) {
  switch (kind) {
    case GameKind::Ttt: return "Tic Tac Toe";
    case GameKind::Sttt: return "Super TTT";
    case GameKind::C4: return "Connect Four";
    case GameKind::Bs: return "Battleship";
    case GameKind::Ck: return "Checkers";
    case GameKind::Mem: return "Memory";
    case GameKind::Rv: return "Reversi";
    case GameKind::Db: return "Dots & Boxes";
    default: return "Game";
  }
}

bool is_my_turn(const GameSlot & s) {
  if (!s.used || s.invite_pending) return false;
  switch (s.kind) {
    case GameKind::Ttt:
      return s.g.ttt.active && !s.g.ttt.waiting && !s.g.ttt.over && s.g.ttt.turn == s.g.ttt.mark;
    case GameKind::Sttt:
      return s.g.sttt.active && !s.g.sttt.waiting && !s.g.sttt.over && s.g.sttt.turn == s.g.sttt.mark;
    case GameKind::C4:
      return s.g.c4.active && !s.g.c4.waiting && !s.g.c4.over && s.g.c4.turn == s.g.c4.my_color;
    case GameKind::Bs:
      return s.g.bs.active && !s.g.bs.waiting && !s.g.bs.over &&
             (s.g.bs.setup ? !s.g.bs.me_ready : s.g.bs.my_turn);
    case GameKind::Ck:
      return s.g.ck.active && !s.g.ck.waiting && !s.g.ck.over && s.g.ck.turn == s.g.ck.side;
    case GameKind::Mem:
      return s.g.mem.active && !s.g.mem.waiting && !s.g.mem.over && s.g.mem.my_turn && !s.g.mem.lock;
    case GameKind::Rv:
      return s.g.rv.active && !s.g.rv.waiting && !s.g.rv.over && s.g.rv.turn == s.g.rv.my_color;
    case GameKind::Db:
      return s.g.db.active && !s.g.db.waiting && !s.g.db.over && s.g.db.turn == s.g.db.my_side;
    default: return false;
  }
}

void note_turn_start(int idx) {
  if (idx < 0 || idx >= kMaxActiveGames) return;
  g_slots[idx].turn_started_ms = mono_ms();
}

bool is_viewing(GameKind kind, const char * peer_id) {
  if (ui::current_screen() != screen_for(kind)) return false;
  GameSlot * s = focused_kind(kind);
  if (!s || !slot_live(*s)) return false;
  return same(opp_id_of(*s), peer_id);
}

void notify_your_turn(GameKind kind, const char * opp_name, const char * peer_id) {
  if (is_viewing(kind, peer_id)) return;
  char toast[96];
  std::snprintf(toast, sizeof(toast), "%s vs %s: Your Turn", kind_name(kind),
                opp_name ? opp_name : "peer");
  ui::toast_fmt("%s", toast);
}

void refresh_viewing(GameKind kind, const char * peer_id) {
  if (is_viewing(kind, peer_id)) go_kind(kind);
  /* Keep Active Games rows (Invite sent → Your turn) in sync. */
  if (ui::current_screen() == ui::Screen::ActiveGames) ui::go_active_games();
}

void open_slot(int idx) {
  if (idx < 0 || idx >= kMaxActiveGames || !slot_live(g_slots[idx])) return;
  set_focus(idx);
  go_kind(g_slots[idx].kind);
}

bool has_focused(GameKind kind) {
  GameSlot * s = focused_kind(kind);
  return s && (slot_live(*s) || (!s->invite_pending && s->used));
}

bool invite_active(GameKind kind) {
  GameSlot * s = focused_kind(kind);
  return s && s->invite_pending && s->invite.active;
}

Invite & invite_ref(GameKind kind) { return require_focus(kind).invite; }

TttGame & ttt() { return require_focus(GameKind::Ttt).g.ttt; }
StttGame & sttt() { return require_focus(GameKind::Sttt).g.sttt; }
C4Game & c4() { return require_focus(GameKind::C4).g.c4; }
BsGame & bs() { return require_focus(GameKind::Bs).g.bs; }
CkGame & ck() { return require_focus(GameKind::Ck).g.ck; }
MemGame & mem() { return require_focus(GameKind::Mem).g.mem; }
RvGame & rv() { return require_focus(GameKind::Rv).g.rv; }
DbGame & db() { return require_focus(GameKind::Db).g.db; }

bool begin_match(GameKind kind, const char * peer_id) {
  if (!can_start(kind, peer_id)) return false;
  const int idx = alloc_slot(kind);
  if (idx < 0) return false;
  set_focus(idx);
  g_slots[idx].invite_pending = false;
  return true;
}

void accept_invite(GameKind kind) {
  GameSlot * s = focused_kind(kind);
  if (!s || !s->invite_pending) return;
  s->invite_pending = false;
  s->invite = Invite{};
  s->turn_started_ms = mono_ms();
}

void end_focused() {
  if (g_focus >= 0) free_slot(g_focus);
}

bool is_outgoing_wait(const GameSlot & s) { return outgoing_wait(s); }

bool cancel_slot(int idx) {
  if (idx < 0 || idx >= kMaxActiveGames || !slot_live(g_slots[idx])) return false;
  GameSlot & s = g_slots[idx];
  const GameKind k = s.kind;
  char oid[proto::kMaxId];
  copy_str(oid, sizeof(oid), opp_id_of(s));
  const bool was_focus = g_focus == idx;
  const bool on_board = ui::current_screen() == screen_for(k);
  const bool on_active = ui::current_screen() == ui::Screen::ActiveGames;

  if (s.invite_pending) {
    send_decline(k, oid);
  } else if (outgoing_wait(s)) {
    send_forfeit(k, oid);
  } else {
    return false; /* in-play cancels use forfeit confirm on the board */
  }

  free_slot(idx);
  if (on_active) ui::go_active_games();
  else if (was_focus || on_board) ui::go_hub();
  return true;
}

int list_sorted(int * out_indices, int max_out) {
  if (!out_indices || max_out <= 0) return 0;
  int tmp[kMaxActiveGames];
  int n = 0;
  for (int i = 0; i < kMaxActiveGames; ++i)
    if (slot_live(g_slots[i])) tmp[n++] = i;

  /* your-turn → pending invites (in or out) → everything else */
  auto rank = [](const GameSlot & s) {
    if (is_my_turn(s)) return 0;
    if (s.invite_pending || outgoing_wait(s)) return 1;
    return 2;
  };
  for (int a = 0; a < n; ++a) {
    for (int b = a + 1; b < n; ++b) {
      if (rank(g_slots[tmp[b]]) < rank(g_slots[tmp[a]])) {
        const int sw = tmp[a];
        tmp[a] = tmp[b];
        tmp[b] = sw;
      }
    }
  }
  const int out_n = n < max_out ? n : max_out;
  for (int i = 0; i < out_n; ++i) out_indices[i] = tmp[i];
  return out_n;
}

}  // namespace app
}  // namespace wp
