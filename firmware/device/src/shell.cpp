#include "shell.h"

#include "desk.h"
#include "espnow_link.h"
#include "storage_nvs.h"
#include "ttt_flow.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstdio>
#include <cstring>

namespace wp {
namespace shell {

enum class Screen {
  Hub,
  Settings,
  Idle,
  NameEdit,
  PeerPick,
  Compose,
  MsgEdit,
  Outgoing,
  Incoming
};

lv_obj_t * g_scr = nullptr;
Screen g_screen = Screen::Hub;
lv_obj_t * g_toast = nullptr;
lv_obj_t * g_toast_lbl = nullptr;
uint32_t g_toast_until = 0;
uint32_t g_last_input_ms = 0;
lv_obj_t * g_idle_clock = nullptr;
lv_timer_t * g_clock_timer = nullptr;
lv_obj_t * g_ta = nullptr;

char g_draft_emoji[9] = "!";
char g_draft_message[23] = "You busy?";
int g_compose_peer_idx = -1;

constexpr const char * kCanned[] = {"You busy?", "Come here!", "Downstairs?", "OMG WTF!!!"};
constexpr const char * kEmoji[] = {"!", "?", "*", "#"};

void persist() { storage::save(desk()); }
void note_input() { g_last_input_ms = lv_tick_get(); }
bool call_busy() { return desk().outgoing_active || desk().incoming_active; }

void ensure_toast() {
  if (g_toast) return;
  g_toast = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g_toast);
  lv_obj_set_size(g_toast, 440, 48);
  lv_obj_align(g_toast, LV_ALIGN_TOP_MID, 0, 16);
  lv_obj_set_style_bg_color(g_toast, lv_color_hex(0xf0c040), 0);
  lv_obj_set_style_bg_opa(g_toast, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_toast, 8, 0);
  lv_obj_set_style_pad_hor(g_toast, 12, 0);
  lv_obj_add_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_toast, LV_OBJ_FLAG_SCROLLABLE);

