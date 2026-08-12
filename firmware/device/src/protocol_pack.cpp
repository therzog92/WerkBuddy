#include "protocol_pack.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace pack {
namespace {

void pad_copy(char * dst, size_t n, const char * src) {
  std::memset(dst, 0, n);
  if (src) std::snprintf(dst, n, "%s", src);
}

int pack_peer_name(Type type, const uint8_t from[6], const uint8_t to[6], const char * from_name,
                   uint8_t * out, size_t out_len) {
  if (!out || out_len < kPeerNameSize || !from || !to) return -1;
  out[0] = (uint8_t)type;
  out[1] = kVer;
  std::memcpy(out + 2, from, 6);
  std::memcpy(out + 8, to, 6);
  char name[12] = {};
  pad_copy(name, sizeof(name), from_name);
  std::memcpy(out + 14, name, 12);
  return (int)kPeerNameSize;
}

}  // namespace

int pack_discover(Type type, const uint8_t mac[6], const char * name, uint8_t * out, size_t out_len) {
  if (!out || out_len < kDiscoverSize || !mac) return -1;
  out[0] = (uint8_t)type;
  out[1] = kVer;
  std::memcpy(out + 2, mac, 6);
  char padded[12] = {};
  pad_copy(padded, sizeof(padded), name);
  std::memcpy(out + 8, padded, 12);
  return (int)kDiscoverSize;
}

int pack_call(const uint8_t from[6], const uint8_t to[6], const char * from_name, const char * emoji,
              const char * message, uint8_t * out, size_t out_len) {
  if (!out || out_len < kCallSize || !from || !to) return -1;
  out[0] = (uint8_t)Type::Call;
  out[1] = kVer;
  std::memcpy(out + 2, from, 6);
  std::memcpy(out + 8, to, 6);
  char name[12] = {}, em[8] = {}, msg[22] = {};
  pad_copy(name, sizeof(name), from_name);
  pad_copy(em, sizeof(em), emoji);
  pad_copy(msg, sizeof(msg), message);
  std::memcpy(out + 14, name, 12);
  std::memcpy(out + 26, em, 8);
  std::memcpy(out + 34, msg, 22);
  return (int)kCallSize;
}

int pack_ack_clear(Type type, const uint8_t from[6], const uint8_t to[6], const char * from_name,
                   uint8_t * out, size_t out_len) {
  if (type != Type::Ack && type != Type::Clear) return -1;
  return pack_peer_name(type, from, to, from_name, out, out_len);
}

int pack_ttt_ctrl(Type type, const uint8_t from[6], const uint8_t to[6], const char * from_name,
                  uint8_t * out, size_t out_len) {
  if (type < Type::TttInvite || type > Type::TttForfeit || type == Type::TttMove) return -1;
  return pack_peer_name(type, from, to, from_name, out, out_len);
}

int pack_ttt_move(const uint8_t from[6], const uint8_t to[6], const char * from_name, int8_t cell,
                  char mark, uint8_t * out, size_t out_len) {
  if (!out || out_len < kTttMoveSize) return -1;
  const int n = pack_peer_name(Type::TttMove, from, to, from_name, out, out_len);
  if (n < 0) return -1;
  out[26] = (uint8_t)cell;
  out[27] = (uint8_t)mark;
  return (int)kTttMoveSize;
}

bool unpack(const uint8_t * data, size_t len, Frame * out) {
  if (!data || !out || len < 2 || data[1] != kVer) return false;
  out->type = (Type)data[0];
  std::memset(out->to_mac, 0xFF, 6);
  out->emoji[0] = 0;
  out->message[0] = 0;
  out->cell = -1;
  out->mark = 0;

  if (out->type == Type::Discover || out->type == Type::DiscoverReply) {
    if (len < kDiscoverSize) return false;
    std::memcpy(out->from_mac, data + 2, 6);
    std::memcpy(out->from_name, data + 8, 12);
    out->from_name[12] = 0;
    return true;
  }
  if (out->type == Type::Call) {
    if (len < kCallSize) return false;
    std::memcpy(out->from_mac, data + 2, 6);
    std::memcpy(out->to_mac, data + 8, 6);
    std::memcpy(out->from_name, data + 14, 12);
    out->from_name[12] = 0;
    std::memcpy(out->emoji, data + 26, 8);
    out->emoji[8] = 0;
    std::memcpy(out->message, data + 34, 22);
    out->message[22] = 0;
    return true;
  }
  if (out->type == Type::Ack || out->type == Type::Clear || out->type == Type::TttInvite ||
      out->type == Type::TttAccept || out->type == Type::TttDecline ||
      out->type == Type::TttForfeit) {
    if (len < kPeerNameSize) return false;
    std::memcpy(out->from_mac, data + 2, 6);
    std::memcpy(out->to_mac, data + 8, 6);
    std::memcpy(out->from_name, data + 14, 12);
    out->from_name[12] = 0;
    return true;
  }
  if (out->type == Type::TttMove) {
    if (len < kTttMoveSize) return false;
    std::memcpy(out->from_mac, data + 2, 6);
    std::memcpy(out->to_mac, data + 8, 6);
    std::memcpy(out->from_name, data + 14, 12);
    out->from_name[12] = 0;
    out->cell = (int8_t)data[26];
    out->mark = (char)data[27];
    return true;
  }
  return false;
}

void mac_to_id(const uint8_t mac[6], char * id, size_t id_len) {
  if (!mac || !id || id_len < 13) return;
  std::snprintf(id, id_len, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
                mac[5]);
}

bool id_to_mac(const char * id, uint8_t mac[6]) {
  if (!id || !mac || std::strlen(id) != 12) return false;
  unsigned int b[6] = {};
  if (std::sscanf(id, "%02x%02x%02x%02x%02x%02x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
    return false;
  for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)b[i];
  return true;
}

}  // namespace pack
}  // namespace wp
