# Thermy

A smart temperature monitor built on the ESP32 with a touchscreen display and web interface. Track up to 4 DS18B20 temperature sensors in real time, view historical trends, and manage everything from the device itself or any browser on your network.

<img width="1096" height="591" alt="image" src="https://github.com/user-attachments/assets/cc282e06-f84b-497d-8e5a-3e04add95bac" />

## Features

- **4-Channel Temperature Monitoring** — Connect up to 4 DS18B20 sensors via OneWire. Each sensor is color-coded (red, blue, green, yellow) for easy identification.
- **Touchscreen Display** — 480x320 capacitive touchscreen showing live readings and a scrolling temperature graph. Configure everything directly on the device.
- **Web Dashboard** — Access a full-featured web UI from any browser on your network. View sensor cards, interactive charts, device info, and live logs.
- **Historical Graphs** — Stores up to 8192 samples per sensor in memory. At the default 10-second sample rate that's roughly 22 hours of history. Adjust the rate to store more.
- **Auto Sensor Discovery** — New sensors are detected automatically. A popup lets you assign each sensor to one of the 4 slots.
- **WiFi with AP Fallback** — Connects to your home network. If it can't connect, it creates its own access point (`Thermy-AP`) so you're never locked out.
- **Over-the-Air Updates** — After the initial USB flash, update firmware and the web UI wirelessly from the browser.
- **NTP Time Sync** — Automatic clock sync with configurable timezone support.

## Hardware

| Component | Details |
|-----------|---------|
| Board | WT32-SC01 (ESP32 with 480x320 touchscreen) |
| Sensors | DS18B20 digital temperature sensors (1-4) on GPIO 4 |
| Flash | 16 MB (firmware + web UI + settings) |
| Connectivity | WiFi 802.11 b/g/n |

Wire your DS18B20 sensors to GPIO 4 with a 4.7k pull-up resistor. The device handles the rest.

## Getting Started

### Initial Flash

You need a USB connection for the first flash only. After that, updates go over WiFi.

**Option A — Pre-built release:**
Download `Thermy-factory.bin` from the [Releases](../../releases) page and flash it:
```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x0 Thermy-factory.bin
```

**Option B — Build from source:**
Requires [ESP-IDF v6.0+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) and [Node.js 22+](https://nodejs.org/) with [pnpm](https://pnpm.io/).
```bash
cd frontend && pnpm install && pnpm build && cd ..
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### First Boot

1. Thermy starts in access point mode, broadcasting **Thermy-AP** (open network)
2. Connect to Thermy-AP from your phone or computer
3. Open the web UI at the device's IP address
4. Go to **Settings**, enter your WiFi credentials, and save
5. Thermy reboots and connects to your network

## Using the Touchscreen

The home screen shows the current time, IP address, live temperature readings for all 4 slots, and a scrolling line chart.

Tap the **gear icon** to access settings:

- **WiFi** — Scan for networks, select one, enter the password
- **Sensors & Timing** — Adjust how often the bus is scanned and temperatures are read. Clear sensor assignments to start fresh.
- **Graph** — Set the sample rate and Y-axis range. The display shows how long the current settings will store.
- **System** — Change the device name, NTP server, and timezone

All settings changes require tapping **Save & Reboot** to take effect.

### Assigning Sensors

When a new DS18B20 sensor is detected on the bus, a popup appears on the touchscreen. Tap one of the 4 colored slots to assign it. The assignment is saved and persists across reboots.

## Using the Web Interface

Once connected to WiFi, open Thermy's IP address in any browser. The web UI provides:

| Page | What it does |
|------|-------------|
| **Dashboard** | Firmware version, chip info, memory usage |
| **Temperature** | Live sensor cards and an interactive historical chart |
| **Console** | Real-time device log stream |
| **Settings** | View and edit all device settings |
| **Firmware** | Upload new firmware or web UI updates over the air |

## Updating Over the Air

After the initial USB flash, you never need a cable again. From the web UI's **Firmware** page:

- **Application Firmware** — Upload `Thermy-app.bin` to update the device firmware
- **WWW Partition** — Upload `Thermy-www.bin` to update just the web interface

The device uses dual partitions for safe updates — if something goes wrong, the previous firmware is still available.

## Settings Reference

| Setting | Default | Description |
|---------|---------|-------------|
| Device name | Thermy | Name shown in the UI |
| WiFi SSID / Password | — | Your WiFi network credentials |
| NTP server | pool.ntp.org | Time synchronization server |
| Timezone | CET-1CEST,M3.5.0,M10.5.0/3 | POSIX timezone string |
| Sample rate | 10 seconds | How often a data point is stored to the history graph |
| Graph Y-axis min/max | 0 / 100 | Temperature range for the graph display |
| Bus scan interval | 5000 ms | How often to scan for new sensors |
| Temperature read interval | 1000 ms | How often to read sensor values |

## License

This project is unlicensed. Use it however you want.
