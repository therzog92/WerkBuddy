#include "ui/scr_doodle.h"

#include "app/app.h"
#include "protocol/messages.h"
#include "ui/chrome.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef WP_DEVICE
#include <esp_heap_caps.h>
#endif

namespace wp {
namespace ui {
namespace {

constexpr int kCanvasW = 420; /* body content ~452 after pad */
constexpr int kCanvasH = 280;
constexpr int kQ = 120;

lv_obj_t * g_canvas = nullptr;
lv_obj_t * g_tools = nullptr;
uint32_t g_screen_gen = 0; /* bumps each doodle_screen(); guards async delete */
int g_color = 1;
int g_width = 2;
bool g_erase = false;
uint16_t g_stroke_id = 1;

uint8_t g_pts[proto::kMaxStrokePts * 2];
uint8_t g_n_pts = 0;
bool g_drawing = false;
int g_last_x = -1, g_last_y = -1;

struct SavedStroke {
  int8_t color;
  uint8_t w;
  uint8_t n;
  uint8_t pts[proto::kMaxStrokePts * 2];
};

constexpr int kMaxPads = app::kMaxPeers;
constexpr int kMaxStrokes = 400;

struct PeerPad {
  char id[proto::kMaxId] = {};
  std::vector<SavedStroke> strokes;
};

PeerPad g_pads[kMaxPads];
int g_pad_n = 0;
char g_bound_peer[proto::kMaxId] = {};

PeerPad * pad_for(const char * id, bool create) {
  if (!id || !id[0]) return nullptr;
  for (int i = 0; i < g_pad_n; ++i) {
    if (std::strcmp(g_pads[i].id, id) == 0) return &g_pads[i];
  }
  if (!create || g_pad_n >= kMaxPads) return nullptr;
  PeerPad & p = g_pads[g_pad_n++];
  std::snprintf(p.id, sizeof(p.id), "%s", id);
  p.strokes.clear();
  return &p;
}

void remember_pts(const char * peer_id, const uint8_t * pts, uint8_t n, int8_t color, uint8_t w) {
  if (n < 1) return;
  PeerPad * pad = pad_for(peer_id, true);
  if (!pad) return;
  if ((int)pad->strokes.size() >= kMaxStrokes) {
    pad->strokes.erase(pad->strokes.begin(), pad->strokes.begin() + 40);
  }
  SavedStroke s;
  s.color = color;
  s.w = w;
  s.n = n;
  std::memcpy(s.pts, pts, (size_t)n * 2);
  pad->strokes.push_back(s);
}

/* Aligned canvas buffer — stride-aware size required by LVGL 9.
 * On device this is ~470 KB — keep it out of internal DRAM (PSRAM heap). */
#ifdef WP_DEVICE
static uint8_t * g_cbuf = nullptr;
#else
static uint8_t g_cbuf[LV_CANVAS_BUF_SIZE(kCanvasW, kCanvasH, 32, LV_DRAW_BUF_STRIDE_ALIGN)];
#endif
static bool g_buf_dirty = true; /* true → need clear/fill on next bind */

bool ensure_canvas_buf() {
#ifdef WP_DEVICE
  if (g_cbuf) return true;
  const size_t n = LV_CANVAS_BUF_SIZE(kCanvasW, kCanvasH, 32, LV_DRAW_BUF_STRIDE_ALIGN);
  g_cbuf = (uint8_t *)heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!g_cbuf) g_cbuf = (uint8_t *)malloc(n);
  if (g_cbuf) std::memset(g_cbuf, 0, n);
  return g_cbuf != nullptr;
#else
  return true;
#endif
}

const uint32_t kColors[] = {0x2a2438, 0xe07090, 0xe8b056, 0x5cb88a,
                            0x5a9fd4, 0x9a7ad4, 0xe87858, 0xf0f0f0};
constexpr uint32_t kBg = 0x120c18;

int quantize(lv_coord_t v, lv_coord_t max) {
  if (v < 0) v = 0;
  if (v > max) v = max;
  return (int)((v * kQ + max / 2) / max);
}

lv_coord_t dequant(int q, lv_coord_t max) { return (lv_coord_t)((q * max + kQ / 2) / kQ); }

int brush_px(uint8_t w) {
  /* Match web DOODLE_SIZES: 3 / 7 / 14 */
  return w == 1 ? 3 : (w == 3 ? 14 : 7);
}

void refresh_canvas() {
  if (!g_canvas) return;
  /* Pixel edits don't auto-drop the image cache — without this, strokes stay invisible
   * until the widget is recreated (e.g. picking another color). */
#if LV_CACHE_DEF_SIZE > 0
  const void * src = lv_image_get_src(g_canvas);
  if (src) lv_image_cache_drop(src);
#endif
  lv_obj_invalidate(g_canvas);
}

void paint_line(int x0, int y0, int x1, int y1, uint32_t color, int width) {
  if (!g_canvas) return;
  lv_layer_t layer;
  lv_canvas_init_layer(g_canvas, &layer);

  lv_draw_line_dsc_t dsc;
  lv_draw_line_dsc_init(&dsc);
  dsc.color = lv_color_hex(color);
  dsc.width = width < 1 ? 1 : width;
  dsc.round_start = 1;
  dsc.round_end = 1;
  dsc.opa = LV_OPA_COVER;
  dsc.p1.x = x0;
  dsc.p1.y = y0;
  dsc.p2.x = x1;
  dsc.p2.y = y1;
  lv_draw_line(&layer, &dsc);

  lv_canvas_finish_layer(g_canvas, &layer); /* drops cache + invalidates */
}

void apply_pts(const uint8_t * pts, uint8_t n, int8_t color, uint8_t w) {
  if (!g_canvas || n < 1) return;
  const uint32_t paint = color < 0 ? kBg : kColors[color % 8];
  const int px_w = brush_px(w);
  int prev_x = dequant(pts[0], kCanvasW - 1);
  int prev_y = dequant(pts[1], kCanvasH - 1);
  if (n == 1) {
    paint_line(prev_x, prev_y, prev_x, prev_y, paint, px_w);
    return;
  }
  for (uint8_t i = 1; i < n; ++i) {
    const int x = dequant(pts[i * 2], kCanvasW - 1);
    const int y = dequant(pts[i * 2 + 1], kCanvasH - 1);
    paint_line(prev_x, prev_y, x, y, paint, px_w);
    prev_x = x;
    prev_y = y;
  }
}

void clear_canvas() {
  if (!g_canvas) return;
  lv_canvas_fill_bg(g_canvas, lv_color_hex(kBg), LV_OPA_COVER);
  refresh_canvas();
}

void flush_stroke(bool last) {
  if (g_n_pts < 1) return;
  const int8_t col = g_erase ? (int8_t)-1 : (int8_t)g_color;
  app::Desk & d = app::desk();
  remember_pts(d.doodle_peer_id, g_pts, g_n_pts, col, (uint8_t)g_width);
  if (d.doodle_peer_id[0]) {
    proto::Msg m;
    m.type = proto::MsgType::DoodleStroke;
    std::snprintf(m.from_id, sizeof(m.from_id), "%s", d.id);
    std::snprintf(m.from_name, sizeof(m.from_name), "%s", d.name);
    std::snprintf(m.to_id, sizeof(m.to_id), "%s", d.doodle_peer_id);
    m.stroke_id = g_stroke_id;
    m.seq = 0;
    m.last = last;
    m.stroke_color = col;
    m.stroke_w = (uint8_t)g_width;
    m.n_pts = g_n_pts;
    std::memcpy(m.pts, g_pts, (size_t)g_n_pts * 2);
    app::send(m);
  }
  if (last) {
    g_stroke_id++;
    g_n_pts = 0;
  }
}

void local_xy(lv_obj_t * hit, int * lx, int * ly) {
  lv_point_t p;
  lv_indev_t * indev = lv_indev_active();
  if (!indev) {
    *lx = *ly = 0;
    return;
  }
  lv_indev_get_point(indev, &p);
  lv_area_t a;
  lv_obj_get_coords(hit, &a);
  *lx = p.x - a.x1;
  *ly = p.y - a.y1;
  if (*lx < 0) *lx = 0;
  if (*ly < 0) *ly = 0;
  if (*lx >= kCanvasW) *lx = kCanvasW - 1;
  if (*ly >= kCanvasH) *ly = kCanvasH - 1;
}

void on_canvas(lv_event_t * e) {
  const lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * hit = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
  int lx = 0, ly = 0;
  local_xy(hit, &lx, &ly);

  const uint32_t paint = g_erase ? kBg : kColors[g_color % 8];
  const int px_w = brush_px((uint8_t)g_width);

  if (code == LV_EVENT_PRESSED) {
    g_drawing = true;
    g_n_pts = 0;
    g_last_x = lx;
    g_last_y = ly;
    /* Full-res local preview (web begins path at pointer) */
    paint_line(lx, ly, lx, ly, paint, px_w);
    g_pts[0] = (uint8_t)quantize(lx, kCanvasW - 1);
    g_pts[1] = (uint8_t)quantize(ly, kCanvasH - 1);
    g_n_pts = 1;
  } else if (code == LV_EVENT_PRESSING && g_drawing) {
    if (g_last_x == lx && g_last_y == ly) return;
    /* Smooth segment at pointer resolution — not the 0..120 grid */
    paint_line(g_last_x, g_last_y, lx, ly, paint, px_w);
    g_last_x = lx;
    g_last_y = ly;

    if (g_n_pts >= proto::kMaxStrokePts) {
      flush_stroke(false);
      g_pts[0] = (uint8_t)quantize(lx, kCanvasW - 1);
      g_pts[1] = (uint8_t)quantize(ly, kCanvasH - 1);
      g_n_pts = 1;
      return;
    }
    const uint8_t qx = (uint8_t)quantize(lx, kCanvasW - 1);
    const uint8_t qy = (uint8_t)quantize(ly, kCanvasH - 1);
    if (g_n_pts > 0 && g_pts[(g_n_pts - 1) * 2] == qx && g_pts[(g_n_pts - 1) * 2 + 1] == qy) return;
    g_pts[g_n_pts * 2] = qx;
    g_pts[g_n_pts * 2 + 1] = qy;
    g_n_pts++;
  } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && g_drawing) {
    g_drawing = false;
    g_last_x = g_last_y = -1;
    flush_stroke(true);
    refresh_canvas(); /* ensure final stroke is visible on release */
  }
}

void rebuild_tools();

void on_clear(lv_event_t * /*e*/) {
  app::Desk & d = app::desk();
  PeerPad * pad = pad_for(d.doodle_peer_id, false);
  if (pad) pad->strokes.clear();
  g_n_pts = 0;
  g_drawing = false;
  clear_canvas();
  if (d.doodle_peer_id[0]) {
    proto::Msg m;
    m.type = proto::MsgType::DoodleClear;
    std::snprintf(m.from_id, sizeof(m.from_id), "%s", d.id);
    std::snprintf(m.from_name, sizeof(m.from_name), "%s", d.name);
    std::snprintf(m.to_id, sizeof(m.to_id), "%s", d.doodle_peer_id);
    app::send(m);
  }
  toast("Canvas cleared");
}

void on_deleted(lv_event_t * e) {
  /* load() deletes the previous screen async — that DELETE must not clear
   * pointers that already belong to the newly built doodle screen. */
  const uint32_t gen = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  if (gen != g_screen_gen) return;
  g_canvas = nullptr;
  g_tools = nullptr;
}

void rebuild_tools() {
  if (!g_tools) return;
  while (lv_obj_get_child_count(g_tools) > 0) lv_obj_delete(lv_obj_get_child(g_tools, 0));

  for (int i = 0; i < 8; ++i) {
    lv_obj_t * sw = lv_obj_create(g_tools);
    lv_obj_remove_style_all(sw);
    lv_obj_set_size(sw, 32, 32);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(kColors[i]), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    if (!g_erase && g_color == i) {
      lv_obj_set_style_border_width(sw, 3, 0);
      lv_obj_set_style_border_color(sw, theme::gold(), 0);
    }
    lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(sw, 10);
    lv_obj_add_event_cb(
        sw,
        [](lv_event_t * e) {
          g_color = (int)(intptr_t)lv_event_get_user_data(e);
          g_erase = false;
          rebuild_tools(); /* keep canvas — do not go_doodle() */
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  lv_obj_t * eras = lv_button_create(g_tools);
  lv_obj_set_size(eras, 40, 32);
  lv_obj_set_style_radius(eras, 10, 0);
  lv_obj_set_style_bg_color(eras, g_erase ? theme::gold() : theme::panel(), 0);
  lv_obj_set_style_shadow_width(eras, 0, 0);
  lv_obj_set_style_pad_all(eras, 2, 0);
  lv_obj_set_ext_click_area(eras, 10);

  /* Classic pink rubber eraser (no Unicode eraser glyph). */
  lv_obj_t * eras_ico = lv_obj_create(eras);
  lv_obj_remove_style_all(eras_ico);
  lv_obj_set_size(eras_ico, 26, 16);
  lv_obj_set_style_radius(eras_ico, 3, 0);
  lv_obj_set_style_bg_color(eras_ico, lv_color_hex(0xf48fb1), 0);
  lv_obj_set_style_bg_opa(eras_ico, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(eras_ico, 1, 0);
  lv_obj_set_style_border_color(eras_ico, lv_color_hex(0xd46a8c), 0);
  lv_obj_remove_flag(eras_ico, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(eras_ico, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(eras_ico);

  lv_obj_t * band = lv_obj_create(eras_ico);
  lv_obj_remove_style_all(band);
  lv_obj_set_size(band, 7, 16);
  lv_obj_set_style_bg_color(band, lv_color_hex(0xb8bcc8), 0);
  lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(band, 0, 0);
  lv_obj_align(band, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_remove_flag(band, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(band, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * tip = lv_obj_create(eras_ico);
  lv_obj_remove_style_all(tip);
  lv_obj_set_size(tip, 5, 16);
  lv_obj_set_style_bg_color(tip, lv_color_hex(0xffc1d5), 0);
  lv_obj_set_style_bg_opa(tip, LV_OPA_COVER, 0);
  lv_obj_align(tip, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_remove_flag(tip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(tip, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_add_event_cb(
      eras,
      [](lv_event_t * /*e*/) {
        g_erase = true;
        rebuild_tools();
      },
      LV_EVENT_CLICKED, nullptr);

  for (int w = 1; w <= 3; ++w) {
    const char * lab = w == 1 ? "S" : (w == 2 ? "M" : "L");
    lv_obj_t * b = lv_button_create(g_tools);
    lv_obj_set_size(b, 32, 32);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, g_width == w ? theme::gold() : theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_ext_click_area(b, 10);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, lab);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, g_width == w ? lv_color_hex(0x1a1200) : theme::ink(), 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(
        b,
        [](lv_event_t * e) {
          g_width = (int)(intptr_t)lv_event_get_user_data(e);
          rebuild_tools();
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)w);
  }
}

}  // namespace

lv_obj_t * doodle_screen() {
  app::Desk & d = app::desk();
  d.doodle_unread = false;
  const uint32_t gen = ++g_screen_gen;
  lv_obj_t * scr = make_screen();
  lv_obj_t * top = make_topbar(scr, "DOODLE", d.name, d.doodle_peer_id[0] ? nullptr : "Draw together");
  lv_obj_t * body = make_body(scr, true);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * dock = make_dock(scr);
  lv_obj_add_event_cb(scr, on_deleted, LV_EVENT_DELETE, (void *)(uintptr_t)gen);

  lv_obj_t * pick = lv_obj_create(body);
  lv_obj_remove_style_all(pick);
  lv_obj_set_width(pick, lv_pct(100));
  lv_obj_set_flex_grow(pick, 1);
  lv_obj_set_flex_flow(pick, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(pick, 8, 0);
  if (d.peer_count == 0) make_tagline(pick, "No saved desks - add one in Settings.");
  for (int i = 0; i < d.peer_count; ++i) {
    make_peer_btn(
        pick, d.peers[i].name, nullptr,
        [](lv_event_t * e) {
          const int idx = (int)(intptr_t)lv_event_get_user_data(e);
          app::Desk & desk = app::desk();
          if (idx < 0 || idx >= desk.peer_count) return;
          std::snprintf(desk.doodle_peer_id, sizeof(desk.doodle_peer_id), "%s", desk.peers[idx].id);
          std::snprintf(desk.doodle_peer_name, sizeof(desk.doodle_peer_name), "%s",
                        desk.peers[idx].name);
          go_doodle();
        },
        (void *)(intptr_t)i);
  }

  lv_obj_t * draw = lv_obj_create(body);
  lv_obj_remove_style_all(draw);
  lv_obj_set_width(draw, lv_pct(100));
  lv_obj_set_flex_grow(draw, 1);
  lv_obj_set_flex_flow(draw, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(draw, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(draw, 12, 0);
  lv_obj_set_style_pad_top(draw, 4, 0);
  lv_obj_add_flag(draw, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(draw, LV_OBJ_FLAG_SCROLLABLE);

  g_tools = lv_obj_create(draw);
  lv_obj_remove_style_all(g_tools);
  lv_obj_set_width(g_tools, lv_pct(100));
  lv_obj_set_height(g_tools, 40);
  lv_obj_set_flex_flow(g_tools, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(g_tools, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(g_tools, 4, 0);
  lv_obj_remove_flag(g_tools, LV_OBJ_FLAG_SCROLLABLE);
  rebuild_tools();

  g_canvas = lv_canvas_create(draw);
  lv_obj_set_size(g_canvas, kCanvasW, kCanvasH);
  lv_obj_set_style_margin_top(g_canvas, 6, 0);
  lv_obj_set_style_radius(g_canvas, 12, 0);
  lv_obj_set_style_clip_corner(g_canvas, true, 0);
  if (!ensure_canvas_buf()) {
    lv_obj_t * err = lv_label_create(draw);
    lv_label_set_text(err, "Canvas OOM");
    lv_obj_set_style_text_color(err, lv_color_hex(0xe07070), 0);
  } else {
    lv_canvas_set_buffer(g_canvas, g_cbuf, kCanvasW, kCanvasH, LV_COLOR_FORMAT_ARGB8888);
  }
  lv_obj_add_flag(g_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(g_canvas, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_canvas, on_canvas, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(g_canvas, on_canvas, LV_EVENT_PRESSING, nullptr);
  lv_obj_add_event_cb(g_canvas, on_canvas, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(g_canvas, on_canvas, LV_EVENT_PRESS_LOST, nullptr);

  if (d.doodle_peer_id[0] && std::strcmp(g_bound_peer, d.doodle_peer_id) != 0) {
    g_buf_dirty = true;
  }
  if (d.doodle_peer_id[0]) {
    std::snprintf(g_bound_peer, sizeof(g_bound_peer), "%s", d.doodle_peer_id);
  }

  if (g_buf_dirty) {
    clear_canvas();
    g_buf_dirty = false;
    PeerPad * pad = pad_for(d.doodle_peer_id, false);
    if (pad) {
      for (const auto & s : pad->strokes) apply_pts(s.pts, s.n, s.color, s.w);
    }
  } else {
    /* Re-bind existing pixels — do not re-quantize/redraw (keeps smooth local ink). */
    refresh_canvas();
  }

  if (d.doodle_peer_id[0]) {
    lv_obj_add_flag(pick, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(draw, LV_OBJ_FLAG_HIDDEN);
    char sub[40];
    lv_snprintf(sub, sizeof(sub), "vs %s", d.doodle_peer_name);
    topbar_set(top, "DOODLE", sub);
    dock_btn(dock, "Clear", false, false, on_clear);
    dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) {
      /* Peer pick only — keep ink until someone Clears. */
      app::desk().doodle_peer_id[0] = 0;
      app::desk().doodle_peer_name[0] = 0;
      go_doodle();
    });
    dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) {
      /* Leave doodle; peer + canvas persist so you can resume. */
      go_hub();
    });
  } else {
    dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
  }
  return scr;
}

void doodle_apply_remote_stroke(const proto::Msg & msg) {
  remember_pts(msg.from_id, msg.pts, msg.n_pts, msg.stroke_color, msg.stroke_w);
  const char * cur = app::desk().doodle_peer_id;
  if (cur[0] && std::strcmp(cur, msg.from_id) == 0) {
    apply_pts(msg.pts, msg.n_pts, msg.stroke_color, msg.stroke_w);
  } else if (std::strcmp(g_bound_peer, msg.from_id) == 0) {
    g_buf_dirty = true;
  }
}

void doodle_remote_clear(const char * from_id) {
  PeerPad * pad = pad_for(from_id, false);
  if (pad) pad->strokes.clear();
  const char * cur = app::desk().doodle_peer_id;
  if (from_id && cur[0] && std::strcmp(cur, from_id) == 0) {
    g_n_pts = 0;
    g_drawing = false;
    if (g_canvas) clear_canvas();
    else g_buf_dirty = true;
  }
}

void doodle_debug_show_draw() {
  app::Desk & d = app::desk();
  std::snprintf(d.doodle_peer_id, sizeof(d.doodle_peer_id), "mac-will");
  std::snprintf(d.doodle_peer_name, sizeof(d.doodle_peer_name), "Will");
  g_buf_dirty = true;
  go_doodle();
}

}  // namespace ui
}  // namespace wp
