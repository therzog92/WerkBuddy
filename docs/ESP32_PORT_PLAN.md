# WerkBuddy → ESP32 Port Plan

**Status:** Web simulator complete. **Phase −1 complete enough** — full LVGL PC (SDL) app under `firmware/` (hub, pager, settings/OSK, all games, doodle, idle). Hardware on order.  
**Primary target:** GUITION **ESP32-4848S040C_I** (ESP32-S3 N16R8, 4″ 480×480 capacitive ST7701 + GT911), ~3 desks.  
**Link:** **ESP-NOW** desk-to-desk (no Wi‑Fi / no cloud required for core features).

This document is the architecture + phase source of truth.  
**When boards arrive:** also follow `docs/HARDWARE_BRINGUP_MANUAL.md` and `AGENTS.md` — session-by-session unbox → glass → ping → shell (one session at a time).

---

## Phase −1 — LVGL on PC (done enough)

**Goal:** Develop real LVGL UI before boards arrive. Operator runs a desktop window; agents edit C++.

| Item | Choice |
|------|--------|
| Toolkit | **LVGL 9.3** |
| PC backend | **SDL2** (`LV_USE_SDL`) |
| Build | **CMake + Ninja + MinGW** (`C:\msys64\mingw64`) |
| Launch | `powershell -File firmware/scripts/run-sim.ps1` |
| Preview | F12 or `-Shot` → `firmware/sim-out/preview.png` |
| ESP32 env | Deferred until Phase 0 (BSP / Arduino vs IDF) |

**Keep the LVGL PC sim (`firmware/`) as UX/behavior bible.** It already has hub → settings → pager → games → doodle → utilities; on device, bring glass up (Phase 0) before re-wiring transport. (Former `web/` HTML sim archived at `../WerkBuddy-web-sim-archive/`.)

---

## 1. What already exists (do not reinvent)

| Layer | Location | Role |
|--------|----------|------|
| Message catalog | `protocol/messages.js` | All `MessageType`s + JSON helpers (sim transport) |
| Hub / WerkPager / settings / OSK / idle | `firmware/src/ui/scr_*.cpp` | UX + state machine |
| C4 + Battleship + Checkers + Memory | `firmware/src/ui/scr_games.cpp` | Game rules + UI flows |
| Doodle | `firmware/src/ui/scr_doodle.cpp` | Stroke sync |
| Memory art | `firmware/assets/memory/*` | Pair faces + card back |

**Treat the LVGL PC sim as the product spec.** Device firmware should match those screens, flows, and payloads—not invent parallel UX.

### Product surface (must ship)

1. **Home hub** — WerkPager, Games folder, Doodle, Settings  
2. **WerkPager** — peers, emoji + canned ping, Shantay / Sashay incoming  
3. **Games** — Tic Tac Toe, Connect Four, Battleship, Checkers, Memory  
4. **Doodle** — peer pick, colors, eraser, S/M/L, stroke sync, clear  
5. **Settings** — name, theme, timeout, idle mode, **date & time**, emojis, canned, scan peers  
6. **Idle** — black or clock (uses manually set date/time; no NTP assumed)

### Soft constraints (already designed into the sim)

- Display **480×480**, touch-first (no hover-dependent UX)  
- ESP-NOW ~**250 byte** practical payload → chunk doodle strokes; prefer binary on device  
- Forfeit always confirms: *“Are you sure you want to forfeit?”*  
- Games do **not** background-resume yet (mid-game exit = forfeit path only)  
- Checkers: each player’s **own pieces at bottom** (view transform only; shared logical coords on wire)  
- Memory: shared **seed** at invite; flip `{cardA, cardB}`; art from flash assets  

---

## 2. Hardware assumptions (verify on unbox)

Confirm against Guition docs / pinout for the exact 4848S040 revision:

| Subsystem | Typical on 4848S040 | Notes |
|-----------|---------------------|--------|
| MCU | ESP32-S3 | PSRAM preferred for LVGL framebuffers |
| Panel | 480×480 RGB / ST7701-class | Match vendor BSP |
| Touch | Capacitive (often GT911) | Calibrate once; store in NVS if needed |
| Storage | Flash + optional SD | Bundle Memory JPEGs/PNGs in LittleFS/SPIFFS or embed |
| Radio | Wi‑Fi radio used for ESP-NOW | SoftAP/STA not required for v1 |

**Bring-up day-1 checklist**

1. Flash vendor demo → prove LCD + touch  
2. Note SDK: Arduino + TFT_eSPI/LVGL vs ESP-IDF + BSP  
3. Measure free heap / PSRAM with empty LVGL screen  
4. Confirm ESP-NOW works between two boards at desk distance  

---

## 3. Recommended firmware architecture

