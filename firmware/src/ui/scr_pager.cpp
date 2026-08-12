#include "ui/scr_pager.h"

#include "app/app.h"
#include "app/page_log.h"
#include "ui/chrome.h"
#include "ui/emoji_badge.h"
#include "ui/fonts.h"
#include "ui/nav.h"
#include "ui/scr_settings.h"
#include "ui/theme.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace wp {
namespace ui {
namespace {

/* —— compose state (web composeEmoji/composeMessage) —— */
app::Peer g_compose_peer;
char g_compose_emoji[proto::kMaxEmoji] = "";
char g_compose_message[proto::kMaxMessage] = "";
bool g_compose_fresh = true;

lv_obj_t * g_preview_img = nullptr;
lv_obj_t * g_preview_lbl = nullptr;
lv_obj_t * g_emoji_btns[app::kEmojiSlots] = {};
lv_obj_t * g_emoji_picker_btn = nullptr;
lv_obj_t * g_canned_btns[app::kCannedCount] = {};

bool emoji_is_slot(const char * emoji, int * out_idx = nullptr) {
  if (!emoji || !emoji[0]) return false;
  const app::Desk & d = app::desk();
  for (int i = 0; i < app::kEmojiSlots; ++i) {
    if (std::strcmp(d.emojis[i], emoji) == 0) {
      if (out_idx) *out_idx = i;
      return true;
    }
  }
  return false;
}

void style_emoji_btn(lv_obj_t * b, bool sel) {
  if (!b) return;
  lv_obj_set_style_border_width(b, sel ? 2 : 0, 0);
  lv_obj_set_style_border_color(b, theme::gold(), 0);
  lv_obj_set_style_bg_color(b, sel ? lv_color_mix(theme::gold(), theme::panel(), 60) : theme::panel(),
                            0);
}

void on_home(lv_event_t * /*e*/) { go_hub(); }

void on_peer(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  const app::Desk & d = app::desk();
  if (idx < 0 || idx >= d.peer_count) return;
  compose_mark_fresh();
  go_compose(d.peers[idx]);
}

void on_demo_ring() {
  /* QA helper: fake an incoming call (was Demo dock; now topbar easter egg). */
  proto::Msg m;
  m.type = proto::MsgType::Call;
  std::snprintf(m.from_id, sizeof(m.from_id), "mac-sim");
  std::snprintf(m.from_name, sizeof(m.from_name), "Ru");
  std::snprintf(m.emoji, sizeof(m.emoji), "👑");
  std::snprintf(m.message, sizeof(m.message), "Lipsync for your life");
  app::handle_msg(m);
}

/** Easter egg: WERKPAGER title, then name, three times quickly → demo incoming. */
constexpr uint32_t kDemoEggGapMs = 900;
constexpr int kDemoEggNeed = 6; /* title,name × 3 */
int g_demo_egg_step = 0;
uint32_t g_demo_egg_last = 0;

void demo_egg_tap(int which) {
  /* which: 0 = title, 1 = name */
  const uint32_t now = lv_tick_get();
  if (g_demo_egg_step > 0 && now - g_demo_egg_last > kDemoEggGapMs) g_demo_egg_step = 0;
  const int expect = (g_demo_egg_step % 2 == 0) ? 0 : 1;
  if (which != expect) {
    g_demo_egg_step = (which == 0) ? 1 : 0;
    g_demo_egg_last = now;
    return;
  }
  ++g_demo_egg_step;
  g_demo_egg_last = now;
  if (g_demo_egg_step >= kDemoEggNeed) {
    g_demo_egg_step = 0;
    on_demo_ring();
  }
}

void wire_demo_egg(lv_obj_t * topbar) {
  if (!topbar) return;
  lv_obj_t * left = static_cast<lv_obj_t *>(lv_obj_get_user_data(topbar));
  lv_obj_t * brand = nullptr;
  lv_obj_t * me = nullptr;
  if (left) {
    const uint32_t n = lv_obj_get_child_count(left);
    for (uint32_t i = 0; i < n; ++i) {
      lv_obj_t * c = lv_obj_get_child(left, i);
      if (reinterpret_cast<intptr_t>(lv_obj_get_user_data(c)) == 1) brand = c;
    }
  }
  const uint32_t tn = lv_obj_get_child_count(topbar);
  for (uint32_t i = 0; i < tn; ++i) {
    lv_obj_t * c = lv_obj_get_child(topbar, i);
    if (reinterpret_cast<intptr_t>(lv_obj_get_user_data(c)) != 3) continue;
    if (lv_obj_get_child_count(c) > 0) me = lv_obj_get_child(c, 0);
    break;
  }
  auto bind = [](lv_obj_t * obj, int which) {
    if (!obj) return;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(obj, 8);
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t * e) {
          demo_egg_tap((int)(intptr_t)lv_event_get_user_data(e));
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)which);
  };
  bind(brand, 0);
  bind(me, 1);
}

