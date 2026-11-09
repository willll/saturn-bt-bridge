# Development

## Build and test

```sh
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

The tests need neither Bluetooth nor root:

- `bridge` (`tests/test_bridge.cpp`): command parser, input report bytes and CRC, control-channel replies.
- `mcp` (`tests/test_saturn_mcp.py`): MCP handshake, tool list, command validation. Also runs on its own:
  `python3 tests/test_saturn_mcp.py`.

Put new logic that needs no I/O in `src/bridge.hpp`, so it can be tested the same way.

## Changing the button mapping

Three tables decide what a button does, all in `src/bridge.hpp`:

| Table | Maps | Change it when |
|---|---|---|
| `MAP` | keyboard key or gamepad button → bit in the PS4 report | a key sends the wrong PS4 button |
| `NAMES` | command name (`A`, `START`...) → keyboard key | adding or renaming a command button |
| `HAT` | D-pad direction → PS4 D-pad value | never, it is fixed by the PS4 format |

Commands reuse keyboard keys: `press A` acts exactly like pressing the Z key. So fix `MAP` and the
keyboard and commands both follow. Example: if Cross gives Saturn **B** and Circle gives Saturn **A**,
swap the masks of `KEY_Z` and `KEY_X` in `MAP` (`0x20` ↔ `0x40`). Do the same for the gamepad rows if needed.
Only touch `NAMES` to rename commands or add new ones.

Report bits used in `MAP` (byte, mask):

| Byte | 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80 |
|---|---|---|---|---|---|---|---|---|
| 7 | D-pad | D-pad | D-pad | D-pad | Square | Cross | Circle | Triangle |
| 8 | L1 | R1 | L2 | R2 | Share | Options | L3 | R3 |
| 9 | PS | Touchpad | counter | counter | counter | counter | counter | counter |

## Adding a command button

1. Add it to `NAMES` in `src/bridge.hpp`.
2. Add it to `BTN` and to the `GRAMMAR` text in `src/saturn_mcp.py`. The Python server checks commands
   before sending them, so it must accept the same names.
3. Update the button list in `AGENTS.md` and `docs/commands.md`.
4. Add a case to both test files and run `ctest`.

## Changing the command language

The grammar lives in two places that must agree: `parse()` in `src/bridge.hpp` and the `COMMAND` regex in
`src/saturn_mcp.py`. Add each new valid and invalid example to **both** test files.

## Known limits

- Analog sticks always stay centered, so the Saturn 3D pad's stick does nothing.
  Map `ABS_X`/`ABS_Y` in `build_report()` to add it.
- The adapter is fixed to `hci0`.
- Only the PS4 controller is imitated. Xbox One or Switch Pro would need a different report format,
  and Switch Pro also needs a longer handshake.
- Not yet confirmed against a real receiver. If you try it, please report what works.
