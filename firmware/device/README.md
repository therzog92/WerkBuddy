# WerkBuddy device firmware (Guition ESP32-4848S040)

PlatformIO project that links the shared LVGL UI (`../src`) onto real glass + ESP-NOW.

**Current shipping version:** see `kFirmwareVersion` in `../src/app/app.h` (v0.67).

## Flash

```powershell
cd firmware/device
pio run -t upload --upload-port COM5   # desk A (Tommy)
pio run -t upload --upload-port COM6   # desk B (Will)
```

Leave-off / handoff notes: [`docs/STATUS_v0.67.md`](../../docs/STATUS_v0.67.md).

## Device drive (optional, OFF by default)

USB `shot` / `tap` / `swipe` for agent glass QA. **Compiled out** when:

```ini
-DWERKPAGER_DEVICE_DRIVE=0
```

in `platformio.ini` (shipping default). Header stubs in `src/device_drive.h` mean **zero runtime cost**.

To turn on for a debug flash: set `-DWERKPAGER_DEVICE_DRIVE=1`, rebuild, then use `../scripts/drive-device.ps1`. See port plan §8c.

## Important sources

| File | Role |
|------|------|
| `src/main.cpp` | LCD flush, touch, TZ, UI boot |
| `src/wifi_jobs.cpp` | Ephemeral STA scan + SNTP |
| `src/msg_codec.cpp` / `espnow_app_link.cpp` | Binary ESP-NOW |
| `src/storage_app_nvs.cpp` | Preferences / NVS |
| `src/emoji_assets_gen.cpp` | Baked emoji (regen via `scripts/bake_emoji_assets.py`) |
| `scripts/pio_shared_src.py` | Pulls `firmware/src` into the build |
