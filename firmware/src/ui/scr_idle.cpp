#include "ui/scr_idle.h"

#include "app/app.h"
#include "app/background.h"
#include "app/desk_timer.h"
#include "ui/brightness.h"
#include "ui/chrome.h"
#include "ui/fonts.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cmath>
#include <ctime>

namespace wp {
namespace ui {
namespace {

lv_obj_t * g_time = nullptr;
lv_obj_t * g_desk_lbl = nullptr;
lv_obj_t * g_timer_lbl = nullptr;
lv_obj_t * g_col = nullptr;
lv_timer_t * g_timer = nullptr;
lv_timer_t * g_bounce_timer = nullptr;

float g_bx = 30.0f;
float g_by = 30.0f;
float g_bvx = 1.4f;
float g_bvy = 1.1f;
int g_color_idx = 0;

constexpr uint32_t kBounceColors[] = {
    0xf0c24b, /* Gold */
    0xff4fa3, /* Hot Magenta */
    0x5dffc2, /* Mint */
    0x3d8bfd, /* Electric Blue */
    0xff6b6b, /* Coral */
    0xa78bfa, /* Violet */
    0xd4e157, /* Lime */
    0xff9f43, /* Orange */
};
constexpr int kNumBounceColors = (int)(sizeof(kBounceColors) / sizeof(kBounceColors[0]));

void apply_bounce_color(uint32_t c_u32) {
  const lv_color_t col = lv_color_hex(c_u32);
  if (g_time) lv_obj_set_style_text_color(g_time, col, 0);
  if (g_desk_lbl) lv_obj_set_style_text_color(g_desk_lbl, col, 0);
}

void format_now(char * time_buf, size_t tn) {
  std::tm tm{};
  app::local_time(&tm);
  if (app::desk().clock_24h) {
    lv_snprintf(time_buf, (uint32_t)tn, "%02d:%02d", tm.tm_hour, tm.tm_min);
  } else {
    int hour = tm.tm_hour % 12;
    if (hour == 0) hour = 12;
    const char * ampm = tm.tm_hour >= 12 ? "PM" : "AM";
    lv_snprintf(time_buf, (uint32_t)tn, "%d:%02d %s", hour, tm.tm_min, ampm);
  }
}

void tick(lv_timer_t * /*t*/) {
  char tb[16];
  format_now(tb, sizeof(tb));
  if (g_time) lv_label_set_text(g_time, tb);
  if (g_timer_lbl) {
    if (desk_timer::is_finished()) {
      lv_label_set_text(g_timer_lbl, "Time's up");
      lv_obj_remove_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
    } else if (desk_timer::is_active()) {
      char parts[2][16];
      int n = 0;
      for (int i = 0; i < desk_timer::kSlots; ++i) {
        if (!desk_timer::is_active(i)) continue;
        desk_timer::format_remaining(i, parts[n], sizeof(parts[n]));
        ++n;
      }
      char line[48];
      if (n >= 2)
        lv_snprintf(line, sizeof(line), "T1 %s / T2 %s", parts[0], parts[1]);
      else if (n == 1)
        lv_snprintf(line, sizeof(line), "Timer %s", parts[0]);
      else
        line[0] = '\0';
      if (line[0]) {
        lv_label_set_text(g_timer_lbl, line);
        lv_obj_remove_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void bounce_tick(lv_timer_t * /*t*/) {
  if (!g_col) return;
  lv_coord_t w = lv_obj_get_width(g_col);
  lv_coord_t h = lv_obj_get_height(g_col);
  if (w <= 0) w = 220;
  if (h <= 0) h = 140;

  g_bx += g_bvx;
  g_by += g_bvy;

  bool bounced = false;
  if (g_bx <= 10.0f) {
    g_bx = 10.0f;
    g_bvx = std::abs(g_bvx);
    bounced = true;
  } else if (g_bx + w >= (WP_HOR_RES - 10)) {
    g_bx = (float)(WP_HOR_RES - 10 - w);
    g_bvx = -std::abs(g_bvx);
    bounced = true;
  }

  if (g_by <= 10.0f) {
    g_by = 10.0f;
    g_bvy = std::abs(g_bvy);
    bounced = true;
  } else if (g_by + h >= (WP_VER_RES - 10)) {
    g_by = (float)(WP_VER_RES - 10 - h);
    g_bvy = -std::abs(g_bvy);
    bounced = true;
  }

  if (bounced) {
    g_color_idx = (g_color_idx + 1) % kNumBounceColors;
    apply_bounce_color(kBounceColors[g_color_idx]);
  }

  lv_obj_set_pos(g_col, (lv_coord_t)g_bx, (lv_coord_t)g_by);
}

void on_deleted(lv_event_t * /*e*/) {
  g_time = nullptr;
  g_desk_lbl = nullptr;
  g_timer_lbl = nullptr;
  g_col = nullptr;
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
  if (g_bounce_timer) {
    lv_timer_delete(g_bounce_timer);
    g_bounce_timer = nullptr;
  }
}

}  // namespace

lv_obj_t * idle_screen() {
  lv_obj_t * scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_size(scr, WP_HOR_RES, WP_VER_RES);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  /* PRESSED wakes even if CLICKED is eaten by a toast / short tap glitch. */
  auto wake = [](lv_event_t * /*e*/) { wake_from_idle(); };
  lv_obj_add_event_cb(scr, wake, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(scr, wake, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(scr, on_deleted, LV_EVENT_DELETE, nullptr);

  /* Pre-setup always blacks out (and kills backlight) — no clock drain in a bag. */
  const bool has_display = app::desk().setup_done && (app::desk().idle_mode == 1 || app::desk().idle_mode == 2);
  const bool bounce_mode = app::desk().setup_done && (app::desk().idle_mode == 2);

  theme::apply_idle_bg(scr, !has_display);
  brightness::set_panel_on(has_display);

  if (has_display) {
    /* Accent pool only when there is no photo — wallpaper idle is a picture frame. */
    if (!bounce_mode && !background::has()) {
      lv_obj_t * glow = lv_obj_create(scr);
      lv_obj_remove_style_all(glow);
      lv_obj_set_size(glow, 280, 280);
      lv_obj_center(glow);
      lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(glow, theme::hot(), 0);
      lv_obj_set_style_bg_opa(glow, 22, 0);
      lv_obj_remove_flag(glow, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t * col = lv_obj_create(scr);
    g_col = col;
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    if (!bounce_mode) {
      lv_obj_center(col);
    } else {
      lv_obj_set_pos(col, (lv_coord_t)g_bx, (lv_coord_t)g_by);
    }
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, bounce_mode ? 4 : 6, 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);

    g_time = nullptr;
    g_desk_lbl = nullptr;

    if ((app::desk().chrome_hide & app::Desk::kHideIdleName) == 0) {
      lv_obj_t * desk = lv_label_create(col);
      g_desk_lbl = desk;
      char desk_buf[48];
      lv_snprintf(desk_buf, sizeof(desk_buf), "%s's Desk", app::desk().name);
      lv_label_set_text(desk, desk_buf);
      lv_obj_set_style_text_color(desk, bounce_mode ? lv_color_hex(kBounceColors[g_color_idx]) : theme::gold(), 0);
      lv_obj_set_style_text_font(desk, bounce_mode ? &lv_font_montserrat_20 : &lv_font_montserrat_28, 0);
      lv_obj_set_style_text_letter_space(desk, 1, 0);
    }

    char tb[16];
    format_now(tb, sizeof(tb));
    if ((app::desk().chrome_hide & app::Desk::kHideIdleClock) == 0) {
      g_time = lv_label_create(col);
      lv_label_set_text(g_time, tb);
      lv_obj_set_style_text_color(g_time, bounce_mode ? lv_color_hex(kBounceColors[g_color_idx]) : theme::ink(), 0);
      lv_obj_set_style_text_font(g_time, bounce_mode ? font_display(64) : font_display(96), 0);
      lv_obj_set_style_text_letter_space(g_time, 2, 0);
    }

    g_timer_lbl = lv_label_create(col);
    lv_label_set_text(g_timer_lbl, "");
    lv_obj_set_style_text_color(g_timer_lbl, theme::mint(), 0);
    lv_obj_set_style_text_font(g_timer_lbl, bounce_mode ? &lv_font_montserrat_16 : &lv_font_montserrat_20, 0);
    lv_obj_add_flag(g_timer_lbl, LV_OBJ_FLAG_HIDDEN);

    if (!bounce_mode) {
      lv_obj_t * hint = lv_label_create(col);
      lv_label_set_text(hint, "tap to wake");
      lv_obj_set_style_text_color(hint, theme::muted(), 0);
      lv_obj_set_style_text_opa(hint, LV_OPA_50, 0);
      lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
      lv_obj_set_style_margin_top(hint, 20, 0);
    }

    g_timer = lv_timer_create(tick, 1000, nullptr);
    tick(nullptr);

    if (bounce_mode) {
      g_bounce_timer = lv_timer_create(bounce_tick, 33, nullptr);
    }
  }

  return scr;
}

}  // namespace ui
}  // namespace wp
