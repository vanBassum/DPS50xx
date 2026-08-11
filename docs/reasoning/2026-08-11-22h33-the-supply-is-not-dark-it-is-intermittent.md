---
id: 2026-08-11-22h33
date: 2026-08-11
time: "22:33"
title: The supply is not dark, it is intermittent
builds-on:
supersedes:
---

**Before:** the DPS5020 had not answered since the Strux port was flashed, and the
standing explanation was that the supply was dark or unwired — it had worked on this
wiring on 2026-08-06, so the thing that changed was assumed to be on the bench rather
than on the bus. Every poll produced the same two lines, `Timeout waiting for response
header (0/2 bytes)` and `Attempt 1/1 failed: Timeout`, and there was nothing in them to
argue with.

**What changed it:** one line that had never appeared before —
`RX buffer had 31 stale bytes before flush`. A poll reads `RegCount = 13` holding
registers, and a Modbus RTU response to that is `[unit][func][byteCount][26 data][CRC]`
= **exactly 31 bytes**. So the supply had assembled and sent a complete, correctly sized
response. It arrived after the 500 ms read had given up, sat in the UART buffer until
the next transaction, and was flushed unexamined.

That kills the "dark or unwired" explanation outright, and with it most of the
electrical candidates: a wrong baud rate, a swapped pair or a floating ground do not
produce a frame of exactly the right length, and the `DPS5020 online (addr=1)` moments
in the same log are frames that passed CRC and address checks. Wiring, baud, address
and register map are all correct. Whatever is wrong is *temporal*.

Then the instrumented build said what the old one could not. The header timeout had
been printing a hardcoded `0`, so a silent bus and a slave that got one byte out
produced identical lines. With the real count reported, 48 consecutive polls over 75 s
came back `0/2` — genuinely zero bytes, not a truncated or corrupted frame. Nothing at
all, for a minute and a quarter, on a bus that minutes earlier had delivered a whole
frame.

**Now:** the supply alternates between answering completely and not being there at all,
in stretches of minutes. Both halves are now evidenced rather than inferred, and they
rule out different things: the good frames rule out the link being misconfigured, and
the total silence rules out its being merely slow. What remains is an intermittent
physical connection or an intermittent supply — which is where the bench, not the
firmware, has to be looked at.

The reusable part is the same lesson as
[2026-08-10-19h19](2026-08-10-19h19-the-ap-fallback-argument-was-about-a-field-the-code-threw-away.md),
one layer down and arrived at independently: **the code was discarding the measurement
that classifies the failure, one line before the log that could not classify it.**
There, `wifi_event_sta_disconnected_t::reason` was dropped before the retry policy that
needed it. Here, `ReadExact` returned a bool and threw away how many bytes it had, and
`Execute` counted the stale bytes only to print the count and flush them. In both cases
the fix costs nothing, and in both cases the argument that was stuck for days was stuck
on evidence the device already had and did not report. Worth suspecting, whenever a log
can only say one thing, that the code is computing more than it prints.
