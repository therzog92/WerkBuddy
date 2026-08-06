#include "SDL2/SDL.h"

#include "lvgl/lvgl.h"

#include "app/active_games.h"
#include "app/app.h"
#include "app/checklist.h"
#include "app/desk_timer.h"
#include "app/page_log.h"
#include "sim/driver.h"
#include "sim/screenshot.h"
#include "ui/brightness.h"
#include "ui/nav.h"
#include "ui/scr_doodle.h"
#include "ui/scr_games.h"
#include "ui/theme.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef WP_HOR_RES
#define WP_HOR_RES 480
#endif
#ifndef WP_VER_RES
#define WP_VER_RES 480
#endif

static void f12_poll_cb(lv_timer_t * /*t*/) {
  const Uint8 * keys = SDL_GetKeyboardState(nullptr);
  static bool was_down = false;
  const bool down = keys[SDL_SCANCODE_F12] != 0;
  if (down && !was_down) {
    wp::sim::save_preview_png();
  }
  was_down = down;
}

int main() {
  lv_init();

  lv_display_t * disp = lv_sdl_window_create(WP_HOR_RES, WP_VER_RES);
  if (!disp) {
    std::fprintf(stderr, "Failed to create SDL window\n");
    return 1;
  }
  lv_sdl_window_set_title(disp, "WerkBuddy LVGL sim (480x480)");

  lv_indev_t * mouse = lv_sdl_mouse_create();
  if (mouse) lv_indev_set_display(mouse, disp);
  lv_indev_t * wheel = lv_sdl_mousewheel_create();
  if (wheel) lv_indev_set_display(wheel, disp);
  lv_indev_t * kb = lv_sdl_keyboard_create();
  if (kb) lv_indev_set_display(kb, disp);

  wp::sim::driver_init(disp);

  wp::app::init();
  wp::desk_timer::init();
  wp::theme::set(static_cast<wp::theme::Id>(wp::app::desk().theme));
  wp::ui::brightness::init();
  wp::ui::idle_init();

  const char * scr_name = std::getenv("WERKPAGER_SCREEN");
  const bool qa_jump = scr_name && scr_name[0];
  const char * shot = std::getenv("WERKPAGER_SHOT");
  const bool auto_shot = shot && shot[0] == '1';

  /* Normal boot: splash → setup (first run) or hub. QA env vars skip splash. */
  if (!qa_jump && !auto_shot) wp::ui::go_splash();
  else if (!wp::app::desk().setup_done) wp::ui::go_setup();
  else wp::ui::go_hub();

  /* WERKPAGER_SCREEN=<name> — jump to any screen for visual QA / README shots. */
  if (qa_jump) {
    using namespace wp::ui;
    using namespace wp::app;
    auto is = [scr_name](const char * v) { return std::strcmp(scr_name, v) == 0; };
    if (is("splash")) go_splash();
    else if (is("hub") || is("home")) go_hub();
    else if (is("setup")) go_setup();
    else if (is("idle")) go_idle();
    else if (is("werk")) go_werk();
    else if (is("compose")) {
      if (desk().peer_count > 0) go_compose(desk().peers[0]);
      else go_werk();
    }
    else if (is("outgoing")) {
      Desk & d = desk();
      d.outgoing.active = true;
      std::snprintf(d.outgoing.to_id, sizeof(d.outgoing.to_id), "mac-will");
      std::snprintf(d.outgoing.to_name, sizeof(d.outgoing.to_name), "Will");
      std::snprintf(d.outgoing.emoji, sizeof(d.outgoing.emoji), "👑");
      std::snprintf(d.outgoing.message, sizeof(d.outgoing.message), "Got a sec?");
      go_outgoing();
    }
    else if (is("incoming") || is("incoming-demo")) {
      Desk & d = desk();
      d.incoming.active = true;
      std::snprintf(d.incoming.from_id, sizeof(d.incoming.from_id), "mac-will");
      std::snprintf(d.incoming.from_name, sizeof(d.incoming.from_name), "Will");
      std::snprintf(d.incoming.emoji, sizeof(d.incoming.emoji), "👑");
      std::snprintf(d.incoming.message, sizeof(d.incoming.message), "Lipsync for your life");
      go_incoming();
    }
    else if (is("page-history") || is("history")) {
      wp::page_log::clear();
      wp::page_log::add(wp::page_log::Dir::Out, "Will", "👑", "Got a sec?");
      wp::page_log::add(wp::page_log::Dir::In, "Will", "💅", "Lipsync for your life");
      wp::page_log::add(wp::page_log::Dir::Out, "Alex", "☕", "Coffee run?");
      wp::page_log::add(wp::page_log::Dir::In, "Alex", "👀", "Coming down");
      wp::page_log::add(wp::page_log::Dir::Out, "Will", "✨", "You busy?");
      go_page_history();
    }
    else if (is("gamesfolder") || is("games")) go_games_folder();
    else if (is("active-games") || is("active") || is("hub-active")) {
      using namespace wp::app;
      clear_all_games();
      /* Pending Invites: incoming + outgoing wait */
      {
        const int idx = alloc_slot(GameKind::C4);
        if (idx >= 0) {
          GameSlot * s = slot_at(idx);
          s->invite_pending = true;
          s->invite.active = true;
          std::snprintf(s->invite.from_id, sizeof(s->invite.from_id), "mac-will");
          std::snprintf(s->invite.from_name, sizeof(s->invite.from_name), "Will");
          s->invite.color = 0;
        }
      }
      if (begin_match(GameKind::Sttt, "mac-alex")) {
        sttt() = {};
        sttt().active = true;
        sttt().waiting = true;
        sttt().mark = 'X';
        std::snprintf(sttt().opp_id, sizeof(sttt().opp_id), "mac-alex");
        std::snprintf(sttt().opp_name, sizeof(sttt().opp_name), "Alex");
      }
      /* Your Turn */
      if (begin_match(GameKind::Ttt, "mac-will")) {
        ttt() = {};
        ttt().active = true;
        ttt().mark = 'X';
        ttt().turn = 'X';
        ttt().board[0] = 'X';
        ttt().board[4] = 'O';
        std::snprintf(ttt().opp_id, sizeof(ttt().opp_id), "mac-will");
        std::snprintf(ttt().opp_name, sizeof(ttt().opp_name), "Will");
        note_turn_start(focus_index());
      }
      if (begin_match(GameKind::C4, "mac-alex")) {
        c4() = {};
        c4().active = true;
        c4().my_color = 0;
        c4().turn = 0;
        std::snprintf(c4().opp_id, sizeof(c4().opp_id), "mac-alex");
        std::snprintf(c4().opp_name, sizeof(c4().opp_name), "Alex");
        note_turn_start(focus_index());
      }
      /* Active Games (their turn / in play) */
      if (begin_match(GameKind::Ttt, "mac-alex")) {
        ttt() = {};
        ttt().active = true;
        ttt().mark = 'O';
        ttt().turn = 'X';
        ttt().board[1] = 'X';
        std::snprintf(ttt().opp_id, sizeof(ttt().opp_id), "mac-alex");
        std::snprintf(ttt().opp_name, sizeof(ttt().opp_name), "Alex");
        note_turn_start(focus_index());
      }
      if (begin_match(GameKind::Rv, "mac-will")) {
        rv() = {};
        rv().active = true;
        rv().my_color = 0;
        rv().turn = 1;
        std::snprintf(rv().opp_id, sizeof(rv().opp_id), "mac-will");
        std::snprintf(rv().opp_name, sizeof(rv().opp_name), "Will");
        note_turn_start(focus_index());
      }
      if (is("hub-active")) go_hub();
      else go_active_games();
    }
    else if (is("ttt")) go_ttt();
    else if (is("ttt-play")) games_debug_show("ttt", "play");
    else if (is("ttt-win")) games_debug_show("ttt", "win");
    else if (is("ttt-lose")) games_debug_show("ttt", "lose");
    else if (is("sttt")) go_sttt();
    else if (is("sttt-play")) games_debug_show("sttt", "play");
    else if (is("c4")) go_c4();
    else if (is("c4-play")) games_debug_show("c4", "play");
    else if (is("c4-win")) games_debug_show("c4", "win");
    else if (is("bs")) go_battleship();
    else if (is("bs-setup")) games_debug_show("bs", "setup");
    else if (is("bs-play")) games_debug_show("bs", "play");
    else if (is("bs-defense")) games_debug_show("bs", "defense");
    else if (is("bs-win")) games_debug_show("bs", "win");
    else if (is("ck") || is("checkers")) go_checkers();
    else if (is("ck-play")) games_debug_show("ck", "play");
    else if (is("ck-win")) games_debug_show("ck", "win");
    else if (is("mem") || is("memory")) go_memory();
    else if (is("mem-play")) games_debug_show("mem", "play");
    else if (is("reversi") || is("rv")) go_reversi();
    else if (is("rv-play")) games_debug_show("rv", "play");
    else if (is("dots") || is("db")) go_dots();
    else if (is("db-play")) games_debug_show("db", "play");
    else if (is("scoreboard")) go_scoreboard();
    else if (is("2048") || is("g2048")) go_g2048();
    else if (is("utilsfolder") || is("utils")) go_utils_folder();
    else if (is("timer")) go_timer();
    else if (is("checklist")) {
      wp::checklist::clear_all();
      wp::checklist::add("Standup notes");
      wp::checklist::add("Ping Will about lunch");
      wp::checklist::add("Flash board firmware");
      wp::checklist::add("Order USB-C cables");
      wp::checklist::add("Test ESP-NOW range");
      wp::checklist::toggle(0);
      wp::checklist::toggle(2);
      go_checklist();
    }
    else if (is("calculator") || is("calc")) go_calculator();
    else if (is("doodle")) go_doodle();
    else if (is("doodle-draw")) doodle_debug_show_draw();
    else if (is("settings")) go_settings();
    else if (is("settings-scrolled")) {
      go_settings();
      lv_obj_t * body = lv_obj_get_child(lv_screen_active(), 1);
      if (body) lv_obj_scroll_to_y(body, 900, LV_ANIM_OFF);
    }
    else if (is("keyboard")) go_keyboard_name();
    else if (is("emoji")) go_emoji_picker(0);
    else if (is("wifi") || is("wifi-scan")) go_wifi_scan();
    else if (is("wifi-pass")) go_keyboard_wifi_pass();
    else if (is("ota") || is("updates")) go_ota_releases();
    else if (is("bg-upload") || is("background")) go_bg_upload();
  }

  if (const char * inc = std::getenv("WERKPAGER_INCOMING"); inc && inc[0] == '1') {
    using namespace wp::app;
    Desk & d = desk();
    d.incoming.active = true;
    std::snprintf(d.incoming.from_id, sizeof(d.incoming.from_id), "mac-will");
    std::snprintf(d.incoming.from_name, sizeof(d.incoming.from_name), "Will");
    std::snprintf(d.incoming.emoji, sizeof(d.incoming.emoji), "👑");
    std::snprintf(d.incoming.message, sizeof(d.incoming.message), "Lipsync for your life");
    wp::ui::go_incoming();
  }

  lv_timer_create(f12_poll_cb, 50, nullptr);
  wp::sim::maybe_auto_shot_and_quit();

  if (wp::sim::driver_enabled())
    std::printf("WerkBuddy PC sim ready (DRIVE). TCP 127.0.0.1:9471 — tap/swipe/shot/screen/quit\n");
  else
    std::printf("WerkBuddy PC sim ready. Click apps. F12 = save preview.png\n");

  while (true) {
    lv_timer_handler();
    lv_delay_ms(5);
  }
}
