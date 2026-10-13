#include "pc2c/shaders/ShaderConvert.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "pc2c/core/Fs.h"
#include "pc2c/core/Logger.h"

namespace pc2c::shaders {
namespace {

std::string getenvStr(const char* n) {
#ifdef _WIN32
  char* buf = nullptr;
  std::size_t len = 0;
  if (_dupenv_s(&buf, &len, n) == 0 && buf) {
    std::string s(buf);
    std::free(buf);
    return s;
  }
  return "";
#else
  const char* v = std::getenv(n);
  return v ? v : "";
#endif
}

// Torque/XDK filename stage rule: base ends with V (vertex) or P (pixel),
// ignoring case and an optional trailing 1_1-style variant suffix.
std::string stageOf(const std::string& filename) {
  std::string base = filename;
  auto dot = base.rfind('.');
  if (dot != std::string::npos) {
    std::string ext = fs::lower(base.substr(dot));
    if (ext == ".h" || ext == ".hlsli" || ext == ".inc") return "include";
    base = base.substr(0, dot);
  }
  while (!base.empty() && (base.back() == '_' || (base.back() >= '0' && base.back() <= '9')))
    base.pop_back();
  if (base.empty()) return "unknown";
  char last = base.back();
  if (last == 'V' || last == 'v') return "vertex";
  if (last == 'P' || last == 'p') return "pixel";
  return "unknown";
}

struct RunResult {
  bool ok = false;
  int exitCode = -1;
  std::string log;
};

RunResult runFxc(const std::string& fxc, const std::string& src, const std::string& profile,
                 const std::string& entry, const std::string& includeDir,
                 const std::string& outFile) {
  RunResult r;
  // `call` prefix: _popen uses `cmd /c`, which misparses leading quotes.
  std::string cmd = "call \"" + fxc + "\" /nologo /T" + profile + " /E" + entry + " /I\"" +
                    includeDir + "\" \"" + src + "\" /Fo\"" + outFile + ".pc2c-tmp\" 2>&1";
#ifdef _WIN32
  FILE* pipe = _popen(cmd.c_str(), "r");
#else
  FILE* pipe = popen(cmd.c_str(), "r");
#endif
  if (!pipe) {
    r.log = "popen failed";
    return r;
  }
  char buf[2048];
  while (std::fgets(buf, sizeof(buf), pipe)) {
    r.log += buf;
    if (r.log.size() > 8192) {
      r.log += "\n[truncated]";
      break;
    }
  }
#ifdef _WIN32
  r.exitCode = _pclose(pipe);
#else
  r.exitCode = pclose(pipe);
#endif
  std::error_code ec;
  std::filesystem::path tmp = std::string(outFile) + ".pc2c-tmp";
  if (r.exitCode == 0 && std::filesystem::exists(tmp, ec) &&
      std::filesystem::file_size(tmp, ec) > 0) {
    std::filesystem::rename(tmp, outFile, ec);
    r.ok = !ec;
    if (ec) r.log += "\nrename failed: " + ec.message();
  } else {
    std::filesystem::remove(tmp, ec);
    r.ok = false;
    if (r.log.empty()) r.log = "fxc failed, no output";
  }
  return r;
}

}  // namespace

Status resolveFxc(std::string& path, std::string& note) {
#ifdef _WIN32
  std::string xedk = getenvStr("XEDK");
  if (xedk.empty()) xedk = "C:\\Program Files (x86)\\Microsoft Xbox 360 SDK";
  path = xedk + "\\bin\\win32\\fxc.exe";
  std::error_code ec;
  if (std::filesystem::is_regular_file(path, ec)) {
    note = "XDK fxc: " + path;
    return Status::Ok();
  }
  return Status::Fail("XDK fxc.exe not found: " + path);
#else
  (void)path;
  (void)note;
  return Status::Fail("Xbox shader conversion is Windows+XDK only");
#endif
}

Status convertDir(const std::string& inputDir, const std::string& outputDir,
                  const ShaderOptions& opts, const Limits& limits, DirSummary& summary) {
  summary = DirSummary{};
  summary.vsProfile = opts.vsProfile;
  summary.psProfile = opts.psProfile;
  summary.entry = opts.entry;
  std::string note;
  Status rs = resolveFxc(summary.fxcPath, note);
  if (!rs.ok) return rs;
  Logger::Global().debug(note);

  std::filesystem::path inRoot(inputDir), outRoot(outputDir);
  std::error_code ec;
  if (!std::filesystem::is_directory(inRoot, ec))
    return Status::Fail("input dir not found: " + inputDir);
  if (!opts.dryRun) {
    Status st = fs::ensureDir(outRoot);
    if (!st.ok) return st;
  }

  auto walked = fs::walk(inRoot, limits.maxFiles, limits.maxTotalBytes);
  for (const auto& e : walked.entries) {
    if (e.isDir) continue;
    std::string ext = fs::extensionLower(e.full);
    if (ext != ".hlsl" && ext != ".hlsli" && ext != ".h" && ext != ".fx") continue;
    summary.total++;
    FileResult fr;
    fr.rel = e.rel;
    fr.inBytes = e.size;
    std::string fn = e.full.filename().string();
    fr.stage = stageOf(fn);
    if (fr.stage == "include") {
      fr.status = "unsupported";
      fr.log = "include file, not a compilable unit";
      summary.unsupported++;
      summary.files.push_back(fr);
      continue;
    }
    if (fr.stage == "unknown") {
      // Stage undecidable from filename: TRY vertex first, then pixel.
      // fxc itself is the oracle — whichever profile compiles wins.
      fr.profile = "";
      fr.log = "";
    } else {
      fr.profile = (fr.stage == "vertex") ? opts.vsProfile : opts.psProfile;
    }
    auto outRelFor = [&](const std::string& stage) {
      return fr.rel + (stage == "vertex" ? ".vso" : ".pso");
    };
    if (opts.dryRun) {
      fr.status = "dry-run";
      if (fr.profile.empty()) fr.profile = opts.vsProfile + "+" + opts.psProfile + " (try both)";
      fr.log = "would compile " + fr.profile;
      summary.files.push_back(fr);
      continue;
    }
    // Candidate (profile, outRel, stage) list: one entry normally, two for unknown.
    struct Cand {
      std::string profile, outRel, stage;
    };
    std::vector<Cand> cands;
    if (fr.stage == "unknown") {
      cands.push_back({opts.vsProfile, outRelFor("vertex"), "vertex"});
      cands.push_back({opts.psProfile, outRelFor("pixel"), "pixel"});
    } else {
      cands.push_back({fr.profile, outRelFor(fr.stage), fr.stage});
    }
    bool done = false;
    std::string bothLogs;
    for (const auto& c : cands) {
      auto dst = fs::joinSafe(outRoot, c.outRel);
      if (!dst) {
        fr.status = "failed";
        fr.log = "unsafe-relative-path";
        summary.failed++;
        summary.files.push_back(fr);
        done = true;
        break;
      }
      // Native separators: XDK fxc (fxc1001) chokes on mixed '/' in OUTPUT paths.
      std::string native = dst->string();
      std::replace(native.begin(), native.end(), '/', '\\');
      std::filesystem::path ndst(native);
      if (std::filesystem::exists(ndst, ec) && !opts.overwrite) {
        fr.status = "skipped";
        fr.log = "output-exists (use --overwrite)";
        fr.stage = c.stage;
        fr.profile = c.profile;
        summary.skipped++;
        summary.files.push_back(fr);
        done = true;
        break;
      }
      // fxc writes directly: ensure the parent dir exists (fxc1001 otherwise).
      std::error_code ec3;
      std::filesystem::create_directories(ndst.parent_path(), ec3);
      std::string incDir = e.full.parent_path().string();
      RunResult rr = runFxc(summary.fxcPath, e.full.string(), c.profile, opts.entry, incDir,
                            ndst.string());
      if (rr.ok) {
        fr.status = "compiled";
        fr.stage = c.stage;
        fr.profile = c.profile;
        fr.log = (fr.stage != stageOf(fn) ? "stage resolved by fxc as " + c.stage : "");
        std::error_code ec2;
        fr.outBytes = std::filesystem::file_size(ndst, ec2);
        summary.compiled++;
        summary.inBytes += fr.inBytes;
        summary.outBytes += fr.outBytes;
        summary.files.push_back(std::move(fr));
        done = true;
        break;
      }
      bothLogs += "[" + c.profile + "] " + rr.log + "\n";
    }
    if (done) continue;
    fr.status = "failed";
    fr.log = bothLogs;
    summary.failed++;
    summary.files.push_back(std::move(fr));
  }

  if (!opts.dryRun) {
    summary.reportPath = (outRoot / "pc2c-shader-report.json").string();
    Status ms = fs::atomicWriteFile(summary.reportPath, summaryToJson(summary).dump(2));
    if (!ms.ok) return ms;
  }
  if (walked.truncated) Logger::Global().warn("input walk truncated; results partial");
  if (summary.failed > 0)
    return Status::Fail(std::to_string(summary.failed) + " shader(s) failed (report written)");
  return Status::Ok();
}

json::Value summaryToJson(const DirSummary& s) {
  json::Value o = json::Value::Obj();
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("fxc", json::Value::Str(s.fxcPath));
  o.set("vs_profile", json::Value::Str(s.vsProfile));
  o.set("ps_profile", json::Value::Str(s.psProfile));
  o.set("entry", json::Value::Str(s.entry));
  o.set("total", json::Value::Int(s.total));
  o.set("compiled", json::Value::Int(s.compiled));
  o.set("skipped", json::Value::Int(s.skipped));
  o.set("failed", json::Value::Int(s.failed));
  o.set("unsupported", json::Value::Int(s.unsupported));
  o.set("in_bytes", json::Value::Int(s.inBytes));
  o.set("out_bytes", json::Value::Int(s.outBytes));
  json::Value::Array arr;
  for (const auto& f : s.files) {
    json::Value e = json::Value::Obj();
    e.set("rel", json::Value::Str(f.rel));
    e.set("status", json::Value::Str(f.status));
    e.set("stage", json::Value::Str(f.stage));
    e.set("profile", json::Value::Str(f.profile));
    e.set("in_bytes", json::Value::Int(f.inBytes));
    e.set("out_bytes", json::Value::Int(f.outBytes));
    e.set("log", json::Value::Str(f.log));
    arr.push_back(std::move(e));
  }
  o.set("files", json::Value::Arr(std::move(arr)));
  return o;
}

}  // namespace pc2c::shaders
