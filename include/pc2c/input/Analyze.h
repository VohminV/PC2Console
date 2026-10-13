#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/analysis/Pe.h"
#include "pc2c/core/Config.h"
#include "pc2c/core/Json.h"
#include "pc2c/discovery/Scanner.h"

namespace pc2c::input {

// How the game reads input, as EVIDENCED (never guessed).
struct InputSystemFinding {
  std::string system;  // "keyboard" | "mouse" | "directinput" | "xinput" |
                       // "rawinput" | "sdl" | "unity" | "unreal" | "custom" | "unknown"
  std::string evidence;  // file:line or "import:dll!func"
  std::string confidence;  // "confirmed" | "probable"
};

struct InputAnalysis {
  std::string dir;
  bool hasSources = false;
  std::vector<InputSystemFinding> findings;
  // Draft binding map: old handling site -> suggested gamepad binding.
  // Actions are template slots (unmapped until a human confirms the game's
  // real actions); suggestions never claim to preserve unknown game logic.
  struct MapRow {
    std::string site;
    std::string oldMechanism;
    std::string suggestion;
  };
  std::vector<MapRow> bindingMap;
  std::string mechanismVerdict;  // proven mechanism or honest blocker
  json::Value toJson() const;
};

// Bounded static scan: source files (*.cpp/h/cs/py/lua...) + PE imports +
// config files. Never executes, never modifies the game.
InputAnalysis analyzeInput(const std::string& dir, const discovery::ScanResult& scan,
                           const std::vector<std::pair<std::string, analysis::PeInfo>>& pes,
                           const Limits& limits);

}  // namespace pc2c::input
