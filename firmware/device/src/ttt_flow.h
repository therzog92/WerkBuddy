#pragma once
#include "espnow_link.h"

namespace wp {
namespace shell {

void ttt_on_msg(const net::RxMsg & m);
/** Open TTT (peer pick, invite, waiting, or in-play — mirrors sim go_ttt). */
void go_ttt();
void go_ttt_peer_pick(); /* alias → go_ttt() */
bool ttt_busy();

}  // namespace shell
}  // namespace wp