  g_toast_lbl = lv_label_create(g_toast);
  lv_obj_set_width(g_toast_lbl, 416);
  lv_obj_set_style_text_align(g_toast_lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(g_toast_lbl, lv_color_hex(0x101018), 0);
  lv_obj_set_style_text_font(g_toast_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(g_toast_lbl);
}

void show_toast(const char * text) {
  if (!text || !text[0]) return;
  ensure_toast();
  lv_label_set_text(g_toast_lbl, text);
  lv_obj_clear_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_toast);
  g_toast_until = lv_tick_get() + 3000;
  Serial.printf("toast: %s\n", text);
}

void hide_toast() {
  if (!g_toast) return;
  lv_obj_add_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
  g_toast_until = 0;
}

bool game_busy() { return call_busy() || ttt_busy(); }

namespace {
void build_hub();
}
void go_hub() { build_hub(); }

void load_screen(lv_obj_t * scr) {
  lv_obj_t * old = g_scr;
  g_scr = scr;
  g_idle_clock = nullptr;
  g_ta = nullptr;
  lv_screen_load(scr);
  if (old && old != scr) lv_obj_delete(old);
  note_input();
  ensure_toast();
}

namespace {

void apply_bg(lv_obj_t * scr) {
  lv_obj_set_style_bg_color(scr, lv_color_hex(theme_bg(desk().theme)), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

lv_obj_t * make_btn(lv_obj_t * parent, const char * label, lv_event_cb_t cb, int w, int h) {
  lv_obj_t * btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_bg_color(btn, lv_color_hex(theme_accent(desk().theme)), 0);
  lv_obj_t * lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x101018), 0);
  lv_obj_center(lbl);
  return btn;
}

void load_scr(lv_obj_t * scr) { load_screen(scr); }

void go_settings();
void go_idle();
void go_name_edit();
void go_peer_pick();
void go_compose();
void go_msg_edit();
void go_outgoing();
void go_incoming();

void on_net_msg(const net::RxMsg & m) {
  switch (m.kind) {
    case net::RxMsg::Kind::Discover:
    case net::RxMsg::Kind::DiscoverReply:
      add_nearby(m.from_id, m.from_name);
      show_toast(m.from_name);
      if (g_screen == Screen::Settings) go_settings();
      break;
    case net::RxMsg::Kind::Call:
      desk().incoming_active = true;
      std::snprintf(desk().call_peer_id, sizeof(desk().call_peer_id), "%s", m.from_id);
      std::snprintf(desk().call_peer_name, sizeof(desk().call_peer_name), "%s", m.from_name);
      std::snprintf(desk().call_emoji, sizeof(desk().call_emoji), "%s", m.emoji);
      std::snprintf(desk().call_message, sizeof(desk().call_message), "%s", m.message);
      go_incoming();
      break;
    case net::RxMsg::Kind::Ack:
      if (desk().outgoing_active) {
        desk().outgoing_active = false;
        char line[48];
        std::snprintf(line, sizeof(line), "%s acknowledged", m.from_name);
        go_hub();
        show_toast(line);
      }
      break;
    case net::RxMsg::Kind::Clear:
      /* Sender cancelled while we still show incoming. */
      if (desk().incoming_active && std::strcmp(desk().call_peer_id, m.from_id) == 0) {
        desk().incoming_active = false;
        char line[48];
        std::snprintf(line, sizeof(line), "%s cancelled", m.from_name);
        go_hub();
        show_toast(line);
      }
      break;
    case net::RxMsg::Kind::TttInvite:
    case net::RxMsg::Kind::TttAccept:
    case net::RxMsg::Kind::TttDecline:
    case net::RxMsg::Kind::TttMove:
    case net::RxMsg::Kind::TttForfeit:
      ttt_on_msg(m);
      break;
  }
}

/* ---- Hub ---- */
void on_settings(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  go_settings();
}
void on_pager(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  go_peer_pick();
}
void on_games(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  go_ttt();
}

void build_hub() {
  g_screen = Screen::Hub;
  lv_obj_t * scr = lv_obj_create(nullptr);
  apply_bg(scr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * brand = lv_label_create(scr);
  lv_label_set_text(brand, "WERKBUDDY");
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(brand, lv_color_hex(theme_accent(desk().theme)), 0);
  lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t * who = lv_label_create(scr);
  lv_label_set_text(who, desk().name);
  lv_obj_set_style_text_font(who, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(who, lv_color_hex(theme_fg(desk().theme)), 0);
  lv_obj_align(who, LV_ALIGN_TOP_MID, 0, 96);

  lv_obj_t * mac = lv_label_create(scr);
  lv_label_set_text(mac, net::own_mac_pretty());
  lv_obj_set_style_text_font(mac, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(mac, lv_color_hex(0x9aa0a6), 0);
  lv_obj_align(mac, LV_ALIGN_TOP_MID, 0, 128);

  lv_obj_t * row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 440, 90);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 40);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  make_btn(row, "WERKPAGER", on_pager, 120, 72);
  make_btn(row, "Games", on_games, 100, 72);
  make_btn(row, "Settings", on_settings, 100, 72);

  lv_obj_t * peers = lv_label_create(scr);
  lv_label_set_text_fmt(peers, "%d saved peer%s", desk().peer_count,
                        desk().peer_count == 1 ? "" : "s");
  lv_obj_set_style_text_font(peers, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(peers, lv_color_hex(0x9aa0a6), 0);
  lv_obj_align(peers, LV_ALIGN_BOTTOM_MID, 0, -48);

  load_scr(scr);
}

/* ---- Settings (unchanged flow) ---- */
void on_back_hub(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  go_hub();
}
void on_cycle_theme(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  desk().theme = (uint8_t)((desk().theme + 1) % 4);
  persist();
  go_settings();
}
void on_cycle_timeout(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  desk().timeout_id = (uint8_t)((desk().timeout_id + 1) % 5);
  persist();
  go_settings();
}
void on_cycle_idle(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  desk().idle_mode = desk().idle_mode ? 0 : 1;
  persist();
  go_settings();
}
void on_edit_name(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  go_name_edit();
}
void on_bump_hour(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  std::tm tm{};
  local_time(&tm);
  set_clock_from_parts(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, (tm.tm_hour + 1) % 24, tm.tm_min);
  persist();
  go_settings();
}
void on_bump_min(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  std::tm tm{};
  local_time(&tm);
  set_clock_from_parts(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, (tm.tm_min + 5) % 60);
  persist();
  go_settings();
}
void on_scan(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  clear_nearby();
  net::link_send_discover();
  show_toast("Scanning...");
  go_settings();
}
static void on_save_nearby(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= desk().nearby_count) return;
  add_peer(desk().nearby[idx].id, desk().nearby[idx].name);
  persist();
  show_toast("Peer saved");
  go_settings();
}

void go_settings() {
  g_screen = Screen::Settings;
  lv_obj_t * scr = lv_obj_create(nullptr);
  apply_bg(scr);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr, 16, 0);
  lv_obj_set_style_pad_row(scr, 8, 0);
  lv_obj_set_scroll_dir(scr, LV_DIR_VER);

  lv_obj_t * title = lv_label_create(scr);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(theme_fg(desk().theme)), 0);

