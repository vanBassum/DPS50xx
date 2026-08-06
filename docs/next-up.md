# Next up

**Active work only.** Rewritten constantly, kept tiny, and an item is *removed* when it
lands or is dropped — never ticked off in place. Everything else lives in
`docs/backlog/` (work for later) or `docs/reasoning/` (why things are the way they are).
If a fact wants to survive, it does not belong in this file.

Last updated 2026-08-06.

## Now

**The Strux port has not touched hardware yet.** Both boards build clean
(`dps50xx_esp32`, `dps50xx_c3`) and the frontend typechecks, but nothing has been
flashed. Until it has, "works" means "compiles". What to check first, in order:

1. `psu get` over the WebSocket — does the supply answer on the wire at all, on the
   pins the board folder declares.
2. `psu set -voltage 5 -current 1` — one command carrying two setpoints, which is the
   part that is new rather than ported.
3. `help list -category psu` — the arguments come off the handler's own `readArgs`, so
   this is also the test that `ArgType::Float` describes itself correctly.

**Flash by serial cable, not OTA.** The partition table came from Strux and moved
`ota_1` (0x190000 → 0x1A0000) and shrank `www`. An application OTA does not rewrite the
partition table, so a device still running the pre-port firmware cannot reach this
build over the air.

**`ArgType::Float` wants to go back to Strux.** It is additive, it is four lines across
three files in `lib/protocol/`, and the reply side already had floats — see
`reasoning/2026-08-06-20h47`. Until it is upstreamed it is a fork edit inside the one
directory this port otherwise left alone.
