# saturn_bt

Turns a Linux PC into a Bluetooth controller for the **Retro-Bit SEGA Saturn Bluetooth receiver** (RB-SGA-027).
The receiver only accepts PS4, Xbox One and Switch controllers, so the program pretends to be a **PS4 DualShock 4**.
Your keyboard or USB gamepad drives it, and so can scripts or an AI agent (Claude Code, Copilot) through text commands.

## Documentation

- [How it works](docs/how-it-works.md): architecture, Bluetooth details, code map
- [Commands](docs/commands.md): the command language, MCP tools, scripting
- [Troubleshooting](docs/troubleshooting.md): setup errors, pairing, agent problems
- [Development](docs/development.md): tests, changing the button mapping, adding buttons
- [Pad monitor sample](samples/saturn-pad-monitor/README.md): Saturn program that shows and logs the buttons it receives

## Build

```sh
sudo apt install g++ cmake pkg-config libsystemd-dev
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure   # tests, no Bluetooth or root needed
```

## One-time BlueZ setup

1. In `/etc/bluetooth/main.conf`, under `[General]`, make the PC look like a DS4:

   ```ini
   Class = 0x000508
   DeviceID = usb:054C:05C4:0100
   ```

2. Turn off two BlueZ plugins: `input` holds the HID ports, and `hostname` overwrites the class with "laptop":

   ```sh
   sudo systemctl edit bluetooth
   ```
   ```ini
   [Service]
   ExecStart=
   ExecStart=/usr/libexec/bluetooth/bluetoothd -P input,hostname
   ```
   ```sh
   sudo systemctl restart bluetooth
   ```

   ⚠ While the plugin is off, Bluetooth keyboards, mice and gamepads **on this PC** stop working.

## Pairing

1. In a second terminal, accept pairing without a PIN, and leave it open:
   ```sh
   bluetoothctl
   agent NoInputNoOutput
   default-agent
   ```
2. Find your input device: `ls -l /dev/input/by-id/` (keyboards end in `-event-kbd`, gamepads in `-event-joystick`).
3. Run:
   ```sh
   sudo ./build/saturn_bt -i /dev/input/by-id/usb-XXXX-event-kbd
   ```
   `-i` is optional: leave it out to drive the Saturn only from an AI agent or script.
4. Make the PC look exactly like a PS4 pad in pairing mode. The receiver ignores the PC while its class
   has extra bits from the desktop's sound and file-sharing services (`0x7c2508` instead of `0x002508`):
   ```sh
   systemctl --user stop wireplumber obex      # PC sound stops until you start them again
   sudo btmgmt discov limited 180              # visible for 3 minutes, like a pad in pairing mode
   bluetoothctl show | grep Class              # must be 0x00002508
   ```
5. Within those 3 minutes, plug the receiver into the powered-on Saturn and hold its pairing button
   until the LED blinks fast. If `bluetoothctl` asks you to confirm, answer `yes`.
6. Once it says `Connected.`, write down the receiver's address (`devices` in bluetoothctl; it is named
   `ZXD_BT_HOST`), then `trust <MAC>`. Restart sound with `systemctl --user start wireplumber obex`.

Next time, either repeat steps 3–5, or connect directly:
```sh
sudo ./build/saturn_bt -m AA:BB:CC:DD:EE:FF
```

## Controls

| Keyboard | Gamepad | Sent as (PS4) |
|---|---|---|
| Arrows | D-pad | D-pad |
| Z / X / C | South / East / R2 | Cross / Circle / R2 |
| A / S / D | West / North / L2 | Square / Triangle / L2 |
| Q / E | L1 / R1 | L1 / R1 |
| Enter | Start | Options |
| Backspace | Select | Share |
| H | Home | PS |

Which PS4 button becomes which Saturn button is decided by the receiver, and the manual does not document it.
Test in-game and [change the mapping](docs/development.md#changing-the-button-mapping) if needed.
Keys still reach the terminal while it runs; that's harmless.

## AI agents (MCP)

`src/saturn_mcp.py` is an MCP server (Python standard library only) with two tools, `status` and `send_commands`.
It is already registered for this folder:

- **Claude Code**: `.mcp.json`. Run `claude` here and approve the `saturn` server once.
- **VS Code Copilot**: `.vscode/mcp.json`. Open this folder, start the `saturn` server, use Agent mode.

Start `saturn_bt` yourself first (it needs root); the MCP server runs as your normal user.
Then ask, for example: "press Start, wait a second, then choose the second menu item".

## Commands (scripts)

Scripts can skip MCP and write commands straight to `/run/saturn_bt.fifo`, one per line or separated by `;`.
Use a timeout: if `saturn_bt` isn't running, the write blocks forever.

```sh
timeout 2 sh -c 'echo "press START; wait 1500; press A" > /run/saturn_bt.fifo'
```

Full command reference: [docs/commands.md](docs/commands.md).

## Undo

```sh
sudo systemctl revert bluetooth
# remove the Class/DeviceID lines from /etc/bluetooth/main.conf
sudo systemctl restart bluetooth
bluetoothctl system-alias "$(hostname)"
```
