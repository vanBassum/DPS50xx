# Next up

**Active work only.** Rewritten constantly, kept tiny, and an item is *removed* when it
lands or is dropped — never ticked off in place. Everything else lives in
`docs/backlog/` (work for later) or `docs/reasoning/` (why things are the way they are).
If a fact wants to survive, it does not belong in this file.

Last updated 2026-09-09.

## Now

**The XY6020L runs on hardware, on the capability model, updated over the air.** Flashed to the
C3 at `E8:3D:C1:9C:1C:CC`, then twice updated by OTA with no cable attached. What the unit
confirmed, in `psu get`:

- `runtimeSeconds` 4731 (01:18:51) folded from three registers; those three appear nowhere.
- `externalTemperature` reports **unavailable** — the 8888 sentinel fired, and 888.8 °C is gone.
- `chargeAh` 0.67 Ah and `energyWh` 2.23 Wh, so the 32-bit low-word-first decode is right.
- `activePreset` = `M0` with ten options; `protectionState` = `None` with eleven.
- **The protection block answers.** LVP 6.0 V, OVP 62.0 V, OCP 22.0 A, OTP 95 °C — and OTP
  matching the vendor's documented default is what confirms that block's per-unit scales.
- `psu write` writes, and refuses a read-only capability, an out-of-range value (against the
  capability's own max) and an unknown key.

**Two scale questions left, one of them still needing a load.**

1. **The live power scale (0x0004).** Output was on at 4 V into nothing, so power read 0.00 W.
   The 0.1 W choice is still only the range argument in `reasoning/2026-09-09-18h52`. Put a load
   on and compare with the panel: 10× low means 0.01 W.
2. **OPP is now 1 W per count, inferred not measured.** At 0.1 W this unit read an odd 120 W; at
   1 W it reads 1200 W, exactly its rating, and the vendor's documented 950 W default would be an
   implausible 95 W at 0.1. The neighbouring OTP confirms the block uses natural units where the
   live block uses hundredths. One look at the panel's OPP setting settles it.

**`protOverCurrent` came back holding 22.0 A on a 20 A supply**, so the declared max is 25 A —
the rating is not the register's ceiling, and refusing to re-enter a value the panel already
accepted would repeat the `MAX_CURRENT` mistake. Same open question as the DPS's 20.1 A.

**`model` still reads 25858 and `version` 140.** Both answer every poll; neither is known to
mean what it is named. They are in the `info` group, so nothing displays them.

**The M0 caveat.** The protections are read from memory group M0's block (0x0052–0x005D). The
addresses are per-group, so this is correct only while the panel is on M0 — which is what
`activePreset` reports today. Reading the active group's block would mean addressing the poll off
another capability's value, which nothing supports.

**Preset recall and the composite limits are read-only on purpose.** Recalling a preset rewrites
every setpoint at once, and max charge / max energy / max runtime are 32-bit or split-register
values that would need a multi-register write `ModbusPsu` does not do. The UI shows them and does
not pretend to set them.

**Five fixes want pushing back to Strux.**

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
- `SystemManager::GetDeviceName()` falls back to the build's project name when the stored
  name is empty — and can never reach it, because the typed default of `device.name` is the
  literal `"Strux"`, so `Get()` never returns empty. Every fork therefore ships a device
  called Strux, in the browser tab title, the DHCP hostname and the relay's device list.
  The one-word fix is a `""` default; the fallback already there is the intended behaviour.
  Confirmed identical in upstream Strux.
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
