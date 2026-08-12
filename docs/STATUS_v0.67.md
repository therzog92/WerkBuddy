# Status — leave-off point for v0.67 (2026-08-11)

**Read this first** when picking the project back up after a long break. Product brand **WerkBuddy**; pager app tile **WerkPager**. Boards: two Guition **ESP32-4848S040** (Tommy typically **COM5**, Will **COM6**).

Repo: https://github.com/therzog92/WerkBuddy — release tag **`v0.67`**.

---

## Where we are

The **full LVGL desk app** runs on glass (shared UI from `firmware/src` + device glue in `firmware/device`). This is **past** Phase 0 bring-up: shell, pager, games, doodle, SoftAP wallpaper, Wi‑Fi Sync time, peer clock sync, orientation flip, and OSK perf are in.

| Area | State on glass (v0.67) |
|------|-------------------------|
| Hub / Settings / Idle / Setup | Working |
| WerkPager call / ack / clear | Working |
| Multiplayer games + Active Games (cap 24) | Working |
| Doodle (chunked ESP-NOW) | Working |
| SoftAP wallpaper upload → LittleFS `/wallpaper.jpg` | Working (baked RGB565 in PSRAM) |
| Wi‑Fi Sync time (ephemeral STA + SNTP, Central TZ) | Working |
| Peer `TimeSync` (newest sync_gen wins) + NVS wall restore | Working |
| Screen **Flip 180** (Settings) | Working |
| Custom message OSK (blank unless editing custom; partial redraw) | Working |
| Emoji on device | **Curated baked RGB565A8** in flash (`emoji_assets_gen.cpp`) — not full Twemoji FS |
| Device drive USB `shot`/`tap` | **Code present, compiled OFF** (`WERKPAGER_DEVICE_DRIVE=0`) |
| OTA Updates on device | **Stub** — toast only; sim lists GitHub Releases |
| Sound / battery % / DS3231 RTC | Not started |
| Memory face art | **Baked RGB565 in flash** (`bake_memory_assets.py` → `memory_assets_gen.cpp`) — no LittleFS decode |

---

## Exact next work (when tokens return)

1. **Flash both desks** with `v0.67` if they still have an older build (`pio run -t upload --upload-port COM5` / `COM6` from `firmware/device`).
2. **Device OTA** — Settings → Updates should download GitHub Release `.bin` and flash (port plan §8b). Sim already lists releases.
3. Optional: sound; retail checklist soak.
4. Re-enable **device drive** only when an agent needs glass QA (see below).

Do **not** revive the archived web sim. UX source of truth remains `firmware/` LVGL (PC sim + device).

---

## Flash / build (device)

```powershell
cd firmware/device
pio run -t upload --upload-port COM5
pio run -t upload --upload-port COM6
# Optional LittleFS (wallpaper/emoji data partition) — only if you changed data/:
# pio run -t uploadfs --upload-port COMx
```

- Partition table: `firmware/device/partitions.csv` (app + LittleFS).
- Shared UI is pulled in via `scripts/pio_shared_src.py` from `firmware/src`.
- Serial: 921600 when drive was on; 115200 when drive off (`main.cpp`).

### Re-enable device drive (agent glass QA)

In `firmware/device/platformio.ini`, set:

```ini
-DWERKPAGER_DEVICE_DRIVE=1
```

Rebuild/flash. Host tools: `firmware/scripts/drive-device.ps1` / `drive-device.py` (`shot` / `tap` / `swipe`). Off = header stubs only — **no PSRAM shot buffer, no serial command parser cost**. Details: `docs/ESP32_PORT_PLAN.md` §8c.

---

## Hardware notes (learned on these boards)

- **BAT** white 2P, right side near Guition silk: **B− top / B+ bottom**. **B+ = red**, **B− = black**. Single-cell LiPo only — never 5 V on BAT.
- LiPo keeps the desk powered; it is **not** an RTC backup. After full power cut, NVS restores last wall time (frozen); peer **TimeSync** or Wi‑Fi Sync time corrects it.
- US Central TZ: `CST6CDT,M3.2.0,M11.1.0` (boot `setenv` + `configTzTime` on sync).
- Wi‑Fi password OSK shows **plaintext** (no asterisks) by design for desk use.

---

## Protocol / clock

- New message: **`TimeSync`** (`protocol/messages.js` + `messages.h`). Wire: type+ver+mac+`unix_sec`+`sync_gen` (broadcast).
- Desk fields: `wall_epoch`, `clock_sync_gen`, `rotate_180`, `wifi_pass` persisted in NVS (`storage_app_nvs.cpp`).

---

## Key paths

| Path | Role |
|------|------|
| `firmware/src/` | Shared LVGL app (sim + device) |
| `firmware/device/` | PlatformIO Guition project |
| `firmware/device/src/main.cpp` | Glass, flush, touch, PARTIAL/FULL switch hook |
| `firmware/device/src/wifi_jobs.cpp` | Scan + Sync time |
| `firmware/device/src/msg_codec.cpp` | Binary ESP-NOW |
| `firmware/device/src/emoji_assets_gen.cpp` | Baked emoji (regen: `scripts/bake_emoji_assets.py`) |
| `firmware/src/ui/display_perf.*` | PARTIAL default; FULL for call/alarm wash |
| `firmware/src/ui/orient.*` | Flip 180 |
| `AGENTS.md` | Agent rules |
| `docs/ESP32_PORT_PLAN.md` | Architecture |
| `docs/HARDWARE_BRINGUP_MANUAL.md` | Session ladder |

---

## Suggested operator prompt after a break

> Read `docs/STATUS_v0.67.md` and `AGENTS.md`. We're on WerkBuddy **v0.67** on Guition 4848S040 (COM5/COM6). Continue from **device OTA** unless I say otherwise.

Chat transcript for this bring-up arc (if still on disk):  
`C:\Users\Tommy\.cursor\projects\c-Users-Tommy-Projects-WerkPager\agent-transcripts\8733d9a2-50d5-4c7c-b3bb-53962a027c77\`
