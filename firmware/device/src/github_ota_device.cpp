/*
 * Device GitHub Releases list + OTA flash (ephemeral STA must already be up).
 */

#include "sim/github_ota.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace wp {
namespace sim {
namespace github_ota {
namespace {

void set_err(char * err, int err_cap, const char * msg) {
  if (err && err_cap > 0) std::snprintf(err, (size_t)err_cap, "%s", msg ? msg : "error");
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
      if (*p == 'n')
        dst[n++] = '\n';
      else if (*p == 't')
        dst[n++] = '\t';
      else
        dst[n++] = *p;
      ++p;
      continue;
    }
    dst[n++] = *p++;
  }
  dst[n] = '\0';
  return *p == '"';
}

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
    if (*p == '{')
      ++depth;
    else if (*p == '}') {
      --depth;
      if (depth == 0) return p;
    }
  }
  return nullptr;
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

bool https_get_body(const char * url, std::string & body, char * err, int err_cap,
                    size_t max_bytes) {
  body.clear();
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(20000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("WerkBuddy-OTA/1.0");
  if (!http.begin(client, url)) {
    set_err(err, err_cap, "HTTP begin failed");
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char msg[48];
    std::snprintf(msg, sizeof(msg), "HTTP %d", code);
    set_err(err, err_cap, msg);
    http.end();
    return false;
  }
  WiFiClient * stream = http.getStreamPtr();
  if (!stream) {
    set_err(err, err_cap, "no stream");
    http.end();
    return false;
  }
  uint8_t buf[1024];
  while (http.connected() || stream->available()) {
    const size_t avail = stream->available();
    if (!avail) {
      delay(1);
      continue;
    }
    const size_t n = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
    if (!n) break;
    if (body.size() + n > max_bytes) {
      set_err(err, err_cap, "response too large");
      http.end();
      return false;
    }
    body.append(reinterpret_cast<char *>(buf), n);
  }
  http.end();
  return !body.empty();
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
  if (WiFi.status() != WL_CONNECTED) {
    set_err(err, err_cap, "Wi-Fi not connected");
    return -1;
  }

  char url[160];
  std::snprintf(url, sizeof(url), "https://api.github.com/repos/%s/%s/releases?per_page=%d",
                kOwner, kRepo, max_out);

  std::string body;
  if (!https_get_body(url, body, err, err_cap, 512 * 1024)) return -1;

  const int n = parse_releases(body, out, max_out);
  if (n <= 0) {
    set_err(err, err_cap, "no releases in response");
    return -1;
  }
  return n;
}

bool install_bin(const char * url, char * err, int err_cap) {
  if (err && err_cap > 0) err[0] = '\0';
  if (!url || !url[0]) {
    set_err(err, err_cap, "no firmware URL");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    set_err(err, err_cap, "Wi-Fi not connected");
    return false;
  }

  /* GitHub release assets redirect github.com -> release-assets.githubusercontent.com.
   * ESP32 HTTPClient's cross-host redirect reconnect reuses the same TLS client for a
   * different host and fails (returns -1 / connection refused). Resolve each redirect
   * manually with a fresh TLS connection per hop instead. */
  String target(url);
  for (int hop = 0; hop < 5; ++hop) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(60000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setUserAgent("WerkBuddy-OTA/1.0");
    if (!http.begin(client, target)) {
      set_err(err, err_cap, "HTTP begin failed");
      return false;
    }

    const int code = http.GET();
    if (code == HTTP_CODE_MOVED_PERMANENTLY || code == HTTP_CODE_FOUND ||
        code == HTTP_CODE_SEE_OTHER || code == HTTP_CODE_TEMPORARY_REDIRECT) {
      /* ESP32 stores the redirect target in _location (getLocation()), not header(). */
      const String loc = http.getLocation();
      http.end();
      client.stop();
      if (!loc.length() || !loc.startsWith("https://")) {
        set_err(err, err_cap, "bad redirect URL");
        return false;
      }
      target = loc;
      continue;
    }

    if (code != HTTP_CODE_OK) {
      char msg[48];
      std::snprintf(msg, sizeof(msg), "HTTP %d", code);
      set_err(err, err_cap, msg);
      http.end();
      return false;
    }

    const int content_len = http.getSize();
    constexpr size_t kMaxFw = 0x600000 - 0x10000; /* OTA slot size (6MB) minus margin */
    if (content_len > 0 && (size_t)content_len > kMaxFw) {
      set_err(err, err_cap, "firmware too large");
      http.end();
      return false;
    }

    if (!Update.begin(content_len > 0 ? (size_t)content_len : UPDATE_SIZE_UNKNOWN)) {
      set_err(err, err_cap, "Update.begin failed");
      http.end();
      return false;
    }

    WiFiClient * stream = http.getStreamPtr();
    if (!stream) {
      set_err(err, err_cap, "no stream");
      Update.abort();
      http.end();
      return false;
    }

    size_t written = 0;
    uint8_t buf[4096];
    const int expected = content_len;
    while (http.connected() || stream->available()) {
      const size_t avail = stream->available();
      if (!avail) {
        delay(1);
        continue;
      }
      const size_t n = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
      if (!n) break;
      if (Update.write(buf, n) != n) {
        set_err(err, err_cap, "flash write failed");
        Update.abort();
        http.end();
        return false;
      }
      written += n;
      if (expected > 0 && (int)written >= expected) break;
    }
    http.end();

    if (!Update.end(true)) {
      set_err(err, err_cap, "Update.end failed");
      return false;
    }
    Serial.printf("OTA written %u bytes — rebooting\n", (unsigned)written);
    delay(200);
    ESP.restart();
    return true; /* unreachable */
  }

  set_err(err, err_cap, "too many redirects");
  return false;
}

}  // namespace github_ota
}  // namespace sim
}  // namespace wp
