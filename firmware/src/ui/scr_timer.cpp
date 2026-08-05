#include "ui/scr_timer.h"

#include "app/app.h"
#include "app/desk_timer.h"
#include "ui/chrome.h"
#include "ui/emoji_badge.h"
#include "ui/fonts.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace ui {
namespace {

lv_obj_t * g_time_lbl = nullptr;
lv_obj_t * g_status_lbl = nullptr;
lv_obj_t * g_primary_btn = nullptr;
lv_obj_t * g_primary_lbl = nullptr;
lv_obj_t * g_picker = nullptr;
lv_obj_t * g_roller_min = nullptr;
lv_obj_t * g_roller_sec = nullptr;
lv_obj_t * g_preset_row = nullptr;
bool g_syncing_rollers = false;

void refresh_ui();

void on_listen() { refresh_ui(); }

void on_deleted(lv_event_t * /*e*/) {
  desk_timer::set_listener(nullptr);
  g_time_lbl = nullptr;
  g_status_lbl = nullptr;
  g_primary_btn = nullptr;
  g_primary_lbl = nullptr;
  g_picker = nullptr;
  g_roller_min = nullptr;
  g_roller_sec = nullptr;
  g_preset_row = nullptr;
}

const char * min_options() {
  static char buf[400];
  static bool ready = false;
  if (!ready) {
    buf[0] = '\0';
    for (int i = 0; i <= 99; ++i) {
      char one[8];
      std::snprintf(one, sizeof(one), i ? "\n%d" : "%d", i);
      std::strncat(buf, one, sizeof(buf) - std::strlen(buf) - 1);
    }
    ready = true;
  }
  return buf;
}

const char * sec_options() {
  static char buf[220];
  static bool ready = false;
  if (!ready) {
    buf[0] = '\0';
    for (int i = 0; i <= 59; ++i) {
      char one[8];
      std::snprintf(one, sizeof(one), i ? "\n%02d" : "%02d", i);
      std::strncat(buf, one, sizeof(buf) - std::strlen(buf) - 1);
    }
    ready = true;
  }
  return buf;
}

void style_roller(lv_obj_t * r) {
  lv_obj_set_width(r, 100);
  lv_obj_set_style_bg_color(r, theme::panel(), 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(r, 14, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_text_color(r, theme::muted(), 0);
  lv_obj_set_style_text_font(r, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(r, theme::gold(), LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_set_style_text_color(r, lv_color_hex(0x1a1200), LV_PART_SELECTED);
  lv_obj_set_style_text_font(r, &lv_font_montserrat_20, LV_PART_SELECTED);
  lv_roller_set_visible_row_count(r, 3);
}

void apply_rollers_to_duration() {
  if (!g_roller_min || !g_roller_sec) return;
  if (desk_timer::state() != desk_timer::State::Idle) return;
  const uint32_t m = (uint32_t)lv_roller_get_selected(g_roller_min);
  const uint32_t s = (uint32_t)lv_roller_get_selected(g_roller_sec);
  uint32_t ms = m * 60 * 1000 + s * 1000;
  if (ms < 1000) ms = 1000;
  desk_timer::set_duration_ms(ms);
}

void sync_rollers_from_duration() {
  if (!g_roller_min || !g_roller_sec) return;
  g_syncing_rollers = true;
  const uint32_t sec = desk_timer::duration_ms() / 1000;
  lv_roller_set_selected(g_roller_min, (uint16_t)(sec / 60), LV_ANIM_OFF);
  lv_roller_set_selected(g_roller_sec, (uint16_t)(sec % 60), LV_ANIM_OFF);
  g_syncing_rollers = false;
}

void on_roller(lv_event_t * /*e*/) {
  if (g_syncing_rollers) return;
  apply_rollers_to_duration();
  if (g_status_lbl && desk_timer::state() == desk_timer::State::Idle) {
    char buf[16];
    desk_timer::format_remaining(buf, sizeof(buf));
    char line[40];
    lv_snprintf(line, sizeof(line), "Set  %s", buf);
    lv_label_set_text(g_status_lbl, line);
  }
}

void style_chip(lv_obj_t * b, bool selected) {
  lv_obj_set_style_bg_color(b, selected ? theme::gold() : theme::panel(), 0);
  lv_obj_t * l = lv_obj_get_child(b, 0);
  if (l)
    lv_obj_set_style_text_color(l, selected ? lv_color_hex(0x1a1200) : theme::ink(), 0);
}

void rebuild_presets() {
  if (!g_preset_row) return;
  while (lv_obj_get_child_count(g_preset_row) > 0) lv_obj_delete(lv_obj_get_child(g_preset_row, 0));

  static const uint32_t kPresetsMin[] = {5, 10, 15, 25, 45};
  const uint32_t cur_min = desk_timer::duration_ms() / 60000;
  const bool can_edit = desk_timer::state() == desk_timer::State::Idle;
  const bool exact_min = (desk_timer::duration_ms() % 60000) == 0;

  for (uint32_t m : kPresetsMin) {
    char lab[8];
    lv_snprintf(lab, sizeof(lab), "%um", (unsigned)m);
    lv_obj_t * b = lv_button_create(g_preset_row);
    lv_obj_set_size(b, 56, 34);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, lab);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
    style_chip(b, can_edit && exact_min && cur_min == m);
    if (!can_edit) lv_obj_add_state(b, LV_STATE_DISABLED);
    lv_obj_add_event_cb(
        b,
        [](lv_event_t * e) {
          if (desk_timer::state() != desk_timer::State::Idle) return;
          const uint32_t mins = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
          desk_timer::set_duration_ms(mins * 60 * 1000);
          sync_rollers_from_duration();
          refresh_ui();
        },
        LV_EVENT_CLICKED, (void *)(uintptr_t)m);
  }
}

void refresh_ui() {
  if (!g_time_lbl) return;
  const auto st = desk_timer::state();
  /* Finished takes over the whole screen — rebuild via go_timer. */
  if (st == desk_timer::State::Finished) {
    go_timer();
    return;
  }
  const bool picking = (st == desk_timer::State::Idle);

  if (g_picker) {
    if (picking) lv_obj_remove_flag(g_picker, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g_picker, LV_OBJ_FLAG_HIDDEN);
  }
  if (picking) {
    lv_obj_add_flag(g_time_lbl, LV_OBJ_FLAG_HIDDEN);
    sync_rollers_from_duration();
  } else {
    lv_obj_remove_flag(g_time_lbl, LV_OBJ_FLAG_HIDDEN);
    char buf[16];
    desk_timer::format_remaining(buf, sizeof(buf));
    lv_label_set_text(g_time_lbl, buf);
  }

  if (g_status_lbl) {
    if (st == desk_timer::State::Running) lv_label_set_text(g_status_lbl, "Running");
    else if (st == desk_timer::State::Paused) lv_label_set_text(g_status_lbl, "Paused");
    else {
      char buf[16];
      desk_timer::format_remaining(buf, sizeof(buf));
      char line[40];
      lv_snprintf(line, sizeof(line), "Set  %s", buf);
      lv_label_set_text(g_status_lbl, line);
    }
    lv_obj_set_style_text_color(g_status_lbl, theme::muted(), 0);
  }

  if (g_primary_lbl && g_primary_btn) {
    if (st == desk_timer::State::Running) {
      lv_label_set_text(g_primary_lbl, "Pause");
      lv_obj_set_style_bg_color(g_primary_btn, theme::panel(), 0);
    } else if (st == desk_timer::State::Paused) {
      lv_label_set_text(g_primary_lbl, "Resume");
      lv_obj_set_style_bg_color(g_primary_btn, theme::gold(), 0);
    } else {
      lv_label_set_text(g_primary_lbl, "Start");
      lv_obj_set_style_bg_color(g_primary_btn, theme::gold(), 0);
    }
    lv_obj_set_style_text_color(
        g_primary_lbl,
        (st == desk_timer::State::Running) ? theme::ink() : lv_color_hex(0x1a1200), 0);
  }

  static desk_timer::State s_last = desk_timer::State::Idle;
  static uint32_t s_dur = 0;
  if (st != s_last || desk_timer::duration_ms() != s_dur ||
      (g_preset_row && lv_obj_get_child_count(g_preset_row) == 0)) {
    s_last = st;
    s_dur = desk_timer::duration_ms();
    rebuild_presets();
  }
}

/* —— Full-screen alarm (mirrors incoming page flash) —— */

constexpr lv_coord_t kAlarmStage = 360;

void anim_ring_size(void * obj, int32_t v) {
  auto * ring = static_cast<lv_obj_t *>(obj);
  constexpr int32_t half = kAlarmStage / 2;
  lv_obj_set_size(ring, v, v);
  lv_obj_set_pos(ring, half - v / 2, half - v / 2);
}

void anim_ring_opa(void * obj, int32_t v) {
  lv_obj_set_style_border_opa(static_cast<lv_obj_t *>(obj), (lv_opa_t)v, 0);
}

void anim_bg_mix(void * obj, int32_t v) {
  auto * wash = static_cast<lv_obj_t *>(obj);
  const lv_color_t c = lv_color_mix(theme::call_b(), theme::call_a(), (lv_opa_t)v);
  lv_obj_set_style_bg_color(wash, c, 0);
  lv_obj_invalidate(wash);
}

void anim_blink_opa(void * obj, int32_t v) {
  lv_obj_set_style_text_opa(static_cast<lv_obj_t *>(obj), (lv_opa_t)v, 0);
}

void anim_glow(void * obj, int32_t v) {
  auto * glow = static_cast<lv_obj_t *>(obj);
  constexpr int32_t kBase = 220;
  constexpr int32_t half_stage = kAlarmStage / 2;
  const int32_t size = kBase * (85 + v * 30 / 100) / 100;
  lv_obj_set_size(glow, size, size);
  lv_obj_set_pos(glow, half_stage - size / 2, half_stage - size / 2);
  lv_obj_set_style_opa(glow, (lv_opa_t)(56 + v * 51 / 100), 0);
}

void anim_emoji_pop(void * obj, int32_t v) {
  lv_image_set_scale(static_cast<lv_obj_t *>(obj), (uint32_t)v);
}

void on_alarm_dismiss(lv_event_t * /*e*/) {
  desk_timer::dismiss();
  go_timer();
}

void on_alarm_deleted(lv_event_t * /*e*/) {
  desk_timer::set_listener(nullptr);
}

lv_obj_t * timer_alarm_screen() {
  lv_obj_t * scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_size(scr, WP_HOR_RES, WP_VER_RES);
  lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(scr, on_alarm_deleted, LV_EVENT_DELETE, nullptr);

  lv_obj_t * wash = lv_obj_create(scr);
  lv_obj_remove_style_all(wash);
  lv_obj_set_size(wash, WP_HOR_RES, WP_VER_RES);
  lv_obj_set_pos(wash, 0, 0);
  lv_obj_set_style_bg_color(wash, theme::call_a(), 0);
  lv_obj_set_style_bg_opa(wash, LV_OPA_COVER, 0);
  lv_obj_remove_flag(wash, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(wash, LV_OBJ_FLAG_SCROLLABLE);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, wash);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 550);
    lv_anim_set_reverse_duration(&a, 550);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_bg_mix);
    lv_anim_start(&a);
  }

  constexpr lv_coord_t kStage = kAlarmStage;
  constexpr lv_coord_t kRing = 100;
  lv_obj_t * stage = lv_obj_create(scr);
  lv_obj_remove_style_all(stage);
  lv_obj_set_size(stage, kStage, kStage);
  lv_obj_set_style_bg_opa(stage, LV_OPA_TRANSP, 0);
  lv_obj_align(stage, LV_ALIGN_TOP_MID, 0, (WP_VER_RES * 42) / 100 - kStage / 2);
  lv_obj_remove_flag(stage, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(stage, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * glow = lv_obj_create(stage);
  lv_obj_remove_style_all(glow);
  lv_obj_set_size(glow, 220, 220);
  lv_obj_set_pos(glow, kStage / 2 - 110, kStage / 2 - 110);
  lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(glow, 0, 0);
  lv_obj_set_style_bg_color(glow, theme::hot(), 0);
  lv_obj_set_style_bg_opa(glow, LV_OPA_COVER, 0);
  lv_obj_set_style_opa(glow, LV_OPA_30, 0);
  lv_obj_remove_flag(glow, LV_OBJ_FLAG_CLICKABLE);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, glow);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_duration(&a, 1100);
    lv_anim_set_reverse_duration(&a, 1100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_glow);
    lv_anim_start(&a);
  }

  const lv_color_t ring_colors[3] = {lv_color_hex(0xffffff), theme::gold(), theme::mint()};
  const int32_t ring_delays[3] = {0, 420, 840};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t * ring = lv_obj_create(stage);
    lv_obj_remove_style_all(ring);
    const int32_t start = (kRing * 60) / 100;
    const int32_t end = kRing * 3;
    lv_obj_set_size(ring, start, start);
    lv_obj_set_pos(ring, kStage / 2 - start / 2, kStage / 2 - start / 2);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 4, 0);
    lv_obj_set_style_border_color(ring, ring_colors[i], 0);
    lv_obj_set_style_border_opa(ring, (lv_opa_t)(255 * 85 / 100), 0);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    lv_anim_t as;
    lv_anim_init(&as);
    lv_anim_set_var(&as, ring);
    lv_anim_set_values(&as, start, end);
    lv_anim_set_duration(&as, 1250);
    lv_anim_set_delay(&as, ring_delays[i]);
    lv_anim_set_repeat_count(&as, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&as, lv_anim_path_linear);
    lv_anim_set_exec_cb(&as, anim_ring_size);
    lv_anim_start(&as);

