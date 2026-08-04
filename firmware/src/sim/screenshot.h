#pragma once

namespace wp {
namespace sim {

/** Write PNG of the active screen to firmware/sim-out/preview.png (cwd-aware). */
void save_preview_png();

/** Save to firmware/sim-out/<name>.png */
void save_named_png(const char * name);

/** If WERKPAGER_SHOT=1, save after a short settle and quit. */
void maybe_auto_shot_and_quit();

/**
 * If WERKPAGER_GALLERY=1, walk all UI shells, write sim-out/gallery-*.png, then quit.
 * Agent uses this to self-review before showing Tommy.
 */
void maybe_run_gallery_and_quit();

}  // namespace sim
}  // namespace wp