```
┌─────────────────────────────────────────────┐
│  UI (LVGL) — screens in firmware/src/ui/    │
│  hub / werk / games / doodle / settings     │
└──────────────────┬──────────────────────────┘
                   │ events (tap, timer)
┌──────────────────▼──────────────────────────┐
│  App state — one “desk” (like web desks.*)  │
│  peers, settings, active game, idle timer   │
└──────────────────┬──────────────────────────┘
                   │
┌──────────────────▼──────────────────────────┐
│  Game engines (pure C/C++)                  │
│  same rules as board-games.js / more-games  │
└──────────────────┬──────────────────────────┘
                   │
┌──────────────────▼──────────────────────────┐
│  Protocol codec (binary pack/unpack)        │
│  mirrors protocol/messages.js types         │
└──────────────────┬──────────────────────────┘
                   │
┌──────────────────▼──────────────────────────┐
│  Transport: ESP-NOW                         │
│  broadcast discover / unicast game+doodle   │
└─────────────────────────────────────────────┘
```

### Suggested repo layout (add when starting firmware)

```
WerkPager/
  protocol/           # keep JS sim; add firmware/protocol/ (C headers) or codegen later
  firmware/           # LVGL PC sim (active UX bible) → ESP32 later
  protocol/           # message type catalog
  # web/ archived → ../WerkBuddy-web-sim-archive/
  firmware/           # NEW
    platformio.ini or CMakeLists.txt
    src/
      main.cpp
      ui/             # LVGL screens
      net/            # espnow_transport.*
      protocol/       # pack.h / unpack.cpp
      games/          # ttt, c4, bs, ck, mem, doodle
      storage/        # NVS settings, LittleFS assets
    data/             # LittleFS: memory/*.jpg, fonts
  docs/
    ESP32_PORT_PLAN.md  # this file
```

**UI toolkit:** **LVGL** (best fit for 480×480 + touch). Port visually from CSS themes (Eleganza / Runway / Ice / Lemon / Matcha) as LVGL styles / color tokens—not pixel-perfect CSS.

**Framework pick (decide on bring-up):**

1. Prefer **PlatformIO + Arduino** if Guition’s examples are Arduino/LVGL (faster).  
2. Prefer **ESP-IDF** if vendor BSP is IDF-only or we need tighter control.  
Document the choice in this file once made.

---

## 4. Protocol port (critical path)

### 4.1 Keep type parity

Every `MessageType` in `protocol/messages.js` needs a firmware enum + pack/unpack. Do **not** silently rename types.

### 4.2 Move from JSON → compact binary

Sim uses JSON for convenience. On device:

| Approach | When |
|----------|------|
| **v1 binary structs** | Preferred: fixed header `{type u8, ver u8, …}` + payload |
| JSON | Dev-only / debug builds if needed |

**Header sketch (v1):**

```
u8  type
u8  ver = 1
u8  flags
u8  reserved
u8  fromMac[6]
u8  toMac[6]     // 00..00 = broadcast where applicable
u16 nameLen? or fixed 12-byte UTF-8 name field
```

Names: keep **max 12** chars (sim already clamps). Emoji: store as UTF-8 short string or index into the device emoji table (prefer **index** for CALL to save bytes).

### 4.3 Payload budgets (ESP-NOW)

| Message | Sim fields | Firmware pack target |
|---------|------------|----------------------|
| discover / reply | id, name | mac + name |
| call | emoji, message | emojiIdx + cannedIdx **or** short string ≤22 |
| ttt_move | cell, mark | 2 bytes |
| c4_drop | col, color | 2 bytes |
| ck_move | fromX,Y toX,Y | 4 bytes |
| mem_invite | seed | seed string ≤24 or u32 hash + salt |
| mem_flip | cardA, cardB | 2 bytes |
| bs_fire / result | x,y + flags | ≤6 bytes |
| doodle_stroke | strokeId, seq, last, color, w, pts[] | chunk ≤200B pts |
| doodle_clear | — | header only |

**Doodle:** Keep sim contract: quantized pts `0–120`, `w` = 1|2|3, `color` = palette or `-1` erase, chunk with `strokeId/seq/last`.

### 4.4 Addressing

- **Sim:** `mac-tommy` style ids  
- **Device:** Wi‑Fi MAC as peer id; display name from NVS  
- Discover: ESP-NOW broadcast; reply unicast/broadcast per ESP-NOW peer list policy  
- Persist known peers (MAC + name) in NVS (“Nearby / saved”)

---

## 5. Phased delivery plan

### Phase 0 — Board bring-up (day boards arrive)

**Goal:** Hello world on real glass.

- [ ] Flash vendor LCD + touch demo  
- [ ] Create `firmware/` project with working LVGL blank + one button  
- [ ] Confirm touch coordinates map to 480×480 (Y-flip if needed)  
- [ ] Two-board ESP-NOW ping (`HELLO` / `ACK`) with RSSI log  
- [ ] Record chosen SDK + LVGL version in this doc  

