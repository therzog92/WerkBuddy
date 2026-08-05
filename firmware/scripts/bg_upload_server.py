#!/usr/bin/env python3
"""
WerkPager background upload server (PC sim / Phase −1).

Each desk gets its own URL + token so office phones only hit that unit:
  http://<lan-ip>:8765/u/<desk_id>/<token>

Phone UI: pick photo → pan/zoom crop (1:1) → upload 480×480 JPEG.
Stores one image per desk under firmware/sim-data/bg/<desk_id>.jpg
and writes session metadata for the LVGL sim to show the QR/URL.
"""

from __future__ import annotations

import argparse
import json
import secrets
import socket
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote

try:
    import qrcode
    from PIL import Image
except ImportError as e:
    print("Need pillow + qrcode. Example:", file=sys.stderr)
    print(r'  "%USERPROFILE%\.pyenv\pyenv-win\versions\3.13.0\python.exe" -m pip install pillow qrcode', file=sys.stderr)
    raise SystemExit(1) from e

ROOT = Path(__file__).resolve().parents[1]  # firmware/
DATA = ROOT / "sim-data"
BG_DIR = DATA / "bg"
SESSION_PATH = DATA / "bg_upload_session.json"

# Match firmware / web desk ids
DEFAULT_DESKS = (
    ("mac-tommy", "Tommy"),
    ("mac-will", "Will"),
    ("mac-alex", "Alex"),
)

PORT_DEFAULT = 8765
MAX_UPLOAD = 4 * 1024 * 1024  # 4 MiB


def lan_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        if ip and not ip.startswith("127."):
            return ip
    except OSError:
        pass
    try:
        return socket.gethostbyname(socket.gethostname())
    except OSError:
        return "127.0.0.1"


