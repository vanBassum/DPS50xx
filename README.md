# DPS50xx

ESP32 firmware for monitoring and controlling DPS5020 programmable DC power supplies over
Modbus RTU, with a React web UI for live readouts, setpoint control and charting.

Built on [Strux](https://github.com/vanBassum/Strux) — WiFi, settings, OTA, a log console,
optional off-LAN access through a relay, and optional telemetry to InfluxDB all come from
the framework. This repo adds the supply.

## Supported boards

The board is selected at build time with `-DBOARD=<name>`; each one lives in
`main/hardware/boards/<name>/` and owns its own pins, LED polarity and chip target.

| Board | Chip | Modbus TX | Modbus RX | LED |
| --- | --- | --- | --- | --- |
| `dps50xx_esp32` (default) | ESP32 DevKit | GPIO 17 | GPIO 16 | GPIO 2, active high |
| `dps50xx_c3` | ESP32-C3 SuperMini | GPIO 21 | GPIO 20 | GPIO 8, active low |

Modbus runs at 9600 baud on UART1, to unit address 1. The status LED is lit while the
supply is answering polls.

## Flashing

Download the factory binary for your chip from the [Releases](../../releases) page.

**Option A — Browser (no install required):**
Use the [ESP Web Flasher](https://espressif.github.io/esptool-js/) and flash the factory
binary at address `0x0000`.

**Option B — Command line:**
```bash
esptool.py --chip esp32c3 write_flash 0x0000 DPS50xx-esp32c3-factory.bin
```

After the initial flash, updates can be done OTA from the web UI's firmware page.

> **Upgrading from a pre-Strux-port build (before v0.2):** flash by cable, not OTA. The
> partition table changed (`ota_1` moved, `www` resized) and an application OTA does not
> rewrite the partition table.

## Web UI

No login step unless you set one: authentication is off entirely while the `web.password`
setting is empty, which is the default.

- **Supply** — live voltage/current/power, input voltage, CV/CC mode, protection state,
  setpoint entry, output and key-lock toggles, and rolling charts.
- **Device** — firmware version, chip, heap, device time.
- **Console** — device log lines, streamed live.
- **Settings** — generated from whatever the firmware registered, including `psu.poll`
  (poll interval, ms) and `psu.telem` (record telemetry).
- **Firmware** — OTA by partition, and independent updates of the web UI.

## Controlling it without the UI

Every command is reachable over a WebSocket at `ws://<device>/ws`, and `help list`
enumerates them off the device itself:

```text
help list -category psu            # what psu commands exist and what arguments they take
psu get                            # the full state of the supply
psu set -voltage 12.5 -current 1.0 # any subset of the setpoints, in one command
psu set -output true
```

`psu set` leaves an omitted field alone and skips the Modbus write for anything that did
not change, so there is no reason to split a change across several calls.

## Building from source

Requires [ESP-IDF v6.0+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/)
and [Node.js 22+](https://nodejs.org/) with [pnpm](https://pnpm.io/). `idf.py build` also
builds the frontend when pnpm is on PATH.

```bash
idf.py set-target esp32                 # or esp32c3 for the C3 board
idf.py build                            # defaults to BOARD=dps50xx_esp32
idf.py -p COM11 flash monitor
```

For the C3 board:

```bash
idf.py -DBOARD=dps50xx_c3 set-target esp32c3
idf.py -DBOARD=dps50xx_c3 build
```

Frontend on its own (hot reload against a running device — set `DEV_HOST` in
`frontend/src/config.ts`):

```bash
cd frontend
pnpm dev
pnpm typecheck
```

## Adding a board

Copy a folder under `main/hardware/boards/`, adjust `BoardConfig.h`, and build with
`-DBOARD=<your folder>`. Only the selected board folder is on the include path. See
[CLAUDE.md](CLAUDE.md) for the layering the rest of the firmware follows.

## Remote access

`RelayManager` can dial out to a relay server so the device is reachable off-LAN; it is
off by default (`relay.enabled`). The relay server itself is not in this repo — it is one
server for many devices and it lives in
[Strux](https://github.com/vanBassum/Strux/tree/main/relay-server).

## License

This project is unlicensed. Use it however you want.
