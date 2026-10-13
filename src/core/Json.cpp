#include "pc2c/core/Json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace pc2c::json {

void Value::set(const std::string& key, Value val) {
  if (!isObj()) v = Object{};
  auto& o = std::get<Object>(v);
  for (auto& kv : o) {
    if (kv.first == key) {
      kv.second = std::move(val);
      return;
    }
  }
  o.emplace_back(key, std::move(val));
}

const Value* Value::find(const std::string& key) const {
  if (!isObj()) return nullptr;
  for (const auto& kv : std::get<Object>(v)) {
    if (kv.first == key) return &kv.second;
  }
  return nullptr;
}

namespace {

void appendEscaped(std::string& out, const std::string& s) {
  out += '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char b[8];
          std::snprintf(b, sizeof(b), "\\u%04x", c);
          out += b;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  out += '"';
}

void dumpInto(const Value& val, std::string& out, int indent, int depth) {
  const auto& v = val.v;
  if (std::holds_alternative<std::nullptr_t>(v)) {
    out += "null";
  } else if (std::holds_alternative<bool>(v)) {
    out += std::get<bool>(v) ? "true" : "false";
  } else if (std::holds_alternative<std::int64_t>(v)) {
    out += std::to_string(std::get<std::int64_t>(v));
  } else if (std::holds_alternative<double>(v)) {
    double d = std::get<double>(v);
    if (std::isfinite(d)) {
      std::ostringstream os;
      os.precision(17);
      os << d;
      out += os.str();
    } else {
      out += "null";
    }
  } else if (std::holds_alternative<std::string>(v)) {
    appendEscaped(out, std::get<std::string>(v));
  } else if (std::holds_alternative<Value::Array>(v)) {
    const auto& a = std::get<Value::Array>(v);
    if (a.empty()) {
      out += "[]";
      return;
    }
    out += "[\n";
    for (std::size_t i = 0; i < a.size(); ++i) {
      out.append(static_cast<std::size_t>((depth + 1) * indent), ' ');
      dumpInto(a[i], out, indent, depth + 1);
      if (i + 1 < a.size()) out += ",";
      out += "\n";
    }
    out.append(static_cast<std::size_t>(depth * indent), ' ');
    out += "]";
  } else {
    const auto& o = std::get<Value::Object>(v);
    if (o.empty()) {
      out += "{}";
      return;
    }
    out += "{\n";
    for (std::size_t i = 0; i < o.size(); ++i) {
      out.append(static_cast<std::size_t>((depth + 1) * indent), ' ');
      appendEscaped(out, o[i].first);
      out += ": ";
      dumpInto(o[i].second, out, indent, depth + 1);
      if (i + 1 < o.size()) out += ",";
      out += "\n";
    }
    out.append(static_cast<std::size_t>(depth * indent), ' ');
    out += "}";
  }
}

struct Parser {
  const char* p;
  const char* end;
  std::string* err;

