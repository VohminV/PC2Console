#include "pc2c/assets/Converter.h"

#include <filesystem>
#include <fstream>

#include "pc2c/assets/Image.h"
#include "pc2c/core/Fs.h"
#include "pc2c/core/Hash.h"
#include "pc2c/core/Logger.h"

namespace pc2c::assets {
namespace {

bool endsWith(const std::string& s, const std::string& suf) {
  if (suf.size() > s.size()) return false;
  return s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

struct ManifestEntry {
  std::string rel, shaIn, shaOut, profile;
  std::uint32_t maxLong = 0;
  std::uint32_t outW = 0, outH = 0;
  std::uint64_t gpuIn = 0, gpuOut = 0;
};

// Loads prior manifest entries keyed by rel. Tolerates missing/corrupt file
// (treated as empty with a warning, never as success).
std::vector<ManifestEntry> loadManifest(const std::string& path, std::string& warn) {
  std::vector<ManifestEntry> out;
  std::ifstream f(path, std::ios::binary);
  if (!f) return out;
  std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::string err;
  auto v = json::Value::parse(text, &err);
  if (!v || !v->isObj()) {
    warn = "existing manifest unreadable (" + err + "), ignoring";
    return out;
  }
  const json::Value* files = v->find("files");
  if (!files || !files->isArr()) return out;
  for (const auto& e : std::get<json::Value::Array>(files->v)) {
    if (!e.isObj()) continue;
    ManifestEntry m;
    const json::Value* r = e.find("rel");
    const json::Value* si = e.find("sha256_in");
    const json::Value* so = e.find("sha256_out");
    const json::Value* p = e.find("profile");
    const json::Value* ml = e.find("max_long_side");
    const json::Value* ow = e.find("out_w");
    const json::Value* oh = e.find("out_h");
    if (!r || !r->isStr() || !si || !si->isStr() || !so || !so->isStr()) continue;
    m.rel = std::get<std::string>(r->v);
    m.shaIn = std::get<std::string>(si->v);
    m.shaOut = std::get<std::string>(so->v);
    if (p && p->isStr()) m.profile = std::get<std::string>(p->v);
    if (ml && ml->isInt()) m.maxLong = static_cast<std::uint32_t>(std::get<std::int64_t>(ml->v));
    if (ow && ow->isInt()) m.outW = static_cast<std::uint32_t>(std::get<std::int64_t>(ow->v));
    if (oh && oh->isInt()) m.outH = static_cast<std::uint32_t>(std::get<std::int64_t>(oh->v));
    const json::Value* gi = e.find("gpu_in_est");
    const json::Value* go = e.find("gpu_out_est");
    if (gi && gi->isInt()) m.gpuIn = static_cast<std::uint64_t>(std::get<std::int64_t>(gi->v));
    if (go && go->isInt()) m.gpuOut = static_cast<std::uint64_t>(std::get<std::int64_t>(go->v));
    out.push_back(std::move(m));
  }
  return out;
}

const ManifestEntry* findEntry(const std::vector<ManifestEntry>& es, const std::string& rel) {
  for (const auto& e : es)
    if (e.rel == rel) return &e;
  return nullptr;
}

}  // namespace

Status convertDir(const std::string& inputDir, const std::string& outputDir,
                  const Profile& profile, const ConvertOptions& opts, const ::pc2c::Limits& limits,
                  DirSummary& summary) {
  summary = DirSummary{};
  summary.profile = profile.name;
  std::uint32_t cap =
      opts.maxTextureSizeOverride != 0 ? opts.maxTextureSizeOverride : profile.maxLongSide;
  summary.maxLongSide = cap;

  std::filesystem::path inRoot(inputDir), outRoot(outputDir);
  std::error_code ec;
  if (!std::filesystem::is_directory(inRoot, ec))
    return Status::Fail("input dir not found: " + inputDir);
  if (!opts.dryRun) {
    Status st = fs::ensureDir(outRoot);
    if (!st.ok) return st;
  }

  std::string manifestWarn;
  std::string manifestPath = (outRoot / "pc2c-manifest.json").string();
  summary.manifestPath = manifestPath;
  std::vector<ManifestEntry> prior =
      opts.overwrite ? std::vector<ManifestEntry>{} : loadManifest(manifestPath, manifestWarn);
  if (!manifestWarn.empty()) Logger::Global().warn(manifestWarn);

  auto walked = fs::walk(inRoot, limits.maxFiles, limits.maxTotalBytes);
  Limits ilim;

  json::Value::Array fileArr;
  for (const auto& e : walked.entries) {
    std::string ext = fs::extensionLower(e.full);
    if (e.isDir) continue;
    if (ext != ".bmp" && ext != ".tga") continue;  // only supported inputs are attempted
    summary.total++;
    FileResult fr;
    fr.rel = e.rel;
    fr.inBytes = e.size;

    std::optional<std::string> shaIn = hash::fileSha256(e.full.string(), limits.maxFileBytes);
    if (!shaIn) {
      fr.status = "failed";
      fr.reason = "cannot-hash-input (too large or unreadable)";
      summary.failed++;
      summary.files.push_back(fr);
      continue;
    }
    fr.shaIn = *shaIn;

    // Idempotency: same input sha + same params + output present => skip.
    auto dst = fs::joinSafe(outRoot, fr.rel);
    if (!dst) {
      fr.status = "failed";
      fr.reason = "unsafe-relative-path";
      summary.failed++;
      summary.files.push_back(fr);
      continue;
    }
    if (!opts.overwrite && !opts.dryRun) {
      const ManifestEntry* pe = findEntry(prior, fr.rel);
      if (pe && pe->shaIn == fr.shaIn && pe->profile == profile.name && pe->maxLong == cap &&
          std::filesystem::exists(*dst, ec)) {
        fr.status = "skipped";
        fr.reason = "up-to-date (manifest match)";
        fr.outW = pe->outW;
        fr.outH = pe->outH;
        fr.shaOut = pe->shaOut;
        fr.gpuIn = pe->gpuIn;
        fr.gpuOut = pe->gpuOut;
        std::error_code ec2;
        fr.outBytes = std::filesystem::file_size(*dst, ec2);
        summary.skipped++;
        summary.inBytes += fr.inBytes;
        summary.outBytes += fr.outBytes;
        summary.gpuIn += fr.gpuIn;
        summary.gpuOut += fr.gpuOut;
        summary.files.push_back(fr);
        continue;
      }
      if (std::filesystem::exists(*dst, ec) && !opts.overwrite) {
        fr.status = "skipped";
        fr.reason = "output-exists (use --overwrite)";
        summary.skipped++;
        summary.files.push_back(fr);
        continue;
      }
    }

    std::vector<std::uint8_t> raw;
    Status rs = fs::readFileBounded(e.full, raw, limits.maxImageBytes);
    if (!rs.ok) {
      fr.status = "failed";
      fr.reason = rs.message;
      summary.failed++;
      summary.files.push_back(fr);
      continue;
    }
    Image img;
    std::string detail;
    Status ds = (ext == ".bmp" ? decodeBmp(raw, img, ilim, &detail)
                               : decodeTga(raw, img, ilim, &detail));
    if (!ds.ok) {
      fr.status = "unsupported";
      fr.reason = ds.message;
      summary.unsupported++;
      summary.files.push_back(fr);
      continue;
    }
    fr.inW = img.w;
    fr.inH = img.h;
    std::string semName = fs::lower(e.full.filename().string());
    Semantic sem = semanticFromName(semName);
    fr.semantic = semanticToString(sem);

    std::uint32_t tw, th;
    targetDims(img.w, img.h, cap, tw, th);
    std::string filter;
    Image out = resize(img, tw, th, sem, filter);
    fr.filter = filter;
    fr.outW = out.w;
    fr.outH = out.h;
    auto mips = profile.genMips ? genMipmaps(out, sem)
                                : std::vector<Image>{out};
    fr.mipLevels = static_cast<int>(mips.size());
    fr.gpuIn = estimateGpuBytes(img.w, img.h, static_cast<int>(genMipmaps(img, sem).size()));
    fr.gpuOut = estimateGpuBytes(out.w, out.h, fr.mipLevels);

    std::vector<std::uint8_t> enc;
    Status es = (ext == ".bmp" ? encodeBmp(out, enc) : encodeTga(out, enc));
    if (!es.ok) {
      fr.status = "failed";
      fr.reason = es.message;
      summary.failed++;
      summary.files.push_back(fr);
      continue;
    }
    fr.outFormat = ext.substr(1);
    fr.outBytes = enc.size();
    fr.shaOut = hash::sha256Hex(enc);

    // Verify by re-decoding before writing (decode check).
    {
      Image chk;
      Status vs = (ext == ".bmp" ? decodeBmp(enc, chk, ilim) : decodeTga(enc, chk, ilim));
      if (!vs.ok || chk.w != out.w || chk.h != out.h) {
        fr.status = "failed";
        fr.reason = "roundtrip-verify-failed: " + vs.message;
        summary.failed++;
        summary.files.push_back(fr);
        continue;
      }
    }

    if (opts.dryRun) {
      fr.status = "dry-run";
      fr.reason = "would-convert " + std::to_string(img.w) + "x" + std::to_string(img.h) +
                  " -> " + std::to_string(out.w) + "x" + std::to_string(out.h);
      summary.files.push_back(fr);
      summary.inBytes += fr.inBytes;
      continue;
    }

    Status ws = fs::atomicWriteFile(*dst, enc.data(), enc.size());
    if (!ws.ok) {
      fr.status = "failed";
      fr.reason = ws.message;
      summary.failed++;
      summary.files.push_back(fr);
      continue;
    }
    fr.status = "converted";
    fr.reason = detail + ";" + filter + ";mips=" + std::to_string(fr.mipLevels);
    summary.converted++;
    summary.inBytes += fr.inBytes;
    summary.outBytes += fr.outBytes;
    summary.gpuIn += fr.gpuIn;
    summary.gpuOut += fr.gpuOut;
    summary.files.push_back(std::move(fr));
  }

  // Reconcile skipped gpu stats minimally (already added bytes; gpu unknown for skips).
  if (!opts.dryRun) {
    json::Value m = json::Value::Obj();
    m.set("pc2c_manifest_version", json::Value::Int(kManifestVersion));
    m.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
    m.set("profile", json::Value::Str(profile.name));
    m.set("max_long_side", json::Value::Int(cap));
    json::Value::Array arr;
    for (const auto& f : summary.files) {
      // Persist only actionable entries; dry-run writes no manifest entries.
      if (f.status != "converted" && f.status != "skipped") continue;
      json::Value o = json::Value::Obj();
      o.set("rel", json::Value::Str(f.rel));
      o.set("status", json::Value::Str(f.status));
      o.set("sha256_in", json::Value::Str(f.shaIn));
      o.set("sha256_out", json::Value::Str(f.shaOut));
      o.set("profile", json::Value::Str(profile.name));
      o.set("max_long_side", json::Value::Int(cap));
      o.set("in_w", json::Value::Int(f.inW));
      o.set("in_h", json::Value::Int(f.inH));
      o.set("out_w", json::Value::Int(f.outW));
      o.set("out_h", json::Value::Int(f.outH));
      o.set("semantic", json::Value::Str(f.semantic));
      o.set("mips", json::Value::Int(f.mipLevels));
      o.set("gpu_in_est", json::Value::Int(f.gpuIn));
      o.set("gpu_out_est", json::Value::Int(f.gpuOut));
      arr.push_back(std::move(o));
    }
    m.set("files", json::Value::Arr(std::move(arr)));
    m.set("totals_in_bytes", json::Value::Int(summary.inBytes));
    m.set("totals_out_bytes", json::Value::Int(summary.outBytes));
    Status ms = fs::atomicWriteFile(manifestPath, m.dump(2));
    if (!ms.ok) return ms;
  }
  if (walked.truncated)
    Logger::Global().warn("input walk truncated; results partial");
  (void)endsWith;
  return Status::Ok();
}

Status verifyDir(const std::string& outputDir, const std::string& manifestPath, VerifyReport& rep,
                 const ::pc2c::Limits& limits) {
  rep = VerifyReport{};
  std::string path = manifestPath.empty() ? (std::filesystem::path(outputDir) / "pc2c-manifest.json").string()
                                          : manifestPath;
  std::ifstream f(path, std::ios::binary);
  if (!f) return Status::Fail("manifest not found: " + path);
  std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::string err;
  auto v = json::Value::parse(text, &err);
  if (!v || !v->isObj()) return Status::Fail("manifest parse error: " + err);
  const json::Value* files = v->find("files");
  if (!files || !files->isArr()) return Status::Fail("manifest has no files[]");
  Limits ilim;
  std::filesystem::path outRoot(outputDir);
  for (const auto& e : std::get<json::Value::Array>(files->v)) {
    if (!e.isObj()) continue;
    const json::Value* r = e.find("rel");
    const json::Value* so = e.find("sha256_out");
    const json::Value* ow = e.find("out_w");
    const json::Value* oh = e.find("out_h");
    if (!r || !r->isStr()) continue;
    std::string rel = std::get<std::string>(r->v);
    rep.checked++;
    auto dst = fs::joinSafe(outRoot, rel);
    if (!dst || !std::filesystem::exists(*dst)) {
      rep.missing++;
      rep.problems.push_back("missing: " + rel);
      continue;
    }
    auto sha = hash::fileSha256(dst->string(), limits.maxFileBytes);
    if (!sha) {
      rep.undecodable++;
      rep.problems.push_back("unreadable: " + rel);
      continue;
    }
    if (so && so->isStr() && *sha != std::get<std::string>(so->v)) {
      rep.mismatch++;
      rep.problems.push_back("sha-mismatch: " + rel);
      continue;
    }
    std::vector<std::uint8_t> raw;
    if (!fs::readFileBounded(*dst, raw, limits.maxImageBytes).ok) {
      rep.undecodable++;
      rep.problems.push_back("unreadable: " + rel);
      continue;
    }
    std::string ext = fs::extensionLower(*dst);
    Image img;
    Status ds = (ext == ".bmp" ? decodeBmp(raw, img, ilim) : decodeTga(raw, img, ilim));
    if (!ds.ok) {
      rep.undecodable++;
      rep.problems.push_back("decode-failed: " + rel + " (" + ds.message + ")");
      continue;
    }
    if (ow && ow->isInt() && img.w != static_cast<std::uint32_t>(std::get<std::int64_t>(ow->v))) {
      rep.mismatch++;
      rep.problems.push_back("dim-mismatch: " + rel);
      continue;
    }
    if (oh && oh->isInt() && img.h != static_cast<std::uint32_t>(std::get<std::int64_t>(oh->v))) {
      rep.mismatch++;
      rep.problems.push_back("dim-mismatch: " + rel);
      continue;
    }
    rep.ok++;
  }
  return Status::Ok();
}

json::Value summaryToJson(const DirSummary& s) {
  json::Value o = json::Value::Obj();
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("profile", json::Value::Str(s.profile));
  o.set("max_long_side", json::Value::Int(s.maxLongSide));
  o.set("total", json::Value::Int(s.total));
  o.set("converted", json::Value::Int(s.converted));
  o.set("skipped", json::Value::Int(s.skipped));
  o.set("failed", json::Value::Int(s.failed));
  o.set("unsupported", json::Value::Int(s.unsupported));
  o.set("in_bytes", json::Value::Int(s.inBytes));
  o.set("out_bytes", json::Value::Int(s.outBytes));
  o.set("gpu_in_bytes_est", json::Value::Int(s.gpuIn));
  o.set("gpu_out_bytes_est", json::Value::Int(s.gpuOut));
  json::Value::Array arr;
  for (const auto& f : s.files) {
    json::Value e = json::Value::Obj();
    e.set("rel", json::Value::Str(f.rel));
    e.set("status", json::Value::Str(f.status));
    e.set("reason", json::Value::Str(f.reason));
    e.set("in_w", json::Value::Int(f.inW));
    e.set("in_h", json::Value::Int(f.inH));
    e.set("out_w", json::Value::Int(f.outW));
    e.set("out_h", json::Value::Int(f.outH));
    e.set("in_bytes", json::Value::Int(f.inBytes));
    e.set("out_bytes", json::Value::Int(f.outBytes));
    e.set("sha256_in", json::Value::Str(f.shaIn));
    e.set("sha256_out", json::Value::Str(f.shaOut));
    e.set("semantic", json::Value::Str(f.semantic));
    e.set("filter", json::Value::Str(f.filter));
    e.set("mips", json::Value::Int(f.mipLevels));
    arr.push_back(std::move(e));
  }
  o.set("files", json::Value::Arr(std::move(arr)));
  return o;
}

json::Value verifyToJson(const VerifyReport& r) {
  json::Value o = json::Value::Obj();
  o.set("checked", json::Value::Int(r.checked));
  o.set("ok", json::Value::Int(r.ok));
  o.set("mismatch", json::Value::Int(r.mismatch));
  o.set("missing", json::Value::Int(r.missing));
  o.set("undecodable", json::Value::Int(r.undecodable));
  json::Value::Array arr;
  for (const auto& p : r.problems) arr.push_back(json::Value::Str(p));
  o.set("problems", json::Value::Arr(std::move(arr)));
  return o;
}

}  // namespace pc2c::assets
