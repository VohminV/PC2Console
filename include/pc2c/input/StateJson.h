#pragma once
#include "pc2c/core/Json.h"
#include "pc2c/input/Types.h"

namespace pc2c::input {

// Host-side JSON dump of a snapshot (uses pc2c JSON; not available in the
// XDK-toolchain TU, which is why it lives outside Types.h).
json::Value stateToJson(const ControllerState& s);

}  // namespace pc2c::input
