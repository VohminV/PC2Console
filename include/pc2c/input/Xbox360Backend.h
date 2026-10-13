#pragma once
#include <memory>

#include "pc2c/input/Backend.h"

namespace pc2c {
namespace input {

// Console backend, declared for all platforms but IMPLEMENTED ONLY in the
// XDK-toolchain translation unit (src/input/xbox360/Xbox360Backend.cpp,
// compiled with _XBOX). Uses solely confirmed XDK 21256 interfaces:
// XInputGetState/SetState (xbox.h), XINPUT_STATE/GAMEPAD/VIBRATION
// (xinputdefs.h), ERROR_DEVICE_NOT_CONNECTED (winerror.h).
// On host builds this factory returns nullptr with a reason (no fake backend).
std::unique_ptr<IInputBackend> createXbox360Backend();

}  // namespace input
}  // namespace pc2c
