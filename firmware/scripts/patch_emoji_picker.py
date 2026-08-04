# -*- coding: utf-8 -*-
from pathlib import Path
import re

p = Path(r"c:/Users/Tommy/Projects/WerkPager/firmware/src/ui/scr_settings.cpp")
t = p.read_text(encoding="utf-8")
t2, n = re.subn(
    r"const char \* kPalette\[\] = \{.*?\};\nconstexpr int kPaletteCount = \(int\)\(sizeof\(kPalette\) / sizeof\(kPalette\[0\]\)\);\n\n",
    "",
    t,
    count=1,
    flags=re.S,
)
print("removed palette", n)
if n != 1:
    raise SystemExit(1)

picker = r'''
int g_emoji_picker_slot = 0; /* settings 0..n-1, or kEmojiPickerCompose */
int g_emoji_picker_cat = 0;
lv_obj_t * g_emoji_grid = nullptr;
lv_obj_t * g_emoji_cat_btns[kEmojiCategoryCount] = {};

void emoji_picker_fill_grid() {
  if (!g_emoji_grid) return;
  lv_obj_clean(g_emoji_grid);
  if (g_emoji_picker_cat < 0 || g_emoji_picker_cat >= kEmojiCategoryCount) g_emoji_picker_cat = 0;
  const EmojiCategory & cat = kEmojiCategories[g_emoji_picker_cat];
  for (int i = 0; i < cat.count; ++i) {
    lv_obj_t * b = lv_button_create(g_emoji_grid);
    lv_obj_set_size(b, 58, 48);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 2, 0);
    lv_obj_t * img = make_emoji_image(b, cat.emojis[i], 34);
    lv_obj_center(img);
    /* pack category in high bits, index in low */
    const intptr_t ud = ((intptr_t)g_emoji_picker_cat << 16) | i;
    lv_obj_add_event_cb(
        b,
        [](lv_event_t * e) {
          const intptr_t ud = (intptr_t)lv_event_get_user_data(e);
          const int ci = (int)(ud >> 16);
          const int ei = (int)(ud & 0xffff);
          if (ci < 0 || ci >= kEmojiCategoryCount) return;
          const EmojiCategory & cat = kEmojiCategories[ci];
          if (ei < 0 || ei >= cat.count) return;
          const char * emo = cat.emojis[ei];
          if (g_emoji_picker_slot == kEmojiPickerCompose) {
            compose_set_emoji(emo);
            go_compose_refresh();
            return;
          }
          if (g_emoji_picker_slot >= 0 && g_emoji_picker_slot < app::kEmojiSlots) {
            std::snprintf(app::desk().emojis[g_emoji_picker_slot],
                          sizeof(app::desk().emojis[0]), "%s", emo);
            app::save();
          }
          go_settings();
        },
        LV_EVENT_CLICKED, (void *)ud);
  }
  lv_obj_scroll_to_y(g_emoji_grid, 0, LV_ANIM_OFF);
}

void emoji_picker_style_cats() {
  for (int i = 0; i < kEmojiCategoryCount; ++i) {
    lv_obj_t * b = g_emoji_cat_btns[i];
    if (!b) continue;
    const bool on = i == g_emoji_picker_cat;
    lv_obj_set_style_border_width(b, on ? 2 : 1, 0);
    lv_obj_set_style_border_color(b, on ? theme::gold() : theme::border(), 0);
    lv_obj_set_style_bg_color(b, on ? lv_color_mix(theme::gold(), theme::panel(), 50) : theme::panel(),
                              0);
  }
}

void on_emoji_cat(lv_event_t * e) {
  const int ci = (int)(intptr_t)lv_event_get_user_data(e);
  if (ci < 0 || ci >= kEmojiCategoryCount) return;
  if (ci == g_emoji_picker_cat) return;
  g_emoji_picker_cat = ci;
  emoji_picker_style_cats();
  emoji_picker_fill_grid();
}

lv_obj_t * emoji_picker_screen(int slot) {
  const bool compose_mode = slot == kEmojiPickerCompose;
  if (!compose_mode && (slot < 0 || slot >= app::kEmojiSlots)) slot = 0;
  g_emoji_picker_slot = compose_mode ? kEmojiPickerCompose : slot;
  g_emoji_picker_cat = 0;
  g_emoji_grid = nullptr;
  for (int i = 0; i < kEmojiCategoryCount; ++i) g_emoji_cat_btns[i] = nullptr;

  lv_obj_t * scr = make_screen();
  make_topbar(scr, compose_mode ? "WERK ROOM" : "SETTINGS", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  make_tagline(body, compose_mode ? "Pick an emoji for this ping" : "Pick an emoji");

  /* iPhone-style category strip */
  lv_obj_t * cats = lv_obj_create(body);
  lv_obj_remove_style_all(cats);
  lv_obj_set_width(cats, lv_pct(100));
  lv_obj_set_height(cats, 48);
  lv_obj_set_flex_flow(cats, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cats, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(cats, 4, 0);
  lv_obj_add_flag(cats, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(cats, LV_DIR_HOR);
  lv_obj_remove_flag(cats, LV_OBJ_FLAG_SCROLL_ELASTIC);

  for (int i = 0; i < kEmojiCategoryCount; ++i) {
    lv_obj_t * b = lv_button_create(cats);
    lv_obj_set_size(b, 44, 44);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, theme::panel(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 2, 0);
    lv_obj_set_style_border_opa(b, LV_OPA_COVER, 0);
    lv_obj_t * img = make_emoji_image(b, kEmojiCategories[i].icon, 28);
    lv_obj_center(img);
    lv_obj_add_event_cb(b, on_emoji_cat, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    g_emoji_cat_btns[i] = b;
  }
  emoji_picker_style_cats();

  g_emoji_grid = lv_obj_create(body);
  lv_obj_remove_style_all(g_emoji_grid);
  lv_obj_set_width(g_emoji_grid, lv_pct(100));
  lv_obj_set_flex_grow(g_emoji_grid, 1);
  lv_obj_set_flex_flow(g_emoji_grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(g_emoji_grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(g_emoji_grid, 6, 0);
  lv_obj_set_style_pad_column(g_emoji_grid, 6, 0);
  lv_obj_add_flag(g_emoji_grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g_emoji_grid, LV_DIR_VER);
  emoji_picker_fill_grid();

  lv_obj_t * dock = make_dock(scr);
  if (compose_mode) {
    dock_btn(dock, "Cancel", false, false, [](lv_event_t * /*e*/) { go_compose_refresh(); });
  } else {
    dock_btn(dock, "Cancel", false, false, [](lv_event_t * /*e*/) { go_settings(); });
  }
  return scr;
}

'''

t2, n2 = re.subn(
    r"int g_emoji_picker_slot = 0;.*?lv_obj_t \* emoji_picker_screen\(int slot\) \{.*?\n\}\n\n\}  // namespace ui",
    picker.strip() + "\n\n}  // namespace ui",
    t2,
    count=1,
    flags=re.S,
)
print("replaced picker", n2)
if n2 != 1:
    raise SystemExit(2)

p.write_text(t2, encoding="utf-8", newline="\n")
print("wrote", p)
