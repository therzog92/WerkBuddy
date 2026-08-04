#include "ui/fonts.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace wp {
namespace ui {
namespace {

std::filesystem::path fonts_dir() {
  namespace fs = std::filesystem;
  const fs::path candidates[] = {
      fs::current_path() / ".." / "assets" / "fonts",
      fs::current_path() / "assets" / "fonts",
      fs::path("C:/Users/Tommy/Projects/WerkPager/firmware/assets/fonts"),
  };
  for (const fs::path & p : candidates) {
    if (fs::exists(p / "BebasNeue-Regular.ttf")) return fs::weakly_canonical(p);
  }
  return candidates[0];
}

std::string fs_path(const char * filename) {
  std::string s = "S:" + (fonts_dir() / filename).string();
  for (char & c : s) {
    if (c == '\\') c = '/';
  }
  return s;
}

const lv_font_t * load_cached(const char * file, int32_t px) {
  static std::unordered_map<std::string, const lv_font_t *> cache;
  const std::string key = std::string(file) + "@" + std::to_string(px);
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;

  const std::string path = fs_path(file);
  lv_font_t * font = lv_tiny_ttf_create_file(path.c_str(), px);
  if (!font) {
    std::fprintf(stderr, "[fonts] failed to load %s @ %d\n", path.c_str(), (int)px);
    return nullptr;
  }
  cache[key] = font;
  return font;
}

}  // namespace

const lv_font_t * font_display(int32_t px) {
  if (const lv_font_t * f = load_cached("BebasNeue-Regular.ttf", px)) return f;
  return &lv_font_montserrat_48;
}

const lv_font_t * font_body_italic(int32_t px) {
  if (const lv_font_t * f = load_cached("DMSans-400-Italic.ttf", px)) return f;
  if (const lv_font_t * f = load_cached("Montserrat-Italic.ttf", px)) return f;
  return &lv_font_montserrat_14;
}

}  // namespace ui
}  // namespace wp
