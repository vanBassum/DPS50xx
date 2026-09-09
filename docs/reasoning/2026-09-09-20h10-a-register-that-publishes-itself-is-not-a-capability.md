---
id: 2026-09-09-20h10
date: 2026-09-09
time: "20:10"
title: A register that publishes itself is not a capability
builds-on: 2026-09-09-18h32
supersedes:
---

**Before:** `2026-09-09-18h32` established that a driver declares self-describing readings and
everything above walks them. That was right about the direction and wrong about the type. One
class — `PsuRegister : PsuReading` — carried both an address and a label, so a register *was* a
reading. Publication was therefore the default, and hiding something took a flag.

The XY6020L is what showed the cost. Its runtime lives in three registers, so the dashboard grew
"On Time (h)", "On Time (min)", "On Time (s)". Its unconnected probe answers 8888, so the UI
printed 888.80 °C as a temperature. Its memory group is a small integer, so the UI printed
"Memory Group 0.00". Every one of those is a UI field that exists **because a register exists**,
and each was individually defensible — a reading with a label, a unit, a value. The pattern only
becomes visible in the plural.

The first attempt at a fix made it worse in an instructive way: I added `published` and
`addressed` booleans, so a register could opt *out* of being a capability. That keeps the wrong
default and adds bookkeeping to it. Two flags whose only job is to suppress the model's own
behaviour are the model telling you it is inside out.

**What changed it:** separating storage from meaning into two types, with a binding between them
instead of inheritance:

```
device register map          ModbusReg      — address, scale, word order. Storage.
    ↓ driver decoding
semantic capabilities        PsuCapability  — key, kind, group, unit, value. Meaning.
    ↓
manager / psu get / telemetry / frontend
```

A capability may sit on one register (`BoundCapability`, which HAS a `ModbusReg` — it is not a
kind of one), combine several, need scaling, be derived from arithmetic, or exist on one supply
and not another. And the case that matters: **a register may have no capability at all.** The
poll walks the register list, decodes each address, and hands the value to the capability behind
it *if there is one*. `ModbusReg onHours_{0x000A}` is polled and published nowhere, and needs no
flag to say so, because publishing was never the default.

What that bought, measured on the wire:

- three registers → one `runtimeSeconds` (a `duration`, formatted `01:18:51` by the browser)
- 8888 → `available: false` → an em dash, with the sentinel declared in the driver and the
  number appearing nowhere else
- a small integer → an `enum` carrying `["M0"…"M9"]`, so a selector renders without React
  knowing that 0 means M0
- nine protection registers → typed limits in their own `group`, off the dashboard entirely,
  written through one generic `psu write` validated against each capability's own range

The frontend's key list shrank to the eleven things it addresses BY MEANING (what it reads out,
edits, toggles). Everything else it renders from `group` and `kind`. It had been picking "extras"
by subtracting a hardcoded set of known keys — which is the same conflation wearing a different
hat, and the reason a memory group could reach a dashboard at all.

**Now:** the boundary has a name and a shape, and the test for a violation is easy to state: if a
UI field exists because a register exists, the driver has not finished its job. Registers stay in
`drivers/`, capabilities live in `interfaces/Psu.h`, and `PsuKey` is a list of semantic names
with no supply's map in it (`outputVoltage`, not `outVoltage` off register 0x0002).

**Follows:** `psu get` returns `capabilities`, not `readings`, and the DPS5020 needed no
normalization at all — every one of its registers already means what a capability means, which is
why it has no `Normalize()` override and pays nothing for the seam. Also settled: the reason to
keep telemetry names separate from capability keys, which looked like duplication in
`2026-09-09-18h32` and is what let every key be renamed today without orphaning an Influx series.
