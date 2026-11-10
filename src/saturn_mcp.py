#!/usr/bin/env python3
"""MCP server (stdio) that lets an AI agent press buttons on a SEGA Saturn through saturn_bt.

saturn_bt must already be running as root; this server only writes to its command pipe.
Standard library only.
"""
import errno
import json
import os
import re
import sys

FIFO = "/run/saturn_bt.fifo"
STATUS = "/run/saturn_bt.status"

# Must stay in sync with parse() in bridge.hpp.
BTN = r"(A|B|C|X|Y|Z|L|R|START|HOME|UP|DOWN|LEFT|RIGHT)"
BTNS = rf"{BTN}(\+{BTN})*"
COMMAND = re.compile(rf"press\s+{BTNS}(\s+\d+)?|hold\s+{BTNS}|release\s+(ALL|{BTNS})|wait\s+\d+", re.I)

GRAMMAR = (
    "Commands run in order, separated by ';' or newlines, case-insensitive: "
    "'press A' (tap, 100 ms), 'press A 500' (hold 500 ms then release), 'press A+B' (together), "
    "'hold RIGHT', 'release RIGHT' or 'release all', 'wait 1000' (pause in ms). "
    "Buttons: A B C X Y Z L R START HOME UP DOWN LEFT RIGHT. "
    "The Saturn reads the pad ~60 times/s, so keep presses >= 50 ms. "
    "You cannot see the screen: ask the user what it shows when it matters."
)

TOOLS = [
    {
        "name": "send_commands",
        "description": "Press buttons on the SEGA Saturn controller. Returns as soon as the commands are queued. " + GRAMMAR,
        "inputSchema": {
            "type": "object",
            "properties": {"commands": {"type": "string", "description": "e.g. 'press START; wait 1000; press A'"}},
            "required": ["commands"],
        },
    },
    {
        "name": "status",
        "description": "Tells whether saturn_bt is running and the Saturn Bluetooth receiver is connected.",
        "inputSchema": {"type": "object", "properties": {}},
    },
]


def status():
    try:
        # Non-blocking open of a pipe with no reader fails with ENXIO, so this never hangs.
        os.close(os.open(FIFO, os.O_WRONLY | os.O_NONBLOCK))
    except OSError as e:
        if e.errno in (errno.ENOENT, errno.ENXIO):
            return "saturn_bt is not running. Ask the user to start it or its service (see README.md)."
        raise
    try:
        with open(STATUS) as f:
            return f.read().strip()
    except OSError:
        return "saturn_bt is running, connection status unknown."


def send(commands):
    cmds = [c.strip() for c in re.split(r"[;\n]", commands) if c.strip()]
    if not cmds:
        return "No commands given. " + GRAMMAR, True
    bad = [c for c in cmds if not COMMAND.fullmatch(c)]
    if bad:
        return f"Invalid command(s): {bad}. Nothing was sent. " + GRAMMAR, True
    data = ("\n".join(cmds) + "\n").encode()
    if len(data) > 4096:  # one pipe write is only atomic up to PIPE_BUF
        return "Too long: send at most 4 KB of commands per call.", True
    st = status()
    if st != "Connected.":
        return "Not sent. " + st, True
    try:
        fd = os.open(FIFO, os.O_WRONLY | os.O_NONBLOCK)
        try:
            os.write(fd, data)
        finally:
            os.close(fd)
    except OSError as e:
        return f"Not sent: {e}", True
    return f"Queued {len(cmds)} command(s).", False


def reply(rid, result=None, error=None):
    msg = {"jsonrpc": "2.0", "id": rid}
    if error:
        msg["error"] = error
    else:
        msg["result"] = result
    print(json.dumps(msg), flush=True)


def main():
    for line in sys.stdin:
        if not line.strip():
            continue
        try:
            req = json.loads(line)
            if not isinstance(req, dict):
                raise ValueError
        except ValueError:
            reply(None, error={"code": -32700, "message": "Parse error"})
            continue
        method, rid, params = req.get("method"), req.get("id"), req.get("params") or {}
        if rid is None:
            continue  # notifications need no reply
        if method == "initialize":
            reply(rid, {
                "protocolVersion": params.get("protocolVersion", "2025-06-18"),
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "saturn", "version": "1.0.0"},
            })
        elif method == "ping":
            reply(rid, {})
        elif method == "tools/list":
            reply(rid, {"tools": TOOLS})
        elif method == "tools/call":
            name, args = params.get("name"), params.get("arguments") or {}
            if name == "status":
                text, is_error = status(), False
            elif name == "send_commands":
                text, is_error = send(str(args.get("commands", "")))
            else:
                reply(rid, error={"code": -32602, "message": f"Unknown tool: {name}"})
                continue
            reply(rid, {"content": [{"type": "text", "text": text}], "isError": is_error})
        else:
            reply(rid, error={"code": -32601, "message": f"Method not found: {method}"})


if __name__ == "__main__":
    main()
