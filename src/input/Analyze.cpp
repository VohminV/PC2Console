#include "pc2c/input/Analyze.h"

#include <filesystem>
#include <fstream>

#include "pc2c/core/Fs.h"

namespace pc2c::input {

json::Value InputAnalysis::toJson() const {
  json::Value o = json::Value::Obj();
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("dir", json::Value::Str(dir));
  o.set("has_sources", json::Value::Bool(hasSources));
  json::Value::Array fa;
  for (const auto& f : findings) {
    json::Value e = json::Value::Obj();
    e.set("system", json::Value::Str(f.system));
    e.set("evidence", json::Value::Str(f.evidence));
    e.set("confidence", json::Value::Str(f.confidence));
    fa.push_back(std::move(e));
  }
  o.set("findings", json::Value::Arr(std::move(fa)));
  json::Value::Array ma;
  for (const auto& m : bindingMap) {
    json::Value e = json::Value::Obj();
    e.set("site", json::Value::Str(m.site));
    e.set("old_mechanism", json::Value::Str(m.oldMechanism));
    e.set("suggestion", json::Value::Str(m.suggestion));
    ma.push_back(std::move(e));
  }
  o.set("binding_map_draft", json::Value::Arr(std::move(ma)));
  o.set("mechanism_verdict", json::Value::Str(mechanismVerdict));
  o.set("no_autoinject_note",
        json::Value::Str("pc2c never injects code into a foreign process; gamepad support "
                         "requires a proven integration mechanism (source patch point)"));
  return o;
}

namespace {

struct Token {
  const char* text;
  const char* system;
  const char* suggestion;
};

const Token kTokens[] = {
    {"XInputGetState", "xinput", "map to XInputGetState polling (already gamepad-shaped)"},
    {"XInputGetKeystroke", "xinput", "map keystrokes to gamepad button events"},
    {"DirectInput8Create", "directinput", "replace enumeration with XInput slots 0..3"},
    {"IDirectInputDevice", "directinput", "replace device reads with ControllerManager frames"},
    {"GUID_SysKeyboard", "directinput", "keyboard device: redesign around gamepad actions"},
    {"GUID_SysMouse", "directinput", "mouse device: NO console equivalent, redesign required"},
    {"GetAsyncKeyState", "keyboard", "per-frame key polling: map keys to gamepad actions"},
    {"GetKeyState", "keyboard", "key state: map to gamepad actions"},
    {"WM_KEYDOWN", "keyboard", "message-pump keyboard: needs action-queue rewrite"},
    {"RegisterRawInputDevices", "rawinput", "raw input: map HID usages to gamepad"},
    {"GetRawInputData", "rawinput", "raw input: map HID usages to gamepad"},
    {"GetCursorPos", "mouse", "absolute mouse: NO console equivalent, redesign required"},
    {"SetCursorPos", "mouse", "mouse warping: NO console equivalent, redesign required"},
    {"WM_MOUSEMOVE", "mouse", "mouse messages: NO console equivalent, redesign required"},
    {"SDL_GameController", "sdl", "SDL gamecontroller layer: map to ControllerManager"},
    {"SDL_Joystick", "sdl", "SDL joystick layer: map to ControllerManager"},
    {"SDL_PollEvent", "sdl", "SDL event pump: bridge to action queue"},
    {"glfwGetKey", "glfw", "GLFW keys: map to gamepad actions"},
    {"UnityEngine.Input", "unity", "Unity input: needs console engine build + Input System map"},
    {"InputSystem", "unity", "Unity Input System: rebind via generated C# map"},
    {"BindAction", "unreal", "Unreal input bindings: remap in DefaultInput.ini + code"},
    {"SetupPlayerInputComponent", "unreal", "Unreal input component: remap actions"},
    {"EKeys::", "unreal", "Unreal keys: remap to gamepad keys"},
};

bool isSourceExt(const std::string& ext) {
  return ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".cc" ||
         ext == ".cs" || ext == ".py" || ext == ".lua" || ext == ".js" || ext == ".ini" ||
         ext == ".cfg" || ext == ".xml" || ext == ".json";
}

}  // namespace

InputAnalysis analyzeInput(const std::string& dir, const discovery::ScanResult& scan,
                           const std::vector<std::pair<std::string, analysis::PeInfo>>& pes,
                           const Limits& limits) {
  InputAnalysis a;
  a.dir = dir;
  const std::size_t kMaxHits = 300;
  const std::size_t kMaxFile = 2 * 1024 * 1024;

  std::filesystem::path root(dir);
  for (const auto& f : scan.files) {
    if (a.findings.size() >= kMaxHits) break;
    std::string ext = fs::extensionLower(std::filesystem::path(f.rel));
    if (!isSourceExt(ext)) continue;
    if (f.size > kMaxFile) continue;
    a.hasSources = true;
    std::vector<std::uint8_t> raw;
    if (!fs::readFileBounded(root / f.rel, raw, kMaxFile).ok) continue;
    // Binary guard: skip files with NUL bytes in the head.
    bool binary = false;
    for (std::size_t i = 0; i < raw.size() && i < 4096; ++i)
      if (raw[i] == 0) {
        binary = true;
        break;
      }
    if (binary) continue;
    std::string text(raw.begin(), raw.end());
    std::size_t line = 1;
    std::size_t pos = 0;
    // Token search with line numbers (simple, bounded).
    std::vector<std::size_t> lineStarts{0};
    for (std::size_t i = 0; i < text.size(); ++i)
      if (text[i] == '\n') lineStarts.push_back(i + 1);
    auto lineOf = [&](std::size_t p) {
      std::size_t lo = 0, hi = lineStarts.size();
      while (lo + 1 < hi) {
        std::size_t mid = (lo + hi) / 2;
        if (lineStarts[mid] <= p)
          lo = mid;
        else
          hi = mid;
      }
      return lo + 1;
    };
    for (const auto& t : kTokens) {
      if (a.findings.size() >= kMaxHits) break;
      std::size_t p = 0;
      int hits = 0;
      while ((p = text.find(t.text, p)) != std::string::npos && hits < 5) {
        a.findings.push_back(
            {t.system, f.rel + ":" + std::to_string(lineOf(p)), "confirmed"});
        a.bindingMap.push_back(
            {f.rel + ":" + std::to_string(lineOf(p)), t.system, t.suggestion});
        p += 1;
        hits++;
      }
      (void)line;
      (void)pos;
    }
  }

  // Binary imports: func-level input evidence (presence != proof of call).
  for (const auto& pr : pes) {
    if (!pr.second.valid) continue;
    for (const auto& imp : pr.second.imports) {
      if (a.findings.size() >= kMaxHits) break;
      std::string sys;
      if (imp.find("xinput") != std::string::npos)
        sys = "xinput";
      else if (imp.find("directinput") != std::string::npos || imp.find("dinput") != std::string::npos)
        sys = "directinput";
      else if (imp.find("getasynckeystate") != std::string::npos ||
               imp.find("getkeystate") != std::string::npos)
        sys = "keyboard";
      else if (imp.find("getcursorpos") != std::string::npos ||
               imp.find("setcursorpos") != std::string::npos)
        sys = "mouse";
      else if (imp.find("registerrawinputdevices") != std::string::npos ||
               imp.find("getrawinputdata") != std::string::npos)
        sys = "rawinput";
      else
        continue;
      a.findings.push_back({sys, "import:" + imp + " (in " + pr.first + ")", "probable"});
    }
  }

  bool hasKb = false, hasMouse = false, hasDI = false, hasXI = false, hasOther = false;
  for (const auto& f : a.findings) {
    if (f.system == "keyboard") hasKb = true;
    if (f.system == "mouse") hasMouse = true;
    if (f.system == "directinput") hasDI = true;
    if (f.system == "xinput") hasXI = true;
    if (f.system == "sdl" || f.system == "unity" || f.system == "unreal") hasOther = true;
  }
  if (!a.hasSources && a.findings.empty()) {
    a.mechanismVerdict =
        "UNKNOWN: no sources and no input imports found; gamepad support cannot be promised "
        "without a proven integration mechanism.";
  } else if (!a.hasSources) {
    a.mechanismVerdict =
        "REQUIRES_SOURCE: binary-only input evidence (" +
        std::string(hasXI ? "xinput " : "") + std::string(hasDI ? "directinput " : "") +
        std::string(hasKb ? "keyboard " : "") + std::string(hasMouse ? "mouse" : "") +
        "); import presence does not prove a call path — source patch points required.";
  } else if (hasMouse) {
    a.mechanismVerdict =
        "REQUIRES_REDESIGN: mouse input has no Xbox 360 equivalent; keyboard/directinput parts "
        "are mappable once action sites are confirmed in source.";
  } else if (hasXI || hasDI || hasKb || hasOther) {
    a.mechanismVerdict =
        "MAPPABLE: input layer identified in source; integrate ControllerManager at the "
        "confirmed action sites (binding map draft above), preserving handlers.";
  } else {
    a.mechanismVerdict =
        "UNKNOWN: sources present but no known input API matched; manual review required.";
  }
  return a;
}

}  // namespace pc2c::input
