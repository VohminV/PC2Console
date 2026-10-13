#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pc2c::analysis {

// Bounds-checked, execution-free PE/COFF parser. Never loads or runs the image.
struct PeSection {
  std::string name;
  std::uint32_t vsize = 0;
  std::uint32_t vaddr = 0;
  std::uint32_t rawSize = 0;
  std::uint32_t rawPtr = 0;
};

struct PeInfo {
  bool valid = false;
  std::string error;  // set when !valid
  bool is64 = false;  // PE32+ if true
  std::uint16_t machine = 0;
  std::string arch;  // "x86" | "x86-64" | "arm" | "arm64" | "ppc?" | "hex:0x..."
  std::uint8_t linkerMajor = 0;
  std::uint8_t linkerMinor = 0;
  std::string compilerHint;  // heuristic, labeled as hint
  bool hasRichMarker = false;
  bool isDotNet = false;
  bool isSigned = false;
  bool hasDelayImports = false;
  bool importsTruncated = false;
  std::vector<std::string> importDlls;  // lower-cased dll names
  std::vector<std::string> imports;     // "dll!symbol" or "dll!ordinal#N"
  std::vector<std::string> exports;     // capped
  std::vector<PeSection> sections;
  std::vector<std::string> warnings;
  std::string directxHint;  // e.g. "d3d9", "d3d11", "none-detected"
};

// Parses file at path, reading at most maxBytes. Small capped vectors.
PeInfo parsePeFile(const std::string& path, std::size_t maxBytes);

// DirectX version guess from import dll list (heuristic).
std::string guessDirectx(const std::vector<std::string>& importDlls);

}  // namespace pc2c::analysis
