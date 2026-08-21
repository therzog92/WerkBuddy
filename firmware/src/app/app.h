#pragma once

/*
 * App state — one "desk" (mirrors web/app.js `Desk`). UI reads this; the link
 * layer mutates it through handle_msg(). Keep flows identical to the web sim.
 */

#include "games/battleship.h"
#include "games/checkers.h"
#include "games/c4.h"
#include "games/dots.h"
#include "games/memory.h"
#include "games/reversi.h"
#include "protocol/messages.h"

#include <cstdint>
#include <ctime>

namespace wp {
namespace app {

constexpr int kMaxPeers = 8;
constexpr int kEmojiSlots = 7; /* compose shows these + a full-palette picker */
constexpr int kCannedCount = 4;
/** Build default shown in Updates UI; bump when shipping a Release. */
constexpr const char * kFirmwareVersion = "0.77";
/** Runtime version (sim OTA can change this; empty desk field → kFirmwareVersion). */
const char * firmware_version();
/** Apply a release tag (leading v stripped). Persists. Sim-only until device OTA. */
void set_firmware_version(const char * tag);

struct Peer {
  char id[proto::kMaxId] = {};
  char name[proto::kMaxName] = {};
};

struct IncomingCall {
  bool active = false;
  uint32_t started_ms = 0;
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
  bool first = false; /* challenger is the first player */
  char from_id[proto::kMaxId] = {};
  char from_name[proto::kMaxName] = {};
  int8_t color = -1;  /* c4 */
  uint32_t seed = 0;  /* memory */
};

/** Coin flip for who goes first when we are the challenger. */
bool roll_first();

struct TttGame {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  char mark = 'X', turn = 'X';
  char board[9] = {};
};

/** Super / Ultimate Tic Tac Toe */
struct StttGame {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  char mark = 'X', turn = 'X';
  char boards[9][9] = {};
  char meta[9] = {}; /* 0 open, X/O won, D draw */
  int8_t next_board = -1; /* -1 free choice */
};

struct C4Game {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  bool first = false; /* challenger: we drop first */
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

struct RvGame {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  int8_t my_color = games::rv::kBlack, turn = games::rv::kBlack;
  int8_t board[games::rv::kN][games::rv::kN] = {};
};

struct DbGame {
  bool active = false, waiting = false, over = false, result_dismissed = false;
  char opp_id[proto::kMaxId] = {};
  char opp_name[proto::kMaxName] = {};
  int8_t my_side = games::db::kP1, turn = games::db::kP1;
  games::db::State state;
};

struct Desk {
  char id[proto::kMaxId] = "mac-tommy";
  char name[proto::kMaxName] = {}; /* empty until setup / NVS load */
  uint8_t theme = 0;
  uint8_t bg_preset = 0; /* background::Preset — used when no custom photo */
  uint8_t timeout_id = 2;  /* 0=1m 1=3m 2=5m 3=10m 4=off — default 5m */
  uint8_t idle_mode = 1;   /* 0=black 1=clock */
  uint8_t brightness = 85; /* 10..100; pages force full */
  /**
   * Pager incoming-flash speed: 0=Relaxed 1=Classic 2=Strobe (CRT-like).
   * Stored as an index; map to ms via flash_specs().
   */
  uint8_t flash_id = 1;
  /** 0 = normal, 1 = rotate UI+touch 180° (stand flipped). */
  uint8_t rotate_180 = 0;
  /** 0 = 12-hour, 1 = 24-hour (Settings → Display, or tap Hub clock). */
  uint8_t clock_24h = 0;
  /** Hide chrome bits: hub clock, hub name, idle clock, idle name. 0 = show all. */
  static constexpr uint8_t kHideHubClock = 1 << 0;
  static constexpr uint8_t kHideHubName = 1 << 1;
  static constexpr uint8_t kHideIdleClock = 1 << 2;
  static constexpr uint8_t kHideIdleName = 1 << 3;
  uint8_t chrome_hide = 0;
  /** 1 = do not disturb — blocks incoming pages/games/doodle; still beacons presence. */
  uint8_t dnd = 0;
  bool setup_done = false; /* false → first-run / post-reset setup */
  int64_t clock_offset_ms = 0;
  /** Last known UTC wall seconds (NVS); restored into RTC after power loss. */
  uint32_t wall_epoch = 0;
  /** When this clock lineage was last set (UTC sec). Newer peer wins. */
  uint32_t clock_sync_gen = 0;
  char wifi_ssid[33] = {}; /* saved STA network; empty = none (≠ associated) */
  char wifi_pass[65] = {}; /* WPA password; empty OK for open networks */
  bool wifi_connected = false; /* ephemeral: true only while Sync/OTA job holds STA */
  char emojis[kEmojiSlots][proto::kMaxEmoji] = {};
  char canned[kCannedCount][proto::kMaxMessage] = {};

