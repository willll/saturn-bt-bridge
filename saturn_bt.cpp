// saturn_bt: pretends to be a PS4 DualShock 4 over Bluetooth so a Retro-Bit
// SEGA Saturn Bluetooth receiver (RB-SGA-027) pairs with this PC.
// Input comes from a Linux evdev device (keyboard or gamepad) and/or text
// commands written to FIFO_PATH (for scripts and AI agents).
// Needs the BlueZ setup described in README.md, and root.
#include <systemd/sd-bus.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <endian.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <sstream>
#include <string>
#include <vector>

// Kernel ABI from <bluetooth/l2cap.h>, declared here to avoid needing libbluetooth-dev.
struct sockaddr_l2 { sa_family_t family; uint16_t psm; uint8_t bdaddr[6]; uint16_t cid; uint8_t type; };
constexpr int BTPROTO_L2CAP = 0;
constexpr uint16_t PSM_CTRL = 0x11, PSM_INTR = 0x13;

// Minimal HID descriptor: report 0x11 (77 bytes) in and out, the DS4 Bluetooth report size.
static const char SDP_RECORD[] = R"(<?xml version="1.0" encoding="UTF-8" ?>
<record>
  <attribute id="0x0001"><sequence><uuid value="0x1124" /></sequence></attribute>
  <attribute id="0x0004"><sequence>
    <sequence><uuid value="0x0100" /><uint16 value="0x0011" /></sequence>
    <sequence><uuid value="0x0011" /></sequence>
  </sequence></attribute>
  <attribute id="0x0005"><sequence><uuid value="0x1002" /></sequence></attribute>
  <attribute id="0x0006"><sequence><uint16 value="0x656e" /><uint16 value="0x006a" /><uint16 value="0x0100" /></sequence></attribute>
  <attribute id="0x0009"><sequence><sequence><uuid value="0x1124" /><uint16 value="0x0100" /></sequence></sequence></attribute>
  <attribute id="0x000d"><sequence><sequence>
    <sequence><uuid value="0x0100" /><uint16 value="0x0013" /></sequence>
    <sequence><uuid value="0x0011" /></sequence>
  </sequence></sequence></attribute>
  <attribute id="0x0100"><text value="Wireless Controller" /></attribute>
  <attribute id="0x0101"><text value="Game Pad" /></attribute>
  <attribute id="0x0102"><text value="Sony Computer Entertainment" /></attribute>
  <attribute id="0x0200"><uint16 value="0x0100" /></attribute>
  <attribute id="0x0201"><uint16 value="0x0111" /></attribute>
  <attribute id="0x0202"><uint8 value="0x08" /></attribute>
  <attribute id="0x0203"><uint8 value="0x00" /></attribute>
  <attribute id="0x0204"><boolean value="true" /></attribute>
  <attribute id="0x0205"><boolean value="true" /></attribute>
  <attribute id="0x0206"><sequence><sequence><uint8 value="0x22" />
    <text encoding="hex" value="05010905a10185110600ff0920150026ff007508954d81020921954d9102c0" />
  </sequence></sequence></attribute>
  <attribute id="0x0207"><sequence><sequence><uint16 value="0x0409" /><uint16 value="0x0100" /></sequence></sequence></attribute>
  <attribute id="0x020b"><uint16 value="0x0100" /></attribute>
  <attribute id="0x020c"><uint16 value="0x0c80" /></attribute>
  <attribute id="0x020d"><boolean value="false" /></attribute>
  <attribute id="0x020e"><boolean value="false" /></attribute>
</record>)";

// Input code -> DS4 report byte/bit. Report bytes: 7 = face buttons, 8 = shoulders/Share/Options, 9 = PS.
struct Bind { uint16_t code; uint8_t byte, mask; };
static const Bind MAP[] = {
  // keyboard: Saturn-like layout, bottom row Z X C, top row A S D
  {KEY_Z, 7, 0x20},     {KEY_X, 7, 0x40},     {KEY_C, 8, 0x02},      // Cross, Circle, R1
  {KEY_A, 7, 0x10},     {KEY_S, 7, 0x80},     {KEY_D, 8, 0x01},      // Square, Triangle, L1
  {KEY_Q, 8, 0x04},     {KEY_E, 8, 0x08},                            // L2, R2
  {KEY_ENTER, 8, 0x20}, {KEY_BACKSPACE, 8, 0x10}, {KEY_H, 9, 0x01},  // Options, Share, PS
  // gamepad
  {BTN_SOUTH, 7, 0x20}, {BTN_EAST, 7, 0x40},   {BTN_WEST, 7, 0x10},  {BTN_NORTH, 7, 0x80},
  {BTN_TL, 8, 0x01},    {BTN_TR, 8, 0x02},     {BTN_TL2, 8, 0x04},   {BTN_TR2, 8, 0x08},
  {BTN_SELECT, 8, 0x10}, {BTN_START, 8, 0x20}, {BTN_MODE, 9, 0x01},
};
static const uint8_t HAT[3][3] = {{7, 0, 1}, {6, 8, 2}, {5, 4, 3}};  // [dy+1][dx+1], 8 = released

