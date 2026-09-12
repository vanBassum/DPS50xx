---
id: 2026-09-13-12h00
date: 2026-09-13
time: "12:00"
title: A tag is part of the series key, so recording the mode as one split the graph
builds-on:
supersedes:
---

**Before:** `PsuManager::Record` walked the capability chain generically — every field
named by the driver, nothing named in the application — with exactly one exception, and the
exception was commented as deliberate: the CC/CV mode went out as an Influx **tag**, "because
it is what a query groups by". That reads as the textbook use of a tag, and it is why the
line survived review.

**What changed it:** Bas watched the supply cross from CC into CV and the voltage and current
lines *changed colour*. Not a styling glitch — the graph was drawing different series. A tag
in Influx is not an annotation hanging off a point; it is part of the **series key**. Changing
its value does not label the existing series, it starts a second one. So `psu,mode=cc voltage`
and `psu,mode=cv voltage` are two distinct series that any client picks a fresh colour for,
each gapped exactly where the other is live. The very property that makes a tag good to group
by — that it partitions the data — is what made it wrong here.

**The distinction that was missing:** a tag is for something that identifies *which supply
this is* — an identity that holds for the life of the series. The mode is a *measurement*: it
changes many times within one recording, at the same rate as the numbers beside it. Anything
that varies per point is a field, however categorical it looks. "What you group by" is not the
test; "what is constant across the series" is.

**After:** the mode is a boolean field named by the driver like everything else —
`constantCurrent_` simply grew the telemetry key `cc`, in both DPS5020 and XY6020L — and
`Record` has no named value left in it at all. The exception to the generic walk is gone, and
its removal is also the fix: the one place the application reached in to name something was
the one place that was wrong.

History recorded before this is not recoverable as one series — the old points carry the tag
and always will. What is recorded from here on is continuous.
