#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/core/Config.h"
#include "pc2c/core/Json.h"
#include "pc2c/core/Status.h"

namespace pc2c::shaders {

// HLSL -> Xbox 360 microcode via the REAL XDK fxc (no reimplementation).
// Stage is decided by Torque/XDK filename convention (*V.hlsl vertex,
// *P.hlsl pixel); anything else is recorded, never guessed.
struct ShaderOptions {
  bool overwrite = false;
  bool dryRun = false;
  std::string entry = "main";
  std::string vsProfile = "vs_2_0";
  std::string psProfile = "ps_2_0";
};

struct FileResult {
  std::string rel;
  std::string status;  // "compiled" | "skipped" | "failed" | "unsupported" | "dry-run"
  std::string stage;   // "vertex" | "pixel" | "unknown" | "include"
  std::string profile;
  std::uint64_t inBytes = 0, outBytes = 0;
  std::string log;  // capped fxc output on failure (or note)
};

struct DirSummary {
  std::string vsProfile, psProfile, entry, fxcPath;
  std::size_t total = 0, compiled = 0, skipped = 0, failed = 0, unsupported = 0;
  std::uint64_t inBytes = 0, outBytes = 0;
  std::vector<FileResult> files;
  std::string reportPath;
};

Status resolveFxc(std::string& path, std::string& note);
Status convertDir(const std::string& inputDir, const std::string& outputDir,
                  const ShaderOptions& opts, const Limits& limits, DirSummary& summary);
json::Value summaryToJson(const DirSummary& s);

}  // namespace pc2c::shaders
