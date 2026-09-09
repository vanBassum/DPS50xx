# The XY6020L, on the register chain

The chain landed on 2026-09-09: a driver declares self-describing readings and everything above
it walks them — see
`../reasoning/2026-09-09-18h32-a-driver-that-declares-its-registers-is-the-schema.md` for why,
and `interfaces/Psu.h` for the mechanism. `DPS5020` is now a thirteen-row table on top of
`ModbusPsu`, which is what a second supply is supposed to be too.

This is what the second supply still owes.

## The XY6020L

- [ ] **The register map must come from the datasheet, not from memory.** Addresses, scales,
      which registers are 32-bit pairs, and the protection vocabulary all need checking against
      a real unit before anything is written down. This is the one part with no code answer.
- [ ] `drivers/XY6020L.{h,cpp}` — the table, this supply's limits (60 V where the DPS is 50),
      and its own protection labels. `ReadNumber`'s `words = 2` covers the Ah/Wh accumulators;
      it is implemented, high word first, and **unexercised** — that assumption is the first
      thing to check on hardware.
- [ ] Board folders. A board folder already means MCU + wiring + which drivers get bound, so
      `xy6020_esp32` / `xy6020_c3` uses the existing mechanism. If the 2×2 gets annoying, a
      second cache var (`-DPSU=`) is a later refactor — do not build it yet. Remember `-DBOARD`
      goes on `set-target` too, and `-B build-xy -DSDKCONFIG=sdkconfig-xy` to keep the
      configured trees apart.
- [ ] Writable extras (OVP/OCP/OPP thresholds, memory-group recall, the output timer) as a
      typed `psu adv` command, not through the generic path: they need range validation and
      write ordering, which a table cannot carry. Note `readArgs` is variadic with literal
      names, so a runtime-table-driven argument list has no `help list` at all.

## Open

- [ ] **Build-time supply choice, or auto-detect.** With both drivers compiled in, the supply
      could be detected at boot — a DPS5020 answers `0x000B` with `5020` — giving one firmware
      for both. Attractive, but it adds runtime branching and a detection failure mode. Default
      to the build-time choice unless one binary is actually wanted.
- [ ] **The repo name goes stale.** `DPS50xx` stops describing the product once a second supply
      is in it. Rename, or accept it. Not urgent, and cheap either way.
- [ ] **`psu.*` settings are shared across supplies.** `psu.poll` and `psu.telem` are fine as
      they are; a per-supply setting would need a naming rule that fits inside the 15-char NVS
      limit, and there is no room for one. Avoid needing it.
- [ ] **How many readings is too many for a 1 Hz poll.** The XY's map is bigger than the DPS's
      thirteen rows, and `psu get` now carries five fields per reading rather than one value.
      The reply is outbound and chunked, so the 4096-byte inbound window does not apply, but the
      browser polls every second — measure before assuming it is free. A gap in the map also
      costs an extra Modbus transaction per block, which the DPS never paid.
- [ ] **`MAX_CURRENT` was 20.0 A and the hardware accepts more** (see `next-up.md`). The limit
      now lives on `DPS5020`'s `setCurrent_` row, still at 20 A, so the decision is unchanged
      and merely moved: either the cap is the product's rating, or it follows what the register
      accepts.
