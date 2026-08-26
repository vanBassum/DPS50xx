---
id: 2026-08-26-21h22
date: 2026-08-26
time: "21:22"
title: An AP refusing us and an AP too far away are the same log line
builds-on: 2026-08-12-13h25
supersedes: 2026-08-12-14h05
---

**Before:**
[2026-08-12-14h05](2026-08-12-14h05-the-extender-was-not-wedged-it-was-inaudible.md)
concluded that `vanBassumExt` was simply inaudible — reason 2 at −83 dBm needs no
misbehaving AP to explain it, so the earlier "wedged extender" reading was
withdrawn in favour of range. That was the best available inference from the one
number the device could report, and the note was explicit that range explains the
failures rather than that the extender is healthy.

**What changed it:** the same failure at signal strengths where range cannot
explain it. Reason 2 on every attempt at **−66 to −68 dBm**, three boots out of
three, with a phone associated to that same extender at the same time. And a
scan, once the device could report authmode:

```
SCAN: 'vanBassumExt' ch11 -75 dBm auth=WPA2
SCAN: 'KPN7EBD96'    ch11 -87 dBm auth=WPA2/WPA3
```

Plain WPA2 — with a neighbour proving transition mode would have been reported had
it been there. So the attractive alternative explanation, that the extender
required PMF and refused a station not offering it, is dead too: PMF is a real
cause of reason-2-at-full-signal, and it is not this one.

**Now:** the extender refuses *this client specifically*, on WPA2, with the same
PSK that joins `vanBassum` in 2.5 s. Range is ruled out by RSSI, credentials by
the shared PSK, security negotiation by the authmode, and the station's own radio
by the network it does join. The original "wedged" reading is restored, now on
evidence rather than inference — the remaining candidates are all on the AP: a
client table that needs a power cycle, a MAC filter, or a client limit.

The instrument matters more than the verdict. `ScanResult` carried `bool secure`,
i.e. `authmode != OPEN`, which collapses every security mode into one bit — and
that bit cannot distinguish an AP refusing us from an AP too far to hear. This
project has now twice had to widen what a WiFi event reports before a diagnosis
became possible: the reason code and RSSI in
[2026-08-12-13h25](2026-08-12-13h25-a-timeout-is-patience-for-silence-not-a-gap-between-attempts.md),
and the authmode here. **A field that summarises loses exactly the distinction the
failure turns on**, and which distinction that is only becomes apparent when
something fails.

**Rests on:** the scan being taken from where the device stands, which it is —
this is the device's own radio reporting, not a laptop's.

**Follows:** `ScanResult` carries `wifi_auth_mode_t` and `wifi scan` reports it by
name. The PMF advertisement added the same day stays: correct against WPA3 and
transition-mode APs, free on WPA2-only hardware, and simply not the fix here.
