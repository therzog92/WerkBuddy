#pragma once
/* Prefer this over firmware/src/sim/github_ota.h on device builds.
 * Layout matches the sim API so scr_settings.cpp compiles; bodies are stubs. */

#include <cstddef>
#include <cstdio>
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

inline int fetch_releases(Release * /*out*/, int /*max_out*/, char * err, int err_cap) {
  if (err && err_cap > 0) std::snprintf(err, (size_t)err_cap, "OTA not on device yet");
  return 0;
}

inline const char * tag_body(const char * tag) {
  if (!tag || !tag[0]) return "";
  return (tag[0] == 'v' || tag[0] == 'V') ? tag + 1 : tag;
}

}  // namespace github_ota
}  // namespace sim
}  // namespace wp
