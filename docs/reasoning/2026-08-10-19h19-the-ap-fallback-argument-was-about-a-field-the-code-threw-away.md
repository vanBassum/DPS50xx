---
id: 2026-08-10-19h19
date: 2026-08-10
time: "19:19"
title: The AP-fallback argument was about a field the code threw away
builds-on: 2026-08-05-19h05
supersedes:
---

**Before:** the WiFi retry policy was settled by argument, twice, in opposite
directions. The original code gave up after three attempts and started an open AP;
b1963e5 (2026-08-06) removed that, reasoning that "an absent network is a condition
time fixes; wrong credentials are not, and only the second is worth an AP", and made
retries unbounded with the AP reachable only by a device that had never been
configured. Both positions rest on the same premise: that a failed association falls
into one of two classes, and that the right policy follows from which class it is.

**What changed it:** a device that would not join `vanBassumExt` at all. The log said
`STA disconnected`, three times, and then AP mode — one bit wide, exactly the shape of
[the vanbassum.com outage](2026-08-05-19h05-a-healthy-machine-can-be-entirely-off-the-network.md)
one layer down. `WIFI_EVENT_STA_DISCONNECTED` carries
`wifi_event_sta_disconnected_t::reason`, and `WiFiInterface` was discarding
`event_data` without reading it. So the two-class taxonomy the policy argument turned
on was a taxonomy the code had no way to evaluate — the field that assigns the class
was being dropped one line before the decision that needed it.

Logging the reason produced `auth expired (reason 2)`, identically on every attempt,
~1.1 s in. That is neither of the two classes. Not an absent network (201
`NO_AP_FOUND`), not wrong credentials (15 `4WAY_HANDSHAKE_TIMEOUT`, 202 `AUTH_FAIL`) —
the AP was beaconing at 72%, answering the auth frame, and then letting the exchange
lapse. A Windows laptop with its own saved profile failed against the same SSID twice
(event 8002), and the same PSK associated with the main router in 1.7 s. The extender
was wedged: up, advertising, admitting nobody.

**Now:** two things, and they are inseparable.

The taxonomy is wrong, so classification cannot be the basis of the policy. A wedged
AP is a *third* class that mimics both — it looks like wrong credentials (auth never
completes) and behaves like an absent network (waiting will not fix it, but power-cycling
the AP will). Neither prior position anticipated it, because both were reasoning from a
two-element set that nothing had ever checked. So the structure that survives is the one
that needs to classify nothing: **alternate**. Three station attempts, a 15-minute AP
window, repeat, forever. Whichever failure it actually is, the half that addresses it
comes round again within fifteen minutes, and neither half is terminal. The lopsided
periods are not a tuning detail but the asymmetry itself — 30 s is a machine retrying a
local association, 15 min is a human noticing, walking over, and typing.

And the diagnostic is not a nicety to add once the design is settled — it is what makes
the design decidable. Both previous policies were argued at full confidence about
evidence neither could see. This is the same lesson as the outage note (a probe from
outside re-measures one bit; the fix is a channel showing internal state), except here
the channel existed, arrived on every failure, and was being deliberately dropped. The
cheapest place for that to happen is a handler that takes `void* event_data` and never
casts it.

Rests on: the reason code being trustworthy about *which* stage failed. Reason 2 says
the AP stopped answering during auth; it does not say why the AP is in that state, so
"wedged extender" is inference from the two corroborating observations (another
supplicant failing, the same PSK working elsewhere), not from the code alone.

**Follows:** `WiFiInterface` logs the reason by name; `NetworkManager` cycles instead of
terminating in either mode, and re-reads credentials at the start of every station round
— which is what let a new SSID take effect without a reboot. All three are in `strux/`,
so all three are fork debt against upstream Strux, and the reason-code line is the one
worth contributing back first: it costs nothing and it is what turned this from an
argument into a diagnosis. The AP->STA half of the cycle is still unverified on hardware
— the device rebooted before its first window elapsed.
