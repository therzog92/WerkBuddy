#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace app {
struct Peer;
}
namespace ui {

lv_obj_t * pager_werk_screen();
lv_obj_t * pager_compose_screen(const app::Peer & peer);
lv_obj_t * pager_outgoing_screen(); /* reads desk().outgoing */
lv_obj_t * pager_incoming_screen(); /* reads desk().incoming */

/** Fresh open from peer list — reset emoji/message draft. */
void compose_mark_fresh();
const char * compose_message();
void compose_set_message(const char * msg);

}  // namespace ui
}  // namespace wp
