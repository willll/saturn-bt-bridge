# Troubleshooting

## saturn_bt does not start

**`bind HID port: Address already in use`**
BlueZ's input plugin still owns the HID ports. Check that `-P input` is active:
```sh
ps -o args= -C bluetoothd     # should end with -P input
```
If not, redo step 2 of the BlueZ setup in the [README](../README.md#one-time-bluez-setup) and restart `bluetooth`.

**`bind HID port: Permission denied`**
Run it with `sudo`.

**`RegisterProfile: ...`**
`bluetoothd` is not running: `sudo systemctl start bluetooth`.

**`Powered: ...` / `Alias: ...` with "not found" or "unknown object"**
`saturn_bt` uses the adapter `hci0`. Check with `bluetoothctl list`. If your adapter has another name,
change `/org/bluez/hci0` in `src/saturn_bt.cpp`.

**`/dev/input/...: Permission denied` or `No such file or directory`**
Use `sudo`, and pick the device from `ls -l /dev/input/by-id/`. `-i` is optional.

## The receiver does not find or pair with the PC

1. Check the PC looks like a gamepad:
   ```sh
   bluetoothctl show
   ```
   Expect `Alias: Wireless Controller`, `Discoverable: yes`, `Pairable: yes`, and a `Class` ending in `508`.
   If the class is wrong, check `Class = 0x000508` is under `[General]` in `/etc/bluetooth/main.conf`
   and restart `bluetooth`.
2. Keep the `bluetoothctl` agent open (`agent NoInputNoOutput`, `default-agent`) and answer `yes` if asked.
3. Plug the receiver into a **powered-on** Saturn, then press its pairing button. Stay within a few meters.
4. If the receiver was paired with this PC before, remove the old pairing on the PC:
   `bluetoothctl remove <MAC>`, then pair again.
5. Watch what happens:
   ```sh
   sudo btmon
   ```
   If the receiver connects and then disconnects right away, it is probably rejecting the fake PS4 pad.
   Open an issue with the `btmon` output.

## Connected, but buttons are wrong or missing

The mapping from PS4 to Saturn buttons is a guess. Fix it as described in
[development.md](development.md#changing-the-button-mapping).

If nothing works at all, check that `saturn_bt` says `Connected.`, and try another game:
some games only read the pad on certain screens.

## AI agent problems

**The agent has no `saturn` tools**
- Claude Code: start `claude` in the repo folder and approve the `saturn` server. Check with `claude mcp get saturn`.
- VS Code: open the repo folder, open `.vscode/mcp.json`, click **Start** above `saturn`, and use Agent mode.

**`saturn_bt is not running`**
Start `sudo ./build/saturn_bt` yourself. The agent can't, because it needs root.

**`Not sent. Visible as 'Wireless Controller'...`**
`saturn_bt` runs but the receiver is not connected. Press the receiver's pairing button,
or restart with `-m <receiver MAC>`.

**Commands are queued but nothing happens on screen**
Check the game is on a screen that reacts to input. The agent cannot see the TV, so tell it what is shown.

## My other Bluetooth devices stopped working

That is the `-P input` setting: while it is on, this PC cannot use Bluetooth keyboards, mice or gamepads.
Undo it with the steps in the [README](../README.md#undo).
