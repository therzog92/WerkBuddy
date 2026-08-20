#pragma once

#include <cstddef>
#include <cstdint>

namespace wp {
namespace app {

/** Heard on ESP-NOW within this window counts as nearby. */
constexpr uint32_t kPresenceMs = 20000;
constexpr uint32_t kPresenceBeaconMs = 10000;

bool dnd();
void set_dnd(bool on);

/** Saved peer index helpers (-1 if unknown). */
int peer_index(const char * id);
bool peer_present(const char * id);
bool peer_present_idx(int idx);
bool peer_remote_dnd(const char * id);

/** Subtitle for peer rows: nullptr if nearby and not busy. */
const char * peer_presence_subtitle(int idx, char * buf, size_t buf_n);

/** False + short reason when page/game/doodle should be blocked. */
bool peer_contact_ok(int idx, char * why, size_t why_n);
bool peer_contact_ok_id(const char * id, char * why, size_t why_n);

void note_peer_presence(const char * id, const char * name, bool remote_dnd);
void broadcast_presence();
void presence_tick();
void presence_init();
void presence_on_remove_peer(int removed_idx);

}  // namespace app
}  // namespace wp
