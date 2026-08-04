#include "sim/screenshot.h"

#include "lvgl/lvgl.h"
#include "SDL2/SDL.h"

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace wp {
namespace sim {
namespace {

std::string preview_path() {
  namespace fs = std::filesystem;
  fs::path p = fs::current_path() / ".." / "sim-out" / "preview.png";
  if (fs::path(fs::current_path()).filename() != "build") {
    p = fs::current_path() / "sim-out" / "preview.png";
  }
  fs::create_directories(p.parent_path());
  return p.lexically_normal().string();
}

bool save_via_sdl(const char * path) {
  lv_display_t * disp = lv_display_get_default();
  if (!disp) return false;

  auto * renderer = static_cast<SDL_Renderer *>(lv_sdl_window_get_renderer(disp));
  if (!renderer) {
    std::fprintf(stderr, "[sim] no SDL renderer\n");
    return false;
  }

  const int w = lv_display_get_horizontal_resolution(disp);
  const int h = lv_display_get_vertical_resolution(disp);
  std::vector<uint8_t> bgra(static_cast<size_t>(w) * h * 4);

  // Force a refresh so the texture is current.
  lv_refr_now(disp);

  if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, bgra.data(), w * 4) != 0) {
    std::fprintf(stderr, "[sim] SDL_RenderReadPixels: %s\n", SDL_GetError());
    return false;
  }

  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
  for (int i = 0; i < w * h; ++i) {
    const uint8_t b = bgra[i * 4 + 0];
    const uint8_t g = bgra[i * 4 + 1];
    const uint8_t r = bgra[i * 4 + 2];
    const uint8_t a = bgra[i * 4 + 3];
    rgba[i * 4 + 0] = r;
    rgba[i * 4 + 1] = g;
    rgba[i * 4 + 2] = b;
    rgba[i * 4 + 3] = a ? a : 255;
  }

  if (!stbi_write_png(path, w, h, 4, rgba.data(), w * 4)) {
    std::fprintf(stderr, "[sim] failed to write %s\n", path);
    return false;
  }
  std::printf("[sim] wrote %s (%dx%d)\n", path, w, h);
  return true;
}

static void auto_shot_cb(lv_timer_t * /*t*/) {
  save_preview_png();
  std::exit(0);
}

}  // namespace

void save_preview_png() {
  const std::string path = preview_path();
  if (!save_via_sdl(path.c_str())) {
    std::fprintf(stderr, "[sim] screenshot failed\n");
  }
}

void maybe_auto_shot_and_quit() {
  const char * shot = std::getenv("WERKPAGER_SHOT");
  if (shot && shot[0] == '1') {
    /* Incoming rings need ~800ms to show staggered pulses. */
    lv_timer_create(auto_shot_cb, 900, nullptr);
  }
}

}  // namespace sim
}  // namespace wp