    lv_anim_t ao;
    lv_anim_init(&ao);
    lv_anim_set_var(&ao, ring);
    lv_anim_set_values(&ao, (int32_t)(255 * 85 / 100), LV_OPA_TRANSP);
    lv_anim_set_duration(&ao, 1250);
    lv_anim_set_delay(&ao, ring_delays[i]);
    lv_anim_set_repeat_count(&ao, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&ao, lv_anim_path_linear);
    lv_anim_set_exec_cb(&ao, anim_ring_opa);
    lv_anim_start(&ao);
  }

  lv_obj_t * body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, WP_HOR_RES, WP_VER_RES - kDockH);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 8, 0);
  lv_obj_set_style_pad_top(body, 28, 0);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(body);

  lv_obj_t * eye = lv_label_create(body);
  lv_label_set_text(eye, "TIMER");
  lv_obj_set_style_text_color(eye, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(eye, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_letter_space(eye, 4, 0);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, eye);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
    lv_anim_set_duration(&a, 250);
    lv_anim_set_playback_duration(&a, 250);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_blink_opa);
    lv_anim_start(&a);
  }

  constexpr lv_coord_t kEmoji = 48;
  lv_obj_t * emoji_img = make_emoji_image(body, "\xE2\x8F\xB0", kEmoji); /* ⏰ */
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, emoji_img);
    const int32_t base = (kEmoji * 256) / 72;
    lv_anim_set_values(&a, base, (base * 112) / 100);
    lv_anim_set_duration(&a, 900);
    lv_anim_set_playback_duration(&a, 900);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_emoji_pop);
    lv_anim_start(&a);
  }

  lv_obj_t * title = lv_label_create(body);
  lv_label_set_text(title, "Time's up");
  lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(title, font_display(56), 0);
  lv_obj_set_style_text_letter_space(title, 2, 0);

  lv_obj_t * msg = lv_label_create(body);
  lv_label_set_text(msg, "Your desk timer finished");
  lv_obj_set_style_text_color(msg, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * dock = make_dock(scr);
  lv_obj_t * dismiss = dock_btn(dock, "Dismiss", true, false, on_alarm_dismiss);
  lv_obj_set_style_radius(dismiss, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dismiss, lv_color_hex(0xf0c24b), 0);
  lv_obj_set_height(dismiss, 48);

  return scr;
}

