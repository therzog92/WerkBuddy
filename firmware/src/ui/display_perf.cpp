#include "ui/display_perf.h"

namespace wp {
namespace ui {
namespace display_perf {
namespace {

#if defined(WP_DEVICE)
lv_display_t * g_disp = nullptr;
#endif

}  // namespace

void bind(lv_display_t * disp) {
#if defined(WP_DEVICE)
  g_disp = disp;
#else
  (void)disp;
#endif
}

void prefer_full_frame(bool on) {
#if defined(WP_DEVICE)
  if (!g_disp) return;
  lv_display_set_render_mode(g_disp, on ? LV_DISPLAY_RENDER_MODE_FULL
                                        : LV_DISPLAY_RENDER_MODE_PARTIAL);
#else
  (void)on;
#endif
}

}  // namespace display_perf
}  // namespace ui
}  // namespace wp
