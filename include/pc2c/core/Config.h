#pragma once
#include <cstdint>
#include <string>

namespace pc2c {

// Tool identity, written into every machine-readable artifact.
inline constexpr const char* kToolName = "pc2c";
inline constexpr const char* kToolVersion = "0.1.0";
inline constexpr int kModelVersion = 1;
inline constexpr int kManifestVersion = 1;

struct GlobalOptions {
  std::string input;
  std::string output;
  std::string target;    // "xbox360" | "ps3" | ""
  std::string profile;   // asset profile name
  std::uint32_t maxTextureSize = 0;  // 0 = use profile default
  bool dryRun = false;
  bool overwrite = false;
  bool verbose = false;
  std::string format = "text";  // "text" | "json"
  bool probeTools = false;      // doctor --probe-tools: may execute known SDK binaries
};

struct Limits {
  std::uint64_t maxFileBytes = 512ULL * 1024 * 1024;  // single file cap
  std::uint64_t maxTotalBytes = 32ULL * 1024 * 1024 * 1024;
  std::size_t maxFiles = 500000;
  std::size_t maxPeBytes = 256ULL * 1024 * 1024;
  std::size_t maxImageBytes = 256ULL * 1024 * 1024;
};

}  // namespace pc2c
