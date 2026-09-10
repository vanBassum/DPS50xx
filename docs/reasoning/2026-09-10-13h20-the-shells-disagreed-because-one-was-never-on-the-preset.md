---
id: 2026-09-10-13h20
date: 2026-09-10
time: "13:20"
title: The shells disagreed because one was never on the preset, not because each chose
builds-on: 2026-09-10-12h11
supersedes:
---

**Before:** `2026-09-10-12h11` closed the cascade leak and left one difference standing:
the relay shell publishes `--font-sans: 'Inter Variable'`, the device shell published
`system-ui`, and neither published `--font-mono`. That note called it "correctly the
SHELL's choice rather than an accident of cascade order" — the theme belongs to the
host, two hosts may reasonably want different type, and a module reading tokens will
follow whichever it is rendered in. It read like the contract working as designed.

**What changed it:** `pnpm dlx shadcn@latest apply --preset b0` in both shells. In the
relay it changed the theme not at all — its `index.css` was **already byte-identical** to
what the preset writes, down to `--sidebar-primary: oklch(0.488 0.243 264.376)` and
`--radius-xl: calc(var(--radius) * 1.4)`. In the device shell it changed twelve tokens
and added Inter.

So the two shells were not expressing two preferences. One had applied the preset and
the other never had, and every difference between them — the typeface, `--radius-xl`,
the `--chart-*` family, `--sidebar-primary` — was drift from a shared origin nobody had
re-applied. Comparing the two files afterwards: **103 tokens present in both, zero
differing values.** The device shell has three the relay lacks
(`--destructive-foreground` and its mapping) and no module references them.

**Now:** "the shell owns the theme" was too weak a statement of the contract, and the
weakness was hiding drift. The tokens are not per-shell taste; they are a **shared
shadcn preset that each shell applies**, and the shell's ownership is of *applying and
publishing* it, not of choosing it. That is what makes "the same module looks the same in
every compatible shell" checkable rather than hopeful: two shells on the same preset
publish the same values, and a diff of the two token sets is the test. A shell that
wants to look different is still free to, but it is now a visible decision rather than
the default outcome of nobody re-running a command.

Which also means the earlier open question answers itself. There is no need to decide
whether the device shell "should" match the relay's typeface — being on the preset is
what matching means, and `--font-mono` is absent from *both*, so the module's fallback
resolves the same way in each.

Two costs the preset brought, both worth knowing before the next one:

- **Inter is 213 KB of the `www` partition and 166 KB of it can never be served.**
  fontsource ships seven `unicode-range`-gated subsets; a browser rendering this UI's
  English strings requests only latin (47 KB), but flash holds Cyrillic, Greek,
  Vietnamese and latin-ext regardless. `www` went from 253 KB to 392 KB of 917 KB. The
  prune is one import away and has not been taken, because deviating from what the
  preset writes means the next `apply` silently undoes it.
- **The first binary assets in `www` found a bug in `gzip.mjs`**, which gzipped
  everything it found. A woff2 came out 28 bytes *larger* and would then be served with
  `Content-Encoding: gzip`, so the browser paid a decompress pass to recover bytes it
  could have had directly. Pre-compressed formats are skipped now, and
  `StaticFileHandler` learned `font/woff2`.

**And a correction worth recording, because it was acted on before it was checked:**
`shell-contract/contract.ts` is not owned by this repo. Its own header says SOURCE OF
TRUTH, which reads that way, but it means the file is authoritative *for its shape* — the
file itself is vendored from Strux, and the relay hash-locks its copy against
`vanBassum/Strux` and verifies it over the network (`scripts/check-contract.mjs`). Adding
the host-obligations comment to the relay's copy therefore broke its build, and the
change belongs upstream in Strux like `ArgType::Float` does. Reverted there; still
fork-local here.
