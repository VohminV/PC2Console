#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/assets/Profiles.h"
#include "pc2c/core/Config.h"
#include "pc2c/core/Json.h"
#include "pc2c/core/Status.h"

namespace pc2c::assets {

struct ConvertOptions {
  bool overwrite = false;
  bool dryRun = false;
  bool verbose = false;
  std::uint32_t maxTextureSizeOverride = 0;  // 0 = profile default
};

struct FileResult {
  std::string rel;
  std::string status;  // "converted" | "skipped" | "failed" | "unsupported" | "dry-run"
  std::string reason;
  std::uint32_t inW = 0, inH = 0, outW = 0, outH = 0;
  std::uint64_t inBytes = 0, outBytes = 0;
  std::uint64_t gpuIn = 0, gpuOut = 0;
  std::string shaIn, shaOut;
  std::string semantic, filter;
  int mipLevels = 0;
  std::string outFormat;  // "bmp" | "tga"
};

struct DirSummary {
  std::string profile;
  std::uint32_t maxLongSide = 0;
  std::size_t total = 0, converted = 0, skipped = 0, failed = 0, unsupported = 0;
  std::uint64_t inBytes = 0, outBytes = 0, gpuIn = 0, gpuOut = 0;
  std::vector<FileResult> files;
  std::string manifestPath;
};

// Converts supported images (bmp/tga) from inputDir to outputDir, preserving
// relative layout. Unsupported files are recorded, never faked. Idempotent
// via manifest (re-run skips up-to-date outputs unless overwrite=true).
// inputDir itself is never modified.
Status convertDir(const std::string& inputDir, const std::string& outputDir,
                  const Profile& profile, const ConvertOptions& opts, const ::pc2c::Limits& limits,
                  DirSummary& summary);

// Re-decodes outputs and checks them against the manifest.
struct VerifyReport {
  std::size_t checked = 0, ok = 0, mismatch = 0, missing = 0, undecodable = 0;
  std::vector<std::string> problems;
};
Status verifyDir(const std::string& outputDir, const std::string& manifestPath, VerifyReport& rep,
                 const ::pc2c::Limits& limits);

json::Value summaryToJson(const DirSummary& s);
json::Value verifyToJson(const VerifyReport& r);

}  // namespace pc2c::assets
