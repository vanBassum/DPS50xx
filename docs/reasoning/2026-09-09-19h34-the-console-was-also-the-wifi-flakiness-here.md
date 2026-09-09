---
id: 2026-09-09-19h34
date: 2026-09-09
time: "19:34"
title: The console was also the WiFi flakiness here
builds-on: 2026-09-09-20h40
supersedes:
---

**Before:** this project's WiFi symptoms had been read as an RF and policy story. `next-up.md`
carried "the AP→STA half of the cycle is unverified", "a second network has never been tried",
and a bench conclusion that `vanBassumExt` was simply out of range; `2026-08-10-19h19` and
`2026-08-12-14h05` were about retry policy, reason codes and beacon strength. Every one of those
investigations took the disconnects as *given* and asked what the radio or the policy should do
about them.

**What changed it:** the XY6020L's first flash, on a board freshly copied from `dps50xx_c3`,
disconnected from `vanBassum` with **reason 4 — ASSOC_EXPIRE, "the AP saw us as idle" — at
−70 dBm**, three attempts, then a 15-minute recovery AP window. −70 dBm is a perfectly audible
AP. Upstream had just found (`2026-09-09-20h40`, Strux `7a7f22e`) that
`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` makes USB the *primary* console, whose write blocks on the
USB host, and that `ConsoleManager::LogOutput` puts a `vprintf` in front of every task in the
system — the WiFi and lwIP tasks included. Both C3 overlays here had that line.

Moving USB to the SECONDARY console changed the same board, same AP, same wire, nothing else:

```
before:  attempt ×3 → reason 4 (idle, −70 dBm) → recovery AP at 15.7 s
after:   STA connected 3.0 s · IP 192.168.11.21 at 4.1 s · relay connected 4.6 s
```

So a stalled task did not only *lose packets* here — it lost the **association**, because an AP
drops a station that stops answering, and "stopped answering" is indistinguishable from "went
away". The upstream note's symptom was 78 % ICMP loss with clean command counters; ours was a
station being evicted for idleness. One cause, two presentations, and the second one is the more
misleading because reason 4 reads as a *network's* verdict on us rather than as our own fault.

**Now:** the guard lives in `ConsoleManager.cpp` (an `#error`, with
`STRUX_ALLOW_BLOCKING_CONSOLE` for anyone who has measured otherwise), because a comment in a
board file is exactly what failed the first time — and this repo had *two* copies of that comment
by then, the second written today by copying the first.

What this does and does not overturn, stated carefully:

- It **does** put the idle-eviction disconnects, and therefore some of the recovery-AP windows
  this bench kept producing, on the console rather than on the radio or the retry policy.
- It does **not** overturn `2026-08-12-14h05`. That was reason 201 (AP not found) with two scans
  in three seeing nothing at all and the third hearing −83 dBm; an inaudible access point is
  still inaudible. The lesson is narrower and sharper: **a reason code says what the peer
  concluded, not what caused it**, so reason 4 and reason 201 needed separating before either
  could be acted on, and they were not separated.
- It leaves the AP→STA half of the cycle still unverified, but for the first time on a station
  that reliably associates.

**Follows:** GPIO3/4 turned out fine for the antenna on this module — the association held from
3.0 s — so that check is answered and the pins are not the suspect they were; and the same
overlay fix now sits in `dps50xx_c3`, unflashed, where it predicts the DPS bench's own WiFi
history was partly this.
