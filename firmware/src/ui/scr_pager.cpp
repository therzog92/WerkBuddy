#include "ui/scr_pager.h"

#include "app/app.h"
#include "ui/chrome.h"
#include "ui/emoji_badge.h"
#include "ui/fonts.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cstdio>
#include <cstring>

namespace wp {
namespace ui {
namespace {

/* —— compose state (web composeEmoji/composeMessage) —— */
app::Peer g_compose_peer;
int g_compose_emoji = -1; /* index into desk emojis, -1 = none */
char g_compose_message[proto::kMaxMessage] = "";
bool g_compose_fresh = true;

lv_obj_t * g_preview_img = nullptr;
lv_obj_t * g_preview_lbl = nullptr;
lv_obj_t * g_emoji_btns[app::kEmojiSlots] = {};
lv_obj_t * g_canned_btns[app::kCannedCount] = {};

void on_home(lv_event_t * /*e*/) { go_hub(); }

void on_peer(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  const app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  compose_mark_fresh();
  go_compose(d.peers[idx]);
}

void on_demo_ring(lv_event_t * /*e*/) {
  /* QA helper: fake an incoming call (web btnSimIncoming) */
  proto::Msg m;
  m.type = proto::MsgType::Call;
  std::snprintf(m.from_id, sizeof(m.from_id), "mac-sim");
  std::snprintf(m.from_name, sizeof(m.from_name), "Ru");
  std::snprintf(m.emoji, sizeof(m.emoji), "👑");
  std::snprintf(m.message, sizeof(m.message), "Lipsync for your life");
  app::handle_msg(m);
}

void refresh_compose_styles() {
  const app::Desk & d = app::desk();
  for (int i = 0; i < app::kEmojiSlots; ++i) {
    lv_obj_t * b = g_emoji_btns[i];
    if (!b) continue;
    const bool sel = i == g_compose_emoji;
    lv_obj_set_style_border_width(b, sel ? 2 : 0, 0);
    lv_obj_set_style_border_color(b, theme::gold(), 0);
    lv_obj_set_style_bg_color(b, sel ? lv_color_mix(theme::gold(), theme::panel(), 60) : theme::panel(), 0);
  }
  for (int i = 0; i < app::kCannedCount; ++i) {
    lv_obj_t * b = g_canned_btns[i];
    if (!b) continue;
    const bool sel = std::strcmp(d.canned[i], g_compose_message) == 0;
    lv_obj_set_style_border_color(b, sel ? theme::gold() : theme::border(), 0);
    lv_obj_set_style_border_width(b, sel ? 2 : 1, 0);
  }
  if (g_preview_lbl) {
    lv_label_set_text(g_preview_lbl, g_compose_message[0] ? g_compose_message : "No message");
    lv_obj_set_style_text_color(g_preview_lbl, g_compose_message[0] ? theme::ink() : theme::muted(), 0);
  }
  if (g_preview_img) {
    if (g_compose_emoji >= 0) {
      lv_obj_remove_flag(g_preview_img, LV_OBJ_FLAG_HIDDEN);
      lv_image_set_src(g_preview_img, emoji_png_path(d.emojis[g_compose_emoji]));
    } else {
      lv_obj_add_flag(g_preview_img, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void on_emoji(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  g_compose_emoji = (g_compose_emoji == idx) ? -1 : idx; /* toggle, like web */
  refresh_compose_styles();
}

void on_canned(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  std::snprintf(g_compose_message, sizeof(g_compose_message), "%s", app::desk().canned[idx]);
  refresh_compose_styles();
}

void on_clear_message(lv_event_t * /*e*/) {
  g_compose_message[0] = '\0';
  refresh_compose_styles();
}

void on_custom_message(lv_event_t * /*e*/) { go_keyboard_compose(); }

void on_send(lv_event_t * /*e*/) {
  app::Desk & d = app::desk();
  if (d.outgoing.active || d.incoming.active) return;

  proto::Msg m;
  m.type = proto::MsgType::Call;
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", d.id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", d.name);
  std::snprintf(m.to_id, sizeof(m.to_id), "%s", g_compose_peer.id);
  if (g_compose_emoji >= 0)
    std::snprintf(m.emoji, sizeof(m.emoji), "%s", d.emojis[g_compose_emoji]);
  std::snprintf(m.message, sizeof(m.message), "%s", g_compose_message);

  d.outgoing.active = true;
  std::snprintf(d.outgoing.to_id, sizeof(d.outgoing.to_id), "%s", g_compose_peer.id);
  std::snprintf(d.outgoing.to_name, sizeof(d.outgoing.to_name), "%s", g_compose_peer.name);
  std::snprintf(d.outgoing.emoji, sizeof(d.outgoing.emoji), "%s", m.emoji);
  std::snprintf(d.outgoing.message, sizeof(d.outgoing.message), "%s", m.message);

  app::send(m);
  sync_ui();
}

void on_cancel_ping(lv_event_t * /*e*/) {
  app::Desk & d = app::desk();
  if (!d.outgoing.active) return;
  proto::Msg m;
  m.type = proto::MsgType::Clear;
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", d.id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", d.name);
  app::send(m);
  d.outgoing.active = false;
  sync_ui();
  toast("Ping cancelled");
}

void on_shantay(lv_event_t * /*e*/) {
  app::Desk & d = app::desk();
  if (!d.incoming.active) return;
  proto::Msg m;
  m.type = proto::MsgType::Ack;
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", d.id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", d.name);
  std::snprintf(m.to_id, sizeof(m.to_id), "%s", d.incoming.from_id);
  std::snprintf(m.for_call_from_id, sizeof(m.for_call_from_id), "%s", d.incoming.from_id);
  app::send(m);
  d.incoming.active = false;
  sync_ui();
  toast("Shantay - they know");
}

void on_sashay(lv_event_t * /*e*/) {
  /* web btnClear: dismiss locally, no message */
  app::desk().incoming.active = false;
  sync_ui();
}

void build_pulse(lv_obj_t * parent) {
  /*
   * Web .outgoing-pulse: fixed 72px element, ring scales 0.7→1.35 via transform
   * (no layout shift): fixed holder in the flex flow, animated overflow child.
   */
  lv_obj_t * holder = lv_obj_create(parent);
  lv_obj_remove_style_all(holder);
  lv_obj_set_size(holder, 76, 76);
  lv_obj_set_style_margin_top(holder, 8, 0);
  lv_obj_add_flag(holder, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_remove_flag(holder, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(holder, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * pulse = lv_obj_create(holder);
  lv_obj_remove_style_all(pulse);
  lv_obj_set_size(pulse, 50, 50);
  lv_obj_set_style_radius(pulse, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(pulse, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(pulse, 3, 0);
  lv_obj_set_style_border_color(pulse, theme::hot(), 0);
  lv_obj_set_style_border_opa(pulse, LV_OPA_COVER, 0);
  lv_obj_center(pulse);
  lv_obj_remove_flag(pulse, LV_OBJ_FLAG_CLICKABLE);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, pulse);
  lv_anim_set_values(&a, 50, 97);
  lv_anim_set_duration(&a, 1100);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void * obj, int32_t v) {
    auto * o = static_cast<lv_obj_t *>(obj);
    lv_obj_set_size(o, v, v);
    lv_obj_center(o);
    const lv_opa_t opa = (lv_opa_t)(LV_OPA_COVER - (v - 50) * LV_OPA_COVER / (97 - 50));
    lv_obj_set_style_border_opa(o, opa, 0);
  });
  lv_anim_start(&a);
}

/* Incoming FX stage — rings + glow share this center (50% / 42%). */
constexpr lv_coord_t kIncomingStage = 360;

void anim_ring_size(void * obj, int32_t v) {
  auto * ring = static_cast<lv_obj_t *>(obj);
  constexpr int32_t half = kIncomingStage / 2;
  lv_obj_set_size(ring, v, v);
  lv_obj_set_pos(ring, half - v / 2, half - v / 2);
}

void anim_ring_opa(void * obj, int32_t v) {
  lv_obj_set_style_border_opa(static_cast<lv_obj_t *>(obj), (lv_opa_t)v, 0);
}

void anim_bg_mix(void * obj, int32_t v) {
  auto * wash = static_cast<lv_obj_t *>(obj);
  const lv_color_t c = lv_color_mix(theme::call_b(), theme::call_a(), (lv_opa_t)v);
  lv_obj_set_style_bg_color(wash, c, 0);
  lv_obj_invalidate(wash);
}

void anim_blink_opa(void * obj, int32_t v) {
  lv_obj_set_style_text_opa(static_cast<lv_obj_t *>(obj), (lv_opa_t)v, 0);
}

void anim_glow(void * obj, int32_t v) {
  auto * glow = static_cast<lv_obj_t *>(obj);
  constexpr int32_t kBase = 220;
  constexpr int32_t half_stage = kIncomingStage / 2;
  const int32_t size = kBase * (85 + v * 30 / 100) / 100;
  lv_obj_set_size(glow, size, size);
  lv_obj_set_pos(glow, half_stage - size / 2, half_stage - size / 2);
  lv_obj_set_style_opa(glow, (lv_opa_t)(56 + v * 51 / 100), 0);
}

void anim_emoji_pop(void * obj, int32_t v) {
  lv_image_set_scale(static_cast<lv_obj_t *>(obj), (uint32_t)v);
}

}  // namespace

lv_obj_t * pager_werk_screen() {
  const app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "WERKPAGER", d.name);
  lv_obj_t * body = make_body(scr, true);
  make_tagline(body, "Who are we bothering?");

  if (d.peer_count == 0) {
    make_tagline(body, "No peers saved yet. Scan or add from Settings.");
  }
  for (int i = 0; i < d.peer_count; ++i) {
    make_peer_btn(body, d.peers[i].name, "compose a ping", on_peer, (void *)(intptr_t)i);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Home", false, false, on_home);
  dock_btn(dock, "Demo ring", false, false, on_demo_ring);
  return scr;
}

void compose_mark_fresh() { g_compose_fresh = true; }

const char * compose_message() { return g_compose_message; }

void compose_set_message(const char * msg) {
  std::snprintf(g_compose_message, sizeof(g_compose_message), "%s", msg ? msg : "");
}

lv_obj_t * pager_compose_screen(const app::Peer & peer) {
  g_compose_peer = peer;
  const app::Desk & d = app::desk();
  if (g_compose_fresh) {
    g_compose_emoji = -1;
    g_compose_message[0] = '\0';
    g_compose_fresh = false;
  }

  lv_obj_t * scr = make_screen();
  make_topbar(scr, "WERK ROOM", d.name);
  lv_obj_t * body = make_body(scr, true);

  char heading[48];
  lv_snprintf(heading, sizeof(heading), "Ping to %s", peer.name);
  make_tagline(body, heading);

  lv_obj_t * emo_row = lv_obj_create(body);
  lv_obj_remove_style_all(emo_row);
  lv_obj_set_width(emo_row, lv_pct(100));
  lv_obj_set_height(emo_row, 56);
  lv_obj_set_flex_flow(emo_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(emo_row, 6, 0);
  lv_obj_add_flag(emo_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(emo_row, LV_DIR_HOR);

  for (int i = 0; i < app::kEmojiSlots; ++i) {
    lv_obj_t * b = lv_button_create(emo_row);
    lv_obj_set_size(b, 52, 52);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 2, 0);
    lv_obj_t * img = make_emoji_image(b, d.emojis[i], 44);
    lv_obj_center(img);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, on_emoji, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    g_emoji_btns[i] = b;
  }

  /* Clear / Custom — replace the old default "Quick ping" affordance */
  lv_obj_t * msg_actions = lv_obj_create(body);
  lv_obj_remove_style_all(msg_actions);
  lv_obj_set_width(msg_actions, lv_pct(100));
  lv_obj_set_height(msg_actions, 42);
  lv_obj_set_flex_flow(msg_actions, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(msg_actions, 8, 0);
  lv_obj_remove_flag(msg_actions, LV_OBJ_FLAG_SCROLLABLE);

  auto make_msg_action = [](lv_obj_t * parent, const char * label, lv_event_cb_t cb) {
    lv_obj_t * b = lv_button_create(parent);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, 40);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, theme::border(), 0);
    lv_obj_set_style_pad_hor(b, 6, 0);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, theme::ink(), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
  };
  make_msg_action(msg_actions, LV_SYMBOL_TRASH " Clear Message", on_clear_message);
  make_msg_action(msg_actions, LV_SYMBOL_EDIT " Custom message", on_custom_message);

  lv_obj_t * canned = lv_obj_create(body);
  lv_obj_remove_style_all(canned);
  lv_obj_set_width(canned, lv_pct(100));
  lv_obj_set_flex_grow(canned, 1);
  lv_obj_set_flex_flow(canned, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(canned, 6, 0);

  for (int i = 0; i < app::kCannedCount; ++i) {
    lv_obj_t * b = lv_button_create(canned);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_height(b, 40);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, theme::border(), 0);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, d.canned[i]);
    lv_obj_set_style_text_color(l, theme::ink(), 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, on_canned, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    g_canned_btns[i] = b;
  }

  /* Preview row: selected emoji + message (web #composePreview) */
  lv_obj_t * prev_row = lv_obj_create(body);
  lv_obj_remove_style_all(prev_row);
  lv_obj_set_size(prev_row, lv_pct(100), 26);
  lv_obj_set_flex_flow(prev_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(prev_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(prev_row, 8, 0);
  lv_obj_remove_flag(prev_row, LV_OBJ_FLAG_SCROLLABLE);

  g_preview_img = make_emoji_image(prev_row, d.emojis[0], 20);
  lv_obj_add_flag(g_preview_img, LV_OBJ_FLAG_HIDDEN);

  g_preview_lbl = lv_label_create(prev_row);
  lv_obj_set_style_text_color(g_preview_lbl, theme::ink(), 0);
  lv_obj_set_style_text_font(g_preview_lbl, &lv_font_montserrat_16, 0);

  refresh_compose_styles();

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_werk(); });
  dock_btn(dock, "Send", true, false, on_send);
  return scr;
}

lv_obj_t * pager_outgoing_screen() {
  const app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "WERKPAGER", d.name);

  lv_obj_t * body = make_body(scr, true);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t * eye = lv_label_create(body);
  lv_label_set_text(eye, "Calling");
  lv_obj_set_style_text_color(eye, theme::muted(), 0);

  lv_obj_t * name = lv_label_create(body);
  lv_label_set_text(name, d.outgoing.to_name[0] ? d.outgoing.to_name : "?");
  lv_obj_set_style_text_color(name, theme::gold(), 0);
  lv_obj_set_style_text_font(name, &lv_font_montserrat_28, 0);

  /* Web #outgoingDetail: `${emoji} · ${message}` — emoji rendered as image */
  lv_obj_t * sub_row = lv_obj_create(body);
  lv_obj_remove_style_all(sub_row);
  lv_obj_set_size(sub_row, LV_SIZE_CONTENT, 24);
  lv_obj_set_flex_flow(sub_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(sub_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(sub_row, 7, 0);
  lv_obj_remove_flag(sub_row, LV_OBJ_FLAG_SCROLLABLE);

  const bool has_emoji = d.outgoing.emoji[0] != '\0';
  const bool has_msg = d.outgoing.message[0] != '\0';
  if (has_emoji) make_emoji_image(sub_row, d.outgoing.emoji, 18);
  lv_obj_t * sub = lv_label_create(sub_row);
  lv_label_set_text(sub, has_msg ? d.outgoing.message : "Waiting for them to notice...");
  lv_obj_set_style_text_color(sub, theme::muted(), 0);
  lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);

  build_pulse(body);

  lv_obj_t * hint = lv_label_create(body);
  lv_label_set_text(hint, "Waiting for them to notice...");
  lv_obj_set_style_text_color(hint, theme::muted(), 0);
  lv_obj_set_style_text_font(hint, font_body_italic(14), 0);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Cancel ping", false, true, on_cancel_ping);
  return scr;
}

lv_obj_t * pager_incoming_screen() {
  const app::Desk & d = app::desk();
  const char * from = d.incoming.from_name[0] ? d.incoming.from_name : "Will";
  const char * emoji = d.incoming.emoji[0] ? d.incoming.emoji : "\xF0\x9F\x93\xA2"; /* 📢 */
  const char * message = d.incoming.message[0] ? d.incoming.message : "is calling your desk";

  /*
   * Match web/paging-lvgl-preview.html ".lvgl" board mock:
   * theme call_a↔call_b flash + one soft pulsing glow + radar rings.
   */
  lv_obj_t * scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_size(scr, WP_HOR_RES, WP_VER_RES);
  lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * wash = lv_obj_create(scr);
  lv_obj_remove_style_all(wash);
  lv_obj_set_size(wash, WP_HOR_RES, WP_VER_RES);
  lv_obj_set_pos(wash, 0, 0);
  lv_obj_set_style_bg_color(wash, theme::call_a(), 0);
  lv_obj_set_style_bg_opa(wash, LV_OPA_COVER, 0);
  lv_obj_remove_flag(wash, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(wash, LV_OBJ_FLAG_SCROLLABLE);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, wash);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 550);
    lv_anim_set_reverse_duration(&a, 550);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_bg_mix);
    lv_anim_start(&a);
  }

