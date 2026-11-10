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

1. In `/etc/bluetooth/main.conf`, under `[General]`, make the PC look like a DS4. The file usually already
   has `#Class = ...` and `#DeviceID = ...` lines: remove the `#` and change the values.

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
   pgrep -a bluetoothd       # must end with -P input,hostname
   ```

   ⚠ While the plugin is off, Bluetooth keyboards, mice and gamepads **on this PC** stop working.

## Connecting

Plug the receiver into the Saturn and turn the Saturn on. The receiver is named `ZXD_BT_HOST` on Bluetooth.

### First time: pair

Do this once. All steps take about a minute.

1. Close **Settings → Bluetooth** if it is open: it keeps the PC scanning and uses its own pairing agent.
2. Terminal A: accept pairing without a PIN. Leave it open until step 8.
   ```sh
   bluetoothctl
   agent NoInputNoOutput
   default-agent
   ```
3. Terminal B: start the bridge. Leave it running.
   ```sh
   sudo ./build/saturn_bt
   ```
   It prints `Visible as 'Wireless Controller'. Hold the pairing button on the Saturn receiver.`
4. Terminal C: make the PC look exactly like a PS4 pad in pairing mode.
   ```sh
   systemctl --user stop wireplumber obex      # PC sound stops until step 8
   sudo btmgmt discov limited 180              # visible for 3 minutes, like a pad in pairing mode
   bluetoothctl show | grep Class              # must be 0x00002508
   ```
   The receiver ignores the PC while the class has extra bits (e.g. `0x7c2508`): they come from
   the sound and file-sharing services stopped above.
5. Within those 3 minutes, **hold** the receiver's pairing button until its LED blinks fast.
   A short press is not enough. If `bluetoothctl` asks to confirm, answer `yes`.
6. Terminal B prints `Connected.` The Saturn now gets your input.
7. Get the receiver's address (its MAC). While it is connected, in any terminal:
   ```sh
   bluetoothctl devices Connected
   ```
   ```text
   Device 00:90:E1:6B:0E:52 ZXD_BT_HOST
   ```
   The address is the `XX:XX:XX:XX:XX:XX` part before `ZXD_BT_HOST`. Each receiver has its own: write
   yours down. Later, even when it is not connected, `bluetoothctl devices | grep ZXD_BT_HOST` shows it.
8. In Terminal A, remember the receiver so you can reconnect without the pairing button,
   then quit and turn sound back on:
   ```sh
   trust 00:90:E1:6B:0E:52                     # your receiver's address
   quit
   ```
   ```sh
   systemctl --user start wireplumber obex
   ```

### After that: reconnect

```sh
sudo ./build/saturn_bt -m 00:90:E1:6B:0E:52    # your receiver's address, from step 7
```

No pairing button, no `bluetoothctl`, sound can stay on. It prints `Connected.` within a few seconds.

With `-m`, `saturn_bt` exits when the link drops (Saturn turned off or reset, receiver unplugged).
Run the same command again. Commands sent while disconnected are dropped, not queued.
If it fails with `connect to receiver: ...`, the Saturn is off or the receiver is not plugged in;
if it keeps failing, pair again.

### Or: keep it connected with a service (no sudo afterwards)

A systemd service runs `saturn_bt -m <MAC>` in the background and restarts it 5 seconds after it exits.
Turn the Saturn on and it connects by itself within about 10 seconds; turn it off and the service keeps
retrying quietly. Agents and scripts work as usual, and nobody has to type `sudo` again.

Install once, after the first pairing:
```sh
sudo cmake --install build                                  # /usr/local/bin/saturn_bt + the service file
sudo systemctl daemon-reload
sudo systemctl enable --now saturn-bt@00:90:E1:6B:0E:52     # your receiver's address
```

Optional: let your desktop user start, stop and restart it without `sudo`:
```sh
sudo cp systemd/50-saturn-bt.rules /etc/polkit-1/rules.d/
```

Then:
```sh
systemctl status saturn-bt@00:90:E1:6B:0E:52               # running? connected?
journalctl -u saturn-bt@00:90:E1:6B:0E:52 -f               # its messages
systemctl stop saturn-bt@00:90:E1:6B:0E:52                 # release the receiver (until next boot)
systemctl start saturn-bt@00:90:E1:6B:0E:52
```

- Stop the service before pairing a receiver again or running `./build/saturn_bt` by hand.
- After rebuilding, run `sudo cmake --install build` and `systemctl restart saturn-bt@...` again.
- The service runs the installed copy in `/usr/local/bin`, not `./build/saturn_bt`, because it runs as root
  and a program in your home folder could be replaced by any of your programs.

### Options

| Option | Effect |
|---|---|
| `-m MAC` | connect to a receiver paired before, instead of waiting for its pairing button |
| `-i /dev/input/by-id/...-event-kbd` | also drive the Saturn from this keyboard or gamepad (`ls -l /dev/input/by-id/`; gamepads end in `-event-joystick`) |

Without `-i`, only AI agents and scripts drive the Saturn.

## Controls

| Keyboard | Gamepad | Sent as (PS4) | Saturn |
|---|---|---|---|
| Arrows | D-pad | D-pad | D-pad |
| Z / X / C | South / East / R2 | Cross / Circle / R2 | A / B / C |
| A / S / D | West / North / L2 | Square / Triangle / L2 | X / Y / Z |
| Q / E | L1 / R1 | L1 / R1 | L / R |
| Enter | Start | Options | START |
| Backspace | Select | Share | not tested |
| H | Home | PS | not tested |

The receiver decides which PS4 button becomes which Saturn button. The Saturn column was checked on a real
receiver with the [pad monitor sample](samples/saturn-pad-monitor/README.md); if yours differs,
[change the mapping](docs/development.md#changing-the-button-mapping).
Keys still reach the terminal while it runs; that's harmless.

## AI agents (MCP)

`src/saturn_mcp.py` is an MCP server (Python standard library only) with two tools, `status` and `send_commands`.
It is already registered for this folder:

- **Claude Code**: `.mcp.json`. Run `claude` here and approve the `saturn` server once.
- **VS Code Copilot**: `.vscode/mcp.json`. Open this folder, start the `saturn` server, use Agent mode.

Start `saturn_bt` yourself first and wait for `Connected.` (it needs root); the MCP server runs as your normal user.
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
sudo systemctl disable --now saturn-bt@00:90:E1:6B:0E:52     # your receiver's address
sudo rm /usr/local/bin/saturn_bt /usr/local/lib/systemd/system/saturn-bt@.service /etc/polkit-1/rules.d/50-saturn-bt.rules
sudo systemctl revert bluetooth
# remove the Class/DeviceID lines from /etc/bluetooth/main.conf
sudo systemctl restart bluetooth
bluetoothctl system-alias "$(hostname)"
```
