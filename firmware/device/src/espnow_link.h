#pragma once

#include <cstdint>

namespace wp {
namespace net {

struct RxMsg {
  enum class Kind {
    Discover,
    DiscoverReply,
    Call,
    Ack,
    Clear,
    TttInvite,
    TttAccept,
    TttDecline,
    TttMove,
    TttForfeit
  } kind;
  char from_id[16] = {};
  char from_name[13] = {};
  char to_id[16] = {};
  char emoji[9] = {};
  char message[23] = {};
  int8_t cell = -1;
  char mark = 0;
};

using MsgHandler = void (*)(const RxMsg & msg);

void link_init(MsgHandler on_msg);
void link_send_discover();
void link_send_call(const char * to_id, const char * emoji, const char * message);
void link_send_ack(const char * to_id);
void link_send_clear(const char * to_id);
void link_send_ttt(const char * to_id, int type /* pack::Type */);
void link_send_ttt_move(const char * to_id, int8_t cell, char mark);
void link_poll();
const char * own_mac_id();
const char * own_mac_pretty();

}  // namespace net
}  // namespace wp
