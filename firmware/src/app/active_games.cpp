#include "app/active_games.h"

#include "app/app.h"
#include "app/score_log.h"
#include "app/storage.h"
#include "net/link.h"
#include "protocol/messages.h"
#include "ui/chrome.h"
#include "ui/nav.h"
#include "ui/scr_hub.h"

#include "lvgl/lvgl.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace app {
namespace {

GameSlot g_slots[kMaxActiveGames];
int g_focus = -1;
lv_timer_t * g_forfeit_timer = nullptr;
bool g_persist_dirty = false;
bool g_persist_scheduled = false;

/* Last outbound game packet per slot — RAM only (not in the NVS blob). */
proto::Msg g_last_out[kMaxActiveGames];
bool g_last_out_ok[kMaxActiveGames] = {};
uint32_t g_last_out_ms[kMaxActiveGames] = {};
constexpr uint32_t kResyncMs = 3000;

constexpr uint32_t kGamesMagic = 0x31414757u; /* WGA1 */
constexpr uint16_t kGamesVersion = 2;

#pragma pack(push, 1)
struct GamesHdr {
  uint32_t magic;
  uint16_t version;
  int16_t focus;
  uint16_t count;
  uint16_t _pad;
};
struct GamesRec {
  uint32_t turn_elapsed_ms;
  uint8_t raw[sizeof(GameSlot)];
};
#pragma pack(pop)

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
    case GameKind::Wordle: return s.g.wordle.opp_id;
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
    case GameKind::Wordle: return s.g.wordle.opp_name;
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
    case GameKind::Wordle: return s.g.wordle.active;
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
    case GameKind::Wordle: return ui::Screen::Wordle;
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
    case GameKind::Wordle: ui::go_wordle(); break;
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
    case GameKind::Wordle: m.type = proto::MsgType::WordleForfeit; break;
    default: return;
  }
  fill_from(m);
  copy_str(m.to_id, sizeof(m.to_id), to_id);
  send(m);
}

GameKind kind_of_out(proto::MsgType t) {
  using T = proto::MsgType;
  switch (t) {
    case T::TttInvite:
    case T::TttAccept:
    case T::TttMove: return GameKind::Ttt;
    case T::StttInvite:
    case T::StttAccept:
    case T::StttMove: return GameKind::Sttt;
    case T::C4Invite:
    case T::C4Accept:
    case T::C4Drop: return GameKind::C4;
    case T::BsInvite:
    case T::BsAccept:
    case T::BsReady:
    case T::BsFire:
    case T::BsResult: return GameKind::Bs;
    case T::CkInvite:
    case T::CkAccept:
    case T::CkMove: return GameKind::Ck;
    case T::MemInvite:
    case T::MemAccept:
    case T::MemFlip: return GameKind::Mem;
    case T::RvInvite:
    case T::RvAccept:
    case T::RvMove: return GameKind::Rv;
    case T::DbInvite:
    case T::DbAccept:
    case T::DbLine: return GameKind::Db;
    case T::WordleInvite:
    case T::WordleAccept:
    case T::WordleWord:
    case T::WordleResult: return GameKind::Wordle;
    default: return GameKind::Count;
  }
}

bool is_cached_out(proto::MsgType t) { return kind_of_out(t) != GameKind::Count; }

void clear_last_out(int idx) {
  if (idx < 0 || idx >= kMaxActiveGames) return;
  g_last_out[idx] = proto::Msg{};
  g_last_out_ok[idx] = false;
  g_last_out_ms[idx] = 0;
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
    case GameKind::Wordle: m.type = proto::MsgType::WordleDecline; break;
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
    case GameKind::Wordle: return s.g.wordle.active && s.g.wordle.waiting;
    default: return false;
  }
}

bool is_invite_out(proto::MsgType t) {
  using T = proto::MsgType;
  return t == T::TttInvite || t == T::StttInvite || t == T::C4Invite || t == T::BsInvite ||
         t == T::CkInvite || t == T::MemInvite || t == T::RvInvite || t == T::DbInvite ||
         t == T::WordleInvite;
}

void resync_live_games(uint32_t now) {
  for (int i = 0; i < kMaxActiveGames; ++i) {
    if (!g_last_out_ok[i]) continue;
    GameSlot & s = g_slots[i];
    if (!slot_live(s) || s.invite_pending) continue;
    if (now - g_last_out_ms[i] < kResyncMs) continue;
    /* Keep last move/result even after local game-over — peer may have missed
     * the winning packet. Stop invite retries once they have accepted. */
    if (is_invite_out(g_last_out[i].type) && !outgoing_wait(s)) continue;
    send(g_last_out[i]);
  }
}

