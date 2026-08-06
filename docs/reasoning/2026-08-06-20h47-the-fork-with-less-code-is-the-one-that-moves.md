---
id: 2026-08-06-20h47
date: 2026-08-06
time: "20:47"
title: The fork with less code is the one that moves
builds-on: 2026-08-06-16h55
supersedes:
---

**Before:** DPS50xx was forked from Strux at a point when the two shared a shape, and the plan
for catching up was the obvious one — backport the framework improvements into DPS50xx, feature
by feature. Work had already started that way: about 430 lines of hand-ported authentication
across `WebSocketHandler` and `backend.ts`, which is one feature of roughly ten.

**What changed it:** counting both sides instead of one. DPS50xx's own code — the Modbus
library, the DPS5020 driver, the manager that polls it, the dashboard page and its hook — is
about 1,600 lines. The framework it was going to import is about 5,900 in `strux/` alone,
before `lib/protocol/`, the relay server, and forty reasoning notes.

The ratio understated it, though, and the real argument was that **nothing shared a seam any
more**. DPS50xx had no `CommandContext`, no `ReplyWriter`, no `readArgs` — a grep for all three
returned nothing. Its wire format was a JSON envelope where Strux now sends binary session
chunks. It had one flat `Application/` folder where Strux has three layers, each with a context
and a provider. Every handler would have been rewritten either way. So "backport the
framework" was never really importing code into a compatible host; it was rebuilding Strux
inside DPS50xx, by hand, with no test to say when it was done.

Reversing the direction turns the same work into a port with a *finish line*: the framework
arrives whole and already working, and what remains is placing 1,600 known lines into slots the
framework documents.

**Now:** DPS50xx is Strux plus an application. The layer split is what makes this the last time
the question comes up — `strux/` is untouched by anything in this port, so the next improvement
is a copy of a directory rather than a merge. Two things did get edited outside `app/` and
`hardware/`, and both are worth naming because they are the debt this port took on:

- `lib/protocol/` gained `ArgType::Float`. The reply side already had `value(float)`; only the
  request side lacked it, and a bench supply's whole interface is setpoints. The asymmetry was
  the argument that it belongs upstream, not in a fork.
- `partitions.csv` came from Strux, which moved `ota_1` and shrank `www`. A partition table is
  not updated by an application OTA, so this port reaches a device by serial cable, once.

**Follows:** the archived branch `archive/auth-backport` holding the abandoned 430 lines, kept
only because deleting work silently is how the same idea gets tried twice; `MQTT` and Home
Assistant simply not arriving, since Strux removed them in July and this product never wanted
them; and the DPS5020 reachable as a concrete accessor on `BoardContext` rather than a `Psu`
role, for the reason 2026-08-06-16h56 gives.
