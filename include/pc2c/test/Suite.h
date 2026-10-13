#pragma once

namespace pc2c::test {

// Runs the full regression suite on synthetic fixtures (no external files,
// no game binaries, no SDK required). Returns number of failures (0 = pass).
// Prints [PASS]/[FAIL] lines; verbose adds detail.
int RunAllTests(bool verbose);

}  // namespace pc2c::test
