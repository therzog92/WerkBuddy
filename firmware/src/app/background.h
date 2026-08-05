#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace background {

/** True when this desk has a stored wallpaper (one image slot). */
bool has();

/** LVGL FS path (S:...) for the PNG wallpaper, or nullptr if none. */
const char * lv_src();

/** Absolute / relative filesystem path to the PNG (for existence checks). */
const char * file_path();

/** Invalidate cache after upload / remove. */
void reload();

/** Delete the single wallpaper for this desk. */
void clear();

/**
 * Read upload-server session (sim-data/bg_upload_session.json).
 * Fills url/local_url/qr_path for desk().id. Returns false if server not running.
 */
bool upload_session(char * url, size_t url_n, char * local_url, size_t local_n,
                    char * qr_lv_src, size_t qr_n);

/** Poll stamp file; returns true once when a new upload arrives. */
bool poll_new_upload();

/** Soft dark wash over wallpaper (0..255). Higher recolor = dimmer photo. */
constexpr lv_opa_t kWashHub = 90;
constexpr lv_opa_t kWashPage = 220;
constexpr lv_opa_t kWashIdle = 150;
/** Extra: page screens also lower image opa so text stays readable. */
constexpr lv_opa_t kImageOpaHub = LV_OPA_COVER;
constexpr lv_opa_t kImageOpaPage = 95;  /* ~37% photo — heavily subdued */
constexpr lv_opa_t kImageOpaIdle = 180;
}  // namespace background
}  // namespace wp