// /run is root-only, so nobody can plant a symlink here before we chmod it.
constexpr const char* FIFO_PATH = "/run/saturn_bt.fifo";
constexpr const char* STATUS_PATH = "/run/saturn_bt.status";  // read by saturn_mcp.py

// Command button name -> the keyboard key that already sends it (see MAP), so commands reuse the key path.
static const struct { const char* name; uint16_t code; } NAMES[] = {
  {"A", KEY_Z}, {"B", KEY_X}, {"C", KEY_C}, {"X", KEY_A}, {"Y", KEY_S}, {"Z", KEY_D},
  {"L", KEY_Q}, {"R", KEY_E}, {"START", KEY_ENTER}, {"HOME", KEY_H},
  {"UP", KEY_UP}, {"DOWN", KEY_DOWN}, {"LEFT", KEY_LEFT}, {"RIGHT", KEY_RIGHT},
};
struct Step { int code; bool down; int ms; };  // code < 0 means "wait ms"

// Parses one command: "press A+B [ms]", "hold A", "release A|all", "wait ms". Returns false if malformed.
static bool parse(std::string cmd, std::vector<Step>& out) {
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

static long long now_ms() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}

static void status(const char* text) {
  puts(text);
  if (FILE* f = fopen(STATUS_PATH, "w")) { fprintf(f, "%s\n", text); fclose(f); }
}

[[noreturn]] static void die(const char* what) { perror(what); exit(1); }

static uint32_t crc32(const uint8_t* p, size_t n, uint32_t c = 0) {
  c = ~c;
  while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
  return ~c;
}

// DS4 Bluetooth reports end with a CRC32 over the HIDP header byte + report.
static void put_crc(uint8_t* msg, size_t len) {
  uint32_t c = htole32(crc32(msg, len - 4));
  memcpy(msg + len - 4, &c, 4);
}

static sd_bus* bluez_setup() {
  sd_bus* bus = nullptr;
  sd_bus_error err = SD_BUS_ERROR_NULL;
  if (sd_bus_open_system(&bus) < 0) die("sd_bus_open_system");
  auto check = [&](int r, const char* what) {
    if (r < 0) { fprintf(stderr, "%s: %s\n", what, err.message ? err.message : strerror(-r)); exit(1); }
  };
  // The SDP record lives as long as this bus connection does.
  check(sd_bus_call_method(bus, "org.bluez", "/org/bluez", "org.bluez.ProfileManager1", "RegisterProfile", &err,
                           nullptr, "osa{sv}", "/saturnbt/hid", "00001124-0000-1000-8000-00805f9b34fb", 3,
                           "ServiceRecord", "s", SDP_RECORD, "Role", "s", "server",
                           "RequireAuthentication", "b", 0),
        "RegisterProfile");
  // shortcut: adapter hard-coded to hci0, add an argument if you have several dongles.
  auto set = [&](const char* name, const char* type, auto value) {
    check(sd_bus_set_property(bus, "org.bluez", "/org/bluez/hci0", "org.bluez.Adapter1", name, &err, type, value), name);
  };
  set("Powered", "b", 1);
  set("Alias", "s", "Wireless Controller");  // the name a real DS4 advertises
  set("Pairable", "b", 1);
  set("DiscoverableTimeout", "u", 0u);
  set("Discoverable", "b", 1);
  return bus;
}

static int l2_socket() {
  int s = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  if (s < 0) die("bluetooth socket");
  return s;
}

