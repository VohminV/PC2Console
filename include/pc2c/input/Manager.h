#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "pc2c/core/Json.h"
#include "pc2c/input/Axis.h"
#include "pc2c/input/Backend.h"
#include "pc2c/input/Mapping.h"
#include "pc2c/input/Profiles.h"
#include "pc2c/input/Rumble.h"

namespace pc2c::input {

// Per-frame processed input: connection state, edges, normalized sticks,
// trigger values and resolved action values (buttons 0/1, axes -1..1,
// triggers 0..1).
struct Frame {
  bool connected = false;
  bool changed = false;  // packet changed since last update
  int disconnects = 0;   // total disconnect events seen
  std::uint16_t buttons = 0;
  std::uint16_t pressed = 0;   // rising edges this frame
  std::uint16_t released = 0;  // falling edges this frame
  Vec2 left{0, 0};
  Vec2 right{0, 0};
  float lt = 0.0f;
  float rt = 0.0f;
  // Raw values (pre-processing) for diagnostics; valid only if connected.
  std::int16_t rawLX = 0, rawLY = 0, rawRX = 0, rawRY = 0;
  std::uint8_t rawLT = 0, rawRT = 0;
  std::map<std::string, float> actions;
  json::Value toJson() const;
};

// Owns a backend + profile; applies deadzone/analog/mapping every update.
// Reconnection is tracked (disconnect counter); a reconnected pad resumes
// with fresh edges, never with stale "pressed" state.
class ControllerManager {
 public:
  ControllerManager(std::unique_ptr<IInputBackend> backend, InputProfile profile,
                    int userIndex = 0);
  IInputBackend& backend() { return *backend_; }
  InputProfile& profile() { return profile_; }
  RumbleAdapter& rumble() { return rumble_; }
  const Frame& frame() const { return frame_; }

  void update(std::uint64_t nowMs);
  void setProfile(InputProfile p);

 private:
  std::unique_ptr<IInputBackend> backend_;
  InputProfile profile_;
  RumbleAdapter rumble_;
  int user_ = 0;
  Frame frame_{};
  StickFilter leftFilter_;
  StickFilter rightFilter_;
  std::uint32_t lastPacket_ = 0;
  bool hadState_ = false;
};

}  // namespace pc2c::input
