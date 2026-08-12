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
 * Ephemeral STA: join saved network → SNTP → set system time / clock_offset → disconnect.
 * Restores SoftAP for ESP-NOW afterward. Returns true on success.
 */
bool sync_time(char * err, size_t err_n);

}  // namespace wifi_jobs
}  // namespace wp
