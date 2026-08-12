#pragma once

#include "app/app.h"

#include <cstdint>
#include <cstring>

namespace wp {
namespace app {

constexpr int kMaxActiveGames = 24;
/** 24h of powered-on monotonic time before the desk-to-move auto-forfeits. */
constexpr uint32_t kTurnForfeitMs = 24u * 60u * 60u * 1000u;

enum class GameKind : uint8_t { Ttt = 0, Sttt, C4, Bs, Ck, Mem, Rv, Db, Count };

struct GameSlot {
  bool used = false;
  bool invite_pending = false;
  GameKind kind = GameKind::Ttt;
  uint32_t turn_started_ms = 0;
  Invite invite{};
  union Payload {
    TttGame ttt;
    StttGame sttt;
    C4Game c4;
    BsGame bs;
    CkGame ck;
    MemGame mem;
    RvGame rv;
    DbGame db;
    Payload() { std::memset(this, 0, sizeof(*this)); }
  } g;
};

void games_init(); /* start auto-forfeit timer; restore slots from NVS/disk */
/** Mark registry dirty — flushed to NVS/disk within a few seconds or on idle. */
void games_mark_dirty();
/** Force-write live slots now (call before idle / power-sensitive moments). */
void games_persist();
/** Reload slots from last persist (wake recovery if RAM was wiped). */
bool games_restore();
/** Ask each live opponent whether they still have the match (clears stale NVS games). */
void games_probe_peers();

int active_count();
int your_turn_count();
bool can_start(GameKind kind, const char * peer_id);
int find_slot(GameKind kind, const char * peer_id); /* -1 none */
/** First live slot of this kind (-1 none). Used to restore focus after idle. */
int find_live_kind(GameKind kind);
int alloc_slot(GameKind kind);                      /* -1 full */
void free_slot(int idx);
void clear_all_games();

void set_focus(int idx);
int focus_index();
GameSlot * slot_at(int idx);
GameSlot * focused();
GameSlot * focused_kind(GameKind kind);
const char * slot_peer_id(const GameSlot & s);
const char * slot_peer_name(const GameSlot & s);
bool slot_is_live(const GameSlot & s);

const char * kind_name(GameKind kind);
bool is_my_turn(const GameSlot & s);
void note_turn_start(int idx);
uint32_t mono_ms();

/** True if UI is currently showing this kind+peer match. */
bool is_viewing(GameKind kind, const char * peer_id);
/** Toast if not viewing; always safe to call after a remote move that grants local turn. */
void notify_your_turn(GameKind kind, const char * opp_name, const char * peer_id);
/** Rebuild board if viewing this match; else leave UI alone. */
void refresh_viewing(GameKind kind, const char * peer_id);
/** Focus slot and navigate to its game screen. */
void open_slot(int idx);

/* —— Focused typed accessors (peer-pick screens may have no focus) —— */
bool has_focused(GameKind kind);
bool invite_active(GameKind kind);
Invite & invite_ref(GameKind kind);

TttGame & ttt();
StttGame & sttt();
C4Game & c4();
BsGame & bs();
CkGame & ck();
MemGame & mem();
RvGame & rv();
DbGame & db();

/** List helpers for Active Games UI: write indices sorted your-turn first. */
int list_sorted(int * out_indices, int max_out);

/** Alloc+focus a new slot for challenging `peer_id`. False if blocked. */
bool begin_match(GameKind kind, const char * peer_id);
/** Convert focused invite slot into an active match shell (clears invite_pending). */
void accept_invite(GameKind kind);
/** Drop focused invite or active match. */
void end_focused();

/** True while we challenged them and are waiting on accept. */
bool is_outgoing_wait(const GameSlot & s);
/**
 * Cancel an outgoing invite (forfeit) or decline an incoming invite.
 * Frees the slot. Returns false if idx invalid.
 */
bool cancel_slot(int idx);

}  // namespace app
}  // namespace wp
