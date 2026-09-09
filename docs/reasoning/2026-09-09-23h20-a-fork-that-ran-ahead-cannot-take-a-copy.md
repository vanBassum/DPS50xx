---
id: 2026-09-09-23h20
date: 2026-09-09
time: "23:20"
title: A fork that ran ahead cannot take a copy
builds-on: 2026-08-06-16h56
supersedes:
---

**Before:** CLAUDE.md said pulling an improvement from Strux was `cp -r ../Strux/main/strux
main/`, and that `strux/` "should stay byte-identical to upstream". Both halves of that
were written when they were true, and the second is what made the first safe.

**What changed it:** upstream had moved about sixty commits — including a whole new
framework manager and a frontend rebuilt around device-hosted UI modules — so the copy
was finally worth doing. Classifying the files first is what stopped it being a
disaster:

- 57 of 83 framework files were already **identical to upstream HEAD**.
- 16 matched an **older** upstream commit: never touched here, safe to copy.
- 10 matched **no** upstream commit at all. Three were the known `ArgType::Float` debt.
  Five were WiFi — this fork's station/AP cycling had run ahead for a month (a second
  network, RSSI on a LinkDown, `authmode` in a scan) and upstream had edited the same
  files independently. `NetworkManager.cpp` alone differed by 955 lines.

A `cp -r` would have compiled, flashed, and quietly reverted the WiFi work this fork
spent days on — the failure being invisible precisely because the code it deleted was
code that *worked*. "Byte-identical" is a discipline, not a fact you can assume when
reaching for the copy.

The classification is mechanical and needs no memory of what was edited: compare each
file's blob against every recent upstream commit. Identical to HEAD, identical to
something older, or matching nothing at all — those three answers are exactly "done",
"copy", and "merge". And the merge base is findable the same way: the newest blob that
appears in **both** histories for that path, which here was one commit from 2026-08-06,
the day this fork was rebuilt on Strux.

**Now:** the sync is a merge with a documented procedure, and the WiFi files were done
by cherry-picking upstream's four individual commits onto ours (mDNS optional, MAC in
the AP name, `GetIpv4`, and the `Ipv4Lost` lease fix) rather than by resolving 32
diff3 conflicts across 2200 changed lines. Same result, and reviewable.

**Follows:** two other traps, both of which cost time. The two repositories disagree
about line endings, so every `diff` without `--strip-trailing-cr` reports the whole file
as changed and hides the four lines that matter. And a fork edit is not always where the
layering says it should be: `lib/protocol` is layer-free substrate and still carries the
one deliberate deviation, which is why it needs naming in CLAUDE.md rather than being
inferable from the directory it lives in.
