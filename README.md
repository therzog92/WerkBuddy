# WerkBuddy

Desk **attention pager** + mini-games for nearby desks, built for **480×480** capacitive panels talking over **ESP-NOW** (no Wi‑Fi required for core use).

Product brand: **WerkBuddy**. The pager app on Home stays **WerkPager**.

Active development is the **LVGL PC simulator** under `firmware/` — the same UI stack we will flash to Guition **ESP32-4848S040** desks.

## Screenshots

LVGL PC sim (480×480). One shot per screen; games show a mid-play board. All thumbs are the same width.

### Shell

| Splash | Setup | Home | Idle |
|:------:|:-----:|:----:|:----:|
| <img src="docs/screenshots/splash.png" width="140" alt="Splash" /> | <img src="docs/screenshots/setup.png" width="140" alt="Setup" /> | <img src="docs/screenshots/hub.png" width="140" alt="Home" /> | <img src="docs/screenshots/idle.png" width="140" alt="Idle" /> |

### WerkPager

| Peers | Compose | Outgoing | Incoming |
|:-----:|:-------:|:--------:|:--------:|
| <img src="docs/screenshots/werk.png" width="140" alt="WerkPager" /> | <img src="docs/screenshots/compose.png" width="140" alt="Compose" /> | <img src="docs/screenshots/outgoing.png" width="140" alt="Outgoing" /> | <img src="docs/screenshots/incoming.png" width="140" alt="Incoming" /> |

| Page history | | | |
|:------------:|---|---|---|
| <img src="docs/screenshots/page-history.png" width="140" alt="Page history" /> | | | |

### Games

| Folder | Tic Tac Toe | Super TTT | Connect Four |
|:------:|:-----------:|:---------:|:------------:|
| <img src="docs/screenshots/games.png" width="140" alt="Games" /> | <img src="docs/screenshots/ttt.png" width="140" alt="Tic Tac Toe" /> | <img src="docs/screenshots/sttt.png" width="140" alt="Super TTT" /> | <img src="docs/screenshots/c4.png" width="140" alt="Connect Four" /> |

| Battleship | Checkers | Memory | Reversi |
|:----------:|:--------:|:------:|:-------:|
| <img src="docs/screenshots/battleship.png" width="140" alt="Battleship" /> | <img src="docs/screenshots/checkers.png" width="140" alt="Checkers" /> | <img src="docs/screenshots/memory.png" width="140" alt="Memory" /> | <img src="docs/screenshots/reversi.png" width="140" alt="Reversi" /> |

| Dots & Boxes | Scoreboard | | |
|:------------:|:----------:|---|---|
| <img src="docs/screenshots/dots.png" width="140" alt="Dots and Boxes" /> | <img src="docs/screenshots/scoreboard.png" width="140" alt="Scoreboard" /> | | |

### Utilities & doodle

| Utils | Timer | Checklist | Calculator |
|:-----:|:-----:|:---------:|:----------:|
| <img src="docs/screenshots/utils.png" width="140" alt="Utilities" /> | <img src="docs/screenshots/timer.png" width="140" alt="Timer" /> | <img src="docs/screenshots/checklist.png" width="140" alt="Checklist" /> | <img src="docs/screenshots/calculator.png" width="140" alt="Calculator" /> |

| Doodle pick | Doodle draw | | |
|:-----------:|:-----------:|---|---|
| <img src="docs/screenshots/doodle.png" width="140" alt="Doodle pick" /> | <img src="docs/screenshots/doodle-draw.png" width="140" alt="Doodle draw" /> | | |

### Settings

| Settings | Keyboard | Emoji picker | Wi‑Fi scan |
|:--------:|:--------:|:------------:|:----------:|
| <img src="docs/screenshots/settings.png" width="140" alt="Settings" /> | <img src="docs/screenshots/keyboard.png" width="140" alt="Keyboard" /> | <img src="docs/screenshots/emoji.png" width="140" alt="Emoji picker" /> | <img src="docs/screenshots/wifi.png" width="140" alt="Wi-Fi scan" /> |

| Wi‑Fi password | Updates (OTA) | Background QR upload | |
|:--------------:|:-------------:|:--------------------:|---|
| <img src="docs/screenshots/wifi-pass.png" width="140" alt="Wi-Fi password" /> | <img src="docs/screenshots/ota.png" width="140" alt="OTA updates" /> | <img src="docs/screenshots/bg-upload.png" width="140" alt="Background QR upload" /> | |

