#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/discovery/Scanner.h"

namespace pc2c::analysis {

struct ResourceEntry {
  std::string rel;
  std::uint64_t size = 0;
  std::string kind;        // image/audio/model/shader/archive/config/...
  std::string format;      // "bmp"/"tga"/"png"/"dds"/"wav"/"zip"/...
  std::string confidence;  // "confirmed" | "probable" | "unknown"
  std::string basis;       // why: "magic:BM+header-ok", "ext-only", ...
  bool convertible = false;
  std::string convertNote;  // reason when not convertible
};

struct Inventory {
  std::vector<ResourceEntry> resources;
  std::uint64_t convertibleBytes = 0;
  std::uint64_t totalBytes = 0;
  std::vector<std::string> warnings;
};

// Validates resource headers (bounded reads, no execution).
// Unknown/proprietary formats are kept as-is with confidence=unknown.
Inventory buildInventory(const std::string& root, const discovery::ScanResult& scan,
                         std::uint64_t maxProbeBytes);

}  // namespace pc2c::analysis
