#include "ui/theme.h"

#include "app/app.h"
#include "app/background.h"

#include <cstring>

#ifdef WP_DEVICE
#include <esp_heap_caps.h>
#endif

namespace wp {
namespace theme {
namespace {

struct Palette {
  uint32_t bg0, bg1, panel, ink, muted, gold, hot, mint, danger, border, grad_top, grad_bot, call_a,
      call_b;
};

/* UI tokens match web/styles.css; call_a/call_b are the bright board-mock
 * flash pair (paging-lvgl-preview.html) — web’s near-black call colors only
 * work under the CSS rotating wash, and look static on LVGL alone. */
constexpr Palette kPalettes[] = {
    /* Eleganza — magenta ↔ violet */
    {0x0c0a0f, 0x1a1224, 0x22182f, 0xf7f2ea, 0xb9a8c9, 0xf0c24b, 0xff4fa3, 0x5dffc2, 0xff5c7a,
     0x3d2a55, 0x2b1b3d, 0x120c18, 0xc41e6a, 0x6b1fa8},
    /* Runway — scarlet ↔ deep crimson */
    {0x0a0a0a, 0x1a1212, 0x241616, 0xfff5f0, 0xc9a8a8, 0xf0c24b, 0xff3b3b, 0xffb4a2, 0xff6b6b,
     0x5a2a2a, 0x3a1515, 0x100808, 0xc41e2a, 0x7a1018},
    /* Ice — electric blue X (hot) + warm gold O (was too blue-on-blue) */
    {0x061018, 0x0c1c2a, 0x122536, 0xe8f6ff, 0x8eb4c9, 0xffd166, 0x3d8bfd, 0x5dffc2, 0xff6b8a,
     0x1e3d55, 0x123048, 0x081018, 0x3d8bfd, 0x104070},
    /* Lemon — gold ↔ amber brown */
    {0x12100a, 0x1a1810, 0x2a2418, 0xfff8e8, 0xc9b898, 0xffe566, 0xffb703, 0xb8f248, 0xff5c7a,
     0x5a4a20, 0x3a3010, 0x100c08, 0xffb703, 0x6a4810},
    /* Matcha — green ↔ deep forest (mint = teal so marks/UI aren't two greens) */
    {0x0a120c, 0x122018, 0x1a2a20, 0xf0fff4, 0xa8c9b0, 0xd4e157, 0x2ecc71, 0x5eead4, 0xff5c7a,
     0x2a5540, 0x1a3828, 0x081208, 0x2ecc71, 0x105038},
    /* Cosmic — indigo ↔ electric violet */
    {0x080814, 0x12122a, 0x1c1a38, 0xeef0ff, 0xa0a8d8, 0xc4b5fd, 0x8b5cf6, 0x67e8f9, 0xff5c7a,
     0x35305a, 0x1a1840, 0x0a0a18, 0x7c3aed, 0x312e81},
    /* Coral — peach ↔ rose */
    {0x140c0c, 0x221616, 0x2e1c1c, 0xfff5f2, 0xd4a8a0, 0xffd6a5, 0xff6b6b, 0xffb4a2, 0xff5c7a,
     0x5a3030, 0x3a1818, 0x100808, 0xe85d4c, 0x9a3040},
    /* Slate — cool steel ↔ charcoal */
    {0x0c0e12, 0x161a22, 0x1e2430, 0xf0f4f8, 0x9aa8b8, 0xe2e8f0, 0x64748b, 0x94a3b8, 0xff5c7a,
     0x3a4555, 0x1a222e, 0x080a0e, 0x475569, 0x1e293b},
};

Id g_id = Id::Eleganza;

const Palette & pal() {
  return kPalettes[static_cast<uint8_t>(g_id)];
}

void init_radial(lv_grad_dsc_t * g, lv_color_t accent, lv_color_t edge, lv_opa_t peak) {
  const lv_color_t colors[2] = {accent, edge};
  const lv_opa_t opas[2] = {peak, LV_OPA_TRANSP};
  lv_grad_init_stops(g, colors, opas, nullptr, 2);
  lv_grad_radial_init(g, LV_GRAD_CENTER, LV_GRAD_CENTER, LV_GRAD_RIGHT, LV_GRAD_BOTTOM,
                      LV_GRAD_EXTEND_PAD);
}

/* Blend `b` into `a` by mix/255 in 24-bit space (used for both sim + device stop math). */
uint32_t mix24(uint32_t a, uint32_t b, uint8_t mix) {
  const uint8_t im = (uint8_t)(255 - mix);
  const uint8_t r = (uint8_t)(((((a >> 16) & 0xFF) * im) + (((b >> 16) & 0xFF) * mix) + 127) / 255);
  const uint8_t g = (uint8_t)(((((a >> 8) & 0xFF) * im) + (((b >> 8) & 0xFF) * mix) + 127) / 255);
  const uint8_t bl = (uint8_t)((((a & 0xFF) * im) + ((b & 0xFF) * mix) + 127) / 255);
  return (uint32_t)((r << 16) | (g << 8) | bl);
}

#ifdef WP_DEVICE
/*
 * RGB565 gradient banding fix. The device renders each vertical-gradient row as
 * one quantized 565 color with no dithering, and our dark theme scales collapse
 * to very few 5/6-bit steps — so wide colour bands are clearly visible.
 *
 * Instead we bake the gradient once, in 24-bit, into a full-screen RGB565 image
 * in PSRAM with 4×4 ordered dithering applied at the final quantize. The screen
 * then paints it as a static bg image (a plain blit — no per-frame gradient or
 * dither cost, and the same pattern the wallpaper bake already uses).
 */
struct BakedGrad {
  lv_image_dsc_t img{};
  uint16_t * px = nullptr;
  uint32_t stops[4] = {};
  bool valid = false;
};
BakedGrad g_baked;

void free_baked_grad() {
  if (g_baked.px) heap_caps_free(g_baked.px);
  g_baked = BakedGrad{};
}

uint8_t dithered(uint8_t c, int bayer, uint8_t bits) {
  const int levels = 1 << bits;
  const int step = 256 / levels;
  int q = c / step;
  const int rem = c - q * step;
  const int th = ((bayer * 2 + 1) * step) >> 5;
  if (rem > th) q++;
  if (q >= levels) q = levels - 1;
  return (uint8_t)q;
}

bool bake_gradient(const uint32_t * stops, const uint8_t fracs[4]) {
  free_baked_grad();
  const int w = WP_HOR_RES;
  const int h = WP_VER_RES;
  const size_t nbytes = (size_t)w * (size_t)h * 2u;
  uint16_t * px = (uint16_t *)heap_caps_malloc(nbytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!px) px = (uint16_t *)heap_caps_malloc(nbytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!px) return false;

  static const int8_t kBay[4][4] = {
      {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

  const int range = h;
  for (int y = 0; y < h; ++y) {
    /* Match LVGL's vertical-gradient stop mapping exactly. */
    uint32_t c;
    const int lo_all = (fracs[0] * range) >> 8;
    const int hi_all = (fracs[3] * range) >> 8;
    if (y <= lo_all) {
      c = stops[0];
    } else if (y >= hi_all) {
      c = stops[3];
    } else {
      int fi = 1;
      for (int i = 1; i < 4; ++i) {
        const int cur = (fracs[i] * range) >> 8;
        if (y <= cur) { fi = i; break; }
      }
      const int lo = (fracs[fi - 1] * range) >> 8;
      const int hi = (fracs[fi] * range) >> 8;
      const int d = hi - lo;
      const int mix = ((y - lo) * 255) / (d ? d : 1);
      const int imix = 255 - mix;
      const uint32_t a = stops[fi - 1];
      const uint32_t b = stops[fi];
      const uint8_t r = (uint8_t)(((((a >> 16) & 0xFF) * imix) + (((b >> 16) & 0xFF) * mix) + 127) / 255);
      const uint8_t g = (uint8_t)(((((a >> 8) & 0xFF) * imix) + (((b >> 8) & 0xFF) * mix) + 127) / 255);
      const uint8_t bl = (uint8_t)((((a & 0xFF) * imix) + ((b & 0xFF) * mix) + 127) / 255);
      c = (uint32_t)((r << 16) | (g << 8) | bl);
    }

    uint16_t * row = px + (size_t)y * (size_t)w;
    const int by = y & 3;
    for (int x = 0; x < w; ++x) {
      const int bayer = kBay[by][x & 3];
      const uint8_t r5 = dithered((uint8_t)((c >> 16) & 0xFF), bayer, 5);
      const uint8_t g6 = dithered((uint8_t)((c >> 8) & 0xFF), bayer, 6);
      const uint8_t b5 = dithered((uint8_t)(c & 0xFF), bayer, 5);
      row[x] = (uint16_t)((uint16_t)(r5 << 11) | (uint16_t)(g6 << 5) | (uint16_t)b5);
    }
  }

  std::memcpy(g_baked.stops, stops, 4 * sizeof(uint32_t));
  g_baked.px = px;
  g_baked.img.data = (const uint8_t *)px;
  g_baked.img.data_size = (uint32_t)nbytes;
  g_baked.img.header.magic = LV_IMAGE_HEADER_MAGIC;
  g_baked.img.header.cf = LV_COLOR_FORMAT_RGB565;
  g_baked.img.header.flags = 0;
  g_baked.img.header.w = (uint32_t)w;
  g_baked.img.header.h = (uint32_t)h;
  g_baked.img.header.stride = (uint32_t)w * 2u;
  g_baked.img.reserved = nullptr;
  g_baked.img.reserved_2 = nullptr;
  g_baked.valid = true;
  return true;
}
#endif

}  // namespace

void set(Id id) {
  if (static_cast<uint8_t>(id) >= static_cast<uint8_t>(Id::Count)) return;
  g_id = id;
}

Id current() { return g_id; }

const char * name(Id id) {
  switch (id) {
    case Id::Eleganza: return "Eleganza";
    case Id::Runway: return "Runway";
    case Id::Ice: return "Ice";
    case Id::Lemon: return "Lemon";
    case Id::Matcha: return "Matcha";
    case Id::Cosmic: return "Cosmic";
    case Id::Coral: return "Coral";
    case Id::Slate: return "Slate";
    default: return "?";
  }
}

lv_color_t bg0() { return lv_color_hex(pal().bg0); }
lv_color_t bg1() { return lv_color_hex(pal().bg1); }
lv_color_t panel() { return lv_color_hex(pal().panel); }
lv_color_t ink() { return lv_color_hex(pal().ink); }
lv_color_t muted() { return lv_color_hex(pal().muted); }
lv_color_t gold() { return lv_color_hex(pal().gold); }
lv_color_t hot() { return lv_color_hex(pal().hot); }
lv_color_t mint() { return lv_color_hex(pal().mint); }
lv_color_t danger() { return lv_color_hex(pal().danger); }
lv_color_t border() { return lv_color_hex(pal().border); }
lv_color_t call_a() { return lv_color_hex(pal().call_a); }
lv_color_t call_b() { return lv_color_hex(pal().call_b); }
lv_color_t grad_top() { return lv_color_hex(pal().grad_top); }
lv_color_t grad_bot() { return lv_color_hex(pal().grad_bot); }

void apply_screen_bg(lv_obj_t * scr) { apply_screen_bg(scr, BgWash::Page); }

void apply_screen_bg(lv_obj_t * scr, BgWash wash) {
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(scr, bg0(), 0);

  if (background::has() && background::lv_src()) {
    lv_opa_t dim = background::kWashPage;
    lv_opa_t img_opa = background::kImageOpaPage;
    switch (wash) {
      case BgWash::Hub:
        dim = background::kWashHub;
        img_opa = background::kImageOpaHub;
        break;
      case BgWash::Idle:
        dim = background::kWashIdle;
        img_opa = background::kImageOpaIdle;
        break;
      case BgWash::Page:
      default:
        dim = background::kWashPage;
        img_opa = background::kImageOpaPage;
        break;
    }
    lv_obj_set_style_bg_grad(scr, nullptr, 0);
    /* Paint wallpaper as the screen's own bg-image (not a child) so dock/body
     * layouts can't cover or reorder it away. Dim via image opa + recolor. */
    lv_obj_set_style_bg_image_src(scr, background::lv_src(), 0);
    lv_obj_set_style_bg_image_opa(scr, img_opa, 0);
    lv_obj_set_style_bg_image_recolor(scr, bg0(), 0);
    lv_obj_set_style_bg_image_recolor_opa(scr, dim, 0);
    return;
  }

  lv_obj_set_style_bg_image_src(scr, nullptr, 0);

  const background::Preset preset = background::preset();
  const uint8_t fracs[4] = {0, 70, 150, 255};
  uint32_t s0, s1, s2, s3;
  if (preset == background::Preset::Theme) {
    s0 = pal().grad_top;
    s1 = pal().bg1;
    s2 = pal().bg0;
    s3 = pal().grad_bot;
  } else {
    uint32_t top = 0, bot = 0;
    background::preset_colors(preset, &top, &bot);
    /* Mid stops blend toward theme panel so chrome still feels related. */
    s0 = top;
    s1 = mix24(top, pal().panel, 90);
    s2 = mix24(bot, pal().bg0, 80);
    s3 = bot;
  }

#ifdef WP_DEVICE
  {
    const uint32_t stops[4] = {s0, s1, s2, s3};
    if (!g_baked.valid || std::memcmp(stops, g_baked.stops, sizeof(stops)) != 0) {
      bake_gradient(stops, fracs);
    }
    if (g_baked.valid) {
      lv_obj_set_style_bg_grad(scr, nullptr, 0);
      lv_obj_set_style_bg_image_src(scr, &g_baked.img, 0);
      return;
    }
    /* Bake failed — fall through to the plain gradient. */
  }
#endif

  static lv_grad_dsc_t grad;
  const lv_color_t colors[] = {lv_color_hex(s0), lv_color_hex(s1), lv_color_hex(s2),
                               lv_color_hex(s3)};
  const lv_opa_t opas[] = {LV_OPA_COVER, LV_OPA_COVER, LV_OPA_COVER, LV_OPA_COVER};
  lv_grad_init_stops(&grad, colors, opas, fracs, 4);
  lv_grad_vertical_init(&grad);
  lv_obj_set_style_bg_grad(scr, &grad, 0);
}

void refresh_incoming_grads(lv_grad_dsc_t * hot_g, lv_grad_dsc_t * gold_g, lv_grad_dsc_t * mint_g) {
  init_radial(hot_g, hot(), call_a(), LV_OPA_70);
  init_radial(gold_g, gold(), call_a(), LV_OPA_50);
  init_radial(mint_g, mint(), call_a(), LV_OPA_40);
}

}  // namespace theme
}  // namespace wp
