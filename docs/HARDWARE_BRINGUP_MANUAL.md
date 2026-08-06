# WerkBuddy — Hardware Bring-Up Manual

**Audience:** Cursor agents + the operator bringing up desks  
**Board:** GUITION **ESP32-4848S040C_I** (ESP32-S3, 4″ 480×480 capacitive)  
**Companion docs:** `AGENTS.md`, `docs/ESP32_PORT_PLAN.md`, `.cursor/rules/esp32-port.mdc`  
**Last audited:** 2026-08-05 (pre-hardware; LiPo BAT documented; LVGL sim feature-complete for v1 surface)

---

## How to use this document

### For agents (critical)

1. Read **Part A** (readiness) and **Part B** (what already exists) once when boards arrive.
2. Walk the operator through **exactly one Session** from Part C at a time.
3. After each Session succeeds, **stop**. Say what worked and what the *next* Session is named — do **not** paste Sessions 3–8 in one reply.
4. Do **not** jump to games, doodle, or full app flash until Phase 0 exit criteria are met.
5. Prefer fixing product behavior in the PC LVGL sim first; port to glass second.
6. Never invent hover-only UX or cloud/always-on Wi‑Fi for v1. Multi-game resume follows the port plan (cap 24).

### For the operator

When boards arrive, open this repo and say:

> Boards are here — GUITION ESP32-4848S040C_I. Read `AGENTS.md` and `docs/HARDWARE_BRINGUP_MANUAL.md` and walk me through **Session 0 only**.

Then do each Session when the agent says you’re ready. One chunk at a time.

---

# Part A — Readiness review (2026-08-04)

## Verdict

| Track | State | Ready for glass? |
|-------|--------|------------------|
| LVGL PC sim (`firmware/`) | Feature-complete UX/behavior bible | Yes — active oracle |
| Web HTML sim | Archived at `../WerkBuddy-web-sim-archive/` | Historical only |
| LVGL PC sim (`firmware/`, SDL) | **Phase −1 essentially complete** for v1 product surface | Yes — UI/logic to port |
| ESP32 / PlatformIO / IDF project | **Does not exist yet** | No — Phase 0 |
| Binary ESP-NOW codec | **Not implemented** (fat structs on sim link only) | No — Phase 0–3 |
| NVS / RTC on device | PC uses `werkpager_settings.ini` | No — Phase 1 |
| Vendor BSP / pins in-repo | Assumptions only | Verify on unbox |

**Bottom line:** App UX and game logic are far ahead of hardware plumbing. Night one is **glass + touch + ping**, not “flash the whole desk app.” Most of `firmware/src/{app,ui,games,protocol}` is transport-agnostic and should be reused after a new device `main` + display/touch + `espnow_link`.

## What looks good

1. **Full LVGL shell on PC** — Hub, WerkPager (peer pick / compose / outgoing / incoming), Games folder + TTT / C4 / Battleship / Checkers / Memory, Doodle, Settings (name, themes, timeout, idle, date/time pickers, emoji picker, canned OSK), Idle clock.
2. **Compose polish** — Peer select says **WERKPAGER** + “Who are we bothering?”; compose says **WERK ROOM** with Clear Message / Custom message + canned + emoji.
3. **Game end states** — Win/lose overlays, Play again / Home, forfeit confirm path, Battleship placement without hover, C4 drop anim, Memory faces from `firmware/assets/memory/`.
4. **Protocol type catalog** — `protocol/messages.js` ↔ `firmware/src/protocol/messages.h` (keep names in sync).
5. **Transport seam** — `net/link.h` + `sim_link.cpp` today; `espnow_link.cpp.hardware` is a sketched placeholder with clear rename steps.
6. **Soft constraints already designed** — 480×480 touch-first, ~250B ESP-NOW budget, doodle chunking (`kMaxStrokePts = 40`), names ≤12, messages ≤22, no cloud for v1.

## Gaps / risks before port

