# Next up

**Active work only.** Rewritten constantly, kept tiny, and an item is *removed* when it
lands or is dropped — never ticked off in place. Everything else lives in
`docs/backlog/` (work for later) or `docs/reasoning/` (why things are the way they are).
If a fact wants to survive, it does not belong in this file.

Last updated 2026-08-10.

## Now

**Three fixes want pushing back to Strux.**

- `WiFiInterface` logs the disconnect reason by name, and `NetworkManager` alternates
  station rounds with AP windows instead of ending in either — see
  `reasoning/2026-08-10-19h19`. The reason-code line is the one to upstream first: it
  costs nothing and it is what makes the retry policy decidable at all.
- `lib/protocol/` gained `ArgType::Float`. Additive, four lines across three files, and
  the reply side already had `value(float)` — see `reasoning/2026-08-06-20h47`. Until it
  is upstreamed, a naive copy of `lib/protocol/` from Strux silently breaks `psu set`.
- `backend.ts` sent `{"type":"writePartition"}` for the streaming upload envelope, but
  the dispatcher registers `partition write` and refuses anything without a space with
  "expected: `<category> <command>`". **Firmware upload from the web UI cannot have
  worked in Strux either.** The neighbouring `partition clear`/`activate` calls in the
  same function always had it right, which is what hid it.

**The AP→STA half of the WiFi cycle is unverified on hardware.** Three attempts then a
15-minute AP window is confirmed on the ESP32; the window *closing* and the round that
follows it are not — the device rebooted before its first window elapsed. Next real
outage longer than 30 s exercises it.

**`web.password` is empty, and the AP now recurs by design.** Every outage window puts an
unauthenticated console on an open network, which mattered less when the AP was a
one-way trip. Either set a default or make the recovery AP carry the web password.

**The DPS5020 has not answered since the Strux port was flashed.** Every poll times out
on TX17/RX16 @9600 — `ModbusRtu: Timeout waiting for response header (0/2 bytes)`. It
answered on this wiring on 2026-08-06, so the supply is dark or unwired rather than the
driver being wrong, but nothing about the product is verifiable until it is back.

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
