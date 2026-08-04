#pragma once

/*
 * Settings persistence.
 * PC sim: key=value text file next to the executable (werkpager_settings.ini).
 * ESP32:  replace the file backend with NVS (namespace "werkpager") — same
 *         keys, same call sites. See docs/ESP32_PORT_PLAN.md Phase 1.
 */

namespace wp {
namespace app {
struct Desk;
}

namespace storage {

/** Load persisted settings into the desk. Returns false when nothing saved yet. */
bool load(app::Desk & desk);
void save(const app::Desk & desk);

}  // namespace storage
}  // namespace wp
