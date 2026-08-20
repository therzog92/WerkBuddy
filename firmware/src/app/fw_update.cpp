#include "app/fw_update.h"

/* PC sim: firmware upload runs on the device's SoftAP only. No-op here. */

namespace wp {
namespace fw_update {

bool start() { return false; }
void poll() {}
void stop() {}

bool finished(char * err, size_t err_n) {
  if (err && err_n) err[0] = 0;
  return false;
}

const char * ssid() { return nullptr; }
const char * pass() { return nullptr; }
const char * url() { return nullptr; }

}  // namespace fw_update
}  // namespace wp