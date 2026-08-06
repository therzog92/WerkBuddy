#pragma once

/**
 * PC-sim GitHub Releases client for the Updates screen.
 * Device firmware will use esp_http_client against the same public API.
 */

namespace wp {
namespace sim {
namespace github_ota {

constexpr int kMaxReleases = 16;
constexpr int kTagLen = 32;
constexpr int kNameLen = 64;
constexpr int kUrlLen = 256;

struct Release {
  char tag[kTagLen] = {};       /* e.g. v0.2.0 */
  char name[kNameLen] = {};     /* e.g. Release 0.2.0 */
  char asset_url[kUrlLen] = {}; /* first .bin browser_download_url, if any */
  bool has_bin = false;
};

/** Owner/repo for WerkBuddy Releases (public). */
constexpr const char * kOwner = "therzog92";
constexpr const char * kRepo = "WerkBuddy";

/**
 * HTTPS GET api.github.com/.../releases into `out` (newest first).
 * Returns count (>=0) or -1 on network/parse failure.
 * Blocking — call from a worker thread or accept a short UI hitch.
 */
int fetch_releases(Release * out, int max_out, char * err, int err_cap);

/** Tag body without leading 'v' (points into `tag` or returns tag). */
const char * tag_body(const char * tag);

}  // namespace github_ota
}  // namespace sim
}  // namespace wp
