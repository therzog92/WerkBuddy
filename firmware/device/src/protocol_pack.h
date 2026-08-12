#pragma once
/* Tic Tac Toe wire types — MsgType ordinals match firmware/src/protocol/messages.h */

#include <cstddef>
#include <cstdint>

namespace wp {
namespace pack {

enum class Type : uint8_t {
  Discover = 0,
  DiscoverReply = 1,
  Call = 2,
  Ack = 3,
  Clear = 4,
  Status = 5,
  TttInvite = 6,
  TttAccept = 7,
  TttDecline = 8,
  TttMove = 9,
  TttForfeit = 10,
};

constexpr uint8_t kVer = 1;
constexpr size_t kDiscoverSize = 2 + 6 + 12;
constexpr size_t kCallSize = 2 + 6 + 6 + 12 + 8 + 22;
constexpr size_t kPeerNameSize = 2 + 6 + 6 + 12; /* invite/accept/decline/forfeit/ack/clear */
constexpr size_t kTttMoveSize = kPeerNameSize + 2;
constexpr size_t kAckClearSize = kPeerNameSize;
constexpr size_t kMaxFrame = kCallSize;

struct Frame {
  Type type = Type::Discover;
  uint8_t from_mac[6] = {};
  uint8_t to_mac[6] = {};
  char from_name[13] = {};
  char emoji[9] = {};
  char message[23] = {};
  int8_t cell = -1;
  char mark = 0;
};

int pack_discover(Type type, const uint8_t mac[6], const char * name, uint8_t * out, size_t out_len);
int pack_call(const uint8_t from[6], const uint8_t to[6], const char * from_name, const char * emoji,
              const char * message, uint8_t * out, size_t out_len);
int pack_ack_clear(Type type, const uint8_t from[6], const uint8_t to[6], const char * from_name,
                   uint8_t * out, size_t out_len);
int pack_ttt_ctrl(Type type, const uint8_t from[6], const uint8_t to[6], const char * from_name,
                  uint8_t * out, size_t out_len);
int pack_ttt_move(const uint8_t from[6], const uint8_t to[6], const char * from_name, int8_t cell,
                  char mark, uint8_t * out, size_t out_len);
bool unpack(const uint8_t * data, size_t len, Frame * out);

void mac_to_id(const uint8_t mac[6], char * id, size_t id_len);
bool id_to_mac(const char * id, uint8_t mac[6]);

}  // namespace pack
}  // namespace wp
