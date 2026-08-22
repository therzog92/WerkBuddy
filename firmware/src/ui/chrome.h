#pragma once

#include "lvgl/lvgl.h"
#include "ui/theme.h"

#include <functional>

namespace wp {
namespace ui {

constexpr int kTopbarH = 64;
constexpr int kDockH = 56;
constexpr int kDockCompactH = 44;

lv_obj_t * make_screen(theme::BgWash wash = theme::BgWash::Page);
lv_obj_t * make_topbar(lv_obj_t * scr, const char * title, const char * me = "Tommy",
                       const char * sub = nullptr);
void topbar_set(lv_obj_t * topbar, const char * title, const char * sub = nullptr);
/** Show "you are" + mark badge under the player name (Super TTT chrome). */
void topbar_set_mark(lv_obj_t * topbar, char mark);

/** Content between topbar and dock. */
lv_obj_t * make_body(lv_obj_t * scr, bool with_dock = true);

lv_obj_t * make_dock(lv_obj_t * scr);
lv_obj_t * dock_btn(lv_obj_t * dock, const char * label, bool primary, bool danger,
                    lv_event_cb_t cb, void * user_data = nullptr);

lv_obj_t * make_tagline(lv_obj_t * parent, const char * text);
lv_obj_t * make_peer_btn(lv_obj_t * parent, const char * name, const char * subtitle,
                         lv_event_cb_t cb, void * user_data = nullptr);

/** Hot→gold gradient pill button (peer / primary list style). */
void style_peer_like(lv_obj_t * btn);

void show_forfeit_confirm(lv_event_cb_t on_yes, lv_event_cb_t on_no = nullptr);
/** Generic yes/no overlay (forfeit, clear history, etc.). */
void show_confirm(const char * message, const char * yes_label, bool yes_danger,
                  lv_event_cb_t on_yes, lv_event_cb_t on_no = nullptr, const char * no_label = "Cancel");
void hide_forfeit_confirm();
void hide_confirm();

/** Web-style toast: bottom pill, auto-hides after ~2.2s. */
void toast(const char * text);
void toast_fmt(const char * fmt, const char * arg);

}  // namespace ui
}  // namespace wp
