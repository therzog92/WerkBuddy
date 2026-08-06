#include "ui/scr_page_history.h"

#include "app/app.h"
#include "app/page_log.h"
#include "ui/chrome.h"
#include "ui/emoji_badge.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cstdio>
#include <ctime>

namespace wp {
namespace ui {
namespace {

void format_stamp(char * buf, size_t n, int64_t stamp) {
  /* stamp is YYYYMMDDhhmmss */
  if (stamp < 10000000000LL) {
    lv_snprintf(buf, (uint32_t)n, "—");
    return;
  }
  const int sec = (int)(stamp % 100);
  stamp /= 100;
  const int min = (int)(stamp % 100);
  stamp /= 100;
  const int hour = (int)(stamp % 100);
  int hour12 = hour % 12;
  if (hour12 == 0) hour12 = 12;
  const char * ampm = hour >= 12 ? "PM" : "AM";
  (void)sec;
  lv_snprintf(buf, (uint32_t)n, "%d:%02d %s", hour12, min, ampm);
}

}  // namespace

lv_obj_t * page_history_screen() {
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "HISTORY", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  lv_obj_set_style_pad_row(body, 8, 0);

  const int n = page_log::count();
  if (n == 0) {
    make_tagline(body, "No pages yet.");
  } else {
    make_tagline(body, "Recent pages");
    for (int i = 0; i < n; ++i) {
      const page_log::Entry * e = page_log::at(i);
      if (!e) continue;

      lv_obj_t * row = lv_obj_create(body);
      lv_obj_remove_style_all(row);
      lv_obj_set_width(row, lv_pct(100));
      lv_obj_set_height(row, LV_SIZE_CONTENT);
      lv_obj_set_style_bg_color(row, theme::panel(), 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(row, 12, 0);
      lv_obj_set_style_pad_all(row, 10, 0);
      lv_obj_set_style_pad_row(row, 4, 0);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
      lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t * top = lv_obj_create(row);
      lv_obj_remove_style_all(top);
      lv_obj_set_width(top, lv_pct(100));
      lv_obj_set_height(top, LV_SIZE_CONTENT);
      lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(top, 8, 0);

      lv_obj_t * dir = lv_label_create(top);
      lv_label_set_text(dir, e->dir == page_log::Dir::Out ? "Sent" : "Got");
      lv_obj_set_style_text_color(dir, e->dir == page_log::Dir::Out ? theme::mint() : theme::gold(),
                                  0);
      lv_obj_set_style_text_font(dir, &lv_font_montserrat_12, 0);

      lv_obj_t * name = lv_label_create(top);
      lv_label_set_text(name, e->peer_name[0] ? e->peer_name : "—");
      lv_obj_set_style_text_color(name, theme::ink(), 0);
      lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
      lv_obj_set_flex_grow(name, 1);

      char tb[24];
      format_stamp(tb, sizeof(tb), e->epoch_ms);
      lv_obj_t * when = lv_label_create(top);
      lv_label_set_text(when, tb);
      lv_obj_set_style_text_color(when, theme::muted(), 0);
      lv_obj_set_style_text_font(when, &lv_font_montserrat_12, 0);

      lv_obj_t * msg_row = lv_obj_create(row);
      lv_obj_remove_style_all(msg_row);
      lv_obj_set_width(msg_row, lv_pct(100));
      lv_obj_set_height(msg_row, LV_SIZE_CONTENT);
      lv_obj_set_flex_flow(msg_row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(msg_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(msg_row, 8, 0);

      if (e->emoji[0]) make_emoji_image(msg_row, e->emoji, 22);

      lv_obj_t * msg = lv_label_create(msg_row);
      lv_label_set_text(msg, e->message[0] ? e->message : "(no message)");
      lv_obj_set_style_text_color(msg, theme::muted(), 0);
      lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
      lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
      lv_obj_set_flex_grow(msg, 1);
    }
  }

  lv_obj_t * dock = make_dock(scr);
  /* Back on the left — Clear used to occupy the left half and eat Home/Back taps. */
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_werk(); });
  dock_btn(dock, "Clear", false, true, [](lv_event_t * /*e*/) {
    page_log::clear();
    go_page_history();
  });
  return scr;
}

}  // namespace ui
}  // namespace wp
