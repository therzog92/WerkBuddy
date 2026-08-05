#include "ui/scr_setup.h"

#include "app/app.h"
#include "ui/brightness.h"
#include "ui/chrome.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cstdio>

namespace wp {
namespace ui {
namespace {

void add_theme_swatches(lv_obj_t * parent) {
  lv_obj_t * themes = lv_obj_create(parent);
  lv_obj_remove_style_all(themes);
  lv_obj_set_width(themes, lv_pct(100));
  lv_obj_set_height(themes, 44);
  lv_obj_set_flex_flow(themes, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(themes, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(themes, 6, 0);
  lv_obj_set_scroll_dir(themes, LV_DIR_HOR);
  lv_obj_add_flag(themes, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < (int)theme::Id::Count; ++i) {
    const auto id = static_cast<theme::Id>(i);
    lv_obj_t * s = lv_obj_create(themes);
    lv_obj_remove_style_all(s);
    lv_obj_set_size(s, 40, 36);
    lv_obj_set_style_radius(s, 10, 0);
    theme::Id prev = theme::current();
    theme::set(id);
    lv_obj_set_style_bg_color(s, theme::hot(), 0);
    lv_obj_set_style_bg_grad_color(s, theme::gold(), 0);
    theme::set(prev);
    lv_obj_set_style_bg_grad_dir(s, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    const bool sel = (app::desk().theme == (uint8_t)i);
    lv_obj_set_style_border_width(s, sel ? 2 : 0, 0);
    lv_obj_set_style_border_color(s, theme::ink(), 0);
    lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        s,
        [](lv_event_t * e) {
          const int tid = (int)(intptr_t)lv_event_get_user_data(e);
          app::desk().theme = (uint8_t)tid;
          theme::set(static_cast<theme::Id>(tid));
          app::save();
          go_setup();
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }
}

}  // namespace

lv_obj_t * setup_screen() {
  app::Desk & d = app::desk();
  theme::set(static_cast<theme::Id>(d.theme));
  brightness::apply();

  lv_obj_t * scr = make_screen();
  make_topbar(scr, "SETUP", "WerkBuddy");
  lv_obj_t * body = make_body(scr, true);
  make_tagline(body, "Pick a name and a look");

  lv_obj_t * name_lbl = lv_label_create(body);
  lv_label_set_text(name_lbl, "Your name");
  lv_obj_set_style_text_color(name_lbl, theme::muted(), 0);
  lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, 0);

  lv_obj_t * name = lv_button_create(body);
  lv_obj_set_width(name, lv_pct(100));
  lv_obj_set_height(name, 48);
  style_peer_like(name);
  lv_obj_t * nl = lv_label_create(name);
  lv_label_set_text(nl, d.name[0] ? d.name : "Tap to enter name");
  lv_obj_set_style_text_color(nl, lv_color_hex(0x1a0a12), 0);
  lv_obj_center(nl);
  lv_obj_add_event_cb(name, [](lv_event_t * /*e*/) { go_keyboard_setup_name(); }, LV_EVENT_CLICKED,
                      nullptr);

  lv_obj_t * theme_lbl = lv_label_create(body);
  lv_label_set_text(theme_lbl, "Theme");
  lv_obj_set_style_text_color(theme_lbl, theme::muted(), 0);
  lv_obj_set_style_text_font(theme_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_pad_top(theme_lbl, 12, 0);

  add_theme_swatches(body);

  char theme_name[32];
  lv_snprintf(theme_name, sizeof(theme_name), "%s", theme::name(static_cast<theme::Id>(d.theme)));
  lv_obj_t * tname = lv_label_create(body);
  lv_label_set_text(tname, theme_name);
  lv_obj_set_style_text_color(tname, theme::gold(), 0);
  lv_obj_set_style_text_font(tname, &lv_font_montserrat_14, 0);

  lv_obj_t * dock = make_dock(scr);
  dock_btn(
      dock, "Continue", true, false,
      [](lv_event_t * /*e*/) {
        app::Desk & desk = app::desk();
        if (!desk.name[0]) {
          toast("Enter your name first");
          return;
        }
        desk.setup_done = true;
        app::save();
        go_hub();
      });
  return scr;
}

}  // namespace ui
}  // namespace wp
