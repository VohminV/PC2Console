#include "pc2c/intermediate/Model.h"

namespace pc2c::model {

void ModelDoc::addFact(std::string kind, Source src, std::string detail, std::string conf) {
  facts.push_back({std::move(kind), std::move(src), std::move(detail), std::move(conf)});
}
void ModelDoc::addAssumption(std::string kind, Source src, std::string detail) {
  assumptions.push_back({std::move(kind), std::move(src), std::move(detail), "assumed"});
}
void ModelDoc::addLimitation(std::string sev, std::string reason, std::string remediation) {
  limitations.push_back({std::move(sev), std::move(reason), std::move(remediation)});
}

json::Value ModelDoc::toJson() const {
  json::Value o = json::Value::Obj();
  o.set("pc2c_model_version", json::Value::Int(version));
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("game_dir", json::Value::Str(gameDir));
  o.set("target", json::Value::Str(target));
  auto factArr = [&](const std::vector<Fact>& fs) {
    json::Value::Array a;
    for (const auto& f : fs) {
      json::Value e = json::Value::Obj();
      e.set("kind", json::Value::Str(f.kind));
      json::Value s = json::Value::Obj();
      s.set("file", json::Value::Str(f.source.file));
      s.set("offset", json::Value::Int(f.source.offset));
      s.set("detail", json::Value::Str(f.source.detail));
      e.set("source", std::move(s));
      e.set("detail", json::Value::Str(f.detail));
      e.set("confidence", json::Value::Str(f.confidence));
      a.push_back(std::move(e));
    }
    return json::Value::Arr(std::move(a));
  };
  o.set("facts", factArr(facts));
  o.set("assumptions", factArr(assumptions));
  json::Value::Array la;
  for (const auto& l : limitations) {
    json::Value e = json::Value::Obj();
    e.set("severity", json::Value::Str(l.severity));
    e.set("reason", json::Value::Str(l.reason));
    e.set("remediation", json::Value::Str(l.remediation));
    la.push_back(std::move(e));
  }
  o.set("limitations", json::Value::Arr(std::move(la)));
  o.set("extra", extra);
  return o;
}

}  // namespace pc2c::model
