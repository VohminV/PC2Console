#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pc2c::assets {

// Long side cap in pixels. 0 = keep original size (used by conservative).
struct Profile {
  std::string name;
  std::uint32_t maxLongSide = 0;
  bool genMips = true;
  std::string note;
};

std::vector<Profile> allProfiles();
std::optional<Profile> findProfile(const std::string& name);

// Target dims preserving aspect; never upscales.
void targetDims(std::uint32_t w, std::uint32_t h, std::uint32_t maxLongSide, std::uint32_t& outW,
                std::uint32_t& outH);

}  // namespace pc2c::assets
