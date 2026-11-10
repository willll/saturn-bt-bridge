// Logic of saturn_bt that does no I/O, kept apart so tests can exercise it.
#pragma once
#include <linux/input.h>
#include <endian.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

// Input code -> DS4 report byte/bit. Report bytes: 7 = face buttons, 8 = shoulders/Share/Options, 9 = PS.
struct Bind { uint16_t code; uint8_t byte, mask; };
inline const Bind MAP[] = {
  // keyboard: Saturn-like layout, bottom row Z X C, top row A S D
  // Saturn C/Z/L/R arrive from PS4 R2/L2/L1/R1: checked on a real RB-SGA-027 receiver
  {KEY_Z, 7, 0x20},     {KEY_X, 7, 0x40},     {KEY_C, 8, 0x08},      // Cross, Circle, R2
  {KEY_A, 7, 0x10},     {KEY_S, 7, 0x80},     {KEY_D, 8, 0x04},      // Square, Triangle, L2
  {KEY_Q, 8, 0x01},     {KEY_E, 8, 0x02},                            // L1, R1
  {KEY_ENTER, 8, 0x20}, {KEY_BACKSPACE, 8, 0x10}, {KEY_H, 9, 0x01},  // Options, Share, PS
  // gamepad
  {BTN_SOUTH, 7, 0x20}, {BTN_EAST, 7, 0x40},   {BTN_WEST, 7, 0x10},  {BTN_NORTH, 7, 0x80},
  {BTN_TL, 8, 0x01},    {BTN_TR, 8, 0x02},     {BTN_TL2, 8, 0x04},   {BTN_TR2, 8, 0x08},
  {BTN_SELECT, 8, 0x10}, {BTN_START, 8, 0x20}, {BTN_MODE, 9, 0x01},
};
inline const uint8_t HAT[3][3] = {{7, 0, 1}, {6, 8, 2}, {5, 4, 3}};  // [dy+1][dx+1], 8 = released

// Command button name -> the keyboard key that already sends it (see MAP), so commands reuse the key path.
struct Name { const char* name; uint16_t code; };
inline const Name NAMES[] = {
  {"A", KEY_Z}, {"B", KEY_X}, {"C", KEY_C}, {"X", KEY_A}, {"Y", KEY_S}, {"Z", KEY_D},
  {"L", KEY_Q}, {"R", KEY_E}, {"START", KEY_ENTER}, {"HOME", KEY_H},
  {"UP", KEY_UP}, {"DOWN", KEY_DOWN}, {"LEFT", KEY_LEFT}, {"RIGHT", KEY_RIGHT},
};
struct Step { int code; bool down; int ms; };  // code < 0 means "wait ms"

// Parses one command: "press A+B [ms]", "hold A", "release A|all", "wait ms". Returns false if malformed.
// The same grammar is checked in saturn_mcp.py: keep both in sync.
inline bool parse(std::string cmd, std::vector<Step>& out) {
  for (char& ch : cmd) ch = (char)toupper((unsigned char)ch);
  std::istringstream in(cmd);
  std::string verb, arg, extra;
  if (!(in >> verb)) return true;  // blank line
  if (!(in >> arg)) return false;
  char* end;
  if (verb == "WAIT") {
    long ms = strtol(arg.c_str(), &end, 10);
    if (*end || ms < 0 || in >> extra) return false;
    out.push_back({-1, false, (int)ms});
    return true;
  }
  long ms = 100;
  if (in >> extra) {
    ms = strtol(extra.c_str(), &end, 10);
    if (*end || ms < 0 || verb != "PRESS" || in >> extra) return false;
  }
  std::vector<int> codes;
  if (arg == "ALL") {
    for (auto& n : NAMES) codes.push_back(n.code);
  } else {
    if (arg.back() == '+') return false;  // getline would silently drop the empty last name
    std::istringstream names(arg);
    for (std::string name; std::getline(names, name, '+');) {
      int code = -1;
      for (auto& n : NAMES) if (name == n.name) code = n.code;
      if (code < 0) return false;
      codes.push_back(code);
    }
  }
  if (verb != "PRESS" && verb != "HOLD" && verb != "RELEASE") return false;
  if (verb != "RELEASE") for (int c : codes) out.push_back({c, true, 0});
  if (verb == "HOLD") return true;
  if (verb == "PRESS") out.push_back({-1, false, (int)ms});
  for (int c : codes) out.push_back({c, false, 0});
  if (verb == "PRESS") out.push_back({-1, false, 50});  // gap so back-to-back presses register as two
  return true;
}

inline uint32_t crc32(const uint8_t* p, size_t n, uint32_t c = 0) {
  c = ~c;
  while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
  return ~c;
}

// DS4 Bluetooth reports end with a CRC32 over the HIDP header byte + report.
inline void put_crc(uint8_t* msg, size_t len) {
  uint32_t c = htole32(crc32(msg, len - 4));
  memcpy(msg + len - 4, &c, 4);
}

// Fills msg with DS4 input report 0x11 (HIDP header 0xA1 first) for the pressed keys and hat axes.
inline void build_report(uint8_t (&msg)[79], const bool* down, int hx, int hy, uint8_t counter) {
  memset(msg, 0, sizeof msg);
  uint8_t* r = msg + 1;
  msg[0] = 0xA1;
  r[0] = 0x11;
  r[1] = 0xC0;
  // shortcut: analog sticks stay centered, the Saturn pad is digital; map ABS_X/Y here for the 3D pad.
  r[3] = r[4] = r[5] = r[6] = 0x80;
  r[9] = uint8_t(counter << 2);
  for (const Bind& b : MAP) if (down[b.code]) r[b.byte] |= b.mask;
  int dx = (down[KEY_RIGHT] || down[BTN_DPAD_RIGHT] || hx > 0) - (down[KEY_LEFT] || down[BTN_DPAD_LEFT] || hx < 0);
  int dy = (down[KEY_DOWN] || down[BTN_DPAD_DOWN] || hy > 0) - (down[KEY_UP] || down[BTN_DPAD_UP] || hy < 0);
  r[7] |= HAT[dy + 1][dx + 1];
  r[10] = (r[8] & 0x04) ? 0xFF : 0;  // L2/R2 also have analog bytes
  r[11] = (r[8] & 0x08) ? 0xFF : 0;
  put_crc(msg, sizeof msg);
}

// Reply to one HIDP control-channel message (n >= 1). Returns the reply length,
// 0 for no reply, or -1 when the host unplugged the virtual cable.
inline int ctrl_reply(const uint8_t* m, size_t n, uint8_t (&out)[64]) {
  memset(out, 0, sizeof out);
  uint8_t type = m[0] >> 4, param = m[0] & 0x0f;
  if (type == 0x1) return param == 0x5 ? -1 : 0;  // HID_CONTROL; 0x5 = virtual cable unplug
  if (type == 0x4 && n >= 2) {                    // GET_REPORT: zeroed report of the DS4 size
    uint8_t id = m[1];
    int len = id == 0x05 ? 41 : id == 0xA3 ? 49 : id == 0x81 ? 7 : id == 0x12 ? 16 : 37;
    out[0] = uint8_t(0xA0 | (param & 3));
    out[1] = id;
    put_crc(out, len + 1);
    return len + 1;
  }
  if (type == 0x5 || type == 0x7 || type == 0x9) return 1;  // SET_REPORT/PROTOCOL/IDLE: handshake OK (0x00)
  if (type == 0x6) {                                         // GET_PROTOCOL: report mode
    out[0] = 0xA0;
    out[1] = 0x01;
    return 2;
  }
  out[0] = 0x03;  // handshake: unsupported request
  return 1;
}
