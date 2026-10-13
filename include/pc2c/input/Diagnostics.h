#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/core/Json.h"
#include "pc2c/input/Manager.h"

namespace pc2c::input {

struct DiagOptions {
  int userIndex = 0;
  double seconds = 5.0;
  bool rumbleTest = false;  // sends timed pulses if backend supports rumble
  bool ascii = true;
};

struct DiagReport {
  std::string backend;
  BackendCaps caps;
  std::string testedOn;  // "pc-desktop" | "xbox360" — never ambiguous
  bool everConnected = false;
  int disconnects = 0;
  double pollHz = 0.0;
  std::size_t frames = 0;
  float minLX = 0, maxLX = 0, minLY = 0, maxLY = 0;
  float minRX = 0, maxRX = 0, minRY = 0, maxRY = 0;
  std::uint8_t maxLT = 0, maxRT = 0;
  std::string rawNote;  // raw int16 extremes over the session
  std::vector<std::string> buttonsSeen;
  std::string rumbleResult;  // "ok" | "unsupported" | "not-run" | "failed"
  std::string asciiPlot;     // deadzone visualization (LX/LY scatter + dz circle)
  json::Value toJson() const;
};

// Desktop/field diagnostic: polls a live backend, measures rate, records
// ranges, renders deadzone scatter. Works with zero controller attached —
// then it honestly reports everConnected=false (no fake data).
DiagReport runDiagnostics(ControllerManager& mgr, const DiagOptions& opts);

}  // namespace pc2c::input