void persist_now();

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

bool on_game_board_ui() {
  using S = ui::Screen;
  const S s = ui::current_screen();
  return s == S::Ttt || s == S::Sttt || s == S::C4 || s == S::Bs || s == S::Ck || s == S::Mem ||
         s == S::Rv || s == S::Db || s == S::Wordle;
}

void schedule_persist() {
  if (g_persist_scheduled) return;
  g_persist_scheduled = true;
  schedule(400, [](void * /*ud*/) {
    g_persist_scheduled = false;
    if (g_persist_dirty && !on_game_board_ui()) persist_now();
  }, nullptr);
}

void forfeit_tick(lv_timer_t * /*t*/) {
  /* NVS writes stall the RGB panel — defer so Hub/Idle paints settle first. */
  if (g_persist_dirty && !on_game_board_ui()) schedule_persist();
  const uint32_t now = mono_ms();
  resync_live_games(now);
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

Invite & invite_dummy() {
  static Invite d;
  d = {};
  return d;
}

template <typename T>
T & game_dummy() {
  static T d;
  d = {};
  return d;
}

void persist_now() {
  alignas(4) static uint8_t buf[sizeof(GamesHdr) + sizeof(GamesRec) * kMaxActiveGames];
  GamesHdr hdr{};
  hdr.magic = kGamesMagic;
  hdr.version = kGamesVersion;
  hdr.focus = -1;
  hdr.count = 0;

  auto * recs = reinterpret_cast<GamesRec *>(buf + sizeof(GamesHdr));
  const uint32_t now = mono_ms();
  int packed_focus = -1;
  for (int i = 0; i < kMaxActiveGames; ++i) {
    if (!slot_live(g_slots[i])) continue;
    if (i == g_focus) packed_focus = (int)hdr.count;
    GamesRec & r = recs[hdr.count++];
    uint32_t elapsed = 0;
    if (g_slots[i].turn_started_ms != 0 && now >= g_slots[i].turn_started_ms)
      elapsed = now - g_slots[i].turn_started_ms;
    r.turn_elapsed_ms = elapsed;
    std::memcpy(r.raw, &g_slots[i], sizeof(GameSlot));
  }
  hdr.focus = (int16_t)packed_focus;
  std::memcpy(buf, &hdr, sizeof(hdr));
  const size_t len = sizeof(GamesHdr) + sizeof(GamesRec) * hdr.count;
  storage::save_games_blob(buf, len);
  g_persist_dirty = false;
}

bool restore_now() {
  alignas(4) static uint8_t buf[sizeof(GamesHdr) + sizeof(GamesRec) * kMaxActiveGames];
  size_t len = sizeof(buf);
  if (!storage::load_games_blob(buf, &len)) return false;
  if (len < sizeof(GamesHdr)) return false;
  GamesHdr hdr{};
  std::memcpy(&hdr, buf, sizeof(hdr));
  if (hdr.magic != kGamesMagic || hdr.version != kGamesVersion) return false;
  if (hdr.count > kMaxActiveGames) return false;
  if (len < sizeof(GamesHdr) + sizeof(GamesRec) * hdr.count) return false;

  auto * recs = reinterpret_cast<GamesRec *>(buf + sizeof(GamesHdr));
  GameKind focus_kind = GameKind::Count;
  char focus_peer[proto::kMaxId] = {};
  /* hdr.focus is an index into the packed live-slot list we wrote. */
  if (hdr.focus >= 0 && hdr.focus < (int)hdr.count) {
    GameSlot tmp{};
    std::memcpy(&tmp, recs[hdr.focus].raw, sizeof(GameSlot));
    if (tmp.used) {
      focus_kind = tmp.kind;
      copy_str(focus_peer, sizeof(focus_peer), opp_id_of(tmp));
    }
  }

  for (int i = 0; i < kMaxActiveGames; ++i) {
    g_slots[i] = GameSlot{};
    clear_last_out(i);
  }
  g_focus = -1;

  const uint32_t now = mono_ms();
  int placed = 0;
  for (uint16_t i = 0; i < hdr.count && placed < kMaxActiveGames; ++i) {
    GameSlot s{};
    std::memcpy(&s, recs[i].raw, sizeof(GameSlot));
    if (!s.used) continue;
    s.turn_started_ms = now - recs[i].turn_elapsed_ms;
    g_slots[placed] = s;
    if (focus_kind != GameKind::Count && s.kind == focus_kind &&
        same(opp_id_of(s), focus_peer))
      g_focus = placed;
    ++placed;
  }
  if (g_focus < 0 && placed > 0) g_focus = 0;
  g_persist_dirty = false;
  return placed > 0;
}

}  // namespace

