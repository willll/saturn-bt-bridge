# Troubleshooting

## saturn_bt does not start

**`bind HID port: Address already in use`**
BlueZ's input plugin still owns the HID ports. Check that `-P input,hostname` is active:
```sh
ps -o args= -C bluetoothd     # should end with -P input,hostname
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

## saturn_bt -m fails or the link drops

**`connect to receiver: Host is down`** (or `Connection refused`, `timed out`)
The Saturn is off, the receiver is not plugged in, or it is out of range. Fix that and run the command again.
If it still fails, pair again ([README](../README.md#first-time-pair)).

**`Disconnected.` while playing**
Turning off or resetting the Saturn drops the link. With `-m`, `saturn_bt` then exits: run it again.
Without `-m` it waits, but the receiver does not come back by itself: restart with `-m`.

## The receiver does not find or pair with the PC

1. Check the PC looks like a gamepad:
   ```sh
   bluetoothctl show
   ```
   Expect `Alias: Wireless Controller`, `Discoverable: yes`, `Pairable: yes`, and a `Class` ending in `508`.
   It must be exactly `0x00002508` (see README pairing step 4): with extra bits such as `0x7c2508`
   the receiver ignores the PC. If the class is wrong, check `Class = 0x000508` is under `[General]` in `/etc/bluetooth/main.conf`
   and restart `bluetooth`. If it ends in `10c` (laptop), BlueZ's `hostname` plugin overwrote it:
   `bluetoothd` must run with `-P input,hostname`.
2. Close **Settings → Bluetooth**: while open it scans nonstop and uses its own agent.
   Keep the `bluetoothctl` agent open (`agent NoInputNoOutput`, `default-agent`) and answer `yes` if asked.
3. Plug the receiver into a **powered-on** Saturn, then **hold** its pairing button until the LED blinks fast,
   within the 3 minutes of `btmgmt discov limited`. A short press is not enough. Stay within a few meters.
4. If the receiver was paired with this PC before, remove the old pairing on the PC:
   `bluetoothctl remove <MAC>`, then pair again.
5. Watch what happens:
   ```sh
   sudo btmon
   ```
   To save it to a file, use `sudo stdbuf -oL btmon | tee btmon.txt`: without `stdbuf` the end is lost on Ctrl+C.
   If the receiver connects and then disconnects right away, it is probably rejecting the fake PS4 pad.
   Open an issue with the `btmon` output.

## Connected, but buttons are wrong or missing

The mapping was checked on one receiver; others may differ. Check it with the
[pad monitor sample](../samples/saturn-pad-monitor/README.md) and fix it as described in
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
`saturn_bt` runs but the receiver is not connected. Restart it with `-m <receiver MAC>`,
or pair again ([README](../README.md#connecting)).

**Commands are queued but nothing happens on screen**
Check the game is on a screen that reacts to input. The agent cannot see the TV, so tell it what is shown.

## My other Bluetooth devices stopped working

That is the `-P input` setting: while it is on, this PC cannot use Bluetooth keyboards, mice or gamepads.
Undo it with the steps in the [README](../README.md#undo).
