/*
 * WerkBuddy device — shared LVGL UI (sim sources) + ESP-NOW + Guition glass.
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

#include "app/app.h"
#include "app/background.h"
#include "app/desk_timer.h"
#include "board.h"
#include "device_drive.h"
#include "device_net.h"
#include "net/link.h"
#include "st7701_init.h"
#include "touch.h"
#include "ui/brightness.h"
#include "ui/display_perf.h"
#include "ui/nav.h"
#include "ui/orient.h"
#include "ui/theme.h"

/* Shared UI + radial grads + shot flush nest deeper than the thin shell. */
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    GFX_NOT_DEFINED /* DC */, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED /* MISO */);

static Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    TFT_DE, TFT_VSYNC, TFT_HSYNC, TFT_PCLK,
    TFT_R0, TFT_R1, TFT_R2, TFT_R3, TFT_R4,
    TFT_G0, TFT_G1, TFT_G2, TFT_G3, TFT_G4, TFT_G5,
    TFT_B0, TFT_B1, TFT_B2, TFT_B3, TFT_B4,
    1 /* hsync_pol */, 10 /* hfp */, 8 /* hpw */, 50 /* hbp */,
    1 /* vsync_pol */, 10 /* vfp */, 8 /* vpw */, 20 /* vbp */);

static Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    TFT_WIDTH, TFT_HEIGHT, rgbpanel, TFT_ROTATION, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */,
    st7701_4848s040_init_operations, sizeof(st7701_4848s040_init_operations));

/* Full-screen PSRAM buffers — FULL mode so call wash pulses without strip banding. */
#define DRAW_BUF_PIXELS (TFT_WIDTH * TFT_HEIGHT)
static lv_color_t *draw_buf = nullptr;
static lv_color_t *draw_buf_b = nullptr;

static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
  const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
  /* Agent shots stay in LVGL coords; physical panel may be 180° for the stand. */
  wp::drive::on_flush(area, px_map);
  uint16_t * px = (uint16_t *)px_map;
  if (wp::app::desk().rotate_180) {
    const uint32_t n = w * h;
    for (uint32_t i = 0, j = n - 1; i < j; ++i, --j) {
      const uint16_t t = px[i];
      px[i] = px[j];
      px[j] = t;
    }
    gfx->draw16bitRGBBitmap(TFT_WIDTH - 1 - area->x2, TFT_HEIGHT - 1 - area->y2, px, w, h);
  } else {
    gfx->draw16bitRGBBitmap(area->x1, area->y1, px, w, h);
  }
  lv_display_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_t * /*indev*/, lv_indev_data_t *data) {
  if (wp::drive::take_pointer(data)) return;
  if (touch_touched()) {
    data->state = LV_INDEV_STATE_PRESSED;
    int x = touch_last_x;
    int y = touch_last_y;
    if (wp::app::desk().rotate_180) {
      x = TFT_WIDTH - 1 - x;
      y = TFT_HEIGHT - 1 - y;
    }
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static uint32_t my_tick(void) { return millis(); }

void setup() {
#if WERKPAGER_DEVICE_DRIVE
  Serial.begin(921600);
#else
  Serial.begin(115200);
#endif
  delay(200);
  Serial.println();
  Serial.println("WerkBuddy shared UI device");
  Serial.printf("RAM boot: internal free=%u KB  PSRAM free=%u KB\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));

  touch_init();
  if (!gfx->begin()) Serial.println("gfx->begin() FAILED");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  gfx->fillScreen(BLACK);

  lv_init();
  lv_tick_set_cb(my_tick);
  if (!LittleFS.begin(true)) Serial.println("LittleFS begin FAILED");

  const size_t fb_bytes = DRAW_BUF_PIXELS * sizeof(lv_color_t);
  draw_buf = (lv_color_t *)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  draw_buf_b = (lv_color_t *)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!draw_buf) {
    Serial.println("draw_buf alloc FAILED");
    return;
  }
  if (!draw_buf_b) Serial.println("draw_buf_b alloc FAILED — single FULL buffer");

  lv_display_t *disp = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
  lv_display_set_flush_cb(disp, my_disp_flush);
  /* PARTIAL: typing/OSK only redraws dirty strips. FULL while call/alarm wash. */
  lv_display_set_buffers(disp, draw_buf, draw_buf_b, fb_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  wp::ui::display_perf::bind(disp);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  /* So localtime() is Central even after reboot (SNTP fills the RTC while powered). */
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
  tzset();

  wp::app::init(); /* includes net::link_init */
  wp::desk_timer::init();
  wp::theme::set(static_cast<wp::theme::Id>(wp::app::desk().theme));
  wp::ui::brightness::init();
  wp::ui::orient::apply();
  wp::ui::idle_init();

  if (!wp::app::desk().setup_done) wp::ui::go_setup();
  else wp::ui::go_hub();

  wp::drive::init(disp);
  Serial.printf("UI ready name=%s peers=%d mac=%s\n", wp::app::desk().name,
                wp::app::desk().peer_count, wp::net::own_mac_pretty());
  Serial.printf("RAM ready: internal free=%u KB  PSRAM free=%u KB\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));
}

void loop() {
  wp::drive::poll();
  wp::net::link_poll();
  wp::background::upload_poll();
  lv_timer_handler();
  delay(5);
}
