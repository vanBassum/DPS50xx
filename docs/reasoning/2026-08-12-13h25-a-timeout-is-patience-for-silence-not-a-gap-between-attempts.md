---
id: 2026-08-12-13h25
date: 2026-08-12
time: "13:25"
title: A timeout is patience for silence, not a gap between attempts
builds-on: 2026-08-10-19h19
supersedes:
---

**Before:** one constant, `StaConnectTimeoutMs`, was the whole timing of a station
round: an attempt was given ten seconds, and the next attempt began when those ten
seconds were up. The disconnect handler said so in as many words — *"If not yet
connected, the cycle timer handles retries"* — and that read as a division of labour
rather than as a cost.

**What changed it:** a log of the wedged extender from
[2026-08-10-19h19](2026-08-10-19h19-the-ap-fallback-argument-was-about-a-field-the-code-threw-away.md),
read for its timestamps instead of its reason codes:

```
W (10749) NetworkManager: 'vanBassumExt' did not answer; alternating 3 attempts ...
I (10749) WiFiInterface: Connecting to 'vanBassumExt'
W (11809) WiFiInterface: STA disconnected: auth expired (reason 2)
                          ... nothing at all until 20749 ...
```

The attempt resolves in 1.06 s and the radio then sits idle for 8.9 s. Nine seconds of
every ten of a station round are spent waiting for information that has already
arrived. A 30-second round is really three seconds of trying and twenty-seven of
nothing.

The timeout was doing two jobs that only look like one. *How long to wait before
concluding an attempt failed* and *how long to leave the radio alone between attempts*
are different questions, and they had the same answer only because the code could not
tell that an attempt had concluded. Once the reason code reaches the manager — which is
what the previous note's change made possible, one level short of here — a failed
association is an event, not an expiry, and the timeout shrinks back to its real job:
the ceiling on an attempt that says nothing at all.

**Now:** the disconnect that means *this attempt failed* schedules the next one two
seconds later; the ten seconds remain, but only for silence. And a round that used to
be paced by the clock is paced by the failures, which is why nothing about
`StaAttemptsPerRound` needed changing.

The same field settles a second question the flags never could. A LinkDown arriving
while the station is unconnected is not always the network saying no — this manager
raises some of them itself, by stopping the radio at the start of a round. The
`apWindowOpen_` flag covers exactly one of those cases and only because the flag
outlives the event; a second flag around the teardown would not, since the event
arrives on the event task whenever it arrives. The reason code answers it without any
flag at all: `ASSOC_LEAVE` is our own `esp_wifi_disconnect`, reason 0 is a LinkDown from
somewhere with nothing to say (a stopped AP), and everything else is an association that
genuinely failed. So the field that
[was being dropped one line before the decision that needed it](2026-08-10-19h19-the-ap-fallback-argument-was-about-a-field-the-code-threw-away.md)
now decides two, and the second one is a race the code could not otherwise have won.

Two smaller things followed from looking at the attempt rather than the round. Tearing
the radio down between retries of the *same* round was buying nothing and generating the
phantom disconnect above, so a retry is now `esp_wifi_connect()` on a station that is
already up — the pattern the was-connected branch had always used. And the default fast
scan associates with the first beacon heard, which on a network carried by more than one
radio is not necessarily one that will complete an association; all-channel scan sorted
by signal picks the strongest each attempt, which is the one policy change aimed at the
extender itself rather than at the log.

**Rests on:** `esp_wifi_disconnect()` reporting `ASSOC_LEAVE` and `WIFI_EVENT_AP_STOP`
carrying no reason. If a future IDF reports our own disconnect as something else, a
teardown echo becomes a prompt retry — bounded (one wasted attempt, and the round caps
at three) but wrong.

**Follows:** `NetworkEvent` carries `reason` and `rssi`; the once-per-outage line names
the failure instead of asserting "did not answer" for all of them. The RSSI rides along
because reason 2 cannot separate an AP that will not have us from one we can hear but
not reach — `-50 dBm` and `-85 dBm` are which is which, and the disconnect event has
carried the number since IDF v5. All of it is in `strux/`, so it joins the WiFi fork
debt already listed for upstream.
