#include "pc2c/graph/Deps.h"

#include <algorithm>
#include <map>

#include "pc2c/core/Fs.h"
#include "pc2c/engines/Torque.h"

namespace pc2c::graph {

namespace {
std::string lowerAscii(std::string s) {
  for (auto& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}
std::string normSlashes(std::string s) {
  for (auto& c : s)
    if (c == '\\') c = '/';
  return s;
}
std::string basenameOf(const std::string& rel) {
  auto p = rel.find_last_of('/');
  return p == std::string::npos ? rel : rel.substr(p + 1);
}
bool endsWithStr(const std::string& s, const std::string& suf) {
  if (suf.size() > s.size()) return false;
  return s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}
}  // namespace

Graph buildGraph(const std::string& root, const discovery::ScanResult& scan,
                 const analysis::Inventory& inv, const Limits& limits) {
  (void)limits;
  Graph g;
  std::map<std::string, const analysis::ResourceEntry*> byRel;
  for (const auto& e : inv.resources) byRel[lowerAscii(normSlashes(e.rel))] = &e;
  // basename -> rels (for suffix/basename resolution).
  std::map<std::string, std::vector<std::string>> byBase;
  for (const auto& f : scan.files) {
    Node n;
    n.rel = f.rel;
    n.kind = discovery::kindToString(f.kind);
    auto it = byRel.find(lowerAscii(normSlashes(f.rel)));
    n.format = (it != byRel.end()) ? it->second->format : "";
    g.nodes.push_back(std::move(n));
    byBase[lowerAscii(basenameOf(normSlashes(f.rel)))].push_back(f.rel);
  }
  auto resolve = [&](const std::string& fromRel, const std::string& rawTok) -> std::string {
    std::string t = lowerAscii(normSlashes(rawTok));
    // Strip Torque variable prefixes ($, %, .) and leading ./.
    while (!t.empty() && (t[0] == '$' || t[0] == '%' || t[0] == '.' || t[0] == '/')) t.erase(0, 1);
    if (t.empty()) return "";
    // 1) exact relative match.
    auto it = byRel.find(t);
    if (it != byRel.end()) return it->second->rel;
    // 2) suffix match: game file ends with token (e.g. token "client/ui/x.png").
    for (const auto& f : scan.files) {
      std::string fl = lowerAscii(normSlashes(f.rel));
      if (fl.size() > t.size() && endsWithStr(fl, t) &&
          fl[fl.size() - t.size() - 1] == '/')
        return f.rel;
    }
    // 3) basename match (weakest, still useful for flat mods).
    std::string base = basenameOf(t);
    auto ib = byBase.find(base);
    if (ib != byBase.end() && ib->second.size() == 1) return ib->second[0];
    (void)fromRel;
    return "";
  };

  const std::uint64_t kMaxTextBytes = 2ULL * 1024 * 1024;
  for (const auto& f : scan.files) {
    std::string ext = fs::extensionLower(std::filesystem::path(f.rel));
    if (!engines::torque::isTorqueText(ext)) continue;
    if (f.size > kMaxTextBytes) {
      g.warnings.push_back("skipped-oversize-text: " + f.rel);
      continue;
    }
    g.filesScanned++;
    std::vector<std::uint8_t> raw;
    std::filesystem::path full = std::filesystem::path(root) / f.rel;
    if (!fs::readFileBounded(full, raw, kMaxTextBytes).ok) {
      g.warnings.push_back("unreadable-text: " + f.rel);
      continue;
    }
    std::string text(raw.begin(), raw.end());
    // Reject binary-looking text (NUL bytes) — not a Torque text file.
    if (text.find('\0') != std::string::npos) {
      g.warnings.push_back("binary-text-skipped: " + f.rel);
      continue;
    }
    g.textFiles++;
    auto refs = engines::torque::extractRefs(text);
    for (const auto& r : refs) {
      Edge e;
      e.from = f.rel;
      e.basis = r.basis;
      e.line = r.line;
      std::string hit = resolve(f.rel, r.target);
      if (!hit.empty()) {
        e.to = hit;
        e.resolved = true;
        g.resolved++;
      } else {
        e.to = r.target;
        e.resolved = false;
        g.unresolved++;
      }
      g.edges.push_back(std::move(e));
    }
  }
  std::sort(g.edges.begin(), g.edges.end(), [](const Edge& a, const Edge& b) {
    if (a.from != b.from) return a.from < b.from;
    return a.line < b.line;
  });
  if (g.edges.size() > 20000) {
    g.warnings.push_back("edge-cap-20000-truncated");
    g.edges.resize(20000);
  }
  return g;
}

json::Value graphToJson(const Graph& g) {
  json::Value o = json::Value::Obj();
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("nodes", json::Value::Int(static_cast<std::int64_t>(g.nodes.size())));
  o.set("edges", json::Value::Int(static_cast<std::int64_t>(g.edges.size())));
  o.set("files_scanned", json::Value::Int(static_cast<std::int64_t>(g.filesScanned)));
  o.set("text_files", json::Value::Int(static_cast<std::int64_t>(g.textFiles)));
  o.set("resolved", json::Value::Int(static_cast<std::int64_t>(g.resolved)));
  o.set("unresolved", json::Value::Int(static_cast<std::int64_t>(g.unresolved)));
  json::Value::Array ea;
  const std::size_t cap = std::min<std::size_t>(g.edges.size(), 5000);
  for (std::size_t i = 0; i < cap; ++i) {
    const auto& e = g.edges[i];
    json::Value v = json::Value::Obj();
    v.set("from", json::Value::Str(e.from));
    v.set("to", json::Value::Str(e.to));
    v.set("basis", json::Value::Str(e.basis));
    v.set("line", json::Value::Int(e.line));
    v.set("resolved", json::Value::Bool(e.resolved));
    ea.push_back(std::move(v));
  }
  o.set("edges_sample", json::Value::Arr(std::move(ea)));
  o.set("edges_truncated_note",
        json::Value::Str(g.edges.size() > cap ? "sample of 5000 shown" : "complete"));
  json::Value::Array wa;
  for (const auto& w : g.warnings) wa.push_back(json::Value::Str(w));
  o.set("warnings", json::Value::Arr(std::move(wa)));
  return o;
}

}  // namespace pc2c::graph
