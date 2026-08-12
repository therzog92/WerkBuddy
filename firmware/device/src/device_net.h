#pragma once
/*
 * Device-only net helpers beyond shared net/link.h (link_init / link_send).
 */

namespace wp {
namespace net {

/** Drain one queued RX frame into app::handle_msg (call from main loop). */
void link_poll();

const char * own_mac_id();
const char * own_mac_pretty();

}  // namespace net
}  // namespace wp
