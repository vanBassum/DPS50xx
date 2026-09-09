---
id: 2026-09-09-18h32
date: 2026-09-09
time: "18:32"
title: A driver that declares its registers is the schema
builds-on: 2026-08-06-16h56
supersedes:
---

**Before:** the supply was a struct. `DPS5020` read thirteen registers into a `DPS5020Data`
with thirteen named fields, and everything above it named those fields again: fourteen
`resp.field(...)` lines in `psu get`, seven `point.Field(...)` lines in the telemetry point,
`MAX_VOLTAGE`/`MAX_CURRENT` in the manager, `max={50}`/`max={20}` in the dashboard, and a
`PROTECTION_LABELS` array in TypeScript naming codes the supply defines. That is five places
that know a DPS5020's register map, and the driver is only the first of them.

It was the right shape for one supply. The question that broke it was an XY6020L: a second
Modbus map, on the same wire, that reads *more* — energy accumulators, input temperature, an
output timer, memory groups — and whose limits and protection codes differ.

**What changed it:** noticing that the extra readings are not a feature. Nothing in the product
branches on Ah, Wh or a temperature; they are a number, a unit and a label that the UI prints
and telemetry records. What the two supplies share is therefore not a chip's API and not a
struct — it is a *chain of self-describing readings* plus four verbs over it. So the driver
stops being the thing that fills a struct and becomes the thing that declares a schema, and
`psu get`, the telemetry point, the setpoint validation and the dashboard all become walks over
whatever it declared.

The pattern was already in the codebase twice, which is the part that makes this cheap: this is
`Setting` and `CommandEntry` — the owner provides the memory, the entry is the link, misuse
fails on the first boot. The settings UI is *generated* from registered definitions; a supply
dashboard generated from registered registers is the same mechanism one layer down. Two
differences were forced, and both are the kind that get "fixed" later by someone matching the
neighbour:

- readings are ordinary members, not `inline static`. One settings namespace per device makes
  `Setting` static; reading VALUES are per instance, and two supplies on one bus sharing static
  storage is a silent bug.
- the chain appends at the tail where `Setting`'s pushes at the head, because here the order is
  visible: declaration order is register order is display order.

**Now:** `DPS5020` is a thirteen-row table and a constructor that registers it — the batch read,
the retries, the offline threshold, the decode and the writes all live once, in `ModbusPsu`.
Nothing above the driver names a register: `Cmd_Get` emits each reading's own label, unit, kind,
value and writable range; `Record()` uses the telemetry name the driver gave a reading (which is
deliberately *not* its key — `voltage` and `inputVoltage` are what the existing Influx series
are called); `psu set` validates against the reading's own min/max, so the same handler refuses
61 V on a supply it was never told is a 60 V one, and refuses `-backlight` on a supply with no
display by finding no such register. The frontend renders a `readings` map it does not have a
type for, and both hardcoded limits and the protection label table are gone.

This also inverts 2026-08-06-16h56 in one particular. That note argued a `Psu` role was
unaffordable because it would have to be "DPS5020's whole API or a lossy subset", obliging
every board to bind a `MockPsu`. True while there was one supply — the shared vocabulary did
not exist yet, so any role would have been one chip's API wearing a role's name. A second
supply is what creates the vocabulary, and the role became the intersection of two maps rather
than the whole of one. `GetPsu()` is now on `BoardProvider`; `GetDps()` stays beside it as the
concrete escape hatch the note's argument still holds for.

**Follows:** the XY6020L is a table and a board folder, with its register map still owed by the
datasheet rather than by any code; `MockPsu` exists unused, the same way `MockLed` does; a
32-bit two-register reading is supported and unexercised until that map lands; and one thing
the walk deliberately did not swallow — the log line still names its five values, because a
human reads it and wants the same five every time.
