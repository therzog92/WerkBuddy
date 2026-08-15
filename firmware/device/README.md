# WerkBuddy device firmware (Guition ESP32-4848S040)

PlatformIO project that links the shared LVGL UI (`../src`) onto real glass + ESP-NOW.

**Current shipping version:** `kFirmwareVersion` in `../src/app/app.h`.

## Flash a fresh board

This writes the **bootloader**, **partition table**, and **app** together — use this for a brand-new board (the release `.bin` is app-only and relies on these already being present):

```powershell
cd firmware/device
pio run -t upload --upload-port COMx   # replace COMx with your port (e.g. COM6)
```

Check the port with `pio device list`.

## Update an existing board

No cable needed — see the main `README.md`:

1. **Settings → Network → Wi-Fi** to save a network (used only for updates/time sync).
2. **Settings → Network → Updates** to pick a release and install it.

The release asset (`werkbuddy-vX.Y.Z.bin`) is produced by building this project and renaming `firmware.bin`.

## Device drive (optional, OFF by default)

USB `shot` / `tap` / `swipe` for automated glass QA. Compiled out when `-DWERKPAGER_DEVICE_DRIVE=0` in `platformio.ini` (shipping default). Enable with `-DWERKPAGER_DEVICE_DRIVE=1`, rebuild, then use `../scripts/drive-device.ps1`.

## Key sources

| File | Role |
|------|------|
| `src/main.cpp` | LCD flush, touch, timezone, UI boot |
| `src/wifi_jobs.cpp` | Ephemeral Wi-Fi scan + SNTP time sync |
| `src/github_ota_device.cpp` | GitHub Releases list + OTA flash |
| `src/msg_codec.cpp` / `espnow_app_link.cpp` | Binary ESP-NOW link |
| `src/storage_app_nvs.cpp` | Preferences / NVS persistence |
| `src/emoji_assets_gen.cpp` | Baked emoji (regen via `scripts/bake_emoji_assets.py`) |
| `scripts/pio_shared_src.py` | Pulls `firmware/src` into the build |