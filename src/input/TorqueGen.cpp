#include "pc2c/input/TorqueGen.h"

#include <cctype>

#include "pc2c/core/Config.h"

namespace pc2c::input {
namespace {

// Assumed Torque joystick table, UNVERIFIED vs TGEA docs (flagged everywhere).
// Buttons are indexed 0..15 in pc2c gamepad order; sticks map to named axes.
const char* kButtonObj(int idx) {
  static const char* kObjs[] = {"button0",  "button1",  "button2",  "button3",
                                "button4",  "button5",  "button6",  "button7",
                                "button8",  "button9",  "button10", "button11",
                                "button12", "button13", "button14", "button15"};
  if (idx < 0 || idx > 15) return nullptr;
  return kObjs[idx];
}
bool controlToButton(const std::string& c, int& idx) {
  const char* names[] = {"A",  "B",     "X",     "Y",    "LB",   "RB",   "Back", "Start",
                         "LThumb", "RThumb", "DUp", "DDown", "DLeft", "DRight", "LT",   "RT"};
  for (int i = 0; i < 16; ++i)
    if (c == names[i]) {
      idx = i;
      return true;
    }
  return false;
}
bool controlToAxis(const std::string& c, std::string& obj) {
  if (c == "LX") {
    obj = "xaxis";
    return true;
  }
  if (c == "LY") {
    obj = "yaxis";
    return true;
  }
  if (c == "RX") {
    obj = "rxaxis";
    return true;
  }
  if (c == "RY") {
    obj = "ryaxis";
    return true;
  }
  return false;
}

// Parses: name ( "arg", "arg", ... ) with balanced quotes/parens. Returns args.
bool parseCall(const std::string& line, std::size_t pos, std::vector<std::string>& args) {
  std::size_t p = line.find('(', pos);
  if (p == std::string::npos) return false;
  ++p;
  std::string cur;
  bool inStr = false;
  int depth = 1;
  while (p < line.size() && depth > 0) {
    char ch = line[p];
    if (inStr) {
      if (ch == '"') {
        inStr = false;
        args.push_back(cur);
        cur.clear();
      } else {
        cur += ch;
      }
    } else if (ch == '"') {
      inStr = true;
    } else if (ch == '(') {
      depth++;
    } else if (ch == ')') {
      depth--;
      if (depth == 0 && !cur.empty()) {
        // trailing unquoted arg (e.g. function name)
        std::string t;
        for (char c : cur)
          if (!std::isspace((unsigned char)c)) t += c;
        if (!t.empty()) args.push_back(t);
      }
    } else if (ch == ',' && depth == 1) {
      // separator: flush a pending unquoted token (e.g. device names).
      std::string t;
      for (char c : cur)
        if (!std::isspace((unsigned char)c)) t += c;
      if (!t.empty()) args.push_back(t);
      cur.clear();
    } else if (depth >= 1 && !std::isspace((unsigned char)ch)) {
      cur += ch;
    }
    ++p;
  }
  return depth == 0 && !inStr;
}

}  // namespace

Status parseTsConfig(const std::string& text, std::vector<TsBind>& out, std::string& err) {
  if (text.size() > 2 * 1024 * 1024) {
    err = "config too large";
    return Status::Fail(err);
  }
  out.clear();
  std::size_t lineNo = 1;
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    std::string line = text.substr(pos, eol - pos);
    // strip // comments (TorqueScript has no block comments inside binds here;
    // quoted URLs with // are not expected in bind lines — noted limitation)
    std::string code = line;
    bool inS = false;
    for (std::size_t i = 0; i + 1 < code.size(); ++i) {
      if (code[i] == '"') inS = !inS;
      if (!inS && code[i] == '/' && code[i + 1] == '/') {
        code = code.substr(0, i);
        break;
      }
    }
    std::size_t mp = code.find("moveMap.bindCmd");
    bool isCmd = mp != std::string::npos;
    if (!isCmd) mp = code.find("moveMap.bind");
    if (mp != std::string::npos) {
      std::vector<std::string> args;
      if (!parseCall(code, mp, args) || args.size() < 3) {
        err = "unparseable bind line " + std::to_string(lineNo);
        return Status::Fail(err);
      }
      TsBind b;
      b.device = args[0];
      b.key = args[1];
      b.isCmd = isCmd;
      b.line = (int)lineNo;
      if (isCmd) {
        if (args.size() < 4) {
          err = "bindCmd needs 4 args at line " + std::to_string(lineNo);
          return Status::Fail(err);
        }
        b.make = args[2];
        b.brk = args[3];
      } else {
        b.make = args[2];
      }
      out.push_back(b);
      if (out.size() > 2000) {
        err = "too many binds";
        return Status::Fail(err);
      }
    }
    pos = eol + 1;
    lineNo++;
  }
  return Status::Ok();
}

bool parseTsRule(const std::string& text, TsRule& out, std::string& err) {
  std::size_t eq = text.find('=');
  std::size_t col = text.find(':');
  if (col != std::string::npos && (eq == std::string::npos || col < eq)) eq = col;
  if (eq == std::string::npos || eq == 0 || eq + 1 >= text.size()) {
    err = "rule must be fn=CONTROL (or fn:CONTROL): " + text;
    return false;
  }
  out.fnSubstr = text.substr(0, eq);
  out.control = text.substr(eq + 1);
  int bi = -1;
  std::string ax;
  if (!controlToButton(out.control, bi) && !controlToAxis(out.control, ax)) {
    err = "unknown CONTROL: " + out.control;
    return false;
  }
  return true;
}

