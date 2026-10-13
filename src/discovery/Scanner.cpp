#include "pc2c/discovery/Scanner.h"

#include <algorithm>

#include "pc2c/core/Logger.h"

namespace pc2c::discovery {

std::string kindToString(FileKind k) {
  switch (k) {
    case FileKind::kExecutable:
      return "executable";
    case FileKind::kLibrary:
      return "library";
    case FileKind::kArchive:
      return "archive";
    case FileKind::kImage:
      return "image";
    case FileKind::kAudio:
      return "audio";
    case FileKind::kModel:
      return "model";
    case FileKind::kShader:
      return "shader";
    case FileKind::kConfig:
      return "config";
    case FileKind::kOther:
      return "other";
  }
  return "other";
}

namespace {

FileKind kindByExt(const std::string& ext, bool hasMz) {
  if (hasMz) {
    if (ext == ".exe" || ext == ".com" || ext == ".scr") return FileKind::kExecutable;
    if (ext == ".dll" || ext == ".drv" || ext == ".ocx" || ext == ".cpl")
      return FileKind::kLibrary;
    return FileKind::kExecutable;  // MZ binary of unknown role: treat as executable-like
  }
  if (ext == ".exe") return FileKind::kExecutable;
  if (ext == ".dll" || ext == ".drv" || ext == ".ocx") return FileKind::kLibrary;
  if (ext == ".zip" || ext == ".pk3" || ext == ".pk4" || ext == ".cab" || ext == ".rar" ||
      ext == ".7z" || ext == ".pak" || ext == ".vpk" || ext == ".upk" || ext == ".u" ||
      ext == ".bsa" || ext == ".wad" || ext == ".dat" || ext == ".bin" || ext == ".arc" ||
      ext == ". Unity3d" || ext == ".assets" || ext == ".bundle" || ext == ".obb")
    return FileKind::kArchive;
  if (ext == ".bmp" || ext == ".tga" || ext == ".dds" || ext == ".png" || ext == ".jpg" ||
      ext == ".jpeg" || ext == ".tif" || ext == ".tiff" || ext == ".gif" || ext == ".webp" ||
      ext == ".ktx" || ext == ".pvr" || ext == ".xbt" || ext == ".tex" || ext == ".gtex")
    return FileKind::kImage;
  if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".wma" ||
      ext == ".xma" || ext == ".fsb" || ext == ".bnk" || ext == ".pck" || ext == ".acb" ||
      ext == ".awb" || ext == ".xa" || ext == ".adx" || ext == ".aax" || ext == ".opus")
    return FileKind::kAudio;
  if (ext == ".fbx" || ext == ".obj" || ext == ".dae" || ext == ".gltf" || ext == ".glb" ||
      ext == ".x" || ext == ".mesh" || ext == ".model" || ext == ".md5mesh" || ext == ".smd" ||
      ext == ".iqm" || ext == ".blend" || ext == ".3ds")
    return FileKind::kModel;
  if (ext == ".fx" || ext == ".hlsl" || ext == ".hlsli" || ext == ".vsh" || ext == ".psh" ||
      ext == ".cso" || ext == ".fxo" || ext == ".vso" || ext == ".pso" || ext == ".cg" ||
      ext == ".glsl" || ext == ".shader" || ext == ".fxc" || ext == ".shd")
    return FileKind::kShader;
  if (ext == ".ini" || ext == ".cfg" || ext == ".config" || ext == ".xml" || ext == ".json" ||
      ext == ".yaml" || ext == ".yml" || ext == ".toml" || ext == ".lua" || ext == ".txt" ||
      ext == ".log" || ext == ".md" || ext == ".nfo")
    return FileKind::kConfig;
  return FileKind::kOther;
}

std::string magicOf(const std::vector<std::uint8_t>& head) {
  auto has = [&](const char* s, std::size_t n) {
    if (head.size() < n) return false;
    for (std::size_t i = 0; i < n; ++i)
      if (head[i] != static_cast<std::uint8_t>(s[i])) return false;
    return true;
  };
  if (has("MZ", 2)) return "MZ";
  if (has("\x89PNG", 4)) return "PNG";
  if (has("\xFF\xD8\xFF", 3)) return "JPEG";
  if (has("DDS ", 4)) return "DDS";
  if (has("BM", 2)) return "BMP";
  if (has("GIF8", 4)) return "GIF";
  if (has("PK\x03\x04", 4) || has("PK\x05\x06", 4) || has("PK\x07\x08", 4)) return "ZIP";
  if (has("Rar!", 4)) return "RAR";
  if (has("7z\xBC\xAF\x27\x1C", 6)) return "7Z";
  if (has("MSCF", 4)) return "CAB";
  if (has("OggS", 4)) return "OGG";
  if (has("fLaC", 4)) return "FLAC";
  if (has("ID3", 3)) return "ID3";
  if (has("RIFF", 4)) {
    if (head.size() >= 12 && has("WAVE", 4) == false) {
      // check "WAVE" at offset 8
      if (head[8] == 'W' && head[9] == 'A' && head[10] == 'V' && head[11] == 'E')
        return "WAV";
      if (head[8] == 'W' && head[9] == 'E' && head[10] == 'B' && head[11] == 'P')
        return "WEBP";
      return "RIFF";
    }
    return "RIFF";
  }
  if (has("FSB", 3)) return "FSB";
  if (has("II*\x00", 4) || has("MM\x00*", 4)) return "TIFF";
  if (has("\x00\x00\x01\x00", 4)) return "ICO";
  if (has("XEX2", 4) || has("XEX1", 4) || has("XEX?", 4)) return "XEX";
  if (has("\x7F"
          "ELF",
          4))
    return "ELF";
  return "";
}

void maybeEngineSignal(const std::string& relLower, std::vector<std::string>& out) {
  auto has = [&](const char* s) { return relLower.find(s) != std::string::npos; };
  if (has("unityplayer.dll") || has("/managed/") || has("_data/")) out.push_back("unity:" + relLower);
  if (has("unreal") || has(".upk") || has("core.dll") || has("engine.dll"))
    out.push_back("unreal-candidate:" + relLower);
  if (has("cry") && (has(".dll") || has(".pak"))) out.push_back("cryengine-candidate:" + relLower);
  if (has("havok") || has("hk_")) out.push_back("havok:" + relLower);
  if (has("physx") || has("nxcooking")) out.push_back("physx:" + relLower);
  if (has("bink") || has(".bik")) out.push_back("bink:" + relLower);
  if (has("fmod") || has("wwise") || has("miles") || has("mss32"))
    out.push_back("audio-mw:" + relLower);
  if (has("xna") || has("xaudio") || has("d3d")) out.push_back("ms-stack:" + relLower);
  if (has("sdl") || has("openal") || has("ogg") || has("vorbis"))
    out.push_back("open-stack:" + relLower);
}

}  // namespace

