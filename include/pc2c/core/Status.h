#pragma once
#include <string>

namespace pc2c {

// Lightweight status, no exceptions across module boundaries.
struct Status {
  bool ok = true;
  std::string message;
  int code = 0;  // 0=ok, 1=error, 2=unsupported (honest "not implemented")

  static Status Ok() { return Status{}; }
  static Status Fail(std::string m, int c = 1) {
    Status s;
    s.ok = false;
    s.message = std::move(m);
    s.code = c;
    return s;
  }
  static Status Unsupported(std::string m) { return Fail(std::move(m), 2); }
};

}  // namespace pc2c
