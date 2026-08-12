#include "device_drive.h"

#if WERKPAGER_DEVICE_DRIVE

#include "app/app.h"
#include "board.h"
#include "ui/nav.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <cstdio>
#include <cstring>

namespace wp {
namespace drive {
namespace {

constexpr int kW = TFT_WIDTH;
constexpr int kH = TFT_HEIGHT;
constexpr size_t kFbBytes = (size_t)kW * kH * sizeof(uint16_t);
constexpr size_t kMaxLine = 160;
constexpr size_t kQueueMax = 32;

struct Sample {
  int16_t x;
  int16_t y;
  bool pressed;
  uint32_t hold_ms;
};

Sample make_sample(int16_t x, int16_t y, bool pressed, uint32_t hold_ms) {
  Sample s;
  s.x = x;
  s.y = y;
  s.pressed = pressed;
  s.hold_ms = hold_ms;
  return s;
}

Sample g_queue[kQueueMax];
size_t g_q_head = 0;
size_t g_q_tail = 0;
size_t g_q_count = 0;

int16_t g_ptr_x = 0;
int16_t g_ptr_y = 0;
bool g_ptr_pressed = false;
uint32_t g_sample_until = 0;

char g_line[kMaxLine];
size_t g_line_len = 0;

bool g_have_reply = false;
char g_reply[96];

uint16_t * g_fb = nullptr;
bool g_capturing = false;
bool g_armed = false; /* host sent "arm" — optional; we accept cmds whenever USB is up */

bool q_push(const Sample & s) {
  if (g_q_count >= kQueueMax) return false;
  g_queue[g_q_tail] = s;
  g_q_tail = (g_q_tail + 1) % kQueueMax;
  ++g_q_count;
  return true;
}

bool q_empty() { return g_q_count == 0 && lv_tick_get() >= g_sample_until; }

void queue_tap(int x, int y) {
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x > kW - 1) x = kW - 1;
  if (y > kH - 1) y = kH - 1;
  q_push(make_sample((int16_t)x, (int16_t)y, true, 50));
  q_push(make_sample((int16_t)x, (int16_t)y, false, 50));
}

void queue_swipe(int x1, int y1, int x2, int y2, int ms) {
  if (ms < 80) ms = 80;
  const int steps = 8;
  q_push(make_sample((int16_t)x1, (int16_t)y1, true, (uint32_t)(ms / steps)));
  for (int i = 1; i <= steps; ++i) {
    const int x = x1 + (x2 - x1) * i / steps;
    const int y = y1 + (y2 - y1) * i / steps;
    q_push(make_sample((int16_t)x, (int16_t)y, true, (uint32_t)(ms / steps)));
  }
  q_push(make_sample((int16_t)x2, (int16_t)y2, false, 40));
}

void reply(const char * msg) {
  std::snprintf(g_reply, sizeof(g_reply), "%s", msg);
  g_have_reply = true;
}

void flush_reply() {
  if (!g_have_reply) return;
  if (!q_empty()) return;
  Serial.println(g_reply);
  g_have_reply = false;
}

void ensure_fb() {
  if (g_fb) return;
  g_fb = (uint16_t *)heap_caps_malloc(kFbBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!g_fb) g_fb = (uint16_t *)malloc(kFbBytes);
}

void do_shot(const char * /*name*/) {
  ensure_fb();
  if (!g_fb) {
    reply("FAIL no-fb");
    return;
  }
  std::memset(g_fb, 0, kFbBytes);
  g_capturing = true;
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(lv_display_get_default());
  g_capturing = false;

  Serial.printf("SHOT %d %d RGB565 %u\n", kW, kH, (unsigned)kFbBytes);
  const uint8_t * p = (const uint8_t *)g_fb;
  size_t left = kFbBytes;
  while (left) {
    const size_t n = left > 2048 ? 2048 : left;
    Serial.write(p, n);
    p += n;
    left -= n;
    /* Keep WiFi/ESP-NOW happy during long dumps. */
    yield();
  }
  Serial.println();
  Serial.println("SHOTEND");
  reply("OK");
}

void handle_line(char * line) {
  while (*line == ' ' || *line == '\t') ++line;
  if (!*line) {
    reply("OK");
    return;
  }

  char cmd[24] = {};
  if (std::sscanf(line, "%23s", cmd) != 1) {
    reply("FAIL bad-line");
    return;
  }

  if (!std::strcmp(cmd, "arm")) {
    g_armed = true;
    reply("OK armed");
    return;
  }
  if (!std::strcmp(cmd, "ping")) {
    reply("OK pong");
    return;
  }
  if (!std::strcmp(cmd, "tap")) {
    int x = 0, y = 0;
    if (std::sscanf(line, "%*s %d %d", &x, &y) != 2) {
      reply("FAIL usage: tap X Y");
      return;
    }
    queue_tap(x, y);
    reply("OK");
    return;
  }
  if (!std::strcmp(cmd, "swipe")) {
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, ms = 180;
    const int n = std::sscanf(line, "%*s %d %d %d %d %d", &x1, &y1, &x2, &y2, &ms);
    if (n < 4) {
      reply("FAIL usage: swipe X1 Y1 X2 Y2 [ms]");
      return;
    }
    queue_swipe(x1, y1, x2, y2, ms);
    reply("OK");
    return;
  }
  if (!std::strcmp(cmd, "wait")) {
    int ms = 0;
    if (std::sscanf(line, "%*s %d", &ms) != 1 || ms < 0) {
      reply("FAIL usage: wait MS");
      return;
    }
    q_push(make_sample(g_ptr_x, g_ptr_y, false, (uint32_t)ms));
    reply("OK");
    return;
  }
  if (!std::strcmp(cmd, "shot")) {
    char name[48] = {};
    std::sscanf(line, "%*s %47s", name);
    do_shot(name[0] ? name : nullptr);
    return;
  }
  if (!std::strcmp(cmd, "info")) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "OK name=%s peers=%d in=%d out=%d theme=%u",
                  app::desk().name, app::desk().peer_count, app::desk().incoming.active ? 1 : 0,
                  app::desk().outgoing.active ? 1 : 0, (unsigned)app::desk().theme);
    reply(buf);
    return;
  }
  if (!std::strcmp(cmd, "nav")) {
    char where[24] = {};
    if (std::sscanf(line, "%*s %23s", where) != 1) {
      reply("FAIL usage: nav hub|games|pager|settings|doodle|utils");
      return;
    }
    using namespace ui;
    if (!std::strcmp(where, "hub") || !std::strcmp(where, "home")) go_hub();
    else if (!std::strcmp(where, "games")) go_games_folder();
    else if (!std::strcmp(where, "pager") || !std::strcmp(where, "werk")) go_werk();
    else if (!std::strcmp(where, "settings")) go_settings();
    else if (!std::strcmp(where, "doodle")) go_doodle();
    else if (!std::strcmp(where, "utils")) go_utils_folder();
    else if (!std::strcmp(where, "ttt")) go_ttt();
    else {
      reply("FAIL unknown-nav");
      return;
    }
    reply("OK");
    return;
  }

