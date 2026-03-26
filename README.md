# DPS50xx

ESP32 firmware for monitoring and controlling DPS5020 programmable DC power supplies via Modbus RTU. Includes a web interface for live monitoring, MQTT integration, and OTA updates.

## Supported targets

| Target | Board | Modbus TX | Modbus RX |
|--------|-------|-----------|-----------|
| ESP32-C3 | SuperMini | GPIO 21 | GPIO 20 |
| ESP32 | — | GPIO 17 | GPIO 16 |

Pin assignments are defined in [`main/hardware/BoardConfig.h`](main/hardware/BoardConfig.h).

## Case

A 3D-printable case is available on [Thingiverse](https://www.thingiverse.com/thing:7191665).

## Flashing

Download the factory binary for your target from the [Releases](../../releases) page.

**Option A — Browser (no install required):**
Use the [ESP Web Flasher](https://espressif.github.io/esptool-js/) and flash the factory binary at address `0x0000`.

**Option B — Command line:**
```bash
esptool.py --chip esp32c3 write_flash 0x0000 DPS50xx-esp32c3-factory.bin
```

After the initial flash, subsequent updates can be done OTA via the web UI.

## Building from source

Requires [ESP-IDF v6.0+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) and [Node.js 22+](https://nodejs.org/) with [pnpm](https://pnpm.io/).

```bash
cd frontend && pnpm install && pnpm build && cd ..
idf.py set-target esp32c3
idf.py build
idf.py -p COM11 flash monitor
```

## OTA updates

From the web UI's firmware page:

- **Application firmware** — upload `DPS50xx-<target>-app.bin`
- **WWW partition** — upload `DPS50xx-<target>-www.bin` to update just the web interface

The device uses dual OTA partitions with automatic rollback.

## License

This project is unlicensed. Use it however you want.
