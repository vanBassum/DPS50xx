---
id: 2026-08-26-21h20
date: 2026-08-26
time: "21:20"
title: "'AP started' is a function returning OK, not a beacon on the air"
builds-on: 2026-08-11-22h20
supersedes:
---

**Before:** the recovery AP was treated as working. `next-up.md` recorded "three
attempts then a 15-minute AP window is confirmed on the ESP32", and
[2026-08-11-22h20](2026-08-11-22h20-the-recovery-ap-is-an-address-with-nowhere-to-go.md)
reasoned in detail about what the AP window *buys* — the relay dialling into it,
the address it holds — all on the basis that the AP comes up. Configuring the
fallback network was described as something you do "over `Strux-AP` during a
recovery window", as if that were a path anyone had walked.

**What changed it:** the C3 spent an afternoon opening AP windows that were not
on the air. `Strux-AP` appeared in no scan, from two independent receivers —
a laptop 30 cm away and a phone. Meanwhile the device logged, every time:

```
I (15809) WiFiInterface: Starting AP 'Strux-AP'
I (15819) esp_netif_lwip: DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1
I (15819) WiFiInterface: AP started
```

Reading `StartAP` afterwards showed what that line actually asserts:
`esp_wifi_start()` returned `ESP_OK`. Nothing more. A driver readback confirmed
the configuration was faultless — right SSID, `ssid_hidden=0`, channel 1 inside
the country's 1–11, `beacon=100`, `txpower=80` (20 dBm) — and it still was not
beaconing, because the antenna was detuned by a wire (see
[2026-08-26-21h19](2026-08-26-21h19-a-wire-near-the-antenna-is-indistinguishable-from-a-firmware-bug.md)).

**Now:** **nobody has ever observed `Strux-AP` on the air on the ESP32 either.**
The evidence for "the AP window works" was always this same log line, plus the
relay dialling out — and the relay dialling out only proves the netif holds an
address, which is exactly what the earlier note showed is not connectivity. The
premise was never supported; it was a self-report mistaken for a measurement.

The general shape: a log line written *after* a call is evidence the call
returned, and nothing else. For anything that leaves the device — a beacon, a
frame, a packet — only a second party can confirm it happened. `next-up.md`
already said "the AP→STA half of the WiFi cycle is unverified on hardware"; the
unverified half was larger than that sentence implied, because the *AP itself*
was never verified, only its start-up call.

**Rests on:** the two receivers used being representative. Both are ordinary
2.4 GHz clients that saw eleven other networks including ones at 0–3% signal, so
a beacon from 30 cm should not have been missed.

**Follows:** the AP path needs a verification that is not the device's own word —
a scan from another device, or a client actually associating and loading the page.
Until then "recovery AP" is a design, not a feature, on either board.
