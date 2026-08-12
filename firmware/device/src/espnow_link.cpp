#include "espnow_link.h"

#include "desk.h"
#include "protocol_pack.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <cstring>

namespace wp {
namespace net {
namespace {

constexpr uint8_t kChannel = 1;
uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t g_mac[6] = {};
char g_mac_id[16] = {};
char g_mac_pretty[18] = {};
MsgHandler g_handler = nullptr;

struct Slot {
  bool pending = false;
  RxMsg msg;
};
volatile bool g_rx_flag = false;
Slot g_rx;

void mac_pretty(const uint8_t * mac, char * out, size_t n) {
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
}

bool mac_eq(const uint8_t * a, const uint8_t * b) { return memcmp(a, b, 6) == 0; }
bool mac_bcast(const uint8_t * a) {
  for (int i = 0; i < 6; ++i)
    if (a[i] != 0xFF) return false;
  return true;
}

void handle_frame(const uint8_t * data, int len) {
  pack::Frame fr;
  if (!pack::unpack(data, (size_t)len, &fr)) return;

  char from_id[16], to_id[16];
  pack::mac_to_id(fr.from_mac, from_id, sizeof(from_id));
  pack::mac_to_id(fr.to_mac, to_id, sizeof(to_id));
  if (strcmp(from_id, g_mac_id) == 0) return;

  if (fr.type != pack::Type::Discover && fr.type != pack::Type::DiscoverReply) {
    if (!mac_bcast(fr.to_mac) && !mac_eq(fr.to_mac, g_mac)) return;
  }

  RxMsg m{};
  strncpy(m.from_id, from_id, sizeof(m.from_id) - 1);
  strncpy(m.from_name, fr.from_name, sizeof(m.from_name) - 1);
  strncpy(m.to_id, to_id, sizeof(m.to_id) - 1);
  strncpy(m.emoji, fr.emoji, sizeof(m.emoji) - 1);
  strncpy(m.message, fr.message, sizeof(m.message) - 1);
  m.cell = fr.cell;
  m.mark = fr.mark;

  switch (fr.type) {
    case pack::Type::Discover: m.kind = RxMsg::Kind::Discover; break;
    case pack::Type::DiscoverReply: m.kind = RxMsg::Kind::DiscoverReply; break;
    case pack::Type::Call: m.kind = RxMsg::Kind::Call; break;
    case pack::Type::Ack: m.kind = RxMsg::Kind::Ack; break;
    case pack::Type::Clear: m.kind = RxMsg::Kind::Clear; break;
    case pack::Type::TttInvite: m.kind = RxMsg::Kind::TttInvite; break;
    case pack::Type::TttAccept: m.kind = RxMsg::Kind::TttAccept; break;
    case pack::Type::TttDecline: m.kind = RxMsg::Kind::TttDecline; break;
    case pack::Type::TttMove: m.kind = RxMsg::Kind::TttMove; break;
    case pack::Type::TttForfeit: m.kind = RxMsg::Kind::TttForfeit; break;
    default: return;
  }

  g_rx.msg = m;
  g_rx.pending = true;
  g_rx_flag = true;

  if (fr.type == pack::Type::Discover) {
    uint8_t frame[pack::kDiscoverSize];
    const int n = pack::pack_discover(pack::Type::DiscoverReply, g_mac, shell::desk().name, frame,
                                      sizeof(frame));
    if (n > 0) esp_now_send(kBroadcast, frame, n);
  }
}

#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
void on_recv(const esp_now_recv_info_t * info, const uint8_t * data, int len) {
  (void)info;
  if (data) handle_frame(data, len);
}
#else
void on_recv(const uint8_t * mac, const uint8_t * data, int len) {
  (void)mac;
  if (data) handle_frame(data, len);
}
#endif

}  // namespace

void link_init(MsgHandler on_msg) {
  g_handler = on_msg;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("WerkBuddyShell", nullptr, kChannel);
  delay(80);
  WiFi.macAddress(g_mac);
  pack::mac_to_id(g_mac, g_mac_id, sizeof(g_mac_id));
  mac_pretty(g_mac, g_mac_pretty, sizeof(g_mac_pretty));
  Serial.printf("ESP-NOW own MAC %s id=%s ch=%d\n", g_mac_pretty, g_mac_id, WiFi.channel());

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init FAILED");
    return;
  }
  esp_now_register_recv_cb(on_recv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kBroadcast, 6);
  peer.channel = kChannel;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  if (!esp_now_is_peer_exist(kBroadcast)) esp_now_add_peer(&peer);
}

void link_send_discover() {
  uint8_t frame[pack::kDiscoverSize];
  const int n =
      pack::pack_discover(pack::Type::Discover, g_mac, shell::desk().name, frame, sizeof(frame));
  if (n > 0) esp_now_send(kBroadcast, frame, n);
}

void link_send_call(const char * to_id, const char * emoji, const char * message) {
  uint8_t to[6];
  if (!pack::id_to_mac(to_id, to)) return;
  uint8_t frame[pack::kCallSize];
  const int n =
      pack::pack_call(g_mac, to, shell::desk().name, emoji, message, frame, sizeof(frame));
  if (n > 0) {
    esp_now_send(kBroadcast, frame, n);
    Serial.printf("TX Call -> %s\n", to_id);
  }
}

void link_send_ack(const char * to_id) {
  uint8_t to[6];
  if (!pack::id_to_mac(to_id, to)) return;
  uint8_t frame[pack::kAckClearSize];
  const int n =
      pack::pack_ack_clear(pack::Type::Ack, g_mac, to, shell::desk().name, frame, sizeof(frame));
  if (n > 0) esp_now_send(kBroadcast, frame, n);
}

void link_send_clear(const char * to_id) {
  uint8_t to[6];
  if (!pack::id_to_mac(to_id, to)) return;
  uint8_t frame[pack::kAckClearSize];
  const int n =
      pack::pack_ack_clear(pack::Type::Clear, g_mac, to, shell::desk().name, frame, sizeof(frame));
  if (n > 0) esp_now_send(kBroadcast, frame, n);
}

void link_send_ttt(const char * to_id, int type) {
  uint8_t to[6];
  if (!pack::id_to_mac(to_id, to)) return;
  uint8_t frame[pack::kPeerNameSize];
  const int n = pack::pack_ttt_ctrl((pack::Type)type, g_mac, to, shell::desk().name, frame,
                                    sizeof(frame));
  if (n > 0) {
    esp_now_send(kBroadcast, frame, n);
    Serial.printf("TX TTT type=%d -> %s\n", type, to_id);
  }
}

void link_send_ttt_move(const char * to_id, int8_t cell, char mark) {
  uint8_t to[6];
  if (!pack::id_to_mac(to_id, to)) return;
  uint8_t frame[pack::kTttMoveSize];
  const int n =
      pack::pack_ttt_move(g_mac, to, shell::desk().name, cell, mark, frame, sizeof(frame));
  if (n > 0) esp_now_send(kBroadcast, frame, n);
}

void link_poll() {
  if (!g_rx_flag) return;
  Slot slot = g_rx;
  g_rx.pending = false;
  g_rx_flag = false;
  if (!slot.pending || !g_handler) return;
  g_handler(slot.msg);
}

const char * own_mac_id() { return g_mac_id; }
const char * own_mac_pretty() { return g_mac_pretty; }

}  // namespace net
}  // namespace wp
