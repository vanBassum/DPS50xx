---
id: 2026-09-10-17h24
date: 2026-09-10
time: "17:24"
title: Both shapes are supported, so the UI shape is a product choice and not a fork
builds-on: 2026-09-09-23h10
supersedes:
---

**Before:** `2026-09-09-23h10` established the rule that a shell contributes nothing to a
device's navigation, and everything followed from it — cards went, the landing page became
the manifest's first entry, the framework shipped modules of its own. The rule was right
about what it was for. What went unexamined was whether *this* product wanted it, and the
question never came up because abandoning it looked structural: `UiManager` is a framework
manager, the shell and contract are vendored from Strux, and a fork that walked away from
all of it would be diverging on the single largest surface it shares with upstream. That
made the cost of leaving feel like the cost of a permanent fork.

**What changed it:** Bas, on doubting the whole thing — *"the relay supports both ways, we
can decide per project."*

That is the fact the structural argument was missing, and with it the divergence is not
structural at all. Checking it against the code, the fallback was not merely possible, it
was already **written down in the manager being fought over** — `UiManager::Cmd_Modules`:

> an empty `modules` array is the ordinary answer for a product that ships none, and the
> template's default. A device old enough not to know the command rejects it, which a
> shell reads the same way: no modules, fall back to serving the device's whole page.

So both shapes are first-class *upstream*. A product serving its own whole page is not
opting out of the design; it is taking a path the design names. What follows:

- **Nothing in `strux/` stops registering.** `ConsoleManager`, `SettingsManager` and
  `UpdateManager` each declare a `UiModule`, and deleting those six declarations would have
  made six framework files fork-local, each needing a real merge on every sync. Gating the
  *manifest* instead — one `if` in `Cmd_Modules`, on a `ui.modules` setting — leaves all six
  byte-identical.
- **The divergence is one token.** The setting's default is `false` here and would be `true`
  upstream, because DPS50xx ships no modules and Strux ships four. That is the whole of it,
  and it is contributable: a knob upstream would take the fork to zero.
- **It is reversible at runtime.** The manager stays compiled in and the bundles stay in
  git. Flipping back is a setting, not a revert — which is what makes this a decision the
  product can revisit rather than one it has to get right now.

**The estimate that was wrong, and by how much.** Going back to one SPA was priced at
"~2,000 lines permanently fork-local, the most expensive divergence available." It came in
at one setting and four lines removed from this product's own `PsuManager`. Everything else
deleted — `modules/`, `src/shell/`, `shell-contract/`, `check-modules.mjs` — was code this
fork had written *for* the seam, so removing it is not divergence from upstream; it is
declining to carry a mechanism upstream does not require of a product that ships no modules.

**What the seam actually cost, now that it is countable.** About 2,000 lines that existed
only so a module and a shell could coexist, and no product function among them. The running
tax was more telling than the line count: the three commits before this change were all
seam repair — `process.env` in Vite library mode, the shadcn preset the two shells disagreed
on, the shared Tailwind cascade — and five reasoning notes in two days have the seam as
their subject rather than the supply.

**The general form, worth keeping for the next one.** When a framework offers two shapes and
a fork has taken the richer one, check whether the simpler one is *named in the framework*
before pricing a move as a fork. The expensive-looking direction and the expensive direction
were not the same direction here, and the thing that told them apart was a comment in the
file that would have had to change.
