#pragma once

/*
 * WerkBuddy message protocol — firmware mirror of protocol/messages.js.
 * Keep type names in sync with the JS MessageType table (do not rename).
 *
 * Sim transport passes Msg structs directly. On ESP32 these become compact
 * binary packets (see docs/ESP32_PORT_PLAN.md §4): fixed header + payload,
 * ~250B ESP-NOW budget. Doodle strokes are already chunked to fit.
 */

#include <cstdint>

namespace wp {
namespace proto {

enum class MsgType : uint8_t {
  Discover = 0,
  DiscoverReply,
  Call,
  Ack,
  Clear,
  Status,
  TttInvite,
  TttAccept,
  TttDecline,
  TttMove,
  TttForfeit,
  C4Invite,
  C4Accept,
  C4Decline,
  C4Drop,
  C4Forfeit,
  BsInvite,
  BsAccept,
  BsDecline,
  BsReady,
  BsFire,
  BsResult,
  BsForfeit,
  CkInvite,
  CkAccept,
  CkDecline,
  CkMove,
  CkForfeit,
  MemInvite,
  MemAccept,
  MemDecline,
  MemFlip,
  MemForfeit,
  StttInvite,
  StttAccept,
  StttDecline,
  StttMove,
  StttForfeit,
  RvInvite,
  RvAccept,
  RvDecline,
  RvMove,
  RvForfeit,
  DbInvite,
  DbAccept,
  DbDecline,
  DbLine,
  DbForfeit,
  DoodleStroke,
  DoodleClear,
  /** Ask peer if they still have kind+match with us. cell = GameKind. */
  GameProbe,
  /** Reply: cell = GameKind, hit = still active on this desk. */
  GameProbeReply,
  /** Broadcast wall clock; peers adopt if sync_gen is newer. */
  TimeSync,
};

constexpr int kMaxName = 13;    /* 12 chars + NUL (web clamps to 12) */
constexpr int kMaxId = 16;      /* "mac-tommy" sim ids; MAC hex on device */
constexpr int kMaxEmoji = 8;    /* one UTF-8 emoji */
constexpr int kMaxMessage = 23; /* 22 chars + NUL (web clamps canned to 22) */
constexpr int kMaxStrokePts = 40; /* points per chunk (80 coords) — ESP-NOW budget */

/*
 * Fat struct for the sim link. Fields are per-type; unused ones stay zeroed.
 * The device build will pack only the relevant fields per type.
 */
struct Msg {
  MsgType type = MsgType::Discover;
  char from_id[kMaxId] = {};
  char from_name[kMaxName] = {};
  char to_id[kMaxId] = {};

  /* call */
  char emoji[kMaxEmoji] = {};
  char message[kMaxMessage] = {};
  /* ack */
  char for_call_from_id[kMaxId] = {};

  /* ttt / sttt — sttt uses col=mini-board 0..8, cell=cell 0..8 */
  int8_t cell = -1;
  char mark = 0; /* 'X' | 'O' */

  /* c4 — color is a palette index 0..5 */
  int8_t col = -1;
  int8_t color = -1;

  /* bs */
  int8_t x = -1;
  int8_t y = -1;
  bool hit = false;
  bool sunk = false;
  bool game_over = false;

  /* ck */
  int8_t from_x = -1, from_y = -1, to_x = -1, to_y = -1;

  /* mem — seed as u32 (device plan: u32 hash instead of string) */
  uint32_t seed = 0;
  int8_t card_a = -1, card_b = -1;

  /* doodle stroke */
  uint16_t stroke_id = 0;
  uint8_t seq = 0;
  bool last = true;
  int8_t stroke_color = 1; /* palette id, -1 = erase */
  uint8_t stroke_w = 2;    /* 1|2|3 = S/M/L */
  uint8_t n_pts = 0;       /* point count (pairs in pts) */
  uint8_t pts[kMaxStrokePts * 2] = {}; /* quantized 0..120 */

  /* time_sync — UTC unix seconds + generation (authoritative set time) */
  uint32_t unix_sec = 0;
  uint32_t sync_gen = 0;
};

}  // namespace proto
}  // namespace wp