lv_obj_t * timer_setup_screen() {
  lv_obj_t * scr = make_screen(theme::BgWash::Page);
  lv_obj_add_event_cb(scr, on_deleted, LV_EVENT_DELETE, nullptr);
  make_topbar(scr, "TIMER", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 10, 0);

  g_status_lbl = lv_label_create(body);
  lv_label_set_text(g_status_lbl, "Desk timer");
  lv_obj_set_style_text_color(g_status_lbl, theme::muted(), 0);
  lv_obj_set_style_text_font(g_status_lbl, &lv_font_montserrat_14, 0);

  g_time_lbl = lv_label_create(body);
  lv_obj_set_style_text_color(g_time_lbl, theme::ink(), 0);
  lv_obj_set_style_text_font(g_time_lbl, font_display(72), 0);
  lv_obj_set_style_text_letter_space(g_time_lbl, 2, 0);
  lv_obj_add_flag(g_time_lbl, LV_OBJ_FLAG_HIDDEN);

  g_picker = lv_obj_create(body);
  lv_obj_remove_style_all(g_picker);
  lv_obj_set_width(g_picker, lv_pct(100));
  lv_obj_set_height(g_picker, 150);
  lv_obj_set_flex_flow(g_picker, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(g_picker, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(g_picker, 10, 0);
  lv_obj_remove_flag(g_picker, LV_OBJ_FLAG_SCROLLABLE);

  auto make_col = [](lv_obj_t * parent, const char * unit) {
    lv_obj_t * col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 4, 0);
    lv_obj_t * u = lv_label_create(col);
    lv_label_set_text(u, unit);
    lv_obj_set_style_text_color(u, theme::muted(), 0);
    lv_obj_set_style_text_font(u, &lv_font_montserrat_12, 0);
    return col;
  };

  lv_obj_t * min_col = make_col(g_picker, "min");
  g_roller_min = lv_roller_create(min_col);
  lv_roller_set_options(g_roller_min, min_options(), LV_ROLLER_MODE_NORMAL);
  style_roller(g_roller_min);
  lv_obj_add_event_cb(g_roller_min, on_roller, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t * colon = lv_label_create(g_picker);
  lv_label_set_text(colon, ":");
  lv_obj_set_style_text_color(colon, theme::gold(), 0);
  lv_obj_set_style_text_font(colon, font_display(40), 0);

  lv_obj_t * sec_col = make_col(g_picker, "sec");
  g_roller_sec = lv_roller_create(sec_col);
  lv_roller_set_options(g_roller_sec, sec_options(), LV_ROLLER_MODE_NORMAL);
  style_roller(g_roller_sec);
  lv_obj_add_event_cb(g_roller_sec, on_roller, LV_EVENT_VALUE_CHANGED, nullptr);

  g_preset_row = lv_obj_create(body);
  lv_obj_remove_style_all(g_preset_row);
  lv_obj_set_width(g_preset_row, lv_pct(100));
  lv_obj_set_height(g_preset_row, 40);
  lv_obj_set_flex_flow(g_preset_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(g_preset_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(g_preset_row, 6, 0);

  g_primary_btn = lv_button_create(body);
  lv_obj_set_size(g_primary_btn, 200, 48);
  lv_obj_set_style_radius(g_primary_btn, 16, 0);
  lv_obj_set_style_shadow_width(g_primary_btn, 0, 0);
  g_primary_lbl = lv_label_create(g_primary_btn);
  lv_label_set_text(g_primary_lbl, "Start");
  lv_obj_set_style_text_font(g_primary_lbl, &lv_font_montserrat_16, 0);
  lv_obj_center(g_primary_lbl);
  lv_obj_add_event_cb(
      g_primary_btn,
      [](lv_event_t * /*e*/) {
        const auto st = desk_timer::state();
        if (st == desk_timer::State::Running) desk_timer::pause();
        else {
          if (st == desk_timer::State::Idle) apply_rollers_to_duration();
          desk_timer::start();
        }
        refresh_ui();
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Reset", false, false, [](lv_event_t * /*e*/) {
    desk_timer::reset();
    sync_rollers_from_duration();
    refresh_ui();
  });
  dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });

  desk_timer::set_listener(on_listen);
  refresh_ui();
  return scr;
}

}  // namespace

lv_obj_t * timer_screen() {
  if (desk_timer::is_finished()) return timer_alarm_screen();
  return timer_setup_screen();
}

}  // namespace ui
}  // namespace wp
