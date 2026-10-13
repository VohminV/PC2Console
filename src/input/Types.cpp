#include "pc2c/input/Types.h"

#include "pc2c/input/StateJson.h"

namespace pc2c::input {

std::string axisToString(AxisId a) {
  switch (a) {
    case AxisId::kLX:
      return "LX";
    case AxisId::kLY:
      return "LY";
    case AxisId::kRX:
      return "RX";
    case AxisId::kRY:
      return "RY";
    case AxisId::kLT:
      return "LT";
    case AxisId::kRT:
      return "RT";
  }
  return "?";
}

bool axisFromString(const std::string& s, AxisId& out) {
  if (s == "LX") {
    out = AxisId::kLX;
    return true;
  }
  if (s == "LY") {
    out = AxisId::kLY;
    return true;
  }
  if (s == "RX") {
    out = AxisId::kRX;
    return true;
  }
  if (s == "RY") {
    out = AxisId::kRY;
    return true;
  }
  if (s == "LT") {
    out = AxisId::kLT;
    return true;
  }
  if (s == "RT") {
    out = AxisId::kRT;
    return true;
  }
  return false;
}

std::string buttonName(std::uint16_t bit) {
  switch (bit) {
    case kBtnDpadUp:
      return "DpadUp";
    case kBtnDpadDown:
      return "DpadDown";
    case kBtnDpadLeft:
      return "DpadLeft";
    case kBtnDpadRight:
      return "DpadRight";
    case kBtnStart:
      return "Start";
    case kBtnBack:
      return "Back";
    case kBtnLThumb:
      return "LThumb";
    case kBtnRThumb:
      return "RThumb";
    case kBtnLB:
      return "LB";
    case kBtnRB:
      return "RB";
    case kBtnA:
      return "A";
    case kBtnB:
      return "B";
    case kBtnX:
      return "X";
    case kBtnY:
      return "Y";
  }
  return "?";
}

std::vector<std::string> pressedNames(std::uint16_t buttons) {
  std::vector<std::string> out;
  for (int i = 0; i < 16; ++i) {
    std::uint16_t bit = static_cast<std::uint16_t>(1u << i);
    if (buttons & bit) {
      std::string n = buttonName(bit);
      if (n != "?") out.push_back(n);
    }
  }
  return out;
}

json::Value stateToJson(const ControllerState& s) {
  json::Value o = json::Value::Obj();
  o.set("connected", json::Value::Bool(s.connected));
  o.set("packet", json::Value::Int(s.packet));
  o.set("buttons", json::Value::Int(s.buttons));
  json::Value::Array bn;
  for (const auto& n : pressedNames(s.buttons)) bn.push_back(json::Value::Str(n));
  o.set("pressed", json::Value::Arr(std::move(bn)));
  o.set("lx", json::Value::Int(s.left.x));
  o.set("ly", json::Value::Int(s.left.y));
  o.set("rx", json::Value::Int(s.right.x));
  o.set("ry", json::Value::Int(s.right.y));
  o.set("lt", json::Value::Int(s.lt));
  o.set("rt", json::Value::Int(s.rt));
  o.set("note", json::Value::Str(s.note));
  return o;
}

}  // namespace pc2c::input
