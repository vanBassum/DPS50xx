---
id: 2026-09-10-12h11
date: 2026-09-10
time: "12:11"
title: No ordering fixes a shared namespace, so the boundary has to be structural
builds-on: 2026-09-09-23h55
supersedes:
---

**Before:** a module's stylesheet was adopted into the host document's `<head>`, and the
open question was only *where* in `<head>`. `2026-09-09-23h55` had already established
that excluding preflight isolates a module's ELEMENTS and does nothing about its utility
CLASSES, and the fix taken from that was an ordering one: insert the module's sheet
FIRST, so the shell wins every tie. That read as a complete answer. It stopped a
module's `.hidden` from deleting the shell's sidebar, which was the failure in hand, and
`activate.ts` carried a long comment explaining why first-child was load-bearing.

**What changed it:** the same PSU bundle, on the same device, rendered differently
depending on which shell was drawing it — and the difference was not in anything either
side had decided.

The page's session grid is a two-column grid that widens to three at the `sm`
breakpoint. In the relay shell it came out three columns. On the device shell it came out
two. The device shell's built CSS contains a bare `.grid-cols-2` rule; the relay's does
not, because the relay only ever writes that utility behind a breakpoint. Both sheets
land in the same `utilities` cascade layer, the shell's sheet is later, and a media query
carries no specificity — so on the device the shell's unprefixed rule outranked the
module's `sm:` rule, and in the relay there was no rule to outrank it.

The same mechanism was quietly changing more than layout. `--radius-xl` was
`calc(var(--radius) * 1.2)` in the module, `* 1.4` in the relay shell, and Tailwind's
`0.75rem` in the device shell, which defines none: three different card radii for one
`rounded-xl`. And `dark:` in a module compiled to `@media (prefers-color-scheme: dark)`,
because the shared `theme.css` never declared the `.dark` custom variant that both
shells switch on — so a module's dark colours followed the OS while everything around
them followed the shell.

**Now:** ordering was never the fix, because the fault is not the order. Two independent
Tailwind builds emitting the same class names into the same layer means the module's
appearance is a function of **what its host happened to emit**, and there is no
tie-break that makes that stop being true. Appending lets a module break the shell;
inserting first lets the shell break the module. Both are the same bug seen from
opposite ends, and the leak to close is the shared cascade itself.

So the boundary is now structural rather than a matter of order: a module renders inside
a shadow root and adopts its stylesheet there (`modules/_ui/ModuleRoot.tsx`). Shell rules
do not reach in, module rules do not reach out, and what crosses is exactly the contract
— CSS custom properties, because they inherit through a shadow boundary. That divides
ownership in a way that can be stated in one line each: the SHELL owns what Strux looks
like (the shadcn token set, light/dark, the navigation around the page), the MODULE owns
how its feature UI is built (markup, components, layout, spacing, its own Tailwind).

Four consequences that were not obvious before doing it:

- **A token contract cannot use the token's own name for a theme key.** Mapping
  `--font-mono: var(--font-mono, …)` in `@theme inline` looks like "read the shell's
  value", but Tailwind emits theme keys as variables on `:root, :host` — and inside a
  shadow root `:host` IS the module's host element, so the declaration shadows the very
  token it meant to read. It happens to work, because a self-referential custom property
  is invalid at computed-value time and falls back to the inherited value, but that is
  far too subtle to build a contract on. The keys are dropped (`--font-sans: initial`)
  and the two utilities are declared with `@utility`, which declares no variable and so
  shadows nothing. Names that DIFFER from the token — `--color-card` from `--card`,
  `--radius-xl` from `--radius` — never had the problem.
- **The boundary must be layout-transparent or it is not ignorable.** The wrapper that
  carries the shadow root gets `display: contents`. Without it the console page's
  `h-full` resolves against an auto-height wrapper instead of the shell's flex column,
  and the page collapses — a boundary that changes layout is one every page has to know
  about, which defeats putting it in one place.
- **A module must query its own root, not the document.** The settings page scrolled to a
  section with `document.getElementById`, and an id inside a shadow root is not in the
  document's id map. `event.currentTarget.getRootNode()` is both the fix and the correct
  scope for a module looking up its own DOM.
- **The isolation is cheap enough not to argue about.** `createPortal` bundled from
  `react-dom` rather than added to the import map — which would mean a coordinated change
  in every shell — costs about 4 KB gzipped per module bundle. React's own `require("react")`
  still resolves to the shared instance through the import map, which is the part that
  matters: two Reacts break hooks, two copies of react-dom's facade break nothing.

The old symptom is worth remembering as the tell: a comment in `activate.ts` explaining
the `hidden md:block` bug caused Tailwind to emit `.hidden` and `.md:block` into all four
module bundles, because the extractor scans comments too. A design where writing down the
bug reproduces the bug is a design with a leak, not a discipline problem.

**Still open:** the three renderings have not been compared on hardware since the change
— this is a build-and-look verification and the device has not been reflashed. And the
device shell publishes no `--font-mono` and a different `--font-sans` from the relay's
Inter, so the two will still differ in typeface. That is now correctly the SHELL's
choice rather than an accident of cascade order, but if the two are meant to match, the
token is where to make them match.
