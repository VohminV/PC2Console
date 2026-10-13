#pragma once
#include <memory>
#include <string>

#include "pc2c/input/Types.h"

namespace pc2c {
namespace input {

// Platform interface. Core (manager/mapping/profiles) talks ONLY through it —
// no platform API leaks into pc2c core. Each method documents what
// "unsupported" means instead of faking values.
class IInputBackend {
 public:
  virtual ~IInputBackend() {}
  virtual std::string name() const = 0;
  virtual BackendCaps caps() const = 0;
  // Polls userIndex. Disconnected controller => kDisconnected + connected=false
  // (never zero-filled "ok").
  virtual PollStatus poll(int userIndex, ControllerState& out) = 0;
  // Returns false when the platform/backend has no rumble (no fake success).
  virtual bool setRumble(int userIndex, const RumbleRequest& r) = 0;
};

}  // namespace input
}  // namespace pc2c