const char * slot_peer_id(const GameSlot & s) { return opp_id_of(s); }
const char * slot_peer_name(const GameSlot & s) { return opp_name_of(s); }
bool slot_is_live(const GameSlot & s) { return slot_live(s); }

bool slot_is_over(const GameSlot & s) {
  if (!slot_live(s) || s.invite_pending) return false;
  switch (s.kind) {
    case GameKind::Ttt: return s.g.ttt.over;
    case GameKind::Sttt: return s.g.sttt.over;
    case GameKind::C4: return s.g.c4.over;
    case GameKind::Bs: return s.g.bs.over;
    case GameKind::Ck: return s.g.ck.over;
    case GameKind::Mem: return s.g.mem.over;
    case GameKind::Rv: return s.g.rv.over;
    case GameKind::Db: return s.g.db.over;
    case GameKind::Wordle:
      if (s.g.wordle.race) return s.g.wordle.over;
      return (s.g.wordle.i_won || s.g.wordle.i_lost) && (s.g.wordle.opp_won || s.g.wordle.opp_lost);
    default: return false;
  }
}

uint32_t mono_ms() { return lv_tick_get(); }

void games_mark_dirty() { g_persist_dirty = true; }

void games_persist() { persist_now(); }

void games_persist_soon() {
  g_persist_dirty = true;
  schedule_persist();
}

bool games_restore() { return restore_now(); }

void games_probe_peers() {
  for (int i = 0; i < kMaxActiveGames; ++i) {
    if (!slot_live(g_slots[i])) continue;
    const char * oid = opp_id_of(g_slots[i]);
    if (!oid || !oid[0]) continue;
    proto::Msg m;
    m.type = proto::MsgType::GameProbe;
    fill_from(m);
    copy_str(m.to_id, sizeof(m.to_id), oid);
    m.cell = (int8_t)g_slots[i].kind;
    m.hit = false;
    send(m);
  }
}

void games_init() {
  if (restore_now()) games_probe_peers();
  if (!g_forfeit_timer) g_forfeit_timer = lv_timer_create(forfeit_tick, 3000, nullptr);
}

void note_sent_game(const proto::Msg & msg) {
  if (!is_cached_out(msg.type) || !msg.to_id[0]) return;
  const GameKind k = kind_of_out(msg.type);
  const int idx = find_slot(k, msg.to_id);
  if (idx < 0) return;
  g_last_out[idx] = msg;
  g_last_out_ok[idx] = true;
  g_last_out_ms[idx] = mono_ms();
}

