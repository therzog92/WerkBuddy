#pragma once
/*
 * Binary ESP-NOW codec for proto::Msg (device).
 * Wire: [u8 type][u8 ver=1] + type-specific body. Discover is compact
 * (compat with thin shell); directed frames use 26B peer header.
 */

#include "protocol/messages.h"

#include <cstddef>
#include <cstdint>

namespace wp {

constexpr uint8_t kMsgVer = 1;
constexpr size_t kEspNowMax = 250;
constexpr size_t kDiscoverSize = 2 + 6 + 12;                 /* 20 */
constexpr size_t kPeerHdrSize = 2 + 6 + 6 + 12;              /* 26 */
constexpr size_t kCallSize = kPeerHdrSize + 16 + 22;        /* 64 — emoji room for ZWJ */
constexpr size_t kTttMoveSize = kPeerHdrSize + 2;           /* 28 */
constexpr size_t kC4DropSize = kPeerHdrSize + 2;            /* 28 */
constexpr size_t kStttMoveSize = kPeerHdrSize + 3;          /* 29 */
constexpr size_t kCkMoveSize = kPeerHdrSize + 4;            /* 30 */
constexpr size_t kMemInviteSize = kPeerHdrSize + 4;         /* 30 seed */
constexpr size_t kMemFlipSize = kPeerHdrSize + 2;           /* 28 */
constexpr size_t kBsFireSize = kPeerHdrSize + 2;            /* 28 */
constexpr size_t kBsResultSize = kPeerHdrSize + 3;          /* 29 */
constexpr size_t kRvMoveSize = kPeerHdrSize + 2;            /* 28 */
constexpr size_t kDbLineSize = kPeerHdrSize + 3;            /* 29 */
constexpr size_t kGameProbeSize = kPeerHdrSize + 2;         /* kind + exists */
constexpr size_t kTimeSyncSize = 2 + 6 + 4 + 4;              /* mac + unix + gen */
constexpr size_t kDoodleStrokeHdr = kPeerHdrSize + 7;       /* + pts */

void mac_to_id(const uint8_t mac[6], char * id, size_t id_len);
bool id_to_mac(const char * id, uint8_t mac[6]);
void mac_to_pretty(const uint8_t mac[6], char * out, size_t out_len);

/** Pack msg into out. Returns byte length, or -1 on error / overflow. */
int pack_msg(const proto::Msg & msg, const uint8_t own_mac[6], uint8_t * out, size_t out_len);

/** Unpack wire bytes into out. Returns false if truncated / unknown / bad ver. */
bool unpack_msg(const uint8_t * data, size_t len, proto::Msg * out);

}  // namespace wp