| Gap | Why it matters | When to fix |
|-----|----------------|-------------|
| No device build target | Can’t flash WerkBuddy yet | Phase 0 |
| No agent glass drive | Agents can’t see/poke the panel like the PC sim | Phase 1 debug builds — §8c (USB shot/tap; dual-COM A+B for ESP-NOW QA; **off in release**) |
| SDL `main.cpp` + `LV_COLOR_DEPTH 32` | Device needs RGB565 + PSRAM FB + GT911 indev | Phase 0 |
| No `pack()` / `unpack()` | ESP-NOW can’t carry fat `Msg` as-is | Phase 0 ping → Phase 3 games |
| Sim bots only (`mac-will` / `mac-alex`) | Real peers = Wi‑Fi MAC hex | Phase 0–1 |
| Settings `.ini` on disk | Device needs NVS (+ RTC for clock) | Phase 1 |
| Doc drift (README still says “hub stub”) | Agents may under-estimate PC progress | Fixed by this manual + plan updates |
| PSRAM / doodle canvas RAM | 480×480 RGB + canvas can OOM | Measure Phase 0; shrink if needed |
| Touch axis / rotation mismatch | Common on 4848S040 | Calibrate in Phase 0 |

## Time across power-off (no Wi‑Fi)

**PC sim / web:** Yes — clock is an offset from the computer’s clock, so closing for 2 days still lands on the right day.

**Real GUITION board, unplugged:** Settings (name, theme, brightness, peers, canned) survive in flash/NVS. **Wall-clock advancing while fully powered off does not**, unless the board has a battery-backed RTC (these modules usually don’t). After a full unplug, re-set Date & time — or use optional **Wi‑Fi → Sync time** (SNTP; join briefly then disconnect). OTA can ship as **GitHub Release** `.bin` assets (port plan §8b). Core paging stays ESP-NOW; Wi‑Fi is ephemeral only — paging may drop while STA is associated.

## Explicit non-goals (still)

- Internet / cloud / NTP required for v1  
- Hover-only UI  
- Shipping JSON over ESP-NOW in production  
- Persisting active games across full power-off  

- Perfect CSS pixel parity  

---

# Part B — Agent briefing (what we have coded)

## Board identity (confirm sticker on arrival)

| Field | Expected |
|-------|----------|
| Brand | **GUITION** |
| Model | **ESP32-4848S040C_I** (also sold as 4848S040) |
| MCU | ESP32-S3 (typically **N16R8**: 16 MB flash, **8 MB PSRAM**) |
| Panel | 4.0″ IPS **480×480**, driver **ST7701** (SPI init + RGB parallel) |
| Touch | Capacitive **GT911** (I2C, often addr `0x5D`) |
| Power | **5V** via USB (bring-up); optional **LiPo** on rear **BAT** MX1.25 2P (`BAT+`/`BAT−`, 3.0–4.2 V only) |
| Radio | On-chip Wi‑Fi used for **ESP-NOW** (no router needed for v1) |

Typical pin map (verify against vendor sheet / sticker; revisions exist):

| Function | Typical GPIO |
|----------|----------------|
| Backlight | 38 |
| ST7701 SPI CS / SCK / SDA | 39 / 48 / 47 |
| RGB DE / VSYNC / HSYNC / PCLK | 18 / 17 / 16 / 21 |
| RGB R0–R4 | 11, 12, 13, 14, 0 |
| RGB G0–G5 | 8, 20, 3, 46, 9, 10 |
| RGB B0–B4 | 4, 5, 6, 7, 15 |
| Touch I2C SDA / SCL | 19 / 45 (some docs say SCL 20 — **measure / use vendor demo**) |
| SD CS (optional) | 42 |
| Relay (some PCBs) | 40 |

Community references (for bring-up demos, not gospel):

- PlatformIO + Arduino_GFX: search “GUITION ESP32-4848S040 PlatformIO”
- IDF + LVGL 9 + GT911: `abcbbck-png/ESP32-4848S040-First-Start` (GitHub)
- Pin / HomeDing notes: `homeding.github.io` panel-4848S040

## Repo map (do not reinvent)

