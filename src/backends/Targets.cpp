#include "pc2c/backends/Targets.h"

#include "pc2c/build/Toolchain.h"

namespace pc2c::backends {

Diagnosis Xbox360Backend::diagnose() const {
  build::XdkInfo x = build::probeXbox360(false);
  Diagnosis d;
  d.backend = name();
  auto cap = [&](std::string n, bool s, std::string note) {
    d.capabilities.push_back({std::move(n), s, std::move(note)});
  };
  cap("sdk-detected", x.found, x.found ? ("XDK root: " + x.root) : "XDK root not found");
  cap("sdk-version-21256", x.version == "21256",
      x.version.empty() ? "xdk.h unreadable" : ("_XDK_VER=" + x.version));
  cap("ppc-compiler", false, "presence checked; build NOT proven until real compile+link");
  for (const auto& c : x.components) {
    if (c.name.find("cl.exe") != std::string::npos || c.name.find("link.exe") != std::string::npos ||
        c.name.find("imagexex") != std::string::npos || c.name.find("fxc") != std::string::npos)
      cap("tool:" + c.name, c.present, c.path);
  }
  cap("vs2010-integration", x.vs2010Present, x.vs2010Note);
  cap("pe-to-xex-direct", false, "Windows PE can never be renamed into an XEX.");
  cap("console-texture-output", false,
      "No verified Xenos tiled-format writer yet; BMP/TGA pipeline is host-side only.");
  cap("min-sample-build", false, "Phase 5: requires executed compile+link, not just presence.");
  d.ready = false;  // honest until Phase 5 passes
  d.blockers.push_back("Backend not build-ready until a minimal XDK sample compiles, links, "
                       "packages (imagexex) and is verified in a legitimate test environment.");
  if (!x.found) d.blockers.push_back("XDK 21256.3 not found at " + x.root + ".");
  for (const auto& m : x.missing) d.blockers.push_back("Missing SDK component: " + m);
  if (!x.vs2010Present) d.blockers.push_back("VS2010 integration missing.");
  d.notes.push_back("Diagnosis is filesystem-only; no binaries were executed.");
  d.notes.push_back("Asset pipeline (BMP/TGA -> profiles) works without the SDK.");
  return d;
}

Status Xbox360Backend::planBuild(const std::string& projectDir, json::Value& out) const {
  Diagnosis d = diagnose();
  out = d.toJson();
  out.set("project_dir", json::Value::Str(projectDir));
  out.set("next_step",
          json::Value::Str("Phase 5: author minimal XDK sample, compile+link with XDK cl/link, "
                           "package with imagexex, verify on test hardware. pc2c reports "
                           "ready=true only after that passes."));
  return Status::Ok();
}

Diagnosis Ps3Backend::diagnose() const {
  Diagnosis d;
  d.backend = name();
  d.capabilities.push_back({"ps3-toolchain", false, "No legitimate PS3 toolchain verified."});
  d.capabilities.push_back(
      {"rsx-graphics", false, "RSX/libgcm research not started; kept separate from Xbox code."});
  d.capabilities.push_back({"cell-spu", false, "PPE/SPE model not implemented."});
  d.capabilities.push_back(
      {"asset-pipeline-reuse", true, "Host-side BMP/TGA profiles are reusable once PS3 gourmet "
                                     "texture formats are researched."});
  d.ready = false;
  d.blockers.push_back("PS3 backend is intentionally future-only until Xbox 360 results are "
                       "documented (see ROADMAP Phase 8).");
  d.blockers.push_back("No PS3 SDK/toolchain present or verified in this environment.");
  d.notes.push_back("Xbox 360 SDK must never be used for PS3 builds.");
  return d;
}

Status Ps3Backend::planBuild(const std::string& projectDir, json::Value& out) const {
  (void)projectDir;
  out = diagnose().toJson();
  return Status::Unsupported("PS3 builds are not implemented in this phase.");
}

}  // namespace pc2c::backends
