#pragma once
#include <cstdint>
#include <string>

#include "pc2c/input/Backend.h"

namespace pc2c::input {

// Platform-independent rumble requests with timed pulses. Never claims
// success when the backend has no rumble: active() is false and update()
// is a no-op in that case.
class RumbleAdapter {
 public:
  void setStrength(float masterGain) { gain_ = masterGain < 0 ? 0 : (masterGain > 1 ? 1 : masterGain); }
  // Starts a pulse of durationMs (0 = stop).
  void pulse(float left, float right, std::uint64_t durationMs, std::uint64_t nowMs);
  void stop();
  // Applies current state to the backend; call every frame. Returns true if a
  // rumble command was actually sent (false => unsupported or idle).
  bool update(IInputBackend& backend, int userIndex, std::uint64_t nowMs, bool enabled);
  bool active() const { return active_; }

 private:
  float gain_ = 1.0f;
  float left_ = 0.0f, right_ = 0.0f;
  std::uint64_t untilMs_ = 0;
  bool active_ = false;
  bool sentStop_ = true;
};

}  // namespace pc2c::input