```
WerkPager/
  firmware/                # LVGL PC sim → ESP32 (UX + behavior SPEC)
  protocol/messages.js     # MessageType names (JSON in sim)
  firmware/
    CMakeLists.txt         # PC sim only today
    lv_conf.h              # LVGL 9.3, SDL on, LittleFS off
    scripts/run-sim.ps1    # Build/run PC window
    assets/{emoji,fonts,fx,memory}/
    src/
      main.cpp             # SDL entry — REPLACE for device
      app/                 # Desk state machine (KEEP)
      ui/                  # All LVGL screens (KEEP, retarget)
      games/*.h            # Engines (KEEP)
      protocol/messages.h  # Enum + fat Msg (KEEP; add pack/unpack)
      net/
        link.h
        sim_link.cpp       # PC bots — keep for desktop
        espnow_link.cpp.hardware  # Rename + implement for device
      storage/             # .ini today → NVS on device
  docs/
    ESP32_PORT_PLAN.md
    HARDWARE_BRINGUP_MANUAL.md   # this file
```

## Architecture (reuse)

```
UI (LVGL screens)  →  app::Desk / handle_msg  →  games engines
                              ↓
                     net::link_send / link_init
                              ↓
              sim_link (PC)  |  espnow_link (device) + binary pack
```

Everything above `link_*` should stay shared. Device work is mostly:

1. Display + touch + backlight bring-up  
2. Device `main` / LVGL tick / flush  
3. Binary pack/unpack + ESP-NOW  
4. NVS + RTC  
5. Asset FS (LittleFS) for Memory faces  

## PC sim how-to (still useful after boards)

```powershell
powershell -File firmware/scripts/run-sim.ps1
# or after build:
Start-Process firmware\build\werkpager_sim.exe -WorkingDirectory firmware\build
```

- Clear `WERKPAGER_SHOT` / `WERKPAGER_SCREEN` for interactive window  
- F12 or `-Shot` → `firmware/sim-out/preview.png`  
- LVGL: `powershell -File firmware/scripts/run-sim.ps1`

## Phase ladder (do not skip)

| Phase | Goal | Exit |
|-------|------|------|
| **−1** | LVGL on PC | ✅ Done enough — polish only as needed |
| **0** | Glass + touch + ESP-NOW HELLO | Both boards show tappable WERKBUDDY; ping toast |
| **1** | Hub + Settings + idle + discover | B sees A after scan; name/theme/clock persist |
| **2** | WerkPager calls IRL | A pings B; B Shantays; A toast |
| **3** | Binary codec + TTT | Full TTT match two desks |
| **4** | C4 → Checkers → Memory → Battleship | Each game playable IRL |
| **5** | Doodle chunk sync | Shared canvas two desks |
| **6** | Polish | Themes, soak, optional sound |

---

# Part C — Session ladder (ONE at a time)

Each **Session** is a single work chunk for Tommy + agent.  
Agent rule: complete Session N, celebrate, name Session N+1, **stop**.

---

## Session 0 — Unbox & inventory (no coding)

**Goal:** Know what physically arrived and that power/USB work.

### You need

- [ ] Each GUITION board (start with **2** if only 2 arrived; expect ~3 desks later)
- [ ] USB cable(s) that carry **data** (not charge-only)
- [ ] 5V power as supplied (USB-C / micro / barrel — match the board)
- [ ] Optional: **LiPo** cells for the rear **BAT** connector (MX1.25 2P) — polarity `BAT+` / `BAT−` only; **never 5 V on BAT**
- [ ] Windows PC with free USB ports
- [ ] This repo open in Cursor

### Steps

1. Open the box. Photograph the **label** on the PCB/back: brand, model, any QR / wiki URL.
2. Confirm model text matches **ESP32-4848S040C_I** (or 4848S040 family). If different, stop and tell the agent the exact string.
3. Photograph the **rear connectors** and confirm silkscreen: **BAT** (2P), **UART** (4P: 3V3/TXD/RXD/GND), **Speak** (2P).
4. Note included accessories: USB cable, power adapter, stand, SD card, pin headers, LiPo (if ordered separately).
5. Plug **one** board into USB (power + data). Do **not** force connectors. Leave LiPo disconnected until USB flash path works.
6. Windows → Device Manager → look for a new **COM port** or “USB Serial” / “USB JTAG”. Write down the COM number (e.g. `COM5`).
7. If nothing appears: try another cable, another port, install Espressif USB drivers later (Session 1).

