#include "ui/chrome.h"

#include "ui/theme.h"

namespace wp {
namespace ui {
namespace {

lv_obj_t * g_confirm = nullptr;
lv_obj_t * g_toast = nullptr;
lv_timer_t * g_toast_timer = nullptr;

}  // namespace

void toast(const char * text) {
  if (g_toast) {
    lv_obj_delete(g_toast);
    g_toast = nullptr;
  }
  if (g_toast_timer) {
    lv_timer_delete(g_toast_timer);
    g_toast_timer = nullptr;
  }
  g_toast = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g_toast);
  lv_obj_set_size(g_toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(g_toast, lv_color_hex(0x241a30), 0);
  lv_obj_set_style_bg_opa(g_toast, 240, 0);
  lv_obj_set_style_radius(g_toast, 18, 0);
  lv_obj_set_style_pad_hor(g_toast, 18, 0);
  lv_obj_set_style_pad_ver(g_toast, 10, 0);
  lv_obj_set_style_border_width(g_toast, 1, 0);
  lv_obj_set_style_border_color(g_toast, theme::border(), 0);
  lv_obj_align(g_toast, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_remove_flag(g_toast, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * l = lv_label_create(g_toast);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_color(l, theme::ink(), 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);

  g_toast_timer = lv_timer_create(
      [](lv_timer_t * /*t*/) {
        /* repeat_count=1 → LVGL deletes the timer itself after this runs */
        if (g_toast) {
          lv_obj_delete(g_toast);
          g_toast = nullptr;
        }
        g_toast_timer = nullptr;
      },
      2200, nullptr);
  lv_timer_set_repeat_count(g_toast_timer, 1);
}

void toast_fmt(const char * fmt, const char * arg) {
  char buf[96];
  lv_snprintf(buf, sizeof(buf), fmt, arg);
  toast(buf);
}

lv_obj_t * make_screen(theme::BgWash wash) {
  lv_obj_t * scr = lv_obj_create(nullptr);
  theme::apply_screen_bg(scr, wash);
  return scr;
}

lv_obj_t * make_topbar(lv_obj_t * scr, const char * title, const char * me, const char * sub) {
  lv_obj_t * top = lv_obj_create(scr);
  lv_obj_remove_style_all(top);
  lv_obj_set_size(top, WP_HOR_RES, kTopbarH);
  lv_obj_set_style_pad_hor(top, 14, 0);
  lv_obj_set_style_pad_top(top, 10, 0);
  lv_obj_set_style_pad_bottom(top, 6, 0);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);

  lv_obj_t * left = lv_obj_create(top);
  lv_obj_remove_style_all(left);
  lv_obj_set_height(left, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(left, 1);
  lv_obj_set_width(left, LV_PCT(70));
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(left, 0, 0);
  lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * brand = lv_label_create(left);
  lv_label_set_text(brand, title);
  lv_obj_set_style_text_color(brand, theme::gold(), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
  lv_label_set_long_mode(brand, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(brand, lv_pct(100));
  lv_obj_set_user_data(brand, (void *)1); /* mark as title */

  lv_obj_t * sub_lbl = lv_label_create(left);
  if (sub && sub[0]) {
    lv_label_set_text(sub_lbl, sub);
    lv_obj_remove_flag(sub_lbl, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(sub_lbl, "");
    lv_obj_add_flag(sub_lbl, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_style_text_color(sub_lbl, theme::muted(), 0);
  lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_12, 0);
  lv_label_set_long_mode(sub_lbl, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(sub_lbl, lv_pct(100));
  lv_obj_set_user_data(sub_lbl, (void *)2);

  lv_obj_t * right = lv_obj_create(top);
  lv_obj_remove_style_all(right);
  lv_obj_set_height(right, LV_SIZE_CONTENT);
  lv_obj_set_width(right, LV_SIZE_CONTENT);
  lv_obj_set_style_min_width(right, 96, 0);
  lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_row(right, 2, 0);
  lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_user_data(right, (void *)3); /* mark as right column */

  lv_obj_t * me_lbl = lv_label_create(right);
  lv_label_set_text(me_lbl, me ? me : "Tommy");
  lv_obj_set_style_text_color(me_lbl, theme::muted(), 0);
  lv_obj_set_style_text_font(me_lbl, &lv_font_montserrat_14, 0);

  lv_obj_t * me_sub = lv_obj_create(right);
  lv_obj_remove_style_all(me_sub);
  lv_obj_set_size(me_sub, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(me_sub, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(me_sub, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(me_sub, 4, 0);
  lv_obj_add_flag(me_sub, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(me_sub, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_user_data(me_sub, (void *)4); /* mark badge slot */

  lv_obj_set_user_data(top, left);
  return top;
}

void topbar_set(lv_obj_t * topbar, const char * title, const char * sub) {
  lv_obj_t * left = static_cast<lv_obj_t *>(lv_obj_get_user_data(topbar));
  if (!left) return;
  const uint32_t n = lv_obj_get_child_count(left);
  for (uint32_t i = 0; i < n; ++i) {
    lv_obj_t * c = lv_obj_get_child(left, i);
    const intptr_t tag = reinterpret_cast<intptr_t>(lv_obj_get_user_data(c));
    if (tag == 1 && title) lv_label_set_text(c, title);
    if (tag == 2) {
      if (sub && sub[0]) {
        lv_label_set_text(c, sub);
        lv_obj_remove_flag(c, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(c, "");
        lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

void topbar_set_mark(lv_obj_t * topbar, char mark) {
  if (!topbar) return;
  lv_obj_t * slot = nullptr;
  const uint32_t n = lv_obj_get_child_count(topbar);
  for (uint32_t i = 0; i < n; ++i) {
    lv_obj_t * c = lv_obj_get_child(topbar, i);
    if (reinterpret_cast<intptr_t>(lv_obj_get_user_data(c)) != 3) continue;
    const uint32_t rn = lv_obj_get_child_count(c);
    for (uint32_t j = 0; j < rn; ++j) {
      lv_obj_t * r = lv_obj_get_child(c, j);
      if (reinterpret_cast<intptr_t>(lv_obj_get_user_data(r)) == 4) {
        slot = r;
        break;
      }
    }
  }
  if (!slot) return;
  lv_obj_clean(slot);
  if (!mark) {
    lv_obj_add_flag(slot, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(slot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t * you = lv_label_create(slot);
  lv_label_set_text(you, "you are");
  lv_obj_set_style_text_color(you, theme::muted(), 0);
  lv_obj_set_style_text_font(you, &lv_font_montserrat_12, 0);
  lv_obj_t * badge = lv_obj_create(slot);
  lv_obj_remove_style_all(badge);
  lv_obj_set_size(badge, 22, 22);
  lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(badge, theme::hot(), 0);
  lv_obj_set_style_bg_grad_color(badge, theme::gold(), 0);
  lv_obj_set_style_bg_grad_dir(badge, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * bl = lv_label_create(badge);
  char mk[2] = {mark, 0};
  lv_label_set_text(bl, mk);
  lv_obj_set_style_text_color(bl, lv_color_hex(0x1a0610), 0);
  lv_obj_set_style_text_font(bl, &lv_font_montserrat_12, 0);
  lv_obj_center(bl);
}

lv_obj_t * make_body(lv_obj_t * scr, bool with_dock) {
  lv_obj_t * body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  const int h = WP_VER_RES - kTopbarH - (with_dock ? kDockH : 0);
  lv_obj_set_size(body, WP_HOR_RES, h);
  lv_obj_set_pos(body, 0, kTopbarH);
  lv_obj_set_style_pad_hor(body, 14, 0);
  lv_obj_set_style_pad_top(body, 6, 0);
  /* Extra bottom pad so the last row isn't clipped under the dock edge. */
  lv_obj_set_style_pad_bottom(body, with_dock ? 16 : 6, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(body, 8, 0);
  /* Keep clickable so the page can scroll; children still win hit-tests when clickable. */
  lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(body, LV_DIR_VER);
  return body;
}

lv_obj_t * make_dock(lv_obj_t * scr) {
  lv_obj_t * dock = lv_obj_create(scr);
  lv_obj_remove_style_all(dock);
  lv_obj_set_size(dock, WP_HOR_RES, kDockH);
  lv_obj_align(dock, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_pad_hor(dock, 12, 0);
  lv_obj_set_style_pad_ver(dock, 6, 0);
  /* Transparent dock — buttons float over the page content. */
  lv_obj_set_style_bg_opa(dock, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(dock, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(dock, 8, 0);
  lv_obj_remove_flag(dock, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(dock, LV_OBJ_FLAG_SCROLLABLE);
  /* Keep dock above body if layouts overlap. */
  lv_obj_move_foreground(dock);
  return dock;
}

void style_peer_like(lv_obj_t * btn) {
  /* Flat solid pill — no gradient. */
  lv_obj_set_style_radius(btn, 16, 0);
  lv_obj_set_style_bg_color(btn, theme::gold(), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_pad_ver(btn, 12, 0);
  lv_obj_set_style_pad_hor(btn, 16, 0);
}

lv_obj_t * dock_btn(lv_obj_t * dock, const char * label, bool primary, bool danger,
                    lv_event_cb_t cb, void * user_data) {
  lv_obj_t * btn = lv_button_create(dock);
  lv_obj_set_height(btn, 40);
  lv_obj_set_flex_grow(btn, 1);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(btn, 8);

  if (danger) {
    lv_obj_set_style_bg_color(btn, theme::danger(), 0);
  } else if (primary) {
    lv_obj_set_style_bg_color(btn, theme::gold(), 0);
  } else {
    /* Flat mid-grey like the reference +/- controls */
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x4a4558), 0);
  }
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

  lv_obj_t * lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_color(lbl, primary && !danger ? lv_color_hex(0x1a1224) : theme::ink(), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);
  lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  return btn;
}

lv_obj_t * make_tagline(lv_obj_t * parent, const char * text) {
  lv_obj_t * t = lv_label_create(parent);
  lv_label_set_text(t, text);
  lv_obj_set_style_text_color(t, theme::muted(), 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
  lv_obj_set_width(t, lv_pct(100));
  return t;
}

lv_obj_t * make_peer_btn(lv_obj_t * parent, const char * name, const char * subtitle,
                         lv_event_cb_t cb, void * user_data) {
  lv_obj_t * btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_width(btn, lv_pct(100));
  lv_obj_set_height(btn, LV_SIZE_CONTENT);
  style_peer_like(btn);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(btn, 2, 0);

  lv_obj_t * n = lv_label_create(btn);
  lv_label_set_text(n, name);
  lv_obj_set_style_text_color(n, lv_color_hex(0x1a0a12), 0);
  const bool has_sub = subtitle && subtitle[0];
  lv_obj_set_style_text_font(n, has_sub ? &lv_font_montserrat_16 : &lv_font_montserrat_20, 0);
  lv_obj_remove_flag(n, LV_OBJ_FLAG_CLICKABLE);

  if (has_sub) {
    lv_obj_t * s = lv_label_create(btn);
    lv_label_set_text(s, subtitle);
    lv_obj_set_style_text_color(s, lv_color_hex(0x3a2030), 0);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_set_style_pad_ver(btn, 16, 0);
  }

  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  return btn;
}

void hide_confirm() {
  if (g_confirm) {
    lv_obj_delete(g_confirm);
    g_confirm = nullptr;
  }
}

void hide_forfeit_confirm() { hide_confirm(); }

void show_confirm(const char * message, const char * yes_label, bool yes_danger,
                  lv_event_cb_t on_yes, lv_event_cb_t on_no, const char * no_label) {
  hide_confirm();
  lv_obj_t * layer = lv_obj_create(lv_layer_top());
  g_confirm = layer;
  lv_obj_remove_style_all(layer);
  lv_obj_set_size(layer, WP_HOR_RES, WP_VER_RES);
  lv_obj_set_style_bg_color(layer, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(layer, LV_OPA_70, 0);
  lv_obj_add_flag(layer, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * card = lv_obj_create(layer);
  lv_obj_set_width(card, 340);
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_center(card);
  lv_obj_set_style_radius(card, 16, 0);
  lv_obj_set_style_bg_color(card, theme::panel(), 0);
  lv_obj_set_style_border_color(card, theme::border(), 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_pad_all(card, 16, 0);
  lv_obj_set_style_pad_row(card, 14, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * msg = lv_label_create(card);
  lv_label_set_text(msg, message && message[0] ? message : "Are you sure?");
  lv_obj_set_style_text_color(msg, theme::ink(), 0);
  lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(msg, 300);

  lv_obj_t * row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 300, 44);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  auto wrap_no = [](lv_event_t * e) {
    auto * user_cb = reinterpret_cast<lv_event_cb_t>(lv_event_get_user_data(e));
    hide_confirm();
    if (user_cb) user_cb(e);
  };
  auto wrap_yes = [](lv_event_t * e) {
    auto * user_cb = reinterpret_cast<lv_event_cb_t>(lv_event_get_user_data(e));
    hide_confirm();
    if (user_cb) user_cb(e);
  };

  dock_btn(row, no_label ? no_label : "Cancel", false, false, wrap_no, (void *)on_no);
  dock_btn(row, yes_label && yes_label[0] ? yes_label : "OK", false, yes_danger, wrap_yes,
           (void *)on_yes);
}

void show_forfeit_confirm(lv_event_cb_t on_yes, lv_event_cb_t on_no) {
  show_confirm("Are you sure you want to forfeit?", "Forfeit", true, on_yes, on_no, nullptr);
}

}  // namespace ui
}  // namespace wp
