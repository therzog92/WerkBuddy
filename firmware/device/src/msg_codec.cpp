#include "msg_codec.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace {

void pad_copy(char * dst, size_t n, const char * src) {
  std::memset(dst, 0, n);
  if (src) std::snprintf(dst, n, "%s", src);
}

void write_u16_le(uint8_t * p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}

uint16_t read_u16_le(const uint8_t * p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

void write_u32_le(uint8_t * p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

uint32_t read_u32_le(const uint8_t * p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

bool is_peer_ctrl(proto::MsgType t) {
  using T = proto::MsgType;
  switch (t) {
    case T::Ack:
    case T::Clear:
    case T::TttAccept:
    case T::TttDecline:
    case T::TttForfeit:
    case T::C4Decline:
    case T::C4Forfeit:
    case T::BsAccept:
    case T::BsDecline:
    case T::BsReady:
    case T::BsForfeit:
    case T::CkAccept:
    case T::CkDecline:
    case T::CkForfeit:
    case T::MemAccept:
    case T::MemDecline:
    case T::MemForfeit:
    case T::StttAccept:
    case T::StttDecline:
    case T::StttForfeit:
    case T::RvAccept:
    case T::RvDecline:
    case T::RvForfeit:
    case T::DbAccept:
    case T::DbDecline:
    case T::DbForfeit:
    case T::WordleAccept:
    case T::WordleDecline:
    case T::WordleForfeit:
    case T::DoodleClear:
      return true;
    default:
      return false;
  }
}

int pack_peer_hdr(proto::MsgType type, const uint8_t from[6], const uint8_t to[6],
                  const char * from_name, uint8_t * out, size_t out_len) {
  if (!out || out_len < kPeerHdrSize || !from || !to) return -1;
  out[0] = (uint8_t)type;
  out[1] = kMsgVer;
  std::memcpy(out + 2, from, 6);
  std::memcpy(out + 8, to, 6);
  char name[12] = {};
  pad_copy(name, sizeof(name), from_name);
  std::memcpy(out + 14, name, 12);
  return (int)kPeerHdrSize;
}

bool unpack_peer_hdr(const uint8_t * data, size_t len, proto::Msg * out) {
  if (!data || !out || len < kPeerHdrSize) return false;
  out->type = (proto::MsgType)data[0];
  mac_to_id(data + 2, out->from_id, sizeof(out->from_id));
  mac_to_id(data + 8, out->to_id, sizeof(out->to_id));
  std::memcpy(out->from_name, data + 14, 12);
  out->from_name[12] = 0;
  return true;
}

}  // namespace

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

void mac_to_pretty(const uint8_t mac[6], char * out, size_t out_len) {
  if (!mac || !out || out_len < 18) return;
  std::snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
                mac[4], mac[5]);
}

int pack_msg(const proto::Msg & msg, const uint8_t own_mac[6], uint8_t * out, size_t out_len) {
  if (!out || !own_mac || out_len < 2) return -1;

  using T = proto::MsgType;
  const T type = msg.type;

  if (type == T::Discover || type == T::DiscoverReply) {
    if (out_len < kDiscoverV2Size) return -1;
    out[0] = (uint8_t)type;
    out[1] = kMsgVer;
    std::memcpy(out + 2, own_mac, 6);
    char padded[12] = {};
    pad_copy(padded, sizeof(padded), msg.from_name);
    std::memcpy(out + 8, padded, 12);
    out[20] = msg.hit ? 1 : 0;
    return (int)kDiscoverV2Size;
  }

  if (type == T::TimeSync) {
    if (out_len < kTimeSyncSize) return -1;
    out[0] = (uint8_t)type;
    out[1] = kMsgVer;
    std::memcpy(out + 2, own_mac, 6);
    write_u32_le(out + 8, msg.unix_sec);
    write_u32_le(out + 12, msg.sync_gen);
    return (int)kTimeSyncSize;
  }

  uint8_t to[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (msg.to_id[0]) id_to_mac(msg.to_id, to); /* keep broadcast if not 12-hex MAC */

  if (type == T::Status) {
    if (out_len < kStatusSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = msg.hit ? 1 : 0;
    return (int)kStatusSize;
  }

  if (type == T::Call) {
    if (out_len < kCallSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    char em[16] = {}, text[22] = {};
    pad_copy(em, sizeof(em), msg.emoji);
    pad_copy(text, sizeof(text), msg.message);
    std::memcpy(out + 26, em, 16);
    std::memcpy(out + 42, text, 22);
    return (int)kCallSize;
  }

  if (type == T::MemInvite) {
    if (out_len < kMemInviteSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    write_u32_le(out + 26, msg.seed);
    out[30] = msg.first ? 1 : 0;
    return (int)kMemInviteSize;
  }

  /* Game invites carry who goes first (challenger coin flip). */
  if (type == T::TttInvite || type == T::StttInvite || type == T::BsInvite ||
      type == T::CkInvite || type == T::RvInvite || type == T::DbInvite) {
    if (out_len < kInviteFirstSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = msg.first ? 1 : 0;
    return (int)kInviteFirstSize;
  }

  if (type == T::C4Invite) {
    if (out_len < kC4InviteSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)(msg.color >= 0 ? msg.color : 0);
    out[27] = msg.first ? 1 : 0;
    return (int)kC4InviteSize;
  }

  if (type == T::C4Accept) {
    if (out_len < kC4AcceptSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)(msg.color >= 0 ? msg.color : 0);
    return (int)kC4AcceptSize;
  }

  if (is_peer_ctrl(type)) {
    return pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len);
  }

  if (type == T::TttMove) {
    if (out_len < kTttMoveSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.cell;
    out[27] = (uint8_t)msg.mark;
    return (int)kTttMoveSize;
  }

  if (type == T::C4Drop) {
    if (out_len < kC4DropSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.col;
    out[27] = (uint8_t)msg.color;
    return (int)kC4DropSize;
  }

  if (type == T::StttMove) {
    if (out_len < kStttMoveSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.col;
    out[27] = (uint8_t)msg.cell;
    out[28] = (uint8_t)msg.mark;
    return (int)kStttMoveSize;
  }

  if (type == T::CkMove) {
    if (out_len < kCkMoveSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.from_x;
    out[27] = (uint8_t)msg.from_y;
    out[28] = (uint8_t)msg.to_x;
    out[29] = (uint8_t)msg.to_y;
    return (int)kCkMoveSize;
  }

  if (type == T::MemFlip) {
    if (out_len < kMemFlipSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.card_a;
    out[27] = (uint8_t)msg.card_b;
    return (int)kMemFlipSize;
  }

  if (type == T::BsFire) {
    if (out_len < kBsFireSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.x;
    out[27] = (uint8_t)msg.y;
    return (int)kBsFireSize;
  }

  if (type == T::BsResult) {
    if (out_len < kBsResultSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.x;
    out[27] = (uint8_t)msg.y;
    uint8_t flags = 0;
    if (msg.hit) flags |= 0x01;
    if (msg.sunk) flags |= 0x02;
    if (msg.game_over) flags |= 0x04;
    out[28] = flags;
    return (int)kBsResultSize;
  }

  if (type == T::RvMove) {
    if (out_len < kRvMoveSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.x;
    out[27] = (uint8_t)msg.y;
    return (int)kRvMoveSize;
  }

  if (type == T::GameProbe || type == T::GameProbeReply) {
    if (out_len < kGameProbeSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.cell;
    out[27] = msg.hit ? 1 : 0;
    return (int)kGameProbeSize;
  }

  if (type == T::DbLine) {
    if (out_len < kDbLineSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = (uint8_t)msg.x;
    out[27] = (uint8_t)msg.y;
    out[28] = (uint8_t)msg.col;
    return (int)kDbLineSize;
  }

  if (type == T::DoodleStroke) {
    const uint8_t n = msg.n_pts > proto::kMaxStrokePts ? (uint8_t)proto::kMaxStrokePts : msg.n_pts;
    const size_t need = kDoodleStrokeHdr + (size_t)n * 2;
    if (need > kEspNowMax || out_len < need) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    write_u16_le(out + 26, msg.stroke_id);
    out[28] = msg.seq;
    out[29] = msg.last ? 1 : 0;
    out[30] = (uint8_t)msg.stroke_color;
    out[31] = msg.stroke_w;
    out[32] = n;
    if (n) std::memcpy(out + 33, msg.pts, (size_t)n * 2);
    return (int)need;
  }

  if (type == T::WordleInvite) {
    if (out_len < kInviteFirstSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = msg.wordle_mode ? 1 : 0;
    return (int)kInviteFirstSize;
  }

  if (type == T::WordleWord) {
    if (out_len < kWordleWordSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    char w[5] = {};
    for (int i = 0; i < 5; ++i) {
      char c = msg.word[i];
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
      w[i] = c ? c : ' ';
    }
    std::memcpy(out + 26, w, 5);
    return (int)kWordleWordSize;
  }

  if (type == T::WordleResult) {
    if (out_len < kWordleResultSize) return -1;
    if (pack_peer_hdr(type, own_mac, to, msg.from_name, out, out_len) < 0) return -1;
    out[26] = msg.won ? 1 : 0;
    out[27] = msg.attempts;
    return (int)kWordleResultSize;
  }

  return -1;
}

bool unpack_msg(const uint8_t * data, size_t len, proto::Msg * out) {
  if (!data || !out || len < 2 || data[1] != kMsgVer) return false;

  proto::Msg m{};
  m.type = (proto::MsgType)data[0];
  using T = proto::MsgType;

  if (m.type == T::Discover || m.type == T::DiscoverReply) {
    if (len < kDiscoverSize) return false;
    mac_to_id(data + 2, m.from_id, sizeof(m.from_id));
    std::memcpy(m.from_name, data + 8, 12);
    m.from_name[12] = 0;
    if (len >= kDiscoverV2Size) m.hit = data[20] != 0;
    *out = m;
    return true;
  }

  if (m.type == T::Status) {
    if (len < kPeerHdrSize || !unpack_peer_hdr(data, len, &m)) return false;
    if (len >= kStatusSize) m.hit = data[26] != 0;
    *out = m;
    return true;
  }

  if (m.type == T::TimeSync) {
    if (len < kTimeSyncSize) return false;
    mac_to_id(data + 2, m.from_id, sizeof(m.from_id));
    m.unix_sec = read_u32_le(data + 8);
    m.sync_gen = read_u32_le(data + 12);
    *out = m;
    return true;
  }

  if (m.type == T::Call) {
    if (len < kCallSize || !unpack_peer_hdr(data, len, &m)) return false;
    std::memcpy(m.emoji, data + 26, 16);
    m.emoji[15] = 0;
    std::memcpy(m.message, data + 42, 22);
    m.message[22] = 0;
    *out = m;
    return true;
  }

  if (m.type == T::MemInvite) {
    if (len < kPeerHdrSize || !unpack_peer_hdr(data, len, &m)) return false;
    if (len >= kMemInviteSize) {
      m.seed = read_u32_le(data + 26);
      if (len >= kMemInviteSize) m.first = data[30] != 0;
    }
    *out = m;
    return true;
  }

  if (m.type == T::TttInvite || m.type == T::StttInvite || m.type == T::BsInvite ||
      m.type == T::CkInvite || m.type == T::RvInvite || m.type == T::DbInvite) {
    if (len < kInviteFirstSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.first = data[26] != 0;
    *out = m;
    return true;
  }

  if (m.type == T::C4Invite) {
    if (len < kC4InviteSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.color = (int8_t)data[26];
    m.first = data[27] != 0;
    *out = m;
    return true;
  }

  if (m.type == T::C4Accept) {
    if (len >= kC4AcceptSize && unpack_peer_hdr(data, len, &m)) {
      m.color = (int8_t)data[26];
      *out = m;
      return true;
    }
    /* fall through to peer-only for backward compat */
    if (len < kPeerHdrSize || !unpack_peer_hdr(data, len, &m)) return false;
    *out = m;
    return true;
  }

  if (is_peer_ctrl(m.type)) {
    if (len < kPeerHdrSize || !unpack_peer_hdr(data, len, &m)) return false;
    if (m.type == T::Ack) {
      std::snprintf(m.for_call_from_id, sizeof(m.for_call_from_id), "%s", m.to_id);
    }
    *out = m;
    return true;
  }

  if (m.type == T::TttMove) {
    if (len < kTttMoveSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.cell = (int8_t)data[26];
    m.mark = (char)data[27];
    *out = m;
    return true;
  }

  if (m.type == T::C4Drop) {
    if (len < kC4DropSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.col = (int8_t)data[26];
    m.color = (int8_t)data[27];
    *out = m;
    return true;
  }

  if (m.type == T::StttMove) {
    if (len < kStttMoveSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.col = (int8_t)data[26];
    m.cell = (int8_t)data[27];
    m.mark = (char)data[28];
    *out = m;
    return true;
  }

  if (m.type == T::CkMove) {
    if (len < kCkMoveSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.from_x = (int8_t)data[26];
    m.from_y = (int8_t)data[27];
    m.to_x = (int8_t)data[28];
    m.to_y = (int8_t)data[29];
    *out = m;
    return true;
  }

  if (m.type == T::MemFlip) {
    if (len < kMemFlipSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.card_a = (int8_t)data[26];
    m.card_b = (int8_t)data[27];
    *out = m;
    return true;
  }

  if (m.type == T::BsFire) {
    if (len < kBsFireSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.x = (int8_t)data[26];
    m.y = (int8_t)data[27];
    *out = m;
    return true;
  }

  if (m.type == T::BsResult) {
    if (len < kBsResultSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.x = (int8_t)data[26];
    m.y = (int8_t)data[27];
    const uint8_t flags = data[28];
    m.hit = (flags & 0x01) != 0;
    m.sunk = (flags & 0x02) != 0;
    m.game_over = (flags & 0x04) != 0;
    *out = m;
    return true;
  }

  if (m.type == T::RvMove) {
    if (len < kRvMoveSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.x = (int8_t)data[26];
    m.y = (int8_t)data[27];
    *out = m;
    return true;
  }

  if (m.type == T::GameProbe || m.type == T::GameProbeReply) {
    if (len < kGameProbeSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.cell = (int8_t)data[26];
    m.hit = data[27] != 0;
    *out = m;
    return true;
  }

  if (m.type == T::DbLine) {
    if (len < kDbLineSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.x = (int8_t)data[26];
    m.y = (int8_t)data[27];
    m.col = (int8_t)data[28];
    *out = m;
    return true;
  }

  if (m.type == T::DoodleStroke) {
    if (len < kDoodleStrokeHdr || !unpack_peer_hdr(data, len, &m)) return false;
    m.stroke_id = read_u16_le(data + 26);
    m.seq = data[28];
    m.last = data[29] != 0;
    m.stroke_color = (int8_t)data[30];
    m.stroke_w = data[31];
    m.n_pts = data[32];
    if (m.n_pts > proto::kMaxStrokePts) return false;
    const size_t need = kDoodleStrokeHdr + (size_t)m.n_pts * 2;
    if (len < need) return false;
    if (m.n_pts) std::memcpy(m.pts, data + 33, (size_t)m.n_pts * 2);
    *out = m;
    return true;
  }

  if (m.type == T::WordleInvite) {
    if (len < kPeerHdrSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.wordle_mode = (len >= kInviteFirstSize) ? data[26] : 1;
    *out = m;
    return true;
  }

  if (m.type == T::WordleWord) {
    if (len < kWordleWordSize || !unpack_peer_hdr(data, len, &m)) return false;
    std::memcpy(m.word, data + 26, 5);
    m.word[5] = 0;
    *out = m;
    return true;
  }

  if (m.type == T::WordleResult) {
    if (len < kWordleResultSize || !unpack_peer_hdr(data, len, &m)) return false;
    m.won = data[26] != 0;
    m.attempts = data[27];
    *out = m;
    return true;
  }

  return false;
}

}  // namespace wp
