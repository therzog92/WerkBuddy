#pragma once

#include "protocol/messages.h"

#include <cstdint>

namespace wp {
namespace score_log {

constexpr int kMaxEntries = 80;
constexpr int kMaxPeer = proto::kMaxName;
constexpr int kMaxGame = 24;

enum class Outcome : uint8_t {
  Win = 0,
  Lose = 1,
  Tie = 2,
  ForfeitSelf = 3, /* "You Forfeited" */
  ForfeitOpp = 4,  /* "Opp. Forfeited" */
};

struct Entry {
  char game[kMaxGame] = {};
  char peer_name[kMaxPeer] = {};
  Outcome outcome = Outcome::Win;
  int64_t stamp = 0; /* YYYYMMDDHHMMSS local wall */
};

void init();
/** Dedupes identical game/peer/outcome for a short window (UI rebuild spam). */
void note(const char * game, const char * peer_name, Outcome outcome);
void clear();
int count();
const Entry * at(int newest_index);
const char * outcome_label(Outcome o);
void save();

}  // namespace score_log
}  // namespace wp
