#include "ui/fonts.h"

namespace wp {
namespace ui {

const lv_font_t * font_display(int32_t px) {
  if (px >= 40) return &lv_font_montserrat_48;
  if (px >= 24) return &lv_font_montserrat_28;
  if (px >= 18) return &lv_font_montserrat_20;
  return &lv_font_montserrat_16;
}

const lv_font_t * font_body_italic(int32_t px) {
  (void)px;
  return &lv_font_montserrat_14;
}

}  // namespace ui
}  // namespace wp
