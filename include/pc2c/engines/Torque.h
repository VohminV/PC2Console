#pragma once
#include <string>
#include <vector>

#include "pc2c/core/Json.h"

namespace pc2c::engines::torque {

// First engine adapter: Torque Game Engine Advanced (TGEA).
// Handles TEXT formats only (.cs/.gui/.mis): reference extraction.
// Binary formats (.dts/.dsq/.ter/.dif/.dso) are inventoried, NOT decoded.
struct Version {
  std::string engine;      // e.g. "torque?"
  std::string confidence;  // "medium" | "low"
  std::string basis;
};

Version detect(const std::vector<std::string>& engineSignals);

struct Ref {
  std::string target;  // raw token as written in source
  std::string basis;   // "exec" | "ref-ext" | "datablock"
  int line = 0;        // 1-based
};

// Extract file references from TorqueScript/GUI/Mission text.
// Bounded, pure, no execution: scans for exec("...") + tokens ending with
// known resource extensions. Returns raw tokens for the graph to resolve.
std::vector<Ref> extractRefs(const std::string& text);

// Text sources the adapter understands.
bool isTorqueText(const std::string& extLower);

// Extensions the adapter can reference (detect-only unless noted).
std::vector<std::string> referencedExtensions();

json::Value refsToJson(const std::vector<Ref>& refs);

}  // namespace pc2c::engines::torque
