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

const char * tag_body(const char * tag);

}  // namespace github_ota
}  // namespace sim
}  // namespace wp
