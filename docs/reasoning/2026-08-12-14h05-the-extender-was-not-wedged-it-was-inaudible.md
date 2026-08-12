---
id: 2026-08-12-14h05
date: 2026-08-12
time: "14:05"
title: The extender was not wedged, it was inaudible
builds-on: 2026-08-12-13h25
supersedes:
---

**Before:**
[2026-08-10-19h19](2026-08-10-19h19-the-ap-fallback-argument-was-about-a-field-the-code-threw-away.md)
concluded that `vanBassumExt` was *wedged*: up, advertising, admitting nobody. The
evidence was reason 2 on every attempt, a beacon reported at 72% strength, a Windows
laptop failing against the same SSID, and the same PSK associating with the main router
in 1.7 s. The note flagged the inference honestly — "not from the code alone" — and it
was the best reading available from one bit of signal strength measured by something
that was not this device.

**What changed it:** the first boot with the RSSI in the disconnect line.

```
W (3239)  STA disconnected: no such network in range (reason 201, last beacon -128 dBm)
W (7659)  STA disconnected: no such network in range (reason 201, last beacon -128 dBm)
W (13089) STA disconnected: auth expired (reason 2, last beacon -83 dBm)
```

Two attempts out of three cannot find the network *at all* — reason 201, not reason 2.
The one that finds it hears it at **-83 dBm**. At that level an association failing
during auth needs no misbehaving AP to explain it: the beacon arrives at 1 Mbps and
decodes, and the frames going back do not reliably arrive. "Wedged" was an inference
built on a signal strength nobody had measured *from where the device is standing*.

**Now:** the failure is range, and the reason code alone could never have said so.
Reason 2 is compatible with an AP that refuses us and with an AP we can hear but not
reach; those are opposite problems with opposite fixes — power-cycle the extender, or
move the device — and the number that separates them was in the event all along. The
previous note's structural conclusion survives untouched (alternate, classify nothing),
because a policy that needs no taxonomy is right whichever of the three failures this
turns out to be. What does not survive is the specific culprit, and it was named at more
confidence than one bit of evidence could carry.

The general shape recurs: this project has now twice reasoned to a confident conclusion
from a measurement taken somewhere other than the thing being diagnosed — the
vanbassum.com outage seen from outside, and an extender's health seen from a laptop.
Both times the fix was a channel that reports what the device itself perceives.

**Rests on:** `-83 dBm` being a fair sample. It is one beacon at one instant from a
device on a bench; a marginal link is exactly the kind that varies. The claim is that
range explains the failures without a wedged AP, not that the extender is healthy.

**Follows:** point this device at the main router rather than the extender, or move it —
neither is a firmware change, which is the useful part of the answer. `-128` no longer
prints as a number: it is esp_wifi's filler for "nothing to measure", and a reader who
takes it for a signal strength learns the opposite of the truth, so the line now says
"no beacon heard".
