#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pc2c/core/Config.h"
#include "pc2c/core/Json.h"

namespace pc2c::model {

// Versioned, console-independent intermediate representation.
// Facts carry reproducible sources; assumptions are labeled as such.
struct Source {
  std::string file;  // relative path or artifact name
  std::int64_t offset = -1;
  std::string detail;  // e.g. "import-table", "header:0x3C", "manifest-entry"
};

struct Fact {
  std::string kind;  // "binary" | "resource" | "conversion" | "sdk" | "constraint"
  Source source;
  std::string detail;
  std::string confidence;  // "confirmed" | "probable" | "assumed"
};

struct Limitation {
  std::string severity;  // mirrors plan categories
  std::string reason;
  std::string remediation;
};

struct ModelDoc {
  int version = kModelVersion;
  std::string gameDir;
  std::string target;
  std::vector<Fact> facts;
  std::vector<Fact> assumptions;
  std::vector<Limitation> limitations;
  json::Value extra = json::Value::Obj();  // embedded plan/inventory summaries

  void addFact(std::string kind, Source src, std::string detail, std::string conf = "confirmed");
  void addAssumption(std::string kind, Source src, std::string detail);
  void addLimitation(std::string sev, std::string reason, std::string remediation);
  json::Value toJson() const;
};

}  // namespace pc2c::model