  char line[64];
  std::tm tm{};
  local_time(&tm);
  std::snprintf(line, sizeof(line), "Name: %s", desk().name);
  make_btn(scr, line, on_edit_name, 440, 48);
  std::snprintf(line, sizeof(line), "Theme: %s", theme_name(desk().theme));
  make_btn(scr, line, on_cycle_theme, 440, 48);
  std::snprintf(line, sizeof(line), "Idle timeout: %s", timeout_label(desk().timeout_id));
  make_btn(scr, line, on_cycle_timeout, 440, 48);
  std::snprintf(line, sizeof(line), "Idle: %s", desk().idle_mode ? "Clock" : "Black");
  make_btn(scr, line, on_cycle_idle, 440, 48);
  std::snprintf(line, sizeof(line), "Time +1h  (%02d:%02d)", tm.tm_hour, tm.tm_min);
  make_btn(scr, line, on_bump_hour, 440, 48);
  make_btn(scr, "Time +5m", on_bump_min, 440, 48);
  make_btn(scr, "Scan desks", on_scan, 440, 52);

  lv_obj_t * near = lv_label_create(scr);
  lv_label_set_text(near, "Nearby");
  lv_obj_set_style_text_color(near, lv_color_hex(theme_accent(desk().theme)), 0);
  lv_obj_set_style_text_font(near, &lv_font_montserrat_14, 0);

  bool any_near = false;
  for (int i = 0; i < desk().nearby_count; ++i) {
    if (peer_saved(desk().nearby[i].id)) continue;
    any_near = true;
    lv_obj_t * b = lv_button_create(scr);
    lv_obj_set_size(b, 440, 44);
    lv_obj_set_style_bg_color(b, lv_color_hex(theme_accent(desk().theme)), 0);
    lv_obj_add_event_cb(b, on_save_nearby, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t * lbl = lv_label_create(b);
    lv_label_set_text(lbl, desk().nearby[i].name);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x101018), 0);
    lv_obj_center(lbl);
  }
  if (!any_near) {
    lv_obj_t * empty = lv_label_create(scr);
    lv_label_set_text(empty, "None yet - tap Scan");
    lv_obj_set_style_text_color(empty, lv_color_hex(0x9aa0a6), 0);
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
  }

  lv_obj_t * saved = lv_label_create(scr);
  lv_label_set_text(saved, "Saved peers");
  lv_obj_set_style_text_color(saved, lv_color_hex(theme_accent(desk().theme)), 0);
  lv_obj_set_style_text_font(saved, &lv_font_montserrat_14, 0);
  if (desk().peer_count == 0) {
    lv_obj_t * empty = lv_label_create(scr);
    lv_label_set_text(empty, "None");
    lv_obj_set_style_text_color(empty, lv_color_hex(0x9aa0a6), 0);
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
  }
  for (int i = 0; i < desk().peer_count; ++i) {
    lv_obj_t * row = lv_label_create(scr);
    lv_label_set_text_fmt(row, "%s", desk().peers[i].name);
    lv_obj_set_style_text_color(row, lv_color_hex(theme_fg(desk().theme)), 0);
    lv_obj_set_style_text_font(row, &lv_font_montserrat_12, 0);
  }

  make_btn(scr, "Home", on_back_hub, 440, 52);
  load_scr(scr);
}

void on_name_ready(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * kb = (lv_obj_t *)lv_event_get_target(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    if (code == LV_EVENT_READY && g_ta) {
      const char * t = lv_textarea_get_text(g_ta);
      if (t && t[0]) {
        std::snprintf(desk().name, sizeof(desk().name), "%.12s", t);
        desk().setup_done = true;
        persist();
      }
    }
    lv_keyboard_set_textarea(kb, nullptr);
    go_settings();
  }
}

