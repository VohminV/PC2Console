#pragma once
#include <string>
#include <vector>

#include "pc2c/core/Json.h"

namespace pc2c::formats {

// Explicit decoder registry: detection is separated from decode capability.
// No format is claimed supported unless a real decoder path exists.
struct Capability {
  bool detect = false;
  bool validate = false;
  bool extract = false;
  bool convert = false;
  bool repack = false;
};

struct FormatInfo {
  std::string id;        // stable id, e.g. "bmp", "png", "torque-dts"
  std::string kind;      // image/audio/model/shader/config/script/scene/...
  std::vector<std::string> extensions;  // lower-case with dot
  std::string magic;     // short magic label or "" if ext-only
  std::string status;    // "supported" | "unsupported" | "unknown"
  Capability caps;
  std::string note;  // why + what is missing
};

// Builtin table: mirrors what pc2c can actually do in this build.
// BMP/TGA decode+convert = supported; PNG/JPEG/DDS detect-only; Torque
// text refs = supported (reference extraction); Torque binary = unsupported.
std::vector<FormatInfo> builtinFormats();

const FormatInfo* findByExtension(const std::vector<FormatInfo>& reg,
                                  const std::string& extLower);
const FormatInfo* findByMagic(const std::vector<FormatInfo>& reg, const std::string& magic);

json::Value registryToJson(const std::vector<FormatInfo>& reg);

}  // namespace pc2c::formats
