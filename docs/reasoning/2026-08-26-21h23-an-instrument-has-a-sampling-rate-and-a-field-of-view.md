---
id: 2026-08-26-21h23
date: 2026-08-26
time: "21:23"
title: An instrument has a sampling rate and a field of view
builds-on: 2026-08-12-14h05
supersedes:
---

**Before:**
[2026-08-12-14h05](2026-08-12-14h05-the-extender-was-not-wedged-it-was-inaudible.md)
named a recurring failure: "this project has now twice reasoned to a confident
conclusion from a measurement taken somewhere other than the thing being
diagnosed", and prescribed the fix as a channel reporting what the device itself
perceives. The pattern was understood as being about **location** — measure at the
device, not at a laptop.

**What changed it:** a session that repeated the failure three more times while
the note was open and being quoted, and none of the three were about location.

- `netsh wlan show networks` returns the driver's **cached** scan list. Run one
  second after a reset, it cannot show an AP that has just started. On that
  result a correct hypothesis was declared dead — and only reinstated when the
  user saw the AP that the scan said was absent.
- A grep filter over a boot log omitted `Switching to '…'`. The remaining lines
  showed an association succeeding, and it was reported as a fix — for the wrong
  network. The device had switched to the main router; the AP under test had
  failed as usual.
- A comparison table was built from tests taken before and after 2k2 resistors
  were fitted and the harness re-dressed. Presented as a controlled comparison, it
  was nothing of the kind, and it produced a confident retraction of the diagnosis
  that turned out to be right.

**Now:** the pattern generalises well past location. An instrument has a **sampling
rate**, a **field of view**, and a **calibration**, and each can silently destroy a
measurement:

- *Sampling rate* — a cached or averaged reading cannot resolve an event faster
  than its refresh. Asking sooner returns the past, not an answer.
- *Field of view* — every filter, grep, and column selection is a lens. What it
  excludes is invisible in the result, and the result looks complete.
- *Calibration* — a comparison is only controlled if nothing else moved. Physical
  setups drift between tests, and the drift is not recorded anywhere in the
  numbers.

The failure mode is the same each time: a measurement that **cannot** answer the
question returns something that **looks** like an answer, and negative results are
where it bites, because "absent" is what both a real absence and a broken
instrument produce. The discipline is to ask, before believing a negative result:
*could this instrument have shown the positive, if it were there?*

**Rests on:** this being three genuine instances rather than one mistake
retold — they are independent (a cached API, a text filter, an uncontrolled
comparison) and each independently reversed a conclusion.

**Follows:** none directly; this is a check to run, not a change to make. It has
teeth mainly against negative results and against any table comparing runs
separated in time.
