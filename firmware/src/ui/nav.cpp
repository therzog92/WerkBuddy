#include "ui/nav.h"

#include "app/app.h"
#include "app/active_games.h"
#include "app/desk_timer.h"
#include "ui/brightness.h"
#include "ui/display_perf.h"
#include "ui/scr_doodle.h"
#include "ui/scr_games.h"
#include "ui/scr_active_games.h"
#include "ui/scr_g2048.h"
#include "ui/scr_wordle.h"
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
int g_idle_focus = -1; /* focus_index saved at idle so we can resume matches */
bool g_idle_had_games = false; /* snapshot for wake recovery if RAM slots vanish */
bool g_wake_pending = false;
bool g_has_compose_peer = false;
app::Peer g_compose_peer;

bool is_game_board(Screen s) {
  return s == Screen::Ttt || s == Screen::Sttt || s == Screen::C4 || s == Screen::Bs ||
         s == Screen::Ck || s == Screen::Mem || s == Screen::Rv || s == Screen::Db ||
         s == Screen::Wordle;
}

app::GameKind kind_for_screen(Screen s) {
  switch (s) {
    case Screen::Ttt: return app::GameKind::Ttt;
    case Screen::Sttt: return app::GameKind::Sttt;
    case Screen::C4: return app::GameKind::C4;
    case Screen::Bs: return app::GameKind::Bs;
    case Screen::Ck: return app::GameKind::Ck;
    case Screen::Mem: return app::GameKind::Mem;
    case Screen::Rv: return app::GameKind::Rv;
    case Screen::Db: return app::GameKind::Db;
    case Screen::Wordle: return app::GameKind::Wordle;
    default: return app::GameKind::Ttt;
  }
}

void load(lv_obj_t * scr, Screen which, lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_NONE) {
  const Screen prev = g_screen;
  lv_obj_t * old = lv_screen_active();
  g_screen = which;
  /* Instant swap — animated loads + partial RGB strips look like a top-down wipe. */
  if (anim != LV_SCR_LOAD_ANIM_NONE) {
    lv_screen_load_anim(scr, anim, 250, 0, true);
  } else if (old && old != scr) {
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
  } else {
    lv_screen_load(scr);
  }
  const bool wash =
      which == Screen::Incoming ||
      (which == Screen::Timer && desk_timer::is_finished());
  brightness::set_page_boost(which == Screen::Incoming || which == Screen::Outgoing ||
                             (which == Screen::Timer && desk_timer::is_finished()));
  /* Full-frame only while a screen-wide color wash is animating. */
  display_perf::prefer_full_frame(wash);
  /* Leaving a board → Hub/Idle: flush games after paint, not mid-teardown. */
  if (is_game_board(prev) && !is_game_board(which)) app::games_persist_soon();
}

void idle_tick(lv_timer_t * /*t*/) {
  if (g_screen == Screen::Idle) return;
  if (app::busy()) return;
  if (g_screen == Screen::Incoming || g_screen == Screen::Outgoing || g_screen == Screen::Splash)
    return;

  uint32_t limit_ms = 0;
  if (!app::desk().setup_done) {
    /* Post-reset / first-run: blank after 1m even on Setup + name OSK so a
     * desk in a backpack isn't stuck at full brightness with no power button. */
    limit_ms = 60000;
  } else {
    uint8_t tid = app::desk().timeout_id;
    if (tid >= app::kTimeoutCount) tid = 2;
    const app::TimeoutSpec & spec = app::timeout_specs()[tid];
    if (!spec.ms) return;
    limit_ms = spec.ms;
    if (g_screen == Screen::Keyboard || g_screen == Screen::EmojiPicker ||
        g_screen == Screen::WifiScan || g_screen == Screen::OtaReleases ||
        g_screen == Screen::BgUpload || g_screen == Screen::FwUpload ||
        g_screen == Screen::OtaProgress || g_screen == Screen::Setup ||
        g_screen == Screen::Doodle ||
        (g_screen == Screen::Timer && desk_timer::is_finished())) {
      return;
    }
  }
  if (lv_display_get_inactive_time(nullptr) >= limit_ms) go_idle();
}

}  // namespace

Screen current_screen() { return g_screen; }

void go_splash() { load(splash_screen(), Screen::Splash); }
void go_hub() { load(hub_screen(), Screen::Hub); }

