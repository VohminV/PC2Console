#pragma once
#include <string>

namespace pc2c {

class Logger {
 public:
  enum Level { kError = 0, kWarn = 1, kInfo = 2, kDebug = 3 };

  static Logger& Global();

  void setVerbose(bool v) { verbose_ = v; }
  bool verbose() const { return verbose_; }

  void error(const std::string& msg);
  void warn(const std::string& msg);
  void info(const std::string& msg);
  void debug(const std::string& msg);
  void log(Level lv, const std::string& msg);

 private:
  bool verbose_ = false;
};

}  // namespace pc2c
