# saturn_bt

Turns a Linux PC into a Bluetooth controller for the **Retro-Bit SEGA Saturn Bluetooth receiver** (RB-SGA-027).
The receiver only accepts PS4, Xbox One and Switch controllers, so the program pretends to be a **PS4 DualShock 4**.
Your keyboard or USB gamepad drives it, and so can scripts or an AI agent (Claude Code, Copilot) through text commands.

## Documentation

- [How it works](docs/how-it-works.md): architecture, Bluetooth details, code map
- [Commands](docs/commands.md): the command language, MCP tools, scripting
- [Troubleshooting](docs/troubleshooting.md): setup errors, pairing, agent problems
- [Development](docs/development.md): tests, changing the button mapping, adding buttons

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

2. Turn off BlueZ's input plugin, which otherwise holds the HID ports:

   ```sh
   sudo systemctl edit bluetooth
   ```
   ```ini
   [Service]
   ExecStart=
   ExecStart=/usr/libexec/bluetooth/bluetoothd -P input
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
4. Plug the receiver into the powered-on Saturn and press its pairing button.
   If `bluetoothctl` asks you to confirm, answer `yes`.
5. Once it says `Connected.`, write down the receiver's address (`devices` in bluetoothctl), then `trust <MAC>`.

Next time, either repeat step 3–4, or connect directly:
```sh
sudo ./build/saturn_bt -m AA:BB:CC:DD:EE:FF
```

## Controls

| Keyboard | Gamepad | Sent as (PS4) |
|---|---|---|
| Arrows | D-pad | D-pad |
| Z / X / C | South / East / R1 | Cross / Circle / R1 |
| A / S / D | West / North / L1 | Square / Triangle / L1 |
| Q / E | L2 / R2 | L2 / R2 |
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