  constexpr lv_coord_t kStage = kIncomingStage;
  constexpr lv_coord_t kRing = 100;
  lv_obj_t * stage = lv_obj_create(scr);
  lv_obj_remove_style_all(stage);
  lv_obj_set_size(stage, kStage, kStage);
  lv_obj_set_style_bg_opa(stage, LV_OPA_TRANSP, 0);
  lv_obj_align(stage, LV_ALIGN_TOP_MID, 0, (WP_VER_RES * 42) / 100 - kStage / 2);
  lv_obj_remove_flag(stage, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(stage, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * glow = lv_obj_create(stage);
  lv_obj_remove_style_all(glow);
  lv_obj_set_size(glow, 220, 220);
  lv_obj_set_pos(glow, kStage / 2 - 110, kStage / 2 - 110);
  lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(glow, 0, 0);
  lv_obj_set_style_bg_color(glow, theme::hot(), 0);
  lv_obj_set_style_bg_opa(glow, LV_OPA_COVER, 0);
  lv_obj_set_style_opa(glow, LV_OPA_30, 0);
  lv_obj_remove_flag(glow, LV_OBJ_FLAG_CLICKABLE);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, glow);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_duration(&a, 1100);
    lv_anim_set_reverse_duration(&a, 1100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_glow);
    lv_anim_start(&a);
  }

