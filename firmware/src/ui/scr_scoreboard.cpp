#include "ui/scr_scoreboard.h"

#include "app/app.h"
#include "app/score_log.h"
#include "ui/chrome.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cstdio>

namespace wp {
namespace ui {
namespace {

/** stamp YYYYMMDDhhmmss → "08/05/2026" */
void format_day(char * buf, size_t n, int64_t stamp) {
  if (stamp < 10000000000LL) {
    lv_snprintf(buf, (uint32_t)n, "—");
    return;
  }
  const int year = (int)(stamp / 10000000000LL);
  const int month = (int)((stamp / 100000000LL) % 100);
  const int day = (int)((stamp / 1000000LL) % 100);
  lv_snprintf(buf, (uint32_t)n, "%02d/%02d/%04d", month, day, year);
}

int64_t day_key(int64_t stamp) { return stamp / 1000000LL; /* YYYYMMDD */ }

/** Fixed status colors — not theme tokens (mint/hot shift per palette). */
lv_color_t outcome_color(score_log::Outcome o) {
  switch (o) {
    case score_log::Outcome::Win: return lv_color_hex(0x4ade80);          /* green */
    case score_log::Outcome::Lose: return lv_color_hex(0xff5c7a);          /* red */
    case score_log::Outcome::Tie: return lv_color_hex(0xf0c24b);           /* yellow */
    case score_log::Outcome::ForfeitSelf: return lv_color_hex(0xff5c7a);   /* red */
    case score_log::Outcome::ForfeitOpp: return lv_color_hex(0x4ade80);    /* green */
    default: return theme::muted();
  }
}

}  // namespace

lv_obj_t * scoreboard_screen() {
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "SCOREBOARD", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  lv_obj_set_style_pad_row(body, 6, 0);

  const int n = score_log::count();
  if (n == 0) {
    make_tagline(body, "No games recorded yet.");
  } else {
    int64_t last_day = -1;
    for (int i = 0; i < n; ++i) {
      const score_log::Entry * e = score_log::at(i);
      if (!e) continue;

      const int64_t dk = day_key(e->stamp);
      if (dk != last_day) {
        last_day = dk;
        char day_buf[24];
        format_day(day_buf, sizeof(day_buf), e->stamp);
        lv_obj_t * hdr = lv_label_create(body);
        lv_label_set_text(hdr, day_buf);
        lv_obj_set_style_text_color(hdr, theme::gold(), 0);
        lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
        lv_obj_set_style_pad_top(hdr, i == 0 ? 0 : 10, 0);
        lv_obj_set_width(hdr, lv_pct(100));
      }

      lv_obj_t * row = lv_obj_create(body);
      lv_obj_remove_style_all(row);
      lv_obj_set_width(row, lv_pct(100));
      lv_obj_set_height(row, LV_SIZE_CONTENT);
      lv_obj_set_style_bg_color(row, theme::panel(), 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(row, 12, 0);
      lv_obj_set_style_pad_all(row, 10, 0);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(row, 8, 0);
      lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t * line = lv_label_create(row);
      char buf[96];
      lv_snprintf(buf, sizeof(buf), "vs %s (%s)", e->peer_name[0] ? e->peer_name : "—",
                  e->game[0] ? e->game : "Game");
      lv_label_set_text(line, buf);
      lv_obj_set_style_text_color(line, theme::ink(), 0);
      lv_obj_set_style_text_font(line, &lv_font_montserrat_14, 0);
      lv_label_set_long_mode(line, LV_LABEL_LONG_DOT);
      lv_obj_set_flex_grow(line, 1);

      lv_obj_t * outcome = lv_label_create(row);
      lv_label_set_text(outcome, score_log::outcome_label(e->outcome));
      lv_obj_set_style_text_color(outcome, outcome_color(e->outcome), 0);
      lv_obj_set_style_text_font(outcome, &lv_font_montserrat_14, 0);
    }
  }

  lv_obj_t * dock = make_dock(scr);
  /* Back left — Clear used to own the left half and eat Back taps / open confirm. */
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_games_folder(); });
  dock_btn(dock, "Clear", false, true, [](lv_event_t * /*e*/) {
    show_confirm("Are you sure you want to clear?", "Yes", true, [](lv_event_t * /*ev*/) {
      score_log::clear();
      go_scoreboard();
    }, nullptr, "No");
  });
  return scr;
}

}  // namespace ui
}  // namespace wp