void go_game_back() {
  const int focus = app::focus_index();
  for (int i = 0; i < app::kMaxActiveGames; ++i) {
    if (i == focus) continue;
    app::GameSlot * s = app::slot_at(i);
    if (s && app::is_my_turn(*s)) {
      go_active_games();
      return;
    }
  }
  go_hub();
}
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
void go_ttt() {
  if (g_screen == Screen::Ttt) {
    game_reload_inplace();
    return;
  }
  load(game_ttt_screen(), Screen::Ttt, LV_SCR_LOAD_ANIM_FADE_IN);
}
void go_sttt() {
  if (g_screen == Screen::Sttt) {
    game_reload_inplace();
    return;
  }
  load(game_sttt_screen(), Screen::Sttt, LV_SCR_LOAD_ANIM_FADE_IN);
}
void go_c4() {
  if (g_screen == Screen::C4) {
    game_reload_inplace();
    return;
  }
  load(game_c4_screen(), Screen::C4, LV_SCR_LOAD_ANIM_FADE_IN);
}
void go_battleship() {
  if (g_screen == Screen::Bs) {
    game_reload_inplace();
    return;
  }
  load(game_bs_screen(), Screen::Bs, LV_SCR_LOAD_ANIM_FADE_IN);
}
void go_checkers() {
  if (g_screen == Screen::Ck) {
    game_reload_inplace();
    return;
  }
  load(game_ck_screen(), Screen::Ck, LV_SCR_LOAD_ANIM_FADE_IN);
}
void go_memory() {
  if (g_screen == Screen::Mem) {
    game_reload_inplace();
    return;
  }
  load(game_mem_screen(), Screen::Mem, LV_SCR_LOAD_ANIM_FADE_IN);
}
void go_reversi() {
  if (g_screen == Screen::Rv) {
    game_reload_inplace();
    return;
  }
  load(game_rv_screen(), Screen::Rv, LV_SCR_LOAD_ANIM_FADE_IN);
}
void go_dots() {
  if (g_screen == Screen::Db) {
    game_reload_inplace();
    return;
  }
  load(game_db_screen(), Screen::Db, LV_SCR_LOAD_ANIM_FADE_IN);
}
void go_scoreboard() { load(scoreboard_screen(), Screen::Scoreboard, LV_SCR_LOAD_ANIM_FADE_IN); }
void go_g2048() { load(game_g2048_screen(), Screen::G2048, LV_SCR_LOAD_ANIM_FADE_IN); }
void go_wordle() { load(game_wordle_screen(), Screen::Wordle, LV_SCR_LOAD_ANIM_FADE_IN); }

void go_doodle() { load(doodle_screen(), Screen::Doodle, LV_SCR_LOAD_ANIM_FADE_IN); }
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
void go_fw_upload() { load(fw_upload_screen(), Screen::FwUpload); }
void go_ota_progress() { load(ota_progress_screen(), Screen::OtaProgress); }
void go_timer() { load(timer_screen(), Screen::Timer); }
void go_checklist() { load(checklist_screen(), Screen::Checklist); }
void go_calculator() { load(calculator_screen(), Screen::Calculator); }
void go_page_history() { load(page_history_screen(), Screen::PageHistory); }

void go_idle() {
  if (g_screen == Screen::Idle) return;
  /* Don't lock over an incoming page / live call — those need the action UI. */
  if (app::desk().incoming.active || app::desk().outgoing.active) return;
  g_before_idle = g_screen;
  g_idle_focus = app::focus_index();
  g_idle_had_games = app::active_count() > 0;
  load(idle_screen(), Screen::Idle);
  /* Flush after idle paints — sync NVS during load stalled the RGB panel. */
  app::schedule(450, [](void * /*ud*/) { app::games_persist(); }, nullptr);
}

void finish_wake_from_idle(void * /*ud*/) {
  g_wake_pending = false;
  if (g_screen != Screen::Idle) return;
  lv_display_trigger_activity(nullptr);

  /*
   * If RAM slots vanished while locked (seen after deleting the game screen /
   * waking from inside a PRESSED handler), reload the idle-time snapshot.
   */
  if (app::active_count() == 0 && g_idle_had_games) {
    if (app::games_restore()) app::games_probe_peers();
  }
  g_idle_had_games = false;

  /* Restore focus saved at lock — go_* boards need it or they show an empty peer pick. */
  if (g_idle_focus >= 0) {
    app::GameSlot * saved = app::slot_at(g_idle_focus);
    if (saved && app::slot_is_live(*saved)) app::set_focus(g_idle_focus);
  }
  if (is_game_board(g_before_idle)) {
    const int live = app::find_live_kind(kind_for_screen(g_before_idle));
    if (live >= 0) app::set_focus(live);
  }

  /*
   * Never rebuild a heavy game board from the press path — that froze touch.
   * Only jump to Active Games when it's your turn. Otherwise Hub / prior screen
   * — games stay live in the registry (NVS already flushed).
   */
  if (app::your_turn_count() > 0) {
    go_active_games();
    return;
  }

  switch (g_before_idle) {
    case Screen::Werk: go_werk(); break;
    case Screen::Compose:
      if (g_has_compose_peer) go_compose(g_compose_peer);
      else go_werk();
      break;
    case Screen::Settings: go_settings(); break;
    case Screen::Setup: go_setup(); break;
    case Screen::Keyboard:
      /* Setup name OSK — don't drop into Hub before Continue. */
      if (!app::desk().setup_done) go_setup();
      else go_hub();
      break;
    case Screen::GamesFolder: go_games_folder(); break;
    case Screen::ActiveGames:
      /* Not your turn — Hub keeps sync; avoid bouncing into AG every wake. */
      go_hub();
      break;
    case Screen::UtilsFolder: go_utils_folder(); break;
    case Screen::Scoreboard: go_scoreboard(); break;
    case Screen::G2048: go_g2048(); break;
    case Screen::Wordle: go_wordle(); break;
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

void wake_from_idle() {
  if (g_screen != Screen::Idle) return;
  if (g_wake_pending) return;
  /* Defer screen swap — deleting Idle from its own PRESSED/CLICKED handler
   * corrupted nearby BSS (including active game slots) on device. */
  g_wake_pending = true;
  brightness::set_panel_on(true);
  lv_display_trigger_activity(nullptr);
  app::schedule(1, finish_wake_from_idle, nullptr);
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
