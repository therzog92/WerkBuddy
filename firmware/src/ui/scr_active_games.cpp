#include "ui/scr_active_games.h"

#include "app/active_games.h"
#include "app/app.h"
#include "ui/chrome.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/nav.h"
#include "ui/scr_games.h"
#include "ui/theme.h"

#include <cstring>

namespace wp {
namespace ui {
namespace {

void section_header(lv_obj_t * body, const char * title) {
  lv_obj_t * h = lv_label_create(body);
  lv_label_set_text(h, title);
  lv_obj_set_style_text_color(h, theme::gold(), 0);
  lv_obj_set_style_text_font(h, font_display(20), 0);
  lv_obj_set_width(h, lv_pct(100));
  lv_obj_set_style_pad_top(h, 8, 0);
}

void style_game_row(lv_obj_t * btn) {
  lv_obj_set_style_radius(btn, 14, 0);
  lv_obj_set_style_bg_color(btn, theme::panel(), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, theme::border(), 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_ver(btn, 10, 0);
  lv_obj_set_style_pad_hor(btn, 12, 0);
}

AppIcon icon_for_kind(app::GameKind kind) {
  switch (kind) {
    case app::GameKind::Ttt: return AppIcon::Ttt;
    case app::GameKind::Sttt: return AppIcon::Sttt;
    case app::GameKind::C4: return AppIcon::C4;
    case app::GameKind::Bs: return AppIcon::Battleship;
    case app::GameKind::Ck: return AppIcon::Checkers;
    case app::GameKind::Mem: return AppIcon::Memory;
    case app::GameKind::Rv: return AppIcon::Reversi;
    case app::GameKind::Db: return AppIcon::Dots;
    case app::GameKind::Wordle: return AppIcon::Wordle;
    default: return AppIcon::Games;
  }
}

/** Small game glyph for the far right of a row (non-interactive). */
void add_kind_glyph(lv_obj_t * parent, app::GameKind kind) {
  constexpr int kVis = 36;
  lv_obj_t * wrap = lv_obj_create(parent);
  lv_obj_remove_style_all(wrap);
  lv_obj_set_size(wrap, kVis, kVis);
  lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(wrap, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_remove_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(wrap, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * glyph = make_app_glyph(wrap, icon_for_kind(kind));
  const int32_t scale = (kVis * 256) / 72;
  lv_obj_set_style_transform_pivot_x(glyph, 36, 0);
  lv_obj_set_style_transform_pivot_y(glyph, 36, 0);
  lv_obj_set_style_transform_scale(glyph, scale, 0);
  lv_obj_set_style_shadow_width(glyph, 0, 0); /* keep row quiet */
  lv_obj_center(glyph);
  lv_obj_remove_flag(glyph, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t * pill_btn(lv_obj_t * row, const char * label, lv_color_t bg, lv_color_t fg,
                    lv_event_cb_t cb, void * ud) {
  lv_obj_t * b = lv_button_create(row);
  lv_obj_set_size(b, 78, 40);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t * l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_color(l, fg, 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

void on_open(lv_event_t * e) {
  app::open_slot((int)(intptr_t)lv_event_get_user_data(e));
}

void on_cancel(lv_event_t * e) {
  app::cancel_slot((int)(intptr_t)lv_event_get_user_data(e));
}

void on_accept(lv_event_t * e) {
  accept_incoming_slot((int)(intptr_t)lv_event_get_user_data(e));
}

void add_pending_row(lv_obj_t * body, int idx, const app::GameSlot & s) {
  const bool incoming = s.invite_pending;

  lv_obj_t * row = lv_obj_create(body);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  add_kind_glyph(row, s.kind);

  lv_obj_t * main = lv_button_create(row);
  lv_obj_remove_style_all(main);
  lv_obj_set_flex_grow(main, 1);
  lv_obj_set_height(main, LV_SIZE_CONTENT);
  style_game_row(main);
  lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(main, 2, 0);

  lv_obj_t * title = lv_label_create(main);
  char line[64];
  lv_snprintf(line, sizeof(line), "%s vs %s", app::kind_name(s.kind), app::slot_peer_name(s));
  lv_label_set_text(title, line);
  lv_obj_set_style_text_color(title, theme::ink(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

  lv_obj_t * st = lv_label_create(main);
  lv_label_set_text(st, incoming ? "Wants to play" : "Waiting on them");
  lv_obj_set_style_text_color(st, theme::muted(), 0);
  lv_obj_set_style_text_font(st, &lv_font_montserrat_12, 0);

  lv_obj_add_event_cb(main, on_open, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

  if (incoming) {
    pill_btn(row, "Decline", theme::danger(), lv_color_hex(0xffffff), on_cancel,
             (void *)(intptr_t)idx);
    pill_btn(row, "Accept", theme::gold(), lv_color_hex(0x1a1200), on_accept,
             (void *)(intptr_t)idx);
  } else {
    pill_btn(row, "Cancel", theme::danger(), lv_color_hex(0xffffff), on_cancel,
             (void *)(intptr_t)idx);
  }
}

void add_live_row(lv_obj_t * body, int idx, const app::GameSlot & s) {
  lv_obj_t * row = lv_obj_create(body);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  add_kind_glyph(row, s.kind);

  lv_obj_t * main = lv_button_create(row);
  lv_obj_remove_style_all(main);
  lv_obj_set_flex_grow(main, 1);
  lv_obj_set_height(main, LV_SIZE_CONTENT);
  style_game_row(main);
  lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(main, 2, 0);

  lv_obj_t * title = lv_label_create(main);
  char line[64];
  lv_snprintf(line, sizeof(line), "%s vs %s", app::kind_name(s.kind), app::slot_peer_name(s));
  lv_label_set_text(title, line);
  lv_obj_set_style_text_color(title, theme::ink(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

  const char * status = "Their turn";
  if (app::is_my_turn(s)) status = "Your turn";
  else if (s.kind == app::GameKind::Bs && s.g.bs.setup) status = "Placing ships";

  lv_obj_t * st = lv_label_create(main);
  lv_label_set_text(st, status);
  lv_obj_set_style_text_color(st, theme::muted(), 0);
  lv_obj_set_style_text_font(st, &lv_font_montserrat_12, 0);

  lv_obj_add_event_cb(main, on_open, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
}

}  // namespace

lv_obj_t * active_games_screen() {
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "ACTIVE GAMES", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  lv_obj_set_style_pad_row(body, 8, 0);
  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_hub(); });

  /* Re-check peers after unplug/forfeit desync — stale rows drop when they reply gone. */
  app::games_probe_peers();

  int idxs[app::kMaxActiveGames];
  const int n = app::list_sorted(idxs, app::kMaxActiveGames);
  if (n == 0) {
    make_tagline(body, "No active games.");
    return scr;
  }

  int pending[app::kMaxActiveGames];
  int your_turn[app::kMaxActiveGames];
  int active[app::kMaxActiveGames];
  int n_pend = 0, n_turn = 0, n_active = 0;

  /* Incoming invites first, then outgoing waits. */
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < n; ++i) {
      const app::GameSlot * s = app::slot_at(idxs[i]);
      if (!s || !app::slot_is_live(*s)) continue;
      const bool is_pend = s->invite_pending || app::is_outgoing_wait(*s);
      if (!is_pend) continue;
      if (pass == 0 && s->invite_pending) pending[n_pend++] = idxs[i];
      if (pass == 1 && !s->invite_pending) pending[n_pend++] = idxs[i];
    }
  }
  for (int i = 0; i < n; ++i) {
    const app::GameSlot * s = app::slot_at(idxs[i]);
    if (!s || !app::slot_is_live(*s)) continue;
    if (s->invite_pending || app::is_outgoing_wait(*s)) continue;
    if (app::is_my_turn(*s)) your_turn[n_turn++] = idxs[i];
    else active[n_active++] = idxs[i];
  }

  if (n_pend > 0) {
    section_header(body, "Pending Invites:");
    for (int i = 0; i < n_pend; ++i) {
      const app::GameSlot * s = app::slot_at(pending[i]);
      if (s) add_pending_row(body, pending[i], *s);
    }
  }

  if (n_turn > 0) {
    section_header(body, "Your Turn:");
    for (int i = 0; i < n_turn; ++i) {
      const app::GameSlot * s = app::slot_at(your_turn[i]);
      if (s) add_live_row(body, your_turn[i], *s);
    }
  }

  if (n_active > 0) {
    section_header(body, "Their Turn:");
    for (int i = 0; i < n_active; ++i) {
      const app::GameSlot * s = app::slot_at(active[i]);
      if (s) add_live_row(body, active[i], *s);
    }
  }

  return scr;
}

}  // namespace ui
}  // namespace wp
