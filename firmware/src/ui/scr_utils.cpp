#include "ui/scr_utils.h"

#include "app/app.h"
#include "app/checklist.h"
#include "ui/chrome.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/nav.h"
#include "ui/theme.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace wp {
namespace ui {
namespace {

/* —— Calculator: expression buffer + recursive-descent eval —— */
constexpr int kCalcExprMax = 48;
char g_calc_expr[kCalcExprMax] = "";
bool g_calc_just_eq = false;
bool g_calc_error = false;
lv_obj_t * g_calc_lbl = nullptr;

void calc_pretty_expr(const char * src, char * dst, size_t n);

void calc_show() {
  if (!g_calc_lbl) return;
  if (g_calc_error) {
    lv_label_set_text(g_calc_lbl, "Error");
    return;
  }
  if (!g_calc_expr[0]) {
    lv_label_set_text(g_calc_lbl, "0");
    return;
  }
  char pretty[96];
  calc_pretty_expr(g_calc_expr, pretty, sizeof(pretty));
  lv_label_set_text(g_calc_lbl, pretty);
}

char calc_last() {
  const size_t n = std::strlen(g_calc_expr);
  return n ? g_calc_expr[n - 1] : 0;
}

bool calc_is_op(char c) { return c == '+' || c == '-' || c == '*' || c == '/' || c == 'x'; }

int calc_open_parens() {
  int n = 0;
  for (const char * p = g_calc_expr; *p; ++p) {
    if (*p == '(') ++n;
    else if (*p == ')') --n;
  }
  return n;
}

/** Insert thousand commas into a digit run (no sign/decimal). */
void calc_commas_digits(const char * digits, size_t len, char * out, size_t n) {
  if (!n) return;
  if (!len) {
    out[0] = '\0';
    return;
  }
  const size_t commas = (len - 1) / 3;
  if (len + commas >= n) {
    /* Truncate rather than overflow. */
    std::snprintf(out, n, "%.*s", (int)(n > 1 ? n - 1 : 0), digits);
    return;
  }
  size_t oi = 0;
  for (size_t i = 0; i < len; ++i) {
    if (i > 0 && (len - i) % 3 == 0) out[oi++] = ',';
    out[oi++] = digits[i];
  }
  out[oi] = '\0';
}

/** Pretty number: optional leading '-', integer with commas, optional fraction. */
void calc_pretty_number(const char * num, size_t len, char * out, size_t n) {
  if (!n || !len) {
    if (n) out[0] = '\0';
    return;
  }
  size_t i = 0;
  size_t o = 0;
  auto put = [&](char c) {
    if (o + 1 < n) out[o++] = c;
  };
  if (num[0] == '-') {
    put('-');
    ++i;
  }
  size_t int_start = i;
  while (i < len && std::isdigit((unsigned char)num[i])) ++i;
  const size_t int_len = i - int_start;
  char int_buf[40];
  calc_commas_digits(num + int_start, int_len, int_buf, sizeof(int_buf));
  for (const char * p = int_buf; *p; ++p) put(*p);
  while (i < len) put(num[i++]);
  out[o < n ? o : n - 1] = '\0';
}

/** Display form: commas in numbers, spaces around binary ops (* shown as x). */
void calc_pretty_expr(const char * src, char * dst, size_t n) {
  if (!n) return;
  size_t o = 0;
  auto put = [&](char c) {
    if (o + 1 < n) dst[o++] = c;
  };
  auto puts = [&](const char * s) {
    while (*s) put(*s++);
  };
  bool prev_was_value = false; /* digit/number or ')' */
  for (size_t i = 0; src[i];) {
    const char c = src[i];
    const bool unary_minus =
        c == '-' && !prev_was_value &&
        (i == 0 || src[i - 1] == '(' || calc_is_op(src[i - 1]));
    if (std::isdigit((unsigned char)c) || unary_minus) {
      size_t j = i;
      if (src[j] == '-') ++j;
      const size_t num_start = i;
      while (src[j] && std::isdigit((unsigned char)src[j])) ++j;
      if (src[j] == '.') {
        ++j;
        while (src[j] && std::isdigit((unsigned char)src[j])) ++j;
      }
      char num_pretty[48];
      calc_pretty_number(src + num_start, j - num_start, num_pretty, sizeof(num_pretty));
      puts(num_pretty);
      i = j;
      prev_was_value = true;
      continue;
    }
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == 'x') {
      put(' ');
      put(c == '*' ? 'x' : c);
      put(' ');
      ++i;
      prev_was_value = false;
      continue;
    }
    put(c);
    prev_was_value = (c == ')');
    ++i;
  }
  dst[o < n ? o : n - 1] = '\0';
}

void calc_format_result(double v, char * out, size_t n) {
  if (!std::isfinite(v)) {
    std::snprintf(out, n, "Error");
    return;
  }
  if (std::fabs(v) < 1e-12) v = 0;
  /* Store plain digits (no commas) so further ops parse cleanly; display adds commas. */
  if (std::fabs(v - std::round(v)) < 1e-9 && std::fabs(v) < 1e15)
    std::snprintf(out, n, "%.0f", std::round(v));
  else
    std::snprintf(out, n, "%.8g", v);
}

/* Parser over g_calc_expr[g_calc_pos…] */
size_t g_calc_pos = 0;
bool g_calc_parse_ok = true;

void calc_skip() {
  while (g_calc_expr[g_calc_pos] == ' ') ++g_calc_pos;
}

double calc_parse_expr();

double calc_parse_number() {
  calc_skip();
  const bool starts_dot = g_calc_expr[g_calc_pos] == '.';
  if (!starts_dot && !std::isdigit((unsigned char)g_calc_expr[g_calc_pos])) {
    g_calc_parse_ok = false;
    return 0;
  }
  double v = 0;
  while (std::isdigit((unsigned char)g_calc_expr[g_calc_pos])) {
    v = v * 10 + (g_calc_expr[g_calc_pos] - '0');
    ++g_calc_pos;
  }
  if (g_calc_expr[g_calc_pos] == '.') {
    ++g_calc_pos;
    double place = 0.1;
    if (!std::isdigit((unsigned char)g_calc_expr[g_calc_pos]) && !starts_dot) {
      /* trailing '.' ok → treat as integer */
    } else {
      while (std::isdigit((unsigned char)g_calc_expr[g_calc_pos])) {
        v += (g_calc_expr[g_calc_pos] - '0') * place;
        place *= 0.1;
        ++g_calc_pos;
      }
    }
  }
  return v;
}

double calc_parse_primary() {
  calc_skip();
  if (g_calc_expr[g_calc_pos] == '(') {
    ++g_calc_pos;
    double v = calc_parse_expr();
    calc_skip();
    if (g_calc_expr[g_calc_pos] != ')') {
      g_calc_parse_ok = false;
      return 0;
    }
    ++g_calc_pos;
    return v;
  }
  return calc_parse_number();
}

double calc_parse_unary() {
  calc_skip();
  if (g_calc_expr[g_calc_pos] == '-') {
    ++g_calc_pos;
    return -calc_parse_unary();
  }
  if (g_calc_expr[g_calc_pos] == '+') {
    ++g_calc_pos;
    return calc_parse_unary();
  }
  return calc_parse_primary();
}

double calc_parse_term() {
  double v = calc_parse_unary();
  for (;;) {
    calc_skip();
    char op = g_calc_expr[g_calc_pos];
    if (op != '*' && op != '/' && op != 'x') break;
    ++g_calc_pos;
    double r = calc_parse_unary();
    if (op == '*' || op == 'x')
      v *= r;
    else {
      if (r == 0) {
        g_calc_parse_ok = false;
        return 0;
      }
      v /= r;
    }
  }
  return v;
}

double calc_parse_expr() {
  double v = calc_parse_term();
  for (;;) {
    calc_skip();
    char op = g_calc_expr[g_calc_pos];
    if (op != '+' && op != '-') break;
    ++g_calc_pos;
    double r = calc_parse_term();
    if (op == '+')
      v += r;
    else
      v -= r;
  }
  return v;
}

bool calc_eval(double * out) {
  if (!g_calc_expr[0]) {
    *out = 0;
    return true;
  }
  g_calc_pos = 0;
  g_calc_parse_ok = true;
  double v = calc_parse_expr();
  calc_skip();
  if (!g_calc_parse_ok || g_calc_expr[g_calc_pos] != '\0') return false;
  *out = v;
  return true;
}

void calc_begin_edit() {
  if (g_calc_error) {
    g_calc_expr[0] = '\0';
    g_calc_error = false;
  }
  g_calc_just_eq = false;
}

void calc_append(char ch) {
  if (g_calc_just_eq) {
    if (std::isdigit((unsigned char)ch) || ch == '(' || ch == '.') {
      g_calc_expr[0] = '\0';
    } else if (!calc_is_op(ch)) {
      g_calc_expr[0] = '\0';
    }
    g_calc_just_eq = false;
    g_calc_error = false;
  } else if (g_calc_error) {
    g_calc_expr[0] = '\0';
    g_calc_error = false;
  }

  const char last = calc_last();
  const size_t len = std::strlen(g_calc_expr);

  if (ch == '.') {
    if (last == ')' || last == '.') return;
    /* Only one decimal in the current number token. */
    for (size_t i = len; i > 0; --i) {
      const char c = g_calc_expr[i - 1];
      if (c == '.') return;
      if (!std::isdigit((unsigned char)c)) break;
    }
    if (len + 1 >= (size_t)kCalcExprMax) return;
    if (!last || calc_is_op(last) || last == '(') {
      /* Leading decimal → "0." */
      if (len + 2 >= (size_t)kCalcExprMax) return;
      g_calc_expr[len] = '0';
      g_calc_expr[len + 1] = '.';
      g_calc_expr[len + 2] = '\0';
    } else {
      g_calc_expr[len] = '.';
      g_calc_expr[len + 1] = '\0';
    }
    calc_show();
    return;
  }

  if (std::isdigit((unsigned char)ch)) {
    if (last == ')') return;
    if (len + 1 >= (size_t)kCalcExprMax) return;
    g_calc_expr[len] = ch;
    g_calc_expr[len + 1] = '\0';
    calc_show();
    return;
  }

  if (ch == '(') {
    if (last && (std::isdigit((unsigned char)last) || last == ')' || last == '.')) return;
    if (len + 1 >= (size_t)kCalcExprMax) return;
    g_calc_expr[len] = '(';
    g_calc_expr[len + 1] = '\0';
    calc_show();
    return;
  }

  if (ch == ')') {
    if (calc_open_parens() <= 0) return;
    if (!last || calc_is_op(last) || last == '(' || last == '.') return;
    if (len + 1 >= (size_t)kCalcExprMax) return;
    g_calc_expr[len] = ')';
    g_calc_expr[len + 1] = '\0';
    calc_show();
    return;
  }

  if (calc_is_op(ch)) {
    if (ch == '*') ch = 'x';
    const bool unary_minus = (ch == '-' && (!last || last == '(' || calc_is_op(last)));
    if (unary_minus) {
      if (len + 1 >= (size_t)kCalcExprMax) return;
      g_calc_expr[len] = '-';
      g_calc_expr[len + 1] = '\0';
      calc_show();
      return;
    }
    if (!last || last == '(') return; /* binary ops need a left side */
    if (last == '.') return;
    if (calc_is_op(last)) {
      /* replace trailing binary op (keep unary - after replace via separate tap) */
      g_calc_expr[len - 1] = ch;
      calc_show();
      return;
    }
    if (len + 1 >= (size_t)kCalcExprMax) return;
    g_calc_expr[len] = ch;
    g_calc_expr[len + 1] = '\0';
    calc_show();
  }
}

void calc_digit(int d) { calc_append((char)('0' + d)); }

void calc_op(char op) { calc_append(op); }

void calc_paren(char p) { calc_append(p); }

void calc_eq() {
  if (g_calc_error || !g_calc_expr[0]) {
    calc_show();
    return;
  }
  /* trim trailing ops for friendliness */
  while (calc_is_op(calc_last()) || calc_last() == '(') {
    size_t n = std::strlen(g_calc_expr);
    if (!n) break;
    g_calc_expr[n - 1] = '\0';
  }
  double v = 0;
  if (!calc_eval(&v)) {
    g_calc_error = true;
    g_calc_just_eq = true;
    calc_show();
    return;
  }
  calc_format_result(v, g_calc_expr, sizeof(g_calc_expr));
  if (std::strcmp(g_calc_expr, "Error") == 0) g_calc_error = true;
  g_calc_just_eq = true;
  calc_show();
}

void calc_clear() {
  g_calc_expr[0] = '\0';
  g_calc_just_eq = false;
  g_calc_error = false;
  calc_show();
}

void calc_back() {
  calc_begin_edit();
  size_t n = std::strlen(g_calc_expr);
  if (!n) {
    calc_show();
    return;
  }
  g_calc_expr[n - 1] = '\0';
  calc_show();
}

lv_obj_t * calc_btn(lv_obj_t * grid, const char * lab, int col, int row, int span,
                    lv_event_cb_t cb, void * ud, bool accent) {
  lv_obj_t * b = lv_button_create(grid);
  lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, col, span, LV_GRID_ALIGN_STRETCH, row, 1);
  lv_obj_set_style_radius(b, 12, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_bg_color(b, accent ? theme::gold() : theme::panel(), 0);
  lv_obj_t * l = lv_label_create(b);
  lv_label_set_text(l, lab);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(l, accent ? lv_color_hex(0x1a1200) : theme::ink(), 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

}  // namespace

lv_obj_t * utils_folder_screen() {
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "UTILITIES", app::desk().name);
  lv_obj_t * body = make_body(scr, true);

  lv_obj_t * grid = lv_obj_create(body);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, lv_pct(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(grid, cols, rows);
  lv_obj_set_style_pad_row(grid, 16, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  struct Item {
    AppIcon icon;
    const char * label;
    lv_event_cb_t cb;
  };
  const Item items[] = {
      {AppIcon::Timer, "Timer", [](lv_event_t * /*e*/) { go_timer(); }},
      {AppIcon::Checklist, "Checklist", [](lv_event_t * /*e*/) { go_checklist(); }},
      {AppIcon::Calculator, "Calculator", [](lv_event_t * /*e*/) { go_calculator(); }},
  };
  for (int i = 0; i < 3; ++i) {
    lv_obj_t * icon = make_app_icon(grid, items[i].icon, items[i].label, items[i].cb);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, i, 1, LV_GRID_ALIGN_START, 0, 1);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Home", false, false, [](lv_event_t * /*e*/) { go_hub(); });
  return scr;
}

/** Swipe-left reveal delete: clip viewport + horizontal scroller [front | X]. */
constexpr int kCheckRowH = 48;
constexpr int kCheckDelW = 56;
constexpr int kCheckBodyPadX = 14;

void checklist_close_rows(lv_obj_t * body, lv_obj_t * except_scroller) {
  const uint32_t n = lv_obj_get_child_count(body);
  for (uint32_t i = 0; i < n; ++i) {
    lv_obj_t * clip = lv_obj_get_child(body, i);
    if (!clip || lv_obj_get_child_count(clip) == 0) continue;
    lv_obj_t * sc = lv_obj_get_child(clip, 0);
    if (!sc || sc == except_scroller) continue;
    if (lv_obj_get_scroll_x(sc) != 0) lv_obj_scroll_to_x(sc, 0, LV_ANIM_ON);
  }
}

void checklist_snap_row(lv_event_t * e) {
  lv_obj_t * sc = (lv_obj_t *)lv_event_get_target(e);
  const int x = lv_obj_get_scroll_x(sc);
  const int target = (x > kCheckDelW / 2) ? kCheckDelW : 0;
  if (x == target) return;
  if (target) checklist_close_rows(lv_obj_get_parent(lv_obj_get_parent(sc)), sc);
  lv_obj_scroll_to_x(sc, target, LV_ANIM_ON);
}

lv_obj_t * checklist_screen() {
  checklist::init();
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "CHECKLIST", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  lv_obj_set_style_pad_row(body, 8, 0);

  const int n = checklist::count();
  if (n == 0) make_tagline(body, "No tasks - tap Add.");

  const int32_t row_w = WP_HOR_RES - (kCheckBodyPadX * 2);

  for (int i = 0; i < n; ++i) {
    const checklist::Item * it = checklist::at(i);
    if (!it) continue;

    lv_obj_t * clip = lv_obj_create(body);
    lv_obj_remove_style_all(clip);
    lv_obj_set_size(clip, row_w, kCheckRowH);
    lv_obj_set_style_radius(clip, 12, 0);
    lv_obj_set_style_clip_corner(clip, true, 0);
    lv_obj_set_style_bg_color(clip, theme::danger(), 0);
    lv_obj_set_style_bg_opa(clip, LV_OPA_COVER, 0);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * scroller = lv_obj_create(clip);
    lv_obj_remove_style_all(scroller);
    lv_obj_set_size(scroller, row_w, kCheckRowH);
    lv_obj_set_pos(scroller, 0, 0);
    lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scroller, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(scroller, 0, 0);
    lv_obj_set_style_pad_column(scroller, 0, 0);
    lv_obj_set_scroll_dir(scroller, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(scroller, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(scroller, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_event_cb(scroller, checklist_snap_row, LV_EVENT_SCROLL_END, nullptr);
    lv_obj_add_event_cb(
        scroller,
        [](lv_event_t * e) {
          lv_obj_t * sc = (lv_obj_t *)lv_event_get_target(e);
          checklist_close_rows(lv_obj_get_parent(lv_obj_get_parent(sc)), sc);
        },
        LV_EVENT_SCROLL_BEGIN, nullptr);

    lv_obj_t * front = lv_obj_create(scroller);
    lv_obj_remove_style_all(front);
    lv_obj_set_size(front, row_w, kCheckRowH);
    lv_obj_set_flex_flow(front, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(front, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(front, 8, 0);
    lv_obj_set_style_pad_column(front, 10, 0);
    lv_obj_set_style_bg_color(front, theme::panel(), 0);
    lv_obj_set_style_bg_opa(front, LV_OPA_COVER, 0);
    lv_obj_remove_flag(front, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(front, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        front,
        [](lv_event_t * e) {
          lv_obj_t * fr = (lv_obj_t *)lv_event_get_target(e);
          lv_obj_t * sc = lv_obj_get_parent(fr);
          if (lv_obj_get_scroll_x(sc) > 4) {
            lv_obj_scroll_to_x(sc, 0, LV_ANIM_ON);
            return;
          }
          checklist::toggle((int)(intptr_t)lv_event_get_user_data(e));
          go_checklist();
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t * box = lv_obj_create(front);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 28, 28);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, theme::gold(), 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    if (it->done) {
      lv_obj_set_style_bg_color(box, theme::gold(), 0);
      lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
      lv_obj_t * ck = lv_label_create(box);
      lv_label_set_text(ck, LV_SYMBOL_OK);
      lv_obj_set_style_text_color(ck, lv_color_hex(0x1a1200), 0);
      lv_obj_center(ck);
      lv_obj_remove_flag(ck, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    }

    lv_obj_t * lab = lv_label_create(front);
    lv_label_set_text(lab, it->text);
    lv_obj_set_style_text_color(lab, it->done ? theme::muted() : theme::ink(), 0);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(lab, 1);
    lv_obj_set_width(lab, LV_SIZE_CONTENT);
    lv_obj_remove_flag(lab, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * rm = lv_button_create(scroller);
    lv_obj_set_size(rm, kCheckDelW, kCheckRowH);
    lv_obj_set_style_radius(rm, 0, 0);
    lv_obj_set_style_bg_color(rm, theme::danger(), 0);
    lv_obj_set_style_bg_opa(rm, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(rm, 0, 0);
    lv_obj_set_style_pad_all(rm, 0, 0);
    lv_obj_t * xl = lv_label_create(rm);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xl, lv_color_hex(0xffffff), 0);
    lv_obj_center(xl);
    lv_obj_add_event_cb(
        rm,
        [](lv_event_t * e) {
          checklist::remove((int)(intptr_t)lv_event_get_user_data(e));
          go_checklist();
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Clear done", false, false, [](lv_event_t * /*e*/) {
    checklist::clear_done();
    go_checklist();
  });
  dock_btn(dock, "Add", true, false, [](lv_event_t * /*e*/) {
    if (checklist::count() >= checklist::kMaxItems) {
      toast("List full (7 max)");
      return;
    }
    go_keyboard_checklist();
  });
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_utils_folder(); });
  return scr;
}

lv_obj_t * calculator_screen() {
  g_calc_lbl = nullptr;
  lv_obj_t * scr = make_screen();
  make_topbar(scr, "CALCULATOR", app::desk().name);
  lv_obj_t * body = make_body(scr, true);
  lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(body, 8, 0);

  g_calc_lbl = lv_label_create(body);
  lv_obj_set_width(g_calc_lbl, lv_pct(100));
  lv_obj_set_style_text_align(g_calc_lbl, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(g_calc_lbl, font_display(28), 0);
  lv_obj_set_style_text_color(g_calc_lbl, theme::ink(), 0);
  lv_obj_set_style_bg_color(g_calc_lbl, theme::panel(), 0);
  lv_obj_set_style_bg_opa(g_calc_lbl, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_calc_lbl, 12, 0);
  lv_obj_set_style_pad_all(g_calc_lbl, 12, 0);
  lv_label_set_long_mode(g_calc_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
  calc_show();

  lv_obj_t * grid = lv_obj_create(body);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, lv_pct(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(grid, cols, rows);
  lv_obj_set_style_pad_row(grid, 6, 0);
  lv_obj_set_style_pad_column(grid, 6, 0);

  auto dig = [](lv_event_t * e) {
    calc_digit((int)(intptr_t)lv_event_get_user_data(e));
  };
  auto op = [](lv_event_t * e) {
    calc_op((char)(intptr_t)lv_event_get_user_data(e));
  };
  auto paren = [](lv_event_t * e) {
    calc_paren((char)(intptr_t)lv_event_get_user_data(e));
  };

  calc_btn(grid, "C", 0, 0, 1, [](lv_event_t * /*e*/) { calc_clear(); }, nullptr, false);
  calc_btn(grid, LV_SYMBOL_BACKSPACE, 1, 0, 1, [](lv_event_t * /*e*/) { calc_back(); }, nullptr,
           false);
  calc_btn(grid, "(", 2, 0, 1, paren, (void *)(intptr_t)'(', false);
  calc_btn(grid, ")", 3, 0, 1, paren, (void *)(intptr_t)')', false);

  calc_btn(grid, "7", 0, 1, 1, dig, (void *)(intptr_t)7, false);
  calc_btn(grid, "8", 1, 1, 1, dig, (void *)(intptr_t)8, false);
  calc_btn(grid, "9", 2, 1, 1, dig, (void *)(intptr_t)9, false);
  calc_btn(grid, "/", 3, 1, 1, op, (void *)(intptr_t)'/', true);

  calc_btn(grid, "4", 0, 2, 1, dig, (void *)(intptr_t)4, false);
  calc_btn(grid, "5", 1, 2, 1, dig, (void *)(intptr_t)5, false);
  calc_btn(grid, "6", 2, 2, 1, dig, (void *)(intptr_t)6, false);
  calc_btn(grid, "x", 3, 2, 1, op, (void *)(intptr_t)'*', true);

  calc_btn(grid, "1", 0, 3, 1, dig, (void *)(intptr_t)1, false);
  calc_btn(grid, "2", 1, 3, 1, dig, (void *)(intptr_t)2, false);
  calc_btn(grid, "3", 2, 3, 1, dig, (void *)(intptr_t)3, false);
  calc_btn(grid, "-", 3, 3, 1, op, (void *)(intptr_t)'-', true);

  calc_btn(grid, "0", 0, 4, 1, dig, (void *)(intptr_t)0, false);
  calc_btn(grid, ".", 1, 4, 1, [](lv_event_t * /*e*/) { calc_append('.'); }, nullptr, false);
  calc_btn(grid, "=", 2, 4, 1, [](lv_event_t * /*e*/) { calc_eq(); }, nullptr, true);
  calc_btn(grid, "+", 3, 4, 1, op, (void *)(intptr_t)'+', true);

  lv_obj_t * dock = make_dock(scr);
  dock_btn(dock, "Back", false, false, [](lv_event_t * /*e*/) { go_utils_folder(); });
  return scr;
}

}  // namespace ui
}  // namespace wp
