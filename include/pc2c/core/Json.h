#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace pc2c::json {

// Minimal JSON value: ordered objects, exact int64, UTF-8 strings.
// Writer + reader for our own machine-readable artifacts (manifests, plans).
struct Value {
  using Array = std::vector<Value>;
  using Object = std::vector<std::pair<std::string, Value>>;
  std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object> v =
      nullptr;

  static Value Null() { return Value{}; }
  static Value Bool(bool b) {
    Value x;
    x.v = b;
    return x;
  }
  static Value Int(std::int64_t i) {
    Value x;
    x.v = i;
    return x;
  }
  static Value Int(std::uint64_t u) { return Int(static_cast<std::int64_t>(u)); }
  static Value Int(std::uint32_t u) { return Int(static_cast<std::int64_t>(u)); }
  static Value Int(int i) { return Int(static_cast<std::int64_t>(i)); }
  static Value Num(double d) {
    Value x;
    x.v = d;
    return x;
  }
  static Value Str(std::string s) {
    Value x;
    x.v = std::move(s);
    return x;
  }
  static Value Arr(Array a = {}) {
    Value x;
    x.v = std::move(a);
    return x;
  }
  static Value Obj(Object o = {}) {
    Value x;
    x.v = std::move(o);
    return x;
  }

  bool isNull() const { return std::holds_alternative<std::nullptr_t>(v); }
  bool isBool() const { return std::holds_alternative<bool>(v); }
  bool isInt() const { return std::holds_alternative<std::int64_t>(v); }
  bool isNum() const {
    return std::holds_alternative<double>(v) || std::holds_alternative<std::int64_t>(v);
  }
  bool isStr() const { return std::holds_alternative<std::string>(v); }
  bool isArr() const { return std::holds_alternative<Array>(v); }
  bool isObj() const { return std::holds_alternative<Object>(v); }

  void set(const std::string& key, Value val);
  const Value* find(const std::string& key) const;
  std::string dump(int indent = 2) const;

  static std::optional<Value> parse(const std::string& text, std::string* err = nullptr);
};

}  // namespace pc2c::json