ScanResult scanGameDir(const std::string& root, const Limits& limits) {
  ScanResult r;
  r.root = root;
  auto walked = fs::walk(root, limits.maxFiles, limits.maxTotalBytes, &r.errors);
  r.truncated = walked.truncated;
  r.totalBytes = walked.totalBytes;
  r.files.reserve(walked.entries.size());
  for (const auto& e : walked.entries) {
    FileRecord rec;
    rec.rel = e.rel;
    rec.size = e.size;
    std::vector<std::uint8_t> head;
    // Magic head read is best-effort; failure keeps ext-based kind with a note.
    std::string headNote;
    if (fs::readFileHead(e.full, head, 64).ok) {
      rec.magic = magicOf(head);
    } else {
      headNote = "unreadable-head;";
    }
    std::string ext = fs::extensionLower(e.full);
    rec.kind = kindByExt(ext, rec.magic == "MZ" || rec.magic == "XEX" || rec.magic == "ELF");
    if (rec.magic == "XEX") {
      rec.note = "xbox360-xex-signature;";
    } else if (rec.magic == "ELF") {
      rec.note = "elf-signature;";
    }
    rec.note += headNote + "ext=" + (ext.empty() ? "(none)" : ext);
    if (!rec.magic.empty()) rec.note += ";magic=" + rec.magic;
    r.files.push_back(std::move(rec));
    std::string relLower = fs::lower(r.files.back().rel);
    maybeEngineSignal(relLower, r.engineSignals);
  }
  std::sort(r.files.begin(), r.files.end(),
            [](const FileRecord& a, const FileRecord& b) { return a.rel < b.rel; });
  if (r.engineSignals.size() > 200) r.engineSignals.resize(200);
  return r;
}

}  // namespace pc2c::discovery
