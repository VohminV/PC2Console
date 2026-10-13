// pc2c -- Windows-games to Xbox 360 / PS3 porting toolkit.
#include <string>
#include <vector>

#include "pc2c/cli/App.h"

int main(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  return Pc2cMain(args);
}