  void skipWs() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
  }
  bool fail(const std::string& m) {
    if (err) *err = m;
    return false;
  }
  bool parseValue(Value& out) {
    skipWs();
    if (p >= end) return fail("unexpected end of JSON");
    if (*p == '{') return parseObj(out);
    if (*p == '[') return parseArr(out);
    if (*p == '"') {
      std::string s;
      if (!parseStr(s)) return false;
      out = Value::Str(std::move(s));
      return true;
    }
    if (*p == 't' || *p == 'f' || *p == 'n') return parseLit(out);
    return parseNum(out);
  }
  bool parseObj(Value& out) {
    ++p;
    Value::Object o;
    skipWs();
    if (p < end && *p == '}') {
      ++p;
      out = Value::Obj(std::move(o));
      return true;
    }
    while (true) {
      skipWs();
      if (p >= end || *p != '"') return fail("expected string key");
      std::string k;
      if (!parseStr(k)) return false;
      skipWs();
      if (p >= end || *p != ':') return fail("expected ':'");
      ++p;
      Value vv;
      if (!parseValue(vv)) return false;
      o.emplace_back(std::move(k), std::move(vv));
      skipWs();
      if (p >= end) return fail("unterminated object");
      if (*p == ',') {
        ++p;
        continue;
      }
      if (*p == '}') {
        ++p;
        out = Value::Obj(std::move(o));
        return true;
      }
      return fail("expected ',' or '}'");
    }
  }
  bool parseArr(Value& out) {
    ++p;
    Value::Array a;
    skipWs();
    if (p < end && *p == ']') {
      ++p;
      out = Value::Arr(std::move(a));
      return true;
    }
    while (true) {
      Value vv;
      if (!parseValue(vv)) return false;
      a.push_back(std::move(vv));
      skipWs();
      if (p >= end) return fail("unterminated array");
      if (*p == ',') {
        ++p;
        continue;
      }
      if (*p == ']') {
        ++p;
        out = Value::Arr(std::move(a));
        return true;
      }
      return fail("expected ',' or ']'");
    }
  }
  bool parseStr(std::string& out) {
    ++p;  // opening quote
    std::string s;
    while (p < end && *p != '"') {
      if (*p == '\\') {
        ++p;
        if (p >= end) return fail("bad escape");
        switch (*p) {
          case '"':
            s += '"';
            break;
          case '\\':
            s += '\\';
            break;
          case '/':
            s += '/';
            break;
          case 'n':
            s += '\n';
            break;
          case 'r':
            s += '\r';
            break;
          case 't':
            s += '\t';
            break;
          case 'b':
            s += '\b';
            break;
          case 'f':
            s += '\f';
            break;
          case 'u': {
            if (p + 4 >= end) return fail("bad \\u escape");
            // Minimal \\u handling: BMP -> UTF-8.
            unsigned code = 0;
            for (int i = 1; i <= 4; ++i) {
              char h = p[i];
              code <<= 4;
              if (h >= '0' && h <= '9')
                code |= static_cast<unsigned>(h - '0');
              else if (h >= 'a' && h <= 'f')
                code |= static_cast<unsigned>(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F')
                code |= static_cast<unsigned>(h - 'A' + 10);
              else
                return fail("bad \\u escape");
            }
            p += 4;
            if (code < 0x80)
              s += static_cast<char>(code);
            else if (code < 0x800) {
              s += static_cast<char>(0xC0 | (code >> 6));
              s += static_cast<char>(0x80 | (code & 0x3F));
            } else {
              s += static_cast<char>(0xE0 | (code >> 12));
              s += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
              s += static_cast<char>(0x80 | (code & 0x3F));
            }
            break;
          }
          default:
            return fail("bad escape");
        }
        ++p;
      } else {
        s += *p;
        ++p;
      }
    }
    if (p >= end) return fail("unterminated string");
    ++p;
    out = std::move(s);
    return true;
  }
  bool parseLit(Value& out) {
    if (end - p >= 4 && std::string(p, p + 4) == "true") {
      p += 4;
      out = Value::Bool(true);
      return true;
    }
    if (end - p >= 5 && std::string(p, p + 5) == "false") {
      p += 5;
      out = Value::Bool(false);
      return true;
    }
    if (end - p >= 4 && std::string(p, p + 4) == "null") {
      p += 4;
      out = Value::Null();
      return true;
    }
    return fail("invalid literal");
  }
  bool parseNum(Value& out) {
    const char* s = p;
    if (p < end && (*p == '-' || *p == '+')) ++p;
    bool isFloat = false;
    while (p < end && (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.' || *p == 'e' ||
                       *p == 'E' || *p == '+' || *p == '-')) {
      if (*p == '.' || *p == 'e' || *p == 'E') isFloat = true;
      ++p;
    }
    if (p == s) return fail("invalid number");
    std::string tok(s, p);
    try {
      if (isFloat) {
        out = Value::Num(std::stod(tok));
      } else {
        out = Value::Int(static_cast<std::int64_t>(std::stoll(tok)));
      }
    } catch (...) {
      return fail("invalid number");
    }
    return true;
  }
};

}  // namespace

std::string Value::dump(int indent) const {
  std::string out;
  if (indent <= 0) indent = 2;
  dumpInto(*this, out, indent, 0);
  out += "\n";
  return out;
}

std::optional<Value> Value::parse(const std::string& text, std::string* err) {
  Parser ps{text.data(), text.data() + text.size(), err};
  Value v;
  if (!ps.parseValue(v)) return std::nullopt;
  ps.skipWs();
  if (ps.p != ps.end) {
    if (err) *err = "trailing characters after JSON value";
    return std::nullopt;
  }
  return v;
}

}  // namespace pc2c::json
