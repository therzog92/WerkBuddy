#pragma once
/* Slim desk state for Phase 1 shell (Session 4). */

#include <cstdint>
#include <ctime>

namespace wp {
namespace shell {

constexpr int kMaxName = 13;
constexpr int kMaxId = 16;
constexpr int kMaxPeers = 8;

struct Peer {
  char id[kMaxId] = {};
  char name[kMaxName] = {};
};

struct Desk {
  char name[kMaxName] = "Desk";
  uint8_t theme = 0;       /* 0..3 shell presets */
  uint8_t timeout_id = 2;  /* 0=1m 1=3m 2=5m 3=10m 4=off */
  uint8_t idle_mode = 1;   /* 0=black 1=clock */
  bool setup_done = false;
  int64_t clock_offset_ms = 0;
  Peer peers[kMaxPeers];
  int peer_count = 0;
  Peer nearby[kMaxPeers];
  int nearby_count = 0;

  bool outgoing_active = false;
  bool incoming_active = false;
  char call_peer_id[kMaxId] = {};
  char call_peer_name[kMaxName] = {};
  char call_emoji[9] = {};
  char call_message[23] = {};

  /* Tic Tac Toe */
  bool ttt_active = false;
  bool ttt_waiting = false;   /* outgoing invite */
  bool ttt_incoming = false;  /* incoming invite */
  bool ttt_over = false;
  bool ttt_result_dismissed = false;
  char ttt_peer_id[kMaxId] = {};
  char ttt_peer_name[kMaxName] = {};
  char ttt_mark = 'X';
  char ttt_turn = 'X';
  char ttt_board[9] = {};
};

Desk & desk();
void desk_defaults();

bool peer_saved(const char * id);
void add_peer(const char * id, const char * name);
void remove_peer(const char * id);
void clear_nearby();
void add_nearby(const char * id, const char * name);

uint32_t idle_timeout_ms();
void local_time(std::tm * out);
void set_clock_from_parts(int year, int mon, int day, int hour, int min);

const char * theme_name(uint8_t id);
uint32_t theme_bg(uint8_t id);
uint32_t theme_fg(uint8_t id);
uint32_t theme_accent(uint8_t id);
const char * timeout_label(uint8_t id);

}  // namespace shell
}  // namespace wp
