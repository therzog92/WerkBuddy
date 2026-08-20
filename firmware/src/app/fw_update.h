#pragma once

#include <cstddef>

namespace wp {
namespace fw_update {

/** Start the phone-upload hotspot (SoftAP + captive portal .bin upload). False on setup failure. */
bool start();

/** Pump DNS + HTTP (call from the main loop). No-op when not running. */
void poll();

/** Teardown the SoftAP job and restore the ESP-NOW radio. */
void stop();

/**
 * True once an upload has completed. On success the desk reboots (so this is
 * only reached on failure); fills err with a short reason. Reset after start().
 */
bool finished(char * err, size_t err_n);

/** Hotspot credentials for the on-screen QR / label (nullptr when not running). */
const char * ssid();
const char * pass();
const char * url();

}  // namespace fw_update
}  // namespace wp