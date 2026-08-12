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

/** Rebuild the active game board without swapping LVGL screens (avoids RGB flash). */
void game_reload_inplace();

/** Sim/QA helper: load a game screen and force a panel ("play" | "wait" | "invite"). */
void games_debug_show(const char * game, const char * panel);

/** Accept an incoming invite slot from Active Games (focuses, sends Accept, opens board). */
bool accept_incoming_slot(int idx);

}  // namespace ui
}  // namespace wp
