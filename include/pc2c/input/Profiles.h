#pragma once
#include <string>
#include <vector>

#include "pc2c/core/Config.h"
#include "pc2c/core/Json.h"
#include "pc2c/core/Status.h"
#include "pc2c/input/Axis.h"
#include "pc2c/input/Mapping.h"

namespace pc2c::input {

inline constexpr int kInputProfileVersion = 1;

struct CameraConfig {
  float sensX = 1.0f;
  float sensY = 1.0f;
  bool invertY = false;
};

struct RumbleConfig {
  bool enabled = true;
  float strength = 1.0f;  // 0..1 master gain
};

// Versioned gamepad profile (JSONPc2c `input_profile_version: 1`).
struct InputProfile {
  int version = kInputProfileVersion;
  std::string name;
  std::string platform;  // "xbox360" | "pc" | ...
  std::string source;    // who authored it: "pc2c-default" | path | ...
  InputMapping mapping;
  AxisConfig leftStick;
  AxisConfig rightStick;
  CameraConfig camera;
  RumbleConfig rumble;
  std::vector<std::string> requiredActions;  // completeness checklist

  static InputProfile makeDefault(const std::string& platform);
  json::Value toJson() const;
  // Returns false + err on any schema/version problem (never half-loaded).
  bool fromJson(const json::Value& v, std::string& err);
  Status saveFile(const std::string& path) const;
  Status loadFile(const std::string& path, std::string& err);
  // Completeness (missing required actions) + mapping conflicts.
  std::vector<MappingIssue> validate() const;
};

}  // namespace pc2c::input