void go_name_edit() {
  g_screen = Screen::NameEdit;
  lv_obj_t * scr = lv_obj_create(nullptr);
  apply_bg(scr);
  lv_obj_t * title = lv_label_create(scr);
  lv_label_set_text(title, "Your name");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(theme_fg(desk().theme)), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

  g_ta = lv_textarea_create(scr);
  lv_obj_set_size(g_ta, 400, 48);
  lv_obj_align(g_ta, LV_ALIGN_TOP_MID, 0, 64);
  lv_textarea_set_max_length(g_ta, 12);
  lv_textarea_set_one_line(g_ta, true);
  lv_textarea_set_text(g_ta, desk().name);

  lv_obj_t * kb = lv_keyboard_create(scr);
  lv_obj_set_size(kb, 480, 220);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb, g_ta);
  lv_obj_add_event_cb(kb, on_name_ready, LV_EVENT_ALL, nullptr);
  load_scr(scr);
}

/* ---- Pager: peer pick ---- */
static void on_pick_peer(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  g_compose_peer_idx = (int)(intptr_t)lv_event_get_user_data(e);
  std::snprintf(g_draft_emoji, sizeof(g_draft_emoji), "%s", kEmoji[0]);
  std::snprintf(g_draft_message, sizeof(g_draft_message), "%s", kCanned[0]);
  go_compose();
}

void go_peer_pick() {
  g_screen = Screen::PeerPick;
  lv_obj_t * scr = lv_obj_create(nullptr);
  apply_bg(scr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  /* Content above a fixed bottom dock — Home must not look like a peer row. */
  lv_obj_t * body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, 480, 400);
  lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(body, 16, 0);
  lv_obj_set_style_pad_row(body, 10, 0);
  lv_obj_set_scroll_dir(body, LV_DIR_VER);
  lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * title = lv_label_create(body);
  lv_label_set_text(title, "WERKPAGER");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(theme_accent(desk().theme)), 0);

  lv_obj_t * sub = lv_label_create(body);
  lv_label_set_text(sub, "Who are we bothering?");
  lv_obj_set_style_text_color(sub, lv_color_hex(theme_fg(desk().theme)), 0);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);

  if (desk().peer_count == 0) {
    lv_obj_t * empty = lv_label_create(body);
    lv_label_set_text(empty, "No saved peers - Settings > Scan");
    lv_obj_set_style_text_color(empty, lv_color_hex(0x9aa0a6), 0);
  }
  for (int i = 0; i < desk().peer_count; ++i) {
    lv_obj_t * b = lv_button_create(body);
    lv_obj_set_size(b, 440, 56);
    lv_obj_set_style_bg_color(b, lv_color_hex(theme_accent(desk().theme)), 0);
    lv_obj_add_event_cb(b, on_pick_peer, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t * lbl = lv_label_create(b);
    lv_label_set_text(lbl, desk().peers[i].name);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x101018), 0);
    lv_obj_center(lbl);
  }

  lv_obj_t * dock = lv_obj_create(scr);
  lv_obj_remove_style_all(dock);
  lv_obj_set_size(dock, 480, 80);
  lv_obj_align(dock, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(dock, lv_color_hex(0x1a1a22), 0);
  lv_obj_set_style_bg_opa(dock, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(dock, 20, 0);
  lv_obj_set_style_pad_ver(dock, 14, 0);

  lv_obj_t * home = lv_button_create(dock);
  lv_obj_set_size(home, 440, 52);
  lv_obj_center(home);
  lv_obj_set_style_bg_color(home, lv_color_hex(0x3a3a48), 0);
  lv_obj_add_event_cb(home, on_back_hub, LV_EVENT_CLICKED, nullptr);
  lv_obj_t * hl = lv_label_create(home);
  lv_label_set_text(hl, "Home");
  lv_obj_set_style_text_font(hl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(hl, lv_color_hex(0xf5f5f5), 0);
  lv_obj_center(hl);

  load_scr(scr);
}

/* ---- Compose ---- */
static void on_emoji(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i >= 0 && i < 4) std::snprintf(g_draft_emoji, sizeof(g_draft_emoji), "%s", kEmoji[i]);
  go_compose();
}
static void on_canned(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i >= 0 && i < 4) std::snprintf(g_draft_message, sizeof(g_draft_message), "%s", kCanned[i]);
  go_compose();
}
static void on_custom_msg(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  go_msg_edit();
}
static void on_send(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  if (g_compose_peer_idx < 0 || g_compose_peer_idx >= desk().peer_count) return;
  const Peer & p = desk().peers[g_compose_peer_idx];
  desk().outgoing_active = true;
  std::snprintf(desk().call_peer_id, sizeof(desk().call_peer_id), "%s", p.id);
  std::snprintf(desk().call_peer_name, sizeof(desk().call_peer_name), "%s", p.name);
  std::snprintf(desk().call_emoji, sizeof(desk().call_emoji), "%s", g_draft_emoji);
  std::snprintf(desk().call_message, sizeof(desk().call_message), "%s", g_draft_message);
  net::link_send_call(p.id, g_draft_emoji, g_draft_message);
  go_outgoing();
}
static void on_compose_back(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  go_peer_pick();
}

