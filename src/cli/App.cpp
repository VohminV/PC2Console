#include "pc2c/cli/App.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <utility>

#include "pc2c/analysis/Inventory.h"
#include "pc2c/analysis/Pe.h"
#include "pc2c/assets/Converter.h"
#include "pc2c/backends/Targets.h"
#include "pc2c/build/Toolchain.h"
#include "pc2c/core/Config.h"
#include "pc2c/core/Fs.h"
#include "pc2c/core/Hash.h"
#include "pc2c/core/Logger.h"
#include "pc2c/discovery/Scanner.h"
#include "pc2c/input/Analyze.h"
#include "pc2c/input/Diagnostics.h"
#include "pc2c/input/Manager.h"
#include "pc2c/input/Profiles.h"
#include "pc2c/input/TorqueGen.h"
#include "pc2c/input/WindowsBackend.h"
#include "pc2c/input/Xbox360Backend.h"
#include "pc2c/shaders/ShaderConvert.h"
#include "pc2c/intermediate/Model.h"
#include "pc2c/portability/Plan.h"
#include "pc2c/test/Suite.h"

namespace {
using pc2c::GlobalOptions;
using pc2c::Limits;

void usage() {
  std::cout << "pc2c " << pc2c::kToolVersion
            << " -- Windows-games to Xbox 360 / PS3 porting toolkit (MVP)\n"
               "\nUsage:\n"
               "  pc2c doctor [--probe-tools] [--format json]\n"
               "  pc2c backends list\n"
               "  pc2c inspect <game_dir> [--format json]\n"
               "  pc2c analyze <game_dir> [--output file.json] [--format json]\n"
               "  pc2c inventory <game_dir> [--format json]\n"
               "  pc2c assets list <game_dir> [--format json]\n"
               "  pc2c assets extract <game_dir> --output <dir> [--overwrite]\n"
               "  pc2c assets convert <input_dir> --output <dir> --profile NAME\n"
               "       [--max-texture-size N] [--overwrite] [--dry-run] [--format json]\n"
               "  pc2c assets verify <dir> [--input manifest.json] [--format json]\n"
               "  pc2c plan <game_dir> --target xbox360|ps3 [--output file.json] [--format json]\n"
               "  pc2c build <project_dir> --target xbox360|ps3 [--dry-run]\n"
               "  pc2c report <game_dir> [--target xbox360] [--output file.json]\n"
               "  pc2c input backends\n"
               "  pc2c input diag [--user N] [--seconds S] [--rumble-test] [--format json]\n"
               "  pc2c input analyze <dir> [--format json] [--output file.json]\n"
               "  pc2c input profile validate <file> | make-default --output <file>\n"
               "  pc2c input torquescript <config.cs> --output <gamepad.cs> [--bind fn=CTRL ...]\n"
               "  pc2c shaders convert <dir> --output <dir> [--profile sm1|sm2|sm3]\n"
               "       [--overwrite] [--dry-run] [--format json]\n"
               "  pc2c test [--verbose]\n"
               "\nGlobal flags: --input --output --target --profile --max-texture-size\n"
               "  --dry-run --overwrite --verbose --format text|json\n"
               "\nProfiles: conservative, xbox360-480p, xbox360-360p, xbox360-240p,\n"
               "  xbox360-120p, balanced\n"
               "\nSafety: inputs are never modified or executed; outputs need --overwrite to\n"
               "replace. Unknown formats fail honestly, never with fake success.\n";
}

struct Parsed {
  std::string cmd, sub;
  std::vector<std::string> positionals;
  GlobalOptions opts;
  int inputUser = 0;
  double inputSeconds = 5.0;
  bool inputRumbleTest = false;
  std::vector<std::string> inputBinds;  // repeated --bind fn=CONTROL
  bool help = false;
  std::string error;
};

Parsed parseArgs(const std::vector<std::string>& a) {
  Parsed p;
  std::size_t i = 0;
  auto needVal = [&](const std::string& flag, std::string& out) -> bool {
    auto eq = flag.find('=');
    if (eq != std::string::npos) {
      out = flag.substr(eq + 1);
      return true;
    }
    if (i + 1 >= a.size()) {
      p.error = "flag needs a value: " + flag;
      return false;
    }
    out = a[++i];
    return true;
  };
  for (; i < a.size(); ++i) {
    const std::string& t = a[i];
    if (t == "--help" || t == "-h") {
      p.help = true;
    } else if (t == "--dry-run") {
      p.opts.dryRun = true;
    } else if (t == "--overwrite") {
      p.opts.overwrite = true;
    } else if (t == "--verbose") {
      p.opts.verbose = true;
    } else if (t == "--probe-tools") {
      p.opts.probeTools = true;
    } else if (t == "--rumble-test") {
      p.inputRumbleTest = true;
    } else if (t.rfind("--bind", 0) == 0) {
      std::string v;
      if (!needVal(t, v)) return p;
      p.inputBinds.push_back(v);
    } else if (t.rfind("--user", 0) == 0) {
      std::string v;
      if (!needVal(t, v)) return p;
      try {
        p.inputUser = std::stoi(v);
      } catch (...) {
        p.error = "bad --user: " + v;
        return p;
      }
    } else if (t.rfind("--seconds", 0) == 0) {
      std::string v;
      if (!needVal(t, v)) return p;
      try {
        p.inputSeconds = std::stod(v);
      } catch (...) {
        p.error = "bad --seconds: " + v;
        return p;
      }
      if (p.inputSeconds <= 0 || p.inputSeconds > 120) {
        p.error = "bad --seconds (0..120): " + v;
        return p;
      }
    } else if (t.rfind("--input", 0) == 0) {
      if (!needVal(t, p.opts.input)) return p;
    } else if (t.rfind("--output", 0) == 0) {
      if (!needVal(t, p.opts.output)) return p;
    } else if (t.rfind("--target", 0) == 0) {
      if (!needVal(t, p.opts.target)) return p;
    } else if (t.rfind("--profile", 0) == 0) {
      if (!needVal(t, p.opts.profile)) return p;
    } else if (t.rfind("--max-texture-size", 0) == 0) {
      std::string v;
      if (!needVal(t, v)) return p;
      try {
        p.opts.maxTextureSize = static_cast<std::uint32_t>(std::stoul(v));
      } catch (...) {
        p.error = "bad --max-texture-size: " + v;
        return p;
      }
    } else if (t.rfind("--format", 0) == 0) {
      if (!needVal(t, p.opts.format)) return p;
      if (p.opts.format != "text" && p.opts.format != "json") {
        p.error = "bad --format (text|json): " + p.opts.format;
        return p;
      }
    } else if (t.rfind("--", 0) == 0) {
      p.error = "unknown flag: " + t;
      return p;
    } else if (p.cmd.empty()) {
      p.cmd = t;
    } else if (p.cmd == "assets" && p.sub.empty()) {
      p.sub = t;
    } else if (p.cmd == "input" && p.sub.empty()) {
      p.sub = t;
    } else if (p.cmd == "shaders" && p.sub.empty()) {
      p.sub = t;
    } else if (p.cmd == "backends" && p.sub.empty()) {
      p.sub = t;
    } else {
      p.positionals.push_back(t);
    }
  }
  // --input/--output double as positional fallbacks where sensible (handled per command).
  return p;
}

std::string positionalOrInput(const Parsed& p, std::size_t idx = 0) {
  if (idx < p.positionals.size()) return p.positionals[idx];
  if (idx == 0 && !p.opts.input.empty()) return p.opts.input;
  return "";
}

int emitJson(const pc2c::json::Value& v, const std::string& output) {
  std::string t = v.dump(2);
  if (output.empty()) {
    std::cout << t;
    return 0;
  }
  pc2c::Status st = pc2c::fs::atomicWriteFile(output, t);
  if (!st.ok) {
    std::cerr << "error: " << st.message << "\n";
    return 1;
  }
  std::cout << "wrote " << output << "\n";
  return 0;
}

pc2c::discovery::ScanResult doScan(const std::string& dir, const Limits& lim) {
  return pc2c::discovery::scanGameDir(dir, lim);
}

std::vector<std::pair<std::string, pc2c::analysis::PeInfo>> doPe(
    const std::string& root, const pc2c::discovery::ScanResult& scan, const Limits& lim) {
  std::vector<std::pair<std::string, pc2c::analysis::PeInfo>> out;
  for (const auto& f : scan.files) {
    if (f.kind != pc2c::discovery::FileKind::kExecutable &&
        f.kind != pc2c::discovery::FileKind::kLibrary)
      continue;
    std::string full = (std::filesystem::path(root) / f.rel).string();
    out.emplace_back(f.rel, pc2c::analysis::parsePeFile(full, lim.maxPeBytes));
  }
  return out;
}

pc2c::build::XdkInfo cachedXdk(bool& init, pc2c::build::XdkInfo& cache) {
  if (!init) {
    cache = pc2c::build::probeXbox360(false);
    init = true;
  }
  return cache;
}

// ---- commands ----

int cmdDoctor(const Parsed& p) {
  pc2c::build::XdkInfo x = pc2c::build::probeXbox360(p.opts.probeTools);
  pc2c::json::Value j = pc2c::build::xdkToJson(x);
  {
    pc2c::backends::Xbox360Backend xb;
    j.set("backend_xbox360", xb.diagnose().toJson());
    pc2c::backends::Ps3Backend ps;
    j.set("backend_ps3", ps.diagnose().toJson());
  }
  j.set("tool", pc2c::json::Value::Str(std::string(pc2c::kToolName) + " " + pc2c::kToolVersion));
  if (p.opts.format == "json") return emitJson(j, p.opts.output);
  std::cout << "XDK root: " << x.root << (x.found ? " (found)" : " (NOT FOUND)") << "\n";
  std::cout << "XDK version: " << (x.version.empty() ? "(unconfirmed)" : x.version) << "\n";
  for (const auto& c : x.components)
    std::cout << "  [" << (c.present ? "ok" : "MISSING") << "] " << c.name << "\n";
  std::cout << "VS2010: " << (x.vs2010Present ? "present" : "missing") << " -- " << x.vs2010Note
            << "\n";
  for (const auto& n : x.notes) std::cout << "note: " << n << "\n";
  if (!p.opts.output.empty()) return emitJson(j, p.opts.output);
  return 0;
}

int cmdBackends(const Parsed& p) {
  if (p.sub != "list") {
    std::cerr << "error: usage: pc2c backends list\n";
    return 1;
  }
  pc2c::backends::Xbox360Backend xb;
  pc2c::backends::Ps3Backend ps;
  pc2c::json::Value::Array a;
  a.push_back(xb.diagnose().toJson());
  a.push_back(ps.diagnose().toJson());
  pc2c::json::Value o = pc2c::json::Value::Obj();
  o.set("backends", pc2c::json::Value::Arr(std::move(a)));
  if (p.opts.format == "json") return emitJson(o, p.opts.output);
  for (const char* t : {"xbox360", "ps3"}) {
    pc2c::backends::Diagnosis d =
        (std::string(t) == "xbox360") ? xb.diagnose() : ps.diagnose();
    std::cout << d.backend << " ready=" << (d.ready ? "yes" : "NO") << "\n";
    for (const auto& b : d.blockers) std::cout << "  blocker: " << b << "\n";
  }
  return 0;
}

int cmdInspect(const Parsed& p, const Limits& lim) {
  std::string dir = positionalOrInput(p);
  if (dir.empty()) {
    std::cerr << "error: usage: pc2c inspect <game_dir>\n";
    return 1;
  }
  auto scan = doScan(dir, lim);
  auto pes = doPe(dir, scan, lim);
  std::map<std::string, std::size_t> byKind;
  std::map<std::string, std::uint64_t> bytes;
  for (const auto& f : scan.files) {
    std::string k = pc2c::discovery::kindToString(f.kind);
    byKind[k]++;
    bytes[k] += f.size;
  }
  std::vector<std::string> dlls;
  for (const auto& pr : pes)
    for (const auto& d : pr.second.importDlls) dlls.push_back(d);
  auto eng = pc2c::portability::guessEngine(scan.engineSignals, dlls);
  pc2c::json::Value o = pc2c::json::Value::Obj();
  o.set("tool", pc2c::json::Value::Str(std::string(pc2c::kToolName) + " " + pc2c::kToolVersion));
  o.set("dir", pc2c::json::Value::Str(dir));
  o.set("files", pc2c::json::Value::Int(scan.files.size()));
  o.set("total_bytes", pc2c::json::Value::Int(scan.totalBytes));
  o.set("truncated", pc2c::json::Value::Bool(scan.truncated));
  pc2c::json::Value::Array ka;
  for (const auto& kv : byKind) {
    pc2c::json::Value e = pc2c::json::Value::Obj();
    e.set("kind", pc2c::json::Value::Str(kv.first));
    e.set("count", pc2c::json::Value::Int(kv.second));
    e.set("bytes", pc2c::json::Value::Int(bytes[kv.first]));
    ka.push_back(std::move(e));
  }
  o.set("by_kind", pc2c::json::Value::Arr(std::move(ka)));
  o.set("pe_binaries", pc2c::json::Value::Int(pes.size()));
  o.set("engine_guess", pc2c::json::Value::Str(eng.engine + " [" + eng.confidence + "]"));
  if (p.opts.format == "json") return emitJson(o, p.opts.output);
  std::cout << "dir: " << dir << " files=" << scan.files.size() << " bytes=" << scan.totalBytes
            << (scan.truncated ? " (TRUNCATED)" : "") << "\n";
  for (const auto& kv : byKind)
    std::cout << "  " << kv.first << ": " << kv.second << " files, " << bytes[kv.first]
              << " bytes\n";
  std::cout << "pe binaries: " << pes.size() << "\n";
  std::cout << "engine guess: " << eng.engine << " [" << eng.confidence << "] via " << eng.basis
            << "\n";
  if (!p.opts.output.empty()) return emitJson(o, p.opts.output);
  return 0;
}

int cmdAnalyze(const Parsed& p, const Limits& lim) {
  std::string dir = positionalOrInput(p);
  if (dir.empty()) {
    std::cerr << "error: usage: pc2c analyze <game_dir> [--output file.json]\n";
    return 1;
  }
  auto scan = doScan(dir, lim);
  auto pes = doPe(dir, scan, lim);
  pc2c::json::Value o = pc2c::json::Value::Obj();
  o.set("tool", pc2c::json::Value::Str(std::string(pc2c::kToolName) + " " + pc2c::kToolVersion));
  o.set("dir", pc2c::json::Value::Str(dir));
  pc2c::json::Value::Array arr;
  for (const auto& pr : pes) {
    pc2c::json::Value e = pc2c::json::Value::Obj();
    e.set("file", pc2c::json::Value::Str(pr.first));
    e.set("valid_pe", pc2c::json::Value::Bool(pr.second.valid));
    if (!pr.second.valid) {
      e.set("error", pc2c::json::Value::Str(pr.second.error));
    } else {
      e.set("arch", pc2c::json::Value::Str(pr.second.arch));
      e.set("pe64", pc2c::json::Value::Bool(pr.second.is64));
      e.set("linker", pc2c::json::Value::Str(std::to_string(pr.second.linkerMajor) + "." +
                                             std::to_string(pr.second.linkerMinor)));
      e.set("compiler_hint", pc2c::json::Value::Str(pr.second.compilerHint));
      e.set("directx_hint", pc2c::json::Value::Str(pr.second.directxHint));
      e.set("dotnet", pc2c::json::Value::Bool(pr.second.isDotNet));
      pc2c::json::Value::Array ds, is;
      for (const auto& d : pr.second.importDlls) ds.push_back(pc2c::json::Value::Str(d));
      for (const auto& s : pr.second.imports) {
        if (is.size() >= 2000) break;
        is.push_back(pc2c::json::Value::Str(s));
      }
      e.set("import_dlls", pc2c::json::Value::Arr(std::move(ds)));
      e.set("imports_sample", pc2c::json::Value::Arr(std::move(is)));
      e.set("import_total", pc2c::json::Value::Int(pr.second.imports.size()));
      e.set("imports_truncated", pc2c::json::Value::Bool(pr.second.importsTruncated));
      e.set("import_caveat",
            pc2c::json::Value::Str("table presence does not prove a call executes"));
    }
    arr.push_back(std::move(e));
  }
  o.set("binaries", pc2c::json::Value::Arr(std::move(arr)));
  if (p.opts.format == "json" || !p.opts.output.empty()) return emitJson(o, p.opts.output);
  for (const auto& pr : pes) {
    std::cout << pr.first << ": ";
    if (!pr.second.valid) {
      std::cout << "NOT-PE (" << pr.second.error << ")\n";
      continue;
    }
    std::cout << pr.second.arch << (pr.second.is64 ? " PE32+" : " PE32") << " dx=" <<
        pr.second.directxHint << " imports=" << pr.second.imports.size() << " dlls=" <<
        pr.second.importDlls.size() << "\n";
  }
  return 0;
}

int cmdInventory(const Parsed& p, const Limits& lim) {
  std::string dir = positionalOrInput(p);
  if (dir.empty()) {
    std::cerr << "error: usage: pc2c inventory <game_dir>\n";
    return 1;
  }
  auto scan = doScan(dir, lim);
  auto inv = pc2c::analysis::buildInventory(dir, scan, lim.maxFileBytes);
  pc2c::json::Value o = pc2c::json::Value::Obj();
  o.set("tool", pc2c::json::Value::Str(std::string(pc2c::kToolName) + " " + pc2c::kToolVersion));
  o.set("dir", pc2c::json::Value::Str(dir));
  o.set("total_bytes", pc2c::json::Value::Int(inv.totalBytes));
  o.set("convertible_bytes_bmp_tga", pc2c::json::Value::Int(inv.convertibleBytes));
  pc2c::json::Value::Array arr;
  for (const auto& e : inv.resources) {
    pc2c::json::Value v = pc2c::json::Value::Obj();
    v.set("rel", pc2c::json::Value::Str(e.rel));
    v.set("kind", pc2c::json::Value::Str(e.kind));
    v.set("format", pc2c::json::Value::Str(e.format));
    v.set("size", pc2c::json::Value::Int(e.size));
    v.set("confidence", pc2c::json::Value::Str(e.confidence));
    v.set("basis", pc2c::json::Value::Str(e.basis));
    v.set("convertible", pc2c::json::Value::Bool(e.convertible));
    v.set("convert_note", pc2c::json::Value::Str(e.convertNote));
    arr.push_back(std::move(v));
  }
  o.set("resources", pc2c::json::Value::Arr(std::move(arr)));
  if (p.opts.format == "json" || !p.opts.output.empty()) return emitJson(o, p.opts.output);
  std::cout << "resources: " << inv.resources.size() << " total_bytes=" << inv.totalBytes
            << " convertible_bmp_tga=" << inv.convertibleBytes << "\n";
  return 0;
}

int cmdAssets(const Parsed& p, const Limits& lim) {
  if (p.sub == "list") {
    std::string dir = positionalOrInput(p);
    if (dir.empty()) {
      std::cerr << "error: usage: pc2c assets list <game_dir>\n";
      return 1;
    }
    auto scan = doScan(dir, lim);
    auto inv = pc2c::analysis::buildInventory(dir, scan, lim.maxFileBytes);
    pc2c::json::Value o = pc2c::json::Value::Obj();
    pc2c::json::Value::Array arr;
    for (const auto& e : inv.resources) {
      if (e.kind != "image") continue;
      pc2c::json::Value v = pc2c::json::Value::Obj();
      v.set("rel", pc2c::json::Value::Str(e.rel));
      v.set("format", pc2c::json::Value::Str(e.format));
      v.set("size", pc2c::json::Value::Int(e.size));
      v.set("confidence", pc2c::json::Value::Str(e.confidence));
      v.set("convertible", pc2c::json::Value::Bool(e.convertible));
      v.set("note", pc2c::json::Value::Str(e.convertNote));
      arr.push_back(std::move(v));
    }
    o.set("images", pc2c::json::Value::Arr(std::move(arr)));
    if (p.opts.format == "json") return emitJson(o, p.opts.output);
    const auto* imgs = o.find("images");
    std::cout << "images: " << (imgs ? std::get<pc2c::json::Value::Array>(imgs->v).size() : 0)
              << "\n";
    if (imgs)
      for (const auto& e : std::get<pc2c::json::Value::Array>(imgs->v)) {
        const auto* r = e.find("rel");
        const auto* c = e.find("convertible");
        std::cout << "  " << (r ? std::get<std::string>(r->v) : "?") << " convertible="
                  << (c && std::get<bool>(c->v) ? "yes" : "no") << "\n";
      }
    return 0;
  }
  if (p.sub == "extract") {
    std::string dir = positionalOrInput(p);
    if (dir.empty() || p.opts.output.empty()) {
      std::cerr << "error: usage: pc2c assets extract <game_dir> --output <dir>\n";
      return 1;
    }
    auto scan = doScan(dir, lim);
    auto inv = pc2c::analysis::buildInventory(dir, scan, lim.maxFileBytes);
    std::filesystem::path outRoot(p.opts.output);
    if (!p.opts.dryRun) {
      pc2c::Status st = pc2c::fs::ensureDir(outRoot);
      if (!st.ok) {
        std::cerr << "error: " << st.message << "\n";
        return 1;
      }
    }
    pc2c::json::Value::Array arr;
    std::size_t copied = 0, skipped = 0;
    for (const auto& e : inv.resources) {
      if (e.kind != "image" || !e.convertible) continue;
      auto src = std::filesystem::path(dir) / e.rel;
      auto dst = pc2c::fs::joinSafe(outRoot, e.rel);
      if (!dst) {
        skipped++;
        continue;
      }
      pc2c::json::Value v = pc2c::json::Value::Obj();
      v.set("rel", pc2c::json::Value::Str(e.rel));
      if (p.opts.dryRun) {
        v.set("status", pc2c::json::Value::Str("dry-run"));
        arr.push_back(std::move(v));
        continue;
      }
      std::error_code ec;
      if (std::filesystem::exists(*dst, ec) && !p.opts.overwrite) {
        v.set("status", pc2c::json::Value::Str("skipped-exists"));
        skipped++;
      } else {
        std::vector<std::uint8_t> raw;
        pc2c::Status rs = pc2c::fs::readFileBounded(src, raw, lim.maxImageBytes);
        if (!rs.ok) {
          v.set("status", pc2c::json::Value::Str("failed"));
          v.set("reason", pc2c::json::Value::Str(rs.message));
          skipped++;
        } else {
          pc2c::Status ws = pc2c::fs::atomicWriteFile(*dst, raw.data(), raw.size());
          if (!ws.ok) {
            v.set("status", pc2c::json::Value::Str("failed"));
            v.set("reason", pc2c::json::Value::Str(ws.message));
            skipped++;
          } else {
            v.set("status", pc2c::json::Value::Str("extracted"));
            copied++;
          }
        }
      }
      arr.push_back(std::move(v));
    }
    pc2c::json::Value o = pc2c::json::Value::Obj();
    o.set("extracted", pc2c::json::Value::Int(copied));
    o.set("skipped", pc2c::json::Value::Int(skipped));
    o.set("note", pc2c::json::Value::Str("archives/containers are staged only after a registered "
                                         "decoder exists; nothing is unpacked blindly"));
    o.set("files", pc2c::json::Value::Arr(std::move(arr)));
    if (p.opts.format == "json") return emitJson(o, "");
    std::cout << "extracted: " << copied << " skipped: " << skipped << " -> " << p.opts.output
              << "\n";
    return 0;
  }
  if (p.sub == "convert") {
    std::string dir = positionalOrInput(p);
    if (dir.empty() || p.opts.output.empty()) {
      std::cerr << "error: usage: pc2c assets convert <input_dir> --output <dir> --profile NAME\n";
      return 1;
    }
    std::string profName = p.opts.profile.empty() ? "xbox360-480p" : p.opts.profile;
    auto prof = pc2c::assets::findProfile(profName);
    if (!prof) {
      std::cerr << "error: unknown profile '" << profName << "' (see `pc2c --help`)\n";
      return 1;
    }
    pc2c::assets::ConvertOptions co;
    co.overwrite = p.opts.overwrite;
    co.dryRun = p.opts.dryRun;
    co.verbose = p.opts.verbose;
    co.maxTextureSizeOverride = p.opts.maxTextureSize;
    pc2c::assets::DirSummary s;
    pc2c::Status st = pc2c::assets::convertDir(dir, p.opts.output, *prof, co, lim, s);
    if (!st.ok) {
      std::cerr << "error: " << st.message << "\n";
      return 1;
    }
    if (p.opts.format == "json") return emitJson(pc2c::assets::summaryToJson(s), "");
    std::cout << "profile=" << s.profile << " max_long=" << s.maxLongSide
              << " total=" << s.total << " converted=" << s.converted << " skipped=" << s.skipped
              << " failed=" << s.failed << " unsupported=" << s.unsupported << "\n";
    std::cout << "bytes " << s.inBytes << " -> " << s.outBytes << " (gpu est " << s.gpuIn << " -> "
              << s.gpuOut << ")\nmanifest: " << s.manifestPath << "\n";
    return 0;
  }
  if (p.sub == "verify") {
    std::string dir = positionalOrInput(p);
    if (dir.empty()) {
      std::cerr << "error: usage: pc2c assets verify <dir> [--input manifest.json]\n";
      return 1;
    }
    pc2c::assets::VerifyReport rep;
    pc2c::Status st = pc2c::assets::verifyDir(dir, p.opts.input, rep, lim);
    if (!st.ok) {
      std::cerr << "error: " << st.message << "\n";
      return 1;
    }
    if (p.opts.format == "json") return emitJson(pc2c::assets::verifyToJson(rep), "");
    std::cout << "checked=" << rep.checked << " ok=" << rep.ok << " mismatch=" << rep.mismatch
              << " missing=" << rep.missing << " undecodable=" << rep.undecodable << "\n";
    for (const auto& pr : rep.problems) std::cout << "  problem: " << pr << "\n";
    return (rep.mismatch + rep.missing + rep.undecodable) == 0 ? 0 : 1;
  }
  std::cerr << "error: unknown assets subcommand '" << p.sub
            << "' (list|extract|convert|verify)\n";
  return 1;
}

pc2c::model::ModelDoc buildModel(const std::string& dir, const std::string& target,
                                 const Limits& lim) {
  using namespace pc2c;
  model::ModelDoc m;
  m.gameDir = dir;
  m.target = target;
  auto scan = discovery::scanGameDir(dir, lim);
  m.addFact("scan", {"<scan>", -1, "directory-walk"},
            "files=" + std::to_string(scan.files.size()) + " bytes=" +
                std::to_string(scan.totalBytes) + (scan.truncated ? " TRUNCATED" : ""));
  auto pes = doPe(dir, scan, lim);
  for (const auto& pr : pes) {
    if (!pr.second.valid) {
      m.addFact("binary", {pr.first, -1, "pe-parse"}, "not-a-valid-pe: " + pr.second.error,
                "confirmed");
      continue;
    }
    m.addFact("binary", {pr.first, 0x3C, "pe-header"},
              "arch=" + pr.second.arch + (pr.second.is64 ? " PE32+" : " PE32") +
                  " linker=" + std::to_string(pr.second.linkerMajor) + "." +
                  std::to_string(pr.second.linkerMinor));
    for (const auto& d : pr.second.importDlls)
      m.addFact("binary", {pr.first, -1, "import-table"}, "imports-dll: " + d);
    m.addAssumption("binary", {pr.first, -1, "import-table"},
                    "import presence does not prove a call executes");
  }
  auto inv = analysis::buildInventory(dir, scan, lim.maxFileBytes);
  for (const auto& e : inv.resources)
    m.addFact("resource", {e.rel, -1, e.basis},
              e.kind + "/" + e.format + " size=" + std::to_string(e.size) +
                  " convertible=" + (e.convertible ? "yes" : "no"),
              e.confidence);
  bool xInit = false;
  build::XdkInfo x;
  bool sdkP = false;
  std::string sdkV;
  std::vector<std::string> sdkM;
  if (target == "xbox360") {
    x = cachedXdk(xInit, x);
    sdkP = x.found && x.missing.empty();
    sdkV = x.version;
    sdkM = x.missing;
  }
  auto plan = portability::buildPlan(dir, target, scan, pes, inv, sdkP, sdkV, sdkM);
  for (const auto& l : plan.limitations)
    m.addLimitation(l.category, l.subject + ": " + l.reason, l.remediation);
  m.extra = portability::planToJson(plan);
  return m;
}

int cmdPlan(const Parsed& p, const Limits& lim) {
  std::string dir = positionalOrInput(p);
  if (dir.empty() || (p.opts.target != "xbox360" && p.opts.target != "ps3")) {
    std::cerr << "error: usage: pc2c plan <game_dir> --target xbox360|ps3\n";
    return 1;
  }
  auto m = buildModel(dir, p.opts.target, lim);
  if (p.opts.format == "json" || !p.opts.output.empty())
    return emitJson(m.extra, p.opts.output);
  const pc2c::json::Value* v = m.extra.find("verdict");
  const pc2c::json::Value* lims = m.extra.find("limitations");
  std::cout << "target: " << p.opts.target << "\n";
  if (v) std::cout << "verdict: " << std::get<std::string>(v->v) << "\n";
  if (lims)
    for (const auto& l : std::get<pc2c::json::Value::Array>(lims->v)) {
      const auto* c = l.find("category");
      const auto* s = l.find("subject");
      std::cout << "  [" << (c ? std::get<std::string>(c->v) : "?") << "] "
                << (s ? std::get<std::string>(s->v) : "?") << "\n";
    }
  return 0;
}

int cmdBuild(const Parsed& p) {
  std::string dir = positionalOrInput(p);
  if (dir.empty() || (p.opts.target != "xbox360" && p.opts.target != "ps3")) {
    std::cerr << "error: usage: pc2c build <project_dir> --target xbox360|ps3 [--dry-run]\n";
    return 1;
  }
  if (p.opts.target == "ps3") {
    pc2c::backends::Ps3Backend ps;
    pc2c::json::Value o;
    ps.planBuild(dir, o);
    std::cout << o.dump(2);
    std::cerr << "unsupported: PS3 builds are not implemented in this phase.\n";
    return 2;
  }
  pc2c::backends::Xbox360Backend xb;
  pc2c::json::Value o;
  xb.planBuild(dir, o);
  o.set("dry_run", pc2c::json::Value::Bool(p.opts.dryRun));
  // If the project dir is an Xbox 360 sample, perform a REAL rebuild with the
  // VS2010+XDK toolchain (explicit user request => known MS binaries may run).
  pc2c::build::SampleBuild sb = pc2c::build::buildXboxSample(dir, p.opts.dryRun);
  o.set("sample_build", pc2c::build::sampleBuildToJson(sb));
  std::cout << o.dump(2);
  if (sb.isSample) {
    if (p.opts.dryRun) {
      std::cout << "dry-run: sample build command above was NOT executed.\n";
      return 0;
    }
    if (sb.ok) {
      std::cout << "sample rebuilt OK: " << sb.xexPath << " (XEX2 verified)\n";
      return 0;
    }
    std::cerr << "sample build FAILED: " << sb.error << "\n";
    return 1;
  }
  std::cout << "note: backend ready=false until a minimal XDK sample compiles, links, packages "
               "and is verified (Phase 5). No XEX was produced by this command.\n";
  return 0;
}

int cmdReport(const Parsed& p, const Limits& lim) {
  std::string dir = positionalOrInput(p);
  if (dir.empty()) {
    std::cerr << "error: usage: pc2c report <game_dir> [--target xbox360] [--output file.json]\n";
    return 1;
  }
  std::string tgt = p.opts.target.empty() ? "xbox360" : p.opts.target;
  auto m = buildModel(dir, tgt, lim);
  return emitJson(m.toJson(), p.opts.output);
}

int cmdShaders(const Parsed& p, const Limits& lim) {
  using namespace pc2c;
  using namespace pc2c::shaders;
  if (p.sub != "convert") {
    std::cerr << "error: usage: pc2c shaders convert <dir> --output <dir> [--profile sm1|sm2|sm3]\n";
    return 1;
  }
  std::string dir = positionalOrInput(p);
  if (dir.empty() || p.opts.output.empty()) {
    std::cerr << "error: usage: pc2c shaders convert <dir> --output <dir> [--profile sm1|sm2|sm3]\n";
    return 1;
  }
  std::string sm = p.opts.profile.empty() ? "sm2" : p.opts.profile;
  ShaderOptions so;
  so.overwrite = p.opts.overwrite;
  so.dryRun = p.opts.dryRun;
  if (sm == "sm1") {
    so.vsProfile = "vs_1_1";
    so.psProfile = "ps_1_1";
  } else if (sm == "sm2") {
    so.vsProfile = "vs_2_0";
    so.psProfile = "ps_2_0";
  } else if (sm == "sm3") {
    so.vsProfile = "vs_3_0";
    so.psProfile = "ps_3_0";
  } else {
    std::cerr << "error: unknown shader profile '" << sm << "' (sm1|sm2|sm3)\n";
    return 1;
  }
  DirSummary s;
  Status st = convertDir(dir, p.opts.output, so, lim, s);
  if (p.opts.format == "json" || !st.ok) {
    int rc = emitJson(summaryToJson(s), "");
    if (!st.ok) std::cerr << "error: " << st.message << "\n";
    return st.ok ? rc : 1;
  }
  std::cout << "shaders total=" << s.total << " compiled=" << s.compiled << " skipped=" << s.skipped
            << " failed=" << s.failed << " unsupported=" << s.unsupported << "\n";
  std::cout << "report: " << s.reportPath << "\n";
  return 0;
}

int cmdInput(const Parsed& p, const Limits& lim) {
  using namespace pc2c;
  using namespace pc2c::input;
  if (p.sub == "backends") {
    json::Value o = json::Value::Obj();
    std::string note;
    auto win = createWindowsBackend(note);
    json::Value w = json::Value::Obj();
    if (win) {
      BackendCaps c = win->caps();
      w.set("available", json::Value::Bool(true));
      w.set("name", json::Value::Str(c.backend));
      w.set("max_controllers", json::Value::Int(c.maxControllers));
      w.set("analog_triggers", json::Value::Bool(c.analogTriggers));
      w.set("rumble", json::Value::Bool(c.hasRumble));
      w.set("detail", json::Value::Str(c.detail));
    } else {
      w.set("available", json::Value::Bool(false));
      w.set("reason", json::Value::Str(note));
    }
    o.set("windows", std::move(w));
    json::Value x = json::Value::Obj();
#ifdef _XBOX
    auto xb = createXbox360Backend();
    x.set("available", json::Value::Bool(xb != nullptr));
#else
    x.set("available", json::Value::Bool(false));
    x.set("reason",
          json::Value::Str("xbox360 backend compiles only under the XDK toolchain "
                           "(src/input/xbox360/Xbox360Backend.cpp); confirmed API: "
                           "XInputGetState/SetState, XINPUT_STATE, ERROR_DEVICE_NOT_CONNECTED"));
#endif
    o.set("xbox360", std::move(x));
    o.set("note", json::Value::Str("desktop and console are independent implementations "
                                   "of IInputBackend; no shared platform code"));
    if (p.opts.format == "json") return emitJson(o, p.opts.output);
    const json::Value* ww = o.find("windows");
    const json::Value* xx = o.find("xbox360");
    std::cout << "windows-xinput: "
              << (ww && ww->find("available") && std::get<bool>(ww->find("available")->v)
                      ? "available"
                      : "UNAVAILABLE")
              << "\n";
    if (ww) {
      const json::Value* d = ww->find("detail");
      const json::Value* r = ww->find("reason");
      if (d) std::cout << "  " << std::get<std::string>(d->v) << "\n";
      if (r) std::cout << "  " << std::get<std::string>(r->v) << "\n";
    }
    std::cout << "xbox360-xinput: "
              << (xx && xx->find("available") && std::get<bool>(xx->find("available")->v)
                      ? "available (console build)"
                      : "host-unavailable (XDK-only TU, by design)")
              << "\n";
    return 0;
  }
  if (p.sub == "diag") {
    std::string note;
    auto backend = createWindowsBackend(note);
    if (!backend) {
      std::cerr << "error: windows input backend unavailable: " << note << "\n";
      return 1;
    }
    InputProfile prof = InputProfile::makeDefault("pc");
    ControllerManager mgr(std::move(backend), prof, p.inputUser);
    DiagOptions d;
    d.userIndex = p.inputUser;
    d.seconds = p.inputSeconds;
    d.rumbleTest = p.inputRumbleTest;
    std::cout << "sampling input " << d.seconds << "s (move sticks, press buttons)...\n";
    DiagReport rep = runDiagnostics(mgr, d);
    if (p.opts.format == "json") return emitJson(rep.toJson(), p.opts.output);
    std::cout << "backend=" << rep.backend << " tested_on=" << rep.testedOn << "\n";
    std::cout << "frames=" << rep.frames << " poll_hz=" << rep.pollHz
              << " ever_connected=" << (rep.everConnected ? "yes" : "NO") << "\n";
    if (!rep.everConnected)
      std::cout << "no controller attached: connect a pad and rerun (reconnect is tracked)\n";
    std::cout << "lx " << rep.minLX << ".." << rep.maxLX << " ly " << rep.minLY << ".." << rep.maxLY
              << " lt_max " << (int)rep.maxLT << " rt_max " << (int)rep.maxRT << "\n";
    std::cout << rep.asciiPlot;
    std::cout << "buttons_seen:";
    for (const auto& b : rep.buttonsSeen) std::cout << " " << b;
    std::cout << "\nrumble=" << rep.rumbleResult << " raw: " << rep.rawNote << "\n";
    if (!p.opts.output.empty()) return emitJson(rep.toJson(), p.opts.output);
    return 0;
  }
  if (p.sub == "analyze") {
    std::string dir = positionalOrInput(p);
    if (dir.empty()) {
      std::cerr << "error: usage: pc2c input analyze <dir>\n";
      return 1;
    }
    auto scan = doScan(dir, lim);
    auto pes = doPe(dir, scan, lim);
    InputAnalysis a = analyzeInput(dir, scan, pes, lim);
    if (p.opts.format == "json" || !p.opts.output.empty())
      return emitJson(a.toJson(), p.opts.output);
    std::cout << "has_sources=" << (a.hasSources ? "yes" : "no")
              << " findings=" << a.findings.size() << "\n";
    for (const auto& f : a.findings)
      std::cout << "  [" << f.system << "/" << f.confidence << "] " << f.evidence << "\n";
    std::cout << "verdict: " << a.mechanismVerdict << "\n";
    return 0;
  }
  if (p.sub == "profile") {
    if (p.positionals.empty()) {
      std::cerr << "error: usage: pc2c input profile validate <file> | make-default --output "
                   "<file>\n";
      return 1;
    }
    if (p.positionals[0] == "validate") {
      std::string file = p.positionals.size() > 1 ? p.positionals[1] : p.opts.input;
      if (file.empty()) {
        std::cerr << "error: usage: pc2c input profile validate <file>\n";
        return 1;
      }
      InputProfile prof;
      std::string err;
      if (!prof.loadFile(file, err).ok) {
        std::cerr << "error: " << err << "\n";
        return 1;
      }
      auto issues = prof.validate();
      json::Value o = json::Value::Obj();
      o.set("file", json::Value::Str(file));
      o.set("version", json::Value::Int(prof.version));
      o.set("actions", json::Value::Int(prof.mapping.bindings().size()));
      json::Value::Array ia;
      for (const auto& i : issues) {
        json::Value e = json::Value::Obj();
        e.set("severity", json::Value::Str(i.severity));
        e.set("message", json::Value::Str(i.message));
        ia.push_back(std::move(e));
      }
      o.set("issues", json::Value::Arr(std::move(ia)));
      if (p.opts.format == "json") return emitJson(o, p.opts.output);
      std::cout << "profile ok: version=" << prof.version
                << " actions=" << prof.mapping.bindings().size() << "\n";
      for (const auto& i : issues) std::cout << "  [" << i.severity << "] " << i.message << "\n";
      return 0;
    }
    if (p.positionals[0] == "make-default") {
      if (p.opts.output.empty()) {
        std::cerr << "error: usage: pc2c input profile make-default --output <file>\n";
        return 1;
      }
      std::string tgt = p.opts.target.empty() ? "xbox360" : p.opts.target;
      InputProfile prof = InputProfile::makeDefault(tgt);
      Status st = prof.saveFile(p.opts.output);
      if (!st.ok) {
        std::cerr << "error: " << st.message << "\n";
        return 1;
      }
      std::cout << "wrote default profile (" << tgt << "): " << p.opts.output << "\n";
      return 0;
    }
    std::cerr << "error: unknown profile action '" << p.positionals[0] << "'\n";
    return 1;
  }
  if (p.sub == "torquescript") {
    std::string cfg = positionalOrInput(p);
    if (cfg.empty() || p.opts.output.empty()) {
      std::cerr << "error: usage: pc2c input torquescript <config.cs> --output <gamepad.cs> "
                   "[--bind fn=CONTROL ...]\n";
      return 1;
    }
    std::vector<std::uint8_t> raw;
    Status rs = fs::readFileBounded(cfg, raw, 2ULL * 1024 * 1024);
    if (!rs.ok) {
      std::cerr << "error: " << rs.message << "\n";
      return 1;
    }
    std::vector<TsBind> binds;
    std::string err;
    if (!parseTsConfig(std::string(raw.begin(), raw.end()), binds, err).ok) {
      std::cerr << "error: " << err << "\n";
      return 1;
    }
    std::vector<TsRule> rules;
    for (const auto& b : p.inputBinds) {
      TsRule rl;
      if (!parseTsRule(b, rl, err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
      }
      rules.push_back(rl);
    }
    TsGenResult g = generateGamepadCs(cfg, binds, rules);
    Status ws = fs::atomicWriteFile(p.opts.output, g.csText);
    if (!ws.ok) {
      std::cerr << "error: " << ws.message << "\n";
      return 1;
    }
    if (p.opts.format == "json") return emitJson(g.report, "");
    std::cout << "binds parsed=" << binds.size() << " active=" << g.active
              << " drafts=" << g.drafts << " warnings=" << g.warnings.size() << "\n";
    for (const auto& w : g.warnings) std::cout << "  warn: " << w << "\n";
    std::cout << "wrote " << p.opts.output << " (REVIEW DRAFT, mechanism unproven)\n";
    return 0;
  }
  std::cerr << "error: unknown input subcommand '" << p.sub
            << "' (backends|diag|analyze|profile)\n";
  return 1;
}

}  // namespace

int Pc2cMain(const std::vector<std::string>& args) {
  Parsed p = parseArgs(args);
  if (!p.error.empty()) {
    std::cerr << "error: " << p.error << "\n";
    usage();
    return 1;
  }
  pc2c::Logger::Global().setVerbose(p.opts.verbose);
  Limits lim;
  if (p.help || p.cmd.empty() || p.cmd == "help") {
    usage();
    return 0;
  }
  if (p.cmd == "doctor") return cmdDoctor(p);
  if (p.cmd == "backends") return cmdBackends(p);
  if (p.cmd == "inspect") return cmdInspect(p, lim);
  if (p.cmd == "analyze") return cmdAnalyze(p, lim);
  if (p.cmd == "inventory") return cmdInventory(p, lim);
  if (p.cmd == "assets") return cmdAssets(p, lim);
  if (p.cmd == "input") return cmdInput(p, lim);
  if (p.cmd == "shaders") return cmdShaders(p, lim);
  if (p.cmd == "plan") return cmdPlan(p, lim);
  if (p.cmd == "build") return cmdBuild(p);
  if (p.cmd == "report") return cmdReport(p, lim);
  if (p.cmd == "test") return pc2c::test::RunAllTests(p.opts.verbose);
  std::cerr << "error: unknown command '" << p.cmd << "'\n";
  usage();
  return 1;
}
