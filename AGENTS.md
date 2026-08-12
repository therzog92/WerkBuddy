# Agent guide — WerkBuddy

Instructions for Cursor (and other) agents working in this repo. Prefer this file + `docs/HARDWARE_BRINGUP_MANUAL.md` when hardware is involved.

---

## Product facts

- **Brand:** WerkBuddy (splash / hub chrome). **Pager app name:** WerkPager.
- **Target board:** Guition **ESP32-4848S040** (ESP32-S3 N16R8 typical, 480×480 capacitive).
- **Link:** ESP-NOW between desks. Wi‑Fi is optional and **ephemeral** (join for SNTP/OTA, then disconnect — never stay on STA). Paging MAY drop while STA is up (same radio/channel). See port plan §8b.
- **UX source of truth:** `firmware/` LVGL PC sim (`scripts/run-sim.ps1`). Do not revive the archived web sim for new work.
- **Interactive QA (PC sim):** Prefer the TCP drive (`WERKPAGER_DRIVE=1`, `scripts/drive-sim.ps1`) — `tap` / `swipe` / `shot` / `screen` — over `WERKPAGER_SCREEN` jumps when validating real navigation. Keep screen jumps for static README galleries.
- **Interactive QA (real glass):** USB **device drive** (`docs/ESP32_PORT_PLAN.md` §8c) — `shot` / `tap` for agents. **Default OFF** in `firmware/device/platformio.ini` (`WERKPAGER_DEVICE_DRIVE=0`) so shipping builds pay zero cost; set to `1` only for debug flashes. No Settings UI. Prefer two COM ports (A+B) for ESP-NOW QA.
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
| **−1** | LVGL PC sim |
| **0–5** | Glass bring-up through doodle — **done on desks as of v0.67** (see `docs/STATUS_v0.67.md`) |
| **6+** | Device OTA, polish (sound / Memory FS / soak) |

**Leave-off doc:** [`docs/STATUS_v0.67.md`](docs/STATUS_v0.67.md) — read before continuing after a break.

Do **not** assume boards are unflashed. Night-one glass+ping is complete; continue from OTA / polish unless the operator says otherwise.
