#pragma once
#include "pc2c/backends/Backend.h"

namespace pc2c::backends {

class Xbox360Backend : public Backend {
 public:
  std::string name() const override { return "Xbox 360 (Xenon/Xenos)"; }
  std::string targetId() const override { return "xbox360"; }
  Diagnosis diagnose() const override;
  Status planBuild(const std::string& projectDir, json::Value& out) const override;
};

class Ps3Backend : public Backend {
 public:
  std::string name() const override { return "PlayStation 3 (Cell/RSX) [future]"; }
  std::string targetId() const override { return "ps3"; }
  Diagnosis diagnose() const override;
  Status planBuild(const std::string& projectDir, json::Value& out) const override;
};

}  // namespace pc2c::backends
