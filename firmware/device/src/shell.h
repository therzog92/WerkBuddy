#pragma once

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

namespace wp {
namespace shell {

void start();
void tick();

/** Used by TTT / screens that need hub navigation + toasts. */
void go_hub();
void show_toast(const char * text);
void load_screen(lv_obj_t * scr); /* screen swap — deletes previous */
bool game_busy(); /* pager call or TTT in progress — blocks idle */

}  // namespace shell
}  // namespace wp
