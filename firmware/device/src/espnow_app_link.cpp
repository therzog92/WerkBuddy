#include "net/link.h"

#include "app/app.h"
#include "device_net.h"
#include "msg_codec.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <cstring>

namespace wp {
namespace net {
namespace {

constexpr uint8_t kChannel = 1;
constexpr size_t kRxDepth = 12;
uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t g_mac[6] = {};
char g_mac_id[16] = {};
char g_mac_pretty[18] = {};

proto::Msg g_rx[kRxDepth];
volatile size_t g_rx_head = 0;
volatile size_t g_rx_tail = 0;
volatile size_t g_rx_count = 0;
portMUX_TYPE g_rx_mux = portMUX_INITIALIZER_UNLOCKED;

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
  portENTER_CRITICAL(&g_rx_mux);
  if (g_rx_count >= kRxDepth) {
    g_rx_head = (g_rx_head + 1) % kRxDepth;
    --g_rx_count;
  }
  g_rx[g_rx_tail] = msg;
  g_rx_tail = (g_rx_tail + 1) % kRxDepth;
  ++g_rx_count;
  portEXIT_CRITICAL(&g_rx_mux);
}

void send_discover_reply() {
  proto::Msg reply{};
  reply.type = proto::MsgType::DiscoverReply;
  std::snprintf(reply.from_id, sizeof(reply.from_id), "%s", g_mac_id);
  const char * name = app::desk().name[0] ? app::desk().name : "Desk";
  std::snprintf(reply.from_name, sizeof(reply.from_name), "%s", name);
  reply.hit = app::desk().dnd != 0;
  uint8_t frame[kDiscoverV2Size];
  const int n = pack_msg(reply, g_mac, frame, sizeof(frame));
  if (n > 0) esp_now_send(kBroadcast, frame, (size_t)n);
  /* Do NOT send TimeSync here — it raced DiscoverReply in the old 1-slot RX
   * queue and made Scan desks look empty. Boot / Sync time still broadcast. */
}

void handle_frame(const uint8_t * data, int len) {
  if (!data || len <= 0) return;
  proto::Msg msg;
  if (!unpack_msg(data, (size_t)len, &msg)) return;

  if (msg.from_id[0] && std::strcmp(msg.from_id, g_mac_id) == 0) return;

  /* Directed frames: drop if addressed to someone else. */
  if (msg.type != proto::MsgType::Discover && msg.type != proto::MsgType::DiscoverReply &&
      msg.type != proto::MsgType::Status && msg.type != proto::MsgType::TimeSync) {
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

void restore_espnow_radio() {
  WiFi.scanDelete();
  WiFi.disconnect(false /*wifioff*/, false /*erase*/);
  delay(30);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("WerkBuddy", nullptr, kChannel);
  /* Setting the channel while a scan/association is still draining is silently
   * ignored — verify and retry until the radio actually reads back channel 1 so
   * ESP-NOW can never be left stranded off-channel after a Wi-Fi job. */
  uint8_t ch = 0;
  for (int i = 0; i < 12; ++i) {
    esp_wifi_set_channel(kChannel, WIFI_SECOND_CHAN_NONE);
    delay(10);
    esp_wifi_get_channel(&ch, nullptr);
    if (ch == kChannel) break;
    delay(20);
  }
  Serial.printf("ESP-NOW radio restored ch=%u\n", (unsigned)ch);
}

void link_init() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("WerkBuddy", nullptr, kChannel);
  esp_wifi_set_channel(kChannel, WIFI_SECOND_CHAN_NONE);
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
  if (!m.from_name[0]) {
    const char * name = app::desk().name[0] ? app::desk().name : "Desk";
    std::snprintf(m.from_name, sizeof(m.from_name), "%s", name);
  }

  uint8_t frame[kEspNowMax];
  const int n = pack_msg(m, g_mac, frame, sizeof(frame));
  if (n <= 0) return;

  const bool bcast = m.type == proto::MsgType::Discover || m.type == proto::MsgType::DiscoverReply ||
                     m.type == proto::MsgType::Status || m.type == proto::MsgType::TimeSync ||
                     m.type == proto::MsgType::Clear || !m.to_id[0];

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
  for (;;) {
    proto::Msg msg;
    bool have = false;
    portENTER_CRITICAL(&g_rx_mux);
    if (g_rx_count > 0) {
      msg = g_rx[g_rx_head];
      g_rx_head = (g_rx_head + 1) % kRxDepth;
      --g_rx_count;
      have = true;
    }
    portEXIT_CRITICAL(&g_rx_mux);
    if (!have) break;
    app::handle_msg(msg);
  }
}

const char * own_mac_id() { return g_mac_id; }
const char * own_mac_pretty() { return g_mac_pretty; }

}  // namespace net
}  // namespace wp
