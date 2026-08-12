/*
 * Device emoji badges — curated RGB565 bitmaps in flash (emoji_assets_gen.cpp).
 * No LittleFS / LODEPNG — decoding 700 PNGs from flash made the UI unusable.
 */

#include "ui/emoji_badge.h"

#include "emoji_pack.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace ui {
namespace {

bool starts_with(const char * s, const char * prefix) {
  return s && prefix && std::strncmp(s, prefix, std::strlen(prefix)) == 0;
}

uint32_t utf8_first_cp(const char * s) {
  const auto * u = reinterpret_cast<const unsigned char *>(s);
  if (!u || !u[0]) return 0;
  if (u[0] < 0x80) return u[0];
  if ((u[0] & 0xE0) == 0xC0) return ((u[0] & 0x1Fu) << 6) | (u[1] & 0x3Fu);
  if ((u[0] & 0xF0) == 0xE0)
    return ((u[0] & 0x0Fu) << 12) | ((u[1] & 0x3Fu) << 6) | (u[2] & 0x3Fu);
  if ((u[0] & 0xF8) == 0xF0)
    return ((u[0] & 0x07u) << 18) | ((u[1] & 0x3Fu) << 12) | ((u[2] & 0x3Fu) << 6) |
           (u[3] & 0x3Fu);
  return 0;
}

}  // namespace

const char * emoji_to_twemoji_id(const char * emoji_utf8) {
  static char id[16];
  const uint32_t cp = utf8_first_cp(emoji_utf8);
  if (!cp) return "1f4e2";
  std::snprintf(id, sizeof(id), "%x", (unsigned)cp);
  return id;
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

const char * emoji_png_path(const char * /*emoji_utf8*/) {
  /* Device uses baked RGB565A8 descriptors, not FS paths. */
  return "";
}

const void * emoji_image_src(const char * emoji_utf8) { return emoji_pack::find_dsc(emoji_utf8); }

lv_font_t * emoji_imgfont(uint16_t /*height_px*/) { return nullptr; }

lv_obj_t * make_emoji_image(lv_obj_t * parent, const char * emoji_utf8, lv_coord_t size) {
  const lv_image_dsc_t * dsc = emoji_pack::find_dsc(emoji_utf8);
  if (dsc) {
    lv_obj_t * img = lv_image_create(parent);
    lv_image_set_src(img, dsc);
    const int32_t scale = (size * 256) / 72;
    lv_image_set_scale(img, scale);
    lv_obj_set_size(img, size, size);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
  }

  lv_obj_t * box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, size, size);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * lbl = lv_label_create(box);
  lv_label_set_text(lbl, emoji_chip_label(emoji_utf8));
  lv_obj_set_style_text_font(lbl, size >= 40 ? &lv_font_montserrat_16 : &lv_font_montserrat_12, 0);
  lv_obj_center(lbl);
  return box;
}

lv_obj_t * make_emoji_badge(lv_obj_t * parent, const char * emoji_utf8, lv_coord_t size) {
  return make_emoji_image(parent, emoji_utf8, size);
}

}  // namespace ui
}  // namespace wp
