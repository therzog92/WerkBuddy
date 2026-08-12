#pragma once
/* Slim chrome helpers ported from firmware/src/ui (sim) for device TTT + games. */

#include <lvgl.h>
#include <cstdint>

namespace wp {
namespace dui {

constexpr int kHor = 480;
constexpr int kVer = 480;
constexpr int kTopbarH = 64;
constexpr int kDockH = 52;

lv_color_t bg0();
lv_color_t panel();
lv_color_t ink();
lv_color_t muted();
lv_color_t gold();
lv_color_t hot();
lv_color_t mint();
lv_color_t danger();
lv_color_t border();
lv_color_t grad_bot();

lv_obj_t * make_screen();
lv_obj_t * make_topbar(lv_obj_t * scr, const char * title, const char * me, const char * sub = nullptr);
lv_obj_t * make_body(lv_obj_t * scr, bool with_dock = true);
lv_obj_t * make_dock(lv_obj_t * scr);
lv_obj_t * dock_btn(lv_obj_t * dock, const char * label, bool primary, bool danger, lv_event_cb_t cb,
                    void * user_data = nullptr);
lv_obj_t * make_status(lv_obj_t * parent, const char * text);
lv_obj_t * make_tagline(lv_obj_t * parent, const char * text);
lv_obj_t * make_wait_block(lv_obj_t * parent, const char * eye, const char * name, const char * sub);
lv_obj_t * make_peer_btn(lv_obj_t * parent, const char * name, lv_event_cb_t cb, void * user_data);

void ttt_draw_mark(lv_obj_t * parent, char mark, int size);
void attach_result_overlay(lv_obj_t * parent, int outcome /*1 win, 0 lose, -1 draw*/,
                           lv_event_cb_t on_dismiss);
void show_forfeit_confirm(lv_event_cb_t on_yes);

}  // namespace dui
}  // namespace wp
