#pragma once

#include "lvgl/lvgl.h"

#include "protocol/messages.h"

namespace wp {
namespace ui {

lv_obj_t * doodle_screen();

/** Draw a remote stroke chunk (call after go_doodle when off-screen). */
void doodle_apply_remote_stroke(const proto::Msg & msg);
void doodle_remote_clear(const char * from_id);

/** Sim/QA helper: jump straight to the draw view. */
void doodle_debug_show_draw();

}  // namespace ui
}  // namespace wp