static int l2_listen(uint16_t psm) {
  int s = l2_socket();
  sockaddr_l2 a{};
  a.family = AF_BLUETOOTH;
  a.psm = htole16(psm);
  if (bind(s, (sockaddr*)&a, sizeof a) < 0) {
    perror("bind HID port");
    fprintf(stderr, "Is bluetoothd running with '-P input'? See README.md.\n");
    exit(1);
  }
  if (listen(s, 1) < 0) die("listen");
  return s;
}

static int l2_connect(const uint8_t* addr, uint16_t psm) {
  int s = l2_socket();
  sockaddr_l2 a{};
  a.family = AF_BLUETOOTH;
  a.psm = htole16(psm);
  memcpy(a.bdaddr, addr, 6);
  if (connect(s, (sockaddr*)&a, sizeof a) < 0) die("connect to receiver");
  return s;
}

// Answers the host's requests on the HID control channel. Returns false when the link is gone.
static bool handle_ctrl(int ctrl) {
  uint8_t m[128];
  ssize_t n = recv(ctrl, m, sizeof m, 0);
  if (n <= 0) return false;
  uint8_t type = m[0] >> 4, param = m[0] & 0x0f;
  if (type == 0x1) return param != 0x5;  // HID_CONTROL; 0x5 = virtual cable unplug
  if (type == 0x4 && n >= 2) {           // GET_REPORT: reply with a zeroed report of the DS4 size
    uint8_t id = m[1];
    size_t len = id == 0x05 ? 41 : id == 0xA3 ? 49 : id == 0x81 ? 7 : id == 0x12 ? 16 : 37;
    uint8_t out[64] = {uint8_t(0xA0 | (param & 3)), id};
    put_crc(out, len + 1);
    return send(ctrl, out, len + 1, 0) > 0;
  }
  uint8_t ok[] = {0x00}, get_proto[] = {0xA0, 0x01}, unsupported[] = {0x03};
  if (type == 0x5 || type == 0x7 || type == 0x9) return send(ctrl, ok, 1, 0) > 0;  // SET_REPORT/PROTOCOL/IDLE
  if (type == 0x6) return send(ctrl, get_proto, 2, 0) > 0;                         // GET_PROTOCOL: report mode
  return send(ctrl, unsupported, 1, 0) > 0;
}

// Streams input as DS4 report 0x11 until the receiver disconnects.
static void run(int ctrl, int intr, int ev, int fifo) {
  static bool down[KEY_CNT];
  int hx = 0, hy = 0;
  uint8_t counter = 0;
  uint8_t msg[79] = {0xA1, 0x11, 0xC0, 0x00};
  uint8_t* r = msg + 1;
  std::string pending;
  std::deque<Step> queue;
  long long resume = 0;
  char buf[4096];
  while (read(fifo, buf, sizeof buf) > 0) {}  // drop commands sent while disconnected
  pollfd p[4] = {{ev, POLLIN, 0}, {ctrl, POLLIN, 0}, {intr, POLLIN, 0}, {fifo, POLLIN, 0}};  // ev may be -1: poll skips it
  for (;;) {
    if (poll(p, 4, 10) < 0) die("poll");
    if (p[3].revents) {
      ssize_t n = read(fifo, buf, sizeof buf);
      if (n > 0) pending.append(buf, n);
      for (size_t end; (end = pending.find_first_of("\n;")) != std::string::npos;) {
        std::string cmd = pending.substr(0, end);
        pending.erase(0, end + 1);
        std::vector<Step> steps;
        if (parse(cmd, steps)) queue.insert(queue.end(), steps.begin(), steps.end());
        else fprintf(stderr, "bad command: %s\n", cmd.c_str());
      }
    }
    for (long long t = now_ms(); !queue.empty() && t >= resume; queue.pop_front()) {
      if (queue.front().code < 0) resume = t + queue.front().ms;
      else down[queue.front().code] = queue.front().down;
    }
    if (p[0].revents) {
      input_event e[64];
      ssize_t n = read(ev, e, sizeof e);
      if (n <= 0) die("read input device");
      for (ssize_t i = 0; i < n / (ssize_t)sizeof *e; i++) {
        if (e[i].type == EV_KEY && e[i].code < KEY_CNT) down[e[i].code] = e[i].value != 0;
        if (e[i].type == EV_ABS && e[i].code == ABS_HAT0X) hx = e[i].value;
        if (e[i].type == EV_ABS && e[i].code == ABS_HAT0Y) hy = e[i].value;
      }
    }
    if (p[1].revents && !handle_ctrl(ctrl)) return;
    if (p[2].revents) {  // output reports (rumble/lightbar) are ignored
      uint8_t junk[128];
      if (recv(intr, junk, sizeof junk, 0) <= 0) return;
    }

    // shortcut: analog sticks stay centered, the Saturn pad is digital; map ABS_X/Y here for the 3D pad.
    r[3] = r[4] = r[5] = r[6] = 0x80;
    r[7] = r[8] = 0;
    r[9] = uint8_t(counter++ << 2);
    for (const Bind& b : MAP) if (down[b.code]) r[b.byte] |= b.mask;
    int dx = (down[KEY_RIGHT] || down[BTN_DPAD_RIGHT] || hx > 0) - (down[KEY_LEFT] || down[BTN_DPAD_LEFT] || hx < 0);
    int dy = (down[KEY_DOWN] || down[BTN_DPAD_DOWN] || hy > 0) - (down[KEY_UP] || down[BTN_DPAD_UP] || hy < 0);
    r[7] |= HAT[dy + 1][dx + 1];
    r[10] = (r[8] & 0x04) ? 0xFF : 0;  // L2/R2 also have analog bytes
    r[11] = (r[8] & 0x08) ? 0xFF : 0;
    put_crc(msg, sizeof msg);
    if (send(intr, msg, sizeof msg, 0) <= 0) return;
  }
}

