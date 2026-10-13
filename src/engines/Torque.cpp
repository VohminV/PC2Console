#include "pc2c/engines/Torque.h"

#include <algorithm>

namespace pc2c::engines::torque {

Version detect(const std::vector<std::string>& engineSignals) {
  int torqueFiles = 0;
  for (const auto& s : engineSignals)
    if (s.find("torque-candidate:") != std::string::npos && ++torqueFiles >= 3) break;
  if (torqueFiles >= 3)
    return {"torque?", "medium", "torque artifacts (.dso/.mis/.ter/.dif/.dts)"};
  return {"unknown", "low", "no torque file triple"};
}

bool isTorqueText(const std::string& extLower) {
  return extLower == ".cs" || extLower == ".gui" || extLower == ".mis";
}

std::vector<std::string> referencedExtensions() {
  return {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".dts", ".dsq", ".dso",
          ".cs",   ".gui", ".mis",  ".ter", ".dif", ".wav", ".ogg", ".hlsl",
          ".fx",   ".uft", ".ml",   ".h"};
}

namespace {
// Lower-case ASCII copy for extension matching (tokens keep original case).
std::string lowerAscii(const std::string& s) {
  std::string o = s;
  for (auto& ch : o)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return o;
}
bool endsWith(const std::string& s, const std::string& suf) {
  if (suf.size() > s.size()) return false;
  return s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}
// Token characters allowed inside a path token.
bool isTok(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
         c == '_' || c == '-' || c == '.' || c == '/' || c == '\\' || c == ':' || c == '$';
}
}  // namespace

std::vector<Ref> extractRefs(const std::string& text) {
  std::vector<Ref> out;
  // Cap input to 2MB to bound runtime on huge files.
  const std::size_t cap = 2ULL * 1024 * 1024;
  const std::string view = text.size() > cap ? text.substr(0, cap) : text;
  const auto exts = referencedExtensions();
  std::size_t pos = 0;
  int line = 1;
  // Track line numbers: precompute line starts.
  std::vector<std::size_t> lineStart{0};
  for (std::size_t i = 0; i < view.size(); ++i)
    if (view[i] == '\n') lineStart.push_back(i + 1);
  auto lineOf = [&](std::size_t p) {
    return static_cast<int>(std::upper_bound(lineStart.begin(), lineStart.end(), p) -
                            lineStart.begin());
  };
  (void)pos;
  (void)line;
  std::size_t i = 0;
  while (i < view.size()) {
    // 1) exec("path") / exec(path) — Torque include primitive.
    if ((view.compare(i, 5, "exec(") == 0 || view.compare(i, 5, "exec ") == 0) &&
        i + 5 < view.size()) {
      std::size_t q1 = view.find('"', i);
      std::size_t nl = view.find('\n', i);
      if (q1 != std::string::npos && (nl == std::string::npos || q1 < nl)) {
        std::size_t q2 = view.find('"', q1 + 1);
        if (q2 != std::string::npos && (nl == std::string::npos || q2 < nl) && q2 > q1 + 1) {
          std::string tok = view.substr(q1 + 1, q2 - q1 - 1);
          if (!tok.empty() && tok.size() < 512) {
            Ref r;
            r.target = tok;
            r.basis = "exec";
            r.line = lineOf(i);
            out.push_back(std::move(r));
          }
          i = q2 + 1;
          continue;
        }
      }
    }
    // 2) bare tokens with resource extensions, e.g. "art/shapes/x.dts".
    if (isTok(view[i])) {
      std::size_t j = i;
      while (j < view.size() && isTok(view[j]) && j - i < 512) ++j;
      std::string tok = view.substr(i, j - i);
      std::string low = lowerAscii(tok);
      for (const auto& e : exts) {
        if (endsWith(low, e) && tok.size() > e.size() + 1) {
          // Must contain at least one alnum before the extension (avoid ".cs" alone).
          Ref r;
          r.target = tok;
          // datablock-ish shape refs often appear as "shapeName = \"...dts\"".
          r.basis = "ref-ext";
          r.line = lineOf(i);
          out.push_back(std::move(r));
          break;
        }
      }
      i = j;
      continue;
    }
    ++i;
  }
  // Deduplicate identical (target,line,basis) to keep graphs readable.
  std::sort(out.begin(), out.end(), [](const Ref& a, const Ref& b) {
    if (a.line != b.line) return a.line < b.line;
    if (a.target != b.target) return a.target < b.target;
    return a.basis < b.basis;
  });
  out.erase(std::unique(out.begin(), out.end(),
                        [](const Ref& a, const Ref& b) {
                          return a.line == b.line && a.target == b.target && a.basis == b.basis;
                        }),
            out.end());
  if (out.size() > 5000) out.resize(5000);
  return out;
}

json::Value refsToJson(const std::vector<Ref>& refs) {
  json::Value::Array arr;
  for (const auto& r : refs) {
    json::Value o = json::Value::Obj();
    o.set("target", json::Value::Str(r.target));
    o.set("basis", json::Value::Str(r.basis));
    o.set("line", json::Value::Int(r.line));
    arr.push_back(std::move(o));
  }
  return json::Value::Arr(std::move(arr));
}

}  // namespace pc2c::engines::torque