### LiPo (optional in Session 0)

- Inventory the cells and connectors; do **not** hot-plug LiPo while experimenting with unknown firmware unless the agent says it’s safe.
- First power path for bring-up remains **USB 5 V**. LiPo is for untethered desks after glass is proven.
- Reminder: LiPo keeps the **board** alive; it is not a coin-cell RTC. Clock across full power-off still needs Sync time or a later DS3231 on I²C.

### Done when

- You have photos of the board label **and** rear BAT/UART silk  
- At least one board powers on from USB (backlight may or may not light until firmware)  
- You know the COM port (or that drivers are missing)  
- LiPo cells (if any) are inventoried with correct connector type noted  

### Tell the agent

> Session 0 done. Model on sticker: ____. COM port: ____. Accessories: ____. LiPo on hand: Y/N. Ready for Session 1.

---

## Session 1 — Vendor demo (prove LCD + touch)

**Goal:** Factory / Guition demo firmware proves the glass and touch. No WerkBuddy code yet.

### You need

- Session 0 complete  
- Vendor demo package (QR on box, Guition site, or SD card) **or** a known working community demo for 4848S040  
- Flash tool: **Arduino IDE**, **PlatformIO**, or **ESP-IDF** — pick whatever the vendor demo instructions use

### Steps (agent guides the exact clicks for the chosen toolchain)

1. Download the **official Guition demo** for ESP32-4848S040C_I if available. Prefer their “LCD + touch” example over Home Assistant/ESPHome for day one.
2. Install only what’s required for that demo (don’t install three frameworks yet).
3. Select board / partition / PSRAM settings that match **N16R8** (16 MB flash, OPI PSRAM) if prompted.
4. Select the COM port from Session 0.
5. Flash the demo to **Board A**.
6. Reset / power cycle. Confirm:
   - [ ] Backlight on  
   - [ ] Picture / UI visible (not white/black forever)  
   - [ ] Touch moves a cursor / presses a button  
7. Optionally flash the same demo to **Board B** so both are known-good glass.

### Record for the decision log (`ESP32_PORT_PLAN.md` §12)

- SDK used for demo: Arduino / PlatformIO / IDF  
- LVGL version in demo (if any): ____  
- Touch works? Y/N · any axis flip? Y/N  

### Done when

Both (or at least one) boards show a working vendor UI with working touch.

### Tell the agent

> Session 1 done. Demo toolchain was ____. Touch OK: Y/N. Axis weirdness: ____. Ready for Session 2.

---

## Session 2 — Toolchain lock + “hello LVGL” on device

**Goal:** Decide **PlatformIO+Arduino** vs **ESP-IDF**, create the device project skeleton, flash a **single tappable “WERKBUDDY”** label (blank shell — not full app).

### Decision guide

| Choose… | If… |
|---------|-----|
| **PlatformIO + Arduino** | Vendor / community demos you just used were Arduino_GFX + LVGL and worked easily |
| **ESP-IDF** | Vendor BSP is IDF-only, or you want `esp_lcd` + `esp_lcd_touch_gt911` (LVGL 9 friendly) |

**Default recommendation if demo was Arduino:** start PlatformIO+Arduino for speed, keep LVGL **9.x** aligned with PC sim if possible (community samples often still use LVGL 8 — note the mismatch and plan bump).

### Agent tasks (this session only)

1. Add device project under `firmware/` (e.g. `firmware/device/` or PlatformIO env) **without breaking** `run-sim.ps1` PC build.
2. Wire ST7701 RGB + backlight + GT911 → LVGL display + indev.
3. Set color depth appropriate for RGB panel (**RGB565** typical).
4. Show centered label **WERKBUDDY**; tap toggles a toast or color.
5. Update `docs/ESP32_PORT_PLAN.md` §12 with SDK + LVGL version locked.

