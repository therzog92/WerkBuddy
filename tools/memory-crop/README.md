# Memory face cropper

Standalone drag‑and‑drop square cropper for WerkBuddy Memory tiles.

**Not part of the desk firmware / LVGL sim.**

## Run

Open `index.html` in a browser (double‑click or “Open with”).

Or from this folder:

```powershell
start index.html
```

## Export

- Size: **96×96 PNG** (what `scr_games.cpp` scales from onto the 72px cards)
- Drop into: `firmware/assets/memory/<any-name>.png`
- Memory picks **8 random faces** from that folder per match (add as many as you want)
- Re-run the sim (or rebuild) so `build/assets/memory` picks up copies