void send_accept(GameKind k, const char * to_id) {
  proto::Msg m;
  switch (k) {
    case GameKind::Ttt: m.type = proto::MsgType::TttAccept; break;
    case GameKind::Sttt: m.type = proto::MsgType::StttAccept; break;
    case GameKind::C4: m.type = proto::MsgType::C4Accept; break;
    case GameKind::Bs: m.type = proto::MsgType::BsAccept; break;
    case GameKind::Ck: m.type = proto::MsgType::CkAccept; break;
    case GameKind::Mem: m.type = proto::MsgType::MemAccept; break;
    case GameKind::Rv: m.type = proto::MsgType::RvAccept; break;
    case GameKind::Db: m.type = proto::MsgType::DbAccept; break;
    case GameKind::Wordle: m.type = proto::MsgType::WordleAccept; break;
    default: return;
  }
  fill_from(m);
  copy_str(m.to_id, sizeof(m.to_id), to_id);
  if (k == GameKind::C4) {
    const int idx = find_slot(k, to_id);
    GameSlot * s = slot_at(idx);
    if (s) m.color = s->g.c4.my_color;
  }
  send(m);
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

int find_live_kind(GameKind kind) {
  for (int i = 0; i < kMaxActiveGames; ++i) {
    if (g_slots[i].used && g_slots[i].kind == kind && slot_live(g_slots[i])) return i;
  }
  return -1;
}

int alloc_slot(GameKind kind) {
  if (active_count() >= kMaxActiveGames) return -1;
  for (int i = 0; i < kMaxActiveGames; ++i) {
    if (g_slots[i].used && slot_live(g_slots[i])) continue;
    g_slots[i] = GameSlot{};
    clear_last_out(i);
    g_slots[i].used = true;
    g_slots[i].kind = kind;
    g_slots[i].turn_started_ms = mono_ms();
    g_persist_dirty = true;
    return i;
  }
  return -1;
}

void free_slot(int idx) {
  if (idx < 0 || idx >= kMaxActiveGames) return;
  g_slots[idx] = GameSlot{};
  clear_last_out(idx);
  if (g_focus == idx) g_focus = -1;
  /* Immediate persist so idle/wake NVS snapshot cannot revive a dropped match. */
  persist_now();
}

void clear_all_games() {
  for (int i = 0; i < kMaxActiveGames; ++i) {
    g_slots[i] = GameSlot{};
    clear_last_out(i);
  }
  g_focus = -1;
  persist_now();
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
    case GameKind::Wordle: return "Wordle";
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
    case GameKind::Wordle:
      if (!s.g.wordle.active || s.g.wordle.over || s.g.wordle.waiting) return false;
      if (!s.g.wordle.my_word_picked) return true;
      if (s.g.wordle.opp_word_ready && !s.g.wordle.i_won && !s.g.wordle.i_lost) return true;
      return false;
    default: return false;
  }
}

void note_turn_start(int idx) {
  if (idx < 0 || idx >= kMaxActiveGames) return;
  g_slots[idx].turn_started_ms = mono_ms();
  g_persist_dirty = true;
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
  if (ui::current_screen() == ui::Screen::ActiveGames) ui::go_active_games();
  else if (ui::current_screen() == ui::Screen::Hub) ui::hub_refresh_games_chrome();
}

void refresh_viewing(GameKind kind, const char * peer_id) {
  if (is_viewing(kind, peer_id)) go_kind(kind);
  if (ui::current_screen() == ui::Screen::ActiveGames) ui::go_active_games();
  else if (ui::current_screen() == ui::Screen::Hub) ui::hub_refresh_games_chrome();
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

Invite & invite_ref(GameKind kind) {
  GameSlot * s = focused_kind(kind);
  return s ? s->invite : invite_dummy();
}

TttGame & ttt() {
  GameSlot * s = focused_kind(GameKind::Ttt);
  return s ? s->g.ttt : game_dummy<TttGame>();
}
StttGame & sttt() {
  GameSlot * s = focused_kind(GameKind::Sttt);
  return s ? s->g.sttt : game_dummy<StttGame>();
}
C4Game & c4() {
  GameSlot * s = focused_kind(GameKind::C4);
  return s ? s->g.c4 : game_dummy<C4Game>();
}
BsGame & bs() {
  GameSlot * s = focused_kind(GameKind::Bs);
  return s ? s->g.bs : game_dummy<BsGame>();
}
CkGame & ck() {
  GameSlot * s = focused_kind(GameKind::Ck);
  return s ? s->g.ck : game_dummy<CkGame>();
}
MemGame & mem() {
  GameSlot * s = focused_kind(GameKind::Mem);
  return s ? s->g.mem : game_dummy<MemGame>();
}
RvGame & rv() {
  GameSlot * s = focused_kind(GameKind::Rv);
  return s ? s->g.rv : game_dummy<RvGame>();
}
DbGame & db() {
  GameSlot * s = focused_kind(GameKind::Db);
  return s ? s->g.db : game_dummy<DbGame>();
}
WordleGame & wordle() {
  GameSlot * s = focused_kind(GameKind::Wordle);
  return s ? s->g.wordle : game_dummy<WordleGame>();
}

bool begin_match(GameKind kind, const char * peer_id) {
  if (!can_start(kind, peer_id)) return false;
  const int idx = alloc_slot(kind);
  if (idx < 0) return false;
  set_focus(idx);
  g_slots[idx].invite_pending = false;
  g_persist_dirty = true;
  return true;
}

void accept_invite(GameKind kind) {
  GameSlot * s = focused_kind(kind);
  if (!s || !s->invite_pending) return;
  s->invite_pending = false;
  s->invite = Invite{};
  s->turn_started_ms = mono_ms();
  g_persist_dirty = true;
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
