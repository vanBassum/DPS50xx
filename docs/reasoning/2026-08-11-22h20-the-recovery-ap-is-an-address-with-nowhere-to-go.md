---
id: 2026-08-11-22h20
date: 2026-08-11
time: "22:20"
title: The recovery AP is an address with nowhere to go
builds-on: 2026-08-10-19h19
supersedes:
---

**Before:** `RelayManager`'s dial-out loop guarded itself with
`NetworkManager::HasIpv4()` — "is there an address to reach the world with yet?" The
gap it was written for was the boot-time race: the relay task starts during `Init()`,
while WiFi is still associating, and without the guard every boot spent one connect
attempt it could not win and printed three ERROR lines from the TLS and transport
layers on the way out. For that gap the predicate was exactly right, and it had been
right for as long as it existed, because the only way the device held an IPv4 address
was by having joined a network.

**What changed it:** the AP window, added the day before
([2026-08-10-19h19](2026-08-10-19h19-the-ap-fallback-argument-was-about-a-field-the-code-threw-away.md)),
which made the access point *recur* instead of being a one-way trip. `WiFiInterface`
repoints `netif_` at the AP netif in `StartAP`, and the AP netif is always 192.168.4.1
— so `has_ipv4` is true for the whole fifteen minutes of every window. The boot log of
2026-08-11 shows what that buys: the AP came up at t=30.8 s, and the relay's first
connect attempt fired one second later, then at 5, 10, 20 and 40 s, each one three
ERROR lines about a hostname that cannot resolve, because there is no resolver and no
route. The guard had inverted — it now said *go ahead* during precisely the stretch of
time when there was certainly nowhere to go.

Nothing in `RelayManager` changed. Nothing in the AP work mentioned the relay. What
joined them is that `has_ipv4` was doing two jobs — an implementation fact (this netif
holds an address) and a contract (the world is reachable) — and the new state is
exactly where the two come apart. The doc comment stated the contract; the *name*
stated the fact; and a name that describes its implementation is what let the two drift
apart with no call site looking wrong.

**Now:** `HasUpstream()`, defined as `staConnected_ && !IsAP()` and named for the
question its one caller actually asks. The rename is the point rather than the
predicate: had it been called that from the start, `getStatus().has_ipv4` would have
read as an obviously insufficient implementation of it the moment a second netif could
be the current one.

The general shape is worth keeping: **a manager that adds a state can invert a
predicate another manager reads, without either file changing.** The layering rules
stop `strux/` reaching *up* into the application, and stop the application being
fetched from the framework — they say nothing about two peer managers coupled through
a shared fact about the device. There is no compile-time guard for that class of
break, only naming a predicate after its question, so that a new state has to argue
with the name.

One thing deliberately not fixed. `RelayManager` silences `esp-x509-crt-bundle` and
`transport_ws` so its retry loop reports a reason rather than an attempt, but
`esp-tls` and `transport_base` still narrate every attempt at ERROR — which is the
noise in that log. Silencing them would be global, and the same two components carry
the only diagnosis a failed OTA pull produces. With the guard corrected the AP-window
case (all of it, here) is gone; a genuinely unreachable relay on a working network
still costs three lines a minute at the 60 s cap, and that is the trade being kept.
