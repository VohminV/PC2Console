#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "pc2c/core/Status.h"

namespace pc2c::fs {

struct Entry {
  std::filesystem::path full;
  std::string rel;  // generic (forward slashes), relative to scan root
  std::uint64_t size = 0;
  bool isDir = false;
};

struct WalkResult {
  std::vector<Entry> entries;
  bool truncated = false;
  std::uint64_t totalBytes = 0;
  std::vector<std::string> errors;
};

// Safe recursive walk: never follows symlinks, skips permission-denied,
// honors file/total caps. Never executes anything.
WalkResult walk(const std::filesystem::path& root, std::uint64_t maxFiles,
                std::uint64_t maxTotalBytes, std::vector<std::string>* errors = nullptr);

Status readFileBounded(const std::filesystem::path& path, std::vector<std::uint8_t>& out,
                       std::uint64_t maxBytes);
Status readFileHead(const std::filesystem::path& path, std::vector<std::uint8_t>& out,
                    std::size_t headBytes);

// Atomic write: temp file + rename. Creates parent dirs.
Status atomicWriteFile(const std::filesystem::path& path, const void* data, std::size_t size);
Status atomicWriteFile(const std::filesystem::path& path, const std::string& text);

// Joins root+rel, rejecting absolute paths and ".." escapes (path traversal guard).
std::optional<std::filesystem::path> joinSafe(const std::filesystem::path& root,
                                              const std::string& rel);

Status ensureDir(const std::filesystem::path& dir);

std::string lower(std::string s);
std::string extensionLower(const std::filesystem::path& p);
std::string genericRel(const std::filesystem::path& root, const std::filesystem::path& full);

}  // namespace pc2c::fs
