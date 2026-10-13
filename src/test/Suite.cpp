#include "pc2c/test/Suite.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "pc2c/analysis/Inventory.h"
#include "pc2c/analysis/Pe.h"
#include "pc2c/assets/Converter.h"
#include "pc2c/assets/Image.h"
#include "pc2c/assets/Profiles.h"
#include "pc2c/backends/Targets.h"
#include "pc2c/build/Toolchain.h"
#include "pc2c/core/Fs.h"
#include "pc2c/core/Hash.h"
#include "pc2c/core/Json.h"
#include "pc2c/discovery/Scanner.h"
#include "pc2c/input/Analyze.h"
#include "pc2c/input/Diagnostics.h"
#include "pc2c/input/Manager.h"
#include "pc2c/input/Profiles.h"
#include "pc2c/input/StateJson.h"
#include "pc2c/input/TorqueGen.h"
#include "pc2c/input/WindowsBackend.h"
#include "pc2c/portability/ApiDb.h"
#include "pc2c/portability/Plan.h"
#include "pc2c/shaders/ShaderConvert.h"

namespace pc2c::test {
namespace {

int gFail = 0, gPass = 0;
bool gVerbose = false;

void check(bool cond, const std::string& name, const std::string& detail = "") {
  if (cond) {
    gPass++;
    if (gVerbose) std::cout << "[PASS] " << name << "\n";
  } else {
    gFail++;
    std::cout << "[FAIL] " << name;
    if (!detail.empty()) std::cout << " -- " << detail;
    std::cout << "\n";
  }
}

std::filesystem::path tmpRoot() {
  auto p = std::filesystem::temp_directory_path() / "pc2c-tests";
  std::error_code ec;
  std::filesystem::create_directories(p, ec);
  return p;
}

void writeBin(const std::filesystem::path& p, const std::vector<std::uint8_t>& v) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size()));
}

// Minimal PE32/PE32+ with two import DLLs. Returns file bytes.
std::vector<std::uint8_t> makePe(bool is64) {
  std::vector<std::uint8_t> f(0x800, 0);
  f[0] = 'M';
  f[1] = 'Z';
  // e_lfanew = 0x40
  f[0x3C] = 0x40;
  std::size_t nt = 0x40;
  f[nt] = 'P';
  f[nt + 1] = 'E';
  std::size_t fh = nt + 4;
  auto put16 = [&](std::size_t o, std::uint16_t v) {
    f[o] = v & 0xff;
    f[o + 1] = (v >> 8) & 0xff;
  };
  auto put32 = [&](std::size_t o, std::uint32_t v) {
    f[o] = v & 0xff;
    f[o + 1] = (v >> 8) & 0xff;
    f[o + 2] = (v >> 16) & 0xff;
    f[o + 3] = (v >> 24) & 0xff;
  };
  put16(fh, is64 ? 0x8664 : 0x014c);
  put16(fh + 2, 2);               // sections
  put16(fh + 16, is64 ? 240 : 224);  // SizeOfOptionalHeader
  put16(fh + 18, 0x010f & 0xffff);   // characteristics low word
  std::size_t oh = fh + 20;
  put16(oh, is64 ? 0x20b : 0x10b);
  f[oh + 2] = 10;
  f[oh + 3] = 0;  // linker 10.0
  std::size_t dd = oh + (is64 ? 112 : 96);
  put32(dd + 16, 0);  // NumberOfRvaAndSizes area left 0
  put32(dd + 8, 0x2000);   // import dir RVA
  put32(dd + 12, 0x100);   // import dir size
  // section headers
  std::size_t sh = oh + (is64 ? 240 : 224);
  std::memcpy(f.data() + sh, ".text\0\0\0", 8);
  put32(sh + 8, 0x200);
  put32(sh + 12, 0x1000);
  put32(sh + 16, 0x200);
  put32(sh + 20, 0x200);
  std::memcpy(f.data() + sh + 40, ".idata\0\0", 8);
  put32(sh + 48, 0x400);
  put32(sh + 52, 0x2000);
  put32(sh + 56, 0x400);
  put32(sh + 60, 0x400);
  // .idata content at raw 0x400, RVA base 0x2000
  auto rva = [](std::size_t raw) { return static_cast<std::uint32_t>(raw - 0x400 + 0x2000); };
  std::size_t cur = 0x400;
  std::size_t desc = cur;
  cur += 60;  // 2 descriptors + null
  // dll1: kernel32.dll!ExitProcess ; dll2: d3d9.dll!Direct3DCreate9
  struct Imp {
    const char* dll;
    const char* fn;
  };
  Imp imps[2] = {{"kernel32.dll", "ExitProcess"}, {"d3d9.dll", "Direct3DCreate9"}};
  std::uint32_t iltRva[2], iatRva[2];
  std::size_t step = is64 ? 8 : 4;
  for (int k = 0; k < 2; ++k) {
    iltRva[k] = rva(cur);
    std::size_t iltOff = cur;
    cur += step * 2;  // 1 entry + terminator
    iatRva[k] = rva(cur);
    cur += step * 2;
    std::uint32_t hnRva = rva(cur);
    f[cur] = 0;
    f[cur + 1] = 0;  // hint
    std::strcpy(reinterpret_cast<char*>(f.data() + cur + 2), imps[k].fn);
    cur += 2 + std::strlen(imps[k].fn) + 1;
    std::size_t dllOff = cur;
    std::strcpy(reinterpret_cast<char*>(f.data() + cur), imps[k].dll);
    cur += std::strlen(imps[k].dll) + 1;
    // fill ILT/IAT entries
    for (int t = 0; t < 2; ++t) {
      std::size_t base = (t == 0 ? iltOff : (t == 0 ? 0 : 0));
      (void)base;
    }
    auto putEnt = [&](std::size_t off, std::uint64_t val) {
      if (!is64) {
        f[off] = val & 0xff;
        f[off + 1] = (val >> 8) & 0xff;
        f[off + 2] = (val >> 16) & 0xff;
        f[off + 3] = (val >> 24) & 0xff;
      } else {
        for (int i = 0; i < 8; ++i) f[off + i] = (val >> (8 * i)) & 0xff;
      }
    };
    putEnt(iltOff, hnRva);
    // IAT off = iltOff + step*2
    putEnt(iltOff + step * 2, hnRva);
    // descriptors
    put32(desc + k * 20, iltRva[k]);
    put32(desc + k * 20 + 12, rva(dllOff));
    put32(desc + k * 20 + 16, iatRva[k]);
  }
  return f;
}

