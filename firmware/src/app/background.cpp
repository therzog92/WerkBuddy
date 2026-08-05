#include "app/background.h"

#include "app/app.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace wp {
namespace background {
namespace {

namespace fs = std::filesystem;

fs::path sim_data_bg_dir() {
  const fs::path candidates[] = {
      fs::current_path() / ".." / "sim-data" / "bg",
      fs::current_path() / "sim-data" / "bg",
      fs::path("C:/Users/Tommy/Projects/WerkPager/firmware/sim-data/bg"),
  };
  for (const fs::path & p : candidates) {
    if (fs::exists(p) || fs::exists(p.parent_path())) return p;
  }
  return candidates[0];
}

fs::path session_path() {
  return sim_data_bg_dir().parent_path() / "bg_upload_session.json";
}

std::string g_file;
std::string g_lv;
bool g_checked = false;
bool g_has = false;
std::string g_last_stamp;

void refresh_paths() {
  g_checked = true;
  g_has = false;
  g_file.clear();
  g_lv.clear();

  const char * id = app::desk().id;
  const fs::path candidates[] = {
      fs::current_path() / "assets" / "bg" / (std::string(id) + ".png"),
      fs::current_path() / ".." / "assets" / "bg" / (std::string(id) + ".png"),
      fs::current_path() / ".." / "sim-data" / "bg" / (std::string(id) + ".png"),
      fs::current_path() / "sim-data" / "bg" / (std::string(id) + ".png"),
      fs::path("C:/Users/Tommy/Projects/WerkPager/firmware/sim-data/bg") / (std::string(id) + ".png"),
      fs::path("C:/Users/Tommy/Projects/WerkPager/firmware/assets/bg") / (std::string(id) + ".png"),
  };

  fs::path chosen;
  for (const fs::path & png : candidates) {
    if (!fs::exists(png)) continue;
    chosen = png;
    break;
  }
  if (chosen.empty()) return;

  /* Prefer a short relative LVGL path when the file lives under cwd/assets (emoji style). */
  const fs::path rel = fs::current_path() / "assets" / "bg" / (std::string(id) + ".png");
  if (fs::exists(rel)) {
    g_file = rel.string();
    for (char & c : g_file) {
      if (c == '\\') c = '/';
    }
    g_lv = "S:assets/bg/" + std::string(id) + ".png";
  } else {
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(chosen, ec);
    if (ec) canon = fs::absolute(chosen, ec);
    if (ec) canon = chosen;
    g_file = canon.string();
    for (char & c : g_file) {
      if (c == '\\') c = '/';
    }
    g_lv = "S:" + g_file;
  }

  lv_image_header_t hdr{};
  if (lv_image_decoder_get_info(g_lv.c_str(), &hdr) != LV_RESULT_OK || hdr.w == 0) {
    g_file.clear();
    g_lv.clear();
    return;
  }
  g_has = true;
}

std::string json_string_field(const std::string & json, const std::string & key) {
  const std::string needle = "\"" + key + "\":";
  size_t p = json.find(needle);
  if (p == std::string::npos) return {};
  p += needle.size();
  while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
  if (p >= json.size() || json[p] != '"') return {};
  ++p;
  std::string out;
  while (p < json.size() && json[p] != '"') {
    if (json[p] == '\\' && p + 1 < json.size()) {
      out.push_back(json[p + 1]);
      p += 2;
      continue;
    }
    out.push_back(json[p++]);
  }
  return out;
}

/** Find desks.<id> object blob (brace-matched). */
std::string desk_object(const std::string & json, const char * desk_id) {
  const std::string needle = std::string("\"") + desk_id + "\"";
  size_t p = json.find(needle);
  if (p == std::string::npos) return {};
  p = json.find('{', p);
  if (p == std::string::npos) return {};
  int depth = 0;
  size_t start = p;
  for (; p < json.size(); ++p) {
    if (json[p] == '{') ++depth;
    else if (json[p] == '}') {
      --depth;
      if (depth == 0) return json.substr(start, p - start + 1);
    }
  }
  return {};
}

}  // namespace

bool has() {
  if (!g_checked) refresh_paths();
  return g_has;
}

const char * lv_src() {
  if (!g_checked) refresh_paths();
  return g_has ? g_lv.c_str() : nullptr;
}

const char * file_path() {
  if (!g_checked) refresh_paths();
  return g_has ? g_file.c_str() : nullptr;
}

void reload() {
  g_checked = false;
  refresh_paths();
  if (g_has && !g_lv.empty()) {
    lv_image_cache_drop(g_lv.c_str());
  }
}

void clear() {
  refresh_paths();
  if (!g_file.empty()) {
    std::error_code ec;
    fs::remove(g_file, ec);
    const fs::path jpg = fs::path(g_file).replace_extension(".jpg");
    fs::remove(jpg, ec);
    const fs::path stamp = fs::path(g_file).replace_extension(".stamp");
    fs::remove(stamp, ec);
  }
  g_checked = false;
  g_has = false;
  g_file.clear();
  g_lv.clear();
  g_last_stamp.clear();
}

bool upload_session(char * url, size_t url_n, char * local_url, size_t local_n, char * qr_lv_src,
                    size_t qr_n) {
  if (url && url_n) url[0] = '\0';
  if (local_url && local_n) local_url[0] = '\0';
  if (qr_lv_src && qr_n) qr_lv_src[0] = '\0';

  const fs::path sp = session_path();
  if (!fs::exists(sp)) return false;
  std::ifstream in(sp);
  if (!in) return false;
  std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const std::string obj = desk_object(json, app::desk().id);
  if (obj.empty()) return false;

  const std::string u = json_string_field(obj, "url");
  const std::string lu = json_string_field(obj, "local_url");
  const std::string qr = json_string_field(obj, "qr");
  if (u.empty()) return false;

  if (url && url_n) std::snprintf(url, url_n, "%s", u.c_str());
  if (local_url && local_n) std::snprintf(local_url, local_n, "%s", lu.c_str());
  if (qr_lv_src && qr_n && !qr.empty() && fs::exists(qr)) {
    std::string s = "S:" + qr;
    for (char & c : s) {
      if (c == '\\') c = '/';
    }
    std::snprintf(qr_lv_src, qr_n, "%s", s.c_str());
  }
  return true;
}

bool poll_new_upload() {
  const fs::path stamp = sim_data_bg_dir() / (std::string(app::desk().id) + ".stamp");
  if (!fs::exists(stamp)) return false;
  std::ifstream in(stamp);
  std::string cur;
  std::getline(in, cur);
  if (cur.empty()) return false;
  if (cur == g_last_stamp) return false;
  const bool first = g_last_stamp.empty();
  g_last_stamp = cur;
  reload();
  /* Ignore the first observation so opening the screen doesn't toast immediately. */
  return !first && has();
}

}  // namespace background
}  // namespace wp
