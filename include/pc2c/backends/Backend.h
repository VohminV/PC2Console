#pragma once
#include <string>
#include <vector>

#include "pc2c/core/Json.h"
#include "pc2c/core/Status.h"

namespace pc2c::backends {

// Capability contract every console backend must implement. The core never
// depends on a console SDK; backends report honest readiness.
struct Capability {
  std::string name;
  bool supported = false;
  std::string note;
};

struct Diagnosis {
  std::string backend;
  bool ready = false;
  std::vector<Capability> capabilities;
  std::vector<std::string> blockers;
  std::vector<std::string> notes;
  json::Value toJson() const;
};

class Backend {
 public:
  virtual ~Backend() = default;
  virtual std::string name() const = 0;
  virtual std::string targetId() const = 0;  // "xbox360" | "ps3"
  virtual Diagnosis diagnose() const = 0;
  // Writes a build plan for projectDir (never fakes artifacts).
  virtual Status planBuild(const std::string& projectDir, json::Value& out) const = 0;
};

}  // namespace pc2c::backends
