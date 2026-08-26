---
id: 2026-08-26-21h19
date: 2026-08-26
time: "21:19"
title: A wire near the antenna is indistinguishable from a firmware bug
builds-on:
supersedes:
---

**Before:** the `dps50xx_c3` board was wired to the supply on GPIO21/20 — the pins
`BoardConfig.h` had always named — and the station would not hold a WiFi
association. It associated, never obtained an address, and the web UI was
unreachable, on every boot. Everything about the failure pointed inward: the
device's own logs were clean, every manager initialised, Modbus answered, and the
only thing misbehaving was the network stack. The natural reading was a firmware
fault, and a long list of plausible ones was worked through — a console/UART pin
collision, stale RF calibration in NVS, the ESP-IDF version, AP misconfiguration,
40 MHz bandwidth on a 20 MHz radio, modem sleep, CPU contention from the Modbus
poll task.

**What changed it:** removing the firmware from the experiment entirely. Not
disabling the poll interval — disabling everything: `pollTask_.Run()` commented
out so no Modbus transaction ever happened, then `rtu_.Init()` skipped so the UART
was never configured at all and GPIO0/1 stayed in their power-on high-impedance
state. Nothing in the build knew those wires existed. **The fault reproduced
unchanged.** Then the same wires, the same supply, the same firmware, moved to
GPIO6/5:

```
I (2809) WiFiInterface: STA connected to AP
I (2819) DPS5020: DPS5020 online (addr=1)
I (3939) WiFiInterface: Got IP: 192.168.11.15
```

and `index.html` served in 63 ms, against 21 s of failure before.

**Now:** the C3 SuperMini places its ceramic antenna without the keep-out
clearance its datasheet requires, so nearby conductors detune it — a documented
flaw of this board that names GPIO20/21 by number (esp32.com/viewtopic.php?t=41895,
and the Hackaday writeup of the 31 mm quarter-wave mod). The pin assignment in a
board config is therefore an **antenna decision, not a UART decision**, and on
this hardware it is the one that decides whether the product works at all.

Two corollaries that cost real time to learn. A passive wire detunes exactly as
well as a driven one, so "the pin is high-impedance" is not isolation — which is
also why 2k2 in series changed nothing: it is a DC barrier, and at 2.4 GHz the
wire *is* the antenna, resistor or not. And a physical RF fault presents with a
perfectly healthy log: every subsystem reports success, because every subsystem
except the radio genuinely is succeeding.

**Rests on:** the boards being ordinary SuperMini clones with the reported layout.
A module with the antenna correctly placed, or the SuperMini Plus with its u.FL
connector, would not behave this way.

**Follows:** `MODBUS_TX_PIN`/`MODBUS_RX_PIN` are 6/5, with the reasoning in the
header so the next person moving them knows what the constraint actually is. The
first C3 module, written off as having a damaged transmit path, is probably not
faulty at all — it was wired to 21/20 throughout — and should be retested before
being scrapped.
