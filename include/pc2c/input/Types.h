#pragma once
#include <cstdint>
#include <string>
#include <vector>

// NOTE: this header (plus Backend.h) is shared with the Xbox 360 XDK-toolchain
// translation unit, whose compiler (VS2010-era, C++03+) rejects C++11-only
// constructs. Keep it C++03-clean: plain enums, explicit constructors,
// no in-class initializers, no =default/=delete, no enum class,
// no nested namespace definitions (namespace a::b is C++17).
namespace pc2c {
namespace input {

// Button bits mirror XINPUT_GAMEPAD_* (verified: XDK xinputdefs.h:66-88).
// Shared by all backends so mappings/profiles stay platform-independent.
enum Button {
  kBtnDpadUp = 0x0001,
  kBtnDpadDown = 0x0002,
  kBtnDpadLeft = 0x0004,
  kBtnDpadRight = 0x0008,
  kBtnStart = 0x0010,
  kBtnBack = 0x0020,
  kBtnLThumb = 0x0040,
  kBtnRThumb = 0x0080,
  kBtnLB = 0x0100,
  kBtnRB = 0x0200,
  kBtnA = 0x1000,
  kBtnB = 0x2000,
  kBtnX = 0x4000,
  kBtnY = 0x8000
};

enum AxisId { kLX, kLY, kRX, kRY, kLT, kRT };
std::string axisToString(AxisId a);
bool axisFromString(const std::string& s, AxisId& out);

struct StickRaw {
  std::int16_t x;
  std::int16_t y;
  StickRaw() : x(0), y(0) {}
};

// Raw controller snapshot. `connected=false` means NO data is valid —
// backends must never substitute fake zeros for a missing controller.
struct ControllerState {
  bool connected;
  std::uint32_t packet;  // backend packet counter (change => new data)
  std::uint16_t buttons;
  StickRaw left;
  StickRaw right;
  std::uint8_t lt;
  std::uint8_t rt;
  std::string note;  // backend detail, e.g. "disconnected", "error 1167"
  ControllerState()
      : connected(false), packet(0), buttons(0), left(), right(), lt(0), rt(0), note() {}
};

enum PollStatus { kOk, kDisconnected, kError };

struct BackendCaps {
  std::string backend;  // "windows-xinput" | "xbox360-xinput" | "fake"
  int maxControllers;
  bool analogTriggers;
  bool hasRumble;
  bool hasStickClicks;
  std::string detail;  // e.g. loaded DLL name or XDK evidence
  BackendCaps()
      : backend(),
        maxControllers(0),
        analogTriggers(false),
        hasRumble(false),
        hasStickClicks(false),
        detail() {}
};

struct RumbleRequest {
  float left;   // 0..1
  float right;  // 0..1
  RumbleRequest() : left(0.0f), right(0.0f) {}
  RumbleRequest(float l, float r) : left(l), right(r) {}
};

std::string buttonName(std::uint16_t bit);
std::vector<std::string> pressedNames(std::uint16_t buttons);

}  // namespace input
}  // namespace pc2c
