#include "pc2c/assets/Image.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pc2c::assets {
namespace {

std::uint16_t rd16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
std::uint32_t rd32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
void wr16(std::vector<std::uint8_t>& o, std::uint16_t v) {
  o.push_back(static_cast<std::uint8_t>(v & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
}
void wr32(std::vector<std::uint8_t>& o, std::uint32_t v) {
  o.push_back(static_cast<std::uint8_t>(v & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
}

bool dimsOk(std::uint32_t w, std::uint32_t h, const Limits& lim, std::string& err) {
  if (w == 0 || h == 0) {
    err = "zero dimension";
    return false;
  }
  if (w > lim.maxDim || h > lim.maxDim) {
    err = "dimension exceeds limit (possible corrupt header)";
    return false;
  }
  std::uint64_t px = static_cast<std::uint64_t>(w) * h;
  if (px > lim.maxPixels) {
    err = "pixel count exceeds limit (possible corrupt header)";
    return false;
  }
  return true;
}

}  // namespace

std::string semanticToString(Semantic s) {
  switch (s) {
    case Semantic::kColor:
      return "color";
    case Semantic::kNormal:
      return "normal";
    case Semantic::kMask:
      return "mask";
  }
  return "color";
}

Semantic semanticFromName(const std::string& n) {
  // Order matters: normal markers first.
  const char* normalKeys[] = {"normal", "nrm", "nmap", "_n.", "_n_", "(n)", "norm"};
  for (auto k : normalKeys)
    if (n.find(k) != std::string::npos) return Semantic::kNormal;
  const char* maskKeys[] = {"mask", "rough", "metal", "_ao", "ao_", "occlusion",
                            "spec",  "gloss", "height", "bump", "opacity",
                            "emissive_mask", "cavity"};
  for (auto k : maskKeys)
    if (n.find(k) != std::string::npos) return Semantic::kMask;
  return Semantic::kColor;
}

Status decodeBmp(const std::vector<std::uint8_t>& data, Image& out, const Limits& lim,
                 std::string* detail) {
  auto note = [&](const std::string& s) {
    if (detail) *detail = s;
  };
  if (data.size() < 54) return Status::Fail("bmp: truncated header");
  if (!(data[0] == 'B' && data[1] == 'M')) return Status::Fail("bmp: missing BM magic");
  std::uint32_t dataOff = rd32(data.data() + 10);
  std::uint32_t dib = rd32(data.data() + 14);
  if (dib != 40 && dib != 108 && dib != 124)
    return Status::Fail("bmp: unsupported DIB size (only BITMAPINFOHEADER/V4/V5)");
  std::int32_t wi = static_cast<std::int32_t>(rd32(data.data() + 18));
  std::int32_t hi = static_cast<std::int32_t>(rd32(data.data() + 22));
  bool topDown = false;
  if (hi < 0) {
    topDown = true;
    hi = -hi;
  }
  if (wi <= 0 || hi <= 0) return Status::Fail("bmp: bad dimensions");
  std::uint16_t planes = rd16(data.data() + 26);
  std::uint16_t bpp = rd16(data.data() + 28);
  std::uint32_t comp = rd32(data.data() + 30);
  if (planes != 1) return Status::Fail("bmp: planes must be 1");
  if (bpp != 24 && bpp != 32) {
    if (bpp <= 8)
      return Status::Fail("bmp: paletted (<=8bpp) not supported, kept as-is");
    if (bpp == 16) return Status::Fail("bmp: 16bpp not supported, kept as-is");
    return Status::Fail("bmp: unsupported bpp");
  }
  if (comp != 0 && comp != 3) {
    if (comp == 1 || comp == 2) return Status::Fail("bmp: RLE compression not supported");
    return Status::Fail("bmp: unsupported compression");
  }
  if (comp == 3 && bpp != 32 && bpp != 16)
    return Status::Fail("bmp: BI_BITFIELDS only for 16/32bpp");
  std::uint32_t w = static_cast<std::uint32_t>(wi), h = static_cast<std::uint32_t>(hi);
  std::string derr;
  if (!dimsOk(w, h, lim, derr)) return Status::Fail("bmp: " + derr);
  std::uint32_t rowBits = bpp * w;
  std::uint32_t rowStride = ((rowBits + 31) / 32) * 4;
  std::uint64_t need = static_cast<std::uint64_t>(dataOff) + static_cast<std::uint64_t>(rowStride) * h;
  if (need > data.size()) return Status::Fail("bmp: pixel data truncated");
  if (dataOff < 14 + dib) return Status::Fail("bmp: bad pixel offset");

  // BI_BITFIELDS masks (default to standard BGRA layout).
  std::uint32_t rMask = 0x00ff0000, gMask = 0x0000ff00, bMask = 0x000000ff, aMask = 0x00000000;
  if (comp == 3) {
    if (data.size() < 14 + 52) return Status::Fail("bmp: truncated bitfields");
    rMask = rd32(data.data() + 14 + 40);
    gMask = rd32(data.data() + 14 + 44);
    bMask = rd32(data.data() + 14 + 48);
  }
  auto shiftOf = [](std::uint32_t m) {
    int s = 0;
    while (m && !(m & 1)) {
      m >>= 1;
      ++s;
    }
    return s;
  };
  auto bitsOf = [](std::uint32_t m) {
    int b = 0;
    while (m & 1) {
      m >>= 1;
      ++b;
    }
    return b;
  };
  int rS = shiftOf(rMask), gS = shiftOf(gMask), bS = shiftOf(bMask), aS = shiftOf(aMask ? aMask : 1);
  int rB = bitsOf(rMask >> rS), gB = bitsOf(gMask >> gS), bB = bitsOf(bMask >> bS);
  int aB = aMask ? bitsOf(aMask >> aS) : 0;
  auto scale = [](std::uint32_t v, int bits) -> std::uint8_t {
    if (bits == 0) return 0;
    if (bits >= 8) return static_cast<std::uint8_t>(v >> (bits - 8));
    if (bits == 0) return 0;
    std::uint32_t max = (1u << bits) - 1;
    return static_cast<std::uint8_t>((v * 255 + max / 2) / max);
  };

  out.w = w;
  out.h = h;
  out.originFormat = "bmp";
  out.px.resize(static_cast<std::size_t>(w) * h * 4);
  out.hasAlpha = false;
  for (std::uint32_t y = 0; y < h; ++y) {
    std::uint32_t srcY = topDown ? y : (h - 1 - y);
    const std::uint8_t* row = data.data() + dataOff + srcY * rowStride;
    for (std::uint32_t x = 0; x < w; ++x) {
      std::uint8_t r, g, b, a = 255;
      if (bpp == 24) {
        b = row[x * 3];
        g = row[x * 3 + 1];
        r = row[x * 3 + 2];
      } else {
        if (comp == 0) {
          b = row[x * 4];
          g = row[x * 4 + 1];
          r = row[x * 4 + 2];
          a = row[x * 4 + 3];
        } else {
          std::uint32_t v = rd32(row + x * 4);
          r = scale((v & rMask) >> rS, rB);
          g = scale((v & gMask) >> gS, gB);
          b = scale((v & bMask) >> bS, bB);
          a = aMask ? scale((v & aMask) >> aS, aB) : 255;
        }
      }
      std::size_t o = (static_cast<std::size_t>(y) * w + x) * 4;
      out.px[o] = r;
      out.px[o + 1] = g;
      out.px[o + 2] = b;
      out.px[o + 3] = a;
      if (a != 255) out.hasAlpha = true;
    }
  }
  note(bpp == 24 ? "bmp24-opaque" : (out.hasAlpha ? "bmp32-alpha" : "bmp32-opaque"));
  return Status::Ok();
}

Status decodeTga(const std::vector<std::uint8_t>& data, Image& out, const Limits& lim,
                 std::string* detail) {
  auto note = [&](const std::string& s) {
    if (detail) *detail = s;
  };
  if (data.size() < 18) return Status::Fail("tga: truncated header");
  std::uint8_t idLen = data[0], cmap = data[1], type = data[2];
  std::uint16_t w = rd16(data.data() + 12), h = rd16(data.data() + 14);
  std::uint8_t bpp = data[16], desc = data[17];
  if (cmap != 0 || type == 1 || type == 9)
    return Status::Fail("tga: colormapped types not supported, kept as-is");
  if (type != 2 && type != 10 && type != 3 && type != 11)
    return Status::Fail("tga: unsupported image type");
  if (type == 3 || type == 11) {
    if (bpp != 8) return Status::Fail("tga: grayscale must be 8bpp");
  } else if (bpp != 24 && bpp != 32) {
    if (bpp == 16) return Status::Fail("tga: 16bpp not supported, kept as-is");
    return Status::Fail("tga: unsupported bpp");
  }
  std::string derr;
  if (!dimsOk(w, h, lim, derr)) return Status::Fail("tga: " + derr);
  bool topLeft = (desc & 0x20) != 0;
  std::size_t off = 18 + idLen;
  if (off > data.size()) return Status::Fail("tga: bad id length");
  std::uint32_t ch = (type == 3 || type == 11) ? 1 : (bpp == 32 ? 4 : 3);
  std::uint64_t pxCount = static_cast<std::uint64_t>(w) * h;

  out.w = w;
  out.h = h;
  out.originFormat = "tga";
  out.px.resize(static_cast<std::size_t>(pxCount) * 4);
  out.hasAlpha = (ch == 4);
  bool alphaUsed = false;

  auto putPx = [&](std::uint64_t idx, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                   std::uint8_t a) {
    std::uint32_t x = static_cast<std::uint32_t>(idx % w);
    std::uint32_t y = static_cast<std::uint32_t>(idx / w);
    std::uint32_t dstY = topLeft ? y : (h - 1 - y);
    std::size_t o = (static_cast<std::size_t>(dstY) * w + x) * 4;
    out.px[o] = r;
    out.px[o + 1] = g;
    out.px[o + 2] = b;
    out.px[o + 3] = a;
    if (a != 255) alphaUsed = true;
  };

  if (type == 2 || type == 3) {
    std::uint64_t need = off + pxCount * ch;
    if (need > data.size()) return Status::Fail("tga: pixel data truncated");
    const std::uint8_t* p = data.data() + off;
    for (std::uint64_t i = 0; i < pxCount; ++i) {
      std::uint8_t r, g, b, a = 255;
      if (ch == 1) {
        r = g = b = p[i];
      } else {
        b = p[i * ch];
        g = p[i * ch + 1];
        r = p[i * ch + 2];
        if (ch == 4) a = p[i * ch + 3];
      }
      putPx(i, r, g, b, a);
    }
    note(type == 3 ? "tga-gray8" : (bpp == 24 ? "tga24" : "tga32"));
  } else {
    // RLE packets with overflow guards.
    std::size_t p = off;
    std::uint64_t i = 0;
    while (i < pxCount) {
      if (p >= data.size()) return Status::Fail("tga: rle truncated");
      std::uint8_t hdr = data[p++];
      std::uint32_t count = (hdr & 0x7F) + 1;
      if (i + count > pxCount) return Status::Fail("tga: rle run overflows image");
      if (hdr & 0x80) {
        if (p + ch > data.size()) return Status::Fail("tga: rle pixel truncated");
        std::uint8_t r, g, b, a = 255;
        if (ch == 1)
          r = g = b = data[p];
        else {
          b = data[p];
          g = data[p + 1];
          r = data[p + 2];
          if (ch == 4) a = data[p + 3];
        }
        p += ch;
        for (std::uint32_t k = 0; k < count; ++k) putPx(i++, r, g, b, a);
      } else {
        if (p + static_cast<std::size_t>(count) * ch > data.size())
          return Status::Fail("tga: rle raw truncated");
        for (std::uint32_t k = 0; k < count; ++k) {
          std::uint8_t r, g, b, a = 255;
          if (ch == 1)
            r = g = b = data[p];
          else {
            b = data[p];
            g = data[p + 1];
            r = data[p + 2];
            if (ch == 4) a = data[p + 3];
          }
          p += ch;
          putPx(i++, r, g, b, a);
        }
      }
    }
    note("tga-rle");
  }
  if (ch != 4) out.hasAlpha = false;
  if (ch == 4 && !alphaUsed) out.hasAlpha = false;  // opaque 32-bit -> encode 24-bit
  return Status::Ok();
}

Status encodeBmp(const Image& img, std::vector<std::uint8_t>& out) {
  if (img.w == 0 || img.h == 0 || img.px.size() != static_cast<std::size_t>(img.w) * img.h * 4)
    return Status::Fail("encode: bad image");
  bool use32 = img.hasAlpha;
  std::uint32_t rowStride = ((img.w * (use32 ? 32u : 24u) + 31) / 32) * 4;
  std::uint32_t pixSize = rowStride * img.h;
  out.clear();
  out.reserve(54 + pixSize);
  out.push_back('B');
  out.push_back('M');
  wr32(out, 54 + pixSize);
  wr16(out, 0);
  wr16(out, 0);
  wr32(out, 54);
  wr32(out, 40);
  wr32(out, img.w);
  wr32(out, img.h);  // bottom-up
  wr16(out, 1);
  wr16(out, use32 ? 32 : 24);
  wr32(out, 0);
  wr32(out, pixSize);
  wr32(out, 2835);
  wr32(out, 2835);
  wr32(out, 0);
  wr32(out, 0);
  std::vector<std::uint8_t> pad(rowStride, 0);
  for (std::uint32_t y = 0; y < img.h; ++y) {
    std::uint32_t srcY = img.h - 1 - y;
    const std::uint8_t* row = img.px.data() + static_cast<std::size_t>(srcY) * img.w * 4;
    if (use32) {
      for (std::uint32_t x = 0; x < img.w; ++x) {
        out.push_back(row[x * 4 + 2]);
        out.push_back(row[x * 4 + 1]);
        out.push_back(row[x * 4]);
        out.push_back(row[x * 4 + 3]);
      }
    } else {
      for (std::uint32_t x = 0; x < img.w; ++x) {
        out.push_back(row[x * 4 + 2]);
        out.push_back(row[x * 4 + 1]);
        out.push_back(row[x * 4]);
      }
      std::size_t rem = rowStride - img.w * 3;
      for (std::size_t i = 0; i < rem; ++i) out.push_back(0);
    }
  }
  return Status::Ok();
}

Status encodeTga(const Image& img, std::vector<std::uint8_t>& out) {
  if (img.w == 0 || img.h == 0 || img.w > 16384 || img.h > 16384)
    return Status::Fail("encode: bad dims");
  if (img.px.size() != static_cast<std::size_t>(img.w) * img.h * 4)
    return Status::Fail("encode: bad image");
  bool use32 = img.hasAlpha;
  out.clear();
  out.reserve(18 + static_cast<std::size_t>(img.w) * img.h * (use32 ? 4 : 3));
  out.insert(out.end(), {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  wr16(out, static_cast<std::uint16_t>(img.w));
  wr16(out, static_cast<std::uint16_t>(img.h));
  out.push_back(use32 ? 32 : 24);
  out.push_back(use32 ? 0x28 : 0x20);  // top-left origin, 8 alpha bits when 32
  for (std::uint32_t y = 0; y < img.h; ++y) {
    const std::uint8_t* row = img.px.data() + static_cast<std::size_t>(y) * img.w * 4;
    for (std::uint32_t x = 0; x < img.w; ++x) {
      out.push_back(row[x * 4 + 2]);
      out.push_back(row[x * 4 + 1]);
      out.push_back(row[x * 4]);
      if (use32) out.push_back(row[x * 4 + 3]);
    }
  }
  return Status::Ok();
}

namespace {

inline std::uint8_t clamp8(float v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return static_cast<std::uint8_t>(v + 0.5f);
}

Image resizeColor(const Image& s, std::uint32_t dw, std::uint32_t dh) {
  Image d;
  d.w = dw;
  d.h = dh;
  d.originFormat = s.originFormat;
  d.px.resize(static_cast<std::size_t>(dw) * dh * 4);
  float sx = static_cast<float>(s.w) / dw, sy = static_cast<float>(s.h) / dh;
  for (std::uint32_t y = 0; y < dh; ++y) {
    float fy = (y + 0.5f) * sy - 0.5f;
    int y0 = static_cast<int>(std::floor(fy));
    float ty = fy - y0;
    y0 = std::max(0, std::min(static_cast<int>(s.h) - 1, y0));
    int y1 = std::min(static_cast<int>(s.h) - 1, y0 + 1);
    for (std::uint32_t x = 0; x < dw; ++x) {
      float fx = (x + 0.5f) * sx - 0.5f;
      int x0 = static_cast<int>(std::floor(fx));
      float tx = fx - x0;
      x0 = std::max(0, std::min(static_cast<int>(s.w) - 1, x0));
      int x1 = std::min(static_cast<int>(s.w) - 1, x0 + 1);
      for (int c = 0; c < 4; ++c) {
        float a = s.px[(static_cast<std::size_t>(y0) * s.w + x0) * 4 + c];
        float b = s.px[(static_cast<std::size_t>(y0) * s.w + x1) * 4 + c];
        float cc = s.px[(static_cast<std::size_t>(y1) * s.w + x0) * 4 + c];
        float dd = s.px[(static_cast<std::size_t>(y1) * s.w + x1) * 4 + c];
        float v = a * (1 - tx) * (1 - ty) + b * tx * (1 - ty) + cc * (1 - tx) * ty + dd * tx * ty;
        d.px[(static_cast<std::size_t>(y) * dw + x) * 4 + c] = clamp8(v);
      }
    }
  }
  // Recompute alpha usefulness.
  d.hasAlpha = false;
  for (std::size_t i = 3; i < d.px.size(); i += 4) {
    if (d.px[i] != 255) {
      d.hasAlpha = true;
      break;
    }
  }
  return d;
}

// Box average over source footprint; normals renormalized.
Image resizeBox(const Image& s, std::uint32_t dw, std::uint32_t dh, bool renormalize) {
  Image d;
  d.w = dw;
  d.h = dh;
  d.originFormat = s.originFormat;
  d.px.resize(static_cast<std::size_t>(dw) * dh * 4);
  for (std::uint32_t y = 0; y < dh; ++y) {
    std::uint32_t y0 = y * s.h / dh, y1 = (y + 1) * s.h / dh;
    if (y1 <= y0) y1 = y0 + 1;
    for (std::uint32_t x = 0; x < dw; ++x) {
      std::uint32_t x0 = x * s.w / dw, x1 = (x + 1) * s.w / dw;
      if (x1 <= x0) x1 = x0 + 1;
      double sum[4] = {};
      double n = 0;
      for (std::uint32_t sy = y0; sy < y1; ++sy)
        for (std::uint32_t sx = x0; sx < x1; ++sx) {
          const std::uint8_t* p = s.px.data() + (static_cast<std::size_t>(sy) * s.w + sx) * 4;
          if (renormalize) {
            // Average in vector space, alpha separately.
            sum[0] += (p[0] / 255.0 * 2.0 - 1.0);
            sum[1] += (p[1] / 255.0 * 2.0 - 1.0);
            sum[2] += (p[2] / 255.0 * 2.0 - 1.0);
            sum[3] += p[3];
          } else {
            sum[0] += p[0];
            sum[1] += p[1];
            sum[2] += p[2];
            sum[3] += p[3];
          }
          n += 1;
        }
      std::uint8_t* q = d.px.data() + (static_cast<std::size_t>(y) * dw + x) * 4;
      if (renormalize) {
        double nx = sum[0] / n, ny = sum[1] / n, nz = sum[2] / n;
        double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len < 1e-6) {
          nx = 0;
          ny = 0;
          nz = 1;
          len = 1;
        }
        q[0] = clamp8(static_cast<float>((nx / len * 0.5 + 0.5) * 255.0));
        q[1] = clamp8(static_cast<float>((ny / len * 0.5 + 0.5) * 255.0));
        q[2] = clamp8(static_cast<float>((nz / len * 0.5 + 0.5) * 255.0));
        q[3] = clamp8(static_cast<float>(sum[3] / n));
      } else {
        q[0] = clamp8(static_cast<float>(sum[0] / n));
        q[1] = clamp8(static_cast<float>(sum[1] / n));
        q[2] = clamp8(static_cast<float>(sum[2] / n));
        q[3] = clamp8(static_cast<float>(sum[3] / n));
      }
    }
  }
  d.hasAlpha = false;
  for (std::size_t i = 3; i < d.px.size(); i += 4) {
    if (d.px[i] != 255) {
      d.hasAlpha = true;
      break;
    }
  }
  return d;
}

}  // namespace

Image resize(const Image& src, std::uint32_t dstW, std::uint32_t dstH, Semantic sem,
             std::string& filterUsed) {
  if (dstW == 0) dstW = 1;
  if (dstH == 0) dstH = 1;
  if (dstW == src.w && dstH == src.h) {
    filterUsed = "copy (already at target)";
    return src;
  }
  // Never upscale: clamp (caller should too; double-guard here).
  if (dstW > src.w) dstW = src.w;
  if (dstH > src.h) dstH = src.h;
  bool down = (dstW < src.w || dstH < src.h);
  if (sem == Semantic::kNormal) {
    filterUsed = "box-average+renormalize (normal map)";
    return resizeBox(src, dstW, dstH, true);
  }
  if (sem == Semantic::kMask) {
    filterUsed = "box-average-linear (mask/data, no colorspace change)";
    return resizeBox(src, dstW, dstH, false);
  }
  if (down && (src.w >= dstW * 4 || src.h >= dstH * 4)) {
    // Large downscale: bilinear alone aliases; box prefilter is safer for color too.
    filterUsed = "box-prefilter+bilinear (color, large ratio)";
    std::uint32_t mw = std::max(dstW * 2, 1u), mh = std::max(dstH * 2, 1u);
    if (mw < src.w || mh < src.h) {
      Image mid = resizeBox(src, std::min(mw, src.w), std::min(mh, src.h), false);
      return resizeColor(mid, dstW, dstH);
    }
  }
  filterUsed = "bilinear (color)";
  return resizeColor(src, dstW, dstH);
}

std::vector<Image> genMipmaps(const Image& base, Semantic sem) {
  std::vector<Image> chain;
  chain.push_back(base);
  std::uint32_t w = base.w, h = base.h;
  while (w > 1 || h > 1) {
    std::uint32_t nw = w > 1 ? w / 2 : 1;
    std::uint32_t nh = h > 1 ? h / 2 : 1;
    std::string f;
    Image m = (sem == Semantic::kColor) ? resizeColor(chain.back(), nw, nh)
                                        : resizeBox(chain.back(), nw, nh, sem == Semantic::kNormal);
    (void)f;
    chain.push_back(std::move(m));
    w = nw;
    h = nh;
    if (chain.size() > 16) break;  // hard guard
  }
  return chain;
}

std::uint64_t estimateGpuBytes(std::uint32_t w, std::uint32_t h, int mipLevels, int bpp) {
  std::uint64_t total = 0;
  std::uint64_t per = static_cast<std::uint64_t>(bpp) / 8;
  for (int i = 0; i < mipLevels; ++i) {
    total += static_cast<std::uint64_t>(w) * h * per;
    if (w > 1) w /= 2;
    if (h > 1) h /= 2;
    if (w == 1 && h == 1 && i > 0) {
      // last 1x1 counted; loop ends naturally
    }
  }
  return total;
}

}  // namespace pc2c::assets