  Peer peers[kMaxPeers];
  int peer_count = 0;
  Peer nearby[kMaxPeers]; /* discover results (this session) */
  int nearby_count = 0;

  IncomingCall incoming;
  OutgoingCall outgoing;

  /* Active matches live in active_games registry (cap 24, keyed by kind+peer). */

  char doodle_peer_id[proto::kMaxId] = {};
  char doodle_peer_name[proto::kMaxName] = {};
  bool doodle_unread = false; /* remote strokes since last opening Doodle */

  int high_score_2048 = 0; /* solo 2048 best score */
  char fw_version[24] = {}; /* installed tag body; empty → kFirmwareVersion */
};

Desk & desk();

/** Load settings + start the link. Call once before building UI. */
void init();
/** Persist settings (name/theme/timeout/idle/brightness/clock/emojis/canned/peers). */
void save();

/** Wipe user data and restore defaults; leaves setup_done=false. */
void factory_reset();

/** True while pager call is live (blocks idle). Games do not block idle. */
bool busy();

bool peer_saved(const char * id);
void add_peer(const char * id, const char * name);
/** Update saved desk display name when a peer renames (Discover / invites). */
void touch_peer_name(const char * id, const char * name);
void remove_peer(const char * id);

/* —— time (manual / SNTP / peer TimeSync; NVS survives power loss) —— */
void local_time(std::tm * out);
/** Current wall time as UTC unix seconds (offset applied). */
uint32_t wall_unix();
void adjust_clock_minutes(int minutes);
void adjust_clock_days(int days);
/** Apply local civil time (Settings date/time Done); bumps gen + ESP-NOW sync. */
void set_clock_local(int year, int mon, int day, int hour, int min);
/** After SNTP (or sim sync): treat RTC as authoritative, bump gen, broadcast. */
void note_clock_synced();
/** Snapshot wall_epoch to NVS (periodic); does not bump sync_gen. */
void persist_wall_clock();
/** Broadcast TimeSync so peers can adopt or overwrite us. */
void broadcast_time_sync();

struct TimeoutSpec {
  const char * label;
  uint32_t ms;
};
constexpr int kTimeoutCount = 5;
const TimeoutSpec * timeout_specs(); /* 1m / 3m / 5m / 10m / Off */

/* Pager incoming-flash speed options. */
constexpr int kFlashCount = 3;
const TimeoutSpec * flash_specs(); /* Relaxed / Classic / Strobe */
/** Half-cycle ms for the incoming wash at the current flash_id (Strobe = fast). */
uint32_t flash_ms();

/** Send a message through the link (sim bot now, ESP-NOW later). */
void send(const proto::Msg & msg);
/** Incoming message from the link — the web `bus` handler equivalent. */
void handle_msg(const proto::Msg & msg);

/** One-shot delayed call helper (wraps lv_timer). */
void schedule(uint32_t delay_ms, void (*fn)(void *), void * user_data);

}  // namespace app
}  // namespace wp