**Exit:** Both boards show a tappable “WERKBUDDY” label; ping toast on peer.

### Phase 1 — Shell app (no games)

**Goal:** Hub + Settings + idle + peer discovery.

- [ ] Screens: Hub, Settings, OSK (QWERTY + symbols + CAPS/space/⌫), Idle  
- [ ] NVS: name, theme id, timeout, idle mode, clockOffsetMs (date+time)  
- [ ] Discover / discover_reply over ESP-NOW  
- [ ] Peer list UI  

**Exit:** Set name/date/time/theme on A; B sees A after scan.

### Phase 2 — WerkPager calls

**Goal:** Desk pager works IRL.

- [ ] Compose: emoji row + canned  
- [ ] Outgoing / Incoming (Shantay / Sashay)  
- [ ] call / ack / clear messages  
- [ ] Idle does not sleep over active call  

**Exit:** A pings B; B shantays; A gets toast.

### Phase 3 — Protocol library + one game (TTT)

**Goal:** Prove game invite/accept/move/forfeit loop.

- [ ] Binary codec for discover + call + TTT_*  
- [ ] TTT UI + engine  
- [ ] Forfeit confirm modal  
- [ ] Brand subtitle `vs Name`  

**Exit:** Full TTT match on two desks.

### Phase 4 — Remaining board games

Order by dependency / complexity:

1. **Connect Four** (colors, drop anim optional/simplified)  
2. **Checkers** (4-byte moves, view flip per side, multi-jump)  
3. **Memory** (seed sync, LittleFS images, touch flip)  
4. **Battleship** (setup touch place, offense/defense tabs, auto offense on turn)  

For each: port rules from JS first (unit-testable), then LVGL UI.

### Phase 5 — Doodle

- [ ] Canvas or LVGL draw layer  
- [ ] Local stroke + eraser + sizes  
- [ ] ESP-NOW chunked strokes + clear  
- [ ] Peer in brand `vs Name`  

### Phase 6 — Polish

- [ ] Themes parity  
- [ ] Sound? (optional buzzer / DAC—only if hardware supports)  
- [ ] Long-press / double-tap edge cases  
- [ ] Memory asset size budget (compress JPEGs aggressively)  
- [ ] **Optional Wi‑Fi:** SNTP clock sync + **OTA from GitHub Releases** (see §8b). Core paging stays ESP-NOW offline.

---

## 6. UI port mapping (web → LVGL)

| Web screen `data-screen` | LVGL screen id | Notes |
|--------------------------|----------------|-------|
| `hub` | `scr_hub` | App icons grid |
| `gamesfolder` | `scr_games` | Nested launcher |
| `werk` / `compose` / `outgoing` / `incoming` | pager screens | Incoming = top layer |
| `tictactoe` … `memory` | game screens | Panels: pick / wait / invite / play |
| `doodle` | `scr_doodle` | Canvas heavy |
| `settings` / `keyboard` / `emoji-picker` | settings + OSK | OSK shared |
| `idle` | `scr_idle` | Black or clock |

**Shared chrome:** topbar title + `vs` subtitle + optional meta line (Checkers / Memory status).  
**Modals:** forfeit confirm overlay (same copy).

Touch rules carried from sim:

- Battleship placement = tap anchor → highlight directions → tap to confirm (no hover)  
- Checkers = tap piece → destinations → tap dest  
- Memory = tap to flip  

---

## 7. Assets & memory budget

Memory faces live in `firmware/assets/memory/`. For device:

1. Copy into `firmware/data/memory/`  
2. Convert oversized PNGs → **JPEG ~80–120px** square (card is ~100px on 480 display)  
3. Mount LittleFS; load decode on flip (or decode once into RGB565 cache if RAM allows)  
4. Card back: keep `card-back.svg` re-exported as small PNG, or draw with LVGL primitives  

**Rough budget:** 8 faces × ~8–15 KB JPEG ≈ 100 KB flash + decode RAM spikes.

Doodle: RGB565 canvas 360×300-ish ≈ 200 KB—prefer smaller draw buffer or partial flush if PSRAM tight.

---

## 8. Timekeeping (no Wi‑Fi)

Sim / PC firmware stores `clockOffsetMs` (or desk clock) **relative to the host/OS wall clock**. Closing the app for two days still shows the correct date because the OS kept ticking.

On the **ESP32 desk** without Wi‑Fi:

| Power situation | Keeps correct date/time? |
|-----------------|---------------------------|
| App running / reboot while USB powered | Yes — RTC + NVS |
| Deep sleep (if we use it later) | Usually yes — RTC domain |
| **Full power cut** (unplug, no battery) | **No** — chip has no idea how long it was dead |

