#pragma once

namespace wp {
namespace app {
struct Peer;
}
namespace ui {

enum class Screen {
  Splash,
  Hub,
  Werk,
  Compose,
  Outgoing,
  Incoming,
  GamesFolder,
  ActiveGames,
  UtilsFolder,
  Ttt,
  Sttt,
  C4,
  Bs,
  Ck,
  Mem,
  Rv,
  Db,
  Scoreboard,
  G2048,
  Wordle,
  Doodle,
  Settings,
  Keyboard,
  EmojiPicker,
  WifiScan,
  OtaReleases,
  BgUpload,
  FwUpload,
  OtaProgress,
  Timer,
  Checklist,
  Calculator,
  PageHistory,
  Idle,
  Setup,
};

Screen current_screen();

void go_splash();
void go_hub();
void go_werk();
void go_compose(const app::Peer & peer);
void go_outgoing();
void go_incoming();

void go_games_folder();
void go_active_games();
void go_utils_folder();
void go_ttt();
void go_sttt();
void go_c4();
void go_battleship();
void go_checkers();
void go_memory();
void go_reversi();
void go_dots();
void go_scoreboard();
void go_g2048();
void go_wordle();

void go_doodle();
void go_settings();
void go_setup();
void go_keyboard_name();
void go_keyboard_setup_name();
void go_keyboard_factory_reset();
void go_keyboard_canned(int index);
void go_keyboard_compose();
void go_keyboard_checklist();
void go_wifi_scan();
void go_keyboard_wifi_pass();
void go_ota_releases();
void go_emoji_picker(int slot); /* settings slot, or kEmojiPickerCompose for WerkRoom */
void go_bg_upload();
void go_fw_upload();
void go_ota_progress();
void go_timer();
void go_checklist();
void go_calculator();
void go_page_history();

/** Re-open compose with the current draft (emoji/message) intact. */
void go_compose_refresh();

void go_idle();
void wake_from_idle();

/** Leave an in-play board: Active Games if another match is your turn, else Home. */
void go_game_back();

/** Web syncActiveDeskUi — jump to whatever state demands attention. */
void sync_ui();

/** Start the idle-timeout watcher (call once at boot). */
void idle_init();

}  // namespace ui
}  // namespace wp
