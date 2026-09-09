# Next up

**Active work only.** Rewritten constantly, kept tiny, and an item is *removed* when it
lands or is dropped — never ticked off in place. Everything else lives in
`docs/backlog/` (work for later) or `docs/reasoning/` (why things are the way they are).
If a fact wants to survive, it does not belong in this file.

Last updated 2026-09-09.

## Now

**The XY6020L runs on hardware.** Flashed to the C3 at `E8:3D:C1:9C:1C:CC` (COM5) on
2026-09-09 and driven over its own WebSocket. What the unit confirmed:

- `XY6020L: online (addr=1)` 2.6 s after boot; 30 s of steady polling with **zero** Modbus
  warnings. TX 3 / RX 4 at 115200 is right, so the wiring reading was right too.
- Scales confirmed by plausibility: input 18.75 V, temperature **22.5 °C** (room), setpoints
  4.00 V / 10.00 A off the front panel. All 0.01 except temperature's 0.1.
- `psu set` writes, echoes and refuses correctly against the DRIVER's own limits:
  `"Set Voltage out of range (0-60 V)"` at 61 V, and `"this supply has no 'backlight'"` — from
  the same handler that accepts backlight on a DPS5020.
- Protection reads `None` with its label; all twenty readings arrive in `psu get`.

**Three things the bench has NOT settled, and two need a load on the output.**

1. **The 0.1 W power scale.** Output was off, so power read 0.00 W and the range argument in
   `reasoning/2026-09-09-18h52` is still only an argument. Put a load on and compare against the
   panel: 10× low means the scale is 0.01 W.
2. **The 32-bit word order** on charge and energy. Both accumulators read 0.000, which is what
   either word order gives for zero. They must be nonzero to mean anything.
3. **`model` reads 25858** (0x6502), which is not obviously a product number, and `version`
   reads 140. Register 0x0016/0x0017 answer *something* every poll; whether that something is
   the model is unconfirmed, and it is display-only either way.

**One transient worth knowing, not yet a bug.** At 23 s of the first boot, one poll logged
`TX echo detected: 7 bytes in RX after send` and then timed out reading `0x001D`. Seven bytes is
exactly one single-register reply, so that is a LATE answer found in the buffer by the next
send — the supply being slow once, the same shape as `reasoning/2026-08-11-22h33`. It has not
recurred in 30 s of steady state and the device never went offline. If it becomes frequent,
drop `memoryGroup` from the table: it is the only reading past the main block and it costs the
poll its third transaction.

**`dps50xx_c3` has the console fix and has not been flashed with it.** Same overlay change, and
`reasoning/2026-09-09-19h34` predicts that bench's own WiFi history was partly this. The ESP32
board was never affected — it has a real UART bridge, so its console was always UART-primary.

**An EXISTING sdkconfig keeps the old console setting, so the guard fires until it is
regenerated.** `sdkconfig.defaults` only seeds a *fresh* config, so the first build after this
change fails with the `#error` rather than silently keeping USB primary — which is the guard
working, but the message tells you to change a Kconfig option and not what actually needs doing.
Delete the stale `sdkconfig` (it is gitignored), or build per board the way CLAUDE.md documents:
`-DSDKCONFIG=sdkconfig-c3`. The root `sdkconfig` in this tree is still a stale esp32c3 one and
will fail this way.

**`ConsoleManager.cpp` now holds upstream's guard but not upstream's later log-flood fix**
(Strux `7449f81`), because only the connectivity fix was wanted. A full `cp -r ../Strux/main/strux`
supersedes both, and that is the intended way out — this is not fork debt.

**Four fixes want pushing back to Strux.**

