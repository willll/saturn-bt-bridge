#!/bin/sh
# Presses every Saturn button in turn through saturn_bt, printing each one.
# Usage: ./cycle_buttons.sh [hold_ms] [rounds]    defaults: 500 ms, 1 round; rounds 0 = until Ctrl+C
# HOME is left out: what the receiver does with it is not known.
HOLD=${1:-500}
ROUNDS=${2:-1}
FIFO=/run/saturn_bt.fifo
STATUS=/run/saturn_bt.status

case "$HOLD$ROUNDS" in *[!0-9]*) echo "usage: $0 [hold_ms] [rounds]" >&2; exit 2 ;; esac

round=0
while [ "$ROUNDS" -eq 0 ] || [ "$round" -lt "$ROUNDS" ]; do
  round=$((round + 1))
  for b in A B C X Y Z L R START UP DOWN LEFT RIGHT; do
    if [ "$(cat "$STATUS" 2>/dev/null)" != "Connected." ]; then
      echo "saturn_bt is not connected: $(cat "$STATUS" 2>/dev/null || echo 'not running')" >&2
      exit 1
    fi
    echo "round $round: $b"
    # timeout: writing to the pipe blocks forever if saturn_bt is not running
    timeout 2 sh -c 'echo "$1" > "$2"' _ "press $b $HOLD" "$FIFO" || { echo "could not write to $FIFO" >&2; exit 1; }
    sleep "$(awk "BEGIN { print ($HOLD + 300) / 1000 }")"  # hold time + a short gap, so buttons do not overlap
  done
done
