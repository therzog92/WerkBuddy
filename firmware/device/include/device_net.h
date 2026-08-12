#pragma once
/*
 * Device-only net helpers. Declared here so shared UI (BuildSources) can call
 * restore before Scan desks. Implemented in src/espnow_app_link.cpp.
 */

namespace wp {
namespace net {

void link_poll();
void restore_espnow_radio();
const char * own_mac_id();
const char * own_mac_pretty();

}  // namespace net
}  // namespace wp