### Tommy steps

1. Let the agent generate the project + `platformio.ini` / IDF files.  
2. Plug Board A, flash, confirm tap works.  
3. If touch is mirrored/rotated: tell the agent exact symptom (e.g. “tap top-left acts bottom-right”).

### Done when

Board A shows tappable **WERKBUDDY** from *our* project (not only vendor demo).

### Tell the agent

> Session 2 done. SDK locked: ____. LVGL: ____. Touch mapping: OK / needs flip. Ready for Session 3.

---

## Session 3 — Two-board ESP-NOW ping

**Goal:** Prove radio path before porting the whole app.

### You need

- Two boards powered (USB is fine)  
- ~1–3 m apart on a desk  
- Session 2 shell on both (or Board B still on a tiny ping sketch)

### Agent tasks

1. Minimal `HELLO` / `ACK` over ESP-NOW (can be raw bytes; binary WerkBuddy codec can wait).  
2. On receive: LVGL toast + Serial log of peer MAC + RSSI if available.  
3. Document MAC addresses for desks (Tommy / Will / Alex labels).

### Tommy steps

1. Flash ping firmware to Board A and Board B.  
2. Power both. Tap “Ping” (or auto-ping every 2s).  
3. Confirm each board shows the other’s ping.  
4. Write MACs on a sticky note / in chat.

### Done when

A↔B ping works at desk distance. Phase **0 exit** satisfied if Session 2+3 both pass.

### Tell the agent

> Session 3 done. MACs: A=____ B=____. RSSI roughly ____. Ready for Session 4 (Phase 1 shell).

---

## Session 4 — Phase 1 shell (hub + settings + discover)

**Goal:** Port hub + settings + idle + peer discovery onto glass. **No games yet.**

### Agent tasks

1. Retarget existing `ui/scr_hub.cpp`, `scr_settings.cpp`, `scr_idle.cpp` (and chrome/theme) onto device LVGL port — avoid rewriting screens.  
2. Replace storage with **NVS** (`werkpager` namespace): name, theme, timeout, idle mode, clock offset/RTC.  
3. Implement discover / discover_reply over ESP-NOW (binary or temporary compact format — prefer starting binary header from plan §4.2).  
4. Peer list persists MAC + name.  
5. Keep PC sim build working (`sim_link` still compiles).

### Tommy test script

1. Set name on A to **Tommy**, B to **Will**.  
2. Set date/time on both.  
3. Scan peers on A → Will appears; save.  
4. Wait for idle timeout → clock or black as configured → tap wakes.  
5. Power-cycle both → names/themes still there.

### Done when

Plan Phase 1 exit: *Set name/date/time/theme on A; B sees A after scan.*

### Tell the agent

> Session 4 done. Discover works: Y/N. NVS survives reboot: Y/N. Ready for Session 5 (pager).

---

## Session 5 — Phase 2 WerkPager calls

**Goal:** Real desk paging.

### Agent tasks

Port/wire `scr_pager.cpp` flows: peer pick → compose (emoji, Clear/Custom, canned) → outgoing → incoming (Shantay / Sashay) → ack/clear. Idle must **not** sleep over active call.

### Tommy test script

1. A: open WerkPager → pick Will → emoji + message → Send.  
2. B: full-screen incoming → **Shantay**.  
3. A: toast / clear. Repeat with **Sashay**.  
4. Confirm custom keyboard message works on device OSK.

### Done when

Plan Phase 2 exit: *A pings B; B shantays; A gets toast.*

### Tell the agent

> Session 5 done. Ready for Session 6 (TTT + binary codec).

---

## Session 6 — Phase 3 binary codec + Tic Tac Toe

**Goal:** Lock wire format; prove invite/accept/move/forfeit.

### Agent tasks

1. Implement `pack` / `unpack` per `ESP32_PORT_PLAN.md` §4; keep `MsgType` names aligned with JS.  
2. Swap any temporary ping/JSON off the production path.  
3. Port TTT UI + engine already in `scr_games.cpp` / `games/ttt.h`.  
4. Forfeit confirm copy must match sim.