Most Guition 4848S040 modules **do not include a coin-cell RTC battery**. Rear **BAT** is for a **LiPo power cell** (desk stays powered), not RTC backup. So:

1. Persist name/theme/brightness/peers/canned in **NVS** (survives power loss).  
2. Persist last-known epoch in NVS; on boot, restore RTC from that — but it will be **frozen at power-off time** until the user re-sets Date & time (or we add NTP later).  
3. Optional **LiPo on BAT** reduces how often the desk fully dies while sitting on a desk.  
4. True “off for 2 days → correct date” with **no Wi‑Fi** still needs a **battery-backed RTC** (e.g. DS3231 on touch I²C) — not assumed for v1; not wired to the UART 4P.

Settings **Date & time** writes the clock (same UX as sim). Idle clock reads it.

### 8b. Optional Wi‑Fi — SNTP + OTA from GitHub

Wi‑Fi is **not** required for paging/games. Settings exposes optional actions:

1. **Sync time** — join STA → SNTP → write RTC / `clock_offset` (fixes “unplugged for 2 days” when online).  
2. **Updates** — list GitHub Releases (not only latest); user picks a tag to **upgrade or downgrade**, then download + flash.

**Yes, OTA binaries can live on GitHub:**

| Piece | Where |
|-------|--------|
| Source + roadmap | This git repo (`docs/`, `firmware/`) |
| Firmware binaries | GitHub **Release** assets, e.g. `werkbuddy-v1.2.0.bin` |
| Desk lists | `GET …/repos/<owner>/<repo>/releases` (paged) → show tags; highlight current `kFirmwareVersion` |
| Desk installs | Chosen release’s `browser_download_url` over TLS |
| Apply | ESP-IDF / Arduino OTA partition write + reboot |

Notes:

- Prefer a **public** repo (or release assets) so desks don’t need a GitHub token; if private, use a fine-scoped token in NVS (more ops pain).  
- Keep ESP-NOW working with Wi‑Fi STA connected (same radio; test coexistence).  
- PC sim stubs these buttons; real STA/SNTP/OTA is Phase 6 / post–Phase 0 device work.  
- Settings UI already has **Sync time** / **Check update** stubs.

---

## 9. Testing strategy

| Level | How |
|-------|-----|
| Sim regression | LVGL PC sim; multi-peer via sim link / multi-process later |
| Protocol | Golden vectors: pack/unpack fixtures shared if possible |
| Dual-board | Scripted play checklist per game (invite → move → forfeit → rematch) |
| Range | Desk ~3–10 m office; note walls / 2.4 GHz congestion |
| Soak | Idle timeout + overnight RTC drift check |

**Sim stays canonical** until firmware feature-complete; fix UX in sim first when possible, then port.

---

## 10. Explicit non-goals (v1)

- Internet / cloud sync  
- More than ~3–8 peers discovered  
- Background multi-game resume  
- Perfect CSS parity / web fonts on device  
- Hover-only interactions  
- Shipping JSON ESP-NOW in production builds  

---

## 11. When boards arrive — first agent session prompt

Use exactly (session ladder lives in the bring-up manual):

> Boards are here — GUITION ESP32-4848S040C_I. Read `docs/HARDWARE_BRINGUP_MANUAL.md` and walk me through **Session 0 only**.

Then: `docs/ESP32_PORT_PLAN.md` for architecture; do **not** skip to games.

---

## 12. Decision log (fill in as we go)

| Date | Decision | Notes |
|------|----------|-------|
| 2026-08-03 | Plan written from completed web sim | Pre-hardware |
| 2026-08-04 | Phase −1: LVGL 9.3 + SDL2 PC sim (CMake/MinGW) | Full UI surface on PC (not just hub stub) |
| 2026-08-04 | Bring-up manual written | `docs/HARDWARE_BRINGUP_MANUAL.md` |
| | Board model target | GUITION ESP32-4848S040C_I |
| | SDK: _TBD_ | Lock in Session 2 of bring-up manual |
| | LVGL version: **9.3** (PC); device pin later | Prefer 9.x on device if BSP allows |
| | FS: LittleFS vs SPIFFS: _TBD_ | |
| | Binary protocol ver: 1 | |

---

## 13. Risk register

| Risk | Mitigation |
|------|------------|
| PSRAM / framebuffer OOM | Single buffer + partial render; shrink doodle canvas |
| ESP-NOW packet loss | App-level retry on invites; doodle is best-effort strokes |
| Touch noise | Debounce; ignore multi-touch |
| MAC vs sim id confusion | One identity layer: MAC hex string everywhere in firmware |
| Theme/asset time sink | Ship Eleganza first; other themes later |
| Memory decode latency | Pre-scale assets; cache last N decoded faces |

---

*End of plan. Update this file when Phase 0 SDK choice is locked.*