void go_compose() {
  g_screen = Screen::Compose;
  lv_obj_t * scr = lv_obj_create(nullptr);
  apply_bg(scr);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr, 12, 0);
  lv_obj_set_style_pad_row(scr, 8, 0);
  lv_obj_set_scroll_dir(scr, LV_DIR_VER);

  const char * peer =
      (g_compose_peer_idx >= 0 && g_compose_peer_idx < desk().peer_count)
          ? desk().peers[g_compose_peer_idx].name
          : "?";

  lv_obj_t * title = lv_label_create(scr);
  lv_label_set_text_fmt(title, "To %s", peer);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(theme_fg(desk().theme)), 0);

  lv_obj_t * preview = lv_label_create(scr);
  lv_label_set_text_fmt(preview, "[%s]  %s", g_draft_emoji, g_draft_message);
  lv_obj_set_style_text_font(preview, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(preview, lv_color_hex(theme_accent(desk().theme)), 0);

  lv_obj_t * erow = lv_obj_create(scr);
  lv_obj_remove_style_all(erow);
  lv_obj_set_size(erow, 440, 48);
  lv_obj_set_flex_flow(erow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(erow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  for (int i = 0; i < 4; ++i) {
    lv_obj_t * b = lv_button_create(erow);
    lv_obj_set_size(b, 72, 40);
    lv_obj_add_event_cb(b, on_emoji, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, kEmoji[i]);
    lv_obj_center(l);
  }

  for (int i = 0; i < 4; ++i) {
    lv_obj_t * b = lv_button_create(scr);
    lv_obj_set_size(b, 440, 40);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2a2a36), 0);
    lv_obj_add_event_cb(b, on_canned, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, kCanned[i]);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_center(l);
  }

  make_btn(scr, "Custom message", on_custom_msg, 440, 44);
  make_btn(scr, "Send", on_send, 440, 56);
  make_btn(scr, "Back", on_compose_back, 440, 44);
  load_scr(scr);
}

void on_msg_ready(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    if (code == LV_EVENT_READY && g_ta) {
      const char * t = lv_textarea_get_text(g_ta);
      if (t) std::snprintf(g_draft_message, sizeof(g_draft_message), "%.22s", t);
    }
    go_compose();
  }
}

void go_msg_edit() {
  g_screen = Screen::MsgEdit;
  lv_obj_t * scr = lv_obj_create(nullptr);
  apply_bg(scr);
  lv_obj_t * title = lv_label_create(scr);
  lv_label_set_text(title, "Custom message");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(theme_fg(desk().theme)), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  g_ta = lv_textarea_create(scr);
  lv_obj_set_size(g_ta, 440, 48);
  lv_obj_align(g_ta, LV_ALIGN_TOP_MID, 0, 56);
  lv_textarea_set_max_length(g_ta, 22);
  lv_textarea_set_one_line(g_ta, true);
  lv_textarea_set_text(g_ta, g_draft_message);

  lv_obj_t * kb = lv_keyboard_create(scr);
  lv_obj_set_size(kb, 480, 220);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb, g_ta);
  lv_obj_add_event_cb(kb, on_msg_ready, LV_EVENT_ALL, nullptr);
  load_scr(scr);
}

/* ---- Outgoing / Incoming ---- */
static void on_cancel_out(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  if (desk().outgoing_active) {
    net::link_send_clear(desk().call_peer_id);
    desk().outgoing_active = false;
  }
  go_hub();
  show_toast("Ping cancelled");
}
static void on_acknowledge(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  if (desk().incoming_active) {
    net::link_send_ack(desk().call_peer_id);
    desk().incoming_active = false;
  }
  go_hub();
  show_toast("Acknowledged");
}

