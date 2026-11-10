# Pad monitor (SEGA Saturn)

A Saturn program that shows the buttons held on the pad in port 1, and sends every press and release
to the USB DevCart. On the PC, [ftx](https://github.com/willll/ftx) prints the messages.

Use it to check what the Bluetooth bridge really sends: an agent runs `press A`, and the console
should print `PRESSED A`. If it prints another button, fix the mapping
([docs/development.md](../../docs/development.md#changing-the-button-mapping)).

Built with [SaturnRingLib](https://github.com/ReyeMe/SaturnRingLib); based on its "Input - Gamepad" sample.

## Build

You need SaturnRingLib and its SH-2 compiler.

```sh
make SRL_INSTALL_ROOT=/path/to/SaturnRingLib      # compiler (sh2eb-elf-* or sh-elf-*) on PATH
```

Or with the `willll/saturn-docker` image, which has the compiler:

```sh
docker run --rm --entrypoint "" -u $(id -u):$(id -g) \
  -v /path/to/SaturnRingLib:/srl -v "$PWD":/sample -w /sample willll/saturn-docker:gcc_14.3.0 \
  bash -c 'PATH=/opt/saturn/toolchain/bin:$PATH make all SRL_INSTALL_ROOT=/srl'
```

The program is `cd/data/0.bin`; the disc image is in `BuildDrop/`.

## Run on a Saturn with the USB DevCart

From this folder:

```sh
usbreset "FT245R USB FIFO"                 # reset the DevCart's USB chip
ftx -x cd/data/0.bin 0x06004000            # upload and start the program
ftx -c | tee /tmp/saturn_pad.log           # print the messages, and keep them in a file
```

SaturnRingLib's `tools/scripts/run.sh USBGamers` does the same, without the log file.

Output:

```text
INFO : PAD MONITOR STARTED
INFO : PAD CONNECTED
INFO : PRESSED START
INFO : RELEASED START
INFO : PRESSED A
INFO : PRESSED RIGHT
INFO : RELEASED A
INFO : RELEASED RIGHT
```

Button names are the same as the bridge commands: `A B C X Y Z L R START UP DOWN LEFT RIGHT`.
`PAD CONNECTED` / `PAD DISCONNECTED` also show when the Bluetooth receiver links up or drops.

Keep `ftx -c` running while the sample runs: if nothing reads the DevCart, its USB buffer can fill up
and the sample pauses until it is read.

## Run in an emulator

```sh
mednafen BuildDrop/PadMonitor.cue
```

The screen works the same. Without a DevCart the sample shows `DevCart: not found` and sends nothing.

## Credits

The graphics in `cd/data` come from SaturnRingLib (MIT License,
Copyright (c) 2026 ReyeMe, robertoduarte, 7shades, willll, nemesis-saturn, jae686).
