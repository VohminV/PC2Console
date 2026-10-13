#include "pc2c/analysis/Inventory.h"

#include <filesystem>

#include "pc2c/core/Fs.h"

namespace pc2c::analysis {
namespace {

bool headIs(const std::vector<std::uint8_t>& h, const char* s, std::size_t n) {
  if (h.size() < n) return false;
  for (std::size_t i = 0; i < n; ++i)
    if (h[i] != static_cast<std::uint8_t>(s[i])) return false;
  return true;
}

// Minimal BMP header sanity (full decode happens in assets pipeline).
bool bmpHeaderOk(const std::vector<std::uint8_t>& h, std::string& detail) {
  if (!headIs(h, "BM", 2)) return false;
  if (h.size() < 54) {
    detail = "truncated-bmp-header";
    return false;
  }
  std::uint32_t dib = h[14] | (h[15] << 8) | (h[16] << 16) | (h[17] << 24);
  std::uint16_t bpp = h[28] | (h[29] << 8);
  std::uint32_t comp = h[30] | (h[31] << 8) | (h[32] << 16) | (h[33] << 24);
  if (dib != 40 && dib != 108 && dib != 124) {
    detail = "unsupported-dib-size";
    return false;
  }
  if (bpp != 24 && bpp != 32 && bpp != 8 && bpp != 4 && bpp != 1 && bpp != 16) {
    detail = "unsupported-bpp";
    return false;
  }
  if (comp > 3) {
    detail = "unsupported-compression";
    return false;
  }
  detail = "magic:BM+header-ok";
  return true;
}

bool tgaHeaderSane(const std::vector<std::uint8_t>& h, std::string& detail) {
  if (h.size() < 18) {
    detail = "truncated-tga-header";
    return false;
  }
  std::uint8_t type = h[2], cmap = h[1], bpp = h[16];
  std::uint16_t w = h[12] | (h[13] << 8), hh = h[14] | (h[15] << 8);
  if (w == 0 || hh == 0 || w > 16384 || hh > 16384) {
    detail = "implausible-dims";
    return false;
  }
  if (type != 2 && type != 10 && type != 1 && type != 9 && type != 3 && type != 11) {
    detail = "unknown-tga-type";
    return false;
  }
  if (cmap > 1) {
    detail = "bad-colormap-flag";
    return false;
  }
  if (bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) {
    detail = "unsupported-bpp";
    return false;
  }
  detail = "tga-header-sane";
  return true;
}

}  // namespace

Inventory buildInventory(const std::string& root, const discovery::ScanResult& scan,
                         std::uint64_t maxProbeBytes) {
  Inventory inv;
  for (const auto& f : scan.files) {
    inv.totalBytes += f.size;
    std::string ext = fs::extensionLower(std::filesystem::path(f.rel));
    ResourceEntry e;
    e.rel = f.rel;
    e.size = f.size;
    e.confidence = "unknown";
    e.convertible = false;

    std::string kind = discovery::kindToString(f.kind);
    e.kind = kind;

    if (f.kind == discovery::FileKind::kImage) {
      if (f.magic == "BMP") {
        std::vector<std::uint8_t> head;
        std::string detail;
        bool ok = fs::readFileHead(std::filesystem::path(root) / f.rel, head, 128).ok &&
                  bmpHeaderOk(head, detail);
        e.format = "bmp";
        e.basis = detail.empty() ? "ext-only" : detail;
        e.confidence = ok ? "confirmed" : "probable";
        // 24/32-bit uncompressed convertible; paletted/RLE need decoder check later.
        e.convertible = ok;
        e.convertNote = ok ? "bmp-decode-supported" : "bmp-needs-decode-check";
        if (ok) inv.convertibleBytes += f.size;
      } else if (ext == ".tga") {
        std::vector<std::uint8_t> head;
        std::string detail;
        bool ok = fs::readFileHead(std::filesystem::path(root) / f.rel, head, 64).ok &&
                  tgaHeaderSane(head, detail);
        e.format = "tga";
        e.basis = detail.empty() ? "ext-only" : detail;
        e.confidence = ok ? "confirmed" : "probable";
        e.convertible = ok;
        e.convertNote = ok ? "tga-decode-supported" : "tga-needs-decode-check";
        if (ok) inv.convertibleBytes += f.size;
      } else if (f.magic == "DDS") {
        e.format = "dds";
        e.basis = "magic:DDS";
        e.confidence = "confirmed";
        e.convertible = false;
        e.convertNote =
            "dds-payload-requires-gpu-format-research (block/tiling/swizzle unknown); kept as-is";
      } else if (f.magic == "PNG" || f.magic == "JPEG" || f.magic == "GIF" ||
                 f.magic == "TIFF" || f.magic == "WEBP") {
        e.format = fs::lower(f.magic);
        e.basis = "magic:" + f.magic;
        e.confidence = "confirmed";
        e.convertible = false;
        e.convertNote = "decoder-not-implemented-in-mvp (Phase 3 backlog); kept as-is";
      } else {
        e.format = ext.empty() ? "unknown-image" : ext.substr(1);
        e.basis = "ext-only;no-magic-match";
        e.confidence = "unknown";
        e.convertible = false;
        e.convertNote = "not-identified-as-texture-by-size-or-ext-alone; kept as-is";
      }
    } else if (f.kind == discovery::FileKind::kAudio) {
      e.format = f.magic.empty() ? (ext.empty() ? "unknown-audio" : ext.substr(1))
                                 : fs::lower(f.magic);
      e.basis = f.magic.empty() ? "ext-only" : ("magic:" + f.magic);
      e.confidence = f.magic.empty() ? "probable" : "confirmed";
      e.convertible = false;
      e.convertNote = "audio-pipeline-not-in-mvp; kept as-is";
    } else if (f.kind == discovery::FileKind::kArchive) {
      e.format = f.magic.empty() ? (ext.empty() ? "unknown-container" : ext.substr(1))
                                 : fs::lower(f.magic);
      e.basis = f.magic.empty() ? "ext-only" : ("magic:" + f.magic);
      e.confidence = f.magic.empty() ? "probable" : "confirmed";
      e.convertible = false;
      e.convertNote =
          "container-not-unpacked-automatically (proprietary formats need registered decoder)";
    } else if (f.kind == discovery::FileKind::kModel ||
               f.kind == discovery::FileKind::kShader ||
               f.kind == discovery::FileKind::kConfig) {
      e.format = ext.empty() ? ("unknown-" + kind) : ext.substr(1);
      e.basis = "ext-only";
      e.confidence = "probable";
      e.convertible = false;
      e.convertNote = kind + "-conversion-not-in-mvp; inventoried-only";
    } else {
      e.format = ext.empty() ? "unknown" : ext.substr(1);
      e.basis = f.magic.empty() ? "ext-only" : ("magic:" + f.magic);
      e.confidence = "probable";
      e.convertible = false;
      e.convertNote = "no-registered-decoder; kept as-is";
    }
    (void)maxProbeBytes;
    inv.resources.push_back(std::move(e));
  }
  return inv;
}

}  // namespace pc2c::analysis
