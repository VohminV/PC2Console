#include "pc2c/analysis/Pe.h"

#include <algorithm>
#include <cstring>

#include "pc2c/core/Fs.h"

namespace pc2c::analysis {
namespace {

struct Cursor {
  const std::uint8_t* d;
  std::size_t n;
  bool u16(std::size_t off, std::uint16_t& v) const {
    if (off + 2 > n) return false;
    v = static_cast<std::uint16_t>(d[off] | (d[off + 1] << 8));
    return true;
  }
  bool u32(std::size_t off, std::uint32_t& v) const {
    if (off + 4 > n) return false;
    v = static_cast<std::uint32_t>(d[off]) | (static_cast<std::uint32_t>(d[off + 1]) << 8) |
        (static_cast<std::uint32_t>(d[off + 2]) << 16) |
        (static_cast<std::uint32_t>(d[off + 3]) << 24);
    return true;
  }
  bool u64(std::size_t off, std::uint64_t& v) const {
    if (off + 8 > n) return false;
    v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(d[off + i]) << (8 * i);
    return true;
  }
};

std::string archName(std::uint16_t m) {
  switch (m) {
    case 0x014c:
      return "x86";
    case 0x8664:
      return "x86-64";
    case 0x01c0:
      return "arm";
    case 0x01c4:
      return "armv7";
    case 0xaa64:
      return "arm64";
    case 0x01f0:
      return "ppc";
    case 0x01f1:
      return "ppc64";
    case 0x0169:
      return "mips16?legacy";
    default: {
      char b[16];
      std::snprintf(b, sizeof(b), "unknown:0x%04x", m);
      return b;
    }
  }
}

std::string linkerHint(std::uint8_t maj, std::uint8_t min) {
  char b[64];
  std::snprintf(b, sizeof(b), "MSVC-linker-%u.%u-hint:", maj, min);
  std::string s = b;
  if (maj < 6)
    s += "pre-VC6-or-unknown";
  else if (maj == 6)
    s += "VC6-era";
  else if (maj == 7)
    s += (min >= 10 ? "VS2003/2005-era" : "VS2002/2003-era");
  else if (maj == 8)
    s += "VS2005-era";
  else if (maj == 9)
    s += "VS2008-era";
  else if (maj == 10)
    s += "VS2010-era";
  else if (maj == 11)
    s += "VS2012-era";
  else if (maj == 12)
    s += "VS2013-era";
  else if (maj == 13)
    s += "VS2015-era?";
  else if (maj == 14)
    s += (min >= 40 ? "VS2022-era?" : (min >= 30 ? "VS2019-era?" : "VS2015/2017-era?"));
  else
    s += "newer-than-known-map";
  return s;
}

// Reads a bounded ASCII string at file offset.
bool readCString(const Cursor& c, std::size_t off, std::size_t cap, std::string& out) {
  out.clear();
  for (std::size_t i = 0; i < cap; ++i) {
    if (off + i >= c.n) return false;
    char ch = static_cast<char>(c.d[off + i]);
    if (ch == 0) return true;
    if (ch < 32 || ch > 126) {
      if (static_cast<unsigned char>(ch) == 0) return true;
      // allow, but stop at clearly binary data
      if (ch < 9 || (ch > 13 && ch < 32) || ch > 126) return false;
    }
    out += ch;
  }
  return false;  // unterminated within cap
}

}  // namespace

std::string guessDirectx(const std::vector<std::string>& importDlls) {
  bool d9 = false, d10 = false, d11 = false, d12 = false, dxgi = false, ddx = false;
  for (const auto& d : importDlls) {
    if (d.find("d3d9") != std::string::npos) d9 = true;
    if (d.find("d3dx9") != std::string::npos) {
      d9 = true;
      ddx = true;
    }
    if (d.find("d3d10") != std::string::npos) d10 = true;
    if (d.find("d3d11") != std::string::npos) d11 = true;
    if (d.find("d3d12") != std::string::npos) d12 = true;
    if (d.find("dxgi") != std::string::npos) dxgi = true;
    if (d.find("d3dcompiler") != std::string::npos) ddx = true;
  }
  if (d12) return "d3d12";
  if (d11 || (dxgi && !d9)) return "d3d11";
  if (d10) return "d3d10";
  if (d9) return ddx ? "d3d9+d3dx" : "d3d9";
  return "none-detected";
}

PeInfo parsePeFile(const std::string& path, std::size_t maxBytes) {
  PeInfo pi;
  std::vector<std::uint8_t> data;
  Status st = fs::readFileBounded(path, data, maxBytes);
  if (!st.ok) {
    pi.error = st.message;
    return pi;
  }
  Cursor c{data.data(), data.size()};
  if (c.n < 64) {
    pi.error = "too small for DOS header";
    return pi;
  }
  if (c.d[0] != 'M' || c.d[1] != 'Z') {
    pi.error = "missing MZ signature";
    return pi;
  }
  std::uint32_t e_lfanew = 0;
  if (!c.u32(0x3C, e_lfanew)) {
    pi.error = "cannot read e_lfanew";
    return pi;
  }
  if (e_lfanew > c.n || e_lfanew + 6 > c.n) {
    pi.error = "e_lfanew out of range (truncated?)";
    return pi;
  }
  if (!(c.d[e_lfanew] == 'P' && c.d[e_lfanew + 1] == 'E' && c.d[e_lfanew + 2] == 0 &&
        c.d[e_lfanew + 3] == 0)) {
    pi.error = "missing PE signature";
    return pi;
  }
  std::size_t fh = e_lfanew + 4;
  std::uint16_t machine = 0, numSections = 0;
  std::uint16_t sizeOpt = 0;
  std::uint32_t characteristics = 0;
  if (!c.u16(fh, machine) || !c.u16(fh + 2, numSections) || !c.u16(fh + 16, sizeOpt) ||
      !c.u32(fh + 18, characteristics)) {
    pi.error = "truncated COFF header";
    return pi;
  }
  pi.machine = machine;
  pi.arch = archName(machine);
  if (numSections > 96) {
    pi.error = "implausible section count";
    return pi;
  }
  std::size_t oh = fh + 20;
  std::uint16_t magic = 0;
  if (!c.u16(oh, magic)) {
    pi.error = "truncated optional header";
    return pi;
  }
  if (magic == 0x20b)
    pi.is64 = true;
  else if (magic == 0x10b)
    pi.is64 = false;
  else {
    pi.error = "unknown optional magic (not PE32/PE32+)";
    return pi;
  }
  if (oh + 2 + 2 > c.n) {
    pi.error = "truncated linker version";
    return pi;
  }
  pi.linkerMajor = c.d[oh + 2];
  pi.linkerMinor = c.d[oh + 3];
  pi.compilerHint = linkerHint(pi.linkerMajor, pi.linkerMinor);

  // Rich marker: search DOS stub area for "Rich" (heuristic only).
  std::size_t stubEnd = e_lfanew < c.n ? e_lfanew : c.n;
  if (stubEnd > 512) stubEnd = 512;
  for (std::size_t i = 0; i + 4 <= stubEnd; ++i) {
    if (c.d[i] == 'R' && c.d[i + 1] == 'i' && c.d[i + 2] == 'c' && c.d[i + 3] == 'h') {
      pi.hasRichMarker = true;
      break;
    }
  }

  std::size_t ddOff = oh + (pi.is64 ? 112 : 96);
  std::uint32_t dd[16][2] = {};
  for (int i = 0; i < 16; ++i) {
    std::uint32_t va = 0, sz = 0;
    if (!c.u32(ddOff + i * 8, va) || !c.u32(ddOff + i * 8 + 4, sz)) {
      pi.error = "truncated data directories";
      return pi;
    }
    dd[i][0] = va;
    dd[i][1] = sz;
  }
  pi.isDotNet = (dd[14][0] != 0 && dd[14][1] != 0);
  pi.isSigned = (dd[4][0] != 0 && dd[4][1] != 0);
  pi.hasDelayImports = (dd[13][0] != 0 && dd[13][1] != 0);
  if (pi.isDotNet) pi.warnings.push_back(".NET COM descriptor present: managed assembly?");
  if (pi.hasDelayImports)
    pi.warnings.push_back("delay-load imports present: not enumerated, see limitation");

  std::size_t sh = oh + sizeOpt;
  for (std::uint16_t i = 0; i < numSections; ++i) {
    std::size_t o = sh + i * 40;
    std::uint32_t vs = 0, va = 0, rs = 0, rp = 0;
    if (!c.u32(o + 8, vs) || !c.u32(o + 12, va) || !c.u32(o + 16, rs) || !c.u32(o + 20, rp)) {
      pi.error = "truncated section headers";
      return pi;
    }
    PeSection s;
    char nm[9] = {};
    std::size_t copyN = std::min<std::size_t>(8, (o + 8 <= c.n) ? 8 : 0);
    if (copyN) std::memcpy(nm, c.d + o, copyN);
    s.name = nm;
    s.vsize = vs;
    s.vaddr = va;
    s.rawSize = rs;
    s.rawPtr = rp;
    if (rp > c.n || rs > c.n) {
      pi.warnings.push_back("section raw range outside read window: " + s.name);
    }
    pi.sections.push_back(s);
  }

  auto rvaToOff = [&](std::uint32_t rva, std::uint32_t& off) -> bool {
    for (const auto& s : pi.sections) {
      std::uint32_t sectSize = s.vsize > s.rawSize ? s.vsize : s.rawSize;
      if (sectSize == 0) continue;
      if (rva >= s.vaddr && rva < s.vaddr + sectSize) {
        std::uint64_t o = static_cast<std::uint64_t>(s.rawPtr) + (rva - s.vaddr);
        if (o > c.n) return false;
        off = static_cast<std::uint32_t>(o);
        return true;
      }
    }
    return false;
  };

  // Exports (dir 0), capped.
  if (dd[0][0] != 0) {
    std::uint32_t off = 0;
    if (rvaToOff(dd[0][0], off) && off + 40 <= c.n) {
      std::uint32_t numNames = 0, addrNames = 0;
      c.u32(off + 24, numNames);
      c.u32(off + 32, addrNames);
      if (numNames > 0 && numNames <= 500 && addrNames != 0) {
        std::uint32_t namesOff = 0;
        if (rvaToOff(addrNames, namesOff)) {
          for (std::uint32_t i = 0; i < numNames; ++i) {
            std::uint32_t nameRva = 0;
            if (!c.u32(namesOff + i * 4, nameRva)) break;
            std::uint32_t nameOff = 0;
            if (!rvaToOff(nameRva, nameOff)) continue;
            std::string nm;
            if (readCString(c, nameOff, 256, nm) && !nm.empty()) pi.exports.push_back(nm);
            if (pi.exports.size() >= 500) break;
          }
        }
      }
    } else {
      pi.warnings.push_back("export directory outside readable range");
    }
  }

  // Imports (dir 1), capped at 20000 entries.
  const std::size_t kMaxImports = 20000;
  if (dd[1][0] != 0) {
    std::uint32_t off = 0;
    if (!rvaToOff(dd[1][0], off)) {
      pi.warnings.push_back("import directory outside readable range");
    } else {
      for (std::uint32_t di = 0;; ++di) {
        std::size_t d = static_cast<std::size_t>(off) + di * 20;
        if (d + 20 > c.n) {
          pi.warnings.push_back("import table truncated at descriptor");
          pi.importsTruncated = true;
          break;
        }
        std::uint32_t ilt = 0, nameRva = 0, iat = 0;
        c.u32(d, ilt);
        c.u32(d + 12, nameRva);
        c.u32(d + 16, iat);
        if (ilt == 0 && nameRva == 0 && iat == 0) break;  // end
        if (di > 512) {
          pi.warnings.push_back("import descriptor count suspicious, stopping");
          pi.importsTruncated = true;
          break;
        }
        std::uint32_t nameOff = 0;
        std::string dll = "?";
        if (rvaToOff(nameRva, nameOff)) {
          std::string nm;
          if (readCString(c, nameOff, 256, nm) && !nm.empty()) dll = nm;
        }
        std::string dllLower = dll;
        std::transform(dllLower.begin(), dllLower.end(), dllLower.begin(), ::tolower);
        pi.importDlls.push_back(dllLower);
        std::uint32_t thunkRva = ilt != 0 ? ilt : iat;
        std::uint32_t thunkOff = 0;
        if (thunkRva == 0 || !rvaToOff(thunkRva, thunkOff)) {
          pi.warnings.push_back("thunk table unreadable for " + dll);
          continue;
        }
        std::size_t step = pi.is64 ? 8 : 4;
        for (std::uint32_t ti = 0;; ++ti) {
          if (pi.imports.size() >= kMaxImports) {
            pi.importsTruncated = true;
            pi.warnings.push_back("import cap (20000) reached");
            break;
          }
          std::size_t to = static_cast<std::size_t>(thunkOff) + ti * step;
          if (to + step > c.n) break;
          bool isOrd = false;
          std::uint64_t val = 0;
          if (pi.is64) {
            c.u64(to, val);
            isOrd = (val & 0x8000000000000000ULL) != 0;
          } else {
            std::uint32_t v32 = 0;
            c.u32(to, v32);
            val = v32;
            isOrd = (v32 & 0x80000000U) != 0;
          }
          if (val == 0) break;  // end of thunk list
          if (isOrd) {
            std::uint32_t ord = static_cast<std::uint32_t>(val & 0xFFFF);
            pi.imports.push_back(dllLower + "!ordinal#" + std::to_string(ord));
          } else {
            std::uint32_t hnRva = static_cast<std::uint32_t>(val & 0xFFFFFFFF);
            std::uint32_t hnOff = 0;
            if (!rvaToOff(hnRva, hnOff) || hnOff + 2 >= c.n) continue;
            std::string fn;
            if (readCString(c, hnOff + 2, 256, fn) && !fn.empty())
              pi.imports.push_back(dllLower + "!" + fn);
          }
          if (ti > 20000) {
            pi.importsTruncated = true;
            break;
          }
        }
        if (pi.importsTruncated) break;
      }
    }
  }

  std::sort(pi.importDlls.begin(), pi.importDlls.end());
  pi.importDlls.erase(std::unique(pi.importDlls.begin(), pi.importDlls.end()),
                      pi.importDlls.end());
  pi.directxHint = guessDirectx(pi.importDlls);
  pi.valid = true;
  return pi;
}

}  // namespace pc2c::analysis
