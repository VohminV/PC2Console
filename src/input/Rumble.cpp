#include "pc2c/input/Rumble.h"

namespace pc2c::input {

void RumbleAdapter::pulse(float left, float right, std::uint64_t durationMs,
                          std::uint64_t nowMs) {
  if (durationMs == 0) {
    stop();
    return;
  }
  left_ = left < 0 ? 0 : (left > 1 ? 1 : left);
  right_ = right < 0 ? 0 : (right > 1 ? 1 : right);
  untilMs_ = nowMs + durationMs;
  active_ = true;
  sentStop_ = false;
}

void RumbleAdapter::stop() {
  active_ = false;
  left_ = right_ = 0.0f;
}

bool RumbleAdapter::update(IInputBackend& backend, int userIndex, std::uint64_t nowMs,
                           bool enabled) {
  if (!backend.caps().hasRumble || !enabled) {
    active_ = false;
    return false;
  }
  if (active_ && nowMs >= untilMs_) {
    active_ = false;
    left_ = right_ = 0.0f;
  }
  if (!active_) {
    if (!sentStop_) {
      sentStop_ = backend.setRumble(userIndex, RumbleRequest{0.0f, 0.0f});
    }
    return sentStop_;
  }
  RumbleRequest r{left_ * gain_, right_ * gain_};
  return backend.setRumble(userIndex, r);
}

}  // namespace pc2c::input
