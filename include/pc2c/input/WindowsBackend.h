#pragma once
#include <memory>
#include <string>

#include "pc2c/input/Backend.h"

namespace pc2c::input {

// Host (Windows) diagnostic backend. Loads the OS XInput DLL dynamically
// (xinput1_4 -> xinput1_3 -> xinput9_1_0); no hard link, backend reports which
// DLL was loaded and whether rumble entry points exist. Independent
// implementation of IInputBackend — shares no code with the Xbox 360 backend.
std::unique_ptr<IInputBackend> createWindowsBackend(std::string& note);

}  // namespace pc2c::input
