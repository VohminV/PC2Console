#include "pc2c/input/Profiles.h"

#include <fstream>

#include "pc2c/core/Fs.h"

namespace pc2c::input {

namespace {
// JSON numbers may parse as int64 (e.g. "1") or double ("1.0"); accept both.
bool getFloat(const json::Value& v, float& out) {
  if (v.isInt()) {
    out = static_cast<float>(std::get<std::int64_t>(v.v));
    return true;
  }
  if (v.isStr() || v.isBool() || v.isNull() || v.isArr() || v.isObj()) return false;
  out = static_cast<float>(std::get<double>(v.v));
  return true;
}
}  // namespace

InputProfile InputProfile::makeDefault(const std::string& platform) {
  InputProfile p;
  p.name = "pc2c-default-gamepad";
  p.platform = platform;
  p.source = "pc2c-default";
  p.mapping.bindAxis("move", AxisId::kLX);
  p.mapping.bindAxis("move_y", AxisId::kLY);
  p.mapping.bindAxis("camera", AxisId::kRX);
  p.mapping.bindAxis("camera_y", AxisId::kRY);
  p.mapping.bindTrigger("fire", AxisId::kRT);
  p.mapping.bindTrigger("aim", AxisId::kLT);
  p.mapping.bindButton("jump", kBtnA);
  p.mapping.bindButton("action", kBtnX);
  p.mapping.bindButton("reload", kBtnB);
  p.mapping.bindButton("menu", kBtnY);
  p.mapping.bindButton("sprint", kBtnLThumb);
  p.mapping.bindButton("crouch", kBtnRThumb);
  p.mapping.bindButton("prev_weapon", kBtnLB);
  p.mapping.bindButton("next_weapon", kBtnRB);
  p.mapping.bindButton("dpad_up", kBtnDpadUp);
  p.mapping.bindButton("dpad_down", kBtnDpadDown);
  p.mapping.bindButton("dpad_left", kBtnDpadLeft);
  p.mapping.bindButton("dpad_right", kBtnDpadRight);
  p.mapping.bindButton("start", kBtnStart);
  p.mapping.bindButton("back", kBtnBack);
  p.leftStick = AxisConfig{};
  p.rightStick = AxisConfig{};
  p.rightStick.deadzone = 8689.0f / 32767.0f;
  p.requiredActions = {"move", "camera", "fire", "jump", "start"};
  return p;
}

json::Value InputProfile::toJson() const {
  json::Value o = json::Value::Obj();
  o.set("input_profile_version", json::Value::Int(version));
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("name", json::Value::Str(name));
  o.set("platform", json::Value::Str(platform));
  o.set("source", json::Value::Str(source));
  o.set("mapping", mapping.toJson());
  auto axisJson = [](const AxisConfig& a) {
    json::Value e = json::Value::Obj();
    e.set("deadzone", json::Value::Num(a.deadzone));
    e.set("mode", json::Value::Str(a.mode == DeadzoneMode::kRadial ? "radial" : "axial"));
    e.set("sensitivity", json::Value::Num(a.sensitivity));
    e.set("invert_x", json::Value::Bool(a.invertX));
    e.set("invert_y", json::Value::Bool(a.invertY));
    e.set("curve_gamma", json::Value::Num(a.curveGamma));
    e.set("smooth_window", json::Value::Int(a.smoothWindow));
    e.set("trigger_threshold", json::Value::Num(a.triggerThreshold));
    return e;
  };
  o.set("left_stick", axisJson(leftStick));
  o.set("right_stick", axisJson(rightStick));
  json::Value cam = json::Value::Obj();
  cam.set("sens_x", json::Value::Num(camera.sensX));
  cam.set("sens_y", json::Value::Num(camera.sensY));
  cam.set("invert_y", json::Value::Bool(camera.invertY));
  o.set("camera", std::move(cam));
  json::Value rum = json::Value::Obj();
  rum.set("enabled", json::Value::Bool(rumble.enabled));
  rum.set("strength", json::Value::Num(rumble.strength));
  o.set("rumble", std::move(rum));
  json::Value::Array req;
  for (const auto& r : requiredActions) req.push_back(json::Value::Str(r));
  o.set("required_actions", json::Value::Arr(std::move(req)));
  return o;
}

bool InputProfile::fromJson(const json::Value& v, std::string& err) {
  if (!v.isObj()) {
    err = "profile must be an object";
    return false;
  }
  const json::Value* ver = v.find("input_profile_version");
  if (!ver || !ver->isInt() || std::get<std::int64_t>(ver->v) != kInputProfileVersion) {
    err = "unsupported input_profile_version (expected 1)";
    return false;
  }
  InputProfile tmp;
  auto getStr = [&](const char* key, std::string& out) {
    const json::Value* e = v.find(key);
    if (e && e->isStr()) out = std::get<std::string>(e->v);
  };
  getStr("name", tmp.name);
  getStr("platform", tmp.platform);
  getStr("source", tmp.source);
  const json::Value* m = v.find("mapping");
  if (!m || !tmp.mapping.fromJson(*m, err)) {
    if (err.empty()) err = "profile needs mapping[]";
    return false;
  }
  auto getAxis = [&](const char* key, AxisConfig& a) {
    const json::Value* e = v.find(key);
    if (!e || !e->isObj()) return;
    const json::Value* x = nullptr;
    float f = 0.0f;
    if ((x = e->find("deadzone")) && getFloat(*x, f)) a.deadzone = f;
    if ((x = e->find("mode")) && x->isStr())
      a.mode = (std::get<std::string>(x->v) == "axial" ? DeadzoneMode::kAxial
                                                      : DeadzoneMode::kRadial);
    if ((x = e->find("sensitivity")) && getFloat(*x, f)) a.sensitivity = f;
    if ((x = e->find("invert_x")) && x->isBool()) a.invertX = std::get<bool>(x->v);
    if ((x = e->find("invert_y")) && x->isBool()) a.invertY = std::get<bool>(x->v);
    if ((x = e->find("curve_gamma")) && getFloat(*x, f)) a.curveGamma = f;
    if ((x = e->find("smooth_window")) && x->isInt())
      a.smoothWindow = (int)std::get<std::int64_t>(x->v);
    if ((x = e->find("trigger_threshold")) && getFloat(*x, f)) a.triggerThreshold = f;
  };
  getAxis("left_stick", tmp.leftStick);
  getAxis("right_stick", tmp.rightStick);
  if (const json::Value* c = v.find("camera"); c && c->isObj()) {
    const json::Value* x = nullptr;
    float f = 0.0f;
    if ((x = c->find("sens_x")) && getFloat(*x, f)) tmp.camera.sensX = f;
    if ((x = c->find("sens_y")) && getFloat(*x, f)) tmp.camera.sensY = f;
    if ((x = c->find("invert_y")) && x->isBool()) tmp.camera.invertY = std::get<bool>(x->v);
  }
  if (const json::Value* r = v.find("rumble"); r && r->isObj()) {
    const json::Value* x = nullptr;
    float f = 0.0f;
    if ((x = r->find("enabled")) && x->isBool()) tmp.rumble.enabled = std::get<bool>(x->v);
    if ((x = r->find("strength")) && getFloat(*x, f)) tmp.rumble.strength = f;
  }
  if (const json::Value* q = v.find("required_actions"); q && q->isArr()) {
    for (const auto& e : std::get<json::Value::Array>(q->v))
      if (e.isStr()) tmp.requiredActions.push_back(std::get<std::string>(e.v));
  }
  *this = std::move(tmp);
  return true;
}

Status InputProfile::saveFile(const std::string& path) const {
  return fs::atomicWriteFile(path, toJson().dump(2));
}

Status InputProfile::loadFile(const std::string& path, std::string& err) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    err = "cannot open profile: " + path;
    return Status::Fail(err);
  }
  std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (text.size() > 4 * 1024 * 1024) {
    err = "profile too large";
    return Status::Fail(err);
  }
  auto val = json::Value::parse(text, &err);
  if (!val) return Status::Fail("profile JSON error: " + err);
  if (!fromJson(*val, err)) return Status::Fail("profile schema error: " + err);
  return Status::Ok();
}

std::vector<MappingIssue> InputProfile::validate() const {
  std::vector<MappingIssue> out = mapping.validate();
  for (const auto& m : mapping.missing(requiredActions))
    out.push_back({"conflict", "required action unmapped: " + m});
  if (rumble.strength < 0.0f || rumble.strength > 1.0f)
    out.push_back({"warning", "rumble.strength outside 0..1"});
  if (leftStick.curveGamma <= 0.0f || rightStick.curveGamma <= 0.0f)
    out.push_back({"warning", "curve_gamma must be > 0"});
  return out;
}

}  // namespace pc2c::input
