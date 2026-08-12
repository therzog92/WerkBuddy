/*
 * Device wallpaper: SoftAP + captive portal + HTTP JPEG upload → LittleFS.
 * Phone scans WIFI QR, joins WerkBuddy-Upload, opens upload page (captive portal).
 */

#include "app/background.h"

#include "app/app.h"
#include "device_net.h"

#include <DNSServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include <cstdio>
#include <cstring>

/* ChaN TJpgDec is already linked via LVGL (LV_USE_TJPGD). */
extern "C" {
#include "libs/tjpgd/tjpgd.h"
}

namespace wp {
namespace background {
namespace {

Preset g_preset = Preset::Theme;
bool g_fs_ok = false;
bool g_has = false;
bool g_checked = false;
bool g_job = false;
bool g_pending_finalize = false; /* JPEG on FS; bake after SoftAP teardown */
bool g_seeded_poll = false;

constexpr const char * kApSsid = "WerkBuddy-Upload";
constexpr const char * kApPass = "werkbuddy";
constexpr uint8_t kApChannel = 1;
constexpr const char * kFilePath = "/wallpaper.jpg";
constexpr uint32_t kMaxSide = 480;
constexpr size_t kJpgPool = 48 * 1024;

uint16_t * g_pixels = nullptr;
lv_image_dsc_t g_img = {};

struct JpgBake {
  File * file;
  uint16_t * dst;
  uint32_t dw;
  uint32_t dh;
};

size_t jpg_input(JDEC * jd, uint8_t * buf, size_t ndata) {
  auto * io = static_cast<JpgBake *>(jd->device);
  if (!io || !io->file) return 0;
  if (buf) return (size_t)io->file->read(buf, ndata);
  const size_t pos = (size_t)io->file->position();
  if (!io->file->seek(pos + ndata)) return 0;
  return ndata;
}

int jpg_output(JDEC * jd, void * bitmap, JRECT * rect) {
  auto * io = static_cast<JpgBake *>(jd->device);
  if (!io || !io->dst || !bitmap || !rect) return 0;
  const uint8_t * src = static_cast<const uint8_t *>(bitmap);
  const uint32_t bw = (uint32_t)(rect->right - rect->left + 1);
  for (uint16_t y = rect->top; y <= rect->bottom; ++y) {
    if (y >= io->dh) {
      src += (size_t)bw * 3u;
      continue;
    }
    uint16_t * row = io->dst + (size_t)y * (size_t)io->dw;
    for (uint16_t x = rect->left; x <= rect->right; ++x) {
      /* TJpgDec JD_FORMAT=0 packs B,G,R — not R,G,B. */
      const uint8_t b = src[0], g = src[1], r = src[2];
      src += 3;
      if (x >= io->dw) continue;
      row[x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
  }
  return 1; /* continue */
}

char g_wifi_qr[96] = {};
char g_http_url[32] = "http://192.168.4.1/";

DNSServer g_dns;
WebServer g_http(80);
File g_upload;

/* Compact phone page — crop to 480² JPEG, multipart POST /upload */
const char kUploadHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no"/>
<title>WerkBuddy Background</title>
<style>
body{margin:0;background:#0c0a0f;color:#f7f2ea;font-family:system-ui,sans-serif}
main{max-width:440px;margin:0 auto;padding:16px}
h1{color:#f0c24b;letter-spacing:.08em;font-size:1.4rem;margin:0 0 8px}
.sub{color:#b9a8c9;font-size:.9rem;margin:0 0 14px}
.stage{position:relative;width:100%;aspect-ratio:1;background:#160e1f;border-radius:14px;
border:1px solid #3d2a55;overflow:hidden;touch-action:none;margin-bottom:12px}
.stage.empty{display:grid;place-items:center;color:#b9a8c9}
#viewport{position:absolute;inset:0;display:none}
#img{position:absolute;left:50%;top:50%;transform-origin:center;pointer-events:none}
.frame{position:absolute;inset:8%;border:2px solid #f0c24b;border-radius:8px;
box-shadow:0 0 0 9999px rgba(0,0,0,.55);pointer-events:none}
label.file,button.upload{display:block;width:100%;text-align:center;padding:14px;border:0;
border-radius:14px;font-weight:700;margin-bottom:10px}
label.file{background:linear-gradient(90deg,#ff4fa3,#f0c24b);color:#1a1224}
input[type=file]{display:none}
.row{display:flex;gap:8px;align-items:center;margin-bottom:12px;color:#b9a8c9;font-size:.85rem}
input[type=range]{flex:1;accent-color:#f0c24b}
button.upload{background:#f0c24b;color:#1a1224}
button.upload:disabled{opacity:.4}
.status{min-height:1.3em;color:#b9a8c9;font-size:.9rem}
.status.ok{color:#5dffc2}.status.err{color:#ff5c7a}
</style></head><body><main>
<h1>WERKBUDDY</h1>
<p class="sub">Scan joined this desk — crop a square photo for the wallpaper.</p>
<div class="stage empty" id="stage"><span id="emptyHint">Choose a photo</span>
<div id="viewport"><img id="img" alt=""/><div class="frame"></div></div></div>
<p class="sub" style="font-size:.8rem">Drag to pan · slider to zoom</p>
<label class="file">Choose photo<input type="file" id="file" accept="image/*"/></label>
<div class="row"><label for="zoom">Zoom</label>
<input type="range" id="zoom" min="100" max="400" value="100" disabled/></div>
<button class="upload" id="upload" disabled>Upload to desk</button>
<p class="status" id="status"></p>
</main>
<script>
(()=>{
const file=document.getElementById("file"),img=document.getElementById("img"),
stage=document.getElementById("stage"),viewport=document.getElementById("viewport"),
emptyHint=document.getElementById("emptyHint"),zoom=document.getElementById("zoom"),
uploadBtn=document.getElementById("upload"),status=document.getElementById("status");
let naturalW=0,naturalH=0,scale=1,minScale=1,ox=0,oy=0,dragging=false,lx=0,ly=0;
function frameSize(){const r=stage.getBoundingClientRect();return Math.min(r.width,r.height)*.84}
function apply(){img.style.transform="translate(-50%,-50%) translate("+ox+"px,"+oy+"px) scale("+scale+")"}
function clamp(){const fs=frameSize(),dw=naturalW*scale,dh=naturalH*scale;
const mx=Math.max(0,(dw-fs)/2),my=Math.max(0,(dh-fs)/2);
ox=Math.max(-mx,Math.min(mx,ox));oy=Math.max(-my,Math.min(my,oy))}
function setStatus(m,c){status.textContent=m||"";status.className="status"+(c?" "+c:"")}
file.onchange=()=>{const f=file.files&&file.files[0];if(!f)return;const u=URL.createObjectURL(f);
img.onload=()=>{URL.revokeObjectURL(u);naturalW=img.naturalWidth;naturalH=img.naturalHeight;
const fs=frameSize();minScale=Math.max(fs/naturalW,fs/naturalH);scale=minScale;ox=oy=0;
zoom.value="100";zoom.disabled=false;uploadBtn.disabled=false;stage.classList.remove("empty");
emptyHint.style.display="none";viewport.style.display="block";
img.style.width=naturalW+"px";img.style.height=naturalH+"px";apply();setStatus("Crop, then upload")};
img.onerror=()=>setStatus("Could not load image","err");img.src=u};
zoom.oninput=()=>{if(!naturalW)return;scale=minScale*(Number(zoom.value)/100);clamp();apply()};
stage.onpointerdown=e=>{if(!naturalW)return;stage.setPointerCapture(e.pointerId);dragging=true;lx=e.clientX;ly=e.clientY};
stage.onpointermove=e=>{if(!dragging)return;ox+=e.clientX-lx;oy+=e.clientY-ly;lx=e.clientX;ly=e.clientY;clamp();apply()};
stage.onpointerup=()=>{dragging=false};
uploadBtn.onclick=async()=>{if(!naturalW)return;uploadBtn.disabled=true;setStatus("Uploading…");
try{const out=480,fs=frameSize(),half=fs/2,c=document.createElement("canvas");c.width=c.height=out;
const ctx=c.getContext("2d"),sx=naturalW/2+(-ox-half)/scale,sy=naturalH/2+(-oy-half)/scale,sw=fs/scale;
ctx.fillStyle="#000";ctx.fillRect(0,0,out,out);ctx.drawImage(img,sx,sy,sw,sw,0,0,out,out);
const blob=await new Promise((res,rej)=>c.toBlob(b=>b?res(b):rej(new Error("encode")),"image/jpeg",.88));
const fd=new FormData();fd.append("file",blob,"wallpaper.jpg");
const r=await fetch("/upload",{method:"POST",body:fd});const t=await r.text();
if(!r.ok)throw new Error(t||("HTTP "+r.status));
setStatus("Saved on the desk. You can leave this Wi‑Fi.","ok")}
catch(e){setStatus(String(e.message||e),"err");uploadBtn.disabled=false}}})();
</script></body></html>
)HTML";

void free_baked() {
  if (g_pixels) {
    heap_caps_free(g_pixels);
    g_pixels = nullptr;
  }
  std::memset(&g_img, 0, sizeof(g_img));
}

/** Decode JPEG once into PSRAM RGB565. Streaming TJPGD every redraw freezes the UI. */
bool bake_wallpaper(size_t file_sz) {
  free_baked();

  File f = LittleFS.open(kFilePath, "r");
  if (!f) {
    Serial.println("wallpaper bake open FAIL");
    return false;
  }

  uint8_t * pool = (uint8_t *)heap_caps_malloc(kJpgPool, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!pool) pool = (uint8_t *)heap_caps_malloc(kJpgPool, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!pool) {
    f.close();
    Serial.println("wallpaper bake pool FAIL");
    return false;
  }

  JpgBake io{};
  io.file = &f;
  JDEC jd;
  std::memset(&jd, 0, sizeof(jd));
  JRESULT rc = jd_prepare(&jd, jpg_input, pool, kJpgPool, &io);
  if (rc != JDR_OK) {
    Serial.printf("wallpaper jd_prepare=%d size=%u\n", (int)rc, (unsigned)file_sz);
    heap_caps_free(pool);
    f.close();
    return false;
  }

  const uint32_t src_w = jd.width;
  const uint32_t src_h = jd.height;
  if (src_w == 0 || src_h == 0) {
    heap_caps_free(pool);
    f.close();
    return false;
  }

  const uint32_t dw = src_w > kMaxSide ? kMaxSide : src_w;
  const uint32_t dh = src_h > kMaxSide ? kMaxSide : src_h;
  const size_t nbytes = (size_t)dw * (size_t)dh * sizeof(uint16_t);
  uint16_t * rgb565 =
      (uint16_t *)heap_caps_malloc(nbytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!rgb565) {
    Serial.println("wallpaper bake PSRAM alloc FAIL");
    heap_caps_free(pool);
    f.close();
    return false;
  }
  std::memset(rgb565, 0, nbytes);
  io.dst = rgb565;
  io.dw = dw;
  io.dh = dh;

  rc = jd_decomp(&jd, jpg_output, 0);
  heap_caps_free(pool);
  f.close();
  if (rc != JDR_OK) {
    Serial.printf("wallpaper jd_decomp=%d\n", (int)rc);
    heap_caps_free(rgb565);
    return false;
  }

  g_pixels = rgb565;
  g_img.data = (const uint8_t *)rgb565;
  g_img.data_size = (uint32_t)nbytes;
  g_img.header.magic = LV_IMAGE_HEADER_MAGIC;
  g_img.header.cf = LV_COLOR_FORMAT_RGB565;
  g_img.header.flags = 0;
  g_img.header.w = dw;
  g_img.header.h = dh;
  g_img.header.stride = dw * (uint32_t)sizeof(uint16_t);
  g_img.reserved = nullptr;
  g_img.reserved_2 = nullptr;

  Serial.printf("wallpaper baked %ux%u (%u KB PSRAM) from %u B jpeg\n", (unsigned)dw,
                (unsigned)dh, (unsigned)(nbytes >> 10), (unsigned)file_sz);
  Serial.printf("heap after bake: internal free=%u KB  PSRAM free=%u KB\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));
  return true;
}

void refresh_has() {
  g_checked = true;
  g_has = false;
  if (!g_fs_ok) {
    free_baked();
    return;
  }
  if (!LittleFS.exists(kFilePath)) {
    free_baked();
    return;
  }
  File f = LittleFS.open(kFilePath, "r");
  if (!f || f.size() < 64) {
    if (f) f.close();
    free_baked();
    return;
  }
  const size_t sz = f.size();
  uint8_t magic[2] = {};
  (void)f.read(magic, 2);
  f.close();
  if (magic[0] != 0xFF || magic[1] != 0xD8) {
    Serial.printf("wallpaper.jpg bad JPEG magic %02X %02X size=%u\n", magic[0], magic[1],
                  (unsigned)sz);
    free_baked();
    return;
  }
  if (!bake_wallpaper(sz)) {
    free_baked();
    return;
  }
  g_has = true;
}

void send_redirect() {
  g_http.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  g_http.send(302, "text/plain", "");
}

void handle_root() {
  g_http.sendHeader("Cache-Control", "no-store");
  g_http.send_P(200, "text/html", kUploadHtml);
}

void handle_upload_finish() {
  if (g_upload) {
    g_upload.close();
    g_upload = File();
  }
  /* Do not bake while SoftAP/HTTP are live — that was OOMing and bouncing to Hub. */
  g_pending_finalize = true;
  g_http.send(200, "text/plain", "OK");
}

void handle_upload_file() {
  HTTPUpload & u = g_http.upload();
  if (u.status == UPLOAD_FILE_START) {
    if (g_upload) g_upload.close();
    LittleFS.remove(kFilePath);
    g_upload = LittleFS.open(kFilePath, "w");
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (g_upload) g_upload.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    if (g_upload) {
      g_upload.close();
      g_upload = File();
    }
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    if (g_upload) {
      g_upload.close();
      g_upload = File();
    }
    LittleFS.remove(kFilePath);
  }
}

bool ensure_fs() {
  if (g_fs_ok) return true;
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS begin FAILED");
    return false;
  }
  g_fs_ok = true;
  refresh_has();
  return true;
}

void build_wifi_qr() {
  std::snprintf(g_wifi_qr, sizeof(g_wifi_qr), "WIFI:T:WPA;S:%s;P:%s;H:false;;", kApSsid, kApPass);
}

bool start_job() {
  if (g_job) return true;
  if (!ensure_fs()) return false;

  build_wifi_qr();
  /* Reconfigure SoftAP (ESP-NOW may pause — accepted for this job). */
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(kApSsid, kApPass, kApChannel)) {
    Serial.println("softAP upload FAILED");
    return false;
  }
  delay(100);
  const IPAddress ip = WiFi.softAPIP();
  std::snprintf(g_http_url, sizeof(g_http_url), "http://%s/", ip.toString().c_str());

  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  g_dns.start(53, "*", ip);

  g_http.on("/", HTTP_GET, handle_root);
  g_http.on("/upload", HTTP_POST, handle_upload_finish, handle_upload_file);
  /* Captive portal probes */
  g_http.on("/generate_204", HTTP_GET, send_redirect);
  g_http.on("/gen_204", HTTP_GET, send_redirect);
  g_http.on("/hotspot-detect.html", HTTP_GET, handle_root);
  g_http.on("/library/test/success.html", HTTP_GET, handle_root);
  g_http.on("/ncsi.txt", HTTP_GET, []() { g_http.send(200, "text/plain", "Microsoft NCSI"); });
  g_http.on("/connecttest.txt", HTTP_GET, []() { g_http.send(200, "text/plain", "Microsoft Connect Test"); });
  g_http.on("/fwlink/", HTTP_GET, send_redirect);
  g_http.onNotFound([]() {
    if (g_http.method() == HTTP_GET) send_redirect();
    else g_http.send(404, "text/plain", "nope");
  });
  g_http.begin();

  g_job = true;
  g_pending_finalize = false;
  g_seeded_poll = false;
  Serial.printf("BG upload SoftAP %s pass=%s url=%s\n", kApSsid, kApPass, g_http_url);
  return true;
}

void stop_job() {
  if (!g_job) return;
  g_job = false; /* Clear first so upload_poll stops during teardown. */
  g_http.stop();
  g_dns.stop();
  if (g_upload) {
    g_upload.close();
    g_upload = File();
  }
  /* Restore open SoftAP used for ESP-NOW (channel 1). */
  net::restore_espnow_radio();
  Serial.println("BG upload SoftAP stopped");
}

}  // namespace

void sync_preset_from_desk() {
  const uint8_t p = app::desk().bg_preset;
  if (p < (uint8_t)Preset::Count) g_preset = static_cast<Preset>(p);
}

bool has() {
  sync_preset_from_desk();
  if (!g_checked) {
    ensure_fs();
    refresh_has();
  }
  return g_has;
}

const char * preset_name(Preset p) {
  switch (p) {
    case Preset::Theme: return "Theme";
    case Preset::Aurora: return "Aurora";
    case Preset::Sunset: return "Sunset";
    case Preset::Ocean: return "Ocean";
    case Preset::Ember: return "Ember";
    case Preset::Mist: return "Mist";
    default: return "?";
  }
}

void preset_colors(Preset p, uint32_t * top, uint32_t * bot) {
  uint32_t t = 0x1a1224, b = 0x0c0a0f;
  switch (p) {
    case Preset::Aurora: t = 0x123048; b = 0x061018; break;
    case Preset::Sunset: t = 0x3a1515; b = 0x100808; break;
    case Preset::Ocean: t = 0x104070; b = 0x061018; break;
    case Preset::Ember: t = 0x3a3010; b = 0x100c08; break;
    case Preset::Mist: t = 0x1a222e; b = 0x080a0e; break;
    default: break;
  }
  if (top) *top = t;
  if (bot) *bot = b;
}

Preset preset() { return g_preset; }

void set_preset(Preset p) {
  g_preset = p;
  app::desk().bg_preset = (uint8_t)p;
  app::save();
}

const void * lv_src() {
  if (!has()) return nullptr;
  return g_pixels ? &g_img : nullptr;
}

const char * file_path() {
  if (!has()) return nullptr;
  return kFilePath;
}

void reload() {
  g_checked = false;
  refresh_has();
}

void clear() {
  ensure_fs();
  if (lv_obj_t * scr = lv_screen_active()) {
    lv_obj_set_style_bg_image_src(scr, nullptr, 0);
  }
  LittleFS.remove(kFilePath);
  free_baked();
  g_has = false;
  g_checked = true;
}

bool upload_session(char * url, size_t url_n, char * local_url, size_t local_n, char * qr_lv_src,
                    size_t qr_n) {
  if (url && url_n) url[0] = 0;
  if (local_url && local_n) local_url[0] = 0;
  if (qr_lv_src && qr_n) qr_lv_src[0] = 0;
  if (!start_job()) return false;
  if (url && url_n) std::snprintf(url, url_n, "%s", g_http_url);
  /* Device draws WIFI QR via wifi_qr_payload() — no PNG path. */
  return true;
}

bool poll_new_upload() {
  if (!g_job) return false;
  if (!g_seeded_poll) {
    g_seeded_poll = true;
    g_pending_finalize = false;
    return false;
  }
  if (!g_pending_finalize) return false;
  g_pending_finalize = false;
  return true;
}

bool finalize_upload() {
  reload();
  return has();
}

void upload_poll() {
  if (!g_job) return;
  g_dns.processNextRequest();
  g_http.handleClient();
}

void end_upload_job() { stop_job(); }

const char * wifi_qr_payload() { return g_job && g_wifi_qr[0] ? g_wifi_qr : nullptr; }
const char * softap_ssid() { return g_job ? kApSsid : nullptr; }
const char * softap_pass() { return g_job ? kApPass : nullptr; }

}  // namespace background
}  // namespace wp
