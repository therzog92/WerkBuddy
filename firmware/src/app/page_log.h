#pragma once

#include "protocol/messages.h"

#include <cstdint>

namespace wp {
namespace page_log {

constexpr int kMaxEntries = 20;
constexpr int kMaxPeerName = proto::kMaxName;
constexpr int kMaxEmoji = proto::kMaxEmoji;
constexpr int kMaxMessage = proto::kMaxMessage;

enum class Dir : uint8_t { In = 0, Out = 1 };

struct Entry {
  Dir dir = Dir::In;
  char peer_name[kMaxPeerName] = {};
  char emoji[kMaxEmoji] = {};
  char message[kMaxMessage] = {};
  int64_t epoch_ms = 0;
};

void init(); /* load from disk once */
void add(Dir dir, const char * peer_name, const char * emoji, const char * message);
void clear();
int count();
/** Newest-first: index 0 is most recent. */
const Entry * at(int newest_index);
void save();

}  // namespace page_log
}  // namespace wp
