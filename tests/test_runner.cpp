// CTest entry point: runs the same suite as `pc2c test`.
#include <iostream>

#include "pc2c/test/Suite.h"

int main(int argc, char** argv) {
  bool verbose = argc > 1 && std::string(argv[1]) == "--verbose";
  int fails = pc2c::test::RunAllTests(verbose);
  if (fails != 0) std::cerr << "SUITE FAILED: " << fails << " failure(s)\n";
  return fails == 0 ? 0 : 1;
}
