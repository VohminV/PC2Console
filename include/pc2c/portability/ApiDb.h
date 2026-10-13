#pragma once
#include <string>
#include <vector>

namespace pc2c::portability {

// Compatibility verdicts (see docs/PORTING_LIMITATIONS.md).
// NOTE: an import entry proves the symbol is in the import table, NOT that
// the game actually calls it. Reports must carry that caveat.
struct ApiVerdict {
  std::string category;  // SUPPORTED|CONVERTIBLE|REQUIRES_SOURCE|REQUIRES_RESEARCH|UNSUPPORTED|UNKNOWN
  std::string reason;
  std::string remediation;  // manual action, may be empty
};

ApiVerdict classifyDll(const std::string& dllLower, const std::string& target);
ApiVerdict classifyCpuArch(const std::string& arch, const std::string& target);
ApiVerdict classifyDirectx(const std::string& dxHint, const std::string& target);

struct EngineGuess {
  std::string engine;      // e.g. "unity?", "unreal?", "unknown"
  std::string confidence;  // "high"|"medium"|"low"
  std::string basis;
};
EngineGuess guessEngine(const std::vector<std::string>& signals,
                        const std::vector<std::string>& importDlls);

}  // namespace pc2c::portability
