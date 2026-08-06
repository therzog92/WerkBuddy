#include "sim/github_ota.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace wp {
namespace sim {
namespace github_ota {
namespace {

void set_err(char * err, int err_cap, const char * msg) {
  if (err && err_cap > 0) std::snprintf(err, err_cap, "%s", msg ? msg : "error");
}

const char * find_key(const char * json, const char * key) {
  if (!json || !key) return nullptr;
  char needle[48];
  std::snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char * p = std::strstr(json, needle);
  if (!p) return nullptr;
  p = std::strchr(p + std::strlen(needle), ':');
  if (!p) return nullptr;
  ++p;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
  return p;
}

bool parse_string(const char * p, char * dst, int cap) {
  if (!p || !dst || cap <= 0 || *p != '"') return false;
  ++p;
  int n = 0;
  while (*p && *p != '"' && n + 1 < cap) {
    if (*p == '\\' && p[1]) {
      ++p;
      if (*p == 'n') dst[n++] = '\n';
      else if (*p == 't') dst[n++] = '\t';
      else dst[n++] = *p;
      ++p;
      continue;
    }
    dst[n++] = *p++;
  }
  dst[n] = '\0';
  return *p == '"';
}

/** Find matching `}` for object starting at `{`. */
const char * object_end(const char * start) {
  if (!start || *start != '{') return nullptr;
  int depth = 0;
  bool in_str = false;
  for (const char * p = start; *p; ++p) {
    if (in_str) {
      if (*p == '\\' && p[1]) {
        ++p;
        continue;
      }
      if (*p == '"') in_str = false;
      continue;
    }
    if (*p == '"') {
      in_str = true;
      continue;
    }
    if (*p == '{') ++depth;
    else if (*p == '}') {
      --depth;
      if (depth == 0) return p;
    }
  }
  return nullptr;
}

bool https_get(const wchar_t * host, const wchar_t * path, std::string & body, char * err,
               int err_cap) {
#ifdef _WIN32
  HINTERNET sess = WinHttpOpen(L"WerkBuddySim/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!sess) {
    set_err(err, err_cap, "WinHttpOpen failed");
    return false;
  }
  WinHttpSetTimeouts(sess, 5000, 5000, 10000, 15000);

  HINTERNET conn = WinHttpConnect(sess, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!conn) {
    set_err(err, err_cap, "WinHttpConnect failed");
    WinHttpCloseHandle(sess);
    return false;
  }

  HINTERNET req =
      WinHttpOpenRequest(conn, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!req) {
    set_err(err, err_cap, "WinHttpOpenRequest failed");
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(sess);
    return false;
  }

  BOOL ok = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0,
                               0);
  if (!ok || !WinHttpReceiveResponse(req, nullptr)) {
    set_err(err, err_cap, "HTTP request failed");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(sess);
    return false;
  }

  DWORD status = 0;
  DWORD status_sz = sizeof(status);
  WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_sz, WINHTTP_NO_HEADER_INDEX);
  if (status != 200) {
    char msg[64];
    std::snprintf(msg, sizeof(msg), "HTTP %lu", (unsigned long)status);
    set_err(err, err_cap, msg);
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(sess);
    return false;
  }

  body.clear();
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(req, &avail)) break;
    if (avail == 0) break;
    if (body.size() + avail > 2 * 1024 * 1024) {
      set_err(err, err_cap, "response too large");
      WinHttpCloseHandle(req);
      WinHttpCloseHandle(conn);
      WinHttpCloseHandle(sess);
      return false;
    }
    std::vector<char> buf(avail);
    DWORD read = 0;
    if (!WinHttpReadData(req, buf.data(), avail, &read) || read == 0) break;
    body.append(buf.data(), read);
  }

  WinHttpCloseHandle(req);
  WinHttpCloseHandle(conn);
  WinHttpCloseHandle(sess);
  return !body.empty();
#else
  (void)host;
  (void)path;
  (void)body;
  set_err(err, err_cap, "GitHub OTA fetch is Windows-sim only for now");
  return false;
#endif
}

int parse_releases(const std::string & json, Release * out, int max_out) {
  if (!out || max_out <= 0) return 0;
  const char * p = json.c_str();
  while (*p && *p != '[') ++p;
  if (*p != '[') return 0;
  ++p;

  int n = 0;
  while (*p && n < max_out) {
    while (*p && *p != '{' && *p != ']') ++p;
    if (*p != '{') break;
    const char * end = object_end(p);
    if (!end) break;

    /* Work on a bounded slice so nested objects don't bleed. */
    const size_t len = (size_t)(end - p + 1);
    std::string slice(p, len);

    Release r{};
    const char * tag_p = find_key(slice.c_str(), "tag_name");
    if (!tag_p || !parse_string(tag_p, r.tag, sizeof(r.tag))) {
      p = end + 1;
      continue;
    }
    const char * name_p = find_key(slice.c_str(), "name");
    if (!name_p || !parse_string(name_p, r.name, sizeof(r.name))) {
      std::snprintf(r.name, sizeof(r.name), "%s", r.tag);
    }

    /* Prefer first .bin asset download URL inside this release object. */
    const char * assets = std::strstr(slice.c_str(), "\"assets\"");
    if (assets) {
      const char * q = assets;
      while ((q = std::strstr(q, "\"browser_download_url\"")) != nullptr) {
        const char * colon = std::strchr(q, ':');
        if (!colon) break;
        ++colon;
        while (*colon == ' ' || *colon == '\t') ++colon;
        char url[kUrlLen];
        if (parse_string(colon, url, sizeof(url))) {
          if (std::strstr(url, ".bin")) {
            std::snprintf(r.asset_url, sizeof(r.asset_url), "%s", url);
            r.has_bin = true;
            break;
          }
        }
        q = colon + 1;
      }
    }

    out[n++] = r;
    p = end + 1;
  }
  return n;
}

}  // namespace

const char * tag_body(const char * tag) {
  if (!tag || !tag[0]) return "";
  return (tag[0] == 'v' || tag[0] == 'V') ? tag + 1 : tag;
}

int fetch_releases(Release * out, int max_out, char * err, int err_cap) {
  if (err && err_cap > 0) err[0] = '\0';
  if (!out || max_out <= 0) {
    set_err(err, err_cap, "bad args");
    return -1;
  }

  std::string body;
#ifdef _WIN32
  char path_n[128];
  std::snprintf(path_n, sizeof(path_n), "/repos/%s/%s/releases?per_page=%d", kOwner, kRepo,
                max_out);
  wchar_t path[128];
  MultiByteToWideChar(CP_UTF8, 0, path_n, -1, path, 128);
  if (!https_get(L"api.github.com", path, body, err, err_cap)) return -1;
#else
  if (!https_get(nullptr, nullptr, body, err, err_cap)) return -1;
#endif

  const int n = parse_releases(body, out, max_out);
  if (n <= 0) {
    set_err(err, err_cap, "no releases in response");
    return -1;
  }
  return n;
}

}  // namespace github_ota
}  // namespace sim
}  // namespace wp
