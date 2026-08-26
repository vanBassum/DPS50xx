---
id: 2026-08-26-21h21
date: 2026-08-26
time: "21:21"
title: Association and address are two steps, and one flag cannot hold both
builds-on: 2026-08-11-22h20
supersedes:
---

**Before:** `staConnected_` was the station's one piece of state, set by
`Ipv4Acquired` and cleared by `LinkDown`. `OnCycleTimer` guarded itself with
`if (staConnected_) return;` — reading the flag as "this attempt has succeeded,
leave it alone". `LinkUp` logged "STA connected to AP" and set nothing.

**What changed it:** a boot log where the station joined and the manager kept
rotating anyway:

```
I (13919) NetworkManager: STA connected to AP
I (21349) WiFiInterface: Switching to 'vanBassumExt'
W (21359) wifi:sta is connected, disconnect before connecting to new ap
I (31359) WiFiInterface: Switching to 'vanBassum'
W (31369) wifi:sta is connected, disconnect before connecting to new ap
I (46589) NetworkManager: Reconnected to 'vanBassumExt' after 6 attempts
```

Thirty-two seconds between association and DHCP, and in that gap the cycle timer
counted three failed attempts and rotated away three times from the network that
had just admitted us. Every rotation was refused by the driver — and **being
refused is the only reason the link survived** long enough to get an address. The
summary line then named the wrong SSID, because `CurrentSsid()` follows
`staIndex_`, which the rotation had advanced.

**Now:** association and address acquisition are two separate steps with two
separate failure modes and two separate patiences. DHCP on a busy AP routinely
takes tens of seconds, and that is not a network refusing us. `staConnected_`
answers "do we have an address", which is precisely what its one real caller
(`HasUpstream`) needs — it was never wrong, it was being asked a question it does
not answer. The middle state needs its own name (`staAssociated_`) and its own
timeout.

This is the same failure as
[2026-08-11-22h20](2026-08-11-22h20-the-recovery-ap-is-an-address-with-nowhere-to-go.md),
one level down. There, `has_ipv4` did two jobs — an implementation fact and a
contract — and a new state pulled them apart. Here, one flag serves two
questions, and a slow DHCP is the state that pulls *those* apart. The recurring
lesson is not about naming: **a boolean is a claim about one question, and the
danger is a second caller asking it a different one.** Nothing looks wrong at
either call site while the two questions happen to share an answer.

**Rests on:** `esp_wifi` refusing a `connect` on an already-connected station. It
did so consistently here, which masked the bug; a timing window where the driver
accepted instead would have torn down a working link outright.

**Follows:** `LinkUp` sets `staAssociated_` and hands the timer a
`StaDhcpTimeoutMs` wait; a genuine DHCP timeout retries the *same* network with a
full radio restart rather than a switch the driver refuses, and still counts as an
attempt so an AP that never hands out an address reaches the AP window. The
wrong-SSID log line fixes itself, since it was only wrong because of the rotation.
