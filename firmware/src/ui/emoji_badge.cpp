#include "ui/emoji_badge.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace wp {
namespace ui {
namespace {

bool starts_with(const char * s, const char * prefix) {
  return s && prefix && std::strncmp(s, prefix, std::strlen(prefix)) == 0;
}

std::filesystem::path assets_emoji_dir() {
  namespace fs = std::filesystem;
  const fs::path candidates[] = {
      fs::current_path() / ".." / "assets" / "emoji",
      fs::current_path() / "assets" / "emoji",
      fs::path("C:/Users/Tommy/Projects/WerkPager/firmware/assets/emoji"),
  };
  for (const fs::path & p : candidates) {
    if (fs::exists(p / "1f4e2.png")) return fs::weakly_canonical(p);
  }
  return candidates[0];
}

std::string make_png_path(const char * twemoji_id) {
  std::string s = "S:" + (assets_emoji_dir() / (std::string(twemoji_id) + ".png")).string();
  for (char & c : s) {
    if (c == '\\') c = '/';
  }
  return s;
}

/* Twemoji files are named by lowercase hex codepoint — derive instead of hardcoding. */
const char * unicode_to_twemoji_id(uint32_t unicode) {
  static std::unordered_map<uint32_t, std::string> ids;
  auto it = ids.find(unicode);
  if (it == ids.end()) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%x", unicode);
    it = ids.emplace(unicode, buf).first;
  }
  return it->second.c_str();
}

/** First codepoint of a UTF-8 string (emoji here are single-codepoint). */
uint32_t utf8_first_cp(const char * s) {
  const auto * u = reinterpret_cast<const unsigned char *>(s);
  if (!u || !u[0]) return 0;
  if (u[0] < 0x80) return u[0];
  if ((u[0] & 0xE0) == 0xC0) return ((u[0] & 0x1Fu) << 6) | (u[1] & 0x3Fu);
  if ((u[0] & 0xF0) == 0xE0)
    return ((u[0] & 0x0Fu) << 12) | ((u[1] & 0x3Fu) << 6) | (u[2] & 0x3Fu);
  if ((u[0] & 0xF8) == 0xF0)
    return ((u[0] & 0x07u) << 18) | ((u[1] & 0x3Fu) << 12) | ((u[2] & 0x3Fu) << 6) | (u[3] & 0x3Fu);
  return 0;
}

const char * path_for_unicode(uint32_t unicode) {
  static std::unordered_map<uint32_t, std::string> paths;
  auto it = paths.find(unicode);
  if (it != paths.end()) return it->second.empty() ? nullptr : it->second.c_str();

  /* Twemoji often ships as bare codepoint or codepoint-fe0f — try both. */
  const char * ids[2] = {unicode_to_twemoji_id(unicode), nullptr};
  char fe0f[24];
  std::snprintf(fe0f, sizeof(fe0f), "%x-fe0f", unicode);
  ids[1] = fe0f;

  std::string found;
  for (const char * id : ids) {
    if (!id) continue;
    const std::string path = make_png_path(id);
    if (std::filesystem::exists(path.substr(2))) {
      found = path;
      break;
    }
  }
  paths[unicode] = found;
  return paths[unicode].empty() ? nullptr : paths[unicode].c_str();
}

const void * imgfont_path_cb(const lv_font_t * /*font*/, uint32_t unicode, uint32_t /*unicode_next*/,
                             int32_t * /*offset_y*/, void * /*user_data*/) {
  return path_for_unicode(unicode);
}

}  // namespace

const char * emoji_to_twemoji_id(const char * emoji_utf8) {
  const uint32_t cp = utf8_first_cp(emoji_utf8);
  if (!cp) return "1f4e2";
  return unicode_to_twemoji_id(cp);
}

const char * emoji_chip_label(const char * emoji_utf8) {
  if (starts_with(emoji_utf8, "💅")) return "nails";
  if (starts_with(emoji_utf8, "👑")) return "crown";
  if (starts_with(emoji_utf8, "📢")) return "call";
  if (starts_with(emoji_utf8, "👀")) return "eyes";
  if (starts_with(emoji_utf8, "✨")) return "spark";
  if (starts_with(emoji_utf8, "☕")) return "coffee";
  if (starts_with(emoji_utf8, "🆘")) return "SOS";
  if (starts_with(emoji_utf8, "🎉")) return "party";
  return "ping";
}

const char * emoji_png_path(const char * emoji_utf8) {
  static char path[512];
  const uint32_t cp = utf8_first_cp(emoji_utf8);
  const char * resolved = path_for_unicode(cp);
  if (resolved) {
    /* path_for_unicode returns stable cached "S:..." string */
    return resolved;
  }
  /* Fallback path even if missing — caller may still try to load. */
  const char * id = emoji_to_twemoji_id(emoji_utf8);
  const auto dir = assets_emoji_dir();
  std::snprintf(path, sizeof(path), "S:%s/%s.png", dir.string().c_str(), id);
  for (char * p = path; *p; ++p) {
    if (*p == '\\') *p = '/';
  }
  return path;
}

lv_font_t * emoji_imgfont(uint16_t height_px) {
  static std::unordered_map<uint16_t, lv_font_t *> cache;
  auto it = cache.find(height_px);
  if (it != cache.end()) return it->second;

  lv_font_t * font = lv_imgfont_create(height_px, imgfont_path_cb, nullptr);
  if (!font) {
    std::fprintf(stderr, "[emoji] imgfont create failed @ %u\n", (unsigned)height_px);
    return nullptr;
  }
  font->fallback = &lv_font_montserrat_14;
  cache[height_px] = font;
  return font;
}

lv_obj_t * make_emoji_image(lv_obj_t * parent, const char * emoji_utf8, lv_coord_t size) {
  /*
   * Sized emoji via lv_image + scale (imgfont draws native PNG size and ignores
   * height for pixel dims — fine for inline text, wrong for fixed-size badges).
   * Imgfont remains available via emoji_imgfont() for mixed text labels.
   */
  lv_obj_t * img = lv_image_create(parent);
  lv_image_set_src(img, emoji_png_path(emoji_utf8));
  const int32_t scale = (size * 256) / 72;
  lv_image_set_scale(img, scale);
  lv_obj_set_size(img, size, size);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  return img;
}

lv_obj_t * make_emoji_badge(lv_obj_t * parent, const char * emoji_utf8, lv_coord_t size) {
  return make_emoji_image(parent, emoji_utf8, size);
}

}  // namespace ui
}  // namespace wp
