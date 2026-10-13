#include "pc2c/input/Mapping.h"

namespace pc2c::input {

void InputMapping::bindButton(const std::string& action, std::uint16_t mask) {
  unbind(action);
  bindings_.push_back({action, "button", mask, AxisId::kLX, 1.0f, false});
}
void InputMapping::bindAxis(const std::string& action, AxisId axis, float scale, bool invert) {
  unbind(action);
  bindings_.push_back({action, "axis", 0, axis, scale, invert});
}
void InputMapping::bindTrigger(const std::string& action, AxisId ltOrRt, float scale) {
  unbind(action);
  AxisId a = (ltOrRt == AxisId::kRT) ? AxisId::kRT : AxisId::kLT;
  bindings_.push_back({action, "trigger", 0, a, scale, false});
}
bool InputMapping::unbind(const std::string& action) {
  for (auto it = bindings_.begin(); it != bindings_.end(); ++it) {
    if (it->action == action) {
      bindings_.erase(it);
      return true;
    }
  }
  return false;
}

std::vector<MappingIssue> InputMapping::validate() const {
  std::vector<MappingIssue> out;
  for (std::size_t i = 0; i < bindings_.size(); ++i) {
    if (bindings_[i].action.empty())
      out.push_back({"conflict", "binding #" + std::to_string(i) + " has empty action name"});
    for (std::size_t j = i + 1; j < bindings_.size(); ++j) {
      const auto& a = bindings_[i];
      const auto& b = bindings_[j];
      if (a.kind == "button" && b.kind == "button" && a.buttonMask != 0) {
        if (a.buttonMask == b.buttonMask)
          out.push_back({"conflict", "actions '" + a.action + "' and '" + b.action +
                                           "' share button mask"});
        else if (a.buttonMask & b.buttonMask)
          out.push_back({"warning", "actions '" + a.action + "' and '" + b.action +
                                        "' have overlapping button masks"});
      }
      if ((a.kind == "axis" || a.kind == "trigger") && a.kind == b.kind && a.axis == b.axis)
        out.push_back({"conflict", "actions '" + a.action + "' and '" + b.action + "' share " +
                                       axisToString(a.axis)});
    }
  }
  return out;
}

std::vector<std::string> InputMapping::missing(const std::vector<std::string>& required) const {
  std::vector<std::string> out;
  for (const auto& r : required) {
    bool found = false;
    for (const auto& b : bindings_)
      if (b.action == r) {
        found = true;
        break;
      }
    if (!found) out.push_back(r);
  }
  return out;
}

json::Value InputMapping::toJson() const {
  json::Value::Array arr;
  for (const auto& b : bindings_) {
    json::Value o = json::Value::Obj();
    o.set("action", json::Value::Str(b.action));
    o.set("kind", json::Value::Str(b.kind));
    if (b.kind == "button")
      o.set("buttons", json::Value::Int(b.buttonMask));
    else {
      o.set("axis", json::Value::Str(axisToString(b.axis)));
      o.set("scale", json::Value::Num(b.scale));
      o.set("invert", json::Value::Bool(b.invert));
    }
    arr.push_back(std::move(o));
  }
  return json::Value::Arr(std::move(arr));
}

bool InputMapping::fromJson(const json::Value& v, std::string& err) {
  if (!v.isArr()) {
    err = "bindings must be an array";
    return false;
  }
  std::vector<ActionBinding> tmp;
  for (const auto& e : std::get<json::Value::Array>(v.v)) {
    if (!e.isObj()) {
      err = "binding entry must be an object";
      return false;
    }
    const json::Value* a = e.find("action");
    const json::Value* k = e.find("kind");
    if (!a || !a->isStr() || std::get<std::string>(a->v).empty() || !k || !k->isStr()) {
      err = "binding needs non-empty action + kind";
      return false;
    }
    ActionBinding b;
    b.action = std::get<std::string>(a->v);
    b.kind = std::get<std::string>(k->v);
    if (b.kind == "button") {
      const json::Value* m = e.find("buttons");
      if (!m || !m->isInt()) {
        err = "button binding needs integer buttons mask";
        return false;
      }
      b.buttonMask = static_cast<std::uint16_t>(std::get<std::int64_t>(m->v));
    } else if (b.kind == "axis" || b.kind == "trigger") {
      const json::Value* ax = e.find("axis");
      if (!ax || !ax->isStr() || !axisFromString(std::get<std::string>(ax->v), b.axis)) {
        err = "axis/trigger binding needs valid axis (LX/LY/RX/RY/LT/RT)";
        return false;
      }
      const json::Value* s = e.find("scale");
      if (s && s->isInt())
        b.scale = static_cast<float>(std::get<std::int64_t>(s->v));
      else if (s && s->isNum())  // non-int number => double
        b.scale = static_cast<float>(std::get<double>(s->v));
      const json::Value* iv = e.find("invert");
      if (iv && iv->isBool()) b.invert = std::get<bool>(iv->v);
    } else {
      err = "unknown binding kind: " + b.kind;
      return false;
    }
    tmp.push_back(b);
  }
  bindings_ = std::move(tmp);
  return true;
}

}  // namespace pc2c::input
