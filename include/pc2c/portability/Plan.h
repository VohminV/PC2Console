#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/analysis/Inventory.h"
#include "pc2c/analysis/Pe.h"
#include "pc2c/core/Config.h"
#include "pc2c/core/Json.h"
#include "pc2c/discovery/Scanner.h"
#include "pc2c/portability/ApiDb.h"

namespace pc2c::portability {

struct PeFileReport {
  std::string rel;
  analysis::PeInfo pe;
  ApiVerdict cpu;
  std::vector<ApiVerdict> dlls;  // aligned with pe.importDlls
};

struct PlanDoc {
  std::string gameDir;
  std::string target;
  EngineGuess engine;
  std::vector<PeFileReport> binaries;
  ApiVerdict graphics;
  std::string graphicsDetail;
  // resources
  std::uint64_t fileCount = 0, totalBytes = 0, convertibleBytes = 0;
  std::uint64_t imageCount = 0, audioCount = 0, archiveCount = 0, modelCount = 0, shaderCount = 0;
  // sdk
  bool sdkPresent = false;
  std::string sdkVersion;
  std::vector<std::string> sdkMissing;
  // findings
  struct Item {
    std::string category, subject, reason, remediation;
  };
  std::vector<Item> limitations;
  std::vector<std::string> manualActions;
  std::vector<std::string> requiredTests;
  std::string verdict;
};

PlanDoc buildPlan(const std::string& gameDir, const std::string& target,
                  const discovery::ScanResult& scan,
                  const std::vector<std::pair<std::string, analysis::PeInfo>>& pes,
                  const analysis::Inventory& inv, bool sdkPresent, const std::string& sdkVersion,
                  const std::vector<std::string>& sdkMissing);

json::Value planToJson(const PlanDoc& p);

}  // namespace pc2c::portability
