---
id: 2026-09-10-14h06
date: 2026-09-10
time: "14:06"
title: A module bundle is a browser artifact, and two things forgot that
builds-on: 2026-09-10-12h11
supersedes:
---

**Before:** `2026-09-10-12h11` priced the shadow-root boundary at "about 4 KB gzipped per
module bundle" for bundling `createPortal` out of `react-dom` instead of adding a third
entry to every shell's import map. Bytes were treated as the whole cost, and the build
agreed: it compiled, `check-modules` passed, the bundle shipped, the device served it.

**What changed it:** flashing it. Every one of the four module pages came up as
`"psu" could not be loaded — process is not defined`.

`react-dom`'s CommonJS entry is a switch — `process.env.NODE_ENV === "production" ?
require("./cjs/react-dom.production.js") : require(".../development.js")` — and **Vite
does not substitute `process.env.NODE_ENV` in LIBRARY mode**, on the reasonable theory
that a library might be consumed in Node. A module bundle IS built in library mode. So
the bundle shipped a literal reference to `process` to a browser that has none, and threw
at import, before `activate` was ever called.

Nothing in the build could have said a word, because in library mode this is documented
behaviour rather than a mistake. `define: { "process.env.NODE_ENV": "production" }` in
the shared `moduleConfig` is the fix, and it also drops react-dom's development build
from the bundle: 12.5 KB gzipped back down to 9.9 KB, so the real cost of the portal was
~1.9 KB rather than the 4.6 KB measured before.

**Now:** the general shape is that **a module bundle is a browser artifact built by a
tool that does not assume one**, and every default that hedges towards Node is a trap the
module inherits. `check-modules.mjs` grew a third check for it, alongside the
`document.head` one, because both are the same species: invisible at build time, fatal at
runtime, and only findable by flashing. That is now the script's whole purpose stated
plainly — it guards the seam where the build cannot see the browser.

**And the second thing that forgot the target, which cost more time than the first.**
The device is an XY6020L, and it was flashed with `-DBOARD=dps50xx_c3`. There are two C3
boards in this repo and they differ in exactly the thing that matters: Modbus on GPIO 6/5
at 9600 (`dps50xx_c3`) versus 3/4 at 115200 (`xy6020_c3`). The wrong one **built cleanly,
flashed cleanly, booted, joined the network, served its UI and answered every command** —
and reported `online: false` with all 27 capabilities unavailable, which is
indistinguishable from a wiring fault or the intermittent-supply problem already in the
notes.

So the board is a build-time choice with no runtime witness. Nothing on the device says
which profile it was built from: `system info` reports name, project, firmware, idf,
date, chip, cpu, ip and heap, and not the board. The compile already defines
`BOARD_XY6020_C3` / `BOARD_DPS50XX_C3`, so a `board` field in `system info` is cheap and
would have turned an hour of "is the supply broken?" into one glance. Not taken here,
because it is a framework change and this was a debugging session, but it is the obvious
lesson: **a build-time hardware selection needs a runtime witness, or a mis-flash
presents as a hardware fault.**
