#include "pc2c/input/Diagnostics.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

namespace pc2c::input {

json::Value DiagReport::toJson() const {
  json::Value o = json::Value::Obj();
  o.set("tool", json::Value::Str(std::string(kToolName) + " " + kToolVersion));
  o.set("backend", json::Value::Str(backend));
  o.set("tested_on", json::Value::Str(testedOn));
  o.set("ever_connected", json::Value::Bool(everConnected));
  o.set("disconnects", json::Value::Int(disconnects));
  o.set("poll_hz", json::Value::Num(pollHz));
  o.set("frames", json::Value::Int(frames));
  o.set("lx_range", json::Value::Str(std::to_string(minLX) + ".." + std::to_string(maxLX)));
  o.set("ly_range", json::Value::Str(std::to_string(minLY) + ".." + std::to_string(maxLY)));
  o.set("rx_range", json::Value::Str(std::to_string(minRX) + ".." + std::to_string(maxRX)));
  o.set("ry_range", json::Value::Str(std::to_string(minRY) + ".." + std::to_string(maxRY)));
  o.set("lt_max", json::Value::Int(maxLT));
  o.set("rt_max", json::Value::Int(maxRT));
  o.set("raw_extremes", json::Value::Str(rawNote));
  json::Value::Array bs;
  for (const auto& b : buttonsSeen) bs.push_back(json::Value::Str(b));
  o.set("buttons_seen", json::Value::Arr(std::move(bs)));
  o.set("rumble", json::Value::Str(rumbleResult));
  o.set("note", json::Value::Str(everConnected ? "live controller data"
                                              : "no controller attached during sampling"));
  return o;
}

DiagReport runDiagnostics(ControllerManager& mgr, const DiagOptions& opts) {
  DiagReport r;
  r.backend = mgr.backend().name();
  r.caps = mgr.backend().caps();
#ifdef _XBOX
  r.testedOn = "xbox360";
#else
  r.testedOn = "pc-desktop";
#endif

  using Clock = std::chrono::steady_clock;
  auto t0 = Clock::now();
  auto tend = t0 + std::chrono::duration<double>(opts.seconds > 0 ? opts.seconds : 5.0);
  std::uint64_t nowMs = 0;
  bool rumbleSent = false;
  std::uint16_t seenMask = 0;
  std::vector<Vec2> scatter;
  scatter.reserve(2048);
  bool first = true;
  std::size_t n = 0;
  // Raw extremes (pre-deadzone) across the session.
  std::int16_t rMinLX = 0, rMaxLX = 0, rMinLY = 0, rMaxLY = 0;
  std::int16_t rMinRX = 0, rMaxRX = 0, rMinRY = 0, rMaxRY = 0;

  if (opts.rumbleTest && r.caps.hasRumble) mgr.rumble().pulse(0.7f, 0.7f, 600, 0);

  while (Clock::now() < tend) {
    auto now = Clock::now();
    nowMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count());
    mgr.update(nowMs);
    const Frame& f = mgr.frame();
    n++;
    r.disconnects = f.disconnects;
    if (f.connected) {
      r.everConnected = true;
      seenMask |= f.buttons;
      if (first) {
        r.minLX = r.maxLX = f.left.x;
        r.minLY = r.maxLY = f.left.y;
        r.minRX = r.maxRX = f.right.x;
        r.minRY = r.maxRY = f.right.y;
        rMinLX = rMaxLX = f.rawLX;
        rMinLY = rMaxLY = f.rawLY;
        rMinRX = rMaxRX = f.rawRX;
        rMinRY = rMaxRY = f.rawRY;
        r.maxLT = r.maxRT = 0;
        if (f.rawLT > r.maxLT) r.maxLT = f.rawLT;
        if (f.rawRT > r.maxRT) r.maxRT = f.rawRT;
        first = false;
      } else {
        if (f.left.x < r.minLX) r.minLX = f.left.x;
        if (f.left.x > r.maxLX) r.maxLX = f.left.x;
        if (f.left.y < r.minLY) r.minLY = f.left.y;
        if (f.left.y > r.maxLY) r.maxLY = f.left.y;
        if (f.right.x < r.minRX) r.minRX = f.right.x;
        if (f.right.x > r.maxRX) r.maxRX = f.right.x;
        if (f.right.y < r.minRY) r.minRY = f.right.y;
        if (f.right.y > r.maxRY) r.maxRY = f.right.y;
      }
      // Raw extremes (pre-deadzone) tracked alongside processed ranges.
      if (f.rawLX < rMinLX) rMinLX = f.rawLX;
      if (f.rawLX > rMaxLX) rMaxLX = f.rawLX;
      if (f.rawLY < rMinLY) rMinLY = f.rawLY;
      if (f.rawLY > rMaxLY) rMaxLY = f.rawLY;
      if (f.rawRX < rMinRX) rMinRX = f.rawRX;
      if (f.rawRX > rMaxRX) rMaxRX = f.rawRX;
      if (f.rawRY < rMinRY) rMinRY = f.rawRY;
      if (f.rawRY > rMaxRY) rMaxRY = f.rawRY;
      if (f.rawLT > r.maxLT) r.maxLT = f.rawLT;
      if (f.rawRT > r.maxRT) r.maxRT = f.rawRT;
      if (scatter.size() < 2048 && f.changed) scatter.push_back(f.left);
    }
    if (mgr.rumble().active()) rumbleSent = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  double elapsed =
      std::chrono::duration<double>(Clock::now() - t0).count();
  r.frames = n;
  r.pollHz = elapsed > 0 ? (double)n / elapsed : 0.0;
  r.buttonsSeen = pressedNames(seenMask);
  if (!opts.rumbleTest)
    r.rumbleResult = "not-run";
  else if (!r.caps.hasRumble)
    r.rumbleResult = "unsupported";
  else
    r.rumbleResult = (rumbleSent || !r.everConnected) ? (r.everConnected ? "ok" : "not-run")
                                                     : "failed";

  // ASCII deadzone plot: 25x13 grid, '+' center, 'o' deadzone ring, '*' samples.
  if (opts.ascii) {
    const int W = 25, H = 13;
    std::vector<std::string> g(H, std::string(W, '.'));
    float dz = mgr.profile().leftStick.deadzone;
    for (int yy = 0; yy < H; ++yy) {
      for (int xx = 0; xx < W; ++xx) {
        float fx = (xx / (float)(W - 1)) * 2 - 1;
        float fy = 1 - (yy / (float)(H - 1)) * 2;
        float m = std::sqrt(fx * fx + fy * fy);
        if (m <= dz) g[yy][xx] = 'o';
      }
    }
    g[H / 2][W / 2] = '+';
    for (const auto& p : scatter) {
      int xx = (int)((p.x + 1) / 2 * (W - 1) + 0.5f);
      int yy = (int)((1 - (p.y + 1) / 2) * (H - 1) + 0.5f);
      if (xx >= 0 && xx < W && yy >= 0 && yy < H) g[yy][xx] = '*';
    }
    std::string plot = "LX/LY deadzone=" + std::to_string(dz) + "\n";
    for (const auto& row : g) plot += row + "\n";
    r.asciiPlot = plot;
  }
  // Raw stick extremes -> note (processed ranges are in lx/ly/rx/ry_range).
  {
    char b[160];
    std::snprintf(b, sizeof(b), "raw lx %d..%d ly %d..%d rx %d..%d ry %d..%d",
                  rMinLX, rMaxLX, rMinLY, rMaxLY, rMinRX, rMaxRX, rMinRY, rMaxRY);
    r.rawNote = b;
  }
  return r;
}

}  // namespace pc2c::input