  const lv_color_t ring_colors[3] = {lv_color_hex(0xffffff), theme::gold(), theme::mint()};
  const int32_t ring_delays[3] = {0, 420, 840};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t * ring = lv_obj_create(stage);
    lv_obj_remove_style_all(ring);
    const int32_t start = (kRing * 60) / 100;
    const int32_t end = kRing * 3;
    lv_obj_set_size(ring, start, start);
    lv_obj_set_pos(ring, kStage / 2 - start / 2, kStage / 2 - start / 2);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 4, 0);
    lv_obj_set_style_border_color(ring, ring_colors[i], 0);
    lv_obj_set_style_border_opa(ring, (lv_opa_t)(255 * 85 / 100), 0);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    lv_anim_t as;
    lv_anim_init(&as);
    lv_anim_set_var(&as, ring);
    lv_anim_set_values(&as, start, end);
    lv_anim_set_duration(&as, 1250);
    lv_anim_set_delay(&as, ring_delays[i]);
    lv_anim_set_repeat_count(&as, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&as, lv_anim_path_linear);
    lv_anim_set_exec_cb(&as, anim_ring_size);
    lv_anim_start(&as);

    lv_anim_t ao;
    lv_anim_init(&ao);
    lv_anim_set_var(&ao, ring);
    lv_anim_set_values(&ao, (int32_t)(255 * 85 / 100), LV_OPA_TRANSP);
    lv_anim_set_duration(&ao, 1250);
    lv_anim_set_delay(&ao, ring_delays[i]);
    lv_anim_set_repeat_count(&ao, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&ao, lv_anim_path_linear);
    lv_anim_set_exec_cb(&ao, anim_ring_opa);
    lv_anim_start(&ao);
  }

