#pragma once
#include <string>
#include <vector>

// Single CLI entry; returns process exit code (0 ok, 1 error, 2 unsupported).
int Pc2cMain(const std::vector<std::string>& args);
