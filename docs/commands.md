# Commands

The same small command language is used by AI agents (through MCP) and by scripts (through the pipe).

## Language

Separate commands with `;` or new lines. Case does not matter. They run in order.

| Command | Effect |
|---|---|
| `press A` | press for 100 ms, release, then pause 50 ms |
| `press A 500` | press for 500 ms |
| `press A+B+C` | press several buttons at the same time |
| `hold RIGHT` | press and keep pressed |
| `hold A+RIGHT` | hold several |
| `release RIGHT` | let go |
| `release all` | let go of everything |
| `wait 1000` | pause 1000 ms before the next command |

Buttons: `A B C X Y Z L R START HOME UP DOWN LEFT RIGHT`

Keep presses at 50 ms or more: the Saturn reads the pad about 60 times per second.

### Examples

| Goal | Commands |
|---|---|
| Skip the intro | `press START` |
| Open a menu, wait for it, confirm | `press START; wait 1500; press A` |
| Walk right for 2 seconds | `hold RIGHT; wait 2000; release RIGHT` |
| Press C while running right | `hold RIGHT; wait 500; press C; wait 500; release RIGHT` |
| Soft reset (most games) | `press A+B+C+START 500` |

Comments are not allowed in commands: send only the command text.

### Button mapping

The receiver turns PS4 buttons into Saturn buttons. Its manual does not say how,
so the command names follow the most likely mapping:

| Command | PS4 button sent | Keyboard key |
|---|---|---|
| `A` / `B` / `C` | Cross / Circle / R1 | Z / X / C |
| `X` / `Y` / `Z` | Square / Triangle / L1 | A / S / D |
| `L` / `R` | L2 / R2 | Q / E |
| `START` | Options | Enter |
| `HOME` | PS | H |
| `UP` `DOWN` `LEFT` `RIGHT` | D-pad | arrows |

If a button does the wrong thing on your receiver, see [development.md](development.md#changing-the-button-mapping).

## From an AI agent (MCP)

`src/saturn_mcp.py` gives the agent two tools:

| Tool | Input | Result |
|---|---|---|
| `status` | none | `Connected.`, a disconnected message, or "saturn_bt is not running" |
| `send_commands` | `commands`: text in the language above | `Queued N command(s).` or an error |

`send_commands` checks every command before sending anything. One bad command means nothing is sent,
and the error lists the bad ones. It also refuses when the receiver is not connected.
It returns as soon as the commands are queued, not when they finish.

Setup is in the [README](../README.md#ai-agents-mcp).

## From a script (pipe)

Write commands to `/run/saturn_bt.fifo`. No root needed.

```sh
timeout 2 sh -c 'echo "press START; wait 1500; press A" > /run/saturn_bt.fifo'
```

- Always use `timeout`: if `saturn_bt` is not running, the write waits forever.
- Invalid commands are skipped and printed by `saturn_bt` as `bad command: ...`.
- Check the connection with `cat /run/saturn_bt.status`.