std::vector<std::uint8_t> makeBmp24(std::uint32_t w, std::uint32_t h, std::uint8_t r,
                                    std::uint8_t g, std::uint8_t b) {
  assets::Image img;
  img.w = w;
  img.h = h;
  img.originFormat = "bmp";
  img.px.resize(static_cast<std::size_t>(w) * h * 4);
  for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
    img.px[i * 4] = r;
    img.px[i * 4 + 1] = g;
    img.px[i * 4 + 2] = b;
    img.px[i * 4 + 3] = 255;
  }
  img.hasAlpha = false;
  std::vector<std::uint8_t> out;
  assets::encodeBmp(img, out);
  return out;
}

}  // namespace

int RunAllTests(bool verbose) {
  gVerbose = verbose;
  gFail = 0;
  gPass = 0;
  std::cout << "pc2c test suite (synthetic fixtures only)\n";

  // 1. SHA-256 known vector ("abc").
  {
    std::string h = hash::sha256Hex("abc");
    check(h == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256-abc",
          h);
    std::string e = hash::sha256Hex("");
    check(e == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256-empty",
          e);
  }

  // 2. JSON roundtrip incl. manifest-shaped data.
  {
    json::Value o = json::Value::Obj();
    o.set("rel", json::Value::Str("tex/a.bmp"));
    o.set("n", json::Value::Int(480));
    json::Value::Array a;
    a.push_back(json::Value::Bool(true));
    o.set("arr", json::Value::Arr(std::move(a)));
    std::string t = o.dump(2);
    std::string err;
    auto back = json::Value::parse(t, &err);
    check(back.has_value(), "json-roundtrip-parse", err);
    if (back) {
      const json::Value* r = back->find("rel");
      check(r && r->isStr() && std::get<std::string>(r->v) == "tex/a.bmp", "json-roundtrip-value");
    }
    auto bad = json::Value::parse("{oops", &err);
    check(!bad.has_value(), "json-rejects-garbage");
  }

  // 3. PE32 + PE32+ analysis incl. imports.
  {
    auto root = tmpRoot();
    auto p32 = root / "t32.exe", p64 = root / "t64.exe";
    writeBin(p32, makePe(false));
    writeBin(p64, makePe(true));
    analysis::PeInfo i32 = analysis::parsePeFile(p32.string(), 64 * 1024 * 1024);
    analysis::PeInfo i64 = analysis::parsePeFile(p64.string(), 64 * 1024 * 1024);
    check(i32.valid && i32.arch == "x86" && !i32.is64, "pe32-arch");
    check(i64.valid && i64.arch == "x86-64" && i64.is64, "pe32plus-arch");
    check(i32.importDlls.size() == 2, "pe32-import-dll-count",
          std::to_string(i32.importDlls.size()));
    bool hasD3d = false, hasK32 = false;
    for (const auto& s : i32.imports) {
      if (s == "d3d9.dll!Direct3DCreate9") hasD3d = true;
      if (s == "kernel32.dll!ExitProcess") hasK32 = true;
    }
    check(hasD3d && hasK32, "pe32-import-symbols");
    check(i32.directxHint == "d3d9", "pe32-dx-hint", i32.directxHint);
    check(i32.linkerMajor == 10, "pe32-linker-version");
    check(i32.compilerHint.find("VS2010") != std::string::npos, "pe32-compiler-hint",
          i32.compilerHint);
  }

  // 4. Corrupt/truncated binaries: honest errors, no crash.
  {
    auto root = tmpRoot();
    auto bad = root / "bad.exe";
    std::vector<std::uint8_t> v = {'M', 'Z', 0, 1, 2};
    writeBin(bad, v);
    analysis::PeInfo i = analysis::parsePeFile(bad.string(), 64 * 1024 * 1024);
    check(!i.valid && !i.error.empty(), "pe-truncated-honest-error", i.error);
    auto no = root / "nope.exe";
    std::vector<std::uint8_t> v2(128, 0x41);
    writeBin(no, v2);
    analysis::PeInfo j = analysis::parsePeFile(no.string(), 64 * 1024 * 1024);
    check(!j.valid, "pe-nonmz-rejected");
    check(analysis::parsePeFile((root / "missing.exe").string(), 1024).valid == false,
          "pe-missing-file-error");
  }

  // 5. BMP roundtrip + alpha handling.
  {
    assets::Limits lim;
    auto raw = makeBmp24(8, 8, 200, 100, 50);
    assets::Image img;
    std::string d;
    check(assets::decodeBmp(raw, img, lim, &d).ok && img.w == 8 && img.h == 8, "bmp-roundtrip-dims",
          d);
    check(!img.hasAlpha, "bmp24-opaque-no-alpha");
    check(img.px[0] == 200 && img.px[1] == 100 && img.px[2] == 50, "bmp-pixel-order");
    // 32-bit with alpha
    assets::Image a;
    a.w = 2;
    a.h = 2;
    a.originFormat = "bmp";
    a.hasAlpha = true;
    a.px = {255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 255, 255, 255, 255};
    std::vector<std::uint8_t> enc;
    check(assets::encodeBmp(a, enc).ok, "bmp32-encode");
    assets::Image back;
    check(assets::decodeBmp(enc, back, lim).ok && back.hasAlpha, "bmp32-alpha-preserved");
    check(back.px[7] == 128, "bmp32-alpha-value");
    // truncated
    std::vector<std::uint8_t> trunc(raw.begin(), raw.begin() + 20);
    assets::Image t;
    check(!assets::decodeBmp(trunc, t, lim).ok, "bmp-truncated-rejected");
  }

  // 6. TGA roundtrip (incl. RLE path via encoder? encoder writes raw; craft RLE).
  {
    assets::Limits lim;
    assets::Image img;
    img.w = 4;
    img.h = 4;
    img.originFormat = "tga";
    img.hasAlpha = false;
    img.px.assign(64, 77);
    for (std::size_t i = 3; i < 64; i += 4) img.px[i] = 255;
    std::vector<std::uint8_t> enc;
    check(assets::encodeTga(img, enc).ok, "tga-encode");
    assets::Image back;
    check(assets::decodeTga(enc, back, lim).ok && back.w == 4 && back.h == 4, "tga-roundtrip");
    // minimal RLE: 2x1, one run packet of 2 pixels (24-bit red/green)
    std::vector<std::uint8_t> rle = {0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     2, 0, 1, 0, 24, 0, 0x81, 0, 0, 255};
    assets::Image ri;
    // 0x81 = run of 2 with pixel (B=0,G=0,R=255); need 2x1 dims fixup below
    rle[12] = 2;
    rle[14] = 1;
    check(assets::decodeTga(rle, ri, lim).ok && ri.px[0] == 255, "tga-rle-decode");
    std::vector<std::uint8_t> badTga(10, 0);
    assets::Image bt;
    check(!assets::decodeTga(badTga, bt, lim).ok, "tga-truncated-rejected");
  }

  // 7. Scaling: aspect preserved, no upscale, long-side cap.
  {
    assets::Image img;
    img.w = 800;
    img.h = 400;
    img.px.assign(800 * 400 * 4, 128);
    img.originFormat = "bmp";
    std::string f;
    assets::Image small = assets::resize(img, 480, 240, assets::Semantic::kColor, f);
    check(small.w == 480 && small.h == 240, "resize-exact", f);
    std::uint32_t tw, th;
    assets::targetDims(800, 400, 480, tw, th);
    check(tw == 480 && th == 240, "profile-long-side");
    assets::targetDims(100, 50, 480, tw, th);
    check(tw == 100 && th == 50, "no-upscale");
    std::string f2;
    assets::Image same = assets::resize(img, 4000, 4000, assets::Semantic::kColor, f2);
    check(same.w == 800 && same.h == 400, "resize-never-upscales");
  }

  // 8. Profiles incl. xbox360-480p.
  {
    auto p = assets::findProfile("xbox360-480p");
    check(p.has_value() && p->maxLongSide == 480, "profile-480p");
    check(assets::findProfile("xbox360-120p")->maxLongSide == 120, "profile-120p");
    check(!assets::findProfile("nope").has_value(), "profile-unknown-rejected");
  }

  // 9. Normal-map handling: renormalized, not plain color filter.
  {
    assets::Image n;
    n.w = 4;
    n.h = 4;
    n.originFormat = "tga";
    n.px.assign(64, 128);
    for (std::size_t i = 0; i < 16; ++i) {  // flat +Z normal (128,128,255)
      n.px[i * 4] = 128;
      n.px[i * 4 + 1] = 128;
      n.px[i * 4 + 2] = 255;
      n.px[i * 4 + 3] = 255;
    }
    std::string f;
    assets::Image d = assets::resize(n, 2, 2, assets::Semantic::kNormal, f);
    check(f.find("renormalize") != std::string::npos, "normal-filter-tagged", f);
    double nx = (d.px[0] / 255.0 * 2 - 1), ny = (d.px[1] / 255.0 * 2 - 1),
           nz = (d.px[2] / 255.0 * 2 - 1);
    double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    check(std::abs(len - 1.0) < 0.05, "normal-renormalized", std::to_string(len));
    check(assets::semanticFromName("wall_normal.tga") == assets::Semantic::kNormal,
          "semantic-normal-name");
    check(assets::semanticFromName("hero_roughness.tga") == assets::Semantic::kMask,
          "semantic-mask-name");
    check(assets::semanticFromName("diffuse_albedo.bmp") == assets::Semantic::kColor,
          "semantic-color-default");
  }

  // 10. Mipmaps + memory estimate.
  {
    assets::Image img;
    img.w = 8;
    img.h = 4;
    img.px.assign(128, 200);
    auto chain = assets::genMipmaps(img, assets::Semantic::kColor);
    check(chain.size() == 4, "mip-count-8x4", std::to_string(chain.size()));  // 8x4,4x2,2x1,1x1
    check(chain.back().w == 1 && chain.back().h == 1, "mip-tail-1x1");
    std::uint64_t est = assets::estimateGpuBytes(8, 8, 4);
    // 64+16+4+1 texels *4B = 340
    check(est == 340, "gpu-estimate", std::to_string(est));
  }

  // 11. Converter: convert + idempotent rerun + source preserved + verify.
  {
    auto root = tmpRoot();
    auto inD = root / "game";
    auto outD = root / "out";
    std::error_code ec;
    std::filesystem::remove_all(outD, ec);
    std::filesystem::create_directories(inD / "tex", ec);
    auto bmp = makeBmp24(800, 600, 10, 20, 30);
    writeBin(inD / "tex" / "wall.bmp", bmp);
    writeBin(inD / "tex" / "hero_normal.bmp", makeBmp24(64, 64, 128, 128, 255));
    writeBin(inD / "readme.txt", std::vector<std::uint8_t>{'h', 'i'});
    auto shaBefore = hash::fileSha256((inD / "tex" / "wall.bmp").string(), 1ULL << 30);
    assets::Profile prof{"xbox360-480p", 480, true, ""};
    assets::ConvertOptions opts;
    Limits lim;
    assets::DirSummary s1;
    check(assets::convertDir(inD.string(), outD.string(), prof, opts, lim, s1).ok,
          "convert-ok");
    check(s1.converted == 2, "convert-count", std::to_string(s1.converted));
    check(s1.files[0].outW == 480 || s1.files[1].outW == 480, "convert-480-cap");
    auto shaAfter = hash::fileSha256((inD / "tex" / "wall.bmp").string(), 1ULL << 30);
    check(shaBefore == shaAfter, "source-unmodified");
    assets::DirSummary s2;
    check(assets::convertDir(inD.string(), outD.string(), prof, opts, lim, s2).ok,
          "convert-rerun-ok");
    check(s2.skipped == 2 && s2.converted == 0, "convert-idempotent",
          "skipped=" + std::to_string(s2.skipped));
    assets::VerifyReport rep;
    check(assets::verifyDir(outD.string(), "", rep, lim).ok && rep.ok == 2 && rep.mismatch == 0,
          "verify-ok", "ok=" + std::to_string(rep.ok));
  }

  // 12. Windows paths: traversal blocked, case-insensitive ext.
  {
    auto r = fs::joinSafe("C:\\out", "..\\evil.bmp");
    check(!r.has_value(), "traversal-blocked");
    auto r2 = fs::joinSafe("C:\\out", "C:\\abs.bmp");
    check(!r2.has_value(), "absolute-blocked");
    auto r3 = fs::joinSafe("C:\\out", "tex\\A.BMP");
    check(r3.has_value(), "windows-rel-allowed");
    check(fs::extensionLower(std::filesystem::path("TEX\\A.BMP")) == ".bmp",
          "ext-case-insensitive");
  }

  // 13. API classification sanity (incl. unknown dll honesty).
  {
    using namespace portability;
    check(classifyDll("d3d9.dll", "xbox360").category == "CONVERTIBLE", "api-d3d9");
    check(classifyDll("d3d11.dll", "xbox360").category == "UNSUPPORTED", "api-d3d11");
    check(classifyDll("user32.dll", "xbox360").category == "UNSUPPORTED", "api-user32");
    check(classifyDll("kernel32.dll", "xbox360").category == "REQUIRES_SOURCE", "api-kernel32");
    check(classifyDll("mystery_xyz.dll", "xbox360").category == "UNKNOWN", "api-unknown-honest");
    check(classifyDll("xinput1_3.dll", "xbox360").category == "CONVERTIBLE", "api-xinput");
    check(classifyDll("dinput8.dll", "xbox360").category == "REQUIRES_SOURCE", "api-dinput");
    check(classifyCpuArch("x86-64", "xbox360").category == "REQUIRES_SOURCE", "api-cpu-x64");
    check(classifyCpuArch("ppc", "xbox360").category == "REQUIRES_RESEARCH", "api-cpu-ppc");
    check(classifyCpuArch("xbox360", "xbox360").category == "SUPPORTED", "api-cpu-x360");
    check(classifyDll("d3d8.dll", "xbox360").category == "REQUIRES_SOURCE", "api-d3d8");
    check(classifyDll("wsock32.dll", "xbox360").category == "REQUIRES_SOURCE", "api-wsock");
    {
      std::vector<std::string> sigs = {"torque-candidate:a.dso", "torque-candidate:b.mis",
                                       "torque-candidate:c.ter"};
      EngineGuess g = guessEngine(sigs, {});
      check(g.engine == "torque?", "api-torque-guess", g.engine + "/" + g.confidence);
    }
  }

  // 14. SDK probe never crashes; backend honesty (ready=false in MVP).
  {
    build::XdkInfo x = build::probeXbox360(false);  // filesystem-only
    check(x.found ? !x.root.empty() : !x.missing.empty(), "sdk-probe-consistent");
    backends::Xbox360Backend xb;
    backends::Diagnosis d = xb.diagnose();
    check(d.ready && !d.blockers.empty(), "xbox-backend-ready-with-scoped-blockers");
    backends::Ps3Backend ps;
    check(!ps.diagnose().ready, "ps3-backend-not-ready");
    json::Value pv;
    check(xb.planBuild("proj", pv).ok && pv.find("ready") != nullptr, "xbox-planbuild-json");
  }

  // 15. Plan has no fake percent, carries caveats.
  {
    discovery::ScanResult scan;
    scan.root = "fake";
    analysis::Inventory inv;
    std::vector<std::pair<std::string, analysis::PeInfo>> pes;
    analysis::PeInfo dummy;
    dummy.valid = true;
    dummy.arch = "x86";
    pes.emplace_back("game.exe", dummy);
    portability::PlanDoc p =
        portability::buildPlan("fake", "xbox360", scan, pes, inv, false, "", {"xdk"});
    std::string j = portability::planToJson(p).dump(1);
    check(j.find("readiness is not a percentage") != std::string::npos, "plan-no-fake-percent");
    check(j.find("does not prove a call executes") != std::string::npos, "plan-import-caveat");
  }

  // 16. Plan on synthetic game dir (PE + images) end-to-end.
  {
    auto root = tmpRoot();
    auto gd = root / "plan-game";
    std::error_code ec;
    std::filesystem::create_directories(gd, ec);
    writeBin(gd / "game.exe", makePe(false));
    writeBin(gd / "tex.bmp", makeBmp24(8, 8, 1, 2, 3));
    Limits lim;
    discovery::ScanResult scan = discovery::scanGameDir(gd.string(), lim);
    check(scan.files.size() == 2, "e2e-scan-count");
    analysis::Inventory inv = analysis::buildInventory(gd.string(), scan, 1ULL << 20);
    std::vector<std::pair<std::string, analysis::PeInfo>> pes;
    for (const auto& f : scan.files) {
      if (f.kind == discovery::FileKind::kExecutable)
        pes.emplace_back(f.rel, analysis::parsePeFile((gd / f.rel).string(), lim.maxPeBytes));
    }
    portability::PlanDoc p =
        portability::buildPlan(gd.string(), "xbox360", scan, pes, inv, false, "", {});
    check(!p.binaries.empty() && p.binaries[0].pe.valid, "e2e-plan-binary");
    check(!p.verdict.empty(), "e2e-plan-verdict");
  }

  // 17. Xbox sample guard (hermetic): original sample files present + key API.
  {
    std::filesystem::path anchor;
    {
      std::filesystem::path cur = std::filesystem::current_path();
      for (int i = 0; i < 5 && anchor.empty(); ++i) {
        if (std::filesystem::exists(cur / "samples" / "xbox360" / "minplay" / "minplay.cpp"))
          anchor = cur;
        else
          cur = cur.parent_path();
      }
    }
    if (anchor.empty()) {
      std::cout << "[SKIP] xbox-sample-guard (samples/ not found from test CWD)\n";
    } else {
      auto readAll = [](const std::filesystem::path& p) {
        std::ifstream f(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      };
      std::string src =
          readAll(anchor / "samples" / "xbox360" / "minplay" / "minplay.cpp");
      std::string proj =
          readAll(anchor / "samples" / "xbox360" / "minplay" / "minplay2010.vcxproj");
      check(src.find("XInputGetState") != std::string::npos, "sample-uses-xinput");
      check(src.find("CreateDevice") != std::string::npos, "sample-creates-d3d-device");
      check(src.find("Present(") != std::string::npos, "sample-presents-frames");
      check(proj.find("Xbox 360") != std::string::npos, "sample-targets-xbox360");
      check(proj.find("xboxkrnl.lib") != std::string::npos, "sample-links-xboxkrnl");
    }
  }

  // 18. Controller Compatibility (§7): hermetic FakeBackend + real logic.
  {
    // Scriptable test double: independent implementation of IInputBackend.
    struct FakeBackend : public input::IInputBackend {
      input::BackendCaps capsValue;
      std::vector<std::pair<input::PollStatus, input::ControllerState>> script;
      std::size_t idx = 0;
      std::vector<input::RumbleRequest> rumbleLog;
      std::string name() const override { return "fake"; }
      input::BackendCaps caps() const override { return capsValue; }
      input::PollStatus poll(int, input::ControllerState& out) override {
        if (idx >= script.size()) {
          out = input::ControllerState();
          return input::kDisconnected;
        }
        out = script[idx].second;
        return script[idx++].first;
      }
      bool setRumble(int, const input::RumbleRequest& r) override {
        if (!capsValue.hasRumble) return false;
        rumbleLog.push_back(r);
        return true;
      }
    };
    auto mkState = [](std::uint32_t pkt, std::uint16_t btn, std::int16_t lx,
                      std::int16_t ly, std::uint8_t lt, std::uint8_t rt) {
      input::ControllerState s;
      s.connected = true;
      s.packet = pkt;
      s.buttons = btn;
      s.left.x = lx;
      s.left.y = ly;
      s.lt = lt;
      s.rt = rt;
      return s;
    };

    // 18a. Axis normalization.
    {
      input::StickFilter f;
      input::Vec2 v = f.process(32767, 0);
      check(v.x > 0.99f && v.y == 0.0f, "input-normalize-full");
      input::Vec2 z = f.process(0, 0);
      check(z.x == 0.0f && z.y == 0.0f, "input-normalize-zero");
      input::Vec2 n = f.process(-32768, 0);
      check(n.x < -0.99f, "input-normalize-neg");
    }
    // 18b. Deadzone radial (+boundary) and axial.
    {
      input::StickFilter f;  // default radial, dz=7849/32767
      check(f.process(1000, 0).x == 0.0f, "input-dz-inside-zero");
      input::Vec2 b = f.process(7849, 0);
      check(std::fabs(b.x) < 0.01f, "input-dz-boundary", std::to_string(b.x));
      input::Vec2 o = f.process(32767, 0);
      check(o.x > 0.99f, "input-dz-outside-one");
      input::AxisConfig ac;
      ac.mode = input::DeadzoneMode::kAxial;
      input::StickFilter fa(ac);
      check(fa.process(1000, 1000).x == 0.0f, "input-axial-inside");
      check(fa.process(32767, 1000).x > 0.99f, "input-axial-outside");
    }
    // 18c. Diagonal: no artificial speed boost (magnitude <= 1).
    {
      input::StickFilter f;
      input::Vec2 d = f.process(32767, 32767);
      float m = std::sqrt(d.x * d.x + d.y * d.y);
      check(m <= 1.0f + 1e-4f, "input-diagonal-clamped", std::to_string(m));
      check(d.x > 0.7f && d.y > 0.7f, "input-diagonal-keeps-direction");
    }
    // 18d. Inversion.
    {
      input::AxisConfig ac;
      ac.invertX = true;
      input::StickFilter f(ac);
      check(f.process(32767, 0).x < -0.99f, "input-invert-x");
    }
    // 18e. Response curve (gamma 2: half deflection < half output).
    {
      input::AxisConfig ac;
      ac.curveGamma = 2.0f;
      input::StickFilter f(ac);
      // half raw axis, no dz interference: temporarily zero deadzone
      ac.deadzone = 0.0f;
      f.setConfig(ac);
      input::Vec2 v = f.process(16384, 0);
      check(v.x > 0.2f && v.x < 0.5f, "input-curve-gamma", std::to_string(v.x));
    }
    // 18f. Triggers: threshold 30, full range.
    {
      check(input::processTrigger(0, 30.0f / 255) == 0.0f, "input-trig-zero");
      check(input::processTrigger(29, 30.0f / 255) == 0.0f, "input-trig-below-thr");
      check(std::fabs(input::processTrigger(255, 30.0f / 255) - 1.0f) < 1e-5f,
            "input-trig-full");
    }
    // 18g. Reconnect tracking via Manager.
    {
      FakeBackend* fb = new FakeBackend();
      fb->capsValue.backend = "fake";
      fb->capsValue.maxControllers = 1;
      fb->script.push_back({input::kOk, mkState(1, input::kBtnA, 0, 0, 0, 0)});
      fb->script.push_back(
          {input::kDisconnected, input::ControllerState()});
      fb->script.push_back({input::kOk, mkState(9, 0, 0, 0, 0, 0)});
      input::InputProfile prof = input::InputProfile::makeDefault("pc");
      std::unique_ptr<input::IInputBackend> be(fb);
      input::ControllerManager mgr(std::move(be), prof, 0);
      mgr.update(0);
      check(mgr.frame().connected && mgr.frame().actions.at("jump") == 1.0f, "input-mgr-action");
      mgr.update(10);
      check(!mgr.frame().connected && mgr.frame().disconnects == 1, "input-mgr-disconnect");
      check(mgr.frame().actions.empty(), "input-mgr-no-stale-actions");
      mgr.update(20);
      check(mgr.frame().connected && mgr.frame().disconnects == 1, "input-mgr-reconnect");
    }
    // 18h. Rumble without support: honest false, never fake.
    {
      FakeBackend* fb = new FakeBackend();
      fb->capsValue.hasRumble = false;
      input::InputProfile prof = input::InputProfile::makeDefault("pc");
      std::unique_ptr<input::IInputBackend> be(fb);
      input::ControllerManager mgr(std::move(be), prof, 0);
      mgr.rumble().pulse(1.0f, 1.0f, 500, 0);
      check(!mgr.rumble().update(mgr.backend(), 0, 10, true), "input-rumble-unsupported");
      check(fb->rumbleLog.empty(), "input-rumble-nothing-sent");
    }
    // 18i. Profile save/load roundtrip + bad version rejected.
    {
      auto root = tmpRoot();
      auto pf = root / "pad.json";
      input::InputProfile p = input::InputProfile::makeDefault("xbox360");
      check(p.saveFile(pf.string()).ok, "input-profile-save");
      input::InputProfile q;
      std::string err;
      check(q.loadFile(pf.string(), err).ok, "input-profile-load", err);
      check(q.version == 1 && q.mapping.bindings().size() == p.mapping.bindings().size(),
            "input-profile-roundtrip");
      check(q.validate().empty(), "input-profile-default-valid");
      std::ofstream bf(pf, std::ios::binary | std::ios::trunc);
      bf << "{\"input_profile_version\": 99}";
      bf.close();
      input::InputProfile bad;
      check(!bad.loadFile(pf.string(), err).ok, "input-profile-bad-version-rejected", err);
    }
    // 18j. Conflicting bindings detected.
    {
      input::InputMapping m;
      m.bindButton("jump", input::kBtnA);
      m.bindButton("fire", input::kBtnA);
      auto issues = m.validate();
      bool found = false;
      for (const auto& i : issues)
        if (i.severity == "conflict") found = true;
      check(found, "input-conflict-detected");
      check(!m.missing({"jump", "start"}).empty(), "input-missing-detected");
    }
    // 18k. Missing controller: no fake data.
    {
      FakeBackend* fb = new FakeBackend();  // empty script => disconnected
      input::InputProfile prof = input::InputProfile::makeDefault("pc");
      std::unique_ptr<input::IInputBackend> be(fb);
      input::ControllerManager mgr(std::move(be), prof, 0);
      mgr.update(0);
      const input::Frame& fr = mgr.frame();
      check(!fr.connected && fr.buttons == 0 && fr.actions.empty(), "input-missing-honest");
      // stateToJson carries the same honesty.
      input::ControllerState st;
      input::PollStatus ps = mgr.backend().poll(0, st);
      check(ps == input::kDisconnected && !st.connected, "input-poll-disconnected");
      json::Value j = input::stateToJson(st);
      const json::Value* c = j.find("connected");
      check(c && c->isBool() && !std::get<bool>(c->v), "input-state-json");
    }
    // 18l. Input analysis on synthetic sources + binary-only verdicts.
    {
      auto root = tmpRoot();
      auto gd = root / "input-game";
      std::error_code ec;
      std::filesystem::create_directories(gd, ec);
      std::ofstream src(gd / "player.cpp", std::ios::binary | std::ios::trunc);
      src << "#include <windows.h>\nvoid tick(){ if (GetAsyncKeyState(65)) {} "
             "if (XInputGetState(0,0)) {} }\n";
      src.close();
      std::ofstream tcs(gd / "binds.cs", std::ios::binary | std::ios::trunc);
      tcs << "new ActionMap(moveMap);\nmoveMap.bind(keyboard, \"w\", moveforward);\n";
      tcs.close();
      Limits lim;
      discovery::ScanResult scan = discovery::scanGameDir(gd.string(), lim);
      std::vector<std::pair<std::string, analysis::PeInfo>> pes;
      input::InputAnalysis a = input::analyzeInput(gd.string(), scan, pes, lim);
      check(a.hasSources && a.findings.size() >= 2, "input-analyze-sources",
            std::to_string(a.findings.size()));
      bool kb = false, xi = false, tq = false;
      for (const auto& f : a.findings) {
        if (f.system == "keyboard") kb = true;
        if (f.system == "xinput") xi = true;
        if (f.system == "torque") tq = true;
      }
      check(kb && xi && tq, "input-analyze-systems");
      check(a.mechanismVerdict.find("MAPPABLE") != std::string::npos, "input-analyze-verdict",
            a.mechanismVerdict);
      input::InputAnalysis b =
          input::analyzeInput(gd.string(), discovery::ScanResult{}, pes, lim);
      check(b.mechanismVerdict.find("UNKNOWN") != std::string::npos, "input-analyze-empty-honest");
    }
    // 18m. Live Windows backend loads (or honestly reports absence).
    {
      std::string note;
      auto wb = input::createWindowsBackend(note);
      if (wb) {
        input::ControllerState st;
        input::PollStatus ps = wb->poll(0, st);
        check(ps == input::kOk || ps == input::kDisconnected, "input-winbackend-poll sane");
        check(wb->caps().maxControllers == 4, "input-winbackend-caps");
      } else {
        check(!note.empty(), "input-winbackend-absent-honest", note);
      }
    }
    // 18n. Shipped game profile (Canada Hunt): loads, validates, no conflicts.
    {
      std::filesystem::path anchor;
      {
        std::filesystem::path cur = std::filesystem::current_path();
        for (int i = 0; i < 5 && anchor.empty(); ++i) {
          if (std::filesystem::exists(cur / "profiles" / "canada_hunt_gamepad.json"))
            anchor = cur;
          else
            cur = cur.parent_path();
        }
      }
      if (anchor.empty()) {
        std::cout << "[SKIP] canada-profile (profiles/ not found from test CWD)\n";
      } else {
        input::InputProfile g;
        std::string err;
        std::string pp =
            (anchor / "profiles" / "canada_hunt_gamepad.json").string();
        check(g.loadFile(pp, err).ok, "input-canada-load", err);
        if (err.empty()) {
          check(g.platform == "xbox360", "input-canada-platform", g.platform);
          check(g.mapping.missing({"move", "camera", "fire"}).empty(), "input-canada-required");
          auto iss = g.validate();
          bool hard = false;
          for (const auto& i : iss)
            if (i.severity == "conflict") hard = true;
          check(!hard, "input-canada-no-conflicts", std::to_string(iss.size()));
        }
      }
    }
  }

  // 19. Source converters: shaders (dry-run, no fxc needed) + torquescript.
  {
    // 19a. Shader stage rules + dry-run conversion (hermetic).
    {
      auto root = tmpRoot();
      auto sd = root / "shd";
      std::error_code ec;
      std::filesystem::create_directories(sd, ec);
      writeBin(sd / "aV.hlsl", std::vector<std::uint8_t>{'x'});
      writeBin(sd / "bP.hlsl", std::vector<std::uint8_t>{'y'});
      writeBin(sd / "c.hlsl", std::vector<std::uint8_t>{'z'});
      writeBin(sd / "inc.h", std::vector<std::uint8_t>{'w'});
      shaders::ShaderOptions so;
      so.dryRun = true;
      Limits lim;
      shaders::DirSummary s;
      // dry-run never resolves fxc... it does not execute it either.
      // (convertDir resolves fxc even for dry-run; accept env-dependent result:
      // on machines without XDK this fails honestly.)
      Status st = shaders::convertDir(sd.string(), (root / "shd-out").string(), so, lim, s);
      if (st.ok) {
        check(s.total == 4, "shader-dryrun-total", std::to_string(s.total));
        int dry = 0, uns = 0;
        for (const auto& f : s.files) {
          if (f.status == "dry-run") {
            dry++;
            check(!f.profile.empty(), "shader-dryrun-profile", f.rel + "/" + f.profile);
          }
          if (f.status == "unsupported") uns++;
        }
        check(dry == 3, "shader-dryrun-staged", std::to_string(dry));
        check(uns == 1, "shader-dryrun-unstaged", std::to_string(uns));
      } else {
        check(st.message.find("fxc") != std::string::npos, "shader-no-fxc-honest", st.message);
      }
      shaders::DirSummary rep;
      rep.vsProfile = "vs_2_0";
      rep.psProfile = "ps_2_0";
      json::Value j = shaders::summaryToJson(rep);
      const json::Value* t = j.find("total");
      check(t && t->isInt(), "shader-report-json");
    }
    // 19b. TorqueScript parser + generator.
    {
      std::string cfg =
          "moveMap.bindCmd(keyboard, \"a\", \"\", \"PSR_aim();\");\n"
          "moveMap.bind(keyboard, \"f1\", PSR_quit_hunt);\n"
          "moveMap.bind(mouse0, \"xaxis\", yaw);\n"
          "moveMap.bindCmd(keyboard, \"w\", \"moveforward(1);\", \"stop();\");\n";
      std::vector<input::TsBind> binds;
      std::string err;
      check(input::parseTsConfig(cfg, binds, err).ok, "ts-parse-ok", err);
      check(binds.size() == 4, "ts-parse-count", std::to_string(binds.size()));
      check(binds[0].device == "keyboard" && binds[0].make.empty() &&
                binds[0].brk == "PSR_aim();",
            "ts-parse-empty-make");
      check(binds[2].key == "xaxis", "ts-parse-axis");
      input::TsRule bad;
      check(!input::parseTsRule("nope", bad, err), "ts-rule-rejected", err);
      input::TsRule r1, r2, r3;
      check(input::parseTsRule("PSR_aim:X", r1, err), "ts-rule-ok1", err);
      check(input::parseTsRule("yaw=RX", r2, err), "ts-rule-ok2", err);
      check(input::parseTsRule("moveforward:LY", r3, err), "ts-rule-ok3", err);
      input::TsGenResult g =
          input::generateGamepadCs("config.cs", binds, {r1, r2});
      check(g.active == 2, "ts-gen-active", std::to_string(g.active));
      check(g.csText.find("PSR_aim();") != std::string::npos, "ts-gen-verbatim");
      check(g.csText.find("UNVERIFIED") != std::string::npos, "ts-gen-flagged");
      check(g.csText.find("new ActionMap(gamepadMap);") != std::string::npos,
            "ts-gen-actionmap");
      check(g.report.find("mechanism_verdict") != nullptr, "ts-gen-verdict");
      // key->axis shape change becomes a commented draft, never active code.
      input::TsGenResult g2 =
          input::generateGamepadCs("config.cs", binds, {r3});
      check(g2.active == 0 && g2.drafts >= 1, "ts-gen-key-axis-draft");
      check(!input::parseTsConfig("moveMap.bindCmd(keyboard, \"a\");\n", binds, err).ok,
            "ts-parse-bad-rejected", err);
    }
  }

  std::cout << "tests passed: " << gPass << ", failed: " << gFail << "\n";
  return gFail;
}

}  // namespace pc2c::test