UPLOAD_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no"/>
<title>WerkPager · Background</title>
<style>
  :root {
    --bg: #0c0a0f;
    --panel: #22182f;
    --ink: #f7f2ea;
    --muted: #b9a8c9;
    --gold: #f0c24b;
    --hot: #ff4fa3;
    --mint: #5dffc2;
    --border: #3d2a55;
  }
  * { box-sizing: border-box; }
  html, body {
    margin: 0; min-height: 100%;
    background: radial-gradient(800px 400px at 20% -10%, #3a1848 0%, transparent 55%), var(--bg);
    color: var(--ink);
    font-family: "DM Sans", "Segoe UI", system-ui, sans-serif;
  }
  main {
    max-width: 440px; margin: 0 auto; padding: 20px 16px 40px;
  }
  h1 {
    font-family: "Bebas Neue", "Arial Narrow", Impact, sans-serif;
    letter-spacing: 0.12em; font-weight: 400; font-size: 1.75rem;
    margin: 0 0 4px; color: var(--gold);
  }
  .sub { color: var(--muted); font-size: 0.95rem; margin: 0 0 20px; }
  .desk {
    display: inline-block; padding: 4px 10px; border-radius: 999px;
    background: var(--panel); border: 1px solid var(--border);
    color: var(--mint); font-size: 0.85rem; margin-bottom: 16px;
  }
  .stage {
    position: relative; width: 100%; aspect-ratio: 1;
    background: #160e1f; border-radius: 16px; overflow: hidden;
    border: 1px solid var(--border); touch-action: none;
    margin-bottom: 14px;
  }
  .stage.empty {
    display: grid; place-items: center; color: var(--muted); font-size: 0.95rem;
  }
  #viewport {
    position: absolute; inset: 0; overflow: hidden; display: none;
  }
  #img {
    position: absolute; left: 50%; top: 50%;
    transform-origin: center center;
    will-change: transform;
    user-select: none; pointer-events: none;
  }
  .frame {
    position: absolute; inset: 8%;
    border: 2px solid var(--gold); border-radius: 8px;
    box-shadow: 0 0 0 9999px rgba(0,0,0,0.55);
    pointer-events: none;
  }
  .hint { color: var(--muted); font-size: 0.8rem; margin: 0 0 12px; }
  label.file {
    display: block; width: 100%; text-align: center;
    padding: 14px; border-radius: 14px; cursor: pointer;
    background: linear-gradient(90deg, var(--hot), var(--gold));
    color: #1a1224; font-weight: 700; margin-bottom: 10px;
  }
  input[type=file] { display: none; }
  .row { display: flex; gap: 8px; align-items: center; margin-bottom: 12px; }
  .row label { color: var(--muted); font-size: 0.85rem; min-width: 48px; }
  input[type=range] { flex: 1; accent-color: var(--gold); }
  button.upload {
    width: 100%; padding: 14px; border: 0; border-radius: 14px;
    background: var(--gold); color: #1a1224; font-weight: 700;
    font-size: 1rem; cursor: pointer;
  }
  button.upload:disabled { opacity: 0.4; cursor: default; }
  .status { margin-top: 14px; min-height: 1.4em; font-size: 0.9rem; color: var(--muted); }
  .status.ok { color: var(--mint); }
  .status.err { color: #ff5c7a; }
</style>
</head>
<body>
<main>
  <h1>WERKPAGER</h1>
  <p class="sub">Set background for this desk only</p>
  <div class="desk" id="deskLabel">Desk</div>
  <div class="stage empty" id="stage">
    <span id="emptyHint">Choose a photo to crop</span>
    <div id="viewport">
      <img id="img" alt=""/>
      <div class="frame"></div>
    </div>
  </div>
  <p class="hint">Drag to pan · slider to zoom · square crop fills the 480×480 screen</p>
  <label class="file">
    Choose photo
    <input type="file" id="file" accept="image/*"/>
  </label>
  <div class="row">
    <label for="zoom">Zoom</label>
    <input type="range" id="zoom" min="100" max="400" value="100" disabled/>
  </div>
  <button class="upload" id="upload" disabled>Upload to desk</button>
  <p class="status" id="status"></p>
</main>
<script>
(() => {
  const deskId = __DESK_ID__;
  const token = __TOKEN__;
  const deskName = __DESK_NAME__;
  document.getElementById("deskLabel").textContent = deskName + "'s Desk · " + deskId;

  const file = document.getElementById("file");
  const img = document.getElementById("img");
  const stage = document.getElementById("stage");
  const viewport = document.getElementById("viewport");
  const emptyHint = document.getElementById("emptyHint");
  const zoom = document.getElementById("zoom");
  const uploadBtn = document.getElementById("upload");
  const status = document.getElementById("status");

  let naturalW = 0, naturalH = 0;
  let scale = 1, minScale = 1;
  let ox = 0, oy = 0; /* pan offset in CSS px from center */
  let dragging = false, lx = 0, ly = 0;

  function frameSize() {
    const r = stage.getBoundingClientRect();
    return Math.min(r.width, r.height) * 0.84;
  }

  function applyTransform() {
    img.style.transform =
      "translate(-50%, -50%) translate(" + ox + "px," + oy + "px) scale(" + scale + ")";
  }

  function clampPan() {
    const fs = frameSize();
    const dw = naturalW * scale;
    const dh = naturalH * scale;
    const maxX = Math.max(0, (dw - fs) / 2);
    const maxY = Math.max(0, (dh - fs) / 2);
    ox = Math.max(-maxX, Math.min(maxX, ox));
    oy = Math.max(-maxY, Math.min(maxY, oy));
  }

  function setStatus(msg, cls) {
    status.textContent = msg || "";
    status.className = "status" + (cls ? " " + cls : "");
  }

  file.addEventListener("change", () => {
    const f = file.files && file.files[0];
    if (!f) return;
    const url = URL.createObjectURL(f);
    img.onload = () => {
      URL.revokeObjectURL(url);
      naturalW = img.naturalWidth;
      naturalH = img.naturalHeight;
      const fs = frameSize();
      minScale = Math.max(fs / naturalW, fs / naturalH);
      scale = minScale;
      ox = 0; oy = 0;
      zoom.min = "100";
      zoom.max = String(Math.round((minScale * 4) / minScale * 100));
      zoom.value = "100";
      zoom.disabled = false;
      uploadBtn.disabled = false;
      stage.classList.remove("empty");
      emptyHint.style.display = "none";
      viewport.style.display = "block";
      img.style.width = naturalW + "px";
      img.style.height = naturalH + "px";
      applyTransform();
      setStatus("Crop, then upload");
    };
    img.onerror = () => setStatus("Could not load that image", "err");
    img.src = url;
  });

  zoom.addEventListener("input", () => {
    if (!naturalW) return;
    const pct = Number(zoom.value) / 100;
    scale = minScale * pct;
    clampPan();
    applyTransform();
  });

  function onDown(x, y) {
    if (!naturalW) return;
    dragging = true; lx = x; ly = y;
  }
  function onMove(x, y) {
    if (!dragging) return;
    ox += x - lx; oy += y - ly;
    lx = x; ly = y;
    clampPan();
    applyTransform();
  }
  function onUp() { dragging = false; }

  stage.addEventListener("pointerdown", (e) => {
    stage.setPointerCapture(e.pointerId);
    onDown(e.clientX, e.clientY);
  });
  stage.addEventListener("pointermove", (e) => onMove(e.clientX, e.clientY));
  stage.addEventListener("pointerup", onUp);
  stage.addEventListener("pointercancel", onUp);

  uploadBtn.addEventListener("click", async () => {
    if (!naturalW) return;
    uploadBtn.disabled = true;
    setStatus("Uploading…");
    try {
      const out = 480;
      const fs = frameSize();
      const canvas = document.createElement("canvas");
      canvas.width = out; canvas.height = out;
      const ctx = canvas.getContext("2d");
      /* Map crop frame (centered) through pan/scale into image pixels */
      const half = fs / 2;
      const sx = (naturalW / 2) + (-ox - half) / scale;
      const sy = (naturalH / 2) + (-oy - half) / scale;
      const sw = fs / scale;
      ctx.fillStyle = "#000";
      ctx.fillRect(0, 0, out, out);
      ctx.drawImage(img, sx, sy, sw, sw, 0, 0, out, out);
      const blob = await new Promise((res, rej) =>
        canvas.toBlob((b) => (b ? res(b) : rej(new Error("encode"))), "image/jpeg", 0.88)
      );
      const resp = await fetch("/u/" + encodeURIComponent(deskId) + "/" + encodeURIComponent(token) + "/upload", {
        method: "POST",
        headers: { "Content-Type": "image/jpeg" },
        body: blob,
      });
      const text = await resp.text();
      if (!resp.ok) throw new Error(text || ("HTTP " + resp.status));
      setStatus("Saved on " + deskName + "'s desk. You can close this page.", "ok");
    } catch (err) {
      setStatus(String(err.message || err), "err");
      uploadBtn.disabled = false;
    }
  });
})();
</script>
</body>
</html>
"""


def ensure_dirs() -> None:
    BG_DIR.mkdir(parents=True, exist_ok=True)


def write_qr(path: Path, url: str) -> None:
    qr = qrcode.QRCode(version=None, box_size=6, border=2)
    qr.add_data(url)
    qr.make(fit=True)
    img = qr.make_image(fill_color="black", back_color="white")
    img.save(path)


def build_session(port: int, host_ip: str) -> dict:
    desks = {}
    for desk_id, name in DEFAULT_DESKS:
        token = secrets.token_urlsafe(8)
        path = f"/u/{desk_id}/{token}"
        url = f"http://{host_ip}:{port}{path}"
        local_url = f"http://127.0.0.1:{port}{path}"
        qr_path = BG_DIR / f"qr-{desk_id}.png"
        write_qr(qr_path, url)
        desks[desk_id] = {
            "name": name,
            "token": token,
            "url": url,
            "local_url": local_url,
            "qr": str(qr_path.as_posix()),
            "image": str((BG_DIR / f"{desk_id}.jpg").as_posix()),
        }
    return {
        "port": port,
        "host": host_ip,
        "desks": desks,
    }


def write_session(session: dict) -> None:
    ensure_dirs()
    SESSION_PATH.write_text(json.dumps(session, indent=2), encoding="utf-8")


def page_for(desk_id: str, token: str, session: dict) -> str | None:
    info = session["desks"].get(desk_id)
    if not info or info["token"] != token:
        return None
    html = UPLOAD_HTML
    html = html.replace("__DESK_ID__", json.dumps(desk_id))
    html = html.replace("__TOKEN__", json.dumps(token))
    html = html.replace("__DESK_NAME__", json.dumps(info["name"]))
    return html


class Handler(BaseHTTPRequestHandler):
    session: dict = {}

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("[%s] %s\n" % (self.log_date_time_string(), fmt % args))

    def _send(self, code: int, body: bytes, content_type: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self) -> None:
        path = unquote(self.path.split("?", 1)[0])
        if path in ("/", "/health"):
            body = b"WerkPager background upload OK\n"
            self._send(200, body, "text/plain; charset=utf-8")
            return
        if path == "/session.json":
            body = json.dumps(self.session, indent=2).encode("utf-8")
            self._send(200, body, "application/json; charset=utf-8")
            return
        parts = path.strip("/").split("/")
        if len(parts) == 2 and parts[0] == "bg":
            # /bg/<desk_id>.png|jpg — current wallpaper for web sim / debug
            name = parts[1]
            desk_id = name.rsplit(".", 1)[0] if "." in name else name
            if desk_id not in self.session["desks"]:
                self._send(404, b"Unknown desk\n", "text/plain; charset=utf-8")
                return
            for ext, ctype in (("png", "image/png"), ("jpg", "image/jpeg")):
                fp = BG_DIR / f"{desk_id}.{ext}"
                if fp.exists():
                    data = fp.read_bytes()
                    self._send(200, data, ctype)
                    return
            self._send(404, b"No background yet\n", "text/plain; charset=utf-8")
            return
        if len(parts) == 3 and parts[0] == "u":
            desk_id, token = parts[1], parts[2]
            html = page_for(desk_id, token, self.session)
            if html is None:
                self._send(403, b"Wrong desk or expired link\n", "text/plain; charset=utf-8")
                return
            self._send(200, html.encode("utf-8"), "text/html; charset=utf-8")
            return
        self._send(404, b"Not found\n", "text/plain; charset=utf-8")

    def do_POST(self) -> None:
        path = unquote(self.path.split("?", 1)[0])
        parts = path.strip("/").split("/")
        if not (len(parts) == 4 and parts[0] == "u" and parts[3] == "upload"):
            self._send(404, b"Not found\n", "text/plain; charset=utf-8")
            return
        desk_id, token = parts[1], parts[2]
        info = self.session["desks"].get(desk_id)
        if not info or info["token"] != token:
            self._send(403, b"Wrong desk or expired link\n", "text/plain; charset=utf-8")
            return
        length = int(self.headers.get("Content-Length", "0") or 0)
        if length <= 0 or length > MAX_UPLOAD:
            self._send(400, b"Bad size\n", "text/plain; charset=utf-8")
            return
        data = self.rfile.read(length)
        try:
            from io import BytesIO

            im = Image.open(BytesIO(data))
            im = im.convert("RGB")
            if im.size != (480, 480):
                im = im.resize((480, 480), Image.Resampling.LANCZOS)
            ensure_dirs()
            out = BG_DIR / f"{desk_id}.jpg"
            im.save(out, format="JPEG", quality=88, optimize=True)
            # RGBA PNG for LVGL LODEPNG — also mirror into assets/ (relative S: path).
            png_out = BG_DIR / f"{desk_id}.png"
            rgba = im.convert("RGBA")
            rgba.save(png_out, format="PNG", optimize=True)
            assets_bg = ROOT / "assets" / "bg"
            assets_bg.mkdir(parents=True, exist_ok=True)
            rgba.save(assets_bg / f"{desk_id}.png", format="PNG", optimize=True)
            build_bg = ROOT / "build" / "assets" / "bg"
            if (ROOT / "build").exists():
                build_bg.mkdir(parents=True, exist_ok=True)
                rgba.save(build_bg / f"{desk_id}.png", format="PNG", optimize=True)
            stamp = BG_DIR / f"{desk_id}.stamp"
            stamp.write_text(str(out.stat().st_mtime_ns), encoding="utf-8")
        except Exception as exc:  # noqa: BLE001
            self._send(400, ("Bad image: %s\n" % exc).encode("utf-8"), "text/plain; charset=utf-8")
            return
        self._send(200, b"ok\n", "text/plain; charset=utf-8")
        print("Saved background for %s → %s" % (desk_id, out), flush=True)


def main() -> int:
    ap = argparse.ArgumentParser(description="WerkPager background upload server")
    ap.add_argument("--port", type=int, default=PORT_DEFAULT)
    ap.add_argument("--desk", default="mac-tommy", help="Desk id to open in browser")
    ap.add_argument("--no-browser", action="store_true")
    ap.add_argument("--bind", default="0.0.0.0")
    args = ap.parse_args()

    ensure_dirs()
    host_ip = lan_ip()
    session = build_session(args.port, host_ip)
    write_session(session)
    Handler.session = session

    httpd = ThreadingHTTPServer((args.bind, args.port), Handler)
    info = session["desks"].get(args.desk) or next(iter(session["desks"].values()))
    print("WerkPager background upload", flush=True)
    print("  LAN:   %s" % info["url"], flush=True)
    print("  Local: %s" % info["local_url"], flush=True)
    print("  Session file: %s" % SESSION_PATH, flush=True)
    for did, d in session["desks"].items():
        print("  · %s (%s): %s" % (d["name"], did, d["url"]), flush=True)

    if not args.no_browser:
        webbrowser.open(info["local_url"])

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