### Tommy test script

Full TTT: invite → accept → play to win → Play again → forfeit confirm once.

### Done when

Full TTT match on two desks.

### Tell the agent

> Session 6 done. Ready for Session 7 (remaining games — one game per sitting).

---

## Session 7 — Phase 4 remaining games (split sittings)

**Agent rule:** Do **one game per sitting** with Tommy. Order:

1. Connect Four  
2. Checkers (own pieces at bottom)  
3. Memory (LittleFS/JPEG faces; seed sync)  
4. Battleship (anchor → direction → confirm)

After each game: stop, ask Tommy to smoke-test, then wait for “ready for next game.”

---

## Session 8 — Phase 5 Doodle + Phase 6 polish

1. Doodle peer pick, colors, eraser, S/M/L, chunked strokes, clear.  
2. Polish: theme parity, Memory size budget, soak idle overnight, range test 3–10 m.  
3. Optional later: sound, OTA (needs Wi‑Fi AP — out of v1).

---

# Part D — Agent checklists & prompts

## When Tommy says “boards are here”

Reply with **only** Session 0 instructions (unbox). Do not start coding.

## Night-one success (realistic)

- [ ] Vendor demo LCD + touch  
- [ ] Our LVGL “WERKBUDDY” tap shell  
- [ ] Two-board ESP-NOW ping  
**Not** required night one: all games, doodle, OTA.

## Files to touch first in Phase 0

| Action | Path |
|--------|------|
| New device entry | `firmware/device/...` or PlatformIO env (don’t break PC `main.cpp`) |
| Transport | Rename `espnow_link.cpp.hardware` → implement; wire into device build |
| Codec | New `firmware/src/protocol/pack.cpp` (or similar) |
| Plan log | `docs/ESP32_PORT_PLAN.md` §12 |
| This manual | Check off Sessions as done |

## PC vs device `lv_conf` notes

| Setting | PC sim today | Device likely |
|---------|--------------|---------------|
| `LV_USE_SDL` | 1 | 0 |
| `LV_COLOR_DEPTH` | 32 | 16 (RGB565) |
| `LV_USE_FS_LITTLEFS` | 0 | 1 when Memory assets land |
| Tick / flush | SDL | `esp_timer` + RGB panel flush / bounce buffer in PSRAM |

## Identity rules on device

- Peer id = **MAC hex** (e.g. `a1b2c3d4e5f6`), not `mac-tommy`  
- Display name from NVS (Tommy / Will / Alex)  
- One identity layer everywhere in firmware  

## Smoke matrix (after Phase 2+)

| Test | Pass? |
|------|-------|
| Discover + save peer | |
| Call + Shantay + Sashay | |
| Idle during call does not sleep | |
| TTT full match | |
| Forfeit confirm | |
| C4 / CK / Mem / BS | |
| Doodle stroke + clear | |
| Reboot keeps settings | |
| Desk distance 3–10 m | |

## Paste prompts (Tommy → agent)

**Start:**  
> Boards are here — GUITION ESP32-4848S040C_I. Read `AGENTS.md` and `docs/HARDWARE_BRINGUP_MANUAL.md` and walk me through Session 0 only.

**Continue:**  
> Session N done. Notes: ____. Walk me through Session N+1 only.

**Stuck:**  
> Session N stuck at step ____. Symptoms: ____. COM: ____. Don’t skip ahead.

---

# Part E — Decision log stubs (fill on bring-up)

Copy into `ESP32_PORT_PLAN.md` §12 when known:

| Date | Decision | Notes |
|------|----------|-------|
| | SDK | Arduino PlatformIO / ESP-IDF |
| | LVGL on device | version |
| | FS | LittleFS / SPIFFS |
| | Touch I2C pins | SDA/SCL confirmed |
| | Color depth | RGB565 |
| | Binary protocol | ver 1 |

---

*End of hardware bring-up manual. Agents: one Session per reply. Tommy: plug what the Session says, then report back.*
