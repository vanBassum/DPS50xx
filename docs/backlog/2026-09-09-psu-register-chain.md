# The XY6020L, on the register chain

The chain landed on 2026-09-09 and so did the supply: `XY6020L` is a twenty-row table on
`ModbusPsu`, and `xy6020_c3` binds it. Nothing in `app/`, `strux/` or the frontend was touched to
make a second supply work — see
`../reasoning/2026-09-09-18h32-a-driver-that-declares-its-registers-is-the-schema.md`.

What is left is the half that needs hardware, plus the writes a generic table cannot carry.

## Owed by a bench

Most of the map is verified (see `next-up.md`). What is left needs a LOAD on the output, which is
a bench decision rather than a code one.

- [ ] **The live 0.1 W power scale**, unverifiable with the output into nothing.
- [ ] **OPP's 1 W scale**, inferred from the value this unit holds and the vendor's default. One
      look at the panel confirms or kills it.
- [ ] **What `model` (25858) and `version` (140) actually mean.**
- [ ] **Negative temperatures.** The temperature registers are read unsigned. If the supply
      encodes below zero as two's complement, a cold probe reads about 6553 °C. Nothing in the
      capability model needs signedness yet, so nothing has it.

## Owed by code

- [ ] **Multi-register writes.** `protMaxCharge`, `protMaxEnergy` and `protMaxRuntime` are
      read-only because each spans two registers and `ModbusPsu::Write` does one. A capability
      that writes through several registers is the same normalization seen from the other side,
      and the driver is where it belongs.
- [ ] **Preset recall.** Writing `activePreset` rewrites every setpoint at once, so it wants its
      own command with its own confirmation, not a generic `psu write`.
- [ ] **Protections from the ACTIVE group**, not M0's, which needs an address computed from
      another capability's value — a driver-level indirection nothing supports today.
- [ ] **Device-level settings behind a gear UI**: Modbus address, baud, display brightness and
      sleep, buzzer, temperature unit and calibration. These are `info`/config, they belong
      nowhere near the supply dashboard, and several of them can lock you out of the device if
      set wrongly (baud, slave address). Not worth building until something needs them, and worth
      guarding when it does.
- [ ] **The offline log flood.** With no supply attached, the essential block fails every poll and
      logs two lines each time — 51 of 145 lines in one 40 s capture. The driver already knows it
      is offline after three failures; it should go quiet until the supply returns. These lines
      also broadcast to every WebSocket client and up the relay pipe.

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
- [ ] **`psu get` is 27 capabilities now**, several carrying an options table, polled every second
      by the browser. The reply is outbound and chunked so the 4096-byte inbound window does not
      apply — but measure it before assuming it is free.
- [ ] **`setCurrent` maxes at 20.0 A on both supplies and the DPS hardware accepts more** (a front
      panel set 20.1 A). The limit now lives on each driver's own row, so the decision is
      unchanged and merely moved: either the cap is the product's rating, or it follows what the
      register accepts.
