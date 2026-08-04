#include "ui/nav.h"

#include "app/app.h"
#include "ui/brightness.h"
#include "ui/scr_doodle.h"
#include "ui/scr_games.h"
#include "ui/scr_hub.h"
#include "ui/scr_idle.h"
#include "ui/scr_pager.h"
#include "ui/scr_settings.h"
#include "ui/scr_splash.h"

#include "lvgl/lvgl.h"

namespace wp {
namespace ui {
namespace {

Screen g_screen = Screen::Hub;
Screen g_before_idle = Screen::Hub;
bool g_has_compose_peer = false;
app::Peer g_compose_peer;

/*
 * Screens rebuild from app state on every navigation (mirrors the web sim's
 * render-on-change model). Old screen is deleted async — never mid-event.
 */
void load(lv_obj_t * scr, Screen which) {
  lv_obj_t * old = lv_screen_active();
  g_screen = which;
  lv_screen_load(scr);
  if (old && old != scr) lv_obj_delete_async(old);
  /* Incoming/outgoing pages always full bright so you can't miss a ring. */
  brightness::set_page_boost(which == Screen::Incoming || which == Screen::Outgoing);
}

void idle_tick(lv_timer_t * /*t*/) {
  const app::TimeoutSpec & spec = app::timeout_specs()[app::desk().timeout_id];
  if (!spec.ms) return;
  if (g_screen == Screen::Idle || g_screen == Screen::Incoming || g_screen == Screen::Outgoing ||
      g_screen == Screen::Keyboard || g_screen == Screen::EmojiPicker ||
      g_screen == Screen::WifiScan || g_screen == Screen::OtaReleases ||
      g_screen == Screen::Splash) {
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
void go_ttt() { load(game_ttt_screen(), Screen::Ttt); }
void go_c4() { load(game_c4_screen(), Screen::C4); }
void go_battleship() { load(game_bs_screen(), Screen::Bs); }
void go_checkers() { load(game_ck_screen(), Screen::Ck); }
void go_memory() { load(game_mem_screen(), Screen::Mem); }

void go_doodle() { load(doodle_screen(), Screen::Doodle); }
void go_settings() { load(settings_screen(), Screen::Settings); }
void go_keyboard_name() { load(keyboard_screen_name(), Screen::Keyboard); }
void go_keyboard_canned(int index) { load(keyboard_screen_canned(index), Screen::Keyboard); }
void go_keyboard_compose() { load(keyboard_screen_compose(), Screen::Keyboard); }
void go_wifi_scan() { load(wifi_scan_screen(), Screen::WifiScan); }
void go_keyboard_wifi_pass() { load(keyboard_screen_wifi_pass(), Screen::Keyboard); }
void go_ota_releases() { load(ota_releases_screen(), Screen::OtaReleases); }
void go_emoji_picker(int slot) { load(emoji_picker_screen(slot), Screen::EmojiPicker); }

void go_idle() {
  if (g_screen == Screen::Idle) return;
  g_before_idle = g_screen;
  load(idle_screen(), Screen::Idle);
}

void wake_from_idle() {
  if (g_screen != Screen::Idle) return;
  /* web resumeScreen */
  switch (g_before_idle) {
    case Screen::Werk: go_werk(); break;
    case Screen::Compose:
      if (g_has_compose_peer) go_compose(g_compose_peer);
      else go_werk();
      break;
    case Screen::Settings: go_settings(); break;
    case Screen::GamesFolder: go_games_folder(); break;
    case Screen::Ttt: go_ttt(); break;
    case Screen::C4: go_c4(); break;
    case Screen::Bs: go_battleship(); break;
    case Screen::Ck: go_checkers(); break;
    case Screen::Mem: go_memory(); break;
    case Screen::Doodle: go_doodle(); break;
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
  if (d.ttt_invite.active || d.ttt.active) {
    go_ttt();
    return;
  }
  if (d.c4_invite.active || d.c4.active) {
    go_c4();
    return;
  }
  if (d.bs_invite.active || d.bs.active) {
    go_battleship();
    return;
  }
  if (d.ck_invite.active || d.ck.active) {
    go_checkers();
    return;
  }
  if (d.mem_invite.active || d.mem.active) {
    go_memory();
    return;
  }
  go_hub();
}

void idle_init() { lv_timer_create(idle_tick, 500, nullptr); }

}  // namespace ui
}  // namespace wp