- `WiFiInterface` logs the disconnect reason by name, and `NetworkManager` alternates
  station rounds with AP windows instead of ending in either — see
  `reasoning/2026-08-10-19h19`. The reason-code line is the one to upstream first: it
  costs nothing and it is what makes the retry policy decidable at all. The reason now
  travels in `NetworkEvent` with the beacon RSSI, which is what lets a failed attempt be
  retried in 2 s instead of waiting out a 10 s timeout for news that arrived at 1 s, and
  what tells this manager's own teardown apart from a network refusing — see
  `reasoning/2026-08-12-13h25`. Same commit, same upstream trip.
- `NetworkManager::HasIpv4()` became `HasUpstream()`. The AP netif is always
  192.168.4.1, so the relay's "wait for an address" guard said *go ahead* for the whole
  of every recovery AP window — see `reasoning/2026-08-11-22h20`. Travels with the AP
  window it was broken by, so upstream the two together.
- `lib/protocol/` gained `ArgType::Float`. Additive, four lines across three files, and
  the reply side already had `value(float)` — see `reasoning/2026-08-06-20h47`. Until it
  is upstreamed, a naive copy of `lib/protocol/` from Strux silently breaks `psu set`.
- `backend.ts` sent `{"type":"writePartition"}` for the streaming upload envelope, but
  the dispatcher registers `partition write` and refuses anything without a space with
  "expected: `<category> <command>`". **Firmware upload from the web UI cannot have
  worked in Strux either.** The neighbouring `partition clear`/`activate` calls in the
  same function always had it right, which is what hid it.

**The AP→STA half of the WiFi cycle is unverified on hardware.** Three attempts then a
15-minute AP window is confirmed on the ESP32 — a round now takes 15 s rather than 30,
since a failed attempt is retried on the failure instead of on the timeout. The window
*closing* and the round that follows it are still unverified: the device has never been
left alone for the full fifteen minutes.

**A second WiFi network can be configured, and has never been tried with two.**
`wifi.ssid2`/`wifi.password2` are registered and the round rotates between the
configured networks, three attempts each. With one configured the code takes the same
path it always did, which is what the last flash verified; the rotation itself, and
`SwitchSta` changing networks on a running station, have only ever run as a one-network
degenerate case. Configuring the fallback needs a device you can reach — the settings UI
over `Strux-AP` during a recovery window.

**`vanBassumExt` is out of range of the bench, not wedged.** Two attempts in three
cannot see it at all (reason 201) and the third hears it at −83 dBm — see
`reasoning/2026-08-12-14h05`, which revises the wedged-extender inference in
`2026-08-10-19h19`. Point `wifi.ssid` at the main router or move the device; there is no
firmware fix, and every WiFi log from this bench will keep showing recovery AP windows
until one of those happens.

**`web.password` is empty, and the AP now recurs by design.** Every outage window puts an
unauthenticated console on an open network, which mattered less when the AP was a
one-way trip. Either set a default or make the recovery AP carry the web password.

**The DPS5020 is intermittent, and the bench is where to look.** It alternates between
answering completely and being entirely absent for minutes — a stray
`RX buffer had 31 stale bytes` is exactly one whole 13-register response, and the
instrumented driver then measured 48 consecutive polls receiving literally zero bytes.
Baud, wiring, address and register map are therefore all right; see
`reasoning/2026-08-11-22h33`. Suspect the supply or a connector, not the firmware.
Nothing about the product is verifiable until it stays up.

**The C3 board has never been flashed.** `dps50xx_c3` compiles (1.24 MB, 21% free) and
nothing in it is board-specific beyond `BoardConfig.h`, but the pins are unverified
against real hardware.

**`MAX_CURRENT` is 20.0 A and the hardware accepts more.** The test unit had 20.1 A set
from its front panel, so the firmware now refuses to reproduce a setpoint the panel
allows. Either the cap is the product's rating and the asymmetry is intended, or it
should follow what the register accepts. Not decided.

**The `www` partition shrank to 0xE0000 (917 KB)** and the current bundle is 253 KB
gzipped. Plenty, but `recharts` is most of the 816 KB uncompressed JS — worth knowing
before adding another charting dependency.
