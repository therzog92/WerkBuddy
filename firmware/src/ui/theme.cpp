#include "ui/theme.h"

#include "app/background.h"

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
    /* Ice — electric blue ↔ deep teal */
    {0x061018, 0x0c1c2a, 0x122536, 0xe8f6ff, 0x8eb4c9, 0x7ad7ff, 0x3d8bfd, 0x5dffc2, 0xff6b8a,
     0x1e3d55, 0x123048, 0x081018, 0x3d8bfd, 0x104070},
    /* Lemon — gold ↔ amber brown */
    {0x12100a, 0x1a1810, 0x2a2418, 0xfff8e8, 0xc9b898, 0xffe566, 0xffb703, 0xb8f248, 0xff5c7a,
     0x5a4a20, 0x3a3010, 0x100c08, 0xffb703, 0x6a4810},
    /* Matcha — green ↔ deep forest */
    {0x0a120c, 0x122018, 0x1a2a20, 0xf0fff4, 0xa8c9b0, 0xd4e157, 0x2ecc71, 0x7dffb3, 0xff5c7a,
     0x2a5540, 0x1a3828, 0x081208, 0x2ecc71, 0x105038},
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
  static lv_grad_dsc_t grad;
  const lv_color_t colors[] = {grad_top(), bg1(), bg0(), grad_bot()};
  const lv_opa_t opas[] = {LV_OPA_COVER, LV_OPA_COVER, LV_OPA_COVER, LV_OPA_COVER};
  const uint8_t fracs[] = {0, 70, 150, 255};
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
