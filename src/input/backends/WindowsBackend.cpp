// Windows diagnostic XInput backend (host-only TU; includes <windows.h> here
// and nowhere else in pc2c core). Dynamic-loads the OS DLL so the tool runs
// on machines without a controller and reports honest capabilities.
#include "pc2c/input/WindowsBackend.h"

#ifdef _WIN32
#include <windows.h>

#include <cstdint>
#include <cstring>

namespace pc2c::input {
namespace {

// Minimal XInput declarations (layout matches Microsoft docs; verified sizes).
struct XiGamepad {
  std::uint16_t wButtons;
  std::uint8_t bLeftTrigger;
  std::uint8_t bRightTrigger;
  std::int16_t sThumbLX;
  std::int16_t sThumbLY;
  std::int16_t sThumbRX;
  std::int16_t sThumbRY;
};
struct XiState {
  std::uint32_t dwPacketNumber;
  XiGamepad Gamepad;
};
struct XiVibration {
  std::uint16_t wLeftMotorSpeed;
  std::uint16_t wRightMotorSpeed;
};
static_assert(sizeof(XiGamepad) == 12, "XiGamepad layout");
static_assert(sizeof(XiState) == 16, "XiState layout");

using FnGetState = DWORD(WINAPI*)(DWORD, XiState*);
using FnSetState = DWORD(WINAPI*)(DWORD, XiVibration*);

class WindowsBackend : public IInputBackend {
 public:
  WindowsBackend(HMODULE dll, FnGetState gs, FnSetState ss, std::string dllName)
      : dll_(dll), get_(gs), set_(ss), dllName_(std::move(dllName)) {}
  ~WindowsBackend() override {
    if (dll_) FreeLibrary(dll_);
  }
  std::string name() const override { return "windows-xinput"; }
  BackendCaps caps() const override {
    BackendCaps c;
    c.backend = name();
    c.maxControllers = 4;
    c.analogTriggers = true;
    c.hasRumble = (set_ != nullptr);
    c.hasStickClicks = true;
    c.detail = "loaded " + dllName_ +
               (set_ ? "; SetState present" : "; SetState MISSING -> rumble unsupported");
    return c;
  }
  PollStatus poll(int userIndex, ControllerState& out) override {
    out = ControllerState{};
    if (userIndex < 0 || userIndex > 3) {
      out.note = "user index out of range 0..3";
      return PollStatus::kError;
    }
    XiState xs;
    std::memset(&xs, 0, sizeof(xs));
    DWORD res = get_(static_cast<DWORD>(userIndex), &xs);
    if (res == ERROR_SUCCESS) {
      out.connected = true;
      out.packet = xs.dwPacketNumber;
      out.buttons = xs.Gamepad.wButtons;
      out.left.x = xs.Gamepad.sThumbLX;
      out.left.y = xs.Gamepad.sThumbLY;
      out.right.x = xs.Gamepad.sThumbRX;
      out.right.y = xs.Gamepad.sThumbRY;
      out.lt = xs.Gamepad.bLeftTrigger;
      out.rt = xs.Gamepad.bRightTrigger;
      return PollStatus::kOk;
    }
    if (res == ERROR_DEVICE_NOT_CONNECTED) {
      out.note = "disconnected (ERROR_DEVICE_NOT_CONNECTED)";
      return PollStatus::kDisconnected;
    }
    char b[64];
    std::snprintf(b, sizeof(b), "XInputGetState error %lu", (unsigned long)res);
    out.note = b;
    return PollStatus::kError;
  }
  bool setRumble(int userIndex, const RumbleRequest& r) override {
    if (!set_ || userIndex < 0 || userIndex > 3) return false;
    XiVibration v;
    v.wLeftMotorSpeed = static_cast<std::uint16_t>(r.left * 65535.0f);
    v.wRightMotorSpeed = static_cast<std::uint16_t>(r.right * 65535.0f);
    return set_(static_cast<DWORD>(userIndex), &v) == ERROR_SUCCESS;
  }

 private:
  HMODULE dll_ = nullptr;
  FnGetState get_ = nullptr;
  FnSetState set_ = nullptr;
  std::string dllName_;
};

}  // namespace

std::unique_ptr<IInputBackend> createWindowsBackend(std::string& note) {
  static const char* kDlls[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
  for (const char* dll : kDlls) {
    HMODULE h = LoadLibraryA(dll);
    if (!h) continue;
    auto gs = reinterpret_cast<FnGetState>(GetProcAddress(h, "XInputGetState"));
    if (!gs) {
      FreeLibrary(h);
      continue;
    }
    auto ss = reinterpret_cast<FnSetState>(GetProcAddress(h, "XInputSetState"));
    note = std::string("using ") + dll;
    return std::unique_ptr<IInputBackend>(new WindowsBackend(h, gs, ss, dll));
  }
  note = "no XInput DLL found (xinput1_4/1_3/9_1_0)";
  return nullptr;
}

}  // namespace pc2c::input

#else

namespace pc2c::input {
std::unique_ptr<IInputBackend> createWindowsBackend(std::string& note) {
  note = "windows backend is Win32-only";
  return nullptr;
}
}  // namespace pc2c::input

#endif
