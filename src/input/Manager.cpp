#include "pc2c/input/Manager.h"

namespace pc2c::input {

json::Value Frame::toJson() const {
  json::Value o = json::Value::Obj();
  o.set("connected", json::Value::Bool(connected));
  o.set("changed", json::Value::Bool(changed));
  o.set("disconnects", json::Value::Int(disconnects));
  json::Value::Array bn;
  for (const auto& n : pressedNames(buttons)) bn.push_back(json::Value::Str(n));
  o.set("pressed_names", json::Value::Arr(std::move(bn)));
  o.set("buttons", json::Value::Int(buttons));
  o.set("lx", json::Value::Num(left.x));
  o.set("ly", json::Value::Num(left.y));
  o.set("rx", json::Value::Num(right.x));
  o.set("ry", json::Value::Num(right.y));
  o.set("lt", json::Value::Num(lt));
  o.set("rt", json::Value::Num(rt));
  json::Value::Array acts;
  for (const auto& kv : actions) {
    json::Value e = json::Value::Obj();
    e.set("action", json::Value::Str(kv.first));
    e.set("value", json::Value::Num(kv.second));
    acts.push_back(std::move(e));
  }
  o.set("actions", json::Value::Arr(std::move(acts)));
  return o;
}

ControllerManager::ControllerManager(std::unique_ptr<IInputBackend> backend, InputProfile profile,
                                     int userIndex)
    : backend_(std::move(backend)), profile_(std::move(profile)), user_(userIndex) {
  setProfile(profile_);
}

void ControllerManager::setProfile(InputProfile p) {
  profile_ = std::move(p);
  leftFilter_.setConfig(profile_.leftStick);
  rightFilter_.setConfig(profile_.rightStick);
  rumble_.setStrength(profile_.rumble.enabled ? profile_.rumble.strength : 0.0f);
}

void ControllerManager::update(std::uint64_t nowMs) {
  ControllerState st;
  PollStatus ps = backend_->poll(user_, st);
  Frame next;
  next.disconnects = frame_.disconnects;
  if (ps != PollStatus::kOk || !st.connected) {
    if (frame_.connected || hadState_) next.disconnects++;  // edge: was up, now down
    hadState_ = false;
    lastPacket_ = 0;
    frame_ = next;  // connected=false, zeroed, no stale edges
    rumble_.stop();
    return;
  }
  next.connected = true;
  next.changed = !hadState_ || st.packet != lastPacket_;
  lastPacket_ = st.packet;
  hadState_ = true;
  std::uint16_t prev = frame_.buttons;
  next.buttons = st.buttons;
  next.pressed = static_cast<std::uint16_t>(st.buttons & ~prev);
  next.released = static_cast<std::uint16_t>(prev & ~st.buttons);
  next.left = leftFilter_.process(st.left.x, st.left.y);
  next.right = rightFilter_.process(st.right.x, st.right.y);
  next.lt = processTrigger(st.lt, profile_.leftStick.triggerThreshold);
  next.rt = processTrigger(st.rt, profile_.leftStick.triggerThreshold);
  next.rawLX = st.left.x;
  next.rawLY = st.left.y;
  next.rawRX = st.right.x;
  next.rawRY = st.right.y;
  next.rawLT = st.lt;
  next.rawRT = st.rt;
  for (const auto& b : profile_.mapping.bindings()) {
    float v = 0.0f;
    if (b.kind == "button") {
      v = (st.buttons & b.buttonMask) ? 1.0f : 0.0f;
    } else if (b.kind == "axis") {
      Vec2 base = (b.axis == AxisId::kRX || b.axis == AxisId::kRY) ? next.right : next.left;
      float c = (b.axis == AxisId::kLX || b.axis == AxisId::kRX) ? base.x : base.y;
      // Mapping-level invert applies before camera config (documented order).
      if (b.invert) c = -c;
      v = c * b.scale;
      if (v > 1) v = 1;
      if (v < -1) v = -1;
    } else if (b.kind == "trigger") {
      v = (b.axis == AxisId::kRT ? next.rt : next.lt) * b.scale;
      if (v > 1) v = 1;
    }
    next.actions[b.action] = v;
  }
  frame_ = next;
  rumble_.update(*backend_, user_, nowMs, profile_.rumble.enabled);
}

}  // namespace pc2c::input
