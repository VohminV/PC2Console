#include "pc2c/portability/ApiDb.h"

#include <algorithm>

namespace pc2c::portability {
namespace {

ApiVerdict v(std::string c, std::string r, std::string m = "") {
  return ApiVerdict{std::move(c), std::move(r), std::move(m)};
}

bool contains(const std::string& s, const char* sub) { return s.find(sub) != std::string::npos; }

}  // namespace

ApiVerdict classifyDll(const std::string& dllLower, const std::string& target) {
  const std::string& d = dllLower;
  bool xbox = (target == "xbox360");
  // CRT / low-level runtime: Xbox 360 XDK ships libcMT/libcpMT (confirmed in lib/xbox).
  if (d == "msvcrt.dll" || d == "msvcr70.dll" || d == "msvcr71.dll" || d == "msvcr80.dll" ||
      d == "msvcr90.dll" || d == "msvcr100.dll" || d == "msvcr110.dll" || d == "msvcr120.dll" ||
      d == "msvcp60.dll" || d == "msvcp70.dll" || d == "msvcp71.dll" || d == "msvcp80.dll" ||
      d == "msvcp90.dll" || d == "msvcp100.dll" || d == "msvcp110.dll" || d == "msvcp120.dll" ||
      d == "vcruntime140.dll" || d == "ucrtbase.dll" || d == "api-ms-win-crt-runtime-l1-1-0.dll" ||
      contains(d, "api-ms-win-crt")) {
    if (xbox)
      return v("SUPPORTED", "C/C++ runtime has Xbox 360 counterparts (libcMT/libcpMT in XDK lib).",
               "Rebuild against XDK CRT; verify each CRT function used.");
    return v("REQUIRES_RESEARCH", "PS3 CRT story not verified in this environment.",
             "Verify against legitimate PS3 toolchain docs.");
  }
  if (d == "kernel32.dll" || d == "advapi32.dll" || d == "version.dll")
    return v("REQUIRES_SOURCE",
             "Win32 base services have no binary-compatible Xbox 360 equivalent.",
             "Re-implement file/registry/process helpers on Xbox file + XAM APIs; needs source.");
  if (d == "user32.dll" || d == "gdi32.dll" || d == "comctl32.dll" || d == "comdlg32.dll" ||
      d == "shell32.dll" || d == "ole32.dll" || d == "oleaut32.dll" || d == "shlwapi.dll" ||
      d == "uxtheme.dll" || d == "dwmapi.dll")
    return v("UNSUPPORTED",
             "Win32 windowing/shell has no Xbox 360 equivalent. This also covers "
             "keyboard/mouse/window-message input: there is no mouse and no Win32 "
             "keyboard-message pump on Xbox 360 (text entry goes through IME/virtual-keyboard "
             "paths such as xime.h, not window messages).",
             "Remove or redesign UI and ALL mouse/keyboard input around the gamepad (XInput2); "
             "needs source + input-design work.");
  if (d == "ws2_32.dll" || d == "wininet.dll" || d == "winhttp.dll" || d == "dnsapi.dll")
    return v("REQUIRES_SOURCE", "Sockets differ (xnet/winsockx on Xbox 360).",
             "Port netcode to Xbox secure sockets; needs source + test console/service access.");
  if (d == "winmm.dll" || d == "dsound.dll" || d == "dmusic.dll" || d == "dinput8.dll" ||
      d == "dinput.dll")
    return v("REQUIRES_SOURCE",
             "Legacy WinMM/DirectSound/DirectInput need XAudio2/XInput2 ports. DirectInput "
             "devices (incl. mouse/keyboard/joysticks) map to the XInput2 sample/getter model "
             "(XInput2Sample + XInput2Get*, xinput2.lib/xinputremap.lib in XDK); there is no "
             "1:1 mapping for mice.",
             "Needs source; map gameplay input to XInput2 gamepad controls, drop mouse input, "
             "redesign UI navigation.");
  if (contains(d, "d3d9") || d == "d3dx9_24.dll" || d == "d3dx9_25.dll" || d == "d3dx9_26.dll" ||
      d == "d3dx9_27.dll" || d == "d3dx9_28.dll" || d == "d3dx9_29.dll" || d == "d3dx9_30.dll" ||
      d == "d3dx9_31.dll" || d == "d3dx9_32.dll" || d == "d3dx9_33.dll" || d == "d3dx9_34.dll" ||
      d == "d3dx9_35.dll" || d == "d3dx9_36.dll" || d == "d3dx9_37.dll" || d == "d3dx9_38.dll" ||
      d == "d3dx9_39.dll" || d == "d3dx9_40.dll" || d == "d3dx9_41.dll" || d == "d3dx9_42.dll" ||
      d == "d3dx9_43.dll") {
    if (xbox)
      return v("CONVERTIBLE",
               "D3D9-class API maps to Xbox 360 xgraphics/d3d9 (d3dx9.lib ships in XDK). "
               "Shaders still need Xbox fxc recompile; resource layout differs.",
               "Rebuild renderer against XDK D3D headers; recompile shaders with Xbox fxc; "
               "adapt render targets/tiling. Needs source.");
    return v("REQUIRES_RESEARCH", "PS3 uses RSX/libgcm, not D3D9.",
             "Rewrite renderer for RSX backend; needs source.");
  }
  if (contains(d, "d3d10") || contains(d, "d3d11") || d == "dxgi.dll" || contains(d, "d3d12") ||
      contains(d, "d3dcompiler"))
    return v("UNSUPPORTED",
             "D3D10/11/12 and DXGI have no Xbox 360 equivalent; feature level exceeds Xenos D3D9-class.",
             "Rewrite renderer at D3D9-class feature level; needs source + shader rework.");
  if (contains(d, "xinput") || d == "xaudio2_0.dll" || d == "xaudio2_1.dll" ||
      d == "xaudio2_2.dll" || d == "xaudio2_3.dll" || d == "xaudio2_4.dll" ||
      d == "xaudio2_5.dll" || d == "xaudio2_6.dll" || d == "xaudio2_7.dll" ||
      d == "xaudio2_8.dll" || d == "xaudio2_9.dll" || d == "xapofx1_1.dll" ||
      d == "x3daudio1_0.dll" || d == "x3daudio1_7.dll") {
    if (xbox)
      return v("CONVERTIBLE",
               "PC XInput maps to Xbox 360 XInput (xbox.h: XInputGetState/SetState confirmed; "
               "XINPUT_STATE/VIBRATION in xinputdefs.h; xinput2.lib/xinputremap.lib in XDK; "
               "deadzones/triggers in xinputdefs.h). Classic GetState polling exists on 360, "
               "plus the XInput2 sample model for exotic devices. XAudio2 exists on 360 "
               "(xaudio2 libs in XDK).",
               "Poll via XInputGetState + deadzones; remap device variety via xinputremap; "
               "needs source + gamepad hardware test.");
    return v("REQUIRES_RESEARCH", "Sony equivalents differ; needs PS3 toolchain.",
             "Map to PS3 pad/audio APIs; needs source.");
  }
  if (d == "opengl32.dll" || d == "glu32.dll" || contains(d, "vulkan") || d == "nvapi.dll" ||
      d == "amd_ags_x64.dll")
    return v("UNSUPPORTED", "OpenGL/Vulkan/vendor extensions have no Xbox 360 equivalent.",
             "Rewrite renderer for Xbox D3D; needs source.");
  if (contains(d, "physx") || contains(d, "havok") || contains(d, "bink") ||
      contains(d, "fmod") || contains(d, "wwise") || contains(d, "miles") || contains(d, "mss32") ||
      contains(d, "openal") || contains(d, "sdl") || contains(d, "ogg") || contains(d, "vorbis") ||
      contains(d, "theora") || contains(d, "vivox") || contains(d, "steam_api") ||
      contains(d, "galaxy") || contains(d, "eos"))
    return v("REQUIRES_RESEARCH", "Third-party middleware needs an Xbox 360 build + license.",
             "Obtain console-licensed middleware build; verify symbols; needs source + license.");
  if (contains(d, "unityplayer") || contains(d, "mono") || contains(d, "unreal") ||
      contains(d, "cry") || contains(d, " Dunia") || contains(d, "xna") ||
      contains(d, "microsoft.xna"))
    return v("REQUIRES_SOURCE",
             "Engine runtime needs a console-licensed engine port; binary reuse impossible.",
             "Needs engine source/license + console engine build; game code adaptation.");
  if (contains(d, "mscor") || d == "mscorjit.dll" || contains(d, ".net"))
    return v("UNSUPPORTED", "No CLR on Xbox 360 retail path for arbitrary assemblies.",
             "Managed code needs engine-level port (e.g. XNA path) or rewrite.");
  if (contains(d, "ntdll") || d == "psapi.dll" || d == "dbghelp.dll" || d == "powrprof.dll" ||
      d == "setupapi.dll" || d == "cfgmgr32.dll" || d == "hid.dll" || d == "winusb.dll")
    return v("REQUIRES_SOURCE", "Low-level OS/driver surface differs on console.",
             "Replace with console APIs; needs source.");
  return v("UNKNOWN", "No rule for this dependency; manual review required.",
           "Identify library origin and check XDK lib/include for a counterpart.");
}

ApiVerdict classifyCpuArch(const std::string& arch, const std::string& target) {
  if (target != "xbox360" && target != "ps3")
    return v("UNKNOWN", "Unknown target.", "Pass --target xbox360|ps3.");
  if (arch == "x86" || arch == "x86-64")
    return v("REQUIRES_SOURCE",
             "Windows x86/x86-64 machine code cannot run on PowerPC Xenon / Cell PPE. "
             "PE != XEX: format, opcodes, ABI and syscalls all differ.",
             "Full recompile from source with console compiler; no binary reuse.");
  if (arch == "arm" || arch == "armv7" || arch == "arm64")
    return v("REQUIRES_SOURCE", "ARM code is not executable on Xbox 360 / PS3 CPUs.",
             "Full recompile from source.");
  if (arch == "ppc" || arch == "ppc64")
    return v("REQUIRES_RESEARCH",
             "PowerPC code is closer but still needs XEX packaging, ABI and API adaptation.",
             "Verify object compatibility; repackage via imagexex; needs testing on hardware.");
  if (arch == "xbox360")
    return v(target == "xbox360" ? "SUPPORTED" : "REQUIRES_SOURCE",
             target == "xbox360"
                 ? "Xbox 360 machine code already (XDK link output); still needs XEX packaging, "
                   "API review and hardware/executor verification."
                 : "Xbox 360 machine code cannot run on PS3.",
             target == "xbox360" ? "Package via imagexex; verify on test hardware."
                                 : "Full recompile from source.");
  return v("UNKNOWN", "Unrecognized CPU architecture string: " + arch + ".",
           "Inspect binary headers manually.");
}

ApiVerdict classifyDirectx(const std::string& dx, const std::string& target) {
  if (dx == "d3d9" || dx == "d3d9+d3dx")
    return v("CONVERTIBLE", "D3D9-class renderer is the adaptable path for Xbox 360.",
             "See D3D9 DLL verdicts; recompile shaders; needs source.");
  if (dx == "d3d10" || dx == "d3d11" || dx == "d3d12")
    return v("UNSUPPORTED", dx + " exceeds Xbox 360 GPU/API capabilities.",
             "Renderer rewrite at D3D9-class level; needs source.");
  if (dx == "none-detected")
    return v("UNKNOWN", "No DirectX imports detected (software GL, middleware, or static link?).",
             "Check engine guess + string scan for opengl/sdl/vulkan.");
  return v("UNKNOWN", "Unrecognized dx hint.", "");
}

EngineGuess guessEngine(const std::vector<std::string>& signals,
                        const std::vector<std::string>& importDlls) {
  auto hasDll = [&](const char* sub) {
    for (const auto& d : importDlls)
      if (d.find(sub) != std::string::npos) return true;
    return false;
  };
  auto hasSig = [&](const char* sub) {
    for (const auto& s : signals)
      if (s.find(sub) != std::string::npos) return true;
    return false;
  };
  int unity = (hasSig("unity:") ? 2 : 0) + (hasDll("unityplayer") ? 3 : 0) +
              (hasDll("mono") ? 1 : 0);
  int unreal = (hasSig("unreal") ? 2 : 0) + (hasDll("unreal") ? 3 : 0);
  int cry = (hasSig("cryengine") ? 2 : 0) + (hasDll("cry") ? 2 : 0);
  int xna = (hasDll("xna") ? 3 : 0) + (hasSig("xaudio") || hasSig("d3d") ? 1 : 0);
  if (unity >= 3) return {"unity?", "medium", "unityplayer/managed/_data signals"};
  if (unreal >= 3) return {"unreal-engine?", "medium", "unreal/core signals"};
  if (cry >= 3) return {"cryengine?", "medium", "cry signals"};
  if (xna >= 3) return {"xna-framework?", "low", "xna/ms-stack signals"};
  if (hasSig("havok") || hasSig("physx")) return {"custom-or-licensed (havok/physx mw)", "low", "middleware signals"};
  if (!signals.empty()) return {"custom?", "low", "weak filename signals only"};
  return {"unknown", "low", "no signals"};
}

}  // namespace pc2c::portability
