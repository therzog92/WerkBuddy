#pragma once
/* Guition ESP32-4848S040 — pins verified against community BSPs (ha5dzs / sand1812 / aqua). */

#include <stdint.h>

#define TFT_WIDTH  480
#define TFT_HEIGHT 480
#define TFT_ROTATION 0

#define TFT_BL  38
#define TFT_CS  39
#define TFT_SCK 48
#define TFT_MOSI 47

#define TFT_DE   18
#define TFT_VSYNC 17
#define TFT_HSYNC 16
#define TFT_PCLK 21

#define TFT_R0 11
#define TFT_R1 12
#define TFT_R2 13
#define TFT_R3 14
#define TFT_R4 0

#define TFT_G0 8
#define TFT_G1 20
#define TFT_G2 3
#define TFT_G3 46
#define TFT_G4 9
#define TFT_G5 10

#define TFT_B0 4
#define TFT_B1 5
#define TFT_B2 6
#define TFT_B3 7
#define TFT_B4 15

#define TP_SDA 19
#define TP_SCL 45
#define TP_INT -1
#define TP_RST -1
