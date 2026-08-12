#pragma once
/* GT911 touch — vendor map (inverted axes) as used by Guition/sand demos. */

#include "board.h"
#include <Wire.h>
#include <Touch_GT911.h>

#ifndef TOUCH_MAP_X1
#define TOUCH_MAP_X1 TFT_WIDTH
#define TOUCH_MAP_X2 0
#define TOUCH_MAP_Y1 TFT_HEIGHT
#define TOUCH_MAP_Y2 0
#endif

static Touch_GT911 ts(TP_SDA, TP_SCL, TP_INT, TP_RST,
                      max(TOUCH_MAP_X1, TOUCH_MAP_X2),
                      max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

static int touch_last_x = 0;
static int touch_last_y = 0;

static inline void touch_init() {
  Wire.begin(TP_SDA, TP_SCL);
  ts.begin();
  ts.setRotation(ROTATION_NORMAL);
}

static inline bool touch_touched() {
  ts.read();
  if (!ts.isTouched) return false;
  touch_last_x = map(ts.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, TFT_WIDTH - 1);
  touch_last_y = map(ts.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, TFT_HEIGHT - 1);
  return true;
}