void go_outgoing() {
  g_screen = Screen::Outgoing;
  lv_obj_t * scr = lv_obj_create(nullptr);
  apply_bg(scr);
  lv_obj_t * t = lv_label_create(scr);
  lv_label_set_text_fmt(t, "Paging\n%s", desk().call_peer_name);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(theme_fg(desk().theme)), 0);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -40);

  lv_obj_t * msg = lv_label_create(scr);
  lv_label_set_text_fmt(msg, "[%s] %s", desk().call_emoji, desk().call_message);
  lv_obj_set_style_text_color(msg, lv_color_hex(theme_accent(desk().theme)), 0);
  lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
  lv_obj_align(msg, LV_ALIGN_CENTER, 0, 40);

  lv_obj_t * cancel = make_btn(scr, "Cancel", on_cancel_out, 280, 56);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_MID, 0, -40);
  load_scr(scr);
}

void go_incoming() {
  g_screen = Screen::Incoming;
  lv_obj_t * scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a0810), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t * from = lv_label_create(scr);
  lv_label_set_text_fmt(from, "%s", desk().call_peer_name);
  lv_obj_set_style_text_font(from, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(from, lv_color_hex(0xff8a9a), 0);
  lv_obj_align(from, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t * em = lv_label_create(scr);
  lv_label_set_text_fmt(em, "[%s]", desk().call_emoji);
  lv_obj_set_style_text_font(em, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(em, lv_color_hex(0xf0c040), 0);
  lv_obj_align(em, LV_ALIGN_CENTER, 0, -40);

  lv_obj_t * msg = lv_label_create(scr);
  lv_label_set_text(msg, desk().call_message);
  lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(msg, lv_color_hex(0xf5f5f5), 0);
  lv_obj_set_width(msg, 400);
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(msg, LV_ALIGN_CENTER, 0, 20);

  lv_obj_t * ack = lv_button_create(scr);
  lv_obj_set_size(ack, 320, 72);
  lv_obj_align(ack, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_set_style_bg_color(ack, lv_color_hex(0x3dd68c), 0);
  lv_obj_add_event_cb(ack, on_acknowledge, LV_EVENT_CLICKED, nullptr);
  lv_obj_t * al = lv_label_create(ack);
  lv_label_set_text(al, "Acknowledge");
  lv_obj_set_style_text_font(al, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(al, lv_color_hex(0x0a1a10), 0);
  lv_obj_center(al);

  load_scr(scr);
}

/* ---- Idle ---- */
void idle_clock_tick(lv_timer_t * /*t*/) {
  if (!g_idle_clock || g_screen != Screen::Idle) return;
  std::tm tm{};
  local_time(&tm);
  char buf[32];
  int h = tm.tm_hour % 12;
  if (h == 0) h = 12;
  std::snprintf(buf, sizeof(buf), "%d:%02d %s", h, tm.tm_min, tm.tm_hour >= 12 ? "PM" : "AM");
  lv_label_set_text(g_idle_clock, buf);
}
void on_idle_wake(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  note_input();
  go_hub();
}
void go_idle() {
  g_screen = Screen::Idle;
  lv_obj_t * scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, on_idle_wake, LV_EVENT_CLICKED, nullptr);
  if (desk().idle_mode == 1) {
    g_idle_clock = lv_label_create(scr);
    lv_obj_set_style_text_font(g_idle_clock, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_idle_clock, lv_color_hex(0xf5f5f5), 0);
    lv_obj_center(g_idle_clock);
    idle_clock_tick(nullptr);
    if (!g_clock_timer) g_clock_timer = lv_timer_create(idle_clock_tick, 1000, nullptr);
  }
  load_scr(scr);
}

}  // namespace

void start() {
  desk_defaults();
  storage::load(desk());
  desk().outgoing_active = false;
  desk().incoming_active = false;
  desk().ttt_active = false;
  desk().ttt_waiting = false;
  desk().ttt_incoming = false;
  desk().ttt_over = false;
  desk().ttt_result_dismissed = false;
  net::link_init(on_net_msg);
  note_input();
  ensure_toast();
  if (!desk().setup_done) go_name_edit();
  else go_hub();
  Serial.printf("Shell+pager+ttt start name=%s peers=%d\n", desk().name, desk().peer_count);
}

void tick() {
  net::link_poll();
  if (g_toast_until && (int32_t)(lv_tick_get() - g_toast_until) >= 0) hide_toast();
  const uint32_t to = idle_timeout_ms();
  if (to > 0 && !game_busy() && g_screen != Screen::Idle && g_screen != Screen::NameEdit &&
      g_screen != Screen::MsgEdit && g_screen != Screen::Incoming && g_screen != Screen::Outgoing) {
    if ((lv_tick_get() - g_last_input_ms) >= to) go_idle();
  }
}

}  // namespace shell
}  // namespace wp
