#include "pc2c/formats/Registry.h"

#include "pc2c/core/Config.h"

namespace pc2c::formats {

namespace {
FormatInfo make(std::string id, std::string kind, std::vector<std::string> exts,
                std::string magic, std::string status, Capability caps, std::string note) {
  FormatInfo f;
  f.id = std::move(id);
  f.kind = std::move(kind);
  f.extensions = std::move(exts);
  f.magic = std::move(magic);
  f.status = std::move(status);
  f.caps = caps;
  f.note = std::move(note);
  return f;
}
}  // namespace

std::vector<FormatInfo> builtinFormats() {
  const Capability imgFull{true, true, true, true, false};
  const Capability detectOnly{true, false, false, false, false};
  const Capability textRefs{true, true, true, false, false};
  const Capability pe{true, true, false, false, false};
  const Capability shaderXdk{true, true, false, true, false};
  return {
      make("bmp", "image", {".bmp"}, "BMP", "supported", imgFull,
           "BMP 24/32 decode+convert proven (Converter.cpp); paletted/RLE honest-unsupported"),
      make("tga", "image", {".tga"}, "", "supported", imgFull,
           "TGA type2/10 24/32 decode+convert proven; colormapped/grayscale honest-unsupported"),
      make("png", "image", {".png"}, "PNG", "unsupported", detectOnly,
           "detect+validate only; decoder backlog Phase 3 (133MB in Canada Hunt)"),
      make("jpeg", "image", {".jpg", ".jpeg"}, "JPEG", "unsupported", detectOnly,
           "detect-only; decoder backlog"),
      make("dds", "image", {".dds"}, "DDS", "unsupported", detectOnly,
           "payload needs Xenos tiling research; kept as-is"),
      make("gif", "image", {".gif"}, "GIF", "unsupported", detectOnly, "detect-only"),
      make("ogg", "audio", {".ogg"}, "OGG", "unsupported", detectOnly,
           "audio pipeline not in MVP; kept as-is"),
      make("wav", "audio", {".wav"}, "WAV", "unsupported", detectOnly,
           "detect-only; XAudio2 port needs source"),
      make("hlsl", "shader", {".hlsl", ".hlsli", ".fx", ".h"}, "", "supported", shaderXdk,
           "HLSL->Xbox microcode via real XDK fxc (ShaderConvert.cpp); .h include unsupported"),
      make("pe-exe", "executable", {".exe"}, "MZ", "supported", pe,
           "PE32/PE32+ parse (arch/imports/exports/linker-hint); never executed"),
      make("pe-dll", "library", {".dll"}, "MZ", "supported", pe, "same as pe-exe"),
      make("xex", "executable", {".xex"}, "XEX", "unsupported", detectOnly,
           "signature scan only; XEX parser out of MVP"),
      make("torque-script", "script", {".cs"}, "", "supported", textRefs,
           "TorqueScript TEXT: reference extraction proven (Torque adapter); "
           "compiled .dso NOT decoded"),
      make("torque-gui", "interface", {".gui"}, "", "supported", textRefs,
           "GUI text: reference extraction proven"),
      make("torque-mission", "scene", {".mis"}, "", "supported", textRefs,
           "Mission text: reference extraction proven"),
      make("torque-dts", "model", {".dts"}, "", "unsupported", detectOnly,
           "Torque shape binary: no registered decoder; kept as-is"),
      make("torque-dsq", "animation", {".dsq"}, "", "unsupported", detectOnly,
           "Torque animation binary: no decoder"),
      make("torque-ter", "terrain", {".ter"}, "", "unsupported", detectOnly,
           "Torque terrain binary: no decoder"),
      make("torque-dso", "script-bytecode", {".dso"}, "", "unsupported", detectOnly,
           "Compiled TorqueScript: no decoder; text .cs used instead"),
      make("torque-dif", "interior", {".dif"}, "", "unsupported", detectOnly,
           "Torque interior binary: no decoder"),
  };
}

const FormatInfo* findByExtension(const std::vector<FormatInfo>& reg,
                                  const std::string& extLower) {
  for (const auto& f : reg)
    for (const auto& e : f.extensions)
      if (e == extLower) return &f;
  return nullptr;
}

const FormatInfo* findByMagic(const std::vector<FormatInfo>& reg, const std::string& magic) {
  if (magic.empty()) return nullptr;
  for (const auto& f : reg)
    if (!f.magic.empty() && f.magic == magic) return &f;
  return nullptr;
}

json::Value registryToJson(const std::vector<FormatInfo>& reg) {
  json::Value::Array arr;
  for (const auto& f : reg) {
    json::Value o = json::Value::Obj();
    o.set("id", json::Value::Str(f.id));
    o.set("kind", json::Value::Str(f.kind));
    json::Value::Array ex;
    for (const auto& e : f.extensions) ex.push_back(json::Value::Str(e));
    o.set("extensions", json::Value::Arr(std::move(ex)));
    o.set("magic", json::Value::Str(f.magic));
    o.set("status", json::Value::Str(f.status));
    json::Value c = json::Value::Obj();
    c.set("detect", json::Value::Bool(f.caps.detect));
    c.set("validate", json::Value::Bool(f.caps.validate));
    c.set("extract", json::Value::Bool(f.caps.extract));
    c.set("convert", json::Value::Bool(f.caps.convert));
    c.set("repack", json::Value::Bool(f.caps.repack));
    o.set("caps", std::move(c));
    o.set("note", json::Value::Str(f.note));
    arr.push_back(std::move(o));
  }
  json::Value o = json::Value::Obj();
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("formats", json::Value::Arr(std::move(arr)));
  return o;
}

}  // namespace pc2c::formats
