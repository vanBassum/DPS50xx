---
id: 2026-09-10-00h40
date: 2026-09-10
time: "00:40"
title: DFS does not slow a command, it drops the packet
builds-on: 2026-09-09-23h20
supersedes:
---

**Before:** the Strux sync brought upstream's `sdkconfig.defaults` wholesale, including
`CONFIG_PM_ENABLE` + `CONFIG_PM_DFS_INIT_AUTO`. Upstream's note on it
(`2026-09-02-15h53`) prices the feature honestly for what it measured — "a few
microseconds of extra interrupt latency and coarser RTOS tick timekeeping" — and that
is what I accepted, because it reads like the whole cost of a well-understood IDF
feature.

Within the hour the UI was unusable through the relay. Pages appeared; their DATA
crawled.

**What changed it:** the same device, measured before and after turning DFS off, over
plain `ws://` on the LAN:

| | DFS on | DFS off |
|---|---|---|
| `system ping` median | 3925 ms | 72 ms |
| `psu get` median | 5223 ms | 127 ms |
| ICMP loss, 20 packets | 45 % | 0 % |
| six-file page over HTTP | 51.8 s | 3.7 s |

The mechanism is NOT that a downclocked CPU computes a reply slowly, and two wrong
theories died on the way to that. TLS was the first — the relay pipe is `wss://`, so
software crypto at 40 MHz looked like the answer, except every number above was
measured over an UNENCRYPTED LAN socket. Symmetric crypto on a few hundred bytes is
sub-millisecond even at 40 MHz; it could never buy four seconds.

The distribution is what gives it away. A slow CPU shifts a distribution; this one was
bimodal — `p10 122 ms`, `median 3925 ms`, `p90 10408 ms` — with the slow mode clustering
at 1, 2, 4 and 8 seconds. That is **TCP retransmission backoff**. The command was never
slow. The packet carrying it was dropped, and TCP waited out a timer that doubles.

So: DFS parks the CPU at the XTAL frequency whenever no task holds a power lock, which
on a device waiting for a request is nearly always. A burst then arrives at a
single-core chip running at a quarter speed, where WiFi RX, lwIP, httpd and this
firmware's own session copying share that core — and each idle→active transition
re-locks the PLL before any of it runs. The RX path falls behind, drops what it cannot
take, and everything above it pays a retransmit.

Two independent confirmations, both of which had looked like separate problems:

- **The 45 % "packet loss" was this.** It had been diagnosed as a marginal radio, with a
  -75 dBm association and a stronger AP going unused as supporting evidence. The device's
  own log was clean throughout — one association, held twenty minutes, no disconnects —
  which is exactly what a dropped-RX problem looks like and NOT what a failing link looks
  like. The signal strength was a coincidence, and a plausible one, which is what made it
  expensive.
- **Pacing an upload fixed it.** An unpaced OTA (3900-byte frames, back to back) reset the
  connection three times running. The identical image in 1400-byte frames 20 ms apart
  went through at 63 KB/s. **A weak radio does not care how you pace frames; a CPU that
  cannot drain its socket does.** That test is the one that turned a suspicion into a
  diagnosis, and it costs nothing to run.

**Now:** this fork does not enable DFS, and `sdkconfig.defaults` says why in a comment
rather than silently omitting two lines a future sync would paste straight back.

**Follows:** the general lesson is about the shape of a latency measurement, not about
power management. A median alone cannot tell "slower" from "sometimes lost", and those
have disjoint fixes — one wants more cycles, the other wants fewer drops. Percentiles
are cheap and they separate the two immediately. The specific lesson is that a
template's default is measured on the template's workload: upstream serves a two-file
page and an LED command, and the same option meets a supply polled every second, a
telemetry point per poll, and a UI that asks several commands per click. Worth reporting
upstream rather than just carrying the deviation.
