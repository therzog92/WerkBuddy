#pragma once

/*
 * Desk-to-desk transport.
 * PC sim: sim_link.cpp — a local peer bot plays Will & Alex so every flow can
 *         be exercised without hardware.
 * ESP32:  espnow_link.cpp — real ESP-NOW (Phase 0+). Same two entry points;
 *         received packets are decoded and forwarded to app::handle_msg().
 */

#include "protocol/messages.h"

namespace wp {
namespace net {

void link_init();
void link_send(const proto::Msg & msg);

}  // namespace net
}  // namespace wp
