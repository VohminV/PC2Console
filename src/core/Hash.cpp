#include "pc2c/core/Hash.h"

#include <cstdio>
#include <cstring>

namespace pc2c::hash {
namespace {

struct Sha256Ctx {
  std::uint32_t h[8];
  std::uint64_t total = 0;
  std::uint8_t buf[64];
  std::size_t bufLen = 0;
};

inline std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void compress(Sha256Ctx& c, const std::uint8_t* p) {
  static const std::uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
      0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
      0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
      0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
      0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
      0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
      0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
      0xc67178f2};
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i)
    w[i] = (std::uint32_t(p[i * 4]) << 24) | (std::uint32_t(p[i * 4 + 1]) << 16) |
           (std::uint32_t(p[i * 4 + 2]) << 8) | std::uint32_t(p[i * 4 + 3]);
  for (int i = 16; i < 64; ++i) {
    std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  std::uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3], e = c.h[4], f = c.h[5],
                 g = c.h[6], hh = c.h[7];
  for (int i = 0; i < 64; ++i) {
    std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    std::uint32_t ch = (e & f) ^ (~e & g);
    std::uint32_t t1 = hh + S1 + ch + k[i] + w[i];
    std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    std::uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
    std::uint32_t t2 = S0 + maj;
    hh = g;
    g = f;
    f = e;
    e = d + t1;
    d = cc;
    cc = b;
    b = a;
    a = t1 + t2;
  }
  c.h[0] += a;
  c.h[1] += b;
  c.h[2] += cc;
  c.h[3] += d;
  c.h[4] += e;
  c.h[5] += f;
  c.h[6] += g;
  c.h[7] += hh;
}

void update(Sha256Ctx& c, const std::uint8_t* data, std::size_t n) {
  c.total += n;
  while (n > 0) {
    std::size_t take = 64 - c.bufLen;
    if (take > n) take = n;
    std::memcpy(c.buf + c.bufLen, data, take);
    c.bufLen += take;
    data += take;
    n -= take;
    if (c.bufLen == 64) {
      compress(c, c.buf);
      c.bufLen = 0;
    }
  }
}

void final(Sha256Ctx& c, std::uint8_t out[32]) {
  std::uint64_t bitLen = c.total * 8;
  std::uint8_t pad = 0x80;
  update(c, &pad, 1);
  std::uint8_t zero = 0;
  while (c.bufLen != 56) update(c, &zero, 1);
  std::uint8_t len[8];
  for (int i = 0; i < 8; ++i) len[i] = static_cast<std::uint8_t>((bitLen >> (56 - 8 * i)) & 0xff);
  // Append length without affecting total semantics: compress directly.
  std::uint8_t block[64];
  std::memcpy(block, c.buf, 56);
  std::memcpy(block + 56, len, 8);
  compress(c, block);
  for (int i = 0; i < 8; ++i) {
    out[i * 4] = static_cast<std::uint8_t>((c.h[i] >> 24) & 0xff);
    out[i * 4 + 1] = static_cast<std::uint8_t>((c.h[i] >> 16) & 0xff);
    out[i * 4 + 2] = static_cast<std::uint8_t>((c.h[i] >> 8) & 0xff);
    out[i * 4 + 3] = static_cast<std::uint8_t>(c.h[i] & 0xff);
  }
}

}  // namespace

std::string sha256Hex(const std::uint8_t* data, std::size_t size) {
  Sha256Ctx c;
  c.h[0] = 0x6a09e667;
  c.h[1] = 0xbb67ae85;
  c.h[2] = 0x3c6ef372;
  c.h[3] = 0xa54ff53a;
  c.h[4] = 0x510e527f;
  c.h[5] = 0x9b05688c;
  c.h[6] = 0x1f83d9ab;
  c.h[7] = 0x5be0cd19;
  if (size > 0 && data != nullptr) update(c, data, size);
  if (size == 0) { /* hash of empty input still finalized below */ }
  std::uint8_t out[32];
  final(c, out);
  char hex[65];
  for (int i = 0; i < 32; ++i) std::snprintf(hex + 2 * i, 3, "%02x", out[i]);
  return std::string(hex, 64);
}

std::optional<std::string> fileSha256(const std::string& path, std::uint64_t maxBytes) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return std::nullopt;
  Sha256Ctx c;
  c.h[0] = 0x6a09e667;
  c.h[1] = 0xbb67ae85;
  c.h[2] = 0x3c6ef372;
  c.h[3] = 0xa54ff53a;
  c.h[4] = 0x510e527f;
  c.h[5] = 0x9b05688c;
  c.h[6] = 0x1f83d9ab;
  c.h[7] = 0x5be0cd19;
  std::uint64_t total = 0;
  std::uint8_t buf[65536];
  std::size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
    total += n;
    if (total > maxBytes) {
      std::fclose(f);
      return std::nullopt;
    }
    update(c, buf, n);
  }
  if (std::ferror(f)) {
    std::fclose(f);
    return std::nullopt;
  }
  std::fclose(f);
  std::uint8_t out[32];
  final(c, out);
  char hex[65];
  for (int i = 0; i < 32; ++i) std::snprintf(hex + 2 * i, 3, "%02x", out[i]);
  return std::string(hex, 64);
}

std::uint64_t fnv1a64(const std::uint8_t* data, std::size_t size) {
  std::uint64_t h = 14695981039346656037ULL;
  for (std::size_t i = 0; i < size; ++i) {
    h ^= data[i];
    h *= 1099511628211ULL;
  }
  return h;
}

}  // namespace pc2c::hash