TsGenResult generateGamepadCs(const std::string& srcPath, const std::vector<TsBind>& binds,
                              const std::vector<TsRule>& rules) {
  TsGenResult r;
  std::string cs;
  cs += "// gamepad.cs — GENERATED by pc2c input torquescript from " + srcPath + "\n";
  cs += "// Game functions preserved VERBATIM; only device+object swapped.\n";
  cs += "// ASSUMPTION (UNVERIFIED vs TGEA docs, kJoyTable v1): joystick device with\n";
  cs += "//   buttons button0..15 (A=0,B=1,X=2,Y=3,LB=4,RB=5,Back=6,Start=7,\n";
  cs += "//   LThumb=8,RThumb=9,DUp=10,DDown=11,DLeft=12,DRight=13,LT=14,RT=15)\n";
  cs += "//   and axes xaxis/yaxis/rxaxis/ryaxis.\n";
  cs += "// Mechanism NOT proven: confirm engine joystick path + build before use.\n";
  cs += "if (isObject(gamepadMap)) gamepadMap.delete();\n";
  cs += "new ActionMap(gamepadMap);\n";

  json::Value::Array jbinds;
  json::Value::Array jwarn;
  for (const auto& b : binds) {
    const TsRule* hit = nullptr;
    for (const auto& rl : rules) {
      if (b.make.find(rl.fnSubstr) != std::string::npos ||
          (!b.brk.empty() && b.brk.find(rl.fnSubstr) != std::string::npos)) {
        hit = &rl;
        break;
      }
    }
    json::Value e = json::Value::Obj();
    e.set("old", json::Value::Str(b.device + " " + b.key + " -> " + b.make +
                                  (b.brk.empty() ? "" : " / " + b.brk)));
    e.set("line", json::Value::Int(b.line));
    if (!hit) {
      std::string w = "no --bind rule matched line " + std::to_string(b.line) + " (" + b.make +
                      "); kept as comment";
      r.warnings.push_back(w);
      jwarn.push_back(json::Value::Str(w));
      cs += "// UNMAPPED(line " + std::to_string(b.line) + "): " + b.device + " \"" + b.key +
            "\" -> " + b.make + "\n";
      e.set("new", json::Value::Str("(unmapped)"));
      e.set("active", json::Value::Bool(false));
      jbinds.push_back(std::move(e));
      continue;
    }
    int bi = -1;
    std::string ax;
    bool isBtn = controlToButton(hit->control, bi);
    bool isAxis = !isBtn && controlToAxis(hit->control, ax);
    bool oldIsAxis = (b.key == "xaxis" || b.key == "yaxis");
    std::string dev = "joystick";
    if (isBtn) {
      // button->button or key->button: same bindCmd/bind shape, verbatim code.
      std::string obj = kButtonObj(bi);
      std::string nl;
      if (b.isCmd)
        nl = "gamepadMap.bindCmd(" + dev + ", \"" + obj + "\", \"" + b.make + "\", \"" + b.brk +
             "\");";
      else
        nl = "gamepadMap.bind(" + dev + ", \"" + obj + "\", " + b.make + ");";
      nl += "  // pc2c: device/object UNVERIFIED; from line " + std::to_string(b.line);
      cs += nl + "\n";
      e.set("new", json::Value::Str(dev + " " + obj));
      e.set("active", json::Value::Bool(true));
      r.active++;
    } else if (isAxis && oldIsAxis) {
      std::string nl = "gamepadMap.bind(" + dev + ", \"" + ax + "\", " + b.make +
                       ");  // pc2c: device/object UNVERIFIED; from line " +
                       std::to_string(b.line);
      cs += nl + "\n";
      e.set("new", json::Value::Str(dev + " " + ax));
      e.set("active", json::Value::Bool(true));
      r.active++;
    } else {
      // key->axis: value semantics differ -> commented draft, never active code.
      std::string w = "line " + std::to_string(b.line) + ": key->axis needs value-semantics "
                      "review; emitted as commented draft";
      r.warnings.push_back(w);
      jwarn.push_back(json::Value::Str(w));
      cs += "// DRAFT(line " + std::to_string(b.line) + "): key \"" + b.key + "\" -> " + dev +
            " \"" + ax + "\" calls " + b.make + " (REVIEW value semantics)\n";
      e.set("new", json::Value::Str("(draft) " + dev + " " + ax));
      e.set("active", json::Value::Bool(false));
      r.drafts++;
    }
    e.set("assumption", json::Value::Str("kJoyTable-v1-UNVERIFIED"));
    jbinds.push_back(std::move(e));
  }
  r.csText = cs;
  json::Value o = json::Value::Obj();
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("source", json::Value::Str(srcPath));
  o.set("active", json::Value::Int(r.active));
  o.set("drafts", json::Value::Int(r.drafts));
  o.set("bindings", json::Value::Arr(std::move(jbinds)));
  o.set("warnings", json::Value::Arr(std::move(jwarn)));
  o.set("assumptions",
        json::Value::Str("kJoyTable-v1: joystick button0..15/axis names UNVERIFIED vs TGEA docs"));
  o.set("mechanism_verdict",
        json::Value::Str("UNPROVEN: gamepad support needs confirmed engine joystick path "
                         "(e.g. $pref::Input::JoystickEnabled + console build); this file is a "
                         "review draft, not a proven patch"));
  r.report = std::move(o);
  return r;
}

}  // namespace pc2c::input
