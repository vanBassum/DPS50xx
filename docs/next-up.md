# Next up

**Active work only.** Rewritten constantly, kept tiny, and an item is *removed* when it
lands or is dropped — never ticked off in place. Everything else lives in
`docs/backlog/` (work for later) or `docs/reasoning/` (why things are the way they are).
If a fact wants to survive, it does not belong in this file.

Last updated 2026-08-06.

## Now

**Two fixes want pushing back to Strux.**

- `lib/protocol/` gained `ArgType::Float`. Additive, four lines across three files, and
  the reply side already had `value(float)` — see `reasoning/2026-08-06-20h47`. Until it
  is upstreamed, a naive copy of `lib/protocol/` from Strux silently breaks `psu set`.
- `backend.ts` sent `{"type":"writePartition"}` for the streaming upload envelope, but
  the dispatcher registers `partition write` and refuses anything without a space with
  "expected: `<category> <command>`". **Firmware upload from the web UI cannot have
  worked in Strux either.** The neighbouring `partition clear`/`activate` calls in the
  same function always had it right, which is what hid it.

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
