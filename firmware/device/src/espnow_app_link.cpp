#include "net/link.h"

#include "app/app.h"
#include "device_net.h"
#include "msg_codec.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#include <cstring>

namespace wp {
namespace net {
namespace {

constexpr uint8_t kChannel = 1;
uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t g_mac[6] = {};
char g_mac_id[16] = {};
char g_mac_pretty[18] = {};

struct Slot {
  bool pending = false;
  proto::Msg msg;
};
volatile bool g_rx_flag = false;
Slot g_rx;

bool mac_eq(const uint8_t * a, const uint8_t * b) { return std::memcmp(a, b, 6) == 0; }

bool mac_bcast(const uint8_t * a) {
  for (int i = 0; i < 6; ++i)
    if (a[i] != 0xFF) return false;
  return true;
}

void ensure_peer(const uint8_t mac[6]) {
  if (!mac || mac_bcast(mac)) return;
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer = {};
  std::memcpy(peer.peer_addr, mac, 6);
  peer.channel = kChannel;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  esp_now_add_peer(&peer);
}

void queue_msg(const proto::Msg & msg) {
  g_rx.msg = msg;
  g_rx.pending = true;
  g_rx_flag = true;
}

void send_discover_reply() {
  proto::Msg reply{};
  reply.type = proto::MsgType::DiscoverReply;
  std::snprintf(reply.from_id, sizeof(reply.from_id), "%s", g_mac_id);
  std::snprintf(reply.from_name, sizeof(reply.from_name), "%s", app::desk().name);
  uint8_t frame[kDiscoverSize];
  const int n = pack_msg(reply, g_mac, frame, sizeof(frame));
  if (n > 0) esp_now_send(kBroadcast, frame, (size_t)n);
  /* Also share clock so a desk coming back from power-loss can catch up. */
  app::broadcast_time_sync();
}

void handle_frame(const uint8_t * data, int len) {
  if (!data || len <= 0) return;
  proto::Msg msg;
  if (!unpack_msg(data, (size_t)len, &msg)) return;

  if (msg.from_id[0] && std::strcmp(msg.from_id, g_mac_id) == 0) return;

  /* Directed frames: drop if addressed to someone else. */
  if (msg.type != proto::MsgType::Discover && msg.type != proto::MsgType::DiscoverReply &&
      msg.type != proto::MsgType::TimeSync) {
    uint8_t to[6];
    if (id_to_mac(msg.to_id, to) && !mac_bcast(to) && !mac_eq(to, g_mac)) return;
  }

  queue_msg(msg);

  if (msg.type == proto::MsgType::Discover) send_discover_reply();
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

void link_init() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("WerkBuddy", nullptr, kChannel);
  delay(80);
  WiFi.macAddress(g_mac);
  mac_to_id(g_mac, g_mac_id, sizeof(g_mac_id));
  mac_to_pretty(g_mac, g_mac_pretty, sizeof(g_mac_pretty));

  std::snprintf(app::desk().id, sizeof(app::desk().id), "%s", g_mac_id);

  Serial.printf("ESP-NOW own MAC %s id=%s ch=%d\n", g_mac_pretty, g_mac_id, WiFi.channel());

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init FAILED");
    return;
  }
  esp_now_register_recv_cb(on_recv);

  esp_now_peer_info_t peer = {};
  std::memcpy(peer.peer_addr, kBroadcast, 6);
  peer.channel = kChannel;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  if (!esp_now_is_peer_exist(kBroadcast)) esp_now_add_peer(&peer);
}

void link_send(const proto::Msg & msg) {
  proto::Msg m = msg;
  if (!m.from_id[0]) std::snprintf(m.from_id, sizeof(m.from_id), "%s", g_mac_id);
  if (!m.from_name[0]) std::snprintf(m.from_name, sizeof(m.from_name), "%s", app::desk().name);

  uint8_t frame[kEspNowMax];
  const int n = pack_msg(m, g_mac, frame, sizeof(frame));
  if (n <= 0) return;

  const bool bcast = m.type == proto::MsgType::Discover || m.type == proto::MsgType::DiscoverReply ||
                     m.type == proto::MsgType::TimeSync || m.type == proto::MsgType::Clear ||
                     !m.to_id[0];

  if (bcast) {
    esp_now_send(kBroadcast, frame, (size_t)n);
    return;
  }

  uint8_t to[6];
  if (!id_to_mac(m.to_id, to) || mac_bcast(to)) {
    esp_now_send(kBroadcast, frame, (size_t)n);
    return;
  }
  ensure_peer(to);
  esp_now_send(to, frame, (size_t)n);
}

void link_poll() {
  if (!g_rx_flag) return;
  Slot slot = g_rx;
  g_rx.pending = false;
  g_rx_flag = false;
  if (!slot.pending) return;
  app::handle_msg(slot.msg);
}

const char * own_mac_id() { return g_mac_id; }
const char * own_mac_pretty() { return g_mac_pretty; }

}  // namespace net
}  // namespace wp
