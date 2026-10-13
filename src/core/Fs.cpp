#include "pc2c/core/Fs.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace pc2c::fs {
namespace {

bool isSymlink(const std::filesystem::path& p) {
  std::error_code ec;
  return std::filesystem::is_symlink(p, ec);
}

}  // namespace

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string extensionLower(const std::filesystem::path& p) {
  return lower(p.extension().string());
}

std::string genericRel(const std::filesystem::path& root, const std::filesystem::path& full) {
  std::error_code ec;
  auto rel = std::filesystem::relative(full, root, ec);
  if (ec) return full.generic_string();
  return rel.generic_string();
}

WalkResult walk(const std::filesystem::path& root, std::uint64_t maxFiles,
                std::uint64_t maxTotalBytes, std::vector<std::string>* errors) {
  WalkResult r;
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) {
    r.errors.push_back("input path does not exist: " + root.string());
    return r;
  }
  if (isSymlink(root)) {
    r.errors.push_back("input path is a symlink, refusing to follow");
    return r;
  }
  if (std::filesystem::is_regular_file(root, ec)) {
    Entry e;
    e.full = root;
    e.rel = root.filename().generic_string();
    e.size = std::filesystem::file_size(root, ec);
    r.entries.push_back(std::move(e));
    r.totalBytes = r.entries.back().size;
    return r;
  }
  std::filesystem::recursive_directory_iterator it(
      root, std::filesystem::directory_options::skip_permission_denied, ec);
  const std::filesystem::recursive_directory_iterator end;
  if (ec) {
    r.errors.push_back("cannot open input dir: " + ec.message());
    return r;
  }
  for (; it != end; it.increment(ec)) {
    if (ec) {
      r.errors.push_back(std::string("walk error: ") + ec.message());
      ec.clear();
      continue;
    }
    const auto& p = it->path();
    if (isSymlink(p)) {
      it.disable_recursion_pending();
      continue;
    }
    std::error_code ec2;
    bool dir = it->is_directory(ec2);
    if (ec2) continue;
    if (dir) continue;  // record files only
    if (!it->is_regular_file(ec2) || ec2) continue;
    std::uint64_t sz = it->file_size(ec2);
    if (ec2) continue;
    if (r.entries.size() >= maxFiles) {
      r.truncated = true;
      r.errors.push_back("file count limit reached, scan truncated");
      break;
    }
    if (r.totalBytes + sz > maxTotalBytes) {
      r.truncated = true;
      r.errors.push_back("total size limit reached, scan truncated");
      break;
    }
    Entry e;
    e.full = p;
    e.rel = genericRel(root, p);
    e.size = sz;
    r.totalBytes += sz;
    r.entries.push_back(std::move(e));
  }
  if (errors && !r.errors.empty()) *errors = r.errors;
  return r;
}

Status readFileBounded(const std::filesystem::path& path, std::vector<std::uint8_t>& out,
                       std::uint64_t maxBytes) {
  std::error_code ec;
  std::uint64_t sz = std::filesystem::file_size(path, ec);
  if (ec) return Status::Fail("cannot stat file: " + path.string());
  if (sz > maxBytes)
    return Status::Fail("file exceeds size limit (" + std::to_string(sz) + " bytes): " + path.string());
  std::ifstream f(path, std::ios::binary);
  if (!f) return Status::Fail("cannot open file: " + path.string());
  out.resize(static_cast<std::size_t>(sz));
  if (sz > 0 && !f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz)))
    return Status::Fail("short read (possible truncation/race): " + path.string());
  return Status::Ok();
}

Status readFileHead(const std::filesystem::path& path, std::vector<std::uint8_t>& out,
                    std::size_t headBytes) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return Status::Fail("cannot open file: " + path.string());
  out.resize(headBytes);
  f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(headBytes));
  out.resize(static_cast<std::size_t>(f.gcount()));
  return Status::Ok();
}

Status atomicWriteFile(const std::filesystem::path& path, const void* data, std::size_t size) {
  std::error_code ec;
  auto parent = path.parent_path();
  if (!parent.empty()) std::filesystem::create_directories(parent, ec);
  if (ec) return Status::Fail("cannot create output dir: " + ec.message());
  auto tmp = path;
  tmp += ".pc2c-tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return Status::Fail("cannot write temp file: " + tmp.string());
    if (size > 0) {
      f.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
      if (!f) return Status::Fail("failed writing temp file: " + tmp.string());
    }
    f.flush();
    if (!f) return Status::Fail("failed flushing temp file: " + tmp.string());
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return Status::Fail("atomic rename failed: " + ec.message());
  }
  return Status::Ok();
}

Status atomicWriteFile(const std::filesystem::path& path, const std::string& text) {
  return atomicWriteFile(path, text.data(), text.size());
}

std::optional<std::filesystem::path> joinSafe(const std::filesystem::path& root,
                                              const std::string& rel) {
  if (rel.empty()) return std::nullopt;
  std::filesystem::path rp(rel);
  if (rp.is_absolute()) return std::nullopt;
  for (const auto& part : rp) {
    if (part == "..") return std::nullopt;
  }
  auto joined = root / rp;
  auto normRoot = root.lexically_normal();
  auto normJoined = joined.lexically_normal();
  // Ensure the normalized join stays within root.
  auto itR = normRoot.begin();
  auto itJ = normJoined.begin();
  for (; itR != normRoot.end(); ++itR, ++itJ) {
    if (itJ == normJoined.end() || *itR != *itJ) return std::nullopt;
  }
  return joined;
}

Status ensureDir(const std::filesystem::path& dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) return Status::Fail("cannot create dir " + dir.string() + ": " + ec.message());
  return Status::Ok();
}

}  // namespace pc2c::fs
