#pragma once

/*
 * App state — one "desk" (mirrors web/app.js `Desk`). UI reads this; the link
 * layer mutates it through handle_msg(). Keep flows identical to the web sim.
 */

#include "games/battleship.h"
#include "games/checkers.h"
#include "games/c4.h"
#include "games/memory.h"
#include "protocol/messages.h"

#include <cstdint>
#include <ctime>

namespace wp {
namespace app {

constexpr int kMaxPeers = 8;
constexpr int kEmojiSlots = 8;
constexpr int kCannedCount = 4;
/** Shown in Updates UI; bump when shipping a Release. */
constexpr const char * kFirmwareVersion = "0.1.0-sim";

struct Peer {
  char id[proto::kMaxId] = {};
  char name[proto::kMaxName] = {};
};

struct IncomingCall {
  bool active = false;
  char from_id[proto::kMaxId] = {};
  char from_name[proto::kMaxName] = {};
  char emoji[proto::kMaxEmoji] = {};
  char message[proto::kMaxMessage] = {};
};

struct OutgoingCall {
  bool active = false;
  char to_id[proto::kMaxId] = {};
  char to_name[proto::kMaxName] = {};
  char emoji[proto::kMaxEmoji] = {};
  char message[proto::kMaxMessage] = {};
};

struct Invite {
  bool active = false;
  char from_id[proto::kMaxId] = {};
  char from_name[proto::kMaxName] = {};
  int8_t color = -1;  /* c4 */
  uint32_t seed = 0;  /* memory */
};

struct TttGame {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  char mark = 'X', turn = 'X';
  char board[9] = {};
};

struct C4Game {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  int8_t my_color = 0, opp_color = -1, turn = 0;
  int8_t board[games::c4::kRows][games::c4::kCols] = {};
  int8_t last_r = -1, last_c = -1;
};

struct BsGame {
  bool active = false, waiting = false;
  bool setup = true; /* false = battle */
  bool i_am_first = false, my_turn = false, me_ready = false, opp_ready = false;
  bool over = false, i_won = false, result_dismissed = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  games::bs::Fleet fleet;
  bool fleet_miss[games::bs::kGrid][games::bs::kGrid] = {};
  int8_t tracking[games::bs::kGrid][games::bs::kGrid] = {}; /* 0 unknown, 1 hit, 2 miss */
  int8_t anchor_x = -1, anchor_y = -1;
  int8_t selected_ship = -1;
  char last_msg[40] = {};
  uint8_t mode = 0; /* 0 offense, 1 defense */
};

struct CkGame {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  char side = 'r', turn = 'r';
  char board[games::ck::kSize][games::ck::kSize] = {};
  int8_t sel_x = -1, sel_y = -1;
  int8_t must_x = -1, must_y = -1; /* multi-jump continuation */
};

struct MemGame {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  bool my_turn = false, lock = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  uint32_t seed = 0;
  uint8_t deck[games::mem::kCards] = {};
  bool matched[games::mem::kCards] = {};
  int8_t flip_a = -1, flip_b = -1; /* resolving pair */
  int8_t local_flip = -1;          /* my first face-up card */
  uint8_t my_score = 0, opp_score = 0;
};

struct Desk {
  char id[proto::kMaxId] = "mac-tommy";
  char name[proto::kMaxName] = "Tommy";
  uint8_t theme = 0;
  uint8_t timeout_id = 1;  /* 0=30s 1=1m 2=5m 3=off */
  uint8_t idle_mode = 1;   /* 0=black 1=clock */
  uint8_t brightness = 100; /* 10..100; pages force full */
  int64_t clock_offset_ms = 0;
  char wifi_ssid[33] = {}; /* optional STA; empty = not configured */
  bool wifi_connected = false;
  char emojis[kEmojiSlots][proto::kMaxEmoji] = {};
  char canned[kCannedCount][proto::kMaxMessage] = {};

  Peer peers[kMaxPeers];
  int peer_count = 0;
  Peer nearby[kMaxPeers]; /* discover results (this session) */
  int nearby_count = 0;

  IncomingCall incoming;
  OutgoingCall outgoing;

  Invite ttt_invite, c4_invite, bs_invite, ck_invite, mem_invite;
  TttGame ttt;
  C4Game c4;
  BsGame bs;
  CkGame ck;
  MemGame mem;

  char doodle_peer_id[proto::kMaxId] = {};
  char doodle_peer_name[proto::kMaxName] = {};
};

Desk & desk();

/** Load settings + start the link. Call once before building UI. */
void init();
/** Persist settings (name/theme/timeout/idle/brightness/clock/emojis/canned/peers). */
void save();

/** True while any call/invite/game is live (blocks idle, like web). */
bool busy();

bool peer_saved(const char * id);
void add_peer(const char * id, const char * name);
void remove_peer(const char * id);

/* —— time (manual clock, no NTP — mirrors web clockOffsetMs) —— */
void local_time(std::tm * out);
void adjust_clock_minutes(int minutes);
void adjust_clock_days(int days);

struct TimeoutSpec {
  const char * label;
  uint32_t ms;
};
const TimeoutSpec * timeout_specs(); /* 4 entries: 30s / 1m / 5m / Off */

/** Send a message through the link (sim bot now, ESP-NOW later). */
void send(const proto::Msg & msg);
/** Incoming message from the link — the web `bus` handler equivalent. */
void handle_msg(const proto::Msg & msg);

/** One-shot delayed call helper (wraps lv_timer). */
void schedule(uint32_t delay_ms, void (*fn)(void *), void * user_data);

}  // namespace app
}  // namespace wp
