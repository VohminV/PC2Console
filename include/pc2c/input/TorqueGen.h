#pragma once
#include <string>
#include <vector>

#include "pc2c/core/Json.h"
#include "pc2c/core/Status.h"

namespace pc2c::input {

// TorqueScript moveMap -> gamepad ActionMap converter.
//
// Preserves the game's own functions VERBATIM (game logic untouched) and only
// swaps device+object. Same-shape transforms (button->button, axis->axis) are
// emitted as active code; shape-changing ones (key->axis) as commented drafts.
// Torque joystick device/object names are an explicitly flagged UNVERIFIED
// assumption (kJoyTable v1) — the JSON records it; mechanism is NOT claimed
// proven until the engine joystick path is confirmed.
struct TsBind {
  std::string device;
  std::string key;
  std::string make;   // bindCmd make-code, or bind function
  std::string brk;    // bindCmd break-code (empty for bind)
  bool isCmd = false;
  int line = 0;
};

// Parses moveMap.bind / moveMap.bindCmd lines (bounded, no execution).
Status parseTsConfig(const std::string& text, std::vector<TsBind>& out, std::string& err);

struct TsRule {
  std::string fnSubstr;  // matched against make/brk/fn code
  std::string control;   // A|B|X|Y|LB|RB|Back|Start|LThumb|RThumb|DUp|DDown|DLeft|DRight|
                         // LX|LY|RX|RY|LT|RT
};
bool parseTsRule(const std::string& text, TsRule& out, std::string& err);  // "fn=CONTROL"

struct TsGenResult {
  std::string csText;
  json::Value report = json::Value::Obj();
  int active = 0, drafts = 0;
  std::vector<std::string> warnings;
};

TsGenResult generateGamepadCs(const std::string& srcPath, const std::vector<TsBind>& binds,
                              const std::vector<TsRule>& rules);

}  // namespace pc2c::input
