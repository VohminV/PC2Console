#pragma once
#include <cstdint>
#include <utility>

namespace pc2c::input {

enum class DeadzoneMode { kRadial, kAxial };

// Per-stick analog pipeline config. Defaults match XDK constants
// (LEFT_THUMB_DEADZONE 7849, RIGHT 8689, TRIGGER_THRESHOLD 30).
struct AxisConfig {
  float deadzone = 7849.0f / 32767.0f;  // normalized 0..0.5
  DeadzoneMode mode = DeadzoneMode::kRadial;
  float sensitivity = 1.0f;  // post-curve gain, clamped to [-1,1]
  bool invertX = false;
  bool invertY = false;
  float curveGamma = 1.0f;  // 1 = linear; >1 = finer near center
  int smoothWindow = 0;     // 0 = off; 1..4 = moving average (adds latency!)
  float triggerThreshold = 30.0f / 255.0f;
};

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

class StickFilter {
 public:
  explicit StickFilter(AxisConfig cfg = {}) : cfg_(cfg) {}
  void setConfig(const AxisConfig& c) {
    cfg_ = c;
    nx_ = 0;
  }
  const AxisConfig& config() const { return cfg_; }
  // Raw int16 pair -> processed [-1,1] pair. Diagonals are magnitude-clamped
  // so full diagonal deflection never exceeds 1.0 (no artificial speed boost).
  Vec2 process(std::int16_t rx, std::int16_t ry);

 private:
  AxisConfig cfg_;
  float histX_[4] = {};
  float histY_[4] = {};
  int nx_ = 0;
};

// Single trigger byte -> 0..1 with threshold (below => 0, rescaled above).
float processTrigger(std::uint8_t raw, float threshold01);

}  // namespace pc2c::input
