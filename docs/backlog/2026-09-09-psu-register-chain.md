# The XY6020L, on the register chain

The chain landed on 2026-09-09 and so did the supply: `XY6020L` is a twenty-row table on
`ModbusPsu`, and `xy6020_c3` binds it. Nothing in `app/`, `strux/` or the frontend was touched to
make a second supply work — see
`../reasoning/2026-09-09-18h32-a-driver-that-declares-its-registers-is-the-schema.md`.

What is left is the half that needs hardware, plus the writes a generic table cannot carry.

## Owed by a bench

Most of the map answered on the first flash (see `next-up.md`); what is left needs a LOAD on the
output, which is a bench decision rather than a code one.

- [ ] **The 0.1 W power scale and the 32-bit word order.** Both readings are zero with the output
      off, so neither is verified. Power 10× low means 0.01 W; charge/energy jumping means the
      word order is high-first.
- [ ] **`memoryGroup` at 0x001D** answered every poll but was once late enough to desync the next
      one. It is also the reading that costs the poll a third transaction. Drop it if that
      transient becomes a pattern.
- [ ] **Negative temperatures.** 0x000D/0x000E are read as unsigned. If the supply encodes below
      zero as two's complement, a cold probe reads ~6553 °C. Needs a signed flag on the register
      or a driver-side fixup; nothing in the chain has needed signedness yet.
- [ ] **`xy6020_esp32`, if a second board is ever wired.** Deliberately not created: the only
      XY6020L wiring that exists is the C3's (GPIO3/4 at 115200), and inventing a board folder
      for hardware nobody has built is how a wrong pinout gets flashed.

## Owed by code

- [ ] **Writable extras as a typed `psu adv` command** — OVP/OCP/OPP/LVP/OTP thresholds, the
      capacity and run-time limits, memory-group recall. These need range validation and write
      ordering, which a table cannot carry, and recalling a preset moves every setpoint at once.
      Note `readArgs` is variadic with literal names, so a runtime-table-driven argument list has
      no `help list` at all.
- [ ] **The preset blocks at 0x0050+** (fourteen registers per group, ten groups) are not in the
      table and probably should not be polled. They are the `psu adv` command's business.

## Open

- [ ] **Build-time supply choice, or auto-detect.** Both drivers are compiled in now, so the
      board folder is the only thing selecting between them. A DPS5020 answers 0x000B with 5020
      and an XY6020L answers 0x0016 with its own model number, so one firmware could detect which
      is on the wire — at the cost of runtime branching, a detection failure mode, and the fact
      that the two supplies want DIFFERENT BAUD RATES (9600 vs 115200), which makes detection a
      two-speed probe rather than one read.
- [ ] **The repo name is now wrong.** `DPS50xx` describes one of two supported supplies. Rename,
      or accept it.
- [ ] **`psu.*` settings are shared across supplies.** Fine as they are; a per-supply setting
      would need a naming rule inside the 15-char NVS limit, and there is no room for one.
- [ ] **`psu get` is 20 readings now**, each with five or seven fields, polled every second by the
      browser. The reply is outbound and chunked so the 4096-byte inbound window does not apply —
      but measure it before assuming it is free.
- [ ] **`setCurrent` maxes at 20.0 A on both supplies and the DPS hardware accepts more** (a front
      panel set 20.1 A). The limit now lives on each driver's own row, so the decision is
      unchanged and merely moved: either the cap is the product's rating, or it follows what the
      register accepts.
