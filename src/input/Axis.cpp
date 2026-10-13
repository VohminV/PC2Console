#include "pc2c/input/Axis.h"

#include <cmath>

namespace pc2c::input {
namespace {

float applyCurve(float v, float gamma, float sens) {
  if (v == 0.0f) return 0.0f;
  float m = std::fabs(v);
  float c = std::pow(m, gamma) * sens;
  if (c > 1.0f) c = 1.0f;
  return v < 0 ? -c : c;
}

}  // namespace

Vec2 StickFilter::process(std::int16_t rx, std::int16_t ry) {
  float nx = static_cast<float>(rx) / 32768.0f;
  float ny = static_cast<float>(ry) / 32768.0f;
  if (nx > 1.0f) nx = 1.0f;
  if (nx < -1.0f) nx = -1.0f;
  if (ny > 1.0f) ny = 1.0f;
  if (ny < -1.0f) ny = -1.0f;

  float dz = cfg_.deadzone;
  if (dz < 0.0f) dz = 0.0f;
  if (dz > 0.5f) dz = 0.5f;

  Vec2 out{0.0f, 0.0f};
  if (cfg_.mode == DeadzoneMode::kRadial) {
    float m = std::sqrt(nx * nx + ny * ny);
    if (m >= dz && m > 1e-6f) {
      float scaled = (m - dz) / (1.0f - dz);  // 0..~1
      float k = scaled / m;
      out.x = nx * k;
      out.y = ny * k;
    }
  } else {
    auto axial = [&](float v) {
      float a = std::fabs(v);
      if (a < dz) return 0.0f;
      float s = (a - dz) / (1.0f - dz);
      return v < 0 ? -s : s;
    };
    out.x = axial(nx);
    out.y = axial(ny);
  }

  // Diagonal guard: a fully deflected diagonal has raw magnitude >1;
  // renormalize so the game never sees more than 1.0.
  float mo = std::sqrt(out.x * out.x + out.y * out.y);
  if (mo > 1.0f) {
    out.x /= mo;
    out.y /= mo;
  }

  if (cfg_.invertX) out.x = -out.x;
  if (cfg_.invertY) out.y = -out.y;
  out.x = applyCurve(out.x, cfg_.curveGamma, cfg_.sensitivity);
  out.y = applyCurve(out.y, cfg_.curveGamma, cfg_.sensitivity);

  // Optional light smoothing (documented latency cost, default off).
  int w = cfg_.smoothWindow;
  if (w < 0) w = 0;
  if (w > 4) w = 4;
  if (w >= 2) {
    histX_[nx_ % 4] = out.x;
    histY_[nx_ % 4] = out.y;
    nx_++;
    int n = nx_ < w ? nx_ : w;
    float sx = 0.0f, sy = 0.0f;
    for (int i = 0; i < n; ++i) {
      sx += histX_[i];
      sy += histY_[i];
    }
    out.x = sx / n;
    out.y = sy / n;
  } else {
    nx_ = 0;
  }
  return out;
}

float processTrigger(std::uint8_t raw, float threshold01) {
  float t = static_cast<float>(raw) / 255.0f;
  if (t < threshold01) return 0.0f;
  if (threshold01 >= 1.0f) return 0.0f;
  float v = (t - threshold01) / (1.0f - threshold01);
  if (v > 1.0f) v = 1.0f;
  return v;
}

}  // namespace pc2c::input
