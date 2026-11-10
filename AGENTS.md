# Controlling the SEGA Saturn

A real Saturn is connected to this PC through a Bluetooth receiver. Use the `saturn` MCP server's tools:

- `status`: call first. `Connected.` means you can send. Anything else: tell the user what it says.
- `send_commands`: press buttons, e.g. `press START; wait 1000; press DOWN; press A`.
  Its description lists every command and button.

You cannot see the screen. Ask the user what it shows when the next step depends on it.

Button names match the Saturn buttons (checked on the real receiver: A=Cross, B=Circle, C=R2, X=Square,
Y=Triangle, Z=L2, L=L1, R=R1, START=Options). If a button does the wrong thing, tell the user.