---

## Quick start (LVGL sim)

```powershell
powershell -File firmware/scripts/run-sim.ps1
```

Requires MSYS2 MinGW + SDL2 (`C:\msys64\mingw64`). **F12** (or `-Shot`) saves `firmware/sim-out/preview.png`.

---

## What’s in the sim

| Area | Features |
|------|----------|
| **Home** | WerkPager, Games, Utilities, Doodle, Settings |
| **WerkPager** | Peer list, emoji + canned compose, call / Shantay / Sashay, page history |
| **Games** | Tic Tac Toe, Super TTT, Connect Four, Battleship, Checkers, Memory, Reversi, Dots & Boxes, Scoreboard |
| **Utilities** | Timer, Checklist, Calculator |
| **Doodle** | Peer draw, colors, eraser, S/M/L strokes, chunked stroke sync |
| **Settings** | Name, themes, brightness, background, timeout, idle, date & time, emojis, canned, peers, factory reset |
| **Setup** | First-run / post-reset: name + theme |
| **Idle** | Black or clock |

Notable UX already baked in:

- Touch-first (no hover-only flows)
- Forfeit always confirms
- Checkers: each player sees their pieces at the bottom
- Memory: shared seed + art under `firmware/assets/memory/`
- Mid-game exit is **Forfeit** only (no multi-game resume in v1)

---

## Repo layout

```
protocol/           MessageType catalog (keep names in sync with C++)
firmware/           LVGL app (PC SDL sim now → ESP32 later)
  src/ui/           Screens
  scripts/run-sim.ps1
docs/
  ESP32_PORT_PLAN.md            Architecture + phases
  HARDWARE_BRINGUP_MANUAL.md    Session-by-session unbox → glass → ping
AGENTS.md                       Instructions for Cursor agents (boards, LiPo, sessions)
.cursor/rules/esp32-port.mdc
```

---

## Hardware

- **Guition ESP32-4848S040** (ESP32-S3, 4″ 480×480 capacitive; ST7701 + GT911 typical)
- **ESP-NOW** desk-to-desk (router optional)
- Rear connectors (confirm silkscreen on your unit):
  - **BAT** MX1.25 2P — LiPo cell only (`BAT+` / `BAT−`, ~3.0–4.2 V) — **not** 5 V
  - **UART** MX1.25 4P — `3V3` / `TXD` / `RXD` / `GND`
  - **Speak** MX1.25 2P — speaker
- Optional **LiPo** on BAT keeps the desk powered without USB; wall-clock across full power-off still needs Sync time (Wi‑Fi) or a future external RTC

---

## When the boards arrive

1. Open this repo in Cursor.
2. Say:

   > Boards are here — Guition ESP32-4848S040. Read `AGENTS.md` and `docs/HARDWARE_BRINGUP_MANUAL.md`. Walk me through **Session 0 only**.

3. Do **one session at a time** (unbox → vendor demo → LVGL hello → ESP-NOW ping → shell → pager → games).
4. Night-one goal: LCD + touch + tappable WERKBUDDY + two-desk ping — **not** the full game suite.

Details: `docs/HARDWARE_BRINGUP_MANUAL.md` and `docs/ESP32_PORT_PLAN.md`.

---

## Optional Wi‑Fi (device)

Paging/games stay **ESP-NOW**. Wi‑Fi is **ephemeral only** — never leave STA associated for long:

- Save credentials in NVS; Settings can show “Saved: …” without staying on the AP
- **Sync time** (SNTP) / **OTA** / similar: join briefly → do the job → **disconnect**
- ESP-NOW and STA share the same 2.4 GHz radio. While STA is up, the radio follows the AP’s channel, so **paging/games MAY drop or go offline** until Wi‑Fi disconnects

See `docs/ESP32_PORT_PLAN.md` §8b.

---

## Protocol

Message kinds: `protocol/messages.js` ↔ `firmware/src/protocol/messages.h`. Prefer **compact binary** on the wire (~250 B ESP-NOW budget; doodle already chunks).

---

## Status

| Track | State |
|-------|--------|
| LVGL PC sim | Active UX + behavior surface |
| Firmware on ESP32 | Not started — wait for boards; follow bring-up manual |
| LiPo on BAT | Supported by hardware; safe power path documented in bring-up |
