#include "ui/nav.h"

#include "app/app.h"
#include "app/desk_timer.h"
#include "ui/brightness.h"
#include "ui/scr_doodle.h"
#include "ui/scr_games.h"
#include "ui/scr_active_games.h"
#include "ui/scr_g2048.h"
#include "ui/scr_hub.h"
#include "ui/scr_idle.h"
#include "ui/scr_page_history.h"
#include "ui/scr_pager.h"
#include "ui/scr_scoreboard.h"
#include "ui/scr_settings.h"
#include "ui/scr_setup.h"
#include "ui/scr_splash.h"
#include "ui/scr_timer.h"
#include "ui/scr_utils.h"

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {
namespace {

Screen g_screen = Screen::Hub;
Screen g_before_idle = Screen::Hub;
bool g_has_compose_peer = false;
app::Peer g_compose_peer;

void load(lv_obj_t * scr, Screen which) {
  lv_obj_t * old = lv_screen_active();
  g_screen = which;
  lv_screen_load(scr);
  if (old && old != scr) lv_obj_delete_async(old);
  brightness::set_page_boost(which == Screen::Incoming || which == Screen::Outgoing ||
                             (which == Screen::Timer && desk_timer::is_finished()));
}

void idle_tick(lv_timer_t * /*t*/) {
  uint8_t tid = app::desk().timeout_id;
  if (tid >= app::kTimeoutCount) tid = 2;
  const app::TimeoutSpec & spec = app::timeout_specs()[tid];
  if (!spec.ms) return;
  if (g_screen == Screen::Idle || g_screen == Screen::Incoming || g_screen == Screen::Outgoing ||
      g_screen == Screen::Keyboard || g_screen == Screen::EmojiPicker ||
      g_screen == Screen::WifiScan || g_screen == Screen::OtaReleases ||
      g_screen == Screen::BgUpload || g_screen == Screen::Splash || g_screen == Screen::Setup ||
      g_screen == Screen::Doodle ||
      (g_screen == Screen::Timer && desk_timer::is_finished())) {
    return;
  }
  if (app::busy()) return;
  if (lv_display_get_inactive_time(nullptr) >= spec.ms) go_idle();
}

}  // namespace

Screen current_screen() { return g_screen; }

void go_splash() { load(splash_screen(), Screen::Splash); }
void go_hub() { load(hub_screen(), Screen::Hub); }
void go_werk() { load(pager_werk_screen(), Screen::Werk); }

void go_compose(const app::Peer & peer) {
  g_compose_peer = peer;
  g_has_compose_peer = true;
  load(pager_compose_screen(peer), Screen::Compose);
}

void go_compose_refresh() {
  if (!g_has_compose_peer) {
    go_werk();
    return;
  }
  load(pager_compose_screen(g_compose_peer), Screen::Compose);
}

void go_outgoing() { load(pager_outgoing_screen(), Screen::Outgoing); }
void go_incoming() { load(pager_incoming_screen(), Screen::Incoming); }

void go_games_folder() { load(games_folder_screen(), Screen::GamesFolder); }
void go_active_games() { load(active_games_screen(), Screen::ActiveGames); }
void go_utils_folder() { load(utils_folder_screen(), Screen::UtilsFolder); }
void go_ttt() { load(game_ttt_screen(), Screen::Ttt); }
void go_sttt() { load(game_sttt_screen(), Screen::Sttt); }
void go_c4() { load(game_c4_screen(), Screen::C4); }
void go_battleship() { load(game_bs_screen(), Screen::Bs); }
void go_checkers() { load(game_ck_screen(), Screen::Ck); }
void go_memory() { load(game_mem_screen(), Screen::Mem); }
void go_reversi() { load(game_rv_screen(), Screen::Rv); }
void go_dots() { load(game_db_screen(), Screen::Db); }
void go_scoreboard() { load(scoreboard_screen(), Screen::Scoreboard); }
void go_g2048() { load(game_g2048_screen(), Screen::G2048); }

void go_doodle() { load(doodle_screen(), Screen::Doodle); }
void go_settings() { load(settings_screen(), Screen::Settings); }
void go_setup() { load(setup_screen(), Screen::Setup); }
void go_keyboard_name() { load(keyboard_screen_name(), Screen::Keyboard); }
void go_keyboard_setup_name() { load(keyboard_screen_setup_name(), Screen::Keyboard); }
void go_keyboard_factory_reset() { load(keyboard_screen_factory_reset(), Screen::Keyboard); }
void go_keyboard_canned(int index) { load(keyboard_screen_canned(index), Screen::Keyboard); }
void go_keyboard_compose() { load(keyboard_screen_compose(), Screen::Keyboard); }
void go_keyboard_checklist() { load(keyboard_screen_checklist(), Screen::Keyboard); }
void go_wifi_scan() { load(wifi_scan_screen(), Screen::WifiScan); }
void go_keyboard_wifi_pass() { load(keyboard_screen_wifi_pass(), Screen::Keyboard); }
void go_ota_releases() { load(ota_releases_screen(), Screen::OtaReleases); }
void go_emoji_picker(int slot) { load(emoji_picker_screen(slot), Screen::EmojiPicker); }
void go_bg_upload() { load(bg_upload_screen(), Screen::BgUpload); }
void go_timer() { load(timer_screen(), Screen::Timer); }
void go_checklist() { load(checklist_screen(), Screen::Checklist); }
void go_calculator() { load(calculator_screen(), Screen::Calculator); }
void go_page_history() { load(page_history_screen(), Screen::PageHistory); }

void go_idle() {
  if (g_screen == Screen::Idle) return;
  g_before_idle = g_screen;
  load(idle_screen(), Screen::Idle);
}

void wake_from_idle() {
  if (g_screen != Screen::Idle) return;
  switch (g_before_idle) {
    case Screen::Werk: go_werk(); break;
    case Screen::Compose:
      if (g_has_compose_peer) go_compose(g_compose_peer);
      else go_werk();
      break;
    case Screen::Settings: go_settings(); break;
    case Screen::Setup: go_setup(); break;
    case Screen::GamesFolder: go_games_folder(); break;
    case Screen::ActiveGames: go_active_games(); break;
    case Screen::UtilsFolder: go_utils_folder(); break;
    case Screen::Ttt: go_ttt(); break;
    case Screen::Sttt: go_sttt(); break;
    case Screen::C4: go_c4(); break;
    case Screen::Bs: go_battleship(); break;
    case Screen::Ck: go_checkers(); break;
    case Screen::Mem: go_memory(); break;
    case Screen::Rv: go_reversi(); break;
    case Screen::Db: go_dots(); break;
    case Screen::Scoreboard: go_scoreboard(); break;
    case Screen::G2048: go_g2048(); break;
    case Screen::Doodle: go_doodle(); break;
    case Screen::Timer: go_timer(); break;
    case Screen::Checklist: go_checklist(); break;
    case Screen::Calculator: go_calculator(); break;
    case Screen::PageHistory: go_page_history(); break;
    case Screen::Outgoing:
    case Screen::Incoming: sync_ui(); break;
    default: go_hub(); break;
  }
}

void sync_ui() {
  const app::Desk & d = app::desk();
  if (d.incoming.active) {
    go_incoming();
    return;
  }
  if (d.outgoing.active) {
    go_outgoing();
    return;
  }
  /* Call ended — leave pager rings only. Do not yank games/settings/etc. to hub. */
  const Screen cur = current_screen();
  if (cur == Screen::Incoming || cur == Screen::Outgoing || cur == Screen::Idle) go_hub();
}

void idle_init() { lv_timer_create(idle_tick, 500, nullptr); }

}  // namespace ui
}  // namespace wp
