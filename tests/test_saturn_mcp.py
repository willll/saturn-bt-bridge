# Run: python3 tests/test_saturn_mcp.py   (works without saturn_bt running)
import json
import os
import subprocess
import sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
sys.path.insert(0, SRC)
import saturn_mcp as m  # noqa: E402

assert m.COMMAND.fullmatch("press a+b 200") and m.COMMAND.fullmatch("release all") and m.COMMAND.fullmatch("wait 10")
assert not m.COMMAND.fullmatch("press Q") and not m.COMMAND.fullmatch("hold A 5") and not m.COMMAND.fullmatch("wait x")
assert not m.COMMAND.fullmatch("press A+") and not m.COMMAND.fullmatch("press +A")
assert m.send("press Q")[1] and "Invalid" in m.send("press Q")[0]

reqs = [
    {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-06-18"}},
    {"jsonrpc": "2.0", "method": "notifications/initialized"},
    {"jsonrpc": "2.0", "id": 2, "method": "tools/list"},
    {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "status", "arguments": {}}},
    {"jsonrpc": "2.0", "id": 4, "method": "nope"},
]
out = subprocess.run([sys.executable, os.path.join(SRC, "saturn_mcp.py")], input="\n".join(map(json.dumps, reqs)) + "\n",
                     capture_output=True, text=True, timeout=10).stdout
res = [json.loads(line) for line in out.splitlines()]
assert [r["id"] for r in res] == [1, 2, 3, 4], res  # no reply to the notification
assert res[0]["result"]["capabilities"] == {"tools": {}}
assert {t["name"] for t in res[1]["result"]["tools"]} == {"send_commands", "status"}
assert res[2]["result"]["content"][0]["text"]
assert res[3]["error"]["code"] == -32601
print("ok")
