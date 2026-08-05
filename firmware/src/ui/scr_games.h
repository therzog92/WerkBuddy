#pragma once

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {

lv_obj_t * games_folder_screen();
lv_obj_t * game_ttt_screen();
lv_obj_t * game_sttt_screen();
lv_obj_t * game_c4_screen();
lv_obj_t * game_bs_screen();
lv_obj_t * game_ck_screen();
lv_obj_t * game_mem_screen();
lv_obj_t * game_rv_screen();
lv_obj_t * game_db_screen();

/** Sim/QA helper: load a game screen and force a panel ("play" | "wait" | "invite"). */
void games_debug_show(const char * game, const char * panel);

}  // namespace ui
}  // namespace wp