  reply("FAIL unknown-cmd");
}

void read_serial() {
  if (g_have_reply) return;
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c == '\n') {
      g_line[g_line_len] = 0;
      g_line_len = 0;
      handle_line(g_line);
      break;
    }
    if (g_line_len + 1 < kMaxLine) g_line[g_line_len++] = (char)c;
  }
}

}  // namespace

void init(lv_display_t * /*disp*/) {
  /* Screenshot buffer (~450 KB PSRAM) allocated lazily on first `shot`. */
  Serial.println("OK drive-ready");
}

void poll() {
  read_serial();
  flush_reply();
}

bool take_pointer(lv_indev_data_t * data) {
  const uint32_t now = lv_tick_get();
  if (g_q_count > 0 && now >= g_sample_until) {
    const Sample s = g_queue[g_q_head];
    g_q_head = (g_q_head + 1) % kQueueMax;
    --g_q_count;
    g_ptr_x = s.x;
    g_ptr_y = s.y;
    g_ptr_pressed = s.pressed;
    g_sample_until = now + (s.hold_ms ? s.hold_ms : 1);
  } else if (g_q_count == 0 && now >= g_sample_until && !g_ptr_pressed) {
    /* Idle — let real touch handle it. */
    return false;
  }

  /* While a sequence is in progress (or finger held by drive), own the indev. */
  if (g_q_count > 0 || g_ptr_pressed || now < g_sample_until) {
    data->point.x = g_ptr_x;
    data->point.y = g_ptr_y;
    data->state = g_ptr_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    return true;
  }
  return false;
}

void on_flush(const lv_area_t * area, const uint8_t * px_map) {
  if (!g_capturing || !g_fb || !area || !px_map) return;
  const int32_t w = area->x2 - area->x1 + 1;
  for (int32_t y = area->y1; y <= area->y2; ++y) {
    if (y < 0 || y >= kH) continue;
    const int32_t x0 = area->x1 < 0 ? 0 : area->x1;
    const int32_t x1 = area->x2 >= kW ? kW - 1 : area->x2;
    if (x1 < x0) continue;
    uint16_t * dst = g_fb + (size_t)y * kW + (size_t)x0;
    const uint16_t * src =
        (const uint16_t *)px_map + (size_t)(y - area->y1) * (size_t)w + (size_t)(x0 - area->x1);
    std::memcpy(dst, src, (size_t)(x1 - x0 + 1) * sizeof(uint16_t));
  }
}

}  // namespace drive
}  // namespace wp

#endif /* WERKPAGER_DEVICE_DRIVE */
