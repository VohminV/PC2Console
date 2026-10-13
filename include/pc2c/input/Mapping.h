#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/core/Json.h"
#include "pc2c/input/Types.h"

namespace pc2c::input {

// One game action bound to exactly one source (button mask OR axis OR trigger).
struct ActionBinding {
  std::string action;  // e.g. "move", "camera", "jump", "fire"
  std::string kind;    // "button" | "axis" | "trigger"
  std::uint16_t buttonMask = 0;  // kind==button
  AxisId axis = AxisId::kLX;     // kind==axis|trigger
  float scale = 1.0f;
  bool invert = false;
};

struct MappingIssue {
  std::string severity;  // "conflict" | "warning"
  std::string message;
};

// Ordered action->binding map with validation.
class InputMapping {
 public:
  void clear() { bindings_.clear(); }
  void bindButton(const std::string& action, std::uint16_t mask);
  void bindAxis(const std::string& action, AxisId axis, float scale = 1.0f, bool invert = false);
  void bindTrigger(const std::string& action, AxisId ltOrRt, float scale = 1.0f);
  bool unbind(const std::string& action);
  const std::vector<ActionBinding>& bindings() const { return bindings_; }

  // Conflicts: same button mask on 2+ actions; same axis on 2+ actions with
  // compatible scales (likely accidental duplicate). Warnings: overlapping
  // (non-equal) masks.
  std::vector<MappingIssue> validate() const;
  // Every name in `required` must have a binding.
  std::vector<std::string> missing(const std::vector<std::string>& required) const;

  json::Value toJson() const;
  bool fromJson(const json::Value& v, std::string& err);

 private:
  std::vector<ActionBinding> bindings_;
};

}  // namespace pc2c::input
