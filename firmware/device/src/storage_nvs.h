#pragma once
#include "desk.h"

namespace wp {
namespace storage {

bool load(shell::Desk & desk);
void save(const shell::Desk & desk);

}  // namespace storage
}  // namespace wp
