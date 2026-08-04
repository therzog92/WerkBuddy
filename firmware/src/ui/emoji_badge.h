#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

const char * emoji_to_twemoji_id(const char * emoji_utf8);
const char * emoji_chip_label(const char * emoji_utf8);
const char * emoji_png_path(const char * emoji_utf8);

/**
 * LVGL imgfont that maps Unicode emoji → Twemoji PNGs.
 * Use as text_font on labels that contain real emoji UTF-8.
 */
lv_font_t * emoji_imgfont(uint16_t height_px);

/** Label showing one emoji via imgfont (preferred over raw lv_image). */
lv_obj_t * make_emoji_image(lv_obj_t * parent, const char * emoji_utf8, lv_coord_t size);

/** Same as make_emoji_image. */
lv_obj_t * make_emoji_badge(lv_obj_t * parent, const char * emoji_utf8, lv_coord_t size);

}  // namespace ui
}  // namespace wp
