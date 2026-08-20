/*
 * Phone firmware upload: SoftAP + captive portal + multipart .bin POST streamed
 * directly into the OTA slot (Update). Reuses the photo-upload SoftAP pattern.
 */

#include "app/fw_update.h"

#include "device_net.h"

#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>

namespace wp {
namespace fw_update {
namespace {

constexpr const char * kApSsid = "WerkBuddy-Update";
constexpr const char * kApPass = "werkbuddy";
constexpr uint8_t kApChannel = 1;
constexpr size_t kMaxFw = 0x600000 - 0x10000; /* OTA slot (6MB) minus margin */

DNSServer g_dns;
WebServer g_http(80);

bool g_running = false;
char g_url[32] = "http://192.168.4.1/";
char g_err[96] = {};
bool g_done = false;   /* only reachable on failure — success reboots */
bool g_writing = false;
size_t g_written = 0;

void set_err(const char * msg) {
  std::snprintf(g_err, sizeof(g_err), "%s", msg ? msg : "error");
}

/* Compact phone page — pick a .bin, upload to the OTA slot. */
const char kUploadHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no"/>
<title>WerkBuddy Update</title>
<style>
body{margin:0;background:#0c0a0f;color:#f7f2ea;font-family:system-ui,sans-serif}
main{max-width:440px;margin:0 auto;padding:16px}
h1{color:#f0c24b;letter-spacing:.08em;font-size:1.4rem;margin:0 0 8px}
.sub{color:#b9a8c9;font-size:.9rem;margin:0 0 14px;line-height:1.35}
.card{background:#160e1f;border:1px solid #3d2a55;border-radius:14px;padding:16px;margin-bottom:12px}
label.file{display:block;text-align:center;padding:14px;border:0;border-radius:14px;
font-weight:700;margin-bottom:10px;background:linear-gradient(90deg,#ff4fa3,#f0c24b);color:#1a1224}
input[type=file]{display:none}
.fname{color:#b9a8c9;font-size:.85rem;word-break:break-all;min-height:1.2em;margin-bottom:10px}
button.upload{display:block;width:100%;padding:14px;border:0;border-radius:14px;
background:#f0c24b;color:#1a1224;font-weight:700}
button.upload:disabled{opacity:.4}
.status{min-height:1.3em;color:#b9a8c9;font-size:.9rem;margin-top:10px}
.status.ok{color:#5dffc2}.status.err{color:#ff5c7a}
</style></head><body><main>
<h1>WERKBUDDY</h1>
<p class="sub">Download the release <b>.bin</b> on your phone, then choose it below to update this desk.</p>
<div class="card">
<label class="file">Choose .bin<input type="file" id="file" accept=".bin,application/octet-stream"/></label>
<div class="fname" id="fname">No file chosen</div>
<button class="upload" id="upload" disabled>Upload & update</button>
<p class="status" id="status"></p>
</div>
</main>
<script>
(()=>{
const file=document.getElementById("file"),fname=document.getElementById("fname"),
uploadBtn=document.getElementById("upload"),status=document.getElementById("status");
let f=null;
file.onchange=()=>{f=file.files&&file.files[0];if(!f){return}
fname.textContent=f.name+" ("+(f.size/1048576).toFixed(2)+" MB)";uploadBtn.disabled=false;
status.textContent="";status.className="status"};
uploadBtn.onclick=async()=>{if(!f)return;uploadBtn.disabled=true;
status.textContent="Uploading…";status.className="status";
try{const fd=new FormData();fd.append("fw",f,"firmware.bin");
const r=await fetch("/fw",{method:"POST",body:fd});
if(r.ok){status.textContent="Update OK — desk is rebooting. You can leave this Wi-Fi.";status.className="status ok"}
else{const t=await r.text();throw new Error(t||("HTTP "+r.status))}}
catch(e){status.textContent=String(e.message||e);status.className="status err";uploadBtn.disabled=false}}})();
</script></body></html>
)HTML";

void send_redirect() {
  g_http.sendHeader("Location", String("http://192.168.4.1/"), true);
  g_http.send(302, "text/plain", "");
}

void on_setup_error() {
  set_err("flash write failed");
  if (g_writing) {
    Update.abort();
    g_writing = false;
  }
  g_done = true;
  g_http.send(500, "text/plain", "flash write failed");
}

void on_upload() {
  HTTPUpload & u = g_http.upload();
  if (u.status == UPLOAD_FILE_START) {
    g_writing = false;
    g_written = 0;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      set_err("Update.begin failed");
      g_done = true;
      g_http.send(500, "text/plain", "Update.begin failed");
      return;
    }
    g_writing = true;
    return;
  }

  if (u.status == UPLOAD_FILE_WRITE) {
    if (!g_writing) return;
    g_written += u.currentSize;
    if (g_written > kMaxFw) {
      set_err("firmware too large");
      Update.abort();
      g_writing = false;
      g_done = true;
      g_http.send(413, "text/plain", "firmware too large");
      return;
    }
    if (Update.write(u.buf, u.currentSize) != u.currentSize) {
      on_setup_error();
      return;
    }
    return;
  }

  if (u.status == UPLOAD_FILE_END) {
    if (!g_writing) return;
    g_writing = false;
    const size_t written = g_written;
    if (!Update.end(true)) {
      set_err("Update.end failed");
      g_done = true;
      g_http.send(500, "text/plain", "Update.end failed");
      return;
    }
    Serial.printf("FW upload %u bytes — rebooting\n", (unsigned)written);
    g_http.send(200, "text/plain", "OK");
    delay(200);
    ESP.restart();
    return;
  }
}

void on_upload_done() {
  /* Success path reboots in on_upload(END); reaching here without reboot means
   * a failure already responded. If Update is still mid-stream, abort cleanly. */
  if (g_writing) {
    Update.abort();
    g_writing = false;
  }
}

bool start_job() {
  if (g_running) return true;

  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(kApSsid, kApPass, kApChannel)) {
    Serial.println("FW upload softAP FAILED");
    return false;
  }
  delay(100);
  const IPAddress ip = WiFi.softAPIP();
  std::snprintf(g_url, sizeof(g_url), "http://%s/", ip.toString().c_str());

  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  g_dns.start(53, "*", ip);

  g_http.on("/", HTTP_GET, []() { g_http.send(200, "text/html", kUploadHtml); });
  g_http.on("/fw", HTTP_POST, on_upload_done, on_upload);
  /* Captive portal probes */
  g_http.on("/generate_204", HTTP_GET, send_redirect);
  g_http.on("/gen_204", HTTP_GET, send_redirect);
  g_http.on("/hotspot-detect.html", HTTP_GET,
            []() { g_http.send(200, "text/html", kUploadHtml); });
  g_http.on("/ncsi.txt", HTTP_GET, []() { g_http.send(200, "text/plain", "Microsoft NCSI"); });
  g_http.on("/connecttest.txt", HTTP_GET,
            []() { g_http.send(200, "text/plain", "Microsoft Connect Test"); });
  g_http.on("/fwlink/", HTTP_GET, send_redirect);
  g_http.onNotFound([]() {
    if (g_http.method() == HTTP_GET) send_redirect();
    else g_http.send(404, "text/plain", "nope");
  });
  g_http.begin();

  g_done = false;
  g_err[0] = '\0';
  g_running = true;
  Serial.printf("FW upload SoftAP %s pass=%s url=%s\n", kApSsid, kApPass, g_url);
  return true;
}

}  // namespace

bool start() { return start_job(); }

void poll() {
  if (!g_running) return;
  g_dns.processNextRequest();
  g_http.handleClient();
}

void stop() {
  if (!g_running) return;
  g_running = false;
  g_http.stop();
  g_dns.stop();
  net::restore_espnow_radio();
  Serial.println("FW upload SoftAP stopped");
}

bool finished(char * err, size_t err_n) {
  if (!g_done) return false;
  if (err && err_n) std::snprintf(err, err_n, "%s", g_err[0] ? g_err : "update failed");
  return true;
}

const char * ssid() { return g_running ? kApSsid : nullptr; }
const char * pass() { return g_running ? kApPass : nullptr; }
const char * url() { return g_running ? g_url : nullptr; }

}  // namespace fw_update
}  // namespace wp