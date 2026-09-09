# A PSU register chain, and the XY6020L on top of it

Adding an XY6020L means a second Modbus register map, and it reads more than a DPS5020
does — energy counters, input temperature, an output timer, memory groups. The decision
(2026-09-09) is **one repo, and the driver becomes the schema**: registers are declared
as chain entries the way settings and commands already are, so poll, `psu get`,
telemetry and range validation become walks over data instead of hand-written lists per
supply.

Why not a second repo: what differs between the two supplies is a register map and a few
extra readings. What a second repo duplicates is the whole Strux sync burden, the
frontend, and the `ArgType::Float` debt. The layer split exists to absorb exactly this
axis. A separate repo would be right if the *product* diverged — different UI, different
audience — not if only the wire does.

Related: [the structural tidy-ups backlog](2026-08-05-structural-tidy-ups.md) ·
`../reasoning/` — the "driver becomes the schema" note lands with phase 1

## The pattern being copied

[Setting.h](../../main/strux/SettingsManager/Setting.h) and
[CommandEntry.h](../../main/strux/CommandManager/CommandEntry.h) are the same trick
twice: the owner provides the memory, the entries are the links of an intrusive chain,
generic consumers walk it knowing nothing about individual entries, and misuse fails on
the first boot (`FATAL` on double registration, or on a registered entry being
destroyed). The settings UI is *generated* from registered definitions — a PSU dashboard
generated from registered registers is the same mechanism one layer down.

```cpp
// interfaces/PsuRegister.h — link + schema facts
struct PsuRegister {
    const char* const key;      // "outVoltage" — wire field AND telemetry field name
    const char* const label;    // "Output Voltage" — shown in the UI
    const char* const unit;     // "V"
    const uint16_t    address;  // Modbus holding register
    const uint8_t     words;    // 1, or 2 for a 32-bit Ah/Wh pair
    const float       scale;    // 0.01
    const PsuAccess   access;   // Read | ReadWrite
    const PsuFlags    flags;    // Telemetered | Setpoint | …
    // asFloat()/asBool()/asEnum() checked downcasts, next, registered, FATAL dtor
};
```

Four hand-written lists collapse into chain walks in shared code:

| Today | With the chain |
|---|---|
| `DPS5020::Poll()` reads a fixed `0x0000..0x000C` | coalesce contiguous addresses into batch reads, decode into entries |
| `Cmd_Get`'s 14 `resp.field(...)` lines | `for (auto& r : psu) resp.field(r.key, r.Get())` |
| `Record()`'s 7 `point.Field(...)` lines | walk, emit the `Telemetered` ones |
| `MAX_VOLTAGE` / `max={50}` in TSX | `min`/`max` on the entry, sent with the schema |

## Two deviations from the settings idiom, both deliberate

- **Entries are ordinary members, not `inline static`.** A setting is `inline static`
  because there is one settings namespace per device. Register *values* are per
  instance — `inline static` would make two supplies on one bus share value storage.
  Chained in the driver's constructor; memory is still owner-provided, still no heap,
  still immortal in practice (`BoardContext` owns the driver for the life of the app),
  and the `FATAL`-on-destroy guard still earns its keep. **Say this in the header**, or
  someone will "fix" it to match `Setting` later.
- **The chain is not the only surface.** Some registers carry meaning the product
  depends on: the chart plots Vout, the LED mirrors the link, `psu set` orders writes so
  it cannot brown-out a load. So the driver keeps typed named members *and* registers
  them (`d.outVoltage` becomes `outVoltage_.Get()`), exactly as `pollIntervalMs_.Get()`
  sits beside the settings chain — compile-checked, no string lookup on the poll path.
  What generic code must locate by meaning goes through well-known keys on the role
  (`PsuKey::OutVoltage`), resolved once at `Init()`.

**`psu set` stays typed for the core setpoints.** A generic `psu reg set -key -value`
would throw away the subset-in-one-command behaviour and the write ordering in
[PsuManager.cpp](../../main/app/PsuManager/PsuManager.cpp) — and `readArgs` is variadic
with literal names, so a runtime-table-driven arg list has no `help list` at all (see
`reasoning/2026-08-03-15h36-declaring-all-arguments-at-once-makes-the-parse-zero-buffer`).
Typed `psu set` for setpoints; generic `psu reg set` for the long tail.

## Phase 1 — the chain, against the DPS5020 alone

Behaviour identical afterwards, `psu get` gaining schema fields. Done first and alone so
that if the abstraction is wrong, it is wrong before a second map depends on it.

