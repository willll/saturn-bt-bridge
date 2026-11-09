#undef NDEBUG  // keep asserts in Release builds
#include "bridge.hpp"
#include <cassert>
#include <cstdio>
#include <iterator>

static bool down[KEY_CNT];

static bool crc_ok(const uint8_t* m, size_t len) {
  uint32_t c;
  memcpy(&c, m + len - 4, 4);
  return le32toh(c) == crc32(m, len - 4);
}

static void test_parse() {
  std::vector<Step> s;
  assert(parse("press a+b 200", s) && s.size() == 6);
  assert(s[0].code == KEY_Z && s[0].down && s[1].code == KEY_X && s[1].down);
  assert(s[2].code < 0 && s[2].ms == 200);
  assert(s[3].code == KEY_Z && !s[3].down && s[4].code == KEY_X && !s[4].down);
  assert(s[5].code < 0 && s[5].ms == 50);

  s.clear();
  assert(parse("press START", s) && s.size() == 4 && s[0].code == KEY_ENTER && s[1].ms == 100);
  s.clear();
  assert(parse("hold up", s) && s.size() == 1 && s[0].code == KEY_UP && s[0].down);
  s.clear();
  assert(parse("release all", s) && s.size() == std::size(NAMES) && !s[0].down);
  s.clear();
  assert(parse("wait 1000", s) && s.size() == 1 && s[0].code < 0 && s[0].ms == 1000);
  s.clear();
  assert(parse("   ", s) && s.empty());

  for (const char* bad : {"press Q", "press", "wait x", "wait -1", "wait 5 6", "hold A 5", "press A 10 20",
                          "jump A", "press A+", "press +A", "press A -5"}) {
    assert(!parse(bad, s));
    assert(s.empty());
  }
}

static void test_report() {
  uint8_t msg[79];  // msg[k + 1] is DS4 report byte k
  build_report(msg, down, 0, 0, 0);
  assert(msg[0] == 0xA1 && msg[1] == 0x11 && msg[4] == 0x80);
  assert(msg[8] == 0x08 && msg[9] == 0 && msg[10] == 0);  // hat released, nothing pressed
  assert(crc_ok(msg, sizeof msg));

  down[KEY_UP] = down[KEY_RIGHT] = down[KEY_Z] = down[KEY_Q] = true;
  build_report(msg, down, 0, 0, 3);
  assert(msg[8] == (0x20 | 1));                 // Cross + hat up-right
  assert(msg[9] == 0x04 && msg[11] == 0xFF);    // L2 bit and analog L2
  assert(msg[10] == 3 << 2);                    // report counter
  assert(crc_ok(msg, sizeof msg));

  memset(down, 0, sizeof down);
  build_report(msg, down, -1, 1, 0);  // gamepad hat: left + down
  assert(msg[8] == 5);
}

static void test_ctrl() {
  uint8_t out[64];
  const uint8_t unplug[] = {0x15}, suspend[] = {0x13}, get_feature[] = {0x43, 0x05}, set_report[] = {0x52, 0x11},
                get_protocol[] = {0x60}, unknown[] = {0xD0};
  assert(ctrl_reply(unplug, 1, out) == -1);
  assert(ctrl_reply(suspend, 1, out) == 0);
  int n = ctrl_reply(get_feature, 2, out);
  assert(n == 42 && out[0] == 0xA3 && out[1] == 0x05 && crc_ok(out, n));
  assert(ctrl_reply(set_report, 2, out) == 1 && out[0] == 0x00);
  assert(ctrl_reply(get_protocol, 1, out) == 2 && out[0] == 0xA0 && out[1] == 0x01);
  assert(ctrl_reply(unknown, 1, out) == 1 && out[0] == 0x03);
}

int main() {
  assert(crc32((const uint8_t*)"123456789", 9) == 0xCBF43926);  // standard CRC-32 check value
  test_parse();
  test_report();
  test_ctrl();
  puts("ok");
}
