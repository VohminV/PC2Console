#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/core/Status.h"

namespace pc2c::assets {

// RGBA8 image. Channels are raw data bytes; no silent colorspace conversion.
struct Image {
  std::uint32_t w = 0;
  std::uint32_t h = 0;
  std::vector<std::uint8_t> px;  // w*h*4 RGBA
  bool hasAlpha = false;         // any alpha != 255 observed (or format carries alpha)
  std::string originFormat;      // "bmp" | "tga"
};

enum class Semantic {
  kColor,   // albedo/diffuse/emissive/unknown: bilinear sRGB-agnostic resample
  kNormal,  // normal map: box average + renormalize, never plain color filter
  kMask,    // masks/rough/metal/ao/height: linear box average, no colorspace change
};

std::string semanticToString(Semantic s);
Semantic semanticFromName(const std::string& filenameLower);

struct Limits {
  std::uint32_t maxDim = 16384;
  std::uint64_t maxPixels = 256ULL * 1024 * 1024;
};

// Decoders: validate bounds, reject paletted/RLE-unknown explicitly (no guessing).
Status decodeBmp(const std::vector<std::uint8_t>& data, Image& out, const Limits& lim,
                 std::string* detail = nullptr);
Status decodeTga(const std::vector<std::uint8_t>& data, Image& out, const Limits& lim,
                 std::string* detail = nullptr);

// Encoders: 24-bit when no useful alpha, else 32-bit.
Status encodeBmp(const Image& img, std::vector<std::uint8_t>& out);
Status encodeTga(const Image& img, std::vector<std::uint8_t>& out);

// Resize preserving aspect handled by caller via target dims. No upscaling inside.
Image resize(const Image& src, std::uint32_t dstW, std::uint32_t dstH, Semantic sem,
             std::string& filterUsed);

// Box-downsample mipmap chain, level 0 = base. Returns levels including base.
std::vector<Image> genMipmaps(const Image& base, Semantic sem);

// Estimated GPU bytes for RGBA8 incl. mipmaps (no platform alignment claims).
std::uint64_t estimateGpuBytes(std::uint32_t w, std::uint32_t h, int mipLevels, int bpp = 32);

}  // namespace pc2c::assets