- [ ] `main/hardware/interfaces/PsuRegister.h` — the link + schema base, typed leaves
      (`FloatRegister`, `BoolRegister`, `EnumRegister`), checked downcasts, `FATAL` dtor.
- [ ] `main/hardware/interfaces/Psu.h` — the role: `Poll()`, `IsOnline()`, chain
      iteration (`begin()`/`end()`, mirroring `SettingIterator`), `Find(key)`, a
      `PsuLimits`, the core typed accessors, and the write methods the manager needs.
      **Goes on `BoardProvider`**, which inverts the argument in the current
      `BoardContext` comment: a `Psu` role was refused there because it would be
      "DPS5020's whole API or a lossy subset"; with two supplies the intersection is the
      shared vocabulary instead of one chip's API. `MockPsu` for a supply-less board is
      the `MockLed` precedent. Keep `GetDps()` as the concrete escape hatch.
- [ ] Generic poll in one place (a `PsuBase`, or a free helper): walk the chain,
      coalesce contiguous addresses into batch reads, decode by `scale`/`words`, keep
      the existing retry and `OFFLINE_THRESHOLD` behaviour.
- [ ] `DPS5020` reimplemented as a table + the role. Its map is already contiguous
      `0x0000..0x000C`, so the coalescer must produce exactly one read for it — that is
      the phase-1 correctness check.
- [ ] `PsuManager`: `Cmd_Get` and `Record()` become walks; `MAX_VOLTAGE`/`MAX_CURRENT`
      move off the manager onto the driver's limits; the protection *label* comes from
      the driver next to the code, so `PROTECTION_LABELS` in `backend.ts` can die.
- [ ] Frontend: readouts and chart keep a typed path off well-known keys; everything
      else renders from the schema (label/unit/value), the way the settings page already
      renders registered definitions. `max={50}`/`max={20}` in `HomePage.tsx` read
      `maxVoltage`/`maxCurrent` off the reply.
- [ ] Verify on hardware — build, flash, drive `psu get`/`psu set` over the WebSocket,
      confirm one Modbus transaction per poll, and check telemetry field names are
      unchanged (renaming one silently orphans the Influx history).
- [ ] Reasoning note: the driver becomes the schema, and why the `Psu` role's argument
      inverted at the second supply.

## Phase 2 — the XY6020L

- [ ] **The register map must come from the datasheet, not from memory.** Addresses,
      scales, which registers are 32-bit pairs, and the protection vocabulary all need
      checking against a real unit before anything is written down. This is the one part
      of the plan with no code answer.
- [ ] `drivers/XY6020L.{h,cpp}` — the table, the limits (60 V where the DPS is 50), and
      its own protection labels.
- [ ] Board folders. A board folder already means MCU + wiring + which drivers get
      bound, so `xy6020_esp32` / `xy6020_c3` uses the existing mechanism. If the 2×2
      gets annoying, a second cache var (`-DPSU=`) is a later refactor — do not build it
      yet. Remember `-DBOARD` goes on `set-target` too, and use `-B build-xy` to keep
      both configured trees.
- [ ] Writable extras (OVP/OCP/OPP thresholds, memory-group recall, the output timer)
      as a typed `psu adv` command, not through the generic path: they need range
      validation and write ordering, which a table cannot carry.
- [ ] Verify on hardware, and only then decide whether the auto-detect below is wanted.

## Open

- [ ] **Build-time supply choice, or auto-detect.** With both drivers compiled in and a
      `Psu` role, the supply could be detected at boot — a DPS5020 answers `0x000B`
      with `5020` — giving one firmware for both. Attractive, but it adds runtime
      branching and a detection failure mode. Default to the build-time choice unless
      one binary is actually wanted.
- [ ] **The repo name goes stale.** `DPS50xx` stops describing the product at phase 2.
      Rename, or accept it. Not urgent, and cheap either way.
- [ ] **`psu.*` settings become shared across supplies.** `psu.poll` and `psu.telem`
      are fine as they are; a per-supply setting would need a naming rule that fits
      inside the 15-char NVS limit, and there is no room for one. Avoid needing it.
- [ ] **How many entries is too many for a 1 Hz poll.** A ~40-register map is a bigger
      `psu get` reply than today's 14 fields. The reply is outbound and chunked, so the
      4096-byte inbound window does not apply, but the browser polls every second —
      measure before assuming it is free.
- [ ] **`MAX_CURRENT` is 20.0 A and the hardware accepts more** (see `next-up.md`). The
      limits-on-the-driver move is where that decision finally has to be made: either
      the cap is the product's rating, or it follows what the register accepts.
