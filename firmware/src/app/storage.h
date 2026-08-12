#pragma once

/*
 * Settings persistence.
 * PC sim: key=value text file next to the executable (werkpager_settings.ini).
 * ESP32:  replace the file backend with NVS (namespace "werkpager") — same
 *         keys, same call sites. See docs/ESP32_PORT_PLAN.md Phase 1.
 */

#include <cstddef>

namespace wp {
namespace app {
struct Desk;
}

namespace storage {

/** Load persisted settings into the desk. Returns false when nothing saved yet. */
bool load(app::Desk & desk);
void save(const app::Desk & desk);
/** Wipe all persisted desk settings (+ games blob on device). Used by factory reset. */
void wipe();

/**
 * Active-games blob (opaque bytes from active_games).
 * Device: NVS chunks. PC sim: werkpager_games.bin beside the exe.
 */
bool load_games_blob(void * dst, size_t * len_io);
bool save_games_blob(const void * src, size_t len);

/** Desk timer snapshot (device NVS; sim: optional file / no-op). */
bool load_timer_blob(void * dst, size_t * len_io);
bool save_timer_blob(const void * src, size_t len);

}  // namespace storage
}  // namespace wp
