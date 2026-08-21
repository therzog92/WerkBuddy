#pragma once
/* Device GitHub Releases client — same API as firmware/src/sim/github_ota.h */

#include <cstddef>
#include <cstdint>

namespace wp {
namespace sim {
namespace github_ota {

constexpr int kMaxReleases = 16;
constexpr int kTagLen = 32;
constexpr int kNameLen = 64;
constexpr int kUrlLen = 256;

struct Release {
  char tag[kTagLen] = {};
  char name[kNameLen] = {};
  char asset_url[kUrlLen] = {};
  bool has_bin = false;
};

constexpr const char * kOwner = "therzog92";
constexpr const char * kRepo = "WerkBuddy";

/** HTTPS GET api.github.com/.../releases. Blocking. Returns count or -1. */
int fetch_releases(Release * out, int max_out, char * err, int err_cap);

/** Download .bin from browser_download_url and flash OTA slot; reboots on success. */
bool install_bin(const char * url, char * err, int err_cap);

/** 0..100 progress callback (runs on the caller thread mid-download). */
typedef void (*ProgressFn)(int pct, void * user);

/**
 * Like install_bin, but reports download progress (0..100) via cb. cb may be
 * null. Runs on the calling thread; UI should pump LVGL inside cb.
 */
bool install_bin_progress(const char * url, ProgressFn cb, void * user, char * err, int err_cap);

const char * tag_body(const char * tag);

}  // namespace github_ota
}  // namespace sim
}  // namespace wp
