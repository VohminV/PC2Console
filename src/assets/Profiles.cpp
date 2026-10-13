#include "pc2c/assets/Profiles.h"

#include <algorithm>

namespace pc2c::assets {

std::vector<Profile> allProfiles() {
  return {
      {"conservative", 0, true, "minimal quality change; keeps size, regenerates mip note"},
      {"xbox360-480p", 480, true, "target: 480px long side (Xbox 360 low-mem profile)"},
      {"xbox360-360p", 360, true, "target: 360px long side"},
      {"xbox360-240p", 240, true, "target: 240px long side"},
      {"xbox360-120p", 120, true, "target: 120px long side (extreme)"},
      {"balanced", 1024, true, "moderate reduction, quality-preserving default"},
  };
}

std::optional<Profile> findProfile(const std::string& name) {
  for (const auto& p : allProfiles())
    if (p.name == name) return p;
  return std::nullopt;
}

void targetDims(std::uint32_t w, std::uint32_t h, std::uint32_t maxLongSide, std::uint32_t& outW,
                std::uint32_t& outH) {
  if (w == 0 || h == 0) {
    outW = w;
    outH = h;
    return;
  }
  std::uint32_t longSide = std::max(w, h);
  if (maxLongSide == 0 || longSide <= maxLongSide) {
    outW = w;
    outH = h;  // never upscale
    return;
  }
  double k = static_cast<double>(maxLongSide) / longSide;
  outW = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(w * k));
  outH = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(h * k));
}

}  // namespace pc2c::assets
