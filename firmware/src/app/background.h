#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace background {

/** True when this desk has a stored wallpaper (one image slot). */
bool has();

/** Built-in gradient scenes when no custom photo. Theme = follow accent palette. */
enum class Preset : uint8_t {
  Theme = 0,
  Aurora = 1,
  Sunset = 2,
  Ocean = 3,
  Ember = 4,
  Mist = 5,
  Count = 6,
};

const char * preset_name(Preset p);
/** Swatch colors for settings chips (top / bottom of gradient). */
void preset_colors(Preset p, uint32_t * top, uint32_t * bot);
Preset preset();
void set_preset(Preset p);

/**
 * Wallpaper source for LVGL bg-image, or nullptr if none.
 * Sim: FS path string (S:...). Device: baked lv_image_dsc_t* in PSRAM (not a JPEG path —
 * streaming TJPGD every redraw is unusably slow).
 */
const void * lv_src();

/** Absolute / relative filesystem path to the PNG (for existence checks). */
const char * file_path();

/** Invalidate cache after upload / remove. */
void reload();

/** Delete the single wallpaper for this desk. */
void clear();

/**
 * Read upload-server session (sim-data/bg_upload_session.json).
 * Fills url/local_url/qr_path for desk().id. Returns false if server not running.
 * On device: starts SoftAP upload job and fills phone URL (qr_lv_src unused — use wifi_qr_payload).
 */
bool upload_session(char * url, size_t url_n, char * local_url, size_t local_n,
                    char * qr_lv_src, size_t qr_n);

/** Poll SoftAP job; returns true once when a JPEG finished uploading (not baked yet). */
bool poll_new_upload();

/** After SoftAP is stopped: bake JPEG → RGB565. Returns true if wallpaper ready. */
bool finalize_upload();

/** SoftAP job: pump DNS/HTTP (device). No-op on sim. */
void upload_poll();
/** Tear down SoftAP upload job (device). No-op on sim. */
void end_upload_job();

/** WIFI:… QR payload for SoftAP join, or nullptr if N/A. */
const char * wifi_qr_payload();
const char * softap_ssid();
const char * softap_pass();

/** Soft dark wash over wallpaper (0..255). Higher recolor = dimmer photo. */
constexpr lv_opa_t kWashHub = 155;  /* darker so hub chrome/text stays readable */
constexpr lv_opa_t kWashPage = 140; /* readable chrome, photo still visible */
constexpr lv_opa_t kWashIdle = 115; /* clock lock — a bit brighter than hub */
/** Extra: page screens also lower image opa so text stays readable. */
constexpr lv_opa_t kImageOpaHub = 200;
constexpr lv_opa_t kImageOpaPage = 95;  /* ~37% photo — heavily subdued */
constexpr lv_opa_t kImageOpaIdle = 215;
}  // namespace background
}  // namespace wp
