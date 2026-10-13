#include "pc2c/core/Logger.h"

#include <cstdio>

namespace pc2c {

Logger& Logger::Global() {
  static Logger instance;
  return instance;
}

void Logger::log(Level lv, const std::string& msg) {
  if (lv == kDebug && !verbose_) return;
  const char* tag = "INFO";
  FILE* out = stdout;
  if (lv == kError) {
    tag = "ERROR";
    out = stderr;
  } else if (lv == kWarn) {
    tag = "WARN";
    out = stderr;
  } else if (lv == kDebug) {
    tag = "DEBUG";
  }
  std::fprintf(out, "[pc2c][%s] %s\n", tag, msg.c_str());
  std::fflush(out);
}

void Logger::error(const std::string& msg) { log(kError, msg); }
void Logger::warn(const std::string& msg) { log(kWarn, msg); }
void Logger::info(const std::string& msg) { log(kInfo, msg); }
void Logger::debug(const std::string& msg) { log(kDebug, msg); }

}  // namespace pc2c
