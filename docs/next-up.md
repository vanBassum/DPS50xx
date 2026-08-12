# Next up

**Active work only.** Rewritten constantly, kept tiny, and an item is *removed* when it
lands or is dropped — never ticked off in place. Everything else lives in
`docs/backlog/` (work for later) or `docs/reasoning/` (why things are the way they are).
If a fact wants to survive, it does not belong in this file.

Last updated 2026-08-11.

## Now

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
