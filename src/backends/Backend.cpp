#include "pc2c/backends/Backend.h"

namespace pc2c::backends {

json::Value Diagnosis::toJson() const {
  json::Value o = json::Value::Obj();
  o.set("backend", json::Value::Str(backend));
  o.set("ready", json::Value::Bool(ready));
  json::Value::Array cs;
  for (const auto& c : capabilities) {
    json::Value e = json::Value::Obj();
    e.set("name", json::Value::Str(c.name));
    e.set("supported", json::Value::Bool(c.supported));
    e.set("note", json::Value::Str(c.note));
    cs.push_back(std::move(e));
  }
  o.set("capabilities", json::Value::Arr(std::move(cs)));
  json::Value::Array bs;
  for (const auto& b : blockers) bs.push_back(json::Value::Str(b));
  o.set("blockers", json::Value::Arr(std::move(bs)));
  json::Value::Array ns;
  for (const auto& n : notes) ns.push_back(json::Value::Str(n));
  o.set("notes", json::Value::Arr(std::move(ns)));
  return o;
}

}  // namespace pc2c::backends
