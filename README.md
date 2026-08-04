# WerkPager

Desk **attention pager** + mini-games for coworkers (Tommy / Will / Alex), designed for **480×480** capacitive panels talking over **ESP-NOW** (no Wi‑Fi required for core use).

Right now this repo is a **web simulator** + **LVGL PC sim** that are the UX + behavior spec for GUITION **ESP32-4848S040C_I** firmware (boards on order).

---

## Quick start (simulator)

From the project root:

```bash
python -m http.server 8765
```

Open: [http://localhost:8765/web/](http://localhost:8765/web/)

Use the **Desk: Tommy / Will / Alex** buttons under the device to switch who you’re controlling and test pairing, calls, and games.

---

## What’s in the sim

| Area | Features |
|------|----------|
| **Home** | WerkPager, Games folder, Doodle, Settings |
| **WerkPager** | Peer list, emoji + canned compose, call / Shantay / Sashay |
| **Games** | Tic Tac Toe, Connect Four, Battleship, Checkers, Memory |
| **Doodle** | Peer draw, colors, eraser, S/M/L strokes, ESP-NOW-style chunk sync |
| **Settings** | Name, themes, timeout, idle mode, **date & time**, emojis, canned, scan |
| **Idle** | Black or clock (uses manually set date/time) |

Notable UX choices already baked in:

- Touch-first (no hover-only flows; Battleship place = tap → directions → confirm)
- Forfeit always asks for confirmation
- Checkers: each player sees their pieces at the bottom
- Memory: shared seed + RPDR art in `web/assets/memory/`
- Connect Four: soft rainbow board frame
- Games lobby **Back** returns to Games folder; mid-game exit is **Forfeit** only

---

## Repo layout

```
WerkPager/
  web/                  # Simulator UI (spec)
    app.js              # Hub, pager, TTT, settings, idle, OSK
    board-games.js      # Connect Four, Battleship
    more-games.js       # Checkers, Memory, Doodle
    index.html
    styles.css
    assets/memory/      # Matching-game images
  protocol/
    messages.js         # MessageType catalog + helpers (JSON in sim)
  firmware/             # LVGL app (PC sim now → ESP32 later)
    CMakeLists.txt
    lv_conf.h
    src/ui/             # Screens (hub first)
    scripts/run-sim.ps1 # Build + launch SDL window
    third_party/lvgl/   # Cloned on first build (gitignored)
  docs/
    ESP32_PORT_PLAN.md           # Architecture + phases
    HARDWARE_BRINGUP_MANUAL.md   # Session-by-session unbox → glass (start here on arrival)
  .cursor/rules/
    esp32-port.mdc      # Cursor rule pointing agents at the port plan
```

---

## Optional Wi‑Fi (later on device)

Paging/games stay **ESP-NOW** (no router). Settings can optionally use Wi‑Fi for:

- **Sync time** (SNTP) after power loss  
- **OTA** from **GitHub Releases** (upload a `.bin` on a release; desks fetch `releases/latest`)

See `docs/ESP32_PORT_PLAN.md` §8b.

---

## Hardware (on order)

- **GUITION ESP32-4848S040C_I** — ESP32-S3, 4″ 480×480 capacitive (ST7701 + GT911 typical)  
- Link: **ESP-NOW** between desks  
- Settings date/time written locally (no NTP assumed for v1)

---

## When the boards arrive

1. Open this repo in Cursor.
2. Tell the agent:

   > Boards are here — GUITION ESP32-4848S040C_I. Read `docs/HARDWARE_BRINGUP_MANUAL.md` and walk me through **Session 0 only**.

3. Do **one session at a time** (unbox → vendor demo → our LVGL hello → ESP-NOW ping → shell → pager → games).  
4. Night-one expectation: LCD + touch + tappable WERKPAGER + two-board ping — **not** the entire game suite.

Architecture details: `docs/ESP32_PORT_PLAN.md`.  
**Visual note:** sims are the design + behavior bible; device UI is LVGL (not CSS). Pixel-perfect CSS effects are optional.

---

## Protocol

All message kinds live in `protocol/messages.js`. The sim sends JSON over an in-memory “radio.” Firmware should keep **type parity** and prefer **compact binary** on the wire (~250B ESP-NOW budget; doodle already chunks strokes).

---

## Status

| Track | State |
|-------|--------|
| Web simulator | Feature-complete for v1 product surface |
| LVGL PC sim | **Phase −1 done enough** — full desk UI on SDL (hub, pager, games, doodle, settings, idle) |
| Firmware on ESP32 | Not started — wait for boards; follow **bring-up manual** then port plan |

### LVGL desktop simulator

Real LVGL C++ UI in a **480×480** window — same UI stack we will retarget to the desks.

```powershell
powershell -File firmware/scripts/run-sim.ps1
```

- Full app: WerkPager, Games, Doodle, Settings, Idle  
- **F12** saves `firmware/sim-out/preview.png`  
- Agent workflow: edit LVGL → rebuild → you review the window  

Requires MSYS2 MinGW + SDL2 (`C:\msys64\mingw64`).

Last sim milestone: matching art, compact Checkers/Memory chrome, forfeit confirms, ESP32 port plan + Cursor rule + LVGL Phase −1 hub.