int main(int argc, char** argv) {
  assert(crc32((const uint8_t*)"123456789", 9) == 0xCBF43926);  // standard CRC-32 check value
  std::vector<Step> t;
  assert(parse("press a+b 200", t) && t.size() == 6 && t[2].ms == 200 && t[3].code == KEY_Z && !t[3].down);
  assert(!parse("press Q", t) && !parse("wait x", t) && !parse("hold A 5", t) && parse("  ", t));

  setvbuf(stdout, nullptr, _IOLBF, 0);  // so `| tee log` shows status immediately
  const char *input = nullptr, *mac = nullptr;
  for (int o; (o = getopt(argc, argv, "i:m:")) != -1;) {
    if (o == 'i') input = optarg;
    else if (o == 'm') mac = optarg;
    else {
      fprintf(stderr, "usage: sudo %s [-i /dev/input/eventN] [-m RECEIVER_MAC]\n"
                      "  -i  keyboard/gamepad to read (optional; commands also come from %s)\n"
                      "  -m  connect to an already paired receiver instead of waiting for it\n", argv[0], FIFO_PATH);
      return 1;
    }
  }
  int ev = -1;
  if (input && (ev = open(input, O_RDONLY)) < 0) die(input);
  if (mkfifo(FIFO_PATH, 0666) < 0 && errno != EEXIST) die(FIFO_PATH);
  chmod(FIFO_PATH, 0666);  // umask strips write for non-root agents
  int fifo = open(FIFO_PATH, O_RDWR | O_NONBLOCK);  // RDWR: no EOF each time a writer closes
  if (fifo < 0) die(FIFO_PATH);
  printf("Commands: echo 'press A' > %s\n", FIFO_PATH);
  sd_bus* bus = bluez_setup();
  (void)bus;  // kept open so the SDP record stays registered

  if (mac) {
    uint8_t a[6];  // bdaddr is stored byte-reversed
    if (sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &a[5], &a[4], &a[3], &a[2], &a[1], &a[0]) != 6) {
      fprintf(stderr, "bad MAC address: %s\n", mac);
      return 1;
    }
    int ctrl = l2_connect(a, PSM_CTRL), intr = l2_connect(a, PSM_INTR);
    status("Connected.");
    run(ctrl, intr, ev, fifo);
    status("Disconnected.");
    return 0;
  }

  int lc = l2_listen(PSM_CTRL), li = l2_listen(PSM_INTR);
  status("Visible as 'Wireless Controller'. Press the pairing button on the Saturn receiver.");
  for (;;) {
    int ctrl = accept(lc, nullptr, nullptr);
    if (ctrl < 0) die("accept");
    int intr = accept(li, nullptr, nullptr);
    if (intr < 0) die("accept");
    status("Connected.");
    run(ctrl, intr, ev, fifo);
    close(ctrl);
    close(intr);
    status("Disconnected, waiting for the receiver again.");
  }
}
