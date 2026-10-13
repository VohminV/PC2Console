#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pc2c::hash {

// SHA-256 (public domain style implementation, no dependencies).
std::string sha256Hex(const std::uint8_t* data, std::size_t size);
inline std::string sha256Hex(const std::vector<std::uint8_t>& v) {
  return sha256Hex(v.data(), v.size());
}
inline std::string sha256Hex(const std::string& s) {
  return sha256Hex(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

// Bounded file hash; nullopt on any I/O error or size overflow.
std::optional<std::string> fileSha256(const std::string& path, std::uint64_t maxBytes);

std::uint64_t fnv1a64(const std::uint8_t* data, std::size_t size);

}  // namespace pc2c::hash