void refresh_compose_styles() {
  const app::Desk & d = app::desk();
  int slot = -1;
  const bool has = g_compose_emoji[0] != '\0';
  const bool from_slot = has && emoji_is_slot(g_compose_emoji, &slot);
  for (int i = 0; i < app::kEmojiSlots; ++i) {
    style_emoji_btn(g_emoji_btns[i], from_slot && i == slot);
  }
  style_emoji_btn(g_emoji_picker_btn, has && !from_slot);
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
    if (has) {
      lv_obj_remove_flag(g_preview_img, LV_OBJ_FLAG_HIDDEN);
      if (lv_obj_check_type(g_preview_img, &lv_image_class)) {
        if (const void * src = emoji_image_src(g_compose_emoji)) lv_image_set_src(g_preview_img, src);
      }
    } else {
      lv_obj_add_flag(g_preview_img, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void clear_compose_widget_ptrs() {
  g_preview_img = nullptr;
  g_preview_lbl = nullptr;
  g_emoji_picker_btn = nullptr;
  for (int i = 0; i < app::kEmojiSlots; ++i) g_emoji_btns[i] = nullptr;
  for (int i = 0; i < app::kCannedCount; ++i) g_canned_btns[i] = nullptr;
}

void on_emoji(lv_event_t * e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= app::kEmojiSlots) return;
  const char * emo = app::desk().emojis[idx];
  if (std::strcmp(g_compose_emoji, emo) == 0) g_compose_emoji[0] = '\0';
  else std::snprintf(g_compose_emoji, sizeof(g_compose_emoji), "%s", emo);
  refresh_compose_styles();
}

void on_emoji_picker(lv_event_t * /*e*/) { go_emoji_picker(kEmojiPickerCompose); }

/** Flat phone-style emoji-key glyph — soft AA circle / eyes / smile on canvas. */
lv_obj_t * make_smiley_picker_icon(lv_obj_t * parent) {
  constexpr int kS = 40;
  static uint8_t s_buf[kS * kS * 4];
  std::memset(s_buf, 0, sizeof(s_buf));

  const lv_color32_t ink = lv_color_to_32(theme::ink(), LV_OPA_COVER);
  auto put = [&](int x, int y, float a) {
    if (a <= 0.01f || x < 0 || y < 0 || x >= kS || y >= kS) return;
    if (a > 1.f) a = 1.f;
    auto * px = reinterpret_cast<lv_color32_t *>(s_buf + ((size_t)y * kS + (size_t)x) * 4u);
    const float src = a; /* full ink — reads clearly next to Twemoji */
    const float dst = (float)px->alpha / 255.f;
    const float out = src + dst * (1.f - src);
    if (out <= 0.f) return;
    px->red = ink.red;
    px->green = ink.green;
    px->blue = ink.blue;
    px->alpha = (uint8_t)(out * 255.f + 0.5f);
  };

  auto cover_disc = [](float d, float r) -> float {
    constexpr float aa = 0.85f;
    if (d <= r - aa) return 1.f;
    if (d >= r + aa) return 0.f;
    return 1.f - (d - (r - aa)) / (2.f * aa);
  };
  auto cover_ring = [](float d, float r_out, float stroke) -> float {
    const float r_in = r_out - stroke;
    constexpr float aa = 0.85f;
    float outer = 1.f;
    if (d >= r_out + aa) outer = 0.f;
    else if (d > r_out - aa) outer = 1.f - (d - (r_out - aa)) / (2.f * aa);
    float inner = 1.f;
    if (d <= r_in - aa) inner = 0.f;
    else if (d < r_in + aa) inner = (d - (r_in - aa)) / (2.f * aa);
    return outer * inner;
  };
  auto cover_arc = [](float d, float ang, float r, float stroke, float a0, float a1) -> float {
    if (ang < a0 || ang > a1) return 0.f;
    const float half = stroke * 0.5f;
    constexpr float aa = 0.85f;
    const float dist = std::fabs(d - r);
    if (dist <= half - aa) return 1.f;
    if (dist >= half + aa) return 0.f;
    return 1.f - (dist - (half - aa)) / (2.f * aa);
  };

  const float cx = (kS - 1) * 0.5f;
  const float cy = (kS - 1) * 0.5f;
  constexpr float kFaceR = 16.2f;
  constexpr float kStroke = 2.6f;

  for (int y = 0; y < kS; ++y) {
    for (int x = 0; x < kS; ++x) {
      const float px = (float)x + 0.5f;
      const float py = (float)y + 0.5f;
      const float dx = px - cx;
      const float dy = py - cy;
      const float d = std::sqrt(dx * dx + dy * dy);
      put(x, y, cover_ring(d, kFaceR, kStroke));

      /* Eyes — slightly above center, evenly spaced. */
      const float elx = cx - 5.4f;
      const float erx = cx + 5.4f;
      const float ey = cy - 3.4f;
      put(x, y, cover_disc(std::sqrt((px - elx) * (px - elx) + (py - ey) * (py - ey)), 2.25f));
      put(x, y, cover_disc(std::sqrt((px - erx) * (px - erx) + (py - ey) * (py - ey)), 2.25f));

      /* Smile arc in the lower half (screen y-down). */
      const float mx = cx;
      const float my = cy + 1.4f;
      const float mdx = px - mx;
      const float mdy = py - my;
      const float md = std::sqrt(mdx * mdx + mdy * mdy);
      const float ang = std::atan2(mdy, mdx); /* -pi..pi, +y down */
      put(x, y, cover_arc(md, ang, 8.0f, 2.45f, 0.42f, 2.72f));
    }
  }

  lv_obj_t * canvas = lv_canvas_create(parent);
  lv_obj_set_size(canvas, kS, kS);
  lv_canvas_set_buffer(canvas, s_buf, kS, kS, LV_COLOR_FORMAT_ARGB8888);
  lv_obj_center(canvas);
  lv_obj_remove_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
  return canvas;
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
  if (g_compose_emoji[0])
    std::snprintf(m.emoji, sizeof(m.emoji), "%s", g_compose_emoji);
  std::snprintf(m.message, sizeof(m.message), "%s", g_compose_message);

  d.outgoing.active = true;
  std::snprintf(d.outgoing.to_id, sizeof(d.outgoing.to_id), "%s", g_compose_peer.id);
  std::snprintf(d.outgoing.to_name, sizeof(d.outgoing.to_name), "%s", g_compose_peer.name);
  std::snprintf(d.outgoing.emoji, sizeof(d.outgoing.emoji), "%s", m.emoji);
  std::snprintf(d.outgoing.message, sizeof(d.outgoing.message), "%s", m.message);

  page_log::add(page_log::Dir::Out, g_compose_peer.name, m.emoji, m.message);
  app::send(m);
  clear_compose_widget_ptrs(); /* compose is about to be deleted — no dangling refresh */
  go_outgoing();
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
  toast("Acknowledged");
}

void on_sashay(lv_event_t * /*e*/) {
  /* Kept for sim/demo; glass UI uses a single Acknowledge action. */
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
  auto * o = static_cast<lv_obj_t *>(obj);
  /* Device emoji stub is a plain obj+label — lv_image_set_scale would corrupt LVGL. */
  if (!o || !lv_obj_check_type(o, &lv_image_class)) return;
  lv_image_set_scale(o, (uint32_t)v);
}

}  // namespace

lv_obj_t * pager_werk_screen() {
  const app::Desk & d = app::desk();
  lv_obj_t * scr = make_screen();
  lv_obj_t * top = make_topbar(scr, "WERKPAGER", d.name);
  wire_demo_egg(top);
  g_demo_egg_step = 0;
  lv_obj_t * body = make_body(scr, true);

  lv_obj_t * tag = lv_label_create(body);
  lv_label_set_text(tag, "Who are we bothering?");
  lv_obj_set_style_text_color(tag, theme::ink(), 0);
  lv_obj_set_style_text_font(tag, &lv_font_montserrat_20, 0);
  lv_obj_set_width(tag, lv_pct(100));

  if (d.peer_count == 0) {
    make_tagline(body, "No peers saved yet. Scan or add from Settings.");
  }

  /* With ≤3 peers, each row is ~1/3 of the scroll body's content height. */
  const int body_h = WP_VER_RES - kTopbarH - kDockH;
  const int content_h = body_h - 6 - 16; /* make_body pad_top / pad_bottom */
  const int tag_reserve = 28 + 8;        /* tagline + pad_row */
  const int row_h = (content_h - tag_reserve) / 3;
  const bool tall_rows = d.peer_count > 0 && d.peer_count <= 3;

  for (int i = 0; i < d.peer_count; ++i) {
    lv_obj_t * btn = make_peer_btn(body, d.peers[i].name, nullptr, on_peer, (void *)(intptr_t)i);
    if (!tall_rows) continue;
    lv_obj_set_height(btn, row_h);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t * name = lv_obj_get_child(btn, 0);
    if (name) lv_obj_set_style_text_font(name, &lv_font_montserrat_36, 0);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Home", false, false, on_home);
  dock_btn(dock, "History", false, false, [](lv_event_t * /*e*/) { go_page_history(); });
  return scr;
}

void compose_mark_fresh() { g_compose_fresh = true; }

const char * compose_message() { return g_compose_message; }

void compose_set_message(const char * msg) {
  std::snprintf(g_compose_message, sizeof(g_compose_message), "%s", msg ? msg : "");
}

const char * compose_emoji() { return g_compose_emoji; }

void compose_set_emoji(const char * emoji) {
  if (emoji && emoji[0])
    std::snprintf(g_compose_emoji, sizeof(g_compose_emoji), "%s", emoji);
  else
    g_compose_emoji[0] = '\0';
}

lv_obj_t * pager_compose_screen(const app::Peer & peer) {
  clear_compose_widget_ptrs();
  g_compose_peer = peer;
  const app::Desk & d = app::desk();
  if (g_compose_fresh) {
    g_compose_emoji[0] = '\0';
    g_compose_message[0] = '\0';
    g_compose_fresh = false;
  }

  lv_obj_t * scr = make_screen();
  make_topbar(scr, "WerkRoom", d.name);
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

  /* Last slot: open full emoji palette (flat smiley = emoji-keyboard affordance). */
  g_emoji_picker_btn = lv_button_create(emo_row);
  lv_obj_set_size(g_emoji_picker_btn, 52, 52);
  lv_obj_set_style_radius(g_emoji_picker_btn, 12, 0);
  lv_obj_set_style_bg_color(g_emoji_picker_btn, theme::panel(), 0);
  lv_obj_set_style_shadow_width(g_emoji_picker_btn, 0, 0);
  lv_obj_set_style_border_width(g_emoji_picker_btn, 1, 0);
  lv_obj_set_style_border_color(g_emoji_picker_btn, theme::border(), 0);
  lv_obj_set_style_pad_all(g_emoji_picker_btn, 0, 0);
  make_smiley_picker_icon(g_emoji_picker_btn);
  lv_obj_add_event_cb(g_emoji_picker_btn, on_emoji_picker, LV_EVENT_CLICKED, nullptr);

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
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) {
    if (app::desk().outgoing.active || app::desk().incoming.active) {
      sync_ui();
      return;
    }
    go_werk();
  });
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
  lv_obj_add_flag(wash, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(wash, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(wash, on_shantay, LV_EVENT_CLICKED, nullptr);
  {
    /* FULL framebuffer (device) makes this wash pulse smooth like the PC sim. */
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
  /* Nudge rings down so the caller name sits nearer the pulse center. */
  lv_obj_align(stage, LV_ALIGN_TOP_MID, 0, (WP_VER_RES * 42) / 100 - kStage / 2 + 22);
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
  lv_obj_set_size(body, WP_HOR_RES, WP_VER_RES);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 8, 0);
  /* Bias content upward so the name sits in the pulse center. */
  lv_obj_set_style_pad_top(body, 8, 0);
  lv_obj_set_style_pad_bottom(body, 110, 0); /* dock bar + captions */
  lv_obj_remove_flag(body, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(body);

  lv_obj_t * eye = lv_label_create(body);
  lv_label_set_text(eye, "INCOMING");
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
  if (lv_obj_check_type(emoji_img, &lv_image_class)) {
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

  /* Single green Acknowledge pill (product: one clear action, not Shantay/Sashay pair). */
  lv_obj_t * dock = make_dock(scr);
  lv_obj_set_height(dock, 64);
  lv_obj_set_style_pad_ver(dock, 8, 0);
  lv_obj_move_foreground(dock);
  lv_obj_t * ack = dock_btn(dock, "Acknowledge", false, false, on_shantay);
  lv_obj_set_height(ack, 48);
  lv_obj_set_style_bg_color(ack, lv_color_hex(0x22c55e), 0);
  lv_obj_set_style_bg_opa(ack, LV_OPA_COVER, 0);
  if (lv_obj_t * lbl = lv_obj_get_child(ack, 0)) {
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0a1a10), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  }

  return scr;
}

}  // namespace ui
}  // namespace wp
