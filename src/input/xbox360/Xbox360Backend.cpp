// Xbox 360 console input backend — XDK-TOOLCHAIN-ONLY translation unit.
//
// Build with the XDK 21256 PPC cross compiler (see samples/xbox360/minplay
// for the verified pattern); host compilers MUST NOT compile this file.
// Uses solely confirmed XDK 21256 interfaces:
//   XInputGetState / XInputSetState .... xbox.h:227-259
//   XINPUT_STATE / GAMEPAD / VIBRATION .. xinputdefs.h:158-179
//   ERROR_DEVICE_NOT_CONNECTED (1167) ... winerror.h:2862
//   XUSER_MAX_COUNT (4) ................. xinputdefs.h:21
#ifndef _XBOX
#error "Xbox360Backend requires the Xbox 360 XDK toolchain (_XBOX)"
#endif

#include <xtl.h>

#include "pc2c/input/Backend.h"
#include "pc2c/input/Xbox360Backend.h"

namespace pc2c {
namespace input {
namespace {

class Xbox360Backend : public IInputBackend {
 public:
  std::string name() const override { return "xbox360-xinput"; }
  BackendCaps caps() const override {
    BackendCaps c;
    c.backend = name();
    c.maxControllers = XUSER_MAX_COUNT;  // 4, verified xinputdefs.h:21
    c.analogTriggers = true;
    c.hasRumble = true;  // XInputSetState, verified xbox.h:253-259
    c.hasStickClicks = true;
    c.detail = "XDK 21256 XInput (GetState/SetState), deadzones xinputdefs.h";
    return c;
  }
  PollStatus poll(int userIndex, ControllerState& out) override {
    out = ControllerState();
    if (userIndex < 0 || userIndex >= XUSER_MAX_COUNT) {
      out.note = "user index out of range 0..3";
      return kError;
    }
    XINPUT_STATE xs;
    ZeroMemory(&xs, sizeof(xs));
    DWORD res = XInputGetState((DWORD)userIndex, &xs);
    if (res == ERROR_SUCCESS) {
      out.connected = true;
      out.packet = xs.dwPacketNumber;
      out.buttons = xs.Gamepad.wButtons;
      out.left.x = xs.Gamepad.sThumbLX;
      out.left.y = xs.Gamepad.sThumbLY;
      out.right.x = xs.Gamepad.sThumbRX;
      out.right.y = xs.Gamepad.sThumbRY;
      out.lt = xs.Gamepad.bLeftTrigger;
      out.rt = xs.Gamepad.bRightTrigger;
      return kOk;
    }
    if (res == ERROR_DEVICE_NOT_CONNECTED) {
      out.note = "disconnected (ERROR_DEVICE_NOT_CONNECTED)";
      return kDisconnected;
    }
    out.note = "XInputGetState error (not faked as data)";
    return kError;
  }
  bool setRumble(int userIndex, const RumbleRequest& r) override {
    if (userIndex < 0 || userIndex >= XUSER_MAX_COUNT) return false;
    XINPUT_VIBRATION v;
    v.wLeftMotorSpeed = (WORD)(r.left * 65535.0f);
    v.wRightMotorSpeed = (WORD)(r.right * 65535.0f);
    return XInputSetState((DWORD)userIndex, &v) == ERROR_SUCCESS;
  }
};

}  // namespace

std::unique_ptr<IInputBackend> createXbox360Backend() {
  return std::unique_ptr<IInputBackend>(new Xbox360Backend());
}

}  // namespace input
}  // namespace pc2c
