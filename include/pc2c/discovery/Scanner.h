#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/core/Config.h"
#include "pc2c/core/Fs.h"

namespace pc2c::discovery {

enum class FileKind {
  kExecutable,
  kLibrary,
  kArchive,
  kImage,
  kAudio,
  kModel,
  kShader,
  kConfig,
  kOther,
};

std::string kindToString(FileKind k);

struct FileRecord {
  std::string rel;  // generic relative path
  std::uint64_t size = 0;
  FileKind kind = FileKind::kOther;
  std::string magic;  // short signature label, e.g. "MZ", "PNG", "DDS", "PK"
  std::string note;   // detection basis, never a claim of usability
};

struct ScanResult {
  std::string root;
  std::vector<FileRecord> files;
  std::uint64_t totalBytes = 0;
  bool truncated = false;
  std::vector<std::string> errors;
  std::vector<std::string> engineSignals;  // filename-based hints, low confidence
};

// Static, execution-free scan: reads directory entries + small magic heads only.
ScanResult scanGameDir(const std::string& root, const Limits& limits);

}  // namespace pc2c::discovery