  lv_obj_t * body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, WP_HOR_RES, WP_VER_RES - kDockH);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 8, 0);
  lv_obj_set_style_pad_top(body, 28, 0);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(body);

  lv_obj_t * eye = lv_label_create(body);
  lv_label_set_text(eye, "INCOMING WERK");
  lv_obj_set_style_text_color(eye, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(eye, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_letter_space(eye, 4, 0);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, eye);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
    lv_anim_set_duration(&a, 250);
    lv_anim_set_playback_duration(&a, 250);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_blink_opa);
    lv_anim_start(&a);
  }

  constexpr lv_coord_t kEmoji = 48;
  lv_obj_t * emoji_img = make_emoji_image(body, emoji, kEmoji);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, emoji_img);
    const int32_t base = (kEmoji * 256) / 72;
    lv_anim_set_values(&a, base, (base * 112) / 100);
    lv_anim_set_duration(&a, 900);
    lv_anim_set_playback_duration(&a, 900);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_emoji_pop);
    lv_anim_start(&a);
  }

  lv_obj_t * name = lv_label_create(body);
  lv_label_set_text(name, from);
  lv_obj_set_style_text_color(name, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(name, font_display(56), 0);
  lv_obj_set_style_text_letter_space(name, 2, 0);

  lv_obj_t * msg = lv_label_create(body);
  lv_label_set_text(msg, message);
  lv_obj_set_style_text_color(msg, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_opa(msg, LV_OPA_COVER, 0);
  lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * hint = lv_label_create(body);
  lv_label_set_text(hint, "Shantay you stay... or sashay away");
  lv_obj_set_style_text_color(hint, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_opa(hint, LV_OPA_70, 0);
  lv_obj_set_style_text_font(hint, font_body_italic(14), 0);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * dock = make_dock(scr);
  lv_obj_t * yes = dock_btn(dock, "Shantay", true, false, on_shantay);
  lv_obj_set_style_radius(yes, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(yes, lv_color_hex(0xf0c24b), 0);
  lv_obj_set_height(yes, 48);

  lv_obj_t * no = dock_btn(dock, "Sashay away", false, true, on_sashay);
  lv_obj_set_style_radius(no, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(no, lv_color_hex(0xff6b8a), 0);
  lv_obj_set_height(no, 48);
  const uint32_t cnt = lv_obj_get_child_count(no);
  for (uint32_t i = 0; i < cnt; ++i) {
    lv_obj_t * ch = lv_obj_get_child(no, i);
    if (lv_obj_check_type(ch, &lv_label_class)) {
      lv_obj_set_style_text_color(ch, lv_color_hex(0x2a0610), 0);
    }
  }

  return scr;
}

}  // namespace ui
}  // namespace wp
