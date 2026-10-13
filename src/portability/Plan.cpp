#include "pc2c/portability/Plan.h"

#include <map>

namespace pc2c::portability {

PlanDoc buildPlan(const std::string& gameDir, const std::string& target,
                  const discovery::ScanResult& scan,
                  const std::vector<std::pair<std::string, analysis::PeInfo>>& pes,
                  const analysis::Inventory& inv, bool sdkPresent, const std::string& sdkVersion,
                  const std::vector<std::string>& sdkMissing) {
  PlanDoc p;
  p.gameDir = gameDir;
  p.target = target;
  p.sdkPresent = sdkPresent;
  p.sdkVersion = sdkVersion;
  p.sdkMissing = sdkMissing;

  std::vector<std::string> allDlls;
  for (const auto& pr : pes) {
    PeFileReport r;
    r.rel = pr.first;
    r.pe = pr.second;
    r.cpu = classifyCpuArch(pr.second.arch, target);
    for (const auto& d : pr.second.importDlls) {
      r.dlls.push_back(classifyDll(d, target));
      allDlls.push_back(d);
    }
    p.binaries.push_back(std::move(r));
  }

  // Graphics: merge dx hints across binaries.
  std::string dx = "none-detected";
  for (const auto& b : p.binaries) {
    if (b.pe.directxHint == "d3d12") {
      dx = "d3d12";
      break;
    }
    if (b.pe.directxHint == "d3d11") dx = "d3d11";
    if (b.pe.directxHint == "d3d10" && dx != "d3d11") dx = "d3d10";
    if ((b.pe.directxHint == "d3d9" || b.pe.directxHint == "d3d9+d3dx") &&
        dx == "none-detected")
      dx = b.pe.directxHint;
  }
  p.graphics = classifyDirectx(dx, target);
  p.graphicsDetail = dx;

  p.engine = guessEngine(scan.engineSignals, allDlls);

  p.fileCount = scan.files.size();
  p.totalBytes = scan.totalBytes;
  p.convertibleBytes = inv.convertibleBytes;
  for (const auto& e : inv.resources) {
    if (e.kind == "image") p.imageCount++;
    if (e.kind == "audio") p.audioCount++;
    if (e.kind == "archive") p.archiveCount++;
    if (e.kind == "model") p.modelCount++;
    if (e.kind == "shader") p.shaderCount++;
  }

  auto addLim = [&](std::string cat, std::string subj, std::string why, std::string fix) {
    p.limitations.push_back({std::move(cat), std::move(subj), std::move(why), std::move(fix)});
  };

  if (p.binaries.empty())
    addLim("UNKNOWN", "binaries", "No PE executables/libraries found in scan.",
           "Check whether the game dir is complete; archives may hide binaries.");

  std::map<std::string, int> catCount;
  for (const auto& b : p.binaries) {
    addLim(b.cpu.category, "cpu:" + b.rel, b.cpu.reason, b.cpu.remediation);
    catCount[b.cpu.category]++;
    for (std::size_t i = 0; i < b.pe.importDlls.size(); ++i) {
      const auto& vd = b.dlls[i];
      addLim(vd.category, "import:" + b.pe.importDlls[i] + " (in " + b.rel + ")", vd.reason,
             vd.remediation);
      catCount[vd.category]++;
    }
    if (b.pe.isDotNet)
      addLim("UNSUPPORTED", "managed:" + b.rel, "Managed assembly needs CLR/engine port.",
             "Engine-level port or rewrite.");
    if (b.pe.importsTruncated)
      addLim("REQUIRES_RESEARCH", "imports-truncated:" + b.rel,
             "Import table hit parser caps; classification may miss DLLs.",
             "Re-run analysis with larger limits; inspect with parser warnings.");
  }
  addLim(p.graphics.category, "graphics:" + dx, p.graphics.reason, p.graphics.remediation);
  addLim(p.engine.confidence == "low" ? "UNKNOWN" : "REQUIRES_RESEARCH",
         "engine:" + p.engine.engine, "Engine guess: " + p.engine.basis + ".",
         "Confirm engine from game docs/strings; console engine license check.");
  if (p.archiveCount > 0)
    addLim("REQUIRES_RESEARCH", "archives:" + std::to_string(p.archiveCount),
           "Proprietary containers hide unknown payloads; nothing unpacked blindly.",
           "Register decoders per format; inventory after unpack.");
  if (p.shaderCount > 0)
    addLim("REQUIRES_SOURCE", "shaders:" + std::to_string(p.shaderCount),
           "Shader models/bytecode need Xbox fxc recompile and feature-level review.",
           "Collect shader sources; recompile with Xbox toolchain; test on hardware.");
  if (!sdkPresent)
    addLim("UNKNOWN", "sdk", "Xbox 360 toolchain not confirmed in this environment.",
           "Install/verify XDK 21256.3 + VS2010; re-run doctor.");
  else if (!sdkMissing.empty())
    addLim("REQUIRES_RESEARCH", "sdk-gaps", "Some SDK components missing.",
           "Resolve each missing component before claiming build readiness.");

  p.manualActions = {
      "Confirm game dir completeness (binaries + data, no running the game).",
      "Obtain game source code + engine console license (binary-only titles cannot be ported).",
      "Verify XDK 21256.3 + VS2010 toolchain via `pc2c doctor` (filesystem check, no unknown exes).",
      "Convert a BMP/TGA subset with `assets convert --profile xbox360-480p --dry-run` first.",
      "Adapt renderer at D3D9-class level; recompile shaders with Xbox fxc.",
      "Adapt Win32 services (files, threads, net, input, audio) to Xbox APIs.",
      "Package with imagexex and test on devkit or other legitimate test environment.",
  };
  p.requiredTests = {
      "PE analysis on synthetic fixtures (see `pc2c test`).",
      "Asset roundtrip: convert + verify manifest on sample images.",
      "Renderer smoke: XDK sample builds and shows graphics output.",
      "XEX boots to title screen on test hardware.",
      "Memory/perf measurement vs 512MB unified budget.",
  };

  // Honest verdict: no fake readiness percentage.
  int unsup = catCount["UNSUPPORTED"];
  int reqSrc = catCount["REQUIRES_SOURCE"];
  if (p.binaries.empty()) {
    p.verdict =
        "No binaries analyzed: port planning is blocked until game files are inventoried.";
  } else if (unsup > 0) {
    p.verdict = "Port blocked on " + std::to_string(unsup) +
                " UNSUPPORTED item(s): redesign/rewrite required; source mandatory.";
  } else if (reqSrc > 0) {
    p.verdict = "Port possible only with full source + engine license: " +
                std::to_string(reqSrc) + " item(s) need reimplementation.";
  } else {
    p.verdict =
        "No hard blockers in import tables, but launch is NOT proven: build + hardware test required.";
  }
  return p;
}

json::Value planToJson(const PlanDoc& p) {
  json::Value o = json::Value::Obj();
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("model_version", json::Value::Int(kModelVersion));
  o.set("game_dir", json::Value::Str(p.gameDir));
  o.set("target", json::Value::Str(p.target));
  json::Value eng = json::Value::Obj();
  eng.set("engine", json::Value::Str(p.engine.engine));
  eng.set("confidence", json::Value::Str(p.engine.confidence));
  eng.set("basis", json::Value::Str(p.engine.basis));
  o.set("engine_guess", std::move(eng));

  json::Value::Array bins;
  for (const auto& b : p.binaries) {
    json::Value bo = json::Value::Obj();
    bo.set("file", json::Value::Str(b.rel));
    bo.set("arch", json::Value::Str(b.pe.arch));
    bo.set("pe64", json::Value::Bool(b.pe.is64));
    bo.set("linker", json::Value::Str(std::to_string(b.pe.linkerMajor) + "." +
                                      std::to_string(b.pe.linkerMinor)));
    bo.set("compiler_hint", json::Value::Str(b.pe.compilerHint));
    bo.set("directx_hint", json::Value::Str(b.pe.directxHint));
    bo.set("cpu_category", json::Value::Str(b.cpu.category));
    json::Value::Array dlls;
    for (std::size_t i = 0; i < b.pe.importDlls.size(); ++i) {
      json::Value d = json::Value::Obj();
      d.set("dll", json::Value::Str(b.pe.importDlls[i]));
      d.set("category", json::Value::Str(b.dlls[i].category));
      d.set("reason", json::Value::Str(b.dlls[i].reason));
      dlls.push_back(std::move(d));
    }
    bo.set("imports", json::Value::Arr(std::move(dlls)));
    json::Value::Array exps;
    for (const auto& e : b.pe.exports) exps.push_back(json::Value::Str(e));
    bo.set("exports_sample", json::Value::Arr(std::move(exps)));
    bo.set("import_count_note",
           json::Value::Str("import table presence does not prove a call executes"));
    bins.push_back(std::move(bo));
  }
  o.set("binaries", json::Value::Arr(std::move(bins)));
  json::Value gr = json::Value::Obj();
  gr.set("hint", json::Value::Str(p.graphicsDetail));
  gr.set("category", json::Value::Str(p.graphics.category));
  gr.set("reason", json::Value::Str(p.graphics.reason));
  o.set("graphics", std::move(gr));

  json::Value res = json::Value::Obj();
  res.set("file_count", json::Value::Int(p.fileCount));
  res.set("total_bytes", json::Value::Int(p.totalBytes));
  res.set("convertible_bytes_bmp_tga", json::Value::Int(p.convertibleBytes));
  res.set("images", json::Value::Int(p.imageCount));
  res.set("audio", json::Value::Int(p.audioCount));
  res.set("archives", json::Value::Int(p.archiveCount));
  res.set("models", json::Value::Int(p.modelCount));
  res.set("shaders", json::Value::Int(p.shaderCount));
  o.set("resources", std::move(res));

  json::Value sdk = json::Value::Obj();
  sdk.set("present", json::Value::Bool(p.sdkPresent));
  sdk.set("version", json::Value::Str(p.sdkVersion));
  json::Value::Array miss;
  for (const auto& m : p.sdkMissing) miss.push_back(json::Value::Str(m));
  sdk.set("missing", json::Value::Arr(std::move(miss)));
  o.set("sdk", std::move(sdk));

  json::Value::Array lims;
  for (const auto& l : p.limitations) {
    json::Value lo = json::Value::Obj();
    lo.set("category", json::Value::Str(l.category));
    lo.set("subject", json::Value::Str(l.subject));
    lo.set("reason", json::Value::Str(l.reason));
    lo.set("remediation", json::Value::Str(l.remediation));
    lims.push_back(std::move(lo));
  }
  o.set("limitations", json::Value::Arr(std::move(lims)));
  json::Value::Array ma;
  for (const auto& a : p.manualActions) ma.push_back(json::Value::Str(a));
  o.set("manual_actions", json::Value::Arr(std::move(ma)));
  json::Value::Array rt;
  for (const auto& t : p.requiredTests) rt.push_back(json::Value::Str(t));
  o.set("required_tests", json::Value::Arr(std::move(rt)));
  o.set("verdict", json::Value::Str(p.verdict));
  o.set("no_fake_percent_note",
        json::Value::Str("readiness is not a percentage of files or textures"));
  return o;
}

}  // namespace pc2c::portability
