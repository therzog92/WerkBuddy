#pragma once

#include <cstddef>

namespace wp {
namespace wifi_jobs {

constexpr int kMaxScan = 16;

struct ScanAp {
  char ssid[33];
  int bars; /* 1..4 */
  bool open;
};

/** Blocking scan; restores SoftAP afterward. Returns count written to out[]. */
int scan(ScanAp * out, int max_out);

/**
 * Verify credentials only: join SSID/pass briefly, then disconnect and restore
 * SoftAP. Returns true when the network accepted the password (associated).
 * On failure fills a short reason ("Could not find network" / "Wrong password").
 */
bool test_join(const char * ssid, const char * pass, char * err, size_t err_n);

/**
 * Ephemeral STA: join saved network → SNTP → set system time / clock_offset → disconnect.
 * Restores SoftAP for ESP-NOW afterward. Returns true on success.
 */
bool sync_time(char * err, size_t err_n);

/**
 * Join saved Wi-Fi and leave STA up (for Updates fetch/install).
 * Caller must call leave_sta() when done. Returns true on success.
 */
bool join_sta(char * err, size_t err_n);

/** Disconnect STA and restore SoftAP / ESP-NOW radio. */
void leave_sta();

}  // namespace wifi_jobs
}  // namespace wp
