# Agent guide — WerkBuddy

Instructions for Cursor (and other) agents working in this repo. Prefer this file + `docs/HARDWARE_BRINGUP_MANUAL.md` when hardware is involved.

---

## Product facts

- **Brand:** WerkBuddy (splash / hub chrome). **Pager app name:** WerkPager.
- **Target board:** Guition **ESP32-4848S040** (ESP32-S3 N16R8 typical, 480×480 capacitive).
- **Link:** ESP-NOW between desks. Wi‑Fi is optional and **ephemeral** (join for SNTP/OTA, then disconnect — never stay on STA). Paging MAY drop while STA is up (same radio/channel). See port plan §8b.
- **UX source of truth:** `firmware/` LVGL PC sim (`scripts/run-sim.ps1`). Do not revive the archived web sim for new work.
- **Interactive QA (PC sim):** Prefer the TCP drive (`WERKPAGER_DRIVE=1`, `scripts/drive-sim.ps1`) — `tap` / `swipe` / `shot` / `screen` — over `WERKPAGER_SCREEN` jumps when validating real navigation. Keep screen jumps for static README galleries.
- **Interactive QA (real glass):** Planned **device drive** over USB (`docs/ESP32_PORT_PLAN.md` §8c) — same shot/tap idea for agents. Prefer **two boards / two COM ports** so the agent can act as both desks for ESP-NOW pager + games. **Debug builds only**; must compile out of release so normal desk use is unaffected. No Settings UI for it.
- **Protocol:** Keep `protocol/messages.js` type names in sync with `firmware/src/protocol/messages.h`. Binary on device; ~250 B ESP-NOW budget.

---

## Always-on rules

1. Read `.cursor/rules/esp32-port.mdc` (always applied).
2. Before changing wire protocol or device architecture, read `docs/ESP32_PORT_PLAN.md`.
3. No hover-only UI, no cloud required for v1. Multi-game resume (cap 24) is in the port plan.
4. One **bring-up Session** at a time when boards are present — never dump the whole ladder in one reply.

---

## When the operator says boards are here

1. Confirm model matches **ESP32-4848S040** (or note exact sticker text if different).
2. Read **Part A + Session 0** of `docs/HARDWARE_BRINGUP_MANUAL.md`.
3. Walk **Session 0 only** (unbox / inventory / photos / power plan). Stop when Session 0 exit criteria are met.
4. Next sessions (only when asked): vendor demo → our LVGL hello → ESP-NOW ping → shell → pager → games.

**Suggested operator prompt:**

> Boards are here — Guition ESP32-4848S040. Read `AGENTS.md` and `docs/HARDWARE_BRINGUP_MANUAL.md`. Walk me through **Session 0 only**.

---

## LiPo batteries (BAT connector)

Boards have a rear **MX1.25 2P BAT** silk: `BAT+` / `BAT−`.

| Do | Don’t |
|----|--------|
| Use a **single-cell LiPo** (~3.0–4.2 V) with correct polarity | Put **5 V** or USB VBUS on BAT |
| Expect the desk to stay powered without USB while the cell has charge | Assume wall-clock keeps advancing after a **full** power cut |
| Treat BAT as **desk power**, not a coin-cell RTC | Plug a DS3231 into the **UART 4P** (`3V3/TXD/RXD/GND`) as if it were I²C |

**Timekeeping v1:** Settings date/time + optional Wi‑Fi Sync time. External DS3231 (I²C on touch bus) is a later option, not Session 0.

**Other rear connectors (typical):**

- **MX1.25 4P** — UART (`3V3`, `TXD`, `RXD`, `GND`) for serial/flash when useful  
- **Speak** MX1.25 2P — speaker  
- **8-pin header** — to relay / PSU backplane on some SKUs  

Always verify silkscreen on the unit in hand before wiring.

---

## PC sim workflow (pre-hardware)

```powershell
powershell -File firmware/scripts/run-sim.ps1
powershell -File firmware/scripts/run-sim.ps1 -Shot   # → firmware/sim-out/preview.png
```

QA jump: `WERKPAGER_SCREEN=<name>` + `WERKPAGER_SHOT=1` (run from `firmware/build`).

---

## Phase reminder

| Phase | Goal |
|-------|------|
| **−1** | LVGL PC sim (current) |
| **0** | Glass + touch + ESP-NOW ping (bring-up Sessions) |
| **1** | Shell / NVS / idle |
| **2+** | Pager, games, doodle on device |

Do **not** flash the full desk app on day one. Night-one success = LCD + touch + brand + two-board ping.
